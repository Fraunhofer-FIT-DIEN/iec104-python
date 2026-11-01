/**
 * Copyright 2026 Fraunhofer Institute for Applied Information Technology
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
 *  @file test_enum_sweep.cpp
 *  @brief Catch2 coverage tests that sweep the Python-exposed enums (Qoi,
 *  Rqt, Frz, Init, Type) registered in src/python.cpp through the embedded
 *  interpreter and assert their member names and values match the public API
 *  surface declared in c104/__init__.pyi.
 *
 *  SCOPE: this file deliberately keeps only the checks that are NOT already
 *  covered elsewhere in the test suite:
 *    * the exact member-name -> numeric-value contract of the named enums
 *      Qoi/Rqt/Frz/Init. tests/test_pyi_enums.py only iterates the members
 *      and checks their values are non-None; it never asserts the underlying
 *      IEC 60870 protocol constants, so an exact sweep catches value drift
 *      when lib60870 is upgraded.
 *    * getattr(Type, name) reaching the identical registered member object
 *      and the C++ TypeID_toString converter across every registered Type
 *      member (no other test sweeps it over the full member set).
 *  Redundant checks deliberately removed:
 *    * ConnectionInit_toString sweep -- already covered in full (including
 *      the invalid-value "UNKNOWN" fallback) by tests/test_enums_types.cpp.
 *    * generic "members present / .name round-trip / getattr" checks -- already
 *      covered by tests/test_pyi_enums.py EnumStatusMembersTestCase.
 *
 *  NOTE ON GIL: tests/main.cpp keeps a single py::scoped_interpreter alive for
 *  the whole process (no second interpreter is ever created here) and imports
 *  c104 at start-up; we simply import it again, which returns the already
 *  registered module. All enum introspection happens under the GIL that
 *  pybind11 acquires on the Python API calls below.
 */

#include <catch2/catch_test_macros.hpp>

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include <map>
#include <set>
#include <string>

#include "enums.h"

namespace py = pybind11;

// ---------------------------------------------------------------------------
// Expected member name -> value tables transcribed from c104/__init__.pyi and
// the IEC 60870-5-101 enum definitions in src/enums.h / lib60870.
// ---------------------------------------------------------------------------

// Qoi: CS101_QualifierOfInterrogation -- STATION + GROUP_1..GROUP_16 (17).
const std::map<std::string, int> kExpectedQoi = {
    {"STATION", QOI_STATION},   {"GROUP_1", QOI_GROUP_1},
    {"GROUP_2", QOI_GROUP_2},   {"GROUP_3", QOI_GROUP_3},
    {"GROUP_4", QOI_GROUP_4},   {"GROUP_5", QOI_GROUP_5},
    {"GROUP_6", QOI_GROUP_6},   {"GROUP_7", QOI_GROUP_7},
    {"GROUP_8", QOI_GROUP_8},   {"GROUP_9", QOI_GROUP_9},
    {"GROUP_10", QOI_GROUP_10}, {"GROUP_11", QOI_GROUP_11},
    {"GROUP_12", QOI_GROUP_12}, {"GROUP_13", QOI_GROUP_13},
    {"GROUP_14", QOI_GROUP_14}, {"GROUP_15", QOI_GROUP_15},
    {"GROUP_16", QOI_GROUP_16},
};

// Rqt: CS101_QualifierOfCounterInterrogation -- GENERAL + GROUP_1..4 (5).
const std::map<std::string, int> kExpectedRqt = {
    {"GENERAL", IEC60870_QCC_RQT_GENERAL},
    {"GROUP_1", IEC60870_QCC_RQT_GROUP_1},
    {"GROUP_2", IEC60870_QCC_RQT_GROUP_2},
    {"GROUP_3", IEC60870_QCC_RQT_GROUP_3},
    {"GROUP_4", IEC60870_QCC_RQT_GROUP_4},
};

// Frz: CS101_FreezeOfCounterInterrogation is a scoped enum class, hence the
// explicit static_cast<int> (no implicit conversion to int in C++).
const std::map<std::string, int> kExpectedFrz = {
    {"READ", static_cast<int>(CS101_FreezeOfCounterInterrogation::READ)},
    {"FREEZE_WITHOUT_RESET",
     static_cast<int>(
         CS101_FreezeOfCounterInterrogation::FREEZE_WITHOUT_RESET)},
    {"FREEZE_WITH_RESET",
     static_cast<int>(CS101_FreezeOfCounterInterrogation::FREEZE_WITH_RESET)},
    {"COUNTER_RESET",
     static_cast<int>(CS101_FreezeOfCounterInterrogation::COUNTER_RESET)},
};

// Init: ConnectionInit (5).
const std::map<std::string, int> kExpectedInit = {
    {"ALL", INIT_ALL},
    {"INTERROGATION", INIT_INTERROGATION},
    {"CLOCK_SYNC", INIT_CLOCK_SYNC},
    {"MUTED", INIT_MUTED},
    {"NONE", INIT_NONE},
};

// Type: IEC60870_5_TypeID -- the full set declared in c104/__init__.pyi (56).
// The .pyi does not document numeric TypeID values, so only the exact name set
// is enforced here.
const std::set<std::string> kExpectedTypeNames = {
    "C_BO_NA_1", "C_BO_TA_1", "C_CD_NA_1", "C_CI_NA_1", "C_CS_NA_1",
    "C_DC_NA_1", "C_DC_TA_1", "C_IC_NA_1", "C_RC_NA_1", "C_RC_TA_1",
    "C_RD_NA_1", "C_RP_NA_1", "C_SC_NA_1", "C_SC_TA_1", "C_SE_NA_1",
    "C_SE_NB_1", "C_SE_NC_1", "C_SE_TA_1", "C_SE_TB_1", "C_SE_TC_1",
    "C_TS_NA_1", "C_TS_TA_1", "M_BO_NA_1", "M_BO_TA_1", "M_BO_TB_1",
    "M_DP_NA_1", "M_DP_TA_1", "M_DP_TB_1", "M_EI_NA_1", "M_EP_TA_1",
    "M_EP_TB_1", "M_EP_TC_1", "M_EP_TD_1", "M_EP_TE_1", "M_EP_TF_1",
    "M_IT_NA_1", "M_IT_TA_1", "M_IT_TB_1", "M_ME_NA_1", "M_ME_NB_1",
    "M_ME_NC_1", "M_ME_ND_1", "M_ME_TA_1", "M_ME_TB_1", "M_ME_TC_1",
    "M_ME_TD_1", "M_ME_TE_1", "M_ME_TF_1", "M_PS_NA_1", "M_SP_NA_1",
    "M_SP_TA_1", "M_SP_TB_1", "M_ST_NA_1", "M_ST_TA_1", "M_ST_TB_1",
};

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

/** The registered enum's __members__ dict, keyed by member name. */
static py::dict enumMembers(const std::string &enumName) {
  py::object enumType = py::module_::import("c104").attr(enumName.c_str());
  return enumType.attr("__members__").cast<py::dict>();
}

/** Collect the member name -> int value map for an enum's __members__. */
static std::map<std::string, int> membersToMap(const py::dict &members) {
  std::map<std::string, int> result;
  for (auto item : members) {
    const std::string name = item.first.cast<std::string>();
    const int value = item.second.attr("value").cast<int>();
    result.emplace(name, value);
  }
  return result;
}

/**
 * Verify the exact name->value contract of a named enum. Exact map equality
 * against the expected table covers "no missing", "no extra" and "each value
 * matches" in one assertion. Additionally each member must also be reachable
 * via getattr(Type, name) as the identical object.
 */
static void verifyNamedEnum(const std::string &enumName,
                            const std::map<std::string, int> &expected) {
  py::object enumType = py::module_::import("c104").attr(enumName.c_str());
  auto members = enumMembers(enumName);
  auto registered = membersToMap(members);
  // exact member set + numeric values
  REQUIRE(registered == expected);
  for (const auto &[name, value] : registered) {
    // getattr(Type, name) reaches the very same member object as __members__
    py::object byGetAttr = enumType.attr(name.c_str());
    py::object memberObj = members[py::str(name)];
    REQUIRE(byGetAttr.ptr() == memberObj.ptr());
    (void)value;
  }
}

// ---------------------------------------------------------------------------
// Qoi / Rqt / Frz / Init -- exact name+value contract
// ---------------------------------------------------------------------------
TEST_CASE("Enum sweep: Qoi matches .pyi", "[enum-sweep]") {
  verifyNamedEnum("Qoi", kExpectedQoi);
}

TEST_CASE("Enum sweep: Rqt matches .pyi", "[enum-sweep]") {
  verifyNamedEnum("Rqt", kExpectedRqt);
}

TEST_CASE("Enum sweep: Frz matches .pyi", "[enum-sweep]") {
  verifyNamedEnum("Frz", kExpectedFrz);
}

TEST_CASE("Enum sweep: Init matches .pyi", "[enum-sweep]") {
  verifyNamedEnum("Init", kExpectedInit);
}

// ---------------------------------------------------------------------------
// Type -- full member set, Type[name] round-trip and TypeID_toString sweep
// ---------------------------------------------------------------------------
TEST_CASE("Enum sweep: Type full member set matches .pyi", "[enum-sweep]") {
  auto members = enumMembers("Type");
  std::set<std::string> registeredNames;
  for (auto item : members) {
    registeredNames.insert(item.first.cast<std::string>());
  }
  // exact name-set equality (no missing, no extra members)
  REQUIRE(registeredNames == kExpectedTypeNames);
}

TEST_CASE("Enum sweep: Type members round-trip and TypeID_toString covers all",
          "[enum-sweep]") {
  py::object Type = py::module_::import("c104").attr("Type");
  auto members = enumMembers("Type");
  for (auto item : members) {
    const std::string name = item.first.cast<std::string>();
    const IEC60870_5_TypeID id =
        static_cast<IEC60870_5_TypeID>(item.second.attr("value").cast<int>());
    // getattr(Type, name) reaches the very same member object as __members__
    py::object byGetAttr = Type.attr(name.c_str());
    py::object memberObj = members[py::str(name)];
    REQUIRE(byGetAttr.ptr() == memberObj.ptr());
    // the C++ TypeID_toString converter yields the canonical name, never the
    // "unknown" fallback, for every registered member
    const std::string converted = TypeID_toString(id);
    REQUIRE(converted == name);
    REQUIRE(converted != "unknown");
  }
}
