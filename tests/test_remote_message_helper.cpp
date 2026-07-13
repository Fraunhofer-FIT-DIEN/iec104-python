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
 * @authors Martin Unkel <martin.unkel@fit.fraunhofer.de>
 *
 */

#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <string>
#include <vector>

#include "remote/Helper.h"

/**
 * Regression test: rawMessageFormatter and rawMessageDictionaryFormatter were
 * reading fixed offsets (up to TARGETCA2 = 11, and OBJECT_OFFSET + 3 = 15)
 * with only an upper clamp and no lower bound on the buffer length. Short
 * buffers (e.g. 3-byte 'I-format' frames) caused an out-of-bounds heap read
 * (CWE-125). These cases must not crash and must return a safe placeholder.
 */
TEST_CASE("rawMessageFormatter does not read out-of-bounds on short buffers",
          "[remote::helper]") {
  const std::size_t bufferLengths[] = {0, 1, 2, 3, 6, 13, 15};

  for (const auto length : bufferLengths) {
    // fill with meaningful bytes so the format branch is exercised; the
    // actual content does not matter as long as no crash occurs
    std::vector<uint_fast8_t> msg(length, 0);
    for (std::size_t i = 0; i < length; i++) {
      msg[i] = static_cast<uint_fast8_t>(i);
    }

    REQUIRE_NOTHROW(
        Remote::rawMessageFormatter(msg.data(), static_cast<int>(length)));
  }
}

TEST_CASE("rawMessageDictionaryFormatter does not read out-of-bounds on short "
          "buffers",
          "[remote::helper]") {
  const std::size_t bufferLengths[] = {0, 1, 2, 3, 6, 13, 15};

  for (const auto length : bufferLengths) {
    std::vector<uint_fast8_t> msg(length, 0);
    for (std::size_t i = 0; i < length; i++) {
      msg[i] = static_cast<uint_fast8_t>(i);
    }

    REQUIRE_NOTHROW(Remote::rawMessageDictionaryFormatter(
        msg.data(), static_cast<int>(length)));
  }
}

/**
 * Regression: a received buffer may declare an ASDU length octet (msg[1])
 * that is larger than the actual buffer that was received (e.g. a truncated or
 * malformed frame). The formatters must never read out-of-bounds based on that
 * trusted-looking length byte - {@code messageSize = min(clampedMsgSize,
 * 2 + msg[1])} guarantees the effective size never exceeds the real buffer.
 * These cases feed short real buffers whose msg[1] claims a large packed
 * length and assert no out-of-bounds read occurs.
 */
TEST_CASE(
    "rawMessageFormatter does not read out-of-bounds on an invalid (too large) "
    "ASDU length",
    "[remote::helper]") {
  // Real buffers of these sizes, each declaring a large packed length in the
  // length octet (0xFD = 253, 0xFF = 255). The declared length exceeds the
  // actual buffer, which must not lead to an out-of-bounds read.
  const std::size_t bufferLengths[] = {4, 5, 6, 7, 11, 12, 13, 14, 15};
  const uint8_t lengthOctets[] = {0xFD, 0xFF};

  for (const auto length : bufferLengths) {
    for (const auto lenByte : lengthOctets) {
      std::vector<uint_fast8_t> msg(length, 0);
      msg[0] = 0x68;    // start octet
      msg[1] = lenByte; // UNTRUSTED oversized length octet
      msg[2] = 0x00;    // bit0 clear -> "I-format" branch
      for (std::size_t i = 3; i < length; i++) {
        msg[i] = static_cast<uint_fast8_t>(0xA0 + i);
      }

      REQUIRE_NOTHROW(
          Remote::rawMessageFormatter(msg.data(), static_cast<int>(length)));
    }
  }
}

TEST_CASE(
    "rawMessageDictionaryFormatter does not read out-of-bounds on an invalid "
    "(too large) ASDU length",
    "[remote::helper]") {
  // Real buffers claiming a larger length octet than the actual buffer must
  // never be read out-of-bounds, for any received length. tests/main.cpp loads
  // the c104 module so the IEC60870_5_TypeID enum used by the dict formatter is
  // registered, mirroring production.
  const std::size_t bufferLengths[] = {4,  5,  6,  7,  8,  9,
                                       10, 11, 12, 13, 14, 15};
  const uint8_t lengthOctets[] = {0xFD, 0xFF};

  for (const auto length : bufferLengths) {
    for (const auto lenByte : lengthOctets) {
      std::vector<uint_fast8_t> msg(length, 0);
      msg[0] = 0x68;
      msg[1] = lenByte;
      msg[2] = 0x00; // "I-format" branch
      for (std::size_t i = 3; i < length; i++) {
        msg[i] = static_cast<uint_fast8_t>(0xA0 + i);
      }

      REQUIRE_NOTHROW(Remote::rawMessageDictionaryFormatter(
          msg.data(), static_cast<int>(length)));
    }
  }
}
TEST_CASE("rawMessageFormatter returns safe placeholder for tiny buffers",
          "[remote::helper]") {
  const uint_fast8_t msg[] = {0x68, 0x03, 0x00};
  const auto result = Remote::rawMessageFormatter(msg, 3);
  REQUIRE(result.find("INVALID_MESSAGE") != std::string::npos);
}

TEST_CASE(
    "rawMessageDictionaryFormatter returns safe placeholder for tiny buffers",
    "[remote::helper]") {
  const uint_fast8_t msg[] = {0x68, 0x03, 0x00};
  auto result = Remote::rawMessageDictionaryFormatter(msg, 3);
  REQUIRE(result.contains("format"));
  REQUIRE(result["format"].cast<std::string>() == "INVALID");
}

/**
 * Regression: a full valid 255-octet I-format message must still parse
 * correctly - the normal path must be unchanged by the bounds guards.
 */
TEST_CASE("rawMessageFormatter parses a full valid 255-octet I-format message",
          "[remote::helper]") {
  // 255-octet APDU: start octet 0x68, length octet 0xFD (255 - 2 = 253),
  // the rest of the frame body
  std::vector<uint_fast8_t> msg(255, 0);
  msg[0] = 0x68;
  msg[1] = 0xFD;
  // I-format control field byte 0: bit0 = 0 (I-format)
  msg[2] = 0x00;
  msg[3] = 0x00;
  msg[4] = 0x00;
  msg[5] = 0x00;
  // ASDU header
  msg[IEC60870_TYPEID_OFFSET] =
      static_cast<uint_fast8_t>(IEC60870_5_TypeID::M_SP_NA_1);
  msg[IEC60870_STRUCTURE_OFFSET] = 1; // one information object, no sequence
  msg[IEC60870_MSGINFO_OFFSET] =
      static_cast<uint_fast8_t>(CS101_COT_SPONTANEOUS);
  msg[IEC60870_TARGETCA1_OFFSET] = 0x2F;
  msg[IEC60870_TARGETCA2_OFFSET] = 0x00;
  msg[IEC60870_SOURCECA_OFFSET] = 0x00;
  // first information object address (offset 12..14)
  msg[IEC60870_OBJECT_OFFSET] = 0x0B;
  msg[IEC60870_OBJECT_OFFSET + 1] = 0x00;
  msg[IEC60870_OBJECT_OFFSET + 2] = 0x00;

  const auto result = Remote::rawMessageFormatter(msg.data(), 255);
  // parsed full header must be present
  REQUIRE(result.find("I-Format") != std::string::npos);
  REQUIRE(result.find("| CA 47") != std::string::npos);
  REQUIRE(result.find("1st IOA 11") != std::string::npos);
  // no APDU length overflow flag for a valid maximal message
  REQUIRE(result.find("APDU_LENGTH_EXCEEDED") == std::string::npos);
}
