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

#include <pybind11/pybind11.h>

#include <string>

namespace py = pybind11;

/**
 * A minimal but valid I-format APDU carrying a single M_SP_NA_1 information
 * object (single-point, ON) for common address 47 and IOA 11. Used to exercise
 * the module-level explain_bytes / explain_bytes_dict functions with a frame
 * the raw message formatter fully understands.
 */
const unsigned char kValidApdu[] = {
    0x68,       // start octet
    0x0E,       // APDU length (14 = frame total minus start + length octets)
    0x00, 0x00, // I-format control field
    0x00, 0x00, //            (send/receive sequence numbers = 0)
    0x01,       // TypeID: M_SP_NA_1
    0x01,       // VSQ: 1 information object, no sequence
    0x03, 0x00, // COT: spontaneous (cause 3)
    0x2F, 0x00, // common address 47
    0x0B, 0x00, 0x00, // information object address 11
    0x01              // SIQ: single-point ON
};

/**
 * Bitwise-OR two c104.Debug enum values by delegating to the bound __or__
 * operator (py::object has no native bitwise operators in C++).
 */
py::object debug_or(py::object lhs, py::object rhs) {
  return lhs.attr("__or__")(rhs);
}

// ---------------------------------------------------------------------------
// explain_bytes / explain_bytes_dict
// ---------------------------------------------------------------------------

TEST_CASE("Module functions: explain_bytes renders a valid APDU",
          "[module-functions]") {
  // Given: a valid APDU frame as Python bytes and the explain_bytes module fn
  auto c104 = py::module_::import("c104");
  py::bytes apdu(reinterpret_cast<const char *>(kValidApdu),
                 sizeof(kValidApdu));

  // When: explaining the frame
  auto result =
      c104.attr("explain_bytes")(py::arg("apdu") = apdu).cast<std::string>();

  // Then: a non-empty human readable dump containing the parsed frame fields
  REQUIRE_FALSE(result.empty());
  REQUIRE(result.find("I-Format") != std::string::npos);
  // common address 47 and information object address 11 appear in the dump
  REQUIRE(result.find("CA 47") != std::string::npos);
  REQUIRE(result.find("1st IOA 11") != std::string::npos);
}

TEST_CASE("Module functions: explain_bytes_dict returns structured dict",
          "[module-functions]") {
  // Given: the same valid APDU frame as Python bytes
  auto c104 = py::module_::import("c104");
  py::bytes apdu(reinterpret_cast<const char *>(kValidApdu),
                 sizeof(kValidApdu));

  // When: converting the frame to a dictionary
  py::dict result =
      c104.attr("explain_bytes_dict")(py::arg("apdu") = apdu).cast<py::dict>();

  // Then: a non-empty dict with the expected structural keys
  REQUIRE_FALSE(result.empty());
  REQUIRE(result.contains("format"));
  // I-format frame is identified as such
  REQUIRE(result["format"].cast<std::string>() == "I");
}

// ---------------------------------------------------------------------------
// debug_mode round-trip
// ---------------------------------------------------------------------------

TEST_CASE("Module functions: set_debug_mode / get_debug_mode round-trip",
          "[module-functions]") {
  auto c104 = py::module_::import("c104");
  py::object Debug = c104.attr("Debug");
  auto set_debug_mode = c104.attr("set_debug_mode");
  auto get_debug_mode = c104.attr("get_debug_mode");
  auto enable_debug = c104.attr("enable_debug");
  auto disable_debug = c104.attr("disable_debug");

  auto combined = debug_or(Debug.attr("Client"),
                           Debug.attr("Server")); // Debug.Client | Debug.Server

  // Given: a clean debug mode
  set_debug_mode(Debug());

  // When: setting a combined mode, then reading it back
  set_debug_mode(combined);
  auto read_back = get_debug_mode();

  // Then: the combined flags are preserved
  REQUIRE(py::cast<bool>(read_back.attr("__eq__")(combined)));

  // When: enabling an additional flag
  enable_debug(Debug.attr("Connection"));
  auto after_enable = get_debug_mode();
  auto expected_enable = debug_or(combined, Debug.attr("Connection"));

  // Then: the new flag was added
  REQUIRE(py::cast<bool>(after_enable.attr("__eq__")(expected_enable)));

  // When: disabling one of the original flags
  disable_debug(Debug.attr("Server"));
  auto after_disable = get_debug_mode();
  auto expected_disable =
      debug_or(Debug.attr("Client"), Debug.attr("Connection"));

  // Then: only the disabled flag was removed
  REQUIRE(py::cast<bool>(after_disable.attr("__eq__")(expected_disable)));

  // cleanup: reset to no debug for later tests
  set_debug_mode(Debug());
}

// ---------------------------------------------------------------------------
// Client.get_connection dispatch
// ---------------------------------------------------------------------------

TEST_CASE("Module functions: get_connection dispatch by ip",
          "[module-functions]") {
  auto c104 = py::module_::import("c104");
  py::object client = c104.attr("Client")(py::arg("tick_rate_ms") = 100);
  py::object connection = client.attr("add_connection")("127.0.0.1", 24230);
  REQUIRE_FALSE(connection.is_none());

  // When: looking up by ip (and default port 2404 would not match, so use the
  // same port the connection was created with)
  auto by_ip = client.attr("get_connection")(py::arg("ip") = "127.0.0.1",
                                             py::arg("port") = 24230);

  // Then: the added connection object is returned
  REQUIRE_FALSE(by_ip.is_none());
  REQUIRE(py::cast<bool>(by_ip.attr("__eq__")(connection)));

  client.attr("stop")();
}

TEST_CASE("Module functions: get_connection dispatch by common_address",
          "[module-functions]") {
  auto c104 = py::module_::import("c104");
  py::object client = c104.attr("Client")(py::arg("tick_rate_ms") = 100);
  py::object connection = client.attr("add_connection")("127.0.0.1", 24231);
  REQUIRE_FALSE(connection.is_none());

  // Given: a station with common address 47 on that connection
  auto station = connection.attr("add_station")(47);
  REQUIRE_FALSE(station.is_none());

  // When: looking up the connection by common address
  auto by_ca = client.attr("get_connection")(py::arg("common_address") = 47);

  // Then: the connection hosting that station is returned
  REQUIRE_FALSE(by_ca.is_none());
  REQUIRE(py::cast<bool>(by_ca.attr("__eq__")(connection)));

  client.attr("stop")();
}

TEST_CASE(
    "Module functions: get_connection with no arguments raises ValueError",
    "[module-functions]") {
  auto c104 = py::module_::import("c104");
  py::object client = c104.attr("Client")(py::arg("tick_rate_ms") = 100);

  // When: calling get_connection without ip and without common_address
  // Then: a builtin (ValueError) is raised
  try {
    client.attr("get_connection")();
    FAIL("get_connection() with no arguments must raise");
  } catch (const py::error_already_set &e) {
    REQUIRE(e.type() == PyExc_ValueError);
  }

  client.attr("stop")();
}

TEST_CASE(
    "Module functions: get_connection with empty strings raises ValueError",
    "[module-functions]") {
  auto c104 = py::module_::import("c104");
  py::object client = c104.attr("Client")(py::arg("tick_rate_ms") = 100);

  // When: calling get_connection with an empty ip and no common_address
  // Then: a builtin (ValueError) is raised
  try {
    client.attr("get_connection")(py::arg("ip") = "");
    FAIL("get_connection(ip='') must raise");
  } catch (const py::error_already_set &e) {
    REQUIRE(e.type() == PyExc_ValueError);
  }

  client.attr("stop")();
}
