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

#include "remote/Helper.h"
#include "types.h"

#include "Client.h"
#include "Server.h"
#include "remote/message/Batch.h"

#include <pybind11/chrono.h>
#include <pybind11/operators.h>
#include <pybind11/stl.h>

#ifdef VERSION_INFO
#define PY_MODULE(var) PYBIND11_MODULE(_c104, var, py::mod_gil_not_used())
#else
#define VERSION_INFO "embedded"
#include <pybind11/embed.h>
#define PY_MODULE(var) PYBIND11_EMBEDDED_MODULE(c104, var)
#endif

using namespace pybind11::literals;

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
 * @brief Retrieves the raw byte data from an IncomingMessage as a Python bytes
 * object.
 *
 * This function accesses the raw byte data from the given IncomingMessage
 * object, calculates the size of the message including the header, and returns
 * the data encapsulated in a Python-compatible bytes object.
 *
 * @param message Pointer to an IncomingMessage object that contains the raw
 * byte data to retrieve.
 * @return py::bytes A Python bytes object containing the raw byte data from the
 * message.
 */
py::bytes
IncomingMessage_getRawBytes(const Remote::Message::IncomingMessage *message) {
  unsigned char *msg = message->getRawBytes();
  unsigned char msgSize = 2 + msg[1];

  return {reinterpret_cast<const char *>(msg), msgSize};
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
      .value("MISMATCHED_TYPE_ID", MISMATCHED_TYPE_ID)
      .value("UNIMPLEMENTED_GROUP", UNIMPLEMENTED_GROUP);

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
      .def(py::init([](const int &number) {
        uint32_t value = 0;
        if (number < 0) {
          throw std::overflow_error("can't convert negative int to bytes. The "
                                    "accepted range is from 0 to 4294967295.");
        }
        if (number > 4294967295) {
          throw std::overflow_error("can't convert long int to bytes. The "
                                    "accepted range is from 0 to 4294967295.");
        }

        return Byte32(static_cast<uint32_t>(value));
      }))
      .def(py::init<uint32_t>(),
           R"def(__init__(self, value: typing.Union[bytes, int]) -> None

create a new fixed-length bytes representation

Parameters
----------
value: typing.Union[bytes, int]
    native byte data

Raises
------
OverflowError
    cannot convert value into 4 bytes representation

Example
-------
>>> fixed_byte32 = c104.Byte32(0b10101010111)
)def")
      .def(py::init([](const py::bytes &byte_obj) {
        py::buffer_info info(py::buffer(byte_obj).request());

        if (info.size > sizeof(uint32_t)) {
          throw std::overflow_error(
              "Invalid size of bytes object. Expected 4 bytes, got " +
              std::to_string(info.size) + ".");
        }
        uint32_t value = 0;

        // Copy only the available bytes
        std::memcpy(&value, info.ptr, info.size);

        return Byte32(value);
      }))
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

  py::class_<Remote::TransportSecurity,
             std::shared_ptr<Remote::TransportSecurity>>(
      m, "TransportSecurity",
      "This class is responsible for configuring transport layer security "
      "(TLS) for both servers and clients. Once an instance is assigned to a "
      "client or server, it becomes read-only and cannot be modified further.")
      .def(
          py::init(&Remote::TransportSecurity::create),
          R"def(__init__(self: c104.TransportSecurity, validate: bool = True, only_known: bool = True) -> None

Create a new transport layer configuration

Parameters
----------
validate: bool
    validate certificates of communication partners (chain and time)
only_known: bool
    accept communication only from partners with certificate added to the list of allowed remote certificates

Example
-------
>>> tls = c104.TransportSecurity(validate=True, only_known=False)
)def",
          "validate"_a = true, "only_known"_a = true)
      .def(
          "set_certificate", &Remote::TransportSecurity::setCertificate,
          R"def(set_certificate(self: c104.TransportSecurity, cert: str, key: str, passphrase: str = "") -> None

load x509 certificate from file with (optional encrypted) key from file used to encrypt the connection

Parameters
----------
cert: str
    path to certificate file
key: bool
    path to certificates private key file
passphrase: str
    password required to decrypt the certificates private key file

Returns
-------
None

Raises
------
ValueError
    config is readonly and cannot be modified further
ValueError
    failed to load the certificate file, the private key file or failed decrypting the private key

Example
-------
>>> tls = c104.TransportSecurity(validate=True, only_known=False)
>>> tls.set_certificate(cert="certs/server.crt", key="certs/server.key")
)def",
          "cert"_a, "key"_a, "passphrase"_a = "")
      .def(
          "set_ca_certificate", &Remote::TransportSecurity::setCACertificate,
          R"def(set_ca_certificate(self: c104.TransportSecurity, cert: str) -> None

load x509 certificate of trusted authority from file

Parameters
----------
cert: str
    path to certificate authorities certificate file

Returns
-------
None

Raises
------
ValueError
    config is readonly and cannot be modified further
ValueError
    failed to load the certificate file

Example
-------
>>> tls = c104.TransportSecurity(validate=True, only_known=False)
>>> tls.set_ca_certificate(cert="certs/ca.crt")
)def",
          "cert"_a)
      .def(
          "set_ciphers", &Remote::TransportSecurity::setCipherSuites,
          R"def(set_ciphers(self: c104.TransportSecurity, ciphers: list[c104.TlsCipher]) -> None

set the list of accepted TLS cipher suites

When configuring minimum and maximum TLS versions together with cipher suites, it's crucial to ensure that the selected cipher suites are **compatible** with the specified TLS versions.

Parameters
----------
ciphers: list[c104.TlsCipher]
    accepted TLS cipher suites

Returns
-------
None

Raises
------
ValueError
    config is readonly and cannot be modified further
ValueError
    list is empty or contains invalid cipher suites

Example
-------
>>> tls = c104.TransportSecurity(validate=True, only_known=False)
>>> tls.set_ciphers(ciphers=[
>>>   c104.TlsCipher.ECDHE_ECDSA_WITH_AES_128_GCM_SHA256,
>>>   c104.TlsCipher.ECDHE_RSA_WITH_AES_128_GCM_SHA256,
>>>   c104.TlsCipher.ECDHE_ECDSA_WITH_AES_256_GCM_SHA384,
>>>   c104.TlsCipher.ECDHE_RSA_WITH_AES_256_GCM_SHA384,
>>>   c104.TlsCipher.ECDHE_ECDSA_WITH_CHACHA20_POLY1305_SHA256,
>>>   c104.TlsCipher.ECDHE_RSA_WITH_CHACHA20_POLY1305_SHA256,
>>>   c104.TlsCipher.DHE_RSA_WITH_AES_128_GCM_SHA256,
>>>   c104.TlsCipher.DHE_RSA_WITH_AES_256_GCM_SHA384,
>>>   c104.TlsCipher.DHE_RSA_WITH_CHACHA20_POLY1305_SHA256,
>>>   c104.TlsCipher.TLS1_3_AES_128_GCM_SHA256,
>>>   c104.TlsCipher.TLS1_3_AES_256_GCM_SHA384,
>>>   c104.TlsCipher.TLS1_3_CHACHA20_POLY1305_SHA256
>>> ])
)def",
          "ciphers"_a)
      .def(
          "set_renegotiation_time",
          &Remote::TransportSecurity::setRenegotiationTime,
          R"def(set_renegotiation_time(self: c104.TransportSecurity, interval: datetime.timedelta | None = None) -> None

sets the renegotiation interval

This defines how often the TLS connection should renegotiate. If no interval is
specified (None), it disables automatic renegotiation.
Per default renegotiation is disabled.

Parameters
----------
interval: datetime.timedelta, optional
    The interval as a ``datetime.timedelta`` object. If ``None``, renegotiation is disabled.

Returns
-------
None

Raises
------
ValueError
    config is readonly and cannot be modified further
ValueError
    value too small or too large

Example
-------
>>> tls = c104.TransportSecurity(validate=True, only_known=False)
>>> tls.set_renegotiation_time(interval=datetime.timedelta(minutes=30))
)def",
          "interval"_a)
      .def(
          "set_resumption_interval",
          &Remote::TransportSecurity::setResumptionInterval,
          R"def(set_resumption_interval(self: c104.TransportSecurity, interval: datetime.timedelta | None = None) -> None

sets the session resumption interval for the TLS configuration.

This interval determines the frequency at which session resumption can occur,
allowing faster reconnections. If no interval is specified (None), session
resumption will be disabled.
Per default session resumption is set to 6 hours.

Parameters
----------
interval: datetime.timedelta, optional
    The interval as a ``datetime.timedelta`` object. If ``None``, session resumption is disabled.

Returns
-------
None

Raises
------
ValueError
    config is readonly and cannot be modified further
ValueError
    value too small or too large

Example
-------
>>> tls = c104.TransportSecurity(validate=True, only_known=False)
>>> tls.set_resumption_interval(interval=datetime.timedelta(hours=6))
)def",
          "interval"_a)
      .def(
          "add_allowed_remote_certificate",
          &Remote::TransportSecurity::addAllowedRemoteCertificate,
          R"def(add_allowed_remote_certificate(self: c104.TransportSecurity, cert: str) -> None

add a trusted communication partners x509 certificate from file

Parameters
----------
cert: str
    path to trusted communication partners certificate file

Returns
-------
None

Raises
------
ValueError
    config is readonly and cannot be modified further
ValueError
    failed to load the certificate file

Example
-------
>>> tls = c104.TransportSecurity(validate=True, only_known=False)
>>> tls.add_allowed_remote_certificate(cert="certs/client2.crt")
)def",
          "cert"_a)
      .def(
          "set_version", &Remote::TransportSecurity::setVersion,
          R"def(set_version(self: c104.TransportSecurity, min: c104.TlsVersion = c104.TlsVersion.NOT_SELECTED, max: c104.TlsVersion = c104.TlsVersion.NOT_SELECTED) -> None

sets the supported min and/or max TLS version

When configuring minimum and maximum TLS versions together with cipher suites, it's crucial to ensure that the selected cipher suites are **compatible** with the specified TLS versions.

Parameters
----------
min: c104.TlsVersion
    minimum required TLS version for communication
max: c104.TlsVersion
    maximum allowed TLS version for communication

Returns
-------
None

Raises
------
ValueError
    config is readonly and cannot be modified further


Example
-------
>>> tls = c104.TransportSecurity(validate=True, only_known=False)
>>> tls.set_version(min=c104.TLSVersion.TLS_1_2, max=c104.TLSVersion.TLS_1_2)
)def",
          "min"_a = TLS_VERSION_NOT_SELECTED,
          "max"_a = TLS_VERSION_NOT_SELECTED)
      .def("__repr__", &Remote::TransportSecurity::toString);

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

  py::class_<Client, std::shared_ptr<Client>>(
      m, "Client",
      "This class represents a local client and provides access to meta "
      "information and connected remote servers")
      .def(
          py::init(&Client::create),
          R"def(__init__(self: c104.Client, tick_rate_ms: int = 100, command_timeout_ms: int = 10000, transport_security: c104.TransportSecurity = None) -> None

create a new 104er client

Parameters
----------
tick_rate_ms : int
    client thread update interval
command_timeout_ms : int
    time to wait for a command response
transport_security : c104.TransportSecurity, optional
    TLS configuration object

Example
-------
>>> my_client = c104.Client(tick_rate_ms=100, command_timeout_ms=10000)
)def",
          "tick_rate_ms"_a = 100, "command_timeout_ms"_a = 10000,
          "transport_security"_a = nullptr)
      .def_property_readonly(
          "tick_rate_ms", &Client::getTickRate_ms,
          "int: the clients tick rate in milliseconds (read-only)")
      .def_property_readonly("is_running", &Client::isRunning,
                             "bool: test if client is running (read-only)")
      .def_property_readonly("has_connections", &Client::hasConnections,
                             "bool: test if client has at least one remote "
                             "server connection (read-only)")
      .def_property_readonly(
          "has_open_connections", &Client::hasOpenConnections,
          "bool: test if client has open connections to servers (read-only)")
      .def_property_readonly("open_connection_count",
                             &Client::getOpenConnectionCount,
                             "int: represents the number of open connections "
                             "to servers (read-only)")
      .def_property_readonly("has_active_connections",
                             &Client::hasActiveConnections,
                             "bool: test if client has active (open and not "
                             "muted) connections to servers (read-only)")
      .def_property_readonly("active_connection_count",
                             &Client::getActiveConnectionCount,
                             "int: get number of active (open and not muted) "
                             "connections to servers (read-only)")
      .def_property_readonly(
          "connections", &Client::getConnections,
          "list[c104.Connection]: list of all remote terminal unit "
          "(server) Connection objects (read-only)")
      .def_property("originator_address", &Client::getOriginatorAddress,
                    &Client::setOriginatorAddress,
                    "int: originator address of this client (0-255)",
                    py::return_value_policy::copy)
      .def("start", &Client::start, R"def(start(self: c104.Client) -> None

start client and connect all connections

Example
-------
>>> my_client.start()
)def")
      .def("stop", &Client::stop, R"def(stop(self: c104.Client) -> None

disconnect all connections and stop client

Example
-------
>>> my_client.stop()
)def")
      .def(
          "add_connection", &Client::addConnection,
          R"def(add_connection(self: c104.Client, ip: str, port: int = 2404, init = c104.Init.ALL) -> c104.Connection | None

add a new remote server connection to this client and return the new connection object

Parameters
----------
ip: str
    remote terminal units ip address
port: int
    remote terminal units port
init: c104.Init
    communication initiation commands

Returns
-------
c104.Connection, optional
    connection object, if added, else None

Raises
------
ValueError
    ip or port are invalid

Example
-------
>>> con = my_client.add_connection(ip="192.168.50.3", port=2406, init=c104.Init.ALL)
)def",
          "ip"_a, "port"_a = IEC_60870_5_104_DEFAULT_PORT, "init"_a = INIT_ALL)
      .def(
          "get_connection",
          [](Client &client, const std::string ip, const int port,
             const int common_address) {
            if (!ip.empty() && common_address == 0) {
              return client.getConnection(ip, port);
            }
            if (ip.empty() && common_address > 0) {
              return client.getConnectionFromCommonAddress(common_address);
            }
            throw std::invalid_argument("either keyword arguments ip and port "
                                        "or common_address must be specified");
          },
          R"def(get_connection(self: c104.Client, ip: str = "", port: int = 2404, common_address: int = 0) -> c104.Connection | None

get a connection (either by ip and port or by common_address)

Parameters
----------
ip: str, optional
    remote terminal units ip address
port: int, optional
    remote terminal units port
common_address: int, optional
    common address (value between 1 and 65534)

Returns
-------
c104.Connection, optional
    connection object, if found else None

Example
-------
>>> conA = my_client.get_connection(ip="192.168.50.3")
>>> conB = my_client.get_connection(ip="192.168.50.3", port=2406)
>>> conC = my_client.get_connection(common_address=4711)
)def",
          "ip"_a = "", "port"_a = IEC_60870_5_104_DEFAULT_PORT,
          "common_address"_a = 0)
      .def("reconnect_all", &Client::reconnectAll,
           R"def(reconnect_all(self: c104.Client) -> None

close and reopen all connections

Example
-------
>>> my_client.reconnect_all()
)def")
      .def("disconnect_all", &Client::disconnectAll,
           R"def(disconnect_all(self: c104.Client) -> None

close all connections

Example
-------
>>> my_client.disconnect_all()
)def")
      .def(
          "on_station_initialized", &Client::setOnEndOfInitializationCallback,
          R"def(on_station_initialized(self: c104.Client, callable: collections.abc.Callable[[c104.Client, c104.Station, c104.Coi], None]) -> None

set python callback that will be executed on incoming end of initialization message from stations

Parameters
----------
callable: collections.abc.Callable[[c104.Client, c104.Station, c104.Coi], None]
    callback function reference

Returns
-------
None

Raises
------
ValueError
    callable signature does not match exactly

**Callable signature**

Callable Parameters
--------------------
client: c104.Client
    client instance
station: c104.Station
    reporting station
cause: c104.Coi
    what caused the (re-)initialization procedure

Callable Returns
-----------------
None

Example
-------
>>> def cl_on_station_initialized(client: c104.Client, station: c104.Station, cause: c104.Coi) -> None:
>>>     print("STATION {0} INITIALIZED due to {1} | CLIENT OA {2}".format(station.common_address, cause, client.originator_address))
>>>
>>> my_client.on_station_initialized(callable=cl_on_station_initialized)
)def",
          "callable"_a)
      .def(
          "on_new_station", &Client::setOnNewStationCallback,
          R"def(on_new_station(self: c104.Client, callable: collections.abc.Callable[[c104.Client, c104.Connection, int], None]) -> None

set python callback that will be executed on incoming message from unknown station

Parameters
----------
callable: collections.abc.Callable[[c104.Client, c104.Connection, int], None]
    callback function reference

Returns
-------
None

Raises
------
ValueError
    callable signature does not match exactly

**Callable signature**

Callable Parameters
--------------------
client: c104.Client
    client instance
connection: c104.Connection
    connection reporting station
common_address: int
    station common address (value between 1 and 65534)

Callable Returns
-----------------
None

Example
-------
>>> def cl_on_new_station(client: c104.Client, connection: c104.Connection, common_address: int) -> None:
>>>     print("NEW STATION {0} | CLIENT OA {1}".format(common_address, client.originator_address))
>>>     connection.add_station(common_address=common_address)
>>>
>>> my_client.on_new_station(callable=cl_on_new_station)
)def",
          "callable"_a)
      .def(
          "on_new_point", &Client::setOnNewPointCallback,
          R"def(on_new_point(self: c104.Client, callable: collections.abc.Callable[[c104.Client, c104.Station, int, c104.Type], None]) -> None

set python callback that will be executed on incoming message from unknown point

Parameters
----------
callable: collections.abc.Callable[[c104.Client, c104.Station, int, c104.Type], None]
    callback function reference

Returns
-------
None

Raises
------
ValueError
    callable signature does not match exactly

**Callable signature**

Callable Parameters
-------------------
client: c104.Client
    client instance
station: c104.Station
    station reporting point
io_address: int
    point information object address (value between 0 and 16777215)
point_type: c104.Type
    point information type

Callable Returns
----------------
None

Example
-------
>>> def cl_on_new_point(client: c104.Client, station: c104.Station, io_address: int, point_type: c104.Type) -> None:
>>>     print("NEW POINT: {1} with IOA {0} | CLIENT OA {2}".format(io_address, point_type, client.originator_address))
>>>     point = station.add_point(io_address=io_address, type=point_type)
>>>
>>> my_client.on_new_point(callable=cl_on_new_point)
)def",
          "callable"_a)
      .def("__repr__", &Client::toString);

  py::class_<Server, std::shared_ptr<Server>>(
      m, "Server",
      "This class represents a local server and provides access to meta "
      "information and containing stations")
      .def(
          py::init(&Server::create),
          R"def(__init__(self: c104.Server, ip: str = "0.0.0.0", port: int = 2404, tick_rate_ms: int = 100, select_timeout_ms = 10000, max_connections: int = 0, transport_security: c104.TransportSecurity = None) -> None

create a new 104er server

Parameters
----------
ip: str
    listening server ip address
port:int
    listening server port
tick_rate_ms: int
    server thread update interval
select_timeout_ms: int
    execution for points in SELECT_AND_EXECUTE mode must arrive within this interval to succeed
max_connections: int
    maximum number of clients allowed to connect
transport_security: c104.TransportSecurity, optional
    TLS configuration object

Example
-------
>>> my_server = c104.Server(ip="0.0.0.0", port=2404, tick_rate_ms=100, select_timeout_ms=10000, max_connections=0)
)def",
          "ip"_a = "0.0.0.0", "port"_a = IEC_60870_5_104_DEFAULT_PORT,
          "tick_rate_ms"_a = 100, "select_timeout_ms"_a = 10000,
          "max_connections"_a = 0, "transport_security"_a = nullptr)
      .def_property_readonly(
          "tick_rate_ms", &Server::getTickRate_ms,
          "int: the servers tick rate in milliseconds (read-only)")
      .def_property_readonly("ip", &Server::getIP,
                             "str: ip address the server will accept "
                             "connections on, \"0.0.0.0\" = any (read-only)")
      .def_property_readonly(
          "port", &Server::getPort,
          "int: port number the server will accept connections on (read-only)")
      .def_property_readonly("is_running", &Server::isRunning,
                             "bool: test if server is running (read-only)")
      .def_property_readonly(
          "has_open_connections", &Server::hasOpenConnections,
          "bool: test if server has open connections to clients (read-only)")
      .def_property_readonly("open_connection_count",
                             &Server::getOpenConnectionCount,
                             "int: represents the number of open connections "
                             "to clients (read-only)")
      .def_property_readonly("has_active_connections",
                             &Server::hasActiveConnections,
                             "bool: test if server has active (open and not "
                             "muted) connections to clients (read-only)")
      .def_property_readonly("active_connection_count",
                             &Server::getActiveConnectionCount,
                             "int: get number of active (open and not muted) "
                             "connections to clients (read-only)")
      .def_property_readonly(
          "has_stations", &Server::hasStations,
          "bool: test if server has at least one station (read-only)")
      .def_property_readonly("stations", &Server::getStations,
                             "list[c104.Station]: list of all local "
                             "Station objects (read-only)")
      .def_property_readonly(
          "protocol_parameters", &Server::getParameters,
          "c104.ProtocolParameters: read and update protocol parameters",
          py::return_value_policy::reference)
      .def_property("max_connections", &Server::getMaxOpenConnections,
                    &Server::setMaxOpenConnections,
                    "int: maximum number of open connections, 0 = no limit",
                    py::return_value_policy::copy)
      .def("start", &Server::start, R"def(start(self: c104.Server) -> None

open local server socket for incoming connections

Raises
------
RuntimeError
    server thread failed to start

Example
-------
>>> my_server.start()
)def")
      .def("stop", &Server::stop, R"def(stop(self: c104.Server) -> None

stop local server socket

Example
-------
>>> my_server.stop()
)def")
      .def(
          "add_station", &Server::addStation,
          R"def(add_station(self: c104.Server, common_address: int) -> c104.Station | None

add a new station to this server and return the new station object

Parameters
----------
common_address: int
    station common address (value between 1 and 65534)

Returns
-------
c104.Station, optional
    station object, if station was added, else None

Example
-------
>>> station_1 = my_server.add_station(common_address=15)
)def",
          "common_address"_a)
      .def(
          "get_station", &Server::getStation,
          R"def(get_station(self: c104.Server, common_address: int) -> c104.Station | None

get a station object via common address

Parameters
----------
common_address: int
    station common address (value between 1 and 65534)

Returns
-------
c104.Station, optional
    station object, if found, else None

Example
-------
>>> station_2 = my_server.get_connection(common_address=14)
)def",
          "common_address"_a)
      .def("remove_station", &Server::removeStation,
           R"def(remove_station(self: c104.Server, common_address: int) -> bool

removes an existing station from this server

Parameters
----------
common_address: int
    station common address (value between 1 and 65534)

Returns
-------
bool
    True if the station was successfully removed, otherwise False.

Example
-------
>>> station_3.remove_station(common_address=12)
)def",
           "common_address"_a)
      .def(
          "transmit_batch",
          [](Server &server, std::shared_ptr<Remote::Message::Batch> batch) {
            return server.sendBatch(std::move(batch));
          },
          R"def(transmit_batch(self: c104.Server, batch: c104.Batch) -> bool

transmit a batch object

Parameters
----------
batch: c104.Batch
    batch object to transmit

Returns
-------
bool
    send success

Example
-------
>>> success = my_server.transmit_batch(c104.Batch([point1, point2, point3]))
)def",
          "batch"_a)
      .def(
          "on_receive_raw", &Server::setOnReceiveRawCallback,
          R"def(on_receive_raw(self: c104.Server, callable: collections.abc.Callable[[c104.Server, bytes], None]) -> None

set python callback that will be executed on incoming message

Parameters
----------
callable: collections.abc.Callable[[c104.Server, bytes], None]
    callback function reference

Returns
-------
None

Raises
------
ValueError
    callable signature does not match exactly

**Callable signature**

Callable Parameters
-------------------
server: c104.Server
    server instance
data: bytes
    raw message bytes

Callable Returns
----------------
None

Example
-------
>>> def sv_on_receive_raw(server: c104.Server, data: bytes) -> None:
>>>     print("-->| {1} [{0}] | SERVER {2}:{3}".format(data.hex(), c104.explain_bytes(apdu=data), server.ip, server.port))
>>>
>>> my_server.on_receive_raw(callable=sv_on_receive_raw)
)def",
          "callable"_a)
      .def(
          "on_send_raw", &Server::setOnSendRawCallback,
          R"def(on_send_raw(self: c104.Server, callable: collections.abc.Callable[[c104.Server, bytes], None]) -> None

set python callback that will be executed on outgoing message

Parameters
----------
callable: collections.abc.Callable[[c104.Server, bytes], None]
    callback function reference

Returns
-------
None

Raises
------
ValueError
    callable signature does not match exactly

**Callable signature**

Callable Parameters
-------------------
server: c104.Server
    server instance
data: bytes
    raw message bytes

Callable Returns
----------------
None

Example
-------
>>> def sv_on_send_raw(server: c104.Server, data: bytes) -> None:
>>>     print("<--| {1} [{0}] | SERVER {2}:{3}".format(data.hex(), c104.explain_bytes(apdu=data), server.ip, server.port))
>>>
>>> my_server.on_send_raw(callable=sv_on_send_raw)
)def",
          "callable"_a)
      .def(
          "on_connect", &Server::setOnConnectCallback,
          R"def(on_connect(self: c104.Server, callable: collections.abc.Callable[[c104.Server, ip], bool]) -> None

set python callback that will be executed on incoming connection requests

Parameters
----------
callable: collections.abc.Callable[[c104.Server, ip], bool]
    callback function reference

Returns
-------
None

Raises
------
ValueError
    callable signature does not match exactly

**Callable signature**

Callable Parameters
-------------------
server: c104.Server
    server instance
ip: str
    client connection request ip

Callable Returns
----------------
bool
    accept or reject the connection request

Example
-------
>>> def sv_on_connect(server: c104.Server, ip: str) -> bool:
>>>     print("<->| {0} | SERVER {1}:{2}".format(ip, server.ip, server.port))
>>>     return ip == "127.0.0.1"
>>>
>>> my_server.on_connect(callable=sv_on_connect)
)def",
          "callable"_a)
      .def(
          "on_clock_sync", &Server::setOnClockSyncCallback,
          R"def(on_clock_sync(self: c104.Server, callable: collections.abc.Callable[[c104.Server, str, datetime.datetime], c104.ResponseState]) -> None

set python callback that will be executed on incoming clock sync command

Parameters
----------
callable: collections.abc.Callable[[c104.Server, str, datetime.datetime], c104.ResponseState]
    callback function reference

Returns
-------
None

Raises
------
ValueError
    callable signature does not match exactly

**Callable signature**

Callable Parameters
-------------------
server: c104.Server
    server instance
ip: str
    client connection request ip
date_time: datetime.datetime
    clients current clock time

Callable Returns
----------------
c104.ResponseState
    success or failure of clock sync command

Example
-------
>>> import datetime
>>>
>>> def sv_on_clock_sync(server: c104.Server, ip: str, date_time: datetime.datetime) -> c104.ResponseState:
>>>     print("->@| Time {0} from {1} | SERVER {2}:{3}".format(date_time, ip, server.ip, server.port))
>>>     return c104.ResponseState.SUCCESS
>>>
>>> my_server.on_clock_sync(callable=sv_on_clock_sync)
)def",
          "callable"_a)
      .def(
          "on_unexpected_message", &Server::setOnUnexpectedMessageCallback,
          R"def(on_unexpected_message(self: c104.Server, callable: collections.abc.Callable[[c104.Server, c104.IncomingMessage, c104.Umc], None]) -> None

set python callback that will be executed on unexpected incoming messages

Parameters
----------
callable: collections.abc.Callable[[c104.Server, c104.IncomingMessage, c104.Umc], None]
    callback function reference

Returns
-------
None

Raises
------
ValueError
    callable signature does not match exactly

**Callable signature**

Callable Parameters
-------------------
server: c104.Server
    server instance
message: c104.IncomingMessage
    incoming message
cause: c104.Umc
    unexpected message cause

Callable Returns
----------------
None

Example
-------
>>> def sv_on_unexpected_message(server: c104.Server, message: c104.IncomingMessage, cause: c104.Umc) -> None:
>>>     print("->?| {1} from CLIENT OA {0} | SERVER {2}:{3}".format(message.originator_address, cause, server.ip, server.port))
>>>
>>> my_server.on_unexpected_message(callable=sv_on_unexpected_message)
)def",
          "callable"_a)
      .def("__repr__", &Server::toString);

  py::class_<Remote::Connection, std::shared_ptr<Remote::Connection>>(
      m, "Connection",
      "This class represents connections from a client to a remote server and "
      "provides access to meta information and containing stations")
      .def_property_readonly(
          "ip", &Remote::Connection::getIP,
          "str: remote terminal units (server) ip (read-only)")
      .def_property_readonly(
          "port", &Remote::Connection::getPort,
          "int: remote terminal units (server) port (read-only)")
      .def_property_readonly(
          "state", &Remote::Connection::getState,
          "c104.ConnectionState: current connection state (read-only)")
      .def_property_readonly(
          "has_stations", &Remote::Connection::hasStations,
          "bool: test if remote server has at least one station (read-only)")
      .def_property_readonly(
          "stations", &Remote::Connection::getStations,
          "list[c104.Station]: list of all Station objects (read-only)")
      .def_property_readonly("is_connected", &Remote::Connection::isOpen,
                             "bool: test if connection is opened (read-only)")
      .def_property_readonly("is_muted", &Remote::Connection::isMuted,
                             "bool: test if connection is muted (read-only)")
      .def_property("originator_address",
                    &Remote::Connection::getOriginatorAddress,
                    &Remote::Connection::setOriginatorAddress,
                    "int: originator address of this connection (0-255)")
      .def_property_readonly(
          "connected_at", &Remote::Connection::getConnectedAt,
          "datetime.datetime | None : datetime of last connection opening, if "
          "connection is open (read-only)")
      .def_property_readonly(
          "disconnected_at", &Remote::Connection::getDisconnectedAt,
          "datetime.datetime | None : datetime of last connection closing, if "
          "connection is closed (read-only)")
      .def_property_readonly(
          "protocol_parameters", &Remote::Connection::getParameters,
          "c104.ProtocolParameters: read and update protocol parameters",
          py::return_value_policy::reference)
      .def("connect", &Remote::Connection::connect,
           R"def(connect(self: c104.Connection) -> None

initiate connection to remote terminal unit (server) in a background thread (non-blocking)

Example
-------
>>> my_connection.connect()
)def")
      .def("disconnect", &Remote::Connection::disconnect,
           R"def(disconnect(self: c104.Connection) -> None

close connection to remote terminal unit (server)

Example
-------
>>> my_connection.disconnect()
)def")
      .def("mute", &Remote::Connection::mute,
           R"def(mute(self: c104.Connection) -> bool

tell the remote terminal unit (server) that this connection is muted, prohibit monitoring messages

Returns
-------
bool
    True, if connection is Open, False otherwise

Example
-------
>>> if not my_connection.mute():
>>>     raise ValueError("Cannot mute connection")
)def",
           py::return_value_policy::copy)
      .def("unmute", &Remote::Connection::unmute,
           R"def(unmute(self: c104.Connection) -> bool

tell the remote terminal unit (server) that this connection is not muted, allow monitoring messages

Returns
-------
bool
    True, if connection is Open, False otherwise

Example
-------
>>> if not my_connection.unmute():
>>>     raise ValueError("Cannot unmute connection")
)def",
           py::return_value_policy::copy)
      .def(
          "interrogation", &Remote::Connection::interrogation,
          R"def(interrogation(self: c104.Connection, common_address: int, cause: c104.Cot = c104.Cot.ACTIVATION, qualifier: c104.Qoi = c104.Qoi.STATION, wait_for_response: bool = True) -> bool

send an interrogation command to the remote terminal unit (server)

Parameters
----------
common_address: int
    station common address (The valid range is 0 to 65535. Using the values 0 or 65535 sends the command to all stations, acting as a wildcard.)
cause: c104.Cot
    cause of transmission
qualifier: c104.Qoi
    qualifier of interrogation
wait_for_response: bool
    block call until command success or failure response received?

Returns
-------
bool
    True, if connection is Open, False otherwise

Raises
------
ValueError
    qualifier is invalid

Example
-------
>>> if not my_connection.interrogation(common_address=47, cause=c104.Cot.ACTIVATION, qualifier=c104.Qoi.STATION):
>>>     raise ValueError("Cannot send interrogation command")
)def",
          "common_address"_a, "cause"_a = CS101_COT_ACTIVATION,
          "qualifier"_a = QOI_STATION, "wait_for_response"_a = true,
          py::return_value_policy::copy)
      .def(
          "counter_interrogation", &Remote::Connection::counterInterrogation,
          R"def(counter_interrogation(self: c104.Connection, common_address: int, cause: c104.Cot = c104.Cot.ACTIVATION, qualifier: c104.Rqt = c104.Rqt.GENERAL, freeze: c104.Frz = c104.Frz.READ, wait_for_response: bool = True) -> bool

send a counter interrogation command to the remote terminal unit (server)

Parameters
----------
common_address: int
    station common address (The valid range is 0 to 65535. Using the values 0 or 65535 sends the command to all stations, acting as a wildcard.)
cause: c104.Cot
    cause of transmission
qualifier: c104.Rqt
    what counters are addressed
freeze: c104.Frz
    counter behaviour
wait_for_response: bool
    block call until command success or failure response received?

Returns
-------
bool
    True, if connection is Open, False otherwise

Example
-------
>>> if not my_connection.counter_interrogation(common_address=47, cause=c104.Cot.ACTIVATION, qualifier=c104.Rqt.GENERAL, freeze=c104.Frz.COUNTER_RESET):
>>>     raise ValueError("Cannot send counter interrogation command")
)def",
          "common_address"_a, "cause"_a = CS101_COT_ACTIVATION,
          "qualifier"_a = CS101_QualifierOfCounterInterrogation::GENERAL,
          "freeze"_a = CS101_FreezeOfCounterInterrogation::READ,
          "wait_for_response"_a = true, py::return_value_policy::copy)
      .def(
          "clock_sync", &Remote::Connection::clockSync,
          R"def(clock_sync(self: c104.Connection, common_address: int, wait_for_response: bool = True) -> bool

send a clock synchronization command to the remote terminal unit (server)
the clients OS time is used

Parameters
----------
common_address: int
    station common address (The valid range is 0 to 65535. Using the values 0 or 65535 sends the command to all stations, acting as a wildcard.)
wait_for_response: bool
    block call until command success or failure response received?

Returns
-------
bool
    True, if connection is Open, False otherwise

Example
-------
>>> if not my_connection.clock_sync(common_address=47):
>>>     raise ValueError("Cannot send clock sync command")
)def",
          "common_address"_a, "wait_for_response"_a = true,
          py::return_value_policy::copy)
      .def(
          "test", &Remote::Connection::test,
          R"def(test(self: c104.Connection, common_address: int, with_time: bool = True, wait_for_response: bool = True) -> bool

send a test command to the remote terminal unit (server)
the clients OS time is used

Parameters
----------
common_address: int
    station common address (The valid range is 0 to 65535. Using the values 0 or 65535 sends the command to all stations, acting as a wildcard.)
with_time: bool
    send with or without timestamp
wait_for_response: bool
    block call until command success or failure response received?

Returns
-------
bool
    True, if connection is Open, False otherwise

Example
-------
>>> if not my_connection.test(common_address=47):
>>>     raise ValueError("Cannot send test command")
)def",
          "common_address"_a, "with_time"_a = true,
          "wait_for_response"_a = true, py::return_value_policy::copy)
      .def(
          "add_station", &Remote::Connection::addStation,
          R"def(add_station(self: c104.Connection, common_address: int) -> c104.Station | None

add a new station to this connection and return the new station object

Parameters
----------
common_address: int
    station common address (value between 1 and 65534)

Returns
-------
c104.Station, optional
    station object, if station was added, else None

Example
-------
>>> station_1 = my_connection.add_station(common_address=15)
)def",
          "common_address"_a)
      .def(
          "get_station", &Remote::Connection::getStation,
          R"def(get_station(self: c104.Connection, common_address: int) -> c104.Station | None

get a station object via common address

Parameters
----------
common_address: int
    station common address (value between 1 and 65534)

Returns
-------
c104.Station, optional
    station object, if found, else None

Example
-------
>>> station_14 = my_connection.get_station(common_address=14)
)def",
          "common_address"_a)
      .def(
          "remove_station", &Remote::Connection::removeStation,
          R"def(remove_station(self: c104.Connection, common_address: int) -> bool

removes an existing station from this connection

Parameters
----------
common_address: int
    station common address (value between 1 and 65534)

Returns
-------
bool
    True if the station was successfully removed, otherwise False.

Example
-------
>>> my_connection.remove_station(common_address=12)
)def",
          "common_address"_a)
      .def(
          "on_receive_raw", &Remote::Connection::setOnReceiveRawCallback,
          R"def(on_receive_raw(self: c104.Connection, callable: collections.abc.Callable[[c104.Connection, bytes], None]) -> None

set python callback that will be executed on incoming message

Parameters
----------
callable: collections.abc.Callable[[c104.Connection, bytes], None]
    callback function reference

Returns
-------
None

Raises
------
ValueError
    callable signature does not match exactly

**Callable signature**

Callable Parameters
-------------------
connection: c104.Connection
    connection instance
data: bytes
    raw message bytes

Callable Returns
----------------
None

Example
-------
>>> def con_on_receive_raw(connection: c104.Connection, data: bytes) -> None:
>>>     print("-->| {1} [{0}] | CON {2}:{3}".format(data.hex(), c104.explain_bytes(apdu=data), connection.ip, connection.port))
>>>
>>> my_connection.on_receive_raw(callable=con_on_receive_raw)
)def",
          "callable"_a)
      .def(
          "on_send_raw", &Remote::Connection::setOnSendRawCallback,
          R"def(on_send_raw(self: c104.Connection, callable: collections.abc.Callable[[c104.Connection, bytes], None]) -> None

set python callback that will be executed on outgoing message

Parameters
----------
callable: collections.abc.Callable[[c104.Connection, bytes], None]
    callback function reference

Returns
-------
None

Raises
------
ValueError
    callable signature does not match exactly

**Callable signature**

Callable Parameters
-------------------
connection: c104.Connection
    connection instance
data: bytes
    raw message bytes

Callable Returns
----------------
None

Example
-------
>>> def con_on_send_raw(connection: c104.Connection, data: bytes) -> None:
>>>     print("<--| {1} [{0}] | CON {2}:{3}".format(data.hex(), c104.explain_bytes(apdu=data), connection.ip, connection.port))
>>>
>>> my_connection.on_send_raw(callable=con_on_send_raw)
)def",
          "callable"_a)
      .def(
          "on_state_change", &Remote::Connection::setOnStateChangeCallback,
          R"def(on_state_change(self: c104.Connection, callable: collections.abc.Callable[[c104.Connection, c104.ConnectionState], None]) -> None

set python callback that will be executed on connection state changes

Parameters
----------
callable: collections.abc.Callable[[c104.Connection, c104.ConnectionState], None]
    callback function reference

Returns
-------
None

Raises
------
ValueError
    callable signature does not match exactly

**Callable signature**

Callable Parameters
-------------------
connection: c104.Connection
    connection instance
state: c104.ConnectionState
    latest connection state

Callable Returns
----------------
None

Example
-------
>>> def con_on_state_change(connection: c104.Connection, state: c104.ConnectionState) -> None:
>>>     print("CON {0}:{1} STATE changed to {2}".format(connection.ip, connection.port, state))
>>>
>>> my_connection.on_state_change(callable=con_on_state_change)
)def",
          "callable"_a)
      .def(
          "on_unexpected_message",
          &Remote::Connection::setOnUnexpectedMessageCallback,
          R"def(on_unexpected_message(self: c104.Connection, callable: collections.abc.Callable[[c104.Connection, c104.IncomingMessage, c104.Umc], None]) -> None

set python callback that will be executed on unexpected incoming messages

Parameters
----------
callable: collections.abc.Callable[[c104.Connection, c104.IncomingMessage, c104.Umc], None]
    callback function reference

Returns
-------
None

Raises
------
ValueError
    callable signature does not match exactly

**Callable signature**

Callable Parameters
-------------------
connection: c104.Connection
    connection instance
message: c104.IncomingMessage
    incoming message
cause: c104.Umc
    unexpected message cause

Callable Returns
----------------
None

Example
-------
>>> def con_on_unexpected_message(connection: c104.Connection, message: c104.IncomingMessage, cause: c104.Umc) -> None:
>>>     print("->?| {1} from STATION CA {0}".format(message.common_address, cause))
>>>
>>> my_connection.on_unexpected_message(callable=con_on_unexpected_message)
)def",
          "callable"_a)
      .def("__repr__", &Remote::Connection::toString);
  ;

  py::class_<Object::Station, std::shared_ptr<Object::Station>>(
      m, "Station",
      "This class represents local or remote stations and provides access to "
      "meta information and containing points")
      .def_property_readonly("server", &Object::Station::getServer,
                             "c104.Server | None : parent Server of "
                             "local station (read-only)")
      .def_property_readonly("connection", &Object::Station::getConnection,
                             "c104.Connection | None : parent "
                             "Connection of non-local station (read-only)")
      .def_property_readonly(
          "common_address", &Object::Station::getCommonAddress,
          "int: common address of this station (1-65534) (read-only)",
          py::return_value_policy::copy)
      .def_property_readonly("is_local", &Object::Station::isLocal,
                             "bool: test if station is local (has sever) or "
                             "remote (has connection) one (read-only)")
      .def_property_readonly(
          "has_points", &Object::Station::hasPoints,
          "bool: test if station has at least one point (read-only)")
      .def_property_readonly(
          "points", &Object::Station::getPoints,
          "list[c104.Point]: list of all Point objects (read-only)")
      .def(
          "get_point", &Object::Station::getPoint,
          R"def(get_point(self: c104.Station, io_address: int) -> c104.Point | None

get a point object via information object address

Parameters
----------
io_address: int
    point information object address (value between 0 and 16777215)

Returns
-------
c104.Point, optional
    point object, if found, else None

Example
-------
>>> point_11 = my_station.get_point(io_address=11)
)def",
          "io_address"_a)
      .def(
          "add_point", &Object::Station::addPoint,
          R"def(add_point(self: c104.Station, io_address: int, type: c104.Type, report_ms: int = 0, related_io_address: int = None, related_io_autoreturn: bool = False, command_mode: c104.CommandMode = c104.CommandMode.DIRECT) -> c104.Point | None

add a new point to this station and return the new point object

Parameters
----------
io_address: int
    point information object address (value between 0 and 16777215)
type: c104.Type
    point information type
report_ms: int
    automatic reporting interval in milliseconds (monitoring points server-sided only), 0 = disabled
related_io_address: int, optional
    related monitoring point identified by information object address, that should be auto transmitted on incoming client command (for control points server-sided only)
related_io_autoreturn: bool
    automatic reporting interval in milliseconds (for control points server-sided only)
command_mode: c104.CommandMode
    command transmission mode (direct or select-and-execute)

Returns
-------
c104.Station, optional
    station object, if station was added, else None

Raises
------
ValueError
    io_address or type is invalid
ValueError
    report_ms, related_io_address or related_auto_return is set, but type is not a monitoring type
ValueError
    related_auto_return is set, but related_io_address is not set
ValueError
    related_auto_return is set, but type is not a control type

Example
-------
>>> point_1 = sv_station_1.add_point(common_address=15, type=c104.Type.M_ME_NC_1)
>>> point_2 = sv_station_1.add_point(io_address=11, type=c104.Type.M_ME_NC_1, report_ms=1000)
>>> point_3 = sv_station_1.add_point(io_address=12, type=c104.Type.C_SE_NC_1, report_ms=0, related_io_address=point_2.io_address, related_io_autoreturn=True, command_mode=c104.CommandMode.SELECT_AND_EXECUTE)
)def",
          "io_address"_a, "type"_a, "report_ms"_a = 0,
          "related_io_address"_a = std::nullopt,
          "related_io_autoreturn"_a = false, "command_mode"_a = DIRECT_COMMAND)
      .def("remove_point", &Object::Station::removePoint,
           R"def(remove_point(self: c104.Station, io_address: int) -> bool

remove an existing point from this station

Parameters
----------
io_address: int
    point information object address (value between 0 and 16777215)

Returns
-------
bool
    True if the point was successfully removed, otherwise False.

Example
-------
>>> sv_station_1.remove_point(io_address=17)
)def",
           "io_address"_a)
      .def("signal_initialized", &Object::Station::sendEndOfInitialization,
           R"def(signal_initialized(self: c104.Station, cause: c104.Coi) -> None

signal end of initialization for this station to connected clients

Parameters
----------
cause: c104.Coi
    what caused the (re-)initialization procedure

Returns
-------
None

Example
-------
>>> my_station.signal_initialized(cause=c104.Coi.REMOTE_RESET)
)def",
           "cause"_a)
      .def("__repr__", &Object::Station::toString);

  py::class_<Object::DataPoint, std::shared_ptr<Object::DataPoint>>(
      m, "Point",
      "This class represents command and measurement data point of a station "
      "and provides access to structured properties of points")
      .def_property_readonly(
          "station", &Object::DataPoint::getStation,
          "c104.Station | None : parent Station object (read-only)")
      .def_property_readonly("io_address",
                             &Object::DataPoint::getInformationObjectAddress,
                             "int : information object address (read-only)")
      .def_property_readonly("type", &Object::DataPoint::getType,
                             "c104.Type : data related IEC60870 message type "
                             "identifier (read-only)")
      .def_property("related_io_address",
                    &Object::DataPoint::getRelatedInformationObjectAddress,
                    &Object::DataPoint::setRelatedInformationObjectAddress,
                    "int | None : io_address of a related monitoring "
                    "point or None")
      .def_property(
          "related_io_autoreturn",
          &Object::DataPoint::getRelatedInformationObjectAutoReturn,
          &Object::DataPoint::setRelatedInformationObjectAutoReturn,
          "bool: automatic transmission of return info remote messages for "
          "related point on incoming client command (only for control points)")
      .def_property("command_mode", &Object::DataPoint::getCommandMode,
                    &Object::DataPoint::setCommandMode,
                    "c104.CommandMode : command transmission mode (direct or "
                    "select-and-execute)",
                    py::return_value_policy::copy)
      .def_property_readonly("selected_by",
                             &Object::DataPoint::getSelectedByOriginatorAddress,
                             "int | None : originator address (0-255) or None")
      .def_property("report_ms", &Object::DataPoint::getReportInterval_ms,
                    &Object::DataPoint::setReportInterval_ms,
                    "int : interval in milliseconds between periodic "
                    "transmission, 0 = no periodic transmission")
      .def_property_readonly("timer_ms",
                             &Object::DataPoint::getTimerInterval_ms,
                             "int : interval in milliseconds between timer "
                             "callbacks, 0 = no periodic transmission")
      .def_property("info", &Object::DataPoint::getInfo,
                    &Object::DataPoint::setInfo,
                    "c104.Information : current information",
                    py::return_value_policy::automatic)
      .def_property(
          "value", &Object::DataPoint::getValue, &Object::DataPoint::setValue,
          "typing.Union[None, bool, c104.Double, c104.Step, c104.Int7, "
          "c104.Int16, int, c104.Byte32, c104.NormalizedFloat, float, "
          "c104.EventState, c104.StartEvents, c104.OutputCircuits, "
          "c104.PackedSingle] : the primary information value (this is just a "
          "shortcut to "
          "point.info.value)",
          py::return_value_policy::copy)
      .def_property(
          "quality", &Object::DataPoint::getQuality,
          &Object::DataPoint::setQuality,
          "typing.Union[None, c104.Quality, c104.BinaryCounterQuality] : "
          "the primary quality value (this is just a shortcut to "
          "point.info.quality)",
          py::return_value_policy::copy)
      .def_property_readonly(
          "processed_at", &Object::DataPoint::getProcessedAt,
          "datetime.datetime : timestamp with milliseconds of last local "
          "information processing "
          "(read-only)",
          py::return_value_policy::copy)
      .def_property_readonly("recorded_at", &Object::DataPoint::getRecordedAt,
                             "datetime.datetime | None : timestamp with "
                             "milliseconds transported with the "
                             "value "
                             "itself or None (read-only)",
                             py::return_value_policy::copy)
      .def(
          "on_receive", &Object::DataPoint::setOnReceiveCallback,
          R"def(on_receive(self: c104.Point, callable: collections.abc.Callable[[c104.Point, c104.Information, c104.IncomingMessage], c104.ResponseState]) -> None

set python callback that will be executed on every incoming message
this can be either a command or an monitoring message

Parameters
----------
callable: collections.abc.Callable[[c104.Point, c104.Information, c104.IncomingMessage], c104.ResponseState]
    callback function reference

Returns
-------
None

Raises
------
ValueError
    callable signature does not match exactly

**Callable signature**

Callable Parameters
-------------------
point: c104.Point
    point instance
previous_info: c104.Information
    Information object containing the state of the point before the command took effect
message: c104.IncomingMessage
    new command message

Callable Returns
----------------
c104.ResponseState
    send command SUCCESS or FAILURE response

Example
-------
>>> def on_setpoint_command(point: c104.Point, previous_info: c104.Information, message: c104.IncomingMessage) -> c104.ResponseState:
>>>     print("SV] {0} SETPOINT COMMAND on IOA: {1}, new: {2}, prev: {3}, cot: {4}, quality: {5}".format(point.type, point.io_address, point.value, previous_info, message.cot, point.quality))
>>>     if point.related_io_address:
>>>         print("SV] -> RELATED IO ADDRESS: {}".format(point.related_io_address))
>>>         related_point = sv_station_2.get_point(point.related_io_address)
>>>         if related_point:
>>>             print("SV] -> RELATED POINT VALUE UPDATE")
>>>             related_point.value = point.value
>>>         else:
>>>             print("SV] -> RELATED POINT NOT FOUND!")
>>>     return c104.ResponseState.SUCCESS
>>>
>>> sv_measurement_point = sv_station_2.add_point(io_address=11, type=c104.Type.M_ME_NC_1, report_ms=1000)
>>> sv_measurement_point.value = 12.34
>>> sv_command_point = sv_station_2.add_point(io_address=12, type=c104.Type.C_SE_NC_1, report_ms=0, related_io_address=sv_measurement_point.io_address, related_io_autoreturn=True, command_mode=c104.CommandMode.SELECT_AND_EXECUTE)
>>> sv_command_point.on_receive(callable=on_setpoint_command)
)def",
          "callable"_a)
      .def(
          "on_before_read", &Object::DataPoint::setOnBeforeReadCallback,
          R"def(on_before_read(self: c104.Point, callable: collections.abc.Callable[[c104.Point], None]) -> None

set python callback that will be called on incoming interrogation or read commands to support polling

Parameters
----------
callable: collections.abc.Callable[[c104.Point], None]
    callback function reference

Returns
-------
None

Raises
------
ValueError
    callable signature does not match exactly, parent station reference is invalid or function is called from client context

**Callable signature**

Callable Parameters
-------------------
point: c104.Point
    point instance

Callable Returns
----------------
None

Example
-------
>>> def on_before_read_steppoint(point: c104.Point) -> None:
>>>     print("SV] {0} READ COMMAND on IOA: {1}".format(point.type, point.io_address))
>>>     point.value = random.randint(-64,63)  # import random
>>>
>>> step_point = sv_station_2.add_point(io_address=31, type=c104.Type.M_ST_TB_1, report_ms=2000)
>>> step_point.on_before_read(callable=on_before_read_steppoint)
)def",
          "callable"_a)
      .def(
          "on_before_auto_transmit",
          &Object::DataPoint::setOnBeforeAutoTransmitCallback,
          R"def(on_before_auto_transmit(self: c104.Point, callable: collections.abc.Callable[[c104.Point], None]) -> None

set python callback that will be called before server reports a measured value interval-based

Parameters
----------
callable: collections.abc.Callable[[c104.Point], None]
    callback function reference

Returns
-------
None

Raises
------
ValueError
    callable signature does not match exactly, parent station reference is invalid or function is called from client context

**Callable signature**

Callable Parameters
-------------------
point: c104.Point
    point instance

Callable Returns
----------------
None

Warning
-------
The difference between **on_before_read** and **on_before_auto_transmit** is the calling context.
**on_before_read** is called when a client sends a command to report a point (interrogation or read).
**on_before_auto_transmit** is called when the server reports a measured value interval-based.

Example
-------
>>> def on_before_auto_transmit_step(point: c104.Point) -> None:
>>>     print("SV] {0} PERIODIC TRANSMIT on IOA: {1}".format(point.type, point.io_address))
>>>     point.value = c104.Int7(random.randint(-64,63))  # import random
>>>
>>> step_point = sv_station_2.add_point(io_address=31, type=c104.Type.M_ST_TB_1, report_ms=2000)
>>> step_point.on_before_auto_transmit(callable=on_before_auto_transmit_step)
)def",
          "callable"_a)
      .def(
          "on_timer", &Object::DataPoint::setOnTimerCallback,
          R"def(on_timer(self: c104.Point, callable: collections.abc.Callable[[c104.Point], None], int) -> None

set python callback that will be called in a fixed delay (timer_ms)

Parameters
----------
callable: collections.abc.Callable[[c104.Point], None]
    callback function reference
interval_ms: int
    interval between two callback executions in milliseconds

Returns
-------
None

Raises
------
ValueError
    callable signature does not match exactly, parent station reference is invalid or function is called from client context

**Callable signature**

Callable Parameters
-------------------
point: c104.Point
    point instance
interval_ms: int
    fixed delay between timer callback execution, default: 0, min: 50

Callable Returns
----------------
None

Example
-------
>>> def on_timer(point: c104.Point) -> None:
>>>     print("SV] {0} TIMER on IOA: {1}".format(point.type, point.io_address))
>>>     point.value = random.randint(-64,63)  # import random
>>>
>>> nv_point = sv_station_2.add_point(io_address=31, type=c104.Type.M_ME_TD_1)
>>> nv_point.on_timer(callable=on_timer, interval_ms=1000)
)def",
          "callable"_a, "interval_ms"_a = 0)
      .def("read", &Object::DataPoint::read,
           R"def(read(self: c104.Point) -> bool

send read command

Returns
-------
bool
    True if the command was successfully accepted by the server, otherwise False

Raises
------
ValueError
    parent station or connection reference is invalid or called from remote terminal unit (server) context

Example
-------
>>> if cl_step_point.read():
>>>     print("read command successful")
)def",
           py::return_value_policy::copy)
      .def("transmit", &Object::DataPoint::transmit,
           R"def(transmit(self: c104.Point, cause: c104.Cot) -> bool

**Server-side point**
report a measurement value to connected clients

**Client-side point**
send the command point to the server

Parameters
----------
cause: c104.Cot
    cause of the transmission

Raises
------
ValueError
    parent station, server or connection reference is invalid

Returns
-------
bool
    True if the command was successfully send (server-side) or accepted by the server (client-side), otherwise False

Example
-------
>>> sv_measurement_point.transmit(cause=c104.Cot.SPONTANEOUS)
>>> cl_single_command_point.transmit(cause=c104.Cot.ACTIVATION)
)def",
           "cause"_a, py::return_value_policy::copy)
      .def("__repr__", &Object::DataPoint::toString);

  py::class_<Object::Information, std::shared_ptr<Object::Information>>(
      m, "Information",
      "This class represents all specialized kind of information a specific "
      "point may have")
      .def_property_readonly(
          "value", &Object::Information::getValue,
          R"def(typing.Union[None, bool, c104.Double, c104.Step, c104.Int7, c104.Int16, int, c104.Byte32, c104.NormalizedFloat, float, c104.EventState, c104.StartEvents, c104.OutputCircuits, c104.PackedSingle]: the mapped primary information value property (read-only)

The setter is available via point.value=xyz)def")
      .def_property_readonly(
          "quality", &Object::Information::getQuality,
          R"def(typing.Union[None, c104.Quality, c104.BinaryCounterQuality]: the quality (read-only)

The setter is available via point.quality=xyz)def")
      .def_property_readonly(
          "processed_at", &Object::Information::getProcessedAt,
          "datetime.datetime: timestamp with milliseconds of last local "
          "information processing "
          "(read-only)")
      .def_property_readonly("recorded_at", &Object::Information::getRecordedAt,
                             "datetime.datetime | None : timestamp with "
                             "milliseconds transported with the "
                             "value "
                             "itself or None (read-only)")
      .def_property_readonly("is_readonly", &Object::Information::isReadonly,
                             "bool: test if the information is read-only")
      .def("__repr__", &Object::Information::toString);

  py::class_<Object::SingleInfo, Object::Information,
             std::shared_ptr<Object::SingleInfo>>(
      m, "SingleInfo",
      "This class represents all specific single point information")
      .def(
          py::init(&Object::SingleInfo::create),
          R"def(__init__(self: c104.SingleInfo, on: bool, quality: c104.Quality = c104.Quality(), recorded_at: datetime.datetime = None) -> None

create a new single info

Parameters
----------
on: bool
    Single status value
quality: c104.Quality
    Quality information
recorded_at: datetime.datetime, optional
    Timestamp contained in the protocol message, or None if the protocol message type does not contain a timestamp.

Example
-------
>>> single_info = c104.SingleInfo(on=True, quality=c104.Quality.Invalid, recorded_at=datetime.datetime.now(datetime.utc))
)def",
          "on"_a, "quality"_a = Quality::None, "recorded_at"_a = py::none())
      .def_property_readonly("on", &Object::SingleInfo::isOn,
                             "bool: the value (read-only)")
      .def_property_readonly("value", &Object::SingleInfo::getValue,
                             R"def(bool: references property ``on`` (read-only)

The setter is available via point.value=xyz)def")
      .def_property_readonly("quality", &Object::SingleInfo::getQuality,
                             R"def(c104.Quality: the quality (read-only)

The setter is available via point.quality=xyz)def")
      .def("__repr__", &Object::SingleInfo::toString);

  py::class_<Object::SingleCmd, Object::Information,
             std::shared_ptr<Object::SingleCmd>>(
      m, "SingleCmd",
      "This class represents all specific single command information")
      .def(
          py::init(&Object::SingleCmd::create),
          R"def(__init__(self: c104.SingleCmd, on: bool, qualifier: c104.Qoc = c104.Qoc.NONE, recorded_at: datetime.datetime = None) -> None

create a new single command

Parameters
----------
on: bool
    Single command value
qualifier: c104.Qoc
    Qualifier of command
recorded_at: datetime.datetime, optional
    Timestamp contained in the protocol message, or None if the protocol message type does not contain a timestamp.

Example
-------
>>> single_cmd = c104.SingleCmd(on=True, qualifier=c104.Qoc.SHORT_PULSE, recorded_at=datetime.datetime.now(datetime.utc))
)def",
          "on"_a, "qualifier"_a = CS101_QualifierOfCommand::NONE,
          "recorded_at"_a = py::none())
      .def_property_readonly("on", &Object::SingleCmd::isOn,
                             "bool: the value (read-only)")
      .def_property_readonly("value", &Object::SingleCmd::getValue,
                             R"def(bool: references property ``on`` (read-only)

The setter is available via point.value=xyz)def")
      .def_property_readonly(
          "quality", &Object::SingleCmd::getQuality,
          "None: This information does not contain quality information.")
      .def_property_readonly(
          "qualifier", &Object::SingleCmd::getQualifier,
          "c104.Qoc: the command qualifier information (read-only)")
      .def("__repr__", &Object::SingleCmd::toString);

  py::class_<Object::DoubleInfo, Object::Information,
             std::shared_ptr<Object::DoubleInfo>>(
      m, "DoubleInfo",
      "This class represents all specific double point information")
      .def(
          py::init(&Object::DoubleInfo::create),
          R"def(__init__(self: c104.DoubleInfo, state: c104.Double, quality: c104.Quality = c104.Quality(), recorded_at: datetime.datetime = None) -> None

create a new double info

Parameters
----------
state: c104.Double
    Double point status value
quality: c104.Quality
    Quality information
recorded_at: datetime.datetime, optional
    Timestamp contained in the protocol message, or None if the protocol message type does not contain a timestamp.

Example
-------
>>> double_info = c104.DoubleInfo(state=c104.Double.ON, quality=c104.Quality.Invalid, recorded_at=datetime.datetime.now(datetime.utc))
)def",
          "state"_a, "quality"_a = Quality::None, "recorded_at"_a = py::none())
      .def_property_readonly("state", &Object::DoubleInfo::getState,
                             "c104.Double: the value (read-only)")
      .def_property_readonly(
          "value", &Object::DoubleInfo::getValue,
          R"def(c104.Double: references property ``state`` (read-only)

The setter is available via point.value=xyz)def")
      .def_property_readonly("quality", &Object::DoubleInfo::getQuality,
                             R"def(c104.Quality: the quality (read-only)

The setter is available via point.quality=xyz)def")
      .def("__repr__", &Object::DoubleInfo::toString);

  py::class_<Object::DoubleCmd, Object::Information,
             std::shared_ptr<Object::DoubleCmd>>(
      m, "DoubleCmd",
      "This class represents all specific double command information")
      .def(
          py::init(&Object::DoubleCmd::create),
          R"def(__init__(self: c104.DoubleCmd, state: c104.Double, qualifier: c104.Qoc = c104.Qoc.NONE, recorded_at: datetime.datetime = None) -> None

create a new double command

Parameters
----------
state: c104.Double
    Double command value
qualifier: c104.Qoc
    Qualifier of command
recorded_at: datetime.datetime, optional
    Timestamp contained in the protocol message, or None if the protocol message type does not contain a timestamp.

Example
-------
>>> double_cmd = c104.DoubleCmd(state=c104.Double.ON, qualifier=c104.Qoc.SHORT_PULSE, recorded_at=datetime.datetime.now(datetime.utc))
)def",
          "state"_a, "qualifier"_a = CS101_QualifierOfCommand::NONE,
          "recorded_at"_a = py::none())
      .def_property_readonly("state", &Object::DoubleCmd::getState,
                             "c104.Double: the value (read-only)")
      .def_property_readonly(
          "qualifier", &Object::DoubleCmd::getQualifier,
          "c104.Qoc: the command qualifier information (read-only)")
      .def_property_readonly(
          "value", &Object::DoubleCmd::getValue,
          R"def(c104.Double: references property ``state`` (read-only)

The setter is available via point.value=xyz)def")
      .def_property_readonly(
          "quality", &Object::DoubleCmd::getQuality,
          "None: This information does not contain quality information.")
      .def("__repr__", &Object::DoubleCmd::toString);

  py::class_<Object::StepInfo, Object::Information,
             std::shared_ptr<Object::StepInfo>>(
      m, "StepInfo",
      "This class represents all specific step point information")
      .def(
          py::init(&Object::StepInfo::create),
          R"def(__init__(self: c104.StepInfo, position: c104.Int7, transient: bool, quality: c104.Quality = c104.Quality(), recorded_at: datetime.datetime = None) -> None

create a new step info

Parameters
----------
position: c104.Int7
    Current transformer step position value
transient: bool
    Indicator, if transformer is currently in step change procedure
quality: c104.Quality
    Quality information
recorded_at: datetime.datetime, optional
    Timestamp contained in the protocol message, or None if the protocol message type does not contain a timestamp.

Example
-------
>>> step_info = c104.StepInfo(position=c104.Int7(2), transient=False, quality=c104.Quality.Invalid, recorded_at=datetime.datetime.now(datetime.utc))
)def",
          "state"_a, "transient"_a = false, "quality"_a = Quality::None,
          "recorded_at"_a = py::none())
      .def_property_readonly("position", &Object::StepInfo::getPosition,
                             "c104.Int7: the value (read-only)")
      .def_property_readonly("transient", &Object::StepInfo::isTransient,
                             "bool: if the position is transient (read-only)")
      .def_property_readonly(
          "value", &Object::StepInfo::getValue,
          R"def(c104.Int7: references property ``position`` (read-only)

The setter is available via point.value=xyz)def")
      .def_property_readonly("quality", &Object::StepInfo::getQuality,
                             R"def(c104.Quality: the quality (read-only)

The setter is available via point.quality=xyz)def")
      .def("__repr__", &Object::StepInfo::toString);

  py::class_<Object::StepCmd, Object::Information,
             std::shared_ptr<Object::StepCmd>>(
      m, "StepCmd",
      "This class represents all specific step command information")
      .def(
          py::init(&Object::StepCmd::create),
          R"def(__init__(self: c104.StepCmd, direction: c104.Step, qualifier: c104.Qoc = c104.Qoc.NONE, recorded_at: datetime.datetime = None) -> None

create a new step command

Parameters
----------
direction: c104.Step
    Step command direction value
qualifier: c104.Qoc
    Qualifier of Command
recorded_at: datetime.datetime, optional
    Timestamp contained in the protocol message, or None if the protocol message type does not contain a timestamp.

Example
-------
>>> step_cmd = c104.StepCmd(direction=c104.Step.HIGHER, qualifier=c104.Qoc.SHORT_PULSE, recorded_at=datetime.datetime.now(datetime.utc))
)def",
          "direction"_a, "qualifier"_a = CS101_QualifierOfCommand::NONE,
          "recorded_at"_a = py::none())
      .def_property_readonly("direction", &Object::StepCmd::getStep,
                             "c104.Step: the value (read-only)")
      .def_property_readonly(
          "qualifier", &Object::StepCmd::getQualifier,
          "c104.Qoc: the command qualifier information (read-only)")
      .def_property_readonly(
          "value", &Object::DoubleCmd::getValue,
          R"def(c104.Step: references property ``direction`` (read-only)

The setter is available via point.value=xyz)def")
      .def_property_readonly(
          "quality", &Object::DoubleCmd::getQuality,
          "None: This information does not contain quality information.")
      .def("__repr__", &Object::StepCmd::toString);

  py::class_<Object::BinaryInfo, Object::Information,
             std::shared_ptr<Object::BinaryInfo>>(
      m, "BinaryInfo",
      "This class represents all specific binary point information")
      .def(
          py::init(&Object::BinaryInfo::create),
          R"def(__init__(self: c104.BinaryInfo, blob: c104.Byte32, quality: c104.Quality = c104.Quality(), recorded_at: datetime.datetime = None) -> None

create a new binary info

Parameters
----------
blob: c104.Byte32
    Binary status value
quality: c104.Quality
    Quality information
recorded_at: datetime.datetime, optional
    Timestamp contained in the protocol message, or None if the protocol message type does not contain a timestamp.

Example
-------
>>> binary_info = c104.BinaryInfo(blob=c104.Byte32(2345), quality=c104.Quality.Invalid, recorded_at=datetime.datetime.now(datetime.utc))
)def",
          "blob"_a, "quality"_a = Quality::None, "recorded_at"_a = py::none())
      .def_property_readonly("blob", &Object::BinaryInfo::getBlob,
                             "c104.Byte32: the value (read-only)")
      .def_property_readonly(
          "value", &Object::BinaryInfo::getValue,
          R"def(c104.Byte32: references property ``blob`` (read-only)

The setter is available via point.value=xyz)def")
      .def_property_readonly("quality", &Object::BinaryInfo::getQuality,
                             R"def(c104.Quality: the quality (read-only)

The setter is available via point.quality=xyz)def")
      .def("__repr__", &Object::BinaryInfo::toString);

  py::class_<Object::BinaryCmd, Object::Information,
             std::shared_ptr<Object::BinaryCmd>>(
      m, "BinaryCmd",
      "This class represents all specific binary command information")
      .def(
          py::init(&Object::BinaryCmd::create),
          R"def(__init__(self: c104.BinaryCmd, blob: c104.Byte32, recorded_at: datetime.datetime = None) -> None

create a new binary command

Parameters
----------
blob: c104.Byte32
    Binary command value
recorded_at: datetime.datetime, optional
    Timestamp contained in the protocol message, or None if the protocol message type does not contain a timestamp.

Example
-------
>>> binary_cmd = c104.BinaryCmd(blob=c104.Byte32(1234), recorded_at=datetime.datetime.now(datetime.utc))
)def",
          "blob"_a, "recorded_at"_a = py::none())
      .def_property_readonly("blob", &Object::BinaryCmd::getBlob,
                             "c104.Byte32: the value (read-only)")
      .def_property_readonly(
          "value", &Object::BinaryCmd::getValue,
          R"def(c104.Byte32: references property ``blob`` (read-only)

The setter is available via point.value=xyz)def")
      .def_property_readonly(
          "quality", &Object::BinaryCmd::getQuality,
          "None: This information does not contain quality information.")
      .def("__repr__", &Object::BinaryCmd::toString);

  py::class_<Object::NormalizedInfo, Object::Information,
             std::shared_ptr<Object::NormalizedInfo>>(
      m, "NormalizedInfo",
      "This class represents all specific normalized measurement point "
      "information")
      .def(
          py::init(&Object::NormalizedInfo::create),
          R"def(__init__(self: c104.NormalizedInfo, actual: c104.NormalizedFloat, quality: c104.Quality = c104.Quality(), recorded_at: datetime.datetime = None) -> None

create a new normalized measurement info

Parameters
----------
actual: c104.NormalizedFloat
    Actual measurement value [-1.f, 1.f]
quality: c104.Quality
    Quality information
recorded_at: datetime.datetime, optional
    Timestamp contained in the protocol message, or None if the protocol message type does not contain a timestamp.

Example
-------
>>> normalized_info = c104.NormalizedInfo(actual=c104.NormalizedFloat(23.45), quality=c104.Quality.Invalid, recorded_at=datetime.datetime.now(datetime.utc))
)def",
          "actual"_a, "quality"_a = Quality::None, "recorded_at"_a = py::none())
      .def_property_readonly("actual", &Object::NormalizedInfo::getActual,
                             "c104.NormalizedFloat: the value (read-only)")
      .def_property_readonly(
          "value", &Object::NormalizedInfo::getValue,
          R"def(c104.NormalizedFloat: references property ``actual`` (read-only)

The setter is available via point.value=xyz)def")
      .def_property_readonly("quality", &Object::NormalizedInfo::getQuality,
                             R"def(c104.Quality: the quality (read-only)

The setter is available via point.quality=xyz)def")
      .def("__repr__", &Object::NormalizedInfo::toString);

  py::class_<Object::NormalizedCmd, Object::Information,
             std::shared_ptr<Object::NormalizedCmd>>(
      m, "NormalizedCmd",
      "This class represents all specific normalized set point command "
      "information")
      .def(
          py::init(&Object::NormalizedCmd::create),
          R"def(__init__(self: c104.NormalizedCmd, target: c104.NormalizedFloat, qualifier: c104.UInt7 = c104.UInt7(0), recorded_at: datetime.datetime = None) -> None

create a new normalized set point command

Parameters
----------
target: c104.NormalizedFloat
    Target set-point value [-1.f, 1.f]
qualifier: c104.UInt7
    Qualifier of set-point command
recorded_at: datetime.datetime, optional
    Timestamp contained in the protocol message, or None if the protocol message type does not contain a timestamp.

Example
-------
>>> normalized_cmd = c104.NormalizedCmd(target=c104.NormalizedFloat(23.45), qualifier=c104.UInt7(123), recorded_at=datetime.datetime.now(datetime.utc))
)def",
          "target"_a, "qualifier"_a = LimitedUInt7(0),
          "recorded_at"_a = py::none())
      .def_property_readonly("target", &Object::NormalizedCmd::getTarget,
                             "c104.NormalizedFloat: the value (read-only)")
      .def_property_readonly(
          "qualifier", &Object::NormalizedCmd::getQualifier,
          "c104.UInt7: the command qualifier information (read-only)")
      .def_property_readonly(
          "value", &Object::NormalizedCmd::getValue,
          R"def(c104.NormalizedFloat: references property ``target`` (read-only)

The setter is available via point.value=xyz)def")
      .def_property_readonly(
          "quality", &Object::NormalizedCmd::getQuality,
          "None: This information does not contain quality information.")
      .def("__repr__", &Object::NormalizedCmd::toString);

  py::class_<Object::ScaledInfo, Object::Information,
             std::shared_ptr<Object::ScaledInfo>>(
      m, "ScaledInfo",
      "This class represents all specific scaled measurement point information")
      .def(
          py::init(&Object::ScaledInfo::create),
          R"def(__init__(self: c104.ScaledInfo, actual: c104.Int16, quality: c104.Quality = c104.Quality(), recorded_at: datetime.datetime = None) -> None

create a new scaled measurement info

Parameters
----------
actual: c104.Int16
    Actual measurement value [-32768, 32767]
quality: c104.Quality
    Quality information
recorded_at: datetime.datetime, optional
    Timestamp contained in the protocol message, or None if the protocol message type does not contain a timestamp.

Example
-------
>>> scaled_info = c104.ScaledInfo(actual=c104.Int16(-2345), quality=c104.Quality.Invalid, recorded_at=datetime.datetime.now(datetime.utc))
)def",
          "actual"_a, "quality"_a = Quality::None, "recorded_at"_a = py::none())
      .def_property_readonly("actual", &Object::ScaledInfo::getActual,
                             "c104.Int16: the value (read-only)")
      .def_property_readonly(
          "value", &Object::ScaledInfo::getValue,
          R"def(c104.Int16: references property ``actual`` (read-only)

The setter is available via point.value=xyz)def")
      .def_property_readonly("quality", &Object::ScaledInfo::getQuality,
                             R"def(c104.Quality: the quality (read-only)

The setter is available via point.quality=xyz)def")
      .def("__repr__", &Object::ScaledInfo::toString);

  py::class_<Object::ScaledCmd, Object::Information,
             std::shared_ptr<Object::ScaledCmd>>(
      m, "ScaledCmd",
      "This class represents all specific scaled set point command information")
      .def(
          py::init(&Object::ScaledCmd::create),
          R"def(__init__(self: c104.ScaledCmd, target: c104.Int16, qualifier: c104.UInt7 = c104.UInt7(0), recorded_at: datetime.datetime = None) -> None

create a new scaled set point command

Parameters
----------
target: c104.Int16
    Target set-point value [-32768, 32767]
qualifier: c104.UInt7
    Qualifier of set-point command
recorded_at: datetime.datetime, optional
    Timestamp contained in the protocol message, or None if the protocol message type does not contain a timestamp.

Example
-------
>>> scaled_cmd = c104.ScaledCmd(target=c104.Int16(-2345), qualifier=c104.UInt7(123), recorded_at=datetime.datetime.now(datetime.utc))
)def",
          "target"_a, "qualifier"_a = LimitedUInt7(0),
          "recorded_at"_a = py::none())
      .def_property_readonly("target", &Object::ScaledCmd::getTarget,
                             "c104.Int16: the value (read-only)")
      .def_property_readonly(
          "qualifier", &Object::ScaledCmd::getQualifier,
          "c104.UInt7: the command qualifier information (read-only)")
      .def_property_readonly(
          "value", &Object::ScaledCmd::getValue,
          R"def(c104.Int16: references property ``target`` (read-only)

The setter is available via point.value=xyz)def")
      .def_property_readonly(
          "quality", &Object::ScaledCmd::getQuality,
          "None: This information does not contain quality information.")
      .def("__repr__", &Object::ScaledCmd::toString);

  py::class_<Object::ShortInfo, Object::Information,
             std::shared_ptr<Object::ShortInfo>>(
      m, "ShortInfo",
      "This class represents all specific short measurement point information")
      .def(
          py::init(&Object::ShortInfo::create),
          R"def(__init__(self: c104.ShortInfo, actual: float, quality: c104.Quality = c104.Quality(), recorded_at: datetime.datetime = None) -> None

create a new short measurement info

Parameters
----------
actual: float
    Actual measurement value in 32-bit precision
quality: c104.Quality
    Quality information
recorded_at: datetime.datetime, optional
    Timestamp contained in the protocol message, or None if the protocol message type does not contain a timestamp.

Example
-------
>>> short_info = c104.ShortInfo(actual=23.45, quality=c104.Quality.Invalid, recorded_at=datetime.datetime.now(datetime.utc))
)def",
          "actual"_a, "quality"_a = Quality::None, "recorded_at"_a = py::none())
      .def_property_readonly("actual", &Object::ShortInfo::getActual,
                             "float: the value (read-only)")
      .def_property_readonly(
          "value", &Object::ShortInfo::getValue,
          R"def(float: references property ``actual`` (read-only)

The setter is available via point.value=xyz)def")
      .def_property_readonly("quality", &Object::ShortInfo::getQuality,
                             R"def(c104.Quality: the quality (read-only)

The setter is available via point.quality=xyz)def")
      .def("__repr__", &Object::ShortInfo::toString);

  py::class_<Object::ShortCmd, Object::Information,
             std::shared_ptr<Object::ShortCmd>>(
      m, "ShortCmd",
      "This class represents all specific short set point command information")
      .def(
          py::init(&Object::ShortCmd::create),
          R"def(__init__(self: c104.ShortCmd, target: float, qualifier: c104.UInt7 = c104.UInt7(0), recorded_at: datetime.datetime = None) -> None

create a new short set point command

Parameters
----------
target: float
    Target set-point value in 32-bit precision
qualifier: c104.UInt7
    Qualifier of set-point command
recorded_at: datetime.datetime, optional
    Timestamp contained in the protocol message, or None if the protocol message type does not contain a timestamp.

Example
-------
>>> short_cmd = c104.ShortCmd(target=-23.45, qualifier=c104.UInt7(123), recorded_at=datetime.datetime.now(datetime.utc))
)def",
          "target"_a, "qualifier"_a = LimitedUInt7(0),
          "recorded_at"_a = py::none())
      .def_property_readonly("target", &Object::ShortCmd::getTarget,
                             "float: the value (read-only)")
      .def_property_readonly(
          "qualifier", &Object::ShortCmd::getQualifier,
          "c104.UInt7: the command qualifier information (read-only)")
      .def_property_readonly(
          "value", &Object::ShortCmd::getValue,
          R"def(float: references property ``target`` (read-only)

The setter is available via point.value=xyz)def")
      .def_property_readonly(
          "quality", &Object::ShortCmd::getQuality,
          "None: This information does not contain quality information.")
      .def("__repr__", &Object::ShortCmd::toString);

  py::class_<Object::BinaryCounterInfo, Object::Information,
             std::shared_ptr<Object::BinaryCounterInfo>>(
      m, "BinaryCounterInfo",
      "This class represents all specific integrated totals of binary counter "
      "point information")
      .def(
          py::init(&Object::BinaryCounterInfo::create),
          R"def(__init__(self: c104.BinaryCounterInfo, counter: int, sequence: c104.UInt5, quality: c104.BinaryCounterQuality = c104.BinaryCounterQuality(), recorded_at: datetime.datetime = None) -> None

create a new short measurement info

Parameters
----------
counter: int
    Counter value
sequence: c104.UInt5
    Counter info sequence number
quality: c104.BinaryCounterQuality
    Binary counter quality information
recorded_at: datetime.datetime, optional
    Timestamp contained in the protocol message, or None if the protocol message type does not contain a timestamp.

Example
-------
>>> counter_info = c104.BinaryCounterInfo(counter=2345, sequence=c104.UInt5(35), quality=c104.Quality.Invalid, recorded_at=datetime.datetime.now(datetime.utc))
)def",
          "counter"_a, "sequence"_a = LimitedUInt5(0),
          "quality"_a = Quality::None, "recorded_at"_a = py::none())
      .def_property_readonly("counter", &Object::BinaryCounterInfo::getCounter,
                             "int: the value (read-only)")
      .def_property_readonly(
          "sequence", &Object::BinaryCounterInfo::getSequence,
          "c104.UInt5: the counter sequence number (read-only)")
      .def_property_readonly(
          "value", &Object::BinaryCounterInfo::getValue,
          R"def(int: references property ``counter`` (read-only)

The setter is available via point.value=xyz)def")
      .def_property_readonly(
          "quality", &Object::BinaryCounterInfo::getQuality,
          R"def(c104.BinaryCounterQuality: the quality (read-only)

The setter is available via point.quality=xyz)def")
      .def("__repr__", &Object::BinaryCounterInfo::toString);

  py::class_<Object::ProtectionEquipmentEventInfo, Object::Information,
             std::shared_ptr<Object::ProtectionEquipmentEventInfo>>(
      m, "ProtectionEventInfo",
      "This class represents all specific protection equipment single event "
      "point information")
      .def(
          py::init(&Object::ProtectionEquipmentEventInfo::create),
          R"def(__init__(self: c104.ProtectionEventInfo, state: c104.EventState, elapsed_ms: c104.UInt16, quality: c104.Quality = c104.Quality(), recorded_at: datetime.datetime = None) -> None

create a new event info raised by protection equipment

Parameters
----------
state: c104.EventState
    State of the event
elapsed_ms: c104.UInt16
    Time in milliseconds elapsed
quality: c104.Quality
    Quality information
recorded_at: datetime.datetime, optional
    Timestamp contained in the protocol message, or None if the protocol message type does not contain a timestamp.

Example
-------
>>> single_event = c104.ProtectionEventInfo(state=c104.EventState.ON, elapsed_ms=c104.UInt16(35000), quality=c104.Quality.Invalid, recorded_at=datetime.datetime.now(datetime.utc))
)def",
          "state"_a, "elapsed_ms"_a = LimitedUInt16(0),
          "quality"_a = Quality::None, "recorded_at"_a = py::none())
      .def_property_readonly("state",
                             &Object::ProtectionEquipmentEventInfo::getState,
                             "c104.EventState: the state (read-only)")
      .def_property_readonly(
          "value", &Object::ProtectionEquipmentEventInfo::getValue,
          R"def(c104.EventState: references property ``state`` (read-only)

The setter is available via point.value=xyz)def")
      .def_property_readonly("quality",
                             &Object::ProtectionEquipmentEventInfo::getQuality,
                             R"def(c104.Quality: the quality (read-only)

The setter is available via point.quality=xyz)def")
      .def_property_readonly(
          "elapsed_ms", &Object::ProtectionEquipmentEventInfo::getElapsed_ms,
          "int: the elapsed time in milliseconds (read-only)")
      .def("__repr__", &Object::ProtectionEquipmentEventInfo::toString);

  py::class_<Object::ProtectionEquipmentStartEventsInfo, Object::Information,
             std::shared_ptr<Object::ProtectionEquipmentStartEventsInfo>>(
      m, "ProtectionStartInfo",
      "This class represents all specific protection equipment packed start "
      "events point information")
      .def(
          py::init(&Object::ProtectionEquipmentStartEventsInfo::create),
          R"def(__init__(self: c104.ProtectionStartInfo, events: c104.StartEvents, relay_duration_ms: c104.UInt16, quality: c104.Quality = c104.Quality(), recorded_at: datetime.datetime = None) -> None

create a new packed event start info raised by protection equipment

Parameters
----------
events: c104.StartEvents
    Set of start events
relay_duration_ms: c104.UInt16
    Time in milliseconds of relay duration
quality: c104.Quality
    Quality information
recorded_at: datetime.datetime, optional
    Timestamp contained in the protocol message, or None if the protocol message type does not contain a timestamp.

Example
-------
>>> start_events = c104.ProtectionStartInfo(events=c104.StartEvents.ON, relay_duration_ms=c104.UInt16(35000), quality=c104.Quality.Invalid, recorded_at=datetime.datetime.now(datetime.utc))
)def",
          "events"_a, "relay_duration_ms"_a = LimitedUInt16(0),
          "quality"_a = Quality::None, "recorded_at"_a = py::none())
      .def_property_readonly(
          "events", &Object::ProtectionEquipmentStartEventsInfo::getEvents,
          "c104.StartEvents: the started events (read-only)")
      .def_property_readonly(
          "relay_duration_ms",
          &Object::ProtectionEquipmentStartEventsInfo::getRelayDuration_ms,
          "int: the relay duration information (read-only)")
      .def_property_readonly(
          "value", &Object::ProtectionEquipmentStartEventsInfo::getValue,
          R"def(c104.StartEvents: references property ``events`` (read-only)

The setter is available via point.value=xyz)def")
      .def_property_readonly(
          "quality", &Object::ProtectionEquipmentStartEventsInfo::getQuality,
          R"def(c104.Quality: the quality (read-only)

The setter is available via point.quality=xyz)def")
      .def("__repr__", &Object::ProtectionEquipmentStartEventsInfo::toString);

  py::class_<Object::ProtectionEquipmentOutputCircuitInfo, Object::Information,
             std::shared_ptr<Object::ProtectionEquipmentOutputCircuitInfo>>(
      m, "ProtectionCircuitInfo",
      "This class represents all specific protection equipment output circuit "
      "point information")
      .def(
          py::init(&Object::ProtectionEquipmentOutputCircuitInfo::create),
          R"def(__init__(self: c104.ProtectionCircuitInfo, circuits: c104.OutputCircuits, relay_operating_ms: c104.UInt16, quality: c104.Quality = c104.Quality(), recorded_at: datetime.datetime = None) -> None

create a new output circuits info raised by protection equipment

Parameters
----------
circuits: c104.OutputCircuits
    Set of output circuits
relay_operating_ms: c104.UInt16
    Time in milliseconds of relay operation
quality: c104.Quality
    Quality information
recorded_at: datetime.datetime, optional
    Timestamp contained in the protocol message, or None if the protocol message type does not contain a timestamp.

Example
-------
>>> output_circuits = c104.ProtectionCircuitInfo(events=c104.OutputCircuits.PhaseL1|c104.OutputCircuits.PhaseL2, relay_operating_ms=c104.UInt16(35000), quality=c104.Quality.Invalid, recorded_at=datetime.datetime.now(datetime.utc))
)def",
          "events"_a, "relay_duration_ms"_a = LimitedUInt16(0),
          "quality"_a = Quality::None, "recorded_at"_a = py::none())
      .def_property_readonly(
          "circuits",
          &Object::ProtectionEquipmentOutputCircuitInfo::getCircuits,
          "c104.OutputCircuits: the started events (read-only)")
      .def_property_readonly(
          "relay_operating_ms",
          &Object::ProtectionEquipmentOutputCircuitInfo::getRelayOperating_ms,
          "int: the relay operation duration information (read-only)")
      .def_property_readonly(
          "value", &Object::ProtectionEquipmentOutputCircuitInfo::getValue,
          R"def(c104.OutputCircuits: references property ``circuits`` (read-only)

The setter is available via point.value=xyz)def")
      .def_property_readonly(
          "quality", &Object::ProtectionEquipmentOutputCircuitInfo::getQuality,
          R"def(c104.Quality: the quality (read-only)

The setter is available via point.quality=xyz)def")
      .def("__repr__", &Object::ProtectionEquipmentOutputCircuitInfo::toString);

  py::class_<Object::StatusWithChangeDetection, Object::Information,
             std::shared_ptr<Object::StatusWithChangeDetection>>(
      m, "StatusAndChanged",
      "This class represents all specific packed status point information with "
      "change detection")
      .def(
          py::init(&Object::StatusWithChangeDetection::create),
          R"def(__init__(self: c104.StatusAndChanged, status: c104.PackedSingle, changed: c104.PackedSingle, quality: c104.Quality = c104.Quality(), recorded_at: datetime.datetime = None) -> None

create a new event info raised by protection equipment

Parameters
----------
status: c104.PackedSingle
    Set of current single values
changed: c104.PackedSingle
    Set of changed single values
quality: c104.Quality
    Quality information
recorded_at: datetime.datetime, optional
    Timestamp contained in the protocol message, or None if the protocol message type does not contain a timestamp.

Example
-------
>>> status_and_changed = c104.StatusAndChanged(status=c104.PackedSingle.I0|c104.PackedSingle.I5, changed=c104.PackedSingle(15), quality=c104.Quality.Invalid, recorded_at=datetime.datetime.now(datetime.utc))
)def",
          "status"_a, "changed"_a = FieldSet16(0), "quality"_a = Quality::None,
          "recorded_at"_a = py::none())
      .def_property_readonly(
          "status", &Object::StatusWithChangeDetection::getStatus,
          "c104.PackedSingle: the current status (read-only)")
      .def_property_readonly(
          "changed", &Object::StatusWithChangeDetection::getChanged,
          "c104.PackedSingle: the changed information (read-only)")
      .def_property_readonly(
          "value", &Object::StatusWithChangeDetection::getValue,
          R"def(c104.PackedSingle: references property ``status`` (read-only)

The setter is available via point.value=xyz)def")
      .def_property_readonly("quality",
                             &Object::StatusWithChangeDetection::getQuality,
                             R"def(c104.Quality: the quality (read-only)

The setter is available via point.quality=xyz)def")
      .def("__repr__", &Object::StatusWithChangeDetection::toString);

  py::class_<Remote::Message::IncomingMessage,
             std::shared_ptr<Remote::Message::IncomingMessage>>(
      m, "IncomingMessage",
      "This class represents incoming messages and provides access to "
      "structured properties interpreted from incoming messages")
      .def_property_readonly(
          "type", &Remote::Message::IncomingMessage::getType,
          "c104.Type: IEC60870 message type identifier (read-only)",
          py::return_value_policy::copy)
      .def_property_readonly(
          "common_address", &Remote::Message::IncomingMessage::getCommonAddress,
          "int: common address (1-65534) (read-only)",
          py::return_value_policy::copy)
      .def_property_readonly(
          "originator_address",
          &Remote::Message::IncomingMessage::getOriginatorAddress,
          "int: originator address (0-255) (read-only)",
          py::return_value_policy::copy)
      .def_property_readonly(
          "io_address", &Remote::Message::IncomingMessage::getIOA,
          "int: information object address (0-16777215) (read-only)",
          py::return_value_policy::copy)
      .def_property_readonly(
          "cot", &Remote::Message::IncomingMessage::getCauseOfTransmission,
          "c104.Cot: cause of transmission (read-only)",
          py::return_value_policy::copy)
      .def_property_readonly("info", &Remote::Message::IncomingMessage::getInfo,
                             "c104.Information: value (read-only)")
      .def_property_readonly("is_test",
                             &Remote::Message::IncomingMessage::isTest,
                             "bool: test if test flag is set (read-only)")
      .def_property_readonly("is_sequence",
                             &Remote::Message::IncomingMessage::isSequence,
                             "bool: test if sequence flag is set (read-only)")
      .def_property_readonly("is_negative",
                             &Remote::Message::IncomingMessage::isNegative,
                             "bool: test if negative flag is set (read-only)")
      .def_property_readonly("raw", &IncomingMessage_getRawBytes,
                             "bytes: raw ASDU message bytes (read-only)",
                             py::return_value_policy::take_ownership)
      .def_property_readonly(
          "raw_explain", &Remote::Message::IncomingMessage::getRawMessageString,
          "str: ASDU message bytes explained (read-only)",
          py::return_value_policy::copy)
      .def_property_readonly(
          "number_of_object",
          &Remote::Message::IncomingMessage::getNumberOfObjects,
          "int: represents the number of information objects (read-only) "
          "(deprecated, use "
          "``number_of_objects`` instead)",
          py::return_value_policy::copy)
      .def_property_readonly(
          "number_of_objects",
          &Remote::Message::IncomingMessage::getNumberOfObjects,
          "int: represents the number of information objects contained in this "
          "message (read-only)",
          py::return_value_policy::copy)
      .def_property_readonly("is_select_command",
                             &Remote::Message::IncomingMessage::isSelectCommand,
                             "bool: test if message is a point command and has "
                             "select flag set (read-only)",
                             py::return_value_policy::copy)
      .def("first", &Remote::Message::IncomingMessage::first,
           R"def(first(self: c104.IncomingMessage) -> None

reset message information element pointer to first position

Returns
-------
None
)def")
      .def("next", &Remote::Message::IncomingMessage::next,
           R"def(next(self: c104.IncomingMessage) -> bool

move message information element pointer to next position, starting by first one

Returns
-------
bool
    True, if another information element exists, otherwise False
)def")
      .def("__repr__", &Remote::Message::IncomingMessage::toString);

  py::class_<Remote::Message::Batch, std::shared_ptr<Remote::Message::Batch>>(
      m, "Batch",
      "This class represents a batch of outgoing monitoring messages of the "
      "same station "
      "and type")
      .def(
          py::init(&Remote::Message::Batch::create),
          R"def(__init__(self, cause: c104.Cot, points: list[c104.Point] | None = None) -> None

create a new batch of monitoring messages of the same station and the same type

Parameters
----------
cause: c104.Cot
    what caused the transmission of the monitoring data
points: list[c104.Point], optional
    initial list of points

Raises
------
ValueError
    if one point in the list is not compatible with the others

Example
-------
>>> batch = c104.Batch(cause=c104.Cot.SPONTANEOUS, points=[point1, point2, point3])
)def",
          "cause"_a, "points"_a = py::none())
      .def_property_readonly(
          "type", &Remote::Message::Batch::getType,
          "c104.Type: IEC60870 message type identifier (read-only)",
          py::return_value_policy::copy)
      .def_property_readonly("common_address",
                             &Remote::Message::Batch::getCommonAddress,
                             "int: common address (1-65534) (read-only)",
                             py::return_value_policy::copy)
      .def_property_readonly("originator_address",
                             &Remote::Message::Batch::getOriginatorAddress,
                             "int: originator address (0-255) (read-only)",
                             py::return_value_policy::copy)
      .def_property_readonly("cot",
                             &Remote::Message::Batch::getCauseOfTransmission,
                             "c104.Cot: cause of transmission (read-only)",
                             py::return_value_policy::copy)
      .def_property_readonly("is_test", &Remote::Message::Batch::isTest,
                             "bool: test if test flag is set (read-only)")
      .def_property_readonly("is_sequence", &Remote::Message::Batch::isSequence,
                             "bool: test if sequence flag is set (read-only)")
      .def_property_readonly("is_negative", &Remote::Message::Batch::isNegative,
                             "bool: test if negative flag is set (read-only)")
      .def_property_readonly(
          "number_of_objects", &Remote::Message::Batch::getNumberOfObjects,
          "int: represents the number of information objects (read-only)",
          py::return_value_policy::copy)
      .def_property_readonly("has_points", &Remote::Message::Batch::hasPoints,
                             "bool: test if batch contains points (read-only)")
      .def_property_readonly(
          "points", &Remote::Message::Batch::getPoints,
          "list[c104.Point]: get a list of contained points (read-only)")
      .def("add_point", &Remote::Message::Batch::addPoint,
           R"def(add_point(self: c104.Batch, point: c104.Point) -> None

add a new point to this Batch

Parameters
----------
point: c104.Point
    to be added point

Returns
-------
None

Raises
------
ValueError
    if point is not compatible with the batch or if it is already in the batch

Example
-------
>>> my_batch.add_point(my_point)
)def",
           "point"_a)
      .def("__repr__", &Remote::Message::Batch::toString);
  ;

  //*/

  m.attr("__version__") = VERSION_INFO;
}
