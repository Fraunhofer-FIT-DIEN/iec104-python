/**
 * Copyright 2020-2024 Fraunhofer Institute for Applied Information Technology
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

std::string Remote::rawMessageFormatter(uint_fast8_t *msg, const int msgSize) {
  std::string s;

  /*
  for (int i = 0; i < msgSize; i++) {
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
    auto typeId = (IEC60870_5_TypeID)msg[IEC60870_TYPEID_OFFSET];
    s += " | " + std::string(TypeID_toString(typeId));

    auto numberOfObjects =
        (uint8_t)(msg[IEC60870_STRUCTURE_OFFSET] & 0b01111111);
    auto sequence = (bool)(msg[IEC60870_STRUCTURE_OFFSET] & 0b10000000);

    auto cot =
        (CS101_CauseOfTransmission)(msg[IEC60870_MSGINFO_OFFSET] & 0b00111111);
    s += " | " + std::string(CS101_CauseOfTransmission_toString(cot));

    if (msg[IEC60870_MSGINFO_OFFSET] & 0b01000000) {
      s += " | NEGATIVE";
    } else {
      s += " | POSITIVE";
    }

    if (msg[IEC60870_MSGINFO_OFFSET] & 0b10000000) {
      s += " | TEST";
    }

    uint_fast16_t targetCommonAddress =
        msg[IEC60870_TARGETCA1_OFFSET] + (msg[IEC60870_TARGETCA2_OFFSET] << 8);
    if (targetCommonAddress == IEC60870_GLOBAL_COMMON_ADDRESS) {
      s += " | GLOBAL";
    } else {
      s += " | CA " + std::to_string(targetCommonAddress);
    }

    uint_fast16_t sourceCommonAddress = msg[IEC60870_SOURCECA_OFFSET];
    if (sourceCommonAddress > 0)
      s += " | OA " + std::to_string(sourceCommonAddress);

    if (msgSize > IEC60870_OBJECT_OFFSET) {

      if (msgSize > IEC60870_OBJECT_OFFSET + 2) {
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
      for (i = IEC60870_OBJECT_OFFSET; i < msgSize; i++) {
        char buffer[6];
        snprintf(buffer, 5, "%02x ", msg[i]);
        s += buffer;
      }
      s += ")";
    }
  }

  return s;
}

py::dict Remote::rawMessageDictionaryFormatter(uint_fast8_t *msg,
                                               const int msgSize) {
  Module::ScopedGilAcquire gilAcquire("rawMessageDictionaryFormatter");
  py::dict d;

  /*
  for (int i = 0; i < msgSize; i++) {
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
      d["rx"] = ((uint16_t)msg[IEC60870_FORMAT_OFFSET + 2] +
                 (msg[IEC60870_FORMAT_OFFSET + 3] << 8)) >>
                1;
    }
  } else {
    d["format"] = "I";

    uint16_t rx = ((uint16_t)msg[IEC60870_FORMAT_OFFSET + 2] +
                   (msg[IEC60870_FORMAT_OFFSET + 3] << 8)) >>
                  1;
    d["tx"] = ((uint16_t)msg[IEC60870_FORMAT_OFFSET] +
               (msg[IEC60870_FORMAT_OFFSET + 1] << 8)) >>
              1;
    d["rx"] = ((uint16_t)msg[IEC60870_FORMAT_OFFSET + 2] +
               (msg[IEC60870_FORMAT_OFFSET + 3] << 8)) >>
              1;

    IEC60870_5_TypeID type = (IEC60870_5_TypeID)msg[IEC60870_TYPEID_OFFSET];
    d["type"] = type;

    d["numberOfObjects"] =
        (uint8_t)(msg[IEC60870_STRUCTURE_OFFSET] & 0b01111111);
    d["sequence"] = (bool)(msg[IEC60870_STRUCTURE_OFFSET] & 0b10000000);

    d["cot"] =
        (CS101_CauseOfTransmission)(msg[IEC60870_MSGINFO_OFFSET] & 0b00111111);
    d["negative"] = (bool)(msg[IEC60870_MSGINFO_OFFSET] & 0b01000000);
    d["test"] = (bool)(msg[IEC60870_MSGINFO_OFFSET] & 0b10000000);

    d["commonAddress"] = (uint16_t)msg[IEC60870_TARGETCA1_OFFSET] +
                         (msg[IEC60870_TARGETCA2_OFFSET] << 8);
    d["originatorAddress"] = (uint8_t)msg[IEC60870_SOURCECA_OFFSET];

    if (msgSize > IEC60870_OBJECT_OFFSET) {

      if (msgSize > IEC60870_OBJECT_OFFSET + 2) {
        d["firstInformationObjectAddress"] =
            (uint32_t)msg[IEC60870_OBJECT_OFFSET] +
            (msg[IEC60870_OBJECT_OFFSET + 1] << 8) +
            (msg[IEC60870_OBJECT_OFFSET + 2] << 16);

        if ((type >= C_SC_NA_1 && type <= C_SE_NC_1) ||
            (type >= C_SC_TA_1 && type <= C_SE_TC_1)) {
          d["select"] = (bool)(msg[IEC60870_OBJECT_OFFSET + 3] << 7);
        }
      }

      std::string s;
      int i;
      for (i = IEC60870_OBJECT_OFFSET; i < msgSize; i++) {
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
