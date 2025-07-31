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
 * @file python.cpp
 * @brief python module and bindings
 *
 * @package iec104-python
 * @namespace
 *
 * @authors Martin Unkel <martin.unkel@fit.fraunhofer.de>
 *
 */

#include <pybind11/chrono.h>
#include <pybind11/operators.h>
#include <pybind11/stl.h>
#include <sstream>

#ifdef VERSION_INFO
#define PY_MODULE(var) PYBIND11_MODULE(_c104, var, py::mod_gil_not_used())
#else
#define VERSION_INFO "embedded"
#include <pybind11/embed.h>
#define PY_MODULE(var) PYBIND11_EMBEDDED_MODULE(c104, var)
#endif

#include "Client.h"
#include "Server.h"
#include "numbers.h"
#include "remote/Helper.h"
#include "types.h"

using namespace pybind11::literals;

void init_client(py::module_ &);
void init_server(py::module_ &);
void init_remote_security(py::module_ &);
void init_remote_connection(py::module_ &);
void init_object_datapoint(py::module_ &);
void init_object_station(py::module_ &);
void init_object_datetime(py::module_ &);
void init_object_information(py::module_ &);
void init_remote_batch(py::module_ &);
void init_remote_message(py::module_ &);

// Bind Number with Template
template <typename T>
void bind_Number(py::module &m, const std::string &name,
                 const bool with_float = false) {
  auto py_number =
      py::class_<T, std::shared_ptr<T>>(m, name.c_str())
          .def(py::init<int>(), R"def(__init__(self, value: int) -> None

create a fixed-length integer instance

Parameters
----------
value: int
    the value

Raises
------
ValueError
    cannot convert value to fixed-length integer)def")
          // Overloading operators with different types
          .def(py::self + int())
          .def(py::self - int())
          .def(py::self * int())
          .def(py::self / int())
          .def(py::self += int())
          .def(py::self -= int())
          .def(py::self *= int())
          .def(py::self /= int())
          .def("__int__", [](T &a) { return static_cast<int>(a.get()); })
          .def("__float__", [](T &a) { return static_cast<float>(a.get()); })
          .def("__str__", [name](T &a) { return std::to_string(a.get()); })
          .def("__repr__", [name](const T &a) {
            return "<c104." + name + " value=" + std::to_string(a.get()) + ">";
          });

  if (with_float) {
    py_number
        .def(py::init<float>(), R"def(__init__(self, value: int | float) -> None

create a fixed-length float instance

Parameters
----------
value: int | float
    the value

Raises
------
ValueError
    cannot convert value to fixed-length float)def")
        .def_property_readonly("min", &T::getMin,
                               "float: minimum value (read-only)")
        .def_property_readonly("max", &T::getMax,
                               "float: maximum value (read-only)")
        .def(py::self + float())
        .def(py::self - float())
        .def(py::self * float())
        .def(py::self / float())
        .def(py::self += float())
        .def(py::self -= float())
        .def(py::self *= float())
        .def(py::self /= float());
  } else {
    py_number
        .def_property_readonly("min", &T::getMin,
                               "int: minimum value (read-only)")
        .def_property_readonly("max", &T::getMax,
                               "int: maximum value (read-only)");
  }
}

template <typename T>
void bind_BitFlags_ops(py::class_<T> &py_bit_enum, std::string (*fn)(const T &),
                       bool is_quality = false) {
  py_bit_enum.def(py::self & py::self)
      .def(py::self | py::self)
      .def(py::self ^ py::self)
      .def(py::self == py::self)
      .def(py::self != py::self)
      .def(py::self &= py::self)
      .def(py::self |= py::self)
      .def(py::self ^= py::self)
      .def("__contains__",
           [](const T &mode, const T &flag) { return test(mode, flag); })
      .def(
          "is_any", [](const T &mode) { return is_any(mode); },
          R"def(is_any(self) -> bool

test if there are any bits set)def");

  if (is_quality) {
    py_bit_enum.def(
        "is_good", [](const T &mode) { return is_none(mode); },
        R"def(is_good(self) -> bool

test if no quality problems are set)def");
  } else {
    py_bit_enum.def(
        "is_none", [](const T &mode) { return is_none(mode); },
        R"def(is_none(self) -> bool

test if no bits are set)def");
  }

  py_bit_enum.attr("__str__") =
      py::cpp_function(fn, py::name("__str__"), py::is_method(py_bit_enum));
  py_bit_enum.attr("__repr__") =
      py::cpp_function(fn, py::name("__repr__"), py::is_method(py_bit_enum));
}

/**
 * @brief Generates a string representation of the raw bytes data from a Python
 * bytes object.
 *
 * This function interprets the contents of the provided Python bytes object as
 * a binary buffer, retrieves the data using a memory view, and formats it into
 * a string.
 *
 * @param obj A Python bytes object to be processed and explained.
 * @return std::string A formatted string representation of the raw byte data.
 */
std::string explain_bytes(const py::bytes &obj) {

  // PyObject * pymemview = PyMemoryView_FromObject(obj);
  // Py_buffer *buffer = PyMemoryView_GET_BUFFER(pymemview);
  py::memoryview memview(obj);
  Py_buffer *buffer = PyMemoryView_GET_BUFFER(memview.ptr());

  return Remote::rawMessageFormatter((unsigned char *)buffer->buf,
                                     (unsigned char)buffer->len);
}

/**
 * @brief Converts a Python bytes object into a formatted dictionary explaining
 * its contents.
 *
 * This function takes a Python bytes object, creates a memory view for
 * accessing its internal buffer, and utilizes a formatter function to represent
 * the raw message contents as a Python dictionary. The dictionary provides a
 * structured explanation of the data within the bytes object.
 *
 * @param obj A Python bytes object containing the raw byte data to be
 * explained.
 * @return py::dict A Python dictionary representing the structured format of
 * the raw byte data.
 */
py::dict explain_bytes_dict(const py::bytes &obj) {

  // PyObject * pymemview = PyMemoryView_FromObject(obj);
  // Py_buffer *buffer = PyMemoryView_GET_BUFFER(pymemview);
  py::memoryview memview(obj);
  Py_buffer *buffer = PyMemoryView_GET_BUFFER(memview.ptr());

  return Remote::rawMessageDictionaryFormatter((unsigned char *)buffer->buf,
                                               (unsigned char)buffer->len);
}

PY_MODULE(m) {
#ifdef _WIN32
  system("chcp 65001 > nul");
#endif

  py::options options;
  options.disable_function_signatures();
  options.disable_enum_members_docstring();

  py::enum_<InformationCategory>(
      m, "InfoCategory",
      "This enum contains categories of information object classes.")
      .value("MONITORING_STATUS", MONITORING_STATUS)
      .value("MONITORING_COUNTER", MONITORING_COUNTER)
      .value("MONITORING_PROTECTION", MONITORING_EVENT)
      .value("COMMAND", COMMAND)
      .value("PARAMETER", PARAMETER);

  py::enum_<IEC60870_5_TypeID>(m, "Type",
                               "This enum contains all valid IEC60870 message "
                               "types to interpret or create points.")
      .value("M_SP_NA_1", IEC60870_5_TypeID::M_SP_NA_1)
      .value("M_SP_TA_1", IEC60870_5_TypeID::M_SP_TA_1)
      .value("M_DP_NA_1", IEC60870_5_TypeID::M_DP_NA_1)
      .value("M_DP_TA_1", IEC60870_5_TypeID::M_DP_TA_1)
      .value("M_ST_NA_1", IEC60870_5_TypeID::M_ST_NA_1)
      .value("M_ST_TA_1", IEC60870_5_TypeID::M_ST_TA_1)
      .value("M_BO_NA_1", IEC60870_5_TypeID::M_BO_NA_1)
      .value("M_BO_TA_1", IEC60870_5_TypeID::M_BO_TA_1)
      .value("M_ME_NA_1", IEC60870_5_TypeID::M_ME_NA_1)
      .value("M_ME_TA_1", IEC60870_5_TypeID::M_ME_TA_1)
      .value("M_ME_NB_1", IEC60870_5_TypeID::M_ME_NB_1)
      .value("M_ME_TB_1", IEC60870_5_TypeID::M_ME_TB_1)
      .value("M_ME_NC_1", IEC60870_5_TypeID::M_ME_NC_1)
      .value("M_ME_TC_1", IEC60870_5_TypeID::M_ME_TC_1)
      .value("M_IT_NA_1", IEC60870_5_TypeID::M_IT_NA_1)
      .value("M_IT_TA_1", IEC60870_5_TypeID::M_IT_TA_1)
      .value("M_EP_TA_1", IEC60870_5_TypeID::M_EP_TA_1)
      .value("M_EP_TB_1", IEC60870_5_TypeID::M_EP_TB_1)
      .value("M_EP_TC_1", IEC60870_5_TypeID::M_EP_TC_1)
      .value("M_PS_NA_1", IEC60870_5_TypeID::M_PS_NA_1)
      .value("M_ME_ND_1", IEC60870_5_TypeID::M_ME_ND_1)
      .value("M_SP_TB_1", IEC60870_5_TypeID::M_SP_TB_1)
      .value("M_DP_TB_1", IEC60870_5_TypeID::M_DP_TB_1)
      .value("M_ST_TB_1", IEC60870_5_TypeID::M_ST_TB_1)
      .value("M_BO_TB_1", IEC60870_5_TypeID::M_BO_TB_1)
      .value("M_ME_TD_1", IEC60870_5_TypeID::M_ME_TD_1)
      .value("M_ME_TE_1", IEC60870_5_TypeID::M_ME_TE_1)
      .value("M_ME_TF_1", IEC60870_5_TypeID::M_ME_TF_1)
      .value("M_IT_TB_1", IEC60870_5_TypeID::M_IT_TB_1)
      .value("M_EP_TD_1", IEC60870_5_TypeID::M_EP_TD_1)
      .value("M_EP_TE_1", IEC60870_5_TypeID::M_EP_TE_1)
      .value("M_EP_TF_1", IEC60870_5_TypeID::M_EP_TF_1)
      .value("C_SC_NA_1", IEC60870_5_TypeID::C_SC_NA_1)
      .value("C_DC_NA_1", IEC60870_5_TypeID::C_DC_NA_1)
      .value("C_RC_NA_1", IEC60870_5_TypeID::C_RC_NA_1)
      .value("C_SE_NA_1", IEC60870_5_TypeID::C_SE_NA_1)
      .value("C_SE_NB_1", IEC60870_5_TypeID::C_SE_NB_1)
      .value("C_SE_NC_1", IEC60870_5_TypeID::C_SE_NC_1)
      .value("C_BO_NA_1", IEC60870_5_TypeID::C_BO_NA_1)
      .value("C_SC_TA_1", IEC60870_5_TypeID::C_SC_TA_1)
      .value("C_DC_TA_1", IEC60870_5_TypeID::C_DC_TA_1)
      .value("C_RC_TA_1", IEC60870_5_TypeID::C_RC_TA_1)
      .value("C_SE_TA_1", IEC60870_5_TypeID::C_SE_TA_1)
      .value("C_SE_TB_1", IEC60870_5_TypeID::C_SE_TB_1)
      .value("C_SE_TC_1", IEC60870_5_TypeID::C_SE_TC_1)
      .value("C_BO_TA_1", IEC60870_5_TypeID::C_BO_TA_1)
      .value("M_EI_NA_1", IEC60870_5_TypeID::M_EI_NA_1)
      .value("C_IC_NA_1", IEC60870_5_TypeID::C_IC_NA_1)
      .value("C_CI_NA_1", IEC60870_5_TypeID::C_CI_NA_1)
      .value("C_RD_NA_1", IEC60870_5_TypeID::C_RD_NA_1)
      .value("C_CS_NA_1", IEC60870_5_TypeID::C_CS_NA_1)
      .value("C_TS_NA_1", IEC60870_5_TypeID::C_TS_NA_1)
      .value("C_RP_NA_1", IEC60870_5_TypeID::C_RP_NA_1)
      .value("C_CD_NA_1", IEC60870_5_TypeID::C_CD_NA_1)
      .value("C_TS_TA_1", IEC60870_5_TypeID::C_TS_TA_1);

  py::enum_<CS101_CauseOfTransmission>(
      m, "Cot",
      "This enum contains all valid IEC60870 transmission cause identifier to "
      "interpret message context.")
      .value("PERIODIC", CS101_CauseOfTransmission::CS101_COT_PERIODIC)
      .value("BACKGROUND_SCAN",
             CS101_CauseOfTransmission::CS101_COT_BACKGROUND_SCAN)
      .value("SPONTANEOUS", CS101_CauseOfTransmission::CS101_COT_SPONTANEOUS)
      .value("INITIALIZED", CS101_CauseOfTransmission::CS101_COT_INITIALIZED)
      .value("REQUEST", CS101_CauseOfTransmission::CS101_COT_REQUEST)
      .value("ACTIVATION", CS101_CauseOfTransmission::CS101_COT_ACTIVATION)
      .value("ACTIVATION_CON",
             CS101_CauseOfTransmission::CS101_COT_ACTIVATION_CON)
      .value("DEACTIVATION", CS101_CauseOfTransmission::CS101_COT_DEACTIVATION)
      .value("DEACTIVATION_CON",
             CS101_CauseOfTransmission::CS101_COT_DEACTIVATION_CON)
      .value("ACTIVATION_TERMINATION",
             CS101_CauseOfTransmission::CS101_COT_ACTIVATION_TERMINATION)
      .value("RETURN_INFO_REMOTE",
             CS101_CauseOfTransmission::CS101_COT_RETURN_INFO_REMOTE)
      .value("RETURN_INFO_LOCAL",
             CS101_CauseOfTransmission::CS101_COT_RETURN_INFO_LOCAL)
      .value("FILE_TRANSFER",
             CS101_CauseOfTransmission::CS101_COT_FILE_TRANSFER)
      .value("AUTHENTICATION",
             CS101_CauseOfTransmission::CS101_COT_AUTHENTICATION)
      .value(
          "MAINTENANCE_OF_AUTH_SESSION_KEY",
          CS101_CauseOfTransmission::CS101_COT_MAINTENANCE_OF_AUTH_SESSION_KEY)
      .value("MAINTENANCE_OF_USER_ROLE_AND_UPDATE_KEY",
             CS101_CauseOfTransmission::
                 CS101_COT_MAINTENANCE_OF_USER_ROLE_AND_UPDATE_KEY)
      .value("INTERROGATED_BY_STATION",
             CS101_CauseOfTransmission::CS101_COT_INTERROGATED_BY_STATION)
      .value("INTERROGATED_BY_GROUP_1",
             CS101_CauseOfTransmission::CS101_COT_INTERROGATED_BY_GROUP_1)
      .value("INTERROGATED_BY_GROUP_2",
             CS101_CauseOfTransmission::CS101_COT_INTERROGATED_BY_GROUP_2)
      .value("INTERROGATED_BY_GROUP_3",
             CS101_CauseOfTransmission::CS101_COT_INTERROGATED_BY_GROUP_3)
      .value("INTERROGATED_BY_GROUP_4",
             CS101_CauseOfTransmission::CS101_COT_INTERROGATED_BY_GROUP_4)
      .value("INTERROGATED_BY_GROUP_5",
             CS101_CauseOfTransmission::CS101_COT_INTERROGATED_BY_GROUP_5)
      .value("INTERROGATED_BY_GROUP_6",
             CS101_CauseOfTransmission::CS101_COT_INTERROGATED_BY_GROUP_6)
      .value("INTERROGATED_BY_GROUP_7",
             CS101_CauseOfTransmission::CS101_COT_INTERROGATED_BY_GROUP_7)
      .value("INTERROGATED_BY_GROUP_8",
             CS101_CauseOfTransmission::CS101_COT_INTERROGATED_BY_GROUP_8)
      .value("INTERROGATED_BY_GROUP_9",
             CS101_CauseOfTransmission::CS101_COT_INTERROGATED_BY_GROUP_9)
      .value("INTERROGATED_BY_GROUP_10",
             CS101_CauseOfTransmission::CS101_COT_INTERROGATED_BY_GROUP_10)
      .value("INTERROGATED_BY_GROUP_11",
             CS101_CauseOfTransmission::CS101_COT_INTERROGATED_BY_GROUP_11)
      .value("INTERROGATED_BY_GROUP_12",
             CS101_CauseOfTransmission::CS101_COT_INTERROGATED_BY_GROUP_12)
      .value("INTERROGATED_BY_GROUP_13",
             CS101_CauseOfTransmission::CS101_COT_INTERROGATED_BY_GROUP_13)
      .value("INTERROGATED_BY_GROUP_14",
             CS101_CauseOfTransmission::CS101_COT_INTERROGATED_BY_GROUP_14)
      .value("INTERROGATED_BY_GROUP_15",
             CS101_CauseOfTransmission::CS101_COT_INTERROGATED_BY_GROUP_15)
      .value("INTERROGATED_BY_GROUP_16",
             CS101_CauseOfTransmission::CS101_COT_INTERROGATED_BY_GROUP_16)
      .value("REQUESTED_BY_GENERAL_COUNTER",
             CS101_CauseOfTransmission::CS101_COT_REQUESTED_BY_GENERAL_COUNTER)
      .value("REQUESTED_BY_GROUP_1_COUNTER",
             CS101_CauseOfTransmission::CS101_COT_REQUESTED_BY_GROUP_1_COUNTER)
      .value("REQUESTED_BY_GROUP_2_COUNTER",
             CS101_CauseOfTransmission::CS101_COT_REQUESTED_BY_GROUP_2_COUNTER)
      .value("REQUESTED_BY_GROUP_3_COUNTER",
             CS101_CauseOfTransmission::CS101_COT_REQUESTED_BY_GROUP_3_COUNTER)
      .value("REQUESTED_BY_GROUP_4_COUNTER",
             CS101_CauseOfTransmission::CS101_COT_REQUESTED_BY_GROUP_4_COUNTER)
      .value("UNKNOWN_TYPE_ID",
             CS101_CauseOfTransmission::CS101_COT_UNKNOWN_TYPE_ID)
      .value("UNKNOWN_COT", CS101_CauseOfTransmission::CS101_COT_UNKNOWN_COT)
      .value("UNKNOWN_CA", CS101_CauseOfTransmission::CS101_COT_UNKNOWN_CA)
      .value("UNKNOWN_IOA", CS101_CauseOfTransmission::CS101_COT_UNKNOWN_IOA);

  py::enum_<CS101_QualifierOfCommand>(m, "Qoc",
                                      "This enum contains all valid IEC60870 "
                                      "qualifier of command duration options.")
      .value("NONE", CS101_QualifierOfCommand::NONE)
      .value("SHORT_PULSE", CS101_QualifierOfCommand::SHORT_PULSE)
      .value("LONG_PULSE", CS101_QualifierOfCommand::LONG_PULSE)
      .value("PERSISTENT", CS101_QualifierOfCommand::PERSISTENT);

  py::enum_<UnexpectedMessageCause>(
      m, "Umc",
      "This enum contains all unexpected message cause identifier to interpret "
      "error context.")
      .value("NO_ERROR_CAUSE", NO_ERROR_CAUSE)
      .value("UNKNOWN_TYPE_ID", UNKNOWN_TYPE_ID)
      .value("UNKNOWN_COT", UNKNOWN_COT)
      .value("UNKNOWN_CA", UNKNOWN_CA)
      .value("UNKNOWN_IOA", UNKNOWN_IOA)
      .value("INVALID_COT", INVALID_COT)
      .value("INVALID_TYPE_ID", INVALID_TYPE_ID)
      .value("UNIMPLEMENTED_GROUP", CUSTOM_UNIMPLEMENTED_GROUP);

  py::enum_<ConnectionInit>(
      m, "Init",
      "This enum contains all connection init command options. Everytime the "
      "connection is established the client will behave as follows:")
      .value("ALL", INIT_ALL,
             "Unmute the connection, send an interrogation command and after "
             "that send a clock synchronization command")
      .value("INTERROGATION", INIT_INTERROGATION,
             "Unmute the connection and send an interrogation command")
      .value("CLOCK_SYNC", INIT_CLOCK_SYNC,
             "Unmute the connection and send a clock synchronization command")
      .value("NONE", INIT_NONE,
             "Unmute the connection, but without triggering a command")
      .value("MUTED", INIT_MUTED,
             "Act as a redundancy client without active communication");

  py::enum_<ConnectionState>(m, "ConnectionState",
                             "This enum contains all link states for "
                             "connection state machine behaviour.")
      .value("CLOSED", CLOSED, "The connection is closed")
      .value("CLOSED_AWAIT_OPEN", CLOSED_AWAIT_OPEN,
             "The connection is dialing")
      .value("CLOSED_AWAIT_RECONNECT", CLOSED_AWAIT_RECONNECT,
             "The connection will retry dialing soon")
      .value("OPEN", OPEN,
             "The connection is established and active/unmuted, with no init "
             "commands outstanding")
      .value("OPEN_MUTED", OPEN_MUTED,
             "The connection is established and inactive/muted, with no init "
             "commands outstanding")
      .value("OPEN_AWAIT_CLOSED", OPEN_AWAIT_CLOSED,
             "The connection is about to close soon");

  py::enum_<CommandResponseState>(
      m, "ResponseState",
      "This enum contains all command response states, that add the ability to "
      "control the servers command response behaviour via python callbacks "
      "return value.")
      .value("FAILURE", RESPONSE_STATE_FAILURE)
      .value("SUCCESS", RESPONSE_STATE_SUCCESS)
      .value("NONE", RESPONSE_STATE_NONE);

  py::enum_<CommandTransmissionMode>(
      m, "CommandMode",
      "This enum contains all command transmission modes a client"
      "may use to send commands.")
      .value("DIRECT", DIRECT_COMMAND,
             "The value can be set without any selection procedure")
      .value("SELECT_AND_EXECUTE", SELECT_AND_EXECUTE_COMMAND,
             "The client has to send a select command first to get exclusive "
             "access to the points value and can then send an updated value "
             "command. The selection automatically ends by receiving the value "
             "command.");

  py::enum_<CS101_QualifierOfInterrogation>(
      m, "Qoi",
      "This enum contains all valid IEC60870 qualifier for an interrogation "
      "command.")
      .value("STATION", QOI_STATION)
      .value("GROUP_1", QOI_GROUP_1)
      .value("GROUP_2", QOI_GROUP_2)
      .value("GROUP_3", QOI_GROUP_3)
      .value("GROUP_4", QOI_GROUP_4)
      .value("GROUP_5", QOI_GROUP_5)
      .value("GROUP_6", QOI_GROUP_6)
      .value("GROUP_7", QOI_GROUP_7)
      .value("GROUP_8", QOI_GROUP_8)
      .value("GROUP_9", QOI_GROUP_9)
      .value("GROUP_10", QOI_GROUP_10)
      .value("GROUP_11", QOI_GROUP_11)
      .value("GROUP_12", QOI_GROUP_12)
      .value("GROUP_13", QOI_GROUP_13)
      .value("GROUP_14", QOI_GROUP_14)
      .value("GROUP_15", QOI_GROUP_15)
      .value("GROUP_16", QOI_GROUP_16);

  py::enum_<CS101_QualifierOfCounterInterrogation>(
      m, "Rqt",
      "This enum contains all valid IEC60870 qualifier for a counter "
      "interrogation "
      "command.")
      .value("GENERAL", CS101_QualifierOfCounterInterrogation::GENERAL)
      .value("GROUP_1", CS101_QualifierOfCounterInterrogation::GROUP_1)
      .value("GROUP_2", CS101_QualifierOfCounterInterrogation::GROUP_2)
      .value("GROUP_3", CS101_QualifierOfCounterInterrogation::GROUP_3)
      .value("GROUP_4", CS101_QualifierOfCounterInterrogation::GROUP_4);

  py::enum_<CS101_FreezeOfCounterInterrogation>(
      m, "Frz",
      "This enum contains all valid IEC60870 freeze behaviour for a counter "
      "interrogation "
      "command.")
      .value("READ", CS101_FreezeOfCounterInterrogation::READ)
      .value("FREEZE_WITHOUT_RESET",
             CS101_FreezeOfCounterInterrogation::FREEZE_WITHOUT_RESET)
      .value("FREEZE_WITH_RESET",
             CS101_FreezeOfCounterInterrogation::FREEZE_WITH_RESET)
      .value("COUNTER_RESET",
             CS101_FreezeOfCounterInterrogation::COUNTER_RESET);

  py::enum_<CS101_CauseOfInitialization>(
      m, "Coi",
      "This enum contains all valid IEC60870 cause of initialization values.")
      .value("LOCAL_POWER_ON", CS101_CauseOfInitialization::LOCAL_POWER_ON)
      .value("LOCAL_MANUAL_RESET",
             CS101_CauseOfInitialization::LOCAL_MANUAL_RESET)
      .value("REMOTE_RESET", CS101_CauseOfInitialization::REMOTE_RESET);

  py::enum_<StepCommandValue>(
      m, "Step",
      "This enum contains all valid IEC60870 step command values to interpret "
      "and send step commands.")
      .value("INVALID_0", IEC60870_STEP_INVALID_0)
      .value("LOWER", IEC60870_STEP_LOWER)
      .value("HIGHER", IEC60870_STEP_HIGHER)
      .value("INVALID_3", IEC60870_STEP_INVALID_3);

  py::enum_<EventState>(m, "EventState",
                        "This enum contains all valid IEC60870 event states to "
                        "interpret and send event messages.")
      .value("INTERMEDIATE", IEC60870_EVENTSTATE_INDETERMINATE_0)
      .value("OFF", IEC60870_EVENTSTATE_OFF)
      .value("ON", IEC60870_EVENTSTATE_ON)
      .value("INDETERMINATE", IEC60870_EVENTSTATE_INDETERMINATE_3);

  py::enum_<DoublePointValue>(
      m, "Double",
      "This enum contains all valid IEC60870 step command values to identify "
      "and send step commands.")
      .value("INTERMEDIATE", IEC60870_DOUBLE_POINT_INTERMEDIATE)
      .value("OFF", IEC60870_DOUBLE_POINT_OFF)
      .value("ON", IEC60870_DOUBLE_POINT_ON)
      .value("INDETERMINATE", IEC60870_DOUBLE_POINT_INDETERMINATE);

  py::enum_<TLSConfigVersion>(m, "TlsVersion",
                              "This enum contains all supported TLS versions.")
      .value("NOT_SELECTED", TLS_VERSION_NOT_SELECTED)
      .value("SSL_3_0", TLS_VERSION_SSL_3_0)
      .value("TLS_1_0", TLS_VERSION_TLS_1_0)
      .value("TLS_1_1", TLS_VERSION_TLS_1_1)
      .value("TLS_1_2", TLS_VERSION_TLS_1_2)
      .value("TLS_1_3", TLS_VERSION_TLS_1_3);

  py::enum_<TLSCipherSuite>(
      m, "TlsCipher", "This enum contains all supported TLS ciphersuites.")
      .value("RSA_WITH_NULL_MD5", TLSCipherSuite::RSA_WITH_NULL_MD5)
      .value("RSA_WITH_NULL_SHA", TLSCipherSuite::RSA_WITH_NULL_SHA)
      .value("PSK_WITH_NULL_SHA", TLSCipherSuite::PSK_WITH_NULL_SHA)
      .value("DHE_PSK_WITH_NULL_SHA", TLSCipherSuite::DHE_PSK_WITH_NULL_SHA)
      .value("RSA_PSK_WITH_NULL_SHA", TLSCipherSuite::RSA_PSK_WITH_NULL_SHA)
      .value("RSA_WITH_AES_128_CBC_SHA",
             TLSCipherSuite::RSA_WITH_AES_128_CBC_SHA)
      .value("DHE_RSA_WITH_AES_128_CBC_SHA",
             TLSCipherSuite::DHE_RSA_WITH_AES_128_CBC_SHA)
      .value("RSA_WITH_AES_256_CBC_SHA",
             TLSCipherSuite::RSA_WITH_AES_256_CBC_SHA)
      .value("DHE_RSA_WITH_AES_256_CBC_SHA",
             TLSCipherSuite::DHE_RSA_WITH_AES_256_CBC_SHA)
      .value("RSA_WITH_NULL_SHA256", TLSCipherSuite::RSA_WITH_NULL_SHA256)
      .value("RSA_WITH_AES_128_CBC_SHA256",
             TLSCipherSuite::RSA_WITH_AES_128_CBC_SHA256)
      .value("RSA_WITH_AES_256_CBC_SHA256",
             TLSCipherSuite::RSA_WITH_AES_256_CBC_SHA256)
      .value("RSA_WITH_CAMELLIA_128_CBC_SHA",
             TLSCipherSuite::RSA_WITH_CAMELLIA_128_CBC_SHA)
      .value("DHE_RSA_WITH_CAMELLIA_128_CBC_SHA",
             TLSCipherSuite::DHE_RSA_WITH_CAMELLIA_128_CBC_SHA)
      .value("DHE_RSA_WITH_AES_128_CBC_SHA256",
             TLSCipherSuite::DHE_RSA_WITH_AES_128_CBC_SHA256)
      .value("DHE_RSA_WITH_AES_256_CBC_SHA256",
             TLSCipherSuite::DHE_RSA_WITH_AES_256_CBC_SHA256)
      .value("RSA_WITH_CAMELLIA_256_CBC_SHA",
             TLSCipherSuite::RSA_WITH_CAMELLIA_256_CBC_SHA)
      .value("DHE_RSA_WITH_CAMELLIA_256_CBC_SHA",
             TLSCipherSuite::DHE_RSA_WITH_CAMELLIA_256_CBC_SHA)
      .value("PSK_WITH_AES_128_CBC_SHA",
             TLSCipherSuite::PSK_WITH_AES_128_CBC_SHA)
      .value("PSK_WITH_AES_256_CBC_SHA",
             TLSCipherSuite::PSK_WITH_AES_256_CBC_SHA)
      .value("DHE_PSK_WITH_AES_128_CBC_SHA",
             TLSCipherSuite::DHE_PSK_WITH_AES_128_CBC_SHA)
      .value("DHE_PSK_WITH_AES_256_CBC_SHA",
             TLSCipherSuite::DHE_PSK_WITH_AES_256_CBC_SHA)
      .value("RSA_PSK_WITH_AES_128_CBC_SHA",
             TLSCipherSuite::RSA_PSK_WITH_AES_128_CBC_SHA)
      .value("RSA_PSK_WITH_AES_256_CBC_SHA",
             TLSCipherSuite::RSA_PSK_WITH_AES_256_CBC_SHA)
      .value("RSA_WITH_AES_128_GCM_SHA256",
             TLSCipherSuite::RSA_WITH_AES_128_GCM_SHA256)
      .value("RSA_WITH_AES_256_GCM_SHA384",
             TLSCipherSuite::RSA_WITH_AES_256_GCM_SHA384)
      .value("DHE_RSA_WITH_AES_128_GCM_SHA256",
             TLSCipherSuite::DHE_RSA_WITH_AES_128_GCM_SHA256)
      .value("DHE_RSA_WITH_AES_256_GCM_SHA384",
             TLSCipherSuite::DHE_RSA_WITH_AES_256_GCM_SHA384)
      .value("PSK_WITH_AES_128_GCM_SHA256",
             TLSCipherSuite::PSK_WITH_AES_128_GCM_SHA256)
      .value("PSK_WITH_AES_256_GCM_SHA384",
             TLSCipherSuite::PSK_WITH_AES_256_GCM_SHA384)
      .value("DHE_PSK_WITH_AES_128_GCM_SHA256",
             TLSCipherSuite::DHE_PSK_WITH_AES_128_GCM_SHA256)
      .value("DHE_PSK_WITH_AES_256_GCM_SHA384",
             TLSCipherSuite::DHE_PSK_WITH_AES_256_GCM_SHA384)
      .value("RSA_PSK_WITH_AES_128_GCM_SHA256",
             TLSCipherSuite::RSA_PSK_WITH_AES_128_GCM_SHA256)
      .value("RSA_PSK_WITH_AES_256_GCM_SHA384",
             TLSCipherSuite::RSA_PSK_WITH_AES_256_GCM_SHA384)
      .value("PSK_WITH_AES_128_CBC_SHA256",
             TLSCipherSuite::PSK_WITH_AES_128_CBC_SHA256)
      .value("PSK_WITH_AES_256_CBC_SHA384",
             TLSCipherSuite::PSK_WITH_AES_256_CBC_SHA384)
      .value("PSK_WITH_NULL_SHA256", TLSCipherSuite::PSK_WITH_NULL_SHA256)
      .value("PSK_WITH_NULL_SHA384", TLSCipherSuite::PSK_WITH_NULL_SHA384)
      .value("DHE_PSK_WITH_AES_128_CBC_SHA256",
             TLSCipherSuite::DHE_PSK_WITH_AES_128_CBC_SHA256)
      .value("DHE_PSK_WITH_AES_256_CBC_SHA384",
             TLSCipherSuite::DHE_PSK_WITH_AES_256_CBC_SHA384)
      .value("DHE_PSK_WITH_NULL_SHA256",
             TLSCipherSuite::DHE_PSK_WITH_NULL_SHA256)
      .value("DHE_PSK_WITH_NULL_SHA384",
             TLSCipherSuite::DHE_PSK_WITH_NULL_SHA384)
      .value("RSA_PSK_WITH_AES_128_CBC_SHA256",
             TLSCipherSuite::RSA_PSK_WITH_AES_128_CBC_SHA256)
      .value("RSA_PSK_WITH_AES_256_CBC_SHA384",
             TLSCipherSuite::RSA_PSK_WITH_AES_256_CBC_SHA384)
      .value("RSA_PSK_WITH_NULL_SHA256",
             TLSCipherSuite::RSA_PSK_WITH_NULL_SHA256)
      .value("RSA_PSK_WITH_NULL_SHA384",
             TLSCipherSuite::RSA_PSK_WITH_NULL_SHA384)
      .value("RSA_WITH_CAMELLIA_128_CBC_SHA256",
             TLSCipherSuite::RSA_WITH_CAMELLIA_128_CBC_SHA256)
      .value("DHE_RSA_WITH_CAMELLIA_128_CBC_SHA256",
             TLSCipherSuite::DHE_RSA_WITH_CAMELLIA_128_CBC_SHA256)
      .value("RSA_WITH_CAMELLIA_256_CBC_SHA256",
             TLSCipherSuite::RSA_WITH_CAMELLIA_256_CBC_SHA256)
      .value("DHE_RSA_WITH_CAMELLIA_256_CBC_SHA256",
             TLSCipherSuite::DHE_RSA_WITH_CAMELLIA_256_CBC_SHA256)
      .value("ECDH_ECDSA_WITH_NULL_SHA",
             TLSCipherSuite::ECDH_ECDSA_WITH_NULL_SHA)
      .value("ECDH_ECDSA_WITH_AES_128_CBC_SHA",
             TLSCipherSuite::ECDH_ECDSA_WITH_AES_128_CBC_SHA)
      .value("ECDH_ECDSA_WITH_AES_256_CBC_SHA",
             TLSCipherSuite::ECDH_ECDSA_WITH_AES_256_CBC_SHA)
      .value("ECDHE_ECDSA_WITH_NULL_SHA",
             TLSCipherSuite::ECDHE_ECDSA_WITH_NULL_SHA)
      .value("ECDHE_ECDSA_WITH_AES_128_CBC_SHA",
             TLSCipherSuite::ECDHE_ECDSA_WITH_AES_128_CBC_SHA)
      .value("ECDHE_ECDSA_WITH_AES_256_CBC_SHA",
             TLSCipherSuite::ECDHE_ECDSA_WITH_AES_256_CBC_SHA)
      .value("ECDH_RSA_WITH_NULL_SHA", TLSCipherSuite::ECDH_RSA_WITH_NULL_SHA)
      .value("ECDH_RSA_WITH_AES_128_CBC_SHA",
             TLSCipherSuite::ECDH_RSA_WITH_AES_128_CBC_SHA)
      .value("ECDH_RSA_WITH_AES_256_CBC_SHA",
             TLSCipherSuite::ECDH_RSA_WITH_AES_256_CBC_SHA)
      .value("ECDHE_RSA_WITH_NULL_SHA", TLSCipherSuite::ECDHE_RSA_WITH_NULL_SHA)
      .value("ECDHE_RSA_WITH_AES_128_CBC_SHA",
             TLSCipherSuite::ECDHE_RSA_WITH_AES_128_CBC_SHA)
      .value("ECDHE_RSA_WITH_AES_256_CBC_SHA",
             TLSCipherSuite::ECDHE_RSA_WITH_AES_256_CBC_SHA)
      .value("ECDHE_ECDSA_WITH_AES_128_CBC_SHA256",
             TLSCipherSuite::ECDHE_ECDSA_WITH_AES_128_CBC_SHA256)
      .value("ECDHE_ECDSA_WITH_AES_256_CBC_SHA384",
             TLSCipherSuite::ECDHE_ECDSA_WITH_AES_256_CBC_SHA384)
      .value("ECDH_ECDSA_WITH_AES_128_CBC_SHA256",
             TLSCipherSuite::ECDH_ECDSA_WITH_AES_128_CBC_SHA256)
      .value("ECDH_ECDSA_WITH_AES_256_CBC_SHA384",
             TLSCipherSuite::ECDH_ECDSA_WITH_AES_256_CBC_SHA384)
      .value("ECDHE_RSA_WITH_AES_128_CBC_SHA256",
             TLSCipherSuite::ECDHE_RSA_WITH_AES_128_CBC_SHA256)
      .value("ECDHE_RSA_WITH_AES_256_CBC_SHA384",
             TLSCipherSuite::ECDHE_RSA_WITH_AES_256_CBC_SHA384)
      .value("ECDH_RSA_WITH_AES_128_CBC_SHA256",
             TLSCipherSuite::ECDH_RSA_WITH_AES_128_CBC_SHA256)
      .value("ECDH_RSA_WITH_AES_256_CBC_SHA384",
             TLSCipherSuite::ECDH_RSA_WITH_AES_256_CBC_SHA384)
      .value("ECDHE_ECDSA_WITH_AES_128_GCM_SHA256",
             TLSCipherSuite::ECDHE_ECDSA_WITH_AES_128_GCM_SHA256)
      .value("ECDHE_ECDSA_WITH_AES_256_GCM_SHA384",
             TLSCipherSuite::ECDHE_ECDSA_WITH_AES_256_GCM_SHA384)
      .value("ECDH_ECDSA_WITH_AES_128_GCM_SHA256",
             TLSCipherSuite::ECDH_ECDSA_WITH_AES_128_GCM_SHA256)
      .value("ECDH_ECDSA_WITH_AES_256_GCM_SHA384",
             TLSCipherSuite::ECDH_ECDSA_WITH_AES_256_GCM_SHA384)
      .value("ECDHE_RSA_WITH_AES_128_GCM_SHA256",
             TLSCipherSuite::ECDHE_RSA_WITH_AES_128_GCM_SHA256)
      .value("ECDHE_RSA_WITH_AES_256_GCM_SHA384",
             TLSCipherSuite::ECDHE_RSA_WITH_AES_256_GCM_SHA384)
      .value("ECDH_RSA_WITH_AES_128_GCM_SHA256",
             TLSCipherSuite::ECDH_RSA_WITH_AES_128_GCM_SHA256)
      .value("ECDH_RSA_WITH_AES_256_GCM_SHA384",
             TLSCipherSuite::ECDH_RSA_WITH_AES_256_GCM_SHA384)
      .value("ECDHE_PSK_WITH_AES_128_CBC_SHA",
             TLSCipherSuite::ECDHE_PSK_WITH_AES_128_CBC_SHA)
      .value("ECDHE_PSK_WITH_AES_256_CBC_SHA",
             TLSCipherSuite::ECDHE_PSK_WITH_AES_256_CBC_SHA)
      .value("ECDHE_PSK_WITH_AES_128_CBC_SHA256",
             TLSCipherSuite::ECDHE_PSK_WITH_AES_128_CBC_SHA256)
      .value("ECDHE_PSK_WITH_AES_256_CBC_SHA384",
             TLSCipherSuite::ECDHE_PSK_WITH_AES_256_CBC_SHA384)
      .value("ECDHE_PSK_WITH_NULL_SHA", TLSCipherSuite::ECDHE_PSK_WITH_NULL_SHA)
      .value("ECDHE_PSK_WITH_NULL_SHA256",
             TLSCipherSuite::ECDHE_PSK_WITH_NULL_SHA256)
      .value("ECDHE_PSK_WITH_NULL_SHA384",
             TLSCipherSuite::ECDHE_PSK_WITH_NULL_SHA384)
      .value("RSA_WITH_ARIA_128_CBC_SHA256",
             TLSCipherSuite::RSA_WITH_ARIA_128_CBC_SHA256)
      .value("RSA_WITH_ARIA_256_CBC_SHA384",
             TLSCipherSuite::RSA_WITH_ARIA_256_CBC_SHA384)
      .value("DHE_RSA_WITH_ARIA_128_CBC_SHA256",
             TLSCipherSuite::DHE_RSA_WITH_ARIA_128_CBC_SHA256)
      .value("DHE_RSA_WITH_ARIA_256_CBC_SHA384",
             TLSCipherSuite::DHE_RSA_WITH_ARIA_256_CBC_SHA384)
      .value("ECDHE_ECDSA_WITH_ARIA_128_CBC_SHA256",
             TLSCipherSuite::ECDHE_ECDSA_WITH_ARIA_128_CBC_SHA256)
      .value("ECDHE_ECDSA_WITH_ARIA_256_CBC_SHA384",
             TLSCipherSuite::ECDHE_ECDSA_WITH_ARIA_256_CBC_SHA384)
      .value("ECDH_ECDSA_WITH_ARIA_128_CBC_SHA256",
             TLSCipherSuite::ECDH_ECDSA_WITH_ARIA_128_CBC_SHA256)
      .value("ECDH_ECDSA_WITH_ARIA_256_CBC_SHA384",
             TLSCipherSuite::ECDH_ECDSA_WITH_ARIA_256_CBC_SHA384)
      .value("ECDHE_RSA_WITH_ARIA_128_CBC_SHA256",
             TLSCipherSuite::ECDHE_RSA_WITH_ARIA_128_CBC_SHA256)
      .value("ECDHE_RSA_WITH_ARIA_256_CBC_SHA384",
             TLSCipherSuite::ECDHE_RSA_WITH_ARIA_256_CBC_SHA384)
      .value("ECDH_RSA_WITH_ARIA_128_CBC_SHA256",
             TLSCipherSuite::ECDH_RSA_WITH_ARIA_128_CBC_SHA256)
      .value("ECDH_RSA_WITH_ARIA_256_CBC_SHA384",
             TLSCipherSuite::ECDH_RSA_WITH_ARIA_256_CBC_SHA384)
      .value("RSA_WITH_ARIA_128_GCM_SHA256",
             TLSCipherSuite::RSA_WITH_ARIA_128_GCM_SHA256)
      .value("RSA_WITH_ARIA_256_GCM_SHA384",
             TLSCipherSuite::RSA_WITH_ARIA_256_GCM_SHA384)
      .value("DHE_RSA_WITH_ARIA_128_GCM_SHA256",
             TLSCipherSuite::DHE_RSA_WITH_ARIA_128_GCM_SHA256)
      .value("DHE_RSA_WITH_ARIA_256_GCM_SHA384",
             TLSCipherSuite::DHE_RSA_WITH_ARIA_256_GCM_SHA384)
      .value("ECDHE_ECDSA_WITH_ARIA_128_GCM_SHA256",
             TLSCipherSuite::ECDHE_ECDSA_WITH_ARIA_128_GCM_SHA256)
      .value("ECDHE_ECDSA_WITH_ARIA_256_GCM_SHA384",
             TLSCipherSuite::ECDHE_ECDSA_WITH_ARIA_256_GCM_SHA384)
      .value("ECDH_ECDSA_WITH_ARIA_128_GCM_SHA256",
             TLSCipherSuite::ECDH_ECDSA_WITH_ARIA_128_GCM_SHA256)
      .value("ECDH_ECDSA_WITH_ARIA_256_GCM_SHA384",
             TLSCipherSuite::ECDH_ECDSA_WITH_ARIA_256_GCM_SHA384)
      .value("ECDHE_RSA_WITH_ARIA_128_GCM_SHA256",
             TLSCipherSuite::ECDHE_RSA_WITH_ARIA_128_GCM_SHA256)
      .value("ECDHE_RSA_WITH_ARIA_256_GCM_SHA384",
             TLSCipherSuite::ECDHE_RSA_WITH_ARIA_256_GCM_SHA384)
      .value("ECDH_RSA_WITH_ARIA_128_GCM_SHA256",
             TLSCipherSuite::ECDH_RSA_WITH_ARIA_128_GCM_SHA256)
      .value("ECDH_RSA_WITH_ARIA_256_GCM_SHA384",
             TLSCipherSuite::ECDH_RSA_WITH_ARIA_256_GCM_SHA384)
      .value("PSK_WITH_ARIA_128_CBC_SHA256",
             TLSCipherSuite::PSK_WITH_ARIA_128_CBC_SHA256)
      .value("PSK_WITH_ARIA_256_CBC_SHA384",
             TLSCipherSuite::PSK_WITH_ARIA_256_CBC_SHA384)
      .value("DHE_PSK_WITH_ARIA_128_CBC_SHA256",
             TLSCipherSuite::DHE_PSK_WITH_ARIA_128_CBC_SHA256)
      .value("DHE_PSK_WITH_ARIA_256_CBC_SHA384",
             TLSCipherSuite::DHE_PSK_WITH_ARIA_256_CBC_SHA384)
      .value("RSA_PSK_WITH_ARIA_128_CBC_SHA256",
             TLSCipherSuite::RSA_PSK_WITH_ARIA_128_CBC_SHA256)
      .value("RSA_PSK_WITH_ARIA_256_CBC_SHA384",
             TLSCipherSuite::RSA_PSK_WITH_ARIA_256_CBC_SHA384)
      .value("PSK_WITH_ARIA_128_GCM_SHA256",
             TLSCipherSuite::PSK_WITH_ARIA_128_GCM_SHA256)
      .value("PSK_WITH_ARIA_256_GCM_SHA384",
             TLSCipherSuite::PSK_WITH_ARIA_256_GCM_SHA384)
      .value("DHE_PSK_WITH_ARIA_128_GCM_SHA256",
             TLSCipherSuite::DHE_PSK_WITH_ARIA_128_GCM_SHA256)
      .value("DHE_PSK_WITH_ARIA_256_GCM_SHA384",
             TLSCipherSuite::DHE_PSK_WITH_ARIA_256_GCM_SHA384)
      .value("RSA_PSK_WITH_ARIA_128_GCM_SHA256",
             TLSCipherSuite::RSA_PSK_WITH_ARIA_128_GCM_SHA256)
      .value("RSA_PSK_WITH_ARIA_256_GCM_SHA384",
             TLSCipherSuite::RSA_PSK_WITH_ARIA_256_GCM_SHA384)
      .value("ECDHE_PSK_WITH_ARIA_128_CBC_SHA256",
             TLSCipherSuite::ECDHE_PSK_WITH_ARIA_128_CBC_SHA256)
      .value("ECDHE_PSK_WITH_ARIA_256_CBC_SHA384",
             TLSCipherSuite::ECDHE_PSK_WITH_ARIA_256_CBC_SHA384)
      .value("ECDHE_ECDSA_WITH_CAMELLIA_128_CBC_SHA256",
             TLSCipherSuite::ECDHE_ECDSA_WITH_CAMELLIA_128_CBC_SHA256)
      .value("ECDHE_ECDSA_WITH_CAMELLIA_256_CBC_SHA384",
             TLSCipherSuite::ECDHE_ECDSA_WITH_CAMELLIA_256_CBC_SHA384)
      .value("ECDH_ECDSA_WITH_CAMELLIA_128_CBC_SHA256",
             TLSCipherSuite::ECDH_ECDSA_WITH_CAMELLIA_128_CBC_SHA256)
      .value("ECDH_ECDSA_WITH_CAMELLIA_256_CBC_SHA384",
             TLSCipherSuite::ECDH_ECDSA_WITH_CAMELLIA_256_CBC_SHA384)
      .value("ECDHE_RSA_WITH_CAMELLIA_128_CBC_SHA256",
             TLSCipherSuite::ECDHE_RSA_WITH_CAMELLIA_128_CBC_SHA256)
      .value("ECDHE_RSA_WITH_CAMELLIA_256_CBC_SHA384",
             TLSCipherSuite::ECDHE_RSA_WITH_CAMELLIA_256_CBC_SHA384)
      .value("ECDH_RSA_WITH_CAMELLIA_128_CBC_SHA256",
             TLSCipherSuite::ECDH_RSA_WITH_CAMELLIA_128_CBC_SHA256)
      .value("ECDH_RSA_WITH_CAMELLIA_256_CBC_SHA384",
             TLSCipherSuite::ECDH_RSA_WITH_CAMELLIA_256_CBC_SHA384)
      .value("RSA_WITH_CAMELLIA_128_GCM_SHA256",
             TLSCipherSuite::RSA_WITH_CAMELLIA_128_GCM_SHA256)
      .value("RSA_WITH_CAMELLIA_256_GCM_SHA384",
             TLSCipherSuite::RSA_WITH_CAMELLIA_256_GCM_SHA384)
      .value("DHE_RSA_WITH_CAMELLIA_128_GCM_SHA256",
             TLSCipherSuite::DHE_RSA_WITH_CAMELLIA_128_GCM_SHA256)
      .value("DHE_RSA_WITH_CAMELLIA_256_GCM_SHA384",
             TLSCipherSuite::DHE_RSA_WITH_CAMELLIA_256_GCM_SHA384)
      .value("ECDHE_ECDSA_WITH_CAMELLIA_128_GCM_SHA256",
             TLSCipherSuite::ECDHE_ECDSA_WITH_CAMELLIA_128_GCM_SHA256)
      .value("ECDHE_ECDSA_WITH_CAMELLIA_256_GCM_SHA384",
             TLSCipherSuite::ECDHE_ECDSA_WITH_CAMELLIA_256_GCM_SHA384)
      .value("ECDH_ECDSA_WITH_CAMELLIA_128_GCM_SHA256",
             TLSCipherSuite::ECDH_ECDSA_WITH_CAMELLIA_128_GCM_SHA256)
      .value("ECDH_ECDSA_WITH_CAMELLIA_256_GCM_SHA384",
             TLSCipherSuite::ECDH_ECDSA_WITH_CAMELLIA_256_GCM_SHA384)
      .value("ECDHE_RSA_WITH_CAMELLIA_128_GCM_SHA256",
             TLSCipherSuite::ECDHE_RSA_WITH_CAMELLIA_128_GCM_SHA256)
      .value("ECDHE_RSA_WITH_CAMELLIA_256_GCM_SHA384",
             TLSCipherSuite::ECDHE_RSA_WITH_CAMELLIA_256_GCM_SHA384)
      .value("ECDH_RSA_WITH_CAMELLIA_128_GCM_SHA256",
             TLSCipherSuite::ECDH_RSA_WITH_CAMELLIA_128_GCM_SHA256)
      .value("ECDH_RSA_WITH_CAMELLIA_256_GCM_SHA384",
             TLSCipherSuite::ECDH_RSA_WITH_CAMELLIA_256_GCM_SHA384)
      .value("PSK_WITH_CAMELLIA_128_GCM_SHA256",
             TLSCipherSuite::PSK_WITH_CAMELLIA_128_GCM_SHA256)
      .value("PSK_WITH_CAMELLIA_256_GCM_SHA384",
             TLSCipherSuite::PSK_WITH_CAMELLIA_256_GCM_SHA384)
      .value("DHE_PSK_WITH_CAMELLIA_128_GCM_SHA256",
             TLSCipherSuite::DHE_PSK_WITH_CAMELLIA_128_GCM_SHA256)
      .value("DHE_PSK_WITH_CAMELLIA_256_GCM_SHA384",
             TLSCipherSuite::DHE_PSK_WITH_CAMELLIA_256_GCM_SHA384)
      .value("RSA_PSK_WITH_CAMELLIA_128_GCM_SHA256",
             TLSCipherSuite::RSA_PSK_WITH_CAMELLIA_128_GCM_SHA256)
      .value("RSA_PSK_WITH_CAMELLIA_256_GCM_SHA384",
             TLSCipherSuite::RSA_PSK_WITH_CAMELLIA_256_GCM_SHA384)
      .value("PSK_WITH_CAMELLIA_128_CBC_SHA256",
             TLSCipherSuite::PSK_WITH_CAMELLIA_128_CBC_SHA256)
      .value("PSK_WITH_CAMELLIA_256_CBC_SHA384",
             TLSCipherSuite::PSK_WITH_CAMELLIA_256_CBC_SHA384)
      .value("DHE_PSK_WITH_CAMELLIA_128_CBC_SHA256",
             TLSCipherSuite::DHE_PSK_WITH_CAMELLIA_128_CBC_SHA256)
      .value("DHE_PSK_WITH_CAMELLIA_256_CBC_SHA384",
             TLSCipherSuite::DHE_PSK_WITH_CAMELLIA_256_CBC_SHA384)
      .value("RSA_PSK_WITH_CAMELLIA_128_CBC_SHA256",
             TLSCipherSuite::RSA_PSK_WITH_CAMELLIA_128_CBC_SHA256)
      .value("RSA_PSK_WITH_CAMELLIA_256_CBC_SHA384",
             TLSCipherSuite::RSA_PSK_WITH_CAMELLIA_256_CBC_SHA384)
      .value("ECDHE_PSK_WITH_CAMELLIA_128_CBC_SHA256",
             TLSCipherSuite::ECDHE_PSK_WITH_CAMELLIA_128_CBC_SHA256)
      .value("ECDHE_PSK_WITH_CAMELLIA_256_CBC_SHA384",
             TLSCipherSuite::ECDHE_PSK_WITH_CAMELLIA_256_CBC_SHA384)
      .value("RSA_WITH_AES_128_CCM", TLSCipherSuite::RSA_WITH_AES_128_CCM)
      .value("RSA_WITH_AES_256_CCM", TLSCipherSuite::RSA_WITH_AES_256_CCM)
      .value("DHE_RSA_WITH_AES_128_CCM",
             TLSCipherSuite::DHE_RSA_WITH_AES_128_CCM)
      .value("DHE_RSA_WITH_AES_256_CCM",
             TLSCipherSuite::DHE_RSA_WITH_AES_256_CCM)
      .value("RSA_WITH_AES_128_CCM_8", TLSCipherSuite::RSA_WITH_AES_128_CCM_8)
      .value("RSA_WITH_AES_256_CCM_8", TLSCipherSuite::RSA_WITH_AES_256_CCM_8)
      .value("DHE_RSA_WITH_AES_128_CCM_8",
             TLSCipherSuite::DHE_RSA_WITH_AES_128_CCM_8)
      .value("DHE_RSA_WITH_AES_256_CCM_8",
             TLSCipherSuite::DHE_RSA_WITH_AES_256_CCM_8)
      .value("PSK_WITH_AES_128_CCM", TLSCipherSuite::PSK_WITH_AES_128_CCM)
      .value("PSK_WITH_AES_256_CCM", TLSCipherSuite::PSK_WITH_AES_256_CCM)
      .value("DHE_PSK_WITH_AES_128_CCM",
             TLSCipherSuite::DHE_PSK_WITH_AES_128_CCM)
      .value("DHE_PSK_WITH_AES_256_CCM",
             TLSCipherSuite::DHE_PSK_WITH_AES_256_CCM)
      .value("PSK_WITH_AES_128_CCM_8", TLSCipherSuite::PSK_WITH_AES_128_CCM_8)
      .value("PSK_WITH_AES_256_CCM_8", TLSCipherSuite::PSK_WITH_AES_256_CCM_8)
      .value("DHE_PSK_WITH_AES_128_CCM_8",
             TLSCipherSuite::DHE_PSK_WITH_AES_128_CCM_8)
      .value("DHE_PSK_WITH_AES_256_CCM_8",
             TLSCipherSuite::DHE_PSK_WITH_AES_256_CCM_8)
      .value("ECDHE_ECDSA_WITH_AES_128_CCM",
             TLSCipherSuite::ECDHE_ECDSA_WITH_AES_128_CCM)
      .value("ECDHE_ECDSA_WITH_AES_256_CCM",
             TLSCipherSuite::ECDHE_ECDSA_WITH_AES_256_CCM)
      .value("ECDHE_ECDSA_WITH_AES_128_CCM_8",
             TLSCipherSuite::ECDHE_ECDSA_WITH_AES_128_CCM_8)
      .value("ECDHE_ECDSA_WITH_AES_256_CCM_8",
             TLSCipherSuite::ECDHE_ECDSA_WITH_AES_256_CCM_8)
      .value("ECJPAKE_WITH_AES_128_CCM_8",
             TLSCipherSuite::ECJPAKE_WITH_AES_128_CCM_8)
      .value("ECDHE_RSA_WITH_CHACHA20_POLY1305_SHA256",
             TLSCipherSuite::ECDHE_RSA_WITH_CHACHA20_POLY1305_SHA256)
      .value("ECDHE_ECDSA_WITH_CHACHA20_POLY1305_SHA256",
             TLSCipherSuite::ECDHE_ECDSA_WITH_CHACHA20_POLY1305_SHA256)
      .value("DHE_RSA_WITH_CHACHA20_POLY1305_SHA256",
             TLSCipherSuite::DHE_RSA_WITH_CHACHA20_POLY1305_SHA256)
      .value("PSK_WITH_CHACHA20_POLY1305_SHA256",
             TLSCipherSuite::PSK_WITH_CHACHA20_POLY1305_SHA256)
      .value("ECDHE_PSK_WITH_CHACHA20_POLY1305_SHA256",
             TLSCipherSuite::ECDHE_PSK_WITH_CHACHA20_POLY1305_SHA256)
      .value("DHE_PSK_WITH_CHACHA20_POLY1305_SHA256",
             TLSCipherSuite::DHE_PSK_WITH_CHACHA20_POLY1305_SHA256)
      .value("RSA_PSK_WITH_CHACHA20_POLY1305_SHA256",
             TLSCipherSuite::RSA_PSK_WITH_CHACHA20_POLY1305_SHA256)
      .value("TLS1_3_AES_128_GCM_SHA256",
             TLSCipherSuite::TLS1_3_AES_128_GCM_SHA256)
      .value("TLS1_3_AES_256_GCM_SHA384",
             TLSCipherSuite::TLS1_3_AES_256_GCM_SHA384)
      .value("TLS1_3_CHACHA20_POLY1305_SHA256",
             TLSCipherSuite::TLS1_3_CHACHA20_POLY1305_SHA256)
      .value("TLS1_3_AES_128_CCM_SHA256",
             TLSCipherSuite::TLS1_3_AES_128_CCM_SHA256)
      .value("TLS1_3_AES_128_CCM_8_SHA256",
             TLSCipherSuite::TLS1_3_AES_128_CCM_8_SHA256);

  auto py_debug =
      py::enum_<Debug>(m, "Debug",
                       "This enum contains all valid debug bits to interpret "
                       "and manipulate debug mode.")
          .value("Server", Debug::Server)
          .value("Client", Debug::Client)
          .value("Connection", Debug::Connection)
          .value("Station", Debug::Station)
          .value("Point", Debug::Point)
          .value("Message", Debug::Message)
          .value("Callback", Debug::Callback)
          .value("Gil", Debug::Gil)
          .value("All", Debug::All)
          .def(py::init([]() { return Debug::None; }));
  bind_BitFlags_ops(py_debug, &Debug_toString);

  auto py_quality =
      py::enum_<Quality>(m, "Quality",
                         "This enum contains all quality issue bits to "
                         "interpret and manipulate measurement quality.")
          .value("Overflow", Quality::Overflow)
          //          .value("Reserved", Quality::Reserved)
          .value("ElapsedTimeInvalid", Quality::ElapsedTimeInvalid)
          .value("Blocked", Quality::Blocked)
          .value("Substituted", Quality::Substituted)
          .value("NonTopical", Quality::NonTopical)
          .value("Invalid", Quality::Invalid)
          .def(py::init([]() { return Quality::None; }));
  bind_BitFlags_ops(py_quality, &Quality_toString, true);

  auto py_bcrquality =
      py::enum_<BinaryCounterQuality>(
          m, "BinaryCounterQuality",
          "This enum contains all binary counter quality issue bits to "
          "interpret and manipulate counter quality.")
          .value("Adjusted", BinaryCounterQuality::Adjusted)
          .value("Carry", BinaryCounterQuality::Carry)
          .value("Invalid", BinaryCounterQuality::Invalid)
          .def(py::init([]() { return BinaryCounterQuality::None; }));
  bind_BitFlags_ops(py_bcrquality, &BinaryCounterQuality_toString, true);

  auto py_start_events =
      py::enum_<StartEvents>(
          m, "StartEvents",
          "This enum contains all StartEvents issue bits to "
          "interpret and manipulate protection equipment messages.")
          .value("General", StartEvents::General)
          .value("PhaseL1", StartEvents::PhaseL1)
          .value("PhaseL2", StartEvents::PhaseL2)
          .value("PhaseL3", StartEvents::PhaseL3)
          .value("InEarthCurrent", StartEvents::InEarthCurrent)
          .value("ReverseDirection", StartEvents::ReverseDirection)
          .def(py::init([]() { return StartEvents::None; }));
  bind_BitFlags_ops(py_start_events, &StartEvents_toString);

  auto py_output_circuit_info =
      py::enum_<OutputCircuits>(
          m, "OutputCircuits",
          "This enum contains all Output Circuit bits to "
          "interpret and manipulate protection equipment messages.")
          .value("General", OutputCircuits::General)
          .value("PhaseL1", OutputCircuits::PhaseL1)
          .value("PhaseL2", OutputCircuits::PhaseL2)
          .value("PhaseL3", OutputCircuits::PhaseL3)
          .def(py::init([]() { return OutputCircuits::None; }));
  bind_BitFlags_ops(py_output_circuit_info, &OutputCircuits_toString);

  auto py_field_set =
      py::enum_<FieldSet16>(
          m, "PackedSingle",
          "This enum contains all State bits to "
          "interpret and manipulate status with change detection messages.")
          .value("I0", FieldSet16::I0)
          .value("I1", FieldSet16::I1)
          .value("I2", FieldSet16::I2)
          .value("I3", FieldSet16::I3)
          .value("I4", FieldSet16::I4)
          .value("I5", FieldSet16::I5)
          .value("I6", FieldSet16::I6)
          .value("I7", FieldSet16::I7)
          .value("I8", FieldSet16::I8)
          .value("I9", FieldSet16::I9)
          .value("I10", FieldSet16::I10)
          .value("I11", FieldSet16::I11)
          .value("I12", FieldSet16::I12)
          .value("I13", FieldSet16::I13)
          .value("I14", FieldSet16::I14)
          .value("I15", FieldSet16::I15)
          .def(py::init([]() { return FieldSet16::None; }));
  bind_BitFlags_ops(py_field_set, &FieldSet16_toString);

  bind_Number<LimitedUInt5>(m, "UInt5");
  bind_Number<LimitedUInt7>(m, "UInt7");
  bind_Number<LimitedUInt16>(m, "UInt16");
  bind_Number<LimitedInt7>(m, "Int7");
  bind_Number<LimitedInt16>(m, "Int16");
  bind_Number<NormalizedFloat>(m, "NormalizedFloat", true);

  py::class_<Byte32>(m, "Byte32",
                     "This object is compatible to native bytes and ensures "
                     "that the length of the bytes is exactly 32 bit.")
      .def(py::init<const std::variant<py::bytes, int32_t>>(),
           R"def(__init__(self, value: bytes|int) -> None

create a new fixed-length bytes representation

Parameters
----------
value: bytes|int
    native byte data

Raises
------
OverflowError
    cannot convert value into 4 bytes representation

Example
-------
>>> fixed_byte32 = c104.Byte32(0b10101010111)
)def")
      .def(
          "__bytes__",
          [](const Byte32 &b) {
            uint32_t value = b.get();
            return py::bytes(reinterpret_cast<const char *>(&value),
                             sizeof(value));
          },
          R"def(__bytes__(self: c104.Byte32) -> bytes

convert to native bytes
)def")
      .def("__str__", &Byte32_toString)
      .def("__repr__", [](const Byte32 &b) {
        return "<c104.Byte32 value=" + Byte32_toString(b) + ">";
      });

  py::class_<sCS104_APCIParameters>(m, "ProtocolParameters",
                                    "This class is used to configure protocol "
                                    "parameters for server and client")
      .def_readwrite("send_window_size", &sCS104_APCIParameters::k,
                     "int: threshold of unconfirmed outgoing messages, before "
                     "waiting for acknowledgments (property name: k)")
      .def_readwrite("receive_window_size", &sCS104_APCIParameters::w,
                     "int: threshold of unconfirmed incoming messages to send "
                     "acknowledgments (property name: w)")
      .def_readwrite(
          "connection_timeout", &sCS104_APCIParameters::t0,
          "int: socket connection timeout (seconds) (property name: t0)")
      .def_readwrite("message_timeout", &sCS104_APCIParameters::t1,
                     "int: timeout for sent messages to be acknowledged by "
                     "counterparty (seconds) (property name: t1)")
      .def_readwrite("confirm_interval", &sCS104_APCIParameters::t2,
                     "int: maximum interval to acknowledge received messages "
                     "(seconds) (property name: t2)")
      .def_readwrite(
          "keep_alive_interval", &sCS104_APCIParameters::t3,
          "int: maximum interval without communication, send test "
          "frame message to prove liveness (seconds) (property name: t3)")
      .def("__str__",
           [](const sCS104_APCIParameters parameters) {
             std::ostringstream oss;
             oss << "<104.ProtocolParameters send_window_size=" << parameters.k
                 << ", receive_window_size=" << parameters.w
                 << ", connection_timeout=" << parameters.t0
                 << ", message_timeout=" << parameters.t1
                 << ", confirm_interval=" << parameters.t2
                 << ", keep_alive_interval=" << parameters.t3 << ">";
             return oss.str();
           })
      .def("__repr__", [](const sCS104_APCIParameters parameters) {
        std::ostringstream oss;
        oss << "<104.ProtocolParameters send_window_size=" << parameters.k
            << ", receive_window_size=" << parameters.w
            << ", connection_timeout=" << parameters.t0
            << ", message_timeout=" << parameters.t1
            << ", confirm_interval=" << parameters.t2
            << ", keep_alive_interval=" << parameters.t3 << " at " << std::hex
            << std::showbase << reinterpret_cast<std::uintptr_t>(&parameters)
            << ">";
        return oss.str();
      });

  init_remote_security(m);

  m.def("explain_bytes", &explain_bytes, R"def(explain_bytes(apdu: bytes) -> str

analyse 104er APDU bytes and convert it into a human readable interpretation

Parameters
----------
apdu: bytes
    APDU protocol bytes

Returns
-------
str
    information about provided APDU in str representation

Example
-------
>>> def sv_on_receive_raw(server: c104.Server, data: bytes) -> None:
>>>    print("SV] -->| {1} [{0}] | SERVER {2}:{3}".format(data.hex(), c104.explain_bytes(apdu=data), server.ip, server.port))
)def",
        "apdu"_a, py::return_value_policy::copy);

  m.def("explain_bytes_dict", &explain_bytes_dict,
        R"def(explain_bytes_dict(apdu: bytes) -> dict[str, typing.Any]

analyse 104er APDU bytes and extract information into a dictionary

Parameters
----------
apdu: bytes
    APDU protocol bytes

Returns
-------
dict
    information about APDU in dictionary :code:`{"format":str, "type":str, "cot":str, "rx": Optional[int],
    "tx": Optional[int], "numberOfObjects": Optional[int], "sequence": Optional[bool], "negative": Optional[bool],
    "test": Optional[bool], "commonAddress": Optional[int], "originatorAddress": Optional[int],
    "firstInformationObjectAddress": Optional[int], "elements": Optional[str]}`

Example
-------
>>> def sv_on_receive_raw(server: c104.Server, data: bytes) -> None:
>>>    print("SV] -->| {1} [{0}] | SERVER {2}:{3}".format(data.hex(), c104.explain_bytes_dict(apdu=data), server.ip, server.port))
)def",
        "apdu"_a, py::return_value_policy::copy);

  m.def("set_debug_mode", &setDebug,
        R"def(set_debug_mode(mode: c104.Debug) -> None

set the debug mode

Parameters
----------
mode: c104.Debug
    debug mode bitset

Example
-------
>>> c104.set_debug_mode(mode=c104.Debug.Client|c104.Debug.Connection)
)def",
        "mode"_a);
  m.def("get_debug_mode", &getDebug, R"def(get_debug_mode() -> c104.Debug

get current debug mode

Returns
----------
c104.Debug
    debug mode bitset

Example
-------
>>> mode = c104.get_debug_mode()
)def",
        py::return_value_policy::copy);
  m.def("enable_debug", &enableDebug,
        R"def(enable_debug(mode: c104.Debug) -> None

enable additional debugging modes

Parameters
----------
mode: c104.Debug
    debug mode bitset

Example
-------
>>> c104.set_debug_mode(mode=c104.Debug.Client|c104.Debug.Connection)
>>> c104.enable_debug(mode=c104.Debug.Callback|c104.Debug.Gil)
>>> c104.get_debug_mode() == c104.Debug.Client|c104.Debug.Connection|c104.Debug.Callback|c104.Debug.Gil
)def",
        "mode"_a);
  m.def("disable_debug", &disableDebug,
        R"def(disable_debug(mode: c104.Debug) -> None

disable debugging modes

Parameters
----------
mode: c104.Debug
    debug mode bitset

Example
-------
>>> c104.set_debug_mode(mode=c104.Debug.Client|c104.Debug.Connection|c104.Debug.Callback|c104.Debug.Gil)
>>> c104.disable_debug(mode=c104.Debug.Callback|c104.Debug.Gil)
>>> c104.get_debug_mode() == c104.Debug.Client|c104.Debug.Connection
)def",
        "mode"_a);

  init_object_datetime(m);
  init_object_information(m);

  init_client(m);
  init_server(m);
  init_remote_connection(m);
  init_object_datapoint(m);
  init_object_station(m);
  init_remote_batch(m);
  init_remote_message(m);

  m.attr("__version__") = VERSION_INFO;
}
