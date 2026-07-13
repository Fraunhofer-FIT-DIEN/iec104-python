/**
 * Copyright 2020-2026 Fraunhofer Institute for Applied Information Technology
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
 * @file Helper.cpp
 * @brief formatter and shared functionality related to lib60870-C
 *
 * @package iec104-python
 * @namespace Remote
 *
 * @authors Martin Unkel <martin.unkel@fit.fraunhofer.de>
 *
 */

#include "Helper.h"
#include "module/ScopedGilAcquire.h"
#include "types.h"

bool Remote::isSingleCommonAddress(const std::uint_fast16_t commonAddress) {
  return 0 < commonAddress && IEC60870_GLOBAL_COMMON_ADDRESS > commonAddress;
}

bool Remote::isGlobalCommonAddress(const std::uint_fast16_t commonAddress) {
  return IEC60870_GLOBAL_COMMON_ADDRESS == commonAddress;
}

std::string Remote::rawMessageFormatter(const uint_fast8_t *msg,
                                        const int msgSize) {
  std::string s;

  // Guard: the buffer must be long enough to safely read the start octet,
  // the length octet and the first control-field octet (offset 0..2). A
  // shorter buffer cannot be parsed and would otherwise cause an out-of-bounds
  // read (CWE-125) when fed from e.g. an on_receive_raw handler.
  if (msgSize < 4) {
    s += "INVALID_MESSAGE";
    return s;
  }

  // clamp to max apdu length
  const auto clampedMsgSize = static_cast<size_t>(std::min(msgSize, 255));
  // authoritative APDU length: 2 start/length octets plus the length octet
  const auto messageSize =
      std::min(clampedMsgSize, static_cast<size_t>(2) + msg[1]);

  /*
  for (int i = 0; i < messageSize; i++) {
      char buffer[6];
      snprintf(buffer, 5, "%02x ", msg[i]);
      s += buffer;
  }
  return s;*/

  if (msg[IEC60870_FORMAT_OFFSET] & 0b00000001) {
    if (msg[IEC60870_FORMAT_OFFSET] & 0b00000010) {
      s += "U-Format";
      if (msg[IEC60870_FORMAT_OFFSET] & 0b00000100) {
        s += " | StartDT act";
      }
      if (msg[IEC60870_FORMAT_OFFSET] & 0b00001000) {
        s += " | StartDT con";
      }
      if (msg[IEC60870_FORMAT_OFFSET] & 0b00010000) {
        s += " | StopDT act";
      }
      if (msg[IEC60870_FORMAT_OFFSET] & 0b00100000) {
        s += " | StopDT con";
      }
      if (msg[IEC60870_FORMAT_OFFSET] & 0b01000000) {
        s += " | TestFR act";
      }
      if (msg[IEC60870_FORMAT_OFFSET] & 0b10000000) {
        s += " | TestFR con";
      }
    } else {
      s += "S-Format";
    }
  } else {
    s += "I-Format";

    uint8_t numberOfObjects = 0;
    bool sequence = false;

    // The I-format ASDU header spans offsets 6..11 and needs at least 12
    // octets in the buffer to be read safely.
    if (messageSize >= 12) {
      auto typeId = (IEC60870_5_TypeID)msg[IEC60870_TYPEID_OFFSET];
      s += " | " + std::string(TypeID_toString(typeId));

      numberOfObjects = (uint8_t)(msg[IEC60870_STRUCTURE_OFFSET] & 0b01111111);
      sequence = (bool)(msg[IEC60870_STRUCTURE_OFFSET] & 0b10000000);

      auto cot = (CS101_CauseOfTransmission)(msg[IEC60870_MSGINFO_OFFSET] &
                                             0b00111111);
      s += " | " + std::string(CS101_CauseOfTransmission_toString(cot));

      if (msg[IEC60870_MSGINFO_OFFSET] & 0b01000000) {
        s += " | NEGATIVE";
      } else {
        s += " | POSITIVE";
      }

      if (msg[IEC60870_MSGINFO_OFFSET] & 0b10000000) {
        s += " | TEST";
      }

      uint_fast16_t targetCommonAddress = msg[IEC60870_TARGETCA1_OFFSET] +
                                          (msg[IEC60870_TARGETCA2_OFFSET] << 8);
      if (targetCommonAddress == IEC60870_GLOBAL_COMMON_ADDRESS) {
        s += " | GLOBAL";
      } else {
        s += " | CA " + std::to_string(targetCommonAddress);
      }

      uint_fast16_t sourceCommonAddress = msg[IEC60870_SOURCECA_OFFSET];
      if (sourceCommonAddress > 0)
        s += " | OA " + std::to_string(sourceCommonAddress);
    }

    if (messageSize > IEC60870_OBJECT_OFFSET) {

      if (messageSize > IEC60870_OBJECT_OFFSET + 2) {
        uint_fast16_t firstIOA = msg[IEC60870_OBJECT_OFFSET] +
                                 (msg[IEC60870_OBJECT_OFFSET + 1] << 8) +
                                 (msg[IEC60870_OBJECT_OFFSET + 2] << 16);
        s += " | 1st IOA " + std::to_string(firstIOA);
      }

      if (numberOfObjects > 0) {
        if (sequence) {
          s += " | SEQUENCE[" + std::to_string(numberOfObjects) + "]";
        } else {
          s += " | LIST[" + std::to_string(numberOfObjects) + "]";
        }
      }
      s += " (";
      int i;
      for (i = IEC60870_OBJECT_OFFSET; i < static_cast<int>(messageSize); i++) {
        char buffer[6];
        snprintf(buffer, 5, "%02x ", msg[i]);
        s += buffer;
      }
      if (static_cast<int>(messageSize) < msgSize) {
        s += "...APDU_LENGTH_EXCEEDED";
      }
      s += ")";
    }
  }

  return s;
}

py::dict Remote::rawMessageDictionaryFormatter(const uint_fast8_t *msg,
                                               const int msgSize) {
  Module::ScopedGilAcquire gilAcquire("rawMessageDictionaryFormatter");
  py::dict d;

  // Guard: the buffer must be long enough to safely read the start octet,
  // the length octet and the first control-field octet (offset 0..2). A
  // shorter buffer cannot be parsed and would otherwise cause an out-of-bounds
  // read (CWE-125) when fed from e.g. an on_receive_raw handler.
  if (msgSize < 4) {
    d["format"] = "INVALID";
    return d;
  }

  // clamp to max apdu length
  const auto clampedMsgSize = static_cast<size_t>(std::min(msgSize, 255));
  // authoritative APDU length: 2 start/length octets plus the length octet
  const auto messageSize =
      std::min(clampedMsgSize, static_cast<size_t>(2) + msg[1]);

  /*
  for (int i = 0; i < messageSize; i++) {
      char buffer[6];
      snprintf(buffer, 5, "%02x ", msg[i]);
      s += buffer;
  }
  return s;*/

  if (msg[IEC60870_FORMAT_OFFSET] & 0b00000001) {
    if (msg[IEC60870_FORMAT_OFFSET] & 0b00000010) {
      d["format"] = "U";
      if (msg[IEC60870_FORMAT_OFFSET] & 0b00000100) {
        d["type"] = "STARTDT";
        d["cot"] = "ACT";
      }
      if (msg[IEC60870_FORMAT_OFFSET] & 0b00001000) {
        d["type"] = "STARTDT";
        d["cot"] = "CON";
      }
      if (msg[IEC60870_FORMAT_OFFSET] & 0b00010000) {
        d["type"] = "STOPDT";
        d["cot"] = "ACT";
      }
      if (msg[IEC60870_FORMAT_OFFSET] & 0b00100000) {
        d["type"] = "STOPDT";
        d["cot"] = "CON";
      }
      if (msg[IEC60870_FORMAT_OFFSET] & 0b01000000) {
        d["type"] = "TESTFR";
        d["cot"] = "ACT";
      }
      if (msg[IEC60870_FORMAT_OFFSET] & 0b10000000) {
        d["type"] = "TESTFR";
        d["cot"] = "CON";
      }
    } else {
      d["format"] = "S";
      if (messageSize >= 6) {
        d["rx"] = ((uint16_t)msg[IEC60870_FORMAT_OFFSET + 2] +
                   (msg[IEC60870_FORMAT_OFFSET + 3] << 8)) >>
                  1;
      }
    }
  } else {
    d["format"] = "I";

    // the frame sequence (tx/rx) counters live at offsets 2..5 and need at
    // least 6 octets to be read safely
    if (messageSize >= 6) {
      uint16_t rx = ((uint16_t)msg[IEC60870_FORMAT_OFFSET + 2] +
                     (msg[IEC60870_FORMAT_OFFSET + 3] << 8)) >>
                    1;
      d["tx"] = ((uint16_t)msg[IEC60870_FORMAT_OFFSET] +
                 (msg[IEC60870_FORMAT_OFFSET + 1] << 8)) >>
                1;
      d["rx"] = rx;
    }

    IEC60870_5_TypeID type = IEC60870_5_TypeID::M_SP_NA_1;

    // the ASDU header spans offsets 6..11 and needs at least 12 octets to be
    // read safely
    if (messageSize >= 12) {
      type = (IEC60870_5_TypeID)msg[IEC60870_TYPEID_OFFSET];
      d["type"] = type;

      d["numberOfObjects"] =
          (uint8_t)(msg[IEC60870_STRUCTURE_OFFSET] & 0b01111111);
      d["sequence"] = (bool)(msg[IEC60870_STRUCTURE_OFFSET] & 0b10000000);

      d["cot"] = (CS101_CauseOfTransmission)(msg[IEC60870_MSGINFO_OFFSET] &
                                             0b00111111);
      d["negative"] = (bool)(msg[IEC60870_MSGINFO_OFFSET] & 0b01000000);
      d["test"] = (bool)(msg[IEC60870_MSGINFO_OFFSET] & 0b10000000);

      d["commonAddress"] = (uint16_t)msg[IEC60870_TARGETCA1_OFFSET] +
                           (msg[IEC60870_TARGETCA2_OFFSET] << 8);
      d["originatorAddress"] = (uint8_t)msg[IEC60870_SOURCECA_OFFSET];
    }

    if (messageSize > IEC60870_OBJECT_OFFSET) {

      if (messageSize > IEC60870_OBJECT_OFFSET + 2) {
        d["firstInformationObjectAddress"] =
            (uint32_t)msg[IEC60870_OBJECT_OFFSET] +
            (msg[IEC60870_OBJECT_OFFSET + 1] << 8) +
            (msg[IEC60870_OBJECT_OFFSET + 2] << 16);

        if ((type >= C_SC_NA_1 && type <= C_SE_NC_1) ||
            (type >= C_SC_TA_1 && type <= C_SE_TC_1)) {
          if (messageSize > IEC60870_OBJECT_OFFSET + 3) {
            d["select"] = (bool)(msg[IEC60870_OBJECT_OFFSET + 3] >> 7);
          }
        }
      }

      std::string s;
      int i;
      for (i = IEC60870_OBJECT_OFFSET; i < static_cast<int>(messageSize); i++) {
        char buffer[6];
        snprintf(buffer, 5, "%02x ", msg[i]);
        s += buffer;
      }
      d["elements"] = s;
    }
  }

  return d;
}

std::string Remote::CP56Time2a_toString(CP56Time2a time) {
  char buffer[100];
  snprintf(buffer, 99, "%02i:%02i:%02i %02i/%02i/%04i",
           CP56Time2a_getHour(time), CP56Time2a_getMinute(time),
           CP56Time2a_getSecond(time), CP56Time2a_getDayOfMonth(time),
           CP56Time2a_getMonth(time) + 1, CP56Time2a_getYear(time) + 2000);

  return std::string(buffer);
}

std::string Remote::connectionStringFormatter(const std::string &ip,
                                              const uint_fast16_t port) {
  return (0 == port) ? ip + ":" + std::to_string(IEC_60870_5_104_DEFAULT_PORT)
                     : ip + ":" + std::to_string(port);
}

bool Remote::file_exists(const std::string &name) {
  if (FILE *file = fopen(name.c_str(), "r")) {
    fclose(file);
    return true;
  } else {
    return false;
  }
}
