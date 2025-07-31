/**
 * Copyright 2020-2025 Fraunhofer Institute for Applied Information Technology
 * FIT
 *
 * This file is part of iec104-python.
 * iec104-python is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * iec104-python is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with iec104-python. If not, see <https://www.gnu.org/licenses/>.
 *
 *  See LICENSE file for the complete license text.
 *
 *
 * @file IncomingMessage.cpp
 * @brief create a message object from ASDU struct
 * @todo test valid ioa address in series via getIOA()??
 *
 * @package iec104-python
 * @namespace Remote::Message
 *
 * @authors Martin Unkel <martin.unkel@fit.fraunhofer.de>
 *
 */

#include "IncomingMessage.h"
#include "constants.h"
#include "object/information/ICommand.h"
#include "remote/Helper.h"
#include "transformer/Information.h"
#include <memory>
#include <sstream>

using namespace Remote::Message;
using namespace Object::Information;

std::shared_ptr<IncomingMessage>
IncomingMessage::create(CS101_ASDU packet,
                        CS101_AppLayerParameters app_layer_parameters,
                        const bool load_io) {
  // Not using std::make_shared because the constructor is private.
  return std::shared_ptr<IncomingMessage>(
      new IncomingMessage(packet, app_layer_parameters, load_io));
}

IncomingMessage::IncomingMessage(CS101_ASDU packet,
                                 CS101_AppLayerParameters app_layer_parameters,
                                 const bool load_io)
    : IMessageInterface(), asdu(nullptr), parameters(app_layer_parameters) {
  if (packet) {
    asdu = CS101_ASDU_clone(packet, nullptr);
  }

  if (asdu) {
    commonAddress = CS101_ASDU_getCA(asdu);
    originatorAddress = CS101_ASDU_getOA(asdu);
    type = CS101_ASDU_getTypeID(asdu);

    causeOfTransmission = CS101_ASDU_getCOT(asdu);

    negative = CS101_ASDU_isNegative(asdu);
    sequence = CS101_ASDU_isSequence(asdu);
    test = CS101_ASDU_isTest(asdu);

    numberOfObjects = (uint_fast8_t)CS101_ASDU_getNumberOfElements(asdu);

    if (load_io) {
      first();
    }
  }
  DEBUG_PRINT(Debug::Message, "Created (incoming)");
}

IncomingMessage::~IncomingMessage() {
  if (io) {
    InformationObject_destroy(io);
  }
  if (asdu) {
    CS101_ASDU_destroy(asdu);
  }
  DEBUG_PRINT(Debug::Message, "Removed (incoming)");
}

CS101_ASDU IncomingMessage::getAsdu() const { return asdu; }

unsigned char *IncomingMessage::getRawBytes() const {
  unsigned char *payload = CS101_ASDU_getPayload(asdu);
  unsigned char payloadSize = CS101_ASDU_getPayloadSize(asdu);

  // max total len 255 octets ^= 255 * 8 byte = 255 * 64 bit
  // content len = 4 (control) + number of asdu's
  // start = 68 | len | control1 | control2 | control3 | control 4 ^= 4 octet
  // asdu | ... ^= up to 249 octet
  // start octet = 68

  unsigned char asduHeaderLength =
      2 + parameters->sizeOfCOT + parameters->sizeOfCA;
  unsigned char msgSize = 6 + asduHeaderLength + payloadSize;

  const unsigned char HEADER_OFFSET = 0;
  const unsigned char ASDU_OFFSET = 6;
  const unsigned char PAYLOAD_OFFSET = ASDU_OFFSET + asduHeaderLength;

  auto msg = new unsigned char[msgSize];

  // initialize to zero
  for (unsigned char i = 0; i < msgSize; i++) {
    msg[i] = 0;
  }

  msg[HEADER_OFFSET + 0] = 104;
  msg[HEADER_OFFSET + 1] = 4 + asduHeaderLength +
                           payloadSize; // length following behind length field

  msg[ASDU_OFFSET + 0] = type; // typeId
  msg[ASDU_OFFSET + 1] = numberOfObjects;
  if (sequence) {
    msg[ASDU_OFFSET + 1] |= 0x80;
  }
  msg[ASDU_OFFSET + 2] = causeOfTransmission & 0x3f;
  if (test) {
    msg[ASDU_OFFSET + 2] |= 0x80;
  }
  if (negative) {
    msg[ASDU_OFFSET + 2] |= 0x40;
  }
  if (parameters->sizeOfCOT > 1) {
    msg[ASDU_OFFSET + 3] = originatorAddress;
  }
  int caIndex = ASDU_OFFSET + 2 + parameters->sizeOfCOT;
  if (parameters->sizeOfCA == 1) {
    msg[caIndex] = (uint8_t)commonAddress;
  } else {
    msg[caIndex] = (uint8_t)(commonAddress % 0x100);
    msg[caIndex + 1] = (uint8_t)(commonAddress / 0x100);
  }

  for (unsigned char i = 0; i < payloadSize; i++) {
    msg[PAYLOAD_OFFSET + i] = payload[i];
  }

  return msg;
}

std::string IncomingMessage::getRawMessageString() const {
  unsigned char *msg = getRawBytes();
  unsigned char msgSize = 2 + msg[1];

  std::string formatted = rawMessageFormatter(msg, msgSize);
  delete msg;
  return formatted;
}

std::uint_fast8_t IncomingMessage::getNumberOfObjects() const {
  return numberOfObjects;
}

void IncomingMessage::first() {
  // REJECT CP24Time based messages
  switch (type) {
  case M_SP_TA_1:
  case M_DP_TA_1:
  case M_ST_TA_1:
  case M_BO_TA_1:
  case M_ME_TA_1:
  case M_ME_TB_1:
  case M_ME_TC_1:
  case M_IT_TA_1:
  case M_EP_TA_1:
  case M_EP_TB_1:
  case M_EP_TC_1: {
    throw std::invalid_argument("CP24Time based messages not supported by norm "
                                "IEC60870-5-104 (101 only)!");
  }
  case C_TS_NA_1:
  case C_CD_NA_1: {
    throw std::invalid_argument(
        "Message not supported by norm IEC60870-5-104 (101 only)!");
  }
  }

  if (type >= C_SC_NA_1 && type < F_DR_TA_1) {
    // REJECT sequence in non-sequence context
    if (sequence) {
      throw std::invalid_argument("IncomingMessage with TypeID " +
                                  std::string(TypeID_toString(type)) +
                                  " must not be marked as sequence.");
    }
    // REJECT multiple objects in non-list context
    if (numberOfObjects > 1) {
      throw std::invalid_argument(
          "IncomingMessage with TypeID " + std::string(TypeID_toString(type)) +
          " must not contain more than one information object.");
    }
  }

  // REJECT global common address in non-global context
  if (type < C_IC_NA_1 && type > C_RP_NA_1 &&
      commonAddress == IEC60870_GLOBAL_COMMON_ADDRESS) {
    throw std::invalid_argument(
        "IncomingMessage with TypeID " + std::string(TypeID_toString(type)) +
        " must use a single common address and not the global common address.");
  }

  // REJECT file transfer @todo handle file transfer
  if (type >= F_FR_NA_1) {
    throw std::invalid_argument(
        "lib60870-c does not support file transfer messages.");
  }

  {
    std::lock_guard<Module::GilAwareMutex> const lock(position_mutex);

    positionReset = true;
    position = 0;
    positionValid = (numberOfObjects > 0);
  }

  if (positionValid)
    extractInformation();
}

bool IncomingMessage::next() {
  {
    std::lock_guard<Module::GilAwareMutex> const lock(position_mutex);

    if (positionReset) {
      position = 0;
      positionReset = false;
      positionValid = (numberOfObjects > 0);
    } else {
      position++;
      positionValid = (position < numberOfObjects);
    }
  }

  if (positionValid)
    extractInformation();

  return positionValid;
}

void IncomingMessage::extractInformation() {
  std::lock_guard<Module::GilAwareMutex> const lock(access_mutex);

  if (io)
    InformationObject_destroy(io);

  io = CS101_ASDU_getElement(asdu, position);
  informationObjectAddress =
      (io == nullptr) ? 0 : InformationObject_getObjectAddress(io);
  info.reset();

  if ((io != nullptr) && positionValid) {
    info = Transformer::fromInformationObject(io);
  }
}

bool IncomingMessage::isValidCauseOfTransmission() const {
  bool result = true;
  switch (type) {
  case M_SP_TA_1:
  case M_DP_TA_1:
  case M_ST_TA_1:
  case M_BO_TA_1:
  case M_ME_TA_1:
  case M_ME_TB_1:
  case M_ME_TC_1:
  case M_IT_TA_1:
  case M_EP_TA_1:
  case M_EP_TB_1:
  case M_EP_TC_1:
  case C_TS_NA_1:
  case C_CD_NA_1: {
    throw std::invalid_argument(
        "[104.IncomingMessage] type not supported by norm IEC60870-5-104!");
  }
  case M_SP_NA_1:
  case M_DP_NA_1:
  case M_ST_NA_1: {
    if (causeOfTransmission != CS101_COT_BACKGROUND_SCAN &&
        causeOfTransmission != CS101_COT_SPONTANEOUS &&
        causeOfTransmission != CS101_COT_REQUEST &&
        causeOfTransmission != CS101_COT_RETURN_INFO_REMOTE &&
        causeOfTransmission != CS101_COT_RETURN_INFO_LOCAL &&
        (causeOfTransmission < CS101_COT_INTERROGATED_BY_STATION &&
         causeOfTransmission > CS101_COT_INTERROGATED_BY_GROUP_16)) {
      result = false;
    }
  } break;
  case M_BO_NA_1:
  case M_PS_NA_1: {
    if (causeOfTransmission != CS101_COT_BACKGROUND_SCAN &&
        causeOfTransmission != CS101_COT_SPONTANEOUS &&
        causeOfTransmission != CS101_COT_REQUEST &&
        (causeOfTransmission < CS101_COT_INTERROGATED_BY_STATION &&
         causeOfTransmission > CS101_COT_INTERROGATED_BY_GROUP_16)) {
      result = false;
    }
  } break;
  case M_ME_NA_1:
  case M_ME_NB_1:
  case M_ME_NC_1:
  case M_ME_ND_1: {
    if (causeOfTransmission != CS101_COT_PERIODIC &&
        causeOfTransmission != CS101_COT_BACKGROUND_SCAN &&
        causeOfTransmission != CS101_COT_SPONTANEOUS &&
        causeOfTransmission != CS101_COT_REQUEST &&
        (causeOfTransmission < CS101_COT_INTERROGATED_BY_STATION &&
         causeOfTransmission > CS101_COT_INTERROGATED_BY_GROUP_16)) {
      result = false;
    }
  } break;
  case M_IT_NA_1:
  case M_IT_TB_1: {
    if (causeOfTransmission != CS101_COT_SPONTANEOUS &&
        (causeOfTransmission < CS101_COT_REQUESTED_BY_GENERAL_COUNTER &&
         causeOfTransmission > CS101_COT_REQUESTED_BY_GROUP_4_COUNTER)) {
      result = false;
    }
  } break;
  case M_SP_TB_1:
  case M_DP_TB_1:
  case M_ST_TB_1: {
    if (causeOfTransmission != CS101_COT_SPONTANEOUS &&
        causeOfTransmission != CS101_COT_REQUEST &&
        causeOfTransmission != CS101_COT_RETURN_INFO_REMOTE &&
        causeOfTransmission != CS101_COT_RETURN_INFO_LOCAL) {
      result = false;
    }
  } break;
  case M_BO_TB_1:
  case M_ME_TD_1:
  case M_ME_TE_1:
  case M_ME_TF_1:
  case F_DR_TA_1: {
    if (causeOfTransmission != CS101_COT_SPONTANEOUS &&
        causeOfTransmission != CS101_COT_REQUEST) {
      result = false;
    }
  } break;
  case M_EP_TD_1:
  case M_EP_TE_1:
  case M_EP_TF_1: {
    if (causeOfTransmission != CS101_COT_SPONTANEOUS) {
      result = false;
    }
  } break;
  case C_SC_NA_1:
  case C_SC_TA_1:
  case C_DC_NA_1:
  case C_DC_TA_1:
  case C_RC_NA_1:
  case C_RC_TA_1:
  case C_SE_NA_1:
  case C_SE_TA_1:
  case C_SE_NB_1:
  case C_SE_TB_1:
  case C_SE_NC_1:
  case C_SE_TC_1:
  case C_IC_NA_1: {
    if ((causeOfTransmission < CS101_COT_ACTIVATION &&
         causeOfTransmission > CS101_COT_ACTIVATION_TERMINATION) &&
        (causeOfTransmission < CS101_COT_UNKNOWN_TYPE_ID &&
         causeOfTransmission > CS101_COT_UNKNOWN_IOA)) {
      result = false;
    }
  } break;
  case C_BO_NA_1:
  case C_BO_TA_1:
  case C_CI_NA_1: {
    if ((causeOfTransmission != CS101_COT_ACTIVATION &&
         causeOfTransmission != CS101_COT_ACTIVATION_CON &&
         causeOfTransmission != CS101_COT_ACTIVATION_TERMINATION) &&
        (causeOfTransmission < CS101_COT_UNKNOWN_TYPE_ID &&
         causeOfTransmission > CS101_COT_UNKNOWN_IOA)) {
      result = false;
    }
  } break;
  case M_EI_NA_1: {
    if (causeOfTransmission != CS101_COT_INITIALIZED) {
      result = false;
    }
  } break;
  case C_RD_NA_1: {
    if (causeOfTransmission != CS101_COT_REQUEST &&
        (causeOfTransmission < CS101_COT_UNKNOWN_TYPE_ID &&
         causeOfTransmission > CS101_COT_UNKNOWN_IOA)) {
      result = false;
    }
  } break;
  case C_CS_NA_1: {
    if (causeOfTransmission != CS101_COT_ACTIVATION &&
        causeOfTransmission != CS101_COT_ACTIVATION_CON &&
        // causeOfTransmission != CS101_COT_SPONTANEOUS && only cs 101, not
        // allowed in 104
        (causeOfTransmission < CS101_COT_UNKNOWN_TYPE_ID &&
         causeOfTransmission > CS101_COT_UNKNOWN_IOA)) {
      result = false;
    }
  } break;
  case C_RP_NA_1:
  case C_TS_TA_1: {
    if (causeOfTransmission != CS101_COT_ACTIVATION &&
        causeOfTransmission != CS101_COT_ACTIVATION_CON &&
        (causeOfTransmission < CS101_COT_UNKNOWN_TYPE_ID &&
         causeOfTransmission > CS101_COT_UNKNOWN_IOA)) {
      result = false;
    }
  } break;
  case P_ME_NA_1:
  case P_ME_NB_1:
  case P_ME_NC_1: {
    if (causeOfTransmission != CS101_COT_ACTIVATION &&
        causeOfTransmission != CS101_COT_ACTIVATION_CON &&
        (causeOfTransmission < CS101_COT_INTERROGATED_BY_STATION &&
         causeOfTransmission > CS101_COT_INTERROGATED_BY_GROUP_16) &&
        (causeOfTransmission < CS101_COT_UNKNOWN_TYPE_ID &&
         causeOfTransmission > CS101_COT_UNKNOWN_IOA)) {
      result = false;
    }
  } break;
  case P_AC_NA_1: {
    if ((causeOfTransmission < CS101_COT_ACTIVATION &&
         causeOfTransmission > CS101_COT_DEACTIVATION_CON) &&
        (causeOfTransmission < CS101_COT_UNKNOWN_TYPE_ID &&
         causeOfTransmission > CS101_COT_UNKNOWN_IOA)) {
      result = false;
    }
  } break;
  case F_FR_NA_1:
  case F_SR_NA_1:
  case F_LS_NA_1:
  case F_AF_NA_1:
  case F_SG_NA_1: {
    if (causeOfTransmission != CS101_COT_FILE_TRANSFER &&
        (causeOfTransmission < CS101_COT_UNKNOWN_TYPE_ID &&
         causeOfTransmission > CS101_COT_UNKNOWN_IOA)) {
      result = false;
    }
  } break;
  case F_SC_NA_1: {
    if (causeOfTransmission != CS101_COT_REQUEST &&
        causeOfTransmission != CS101_COT_FILE_TRANSFER &&
        (causeOfTransmission < CS101_COT_UNKNOWN_TYPE_ID &&
         causeOfTransmission > CS101_COT_UNKNOWN_IOA)) {
      result = false;
    }
  } break;
  case F_SC_NB_1: {
    if (causeOfTransmission != CS101_COT_SPONTANEOUS &&
        causeOfTransmission != CS101_COT_REQUEST &&
        causeOfTransmission != CS101_COT_FILE_TRANSFER &&
        (causeOfTransmission < CS101_COT_UNKNOWN_TYPE_ID &&
         causeOfTransmission > CS101_COT_UNKNOWN_IOA)) {
      result = false;
    }
  } break;
  }

  return result;
}

bool IncomingMessage::requireConfirmation() const {
  return (causeOfTransmission == CS101_COT_ACTIVATION ||
          causeOfTransmission == CS101_COT_DEACTIVATION);
}

bool IncomingMessage::isSelectCommand() const {
  auto cmd = std::dynamic_pointer_cast<ICommand>(info);
  return cmd && cmd->isSelectable() && cmd->isSelect();
}

std::string IncomingMessage::toString() const {
  std::ostringstream oss;
  oss << "<c104.IncomingMessage common_address="
      << std::to_string(commonAddress)
      << ", io_address=" << std::to_string(informationObjectAddress)
      << ", type=" << TypeID_toString(type) << ", info=" << info->name()
      << ", cot=" << CS101_CauseOfTransmission_toString(causeOfTransmission)
      << ", number_of_objects=" << std::to_string(numberOfObjects)
      << ", test=" << bool_toString(test)
      << ", negative=" << bool_toString(negative)
      << ", sequence=" << bool_toString(sequence) << " at " << std::hex
      << std::showbase << reinterpret_cast<std::uintptr_t>(this) << ">";
  return oss.str();
};
