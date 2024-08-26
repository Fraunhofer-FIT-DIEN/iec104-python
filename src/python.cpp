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
 * @file main.cpp
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

#include <pybind11/chrono.h>
#include <pybind11/stl.h>

#ifdef VERSION_INFO
#define PY_MODULE(name, var) PYBIND11_MODULE(name, var)
#else
#define VERSION_INFO "embedded"
#include <pybind11/embed.h>
#define PY_MODULE(name, var) PYBIND11_EMBEDDED_MODULE(name, var)
#endif

using namespace pybind11::literals;

// set UNBUFFERED mode for correct order of stdout flush between python and c++
struct EnvironmentInitializer {
  EnvironmentInitializer() {
#if defined(_WIN32) || defined(_WIN64)
    _putenv("PYTHONUNBUFFERED=1");
#else
    // Use setenv on Linux
    setenv("PYTHONUNBUFFERED", "1", 1);
#endif
  }
};
// Initialize the environment variable before main() is called
static EnvironmentInitializer initializer;

// @todo Ubuntu 18 x64, Ubuntu 20 x64, arm7v32, later: arm aarch64

// Bind Number with Template
template <typename T, typename Params, typename W>
py::class_<BaseNumber<T, Params, W>> bind_BaseNumber(py::module &m,
                                                     const std::string &name) {
  return py::class_<BaseNumber<T, Params, W>,
                    std::shared_ptr<BaseNumber<T, Params, W>>>(m, name.c_str())
      .def(py::init<T>())
      .def(py::init<W>())
      // Overloading operators with different types
      .def("__add__", [](const BaseNumber<T, Params, W> &self,
                         const W &other) { return self + other; })
      .def("__sub__", [](const BaseNumber<T, Params, W> &self,
                         const W &other) { return self - other; })
      .def("__mul__", [](const BaseNumber<T, Params, W> &self,
                         const W &other) { return self * other; })
      .def("__truediv__", [](const BaseNumber<T, Params, W> &self,
                             const W &other) { return self / other; })
      .def("__iadd__",
           [](BaseNumber<T, Params, W> &self, const W &other) {
             self += other;
             return self;
           })
      .def("__isub__",
           [](BaseNumber<T, Params, W> &self, const W &other) {
             self -= other;
             return self;
           })
      .def("__imul__",
           [](BaseNumber<T, Params, W> &self, const W &other) {
             self *= other;
             return self;
           })
      .def("__itruediv__",
           [](BaseNumber<T, Params, W> &self, const W &other) {
             self /= other;
             return self;
           })
      .def("__int__",
           [](const BaseNumber<T, Params, W> &a) {
             T value = a.get();
             return static_cast<int>(value);
           })
      .def("__float__",
           [](const BaseNumber<T, Params, W> &a) {
             T value = a.get();
             return static_cast<float>(value);
           })
      .def("__str__",
           [name](const BaseNumber<T, Params, W> &a) {
             return std::to_string(a.get());
           })
      .def("__repr__", [name](const BaseNumber<T, Params, W> &a) {
        return "<c104." + name + " value=" + std::to_string(a.get()) + ">";
      });
}

template <typename T>
void bind_BitFlags_ops(py::class_<T> &py_bit_enum,
                       std::string (*fn)(const T &)) {
  py_bit_enum
      .def(
          "__and__", [](const T &a, T b) { return a & b; }, py::is_operator())
      .def(
          "__rand__", [](const T &a, T b) { return a & b; }, py::is_operator())
      .def(
          "__or__", [](const T &a, T b) { return a | b; }, py::is_operator())
      .def(
          "__ror__", [](const T &a, T b) { return a | b; }, py::is_operator())
      .def(
          "__xor__", [](const T &a, T b) { return a ^ b; }, py::is_operator())
      .def(
          "__rxor__", [](const T &a, T b) { return a ^ b; }, py::is_operator())
      .def(
          "__invert__", [](const T &a) { return ~a; }, py::is_operator())
      .def(
          "__iand__",
          [](T &a, T b) {
            a &= b;
            return a;
          },
          py::is_operator())
      .def(
          "__ior__",
          [](T &a, T b) {
            a |= b;
            return a;
          },
          py::is_operator())
      .def(
          "__ixor__",
          [](T &a, T b) {
            a ^= b;
            return a;
          },
          py::is_operator())
      .def(
          "__contains__",
          [](const T &mode, const T &flag) { return test(mode, flag); },
          py::is_operator())
      .def(
          "is_any", [](const T &mode) { return is_any(mode); },
          "test if there are any flags set")
      .def(
          "is_good", [](const T &mode) { return is_none(mode); },
          "test if there are no flags set");

  py_bit_enum.attr("__str__") =
      py::cpp_function(fn, py::name("__str__"), py::is_method(py_bit_enum));
  py_bit_enum.attr("__repr__") =
      py::cpp_function(fn, py::name("__repr__"), py::is_method(py_bit_enum));
}

py::bytes
IncomingMessage_getRawBytes(Remote::Message::IncomingMessage *message) {
  unsigned char *msg = message->getRawBytes();
  unsigned char msgSize = 2 + msg[1];

  return {reinterpret_cast<const char *>(msg), msgSize};
}

std::string explain_bytes(const py::bytes &obj) {

  // PyObject * pymemview = PyMemoryView_FromObject(obj);
  // Py_buffer *buffer = PyMemoryView_GET_BUFFER(pymemview);
  py::memoryview memview(obj);
  Py_buffer *buffer = PyMemoryView_GET_BUFFER(memview.ptr());

  return Remote::rawMessageFormatter((unsigned char *)buffer->buf,
                                     (unsigned char)buffer->len);
}

py::dict explain_bytes_dict(const py::bytes &obj) {

  // PyObject * pymemview = PyMemoryView_FromObject(obj);
  // Py_buffer *buffer = PyMemoryView_GET_BUFFER(pymemview);
  py::memoryview memview(obj);
  Py_buffer *buffer = PyMemoryView_GET_BUFFER(memview.ptr());

  return Remote::rawMessageDictionaryFormatter((unsigned char *)buffer->buf,
                                               (unsigned char)buffer->len);
}

py::object convert_timestamp_to_datetime(const uint64_t timestamp_ms) {
  // Convert milliseconds to seconds and microseconds
  std::time_t seconds = timestamp_ms / 1000;
  std::size_t milliseconds = timestamp_ms % 1000;

  // Convert the time_t to tm structure
  std::tm *time_info = std::gmtime(&seconds);

  // Import datetime module
  py::module_ datetime = py::module_::import("datetime");
  py::object datetime_class = datetime.attr("datetime");

  // Create datetime object
  return datetime_class(
      time_info->tm_year + 1900, // Year
      time_info->tm_mon + 1,     // Month (tm_mon is in range [0, 11])
      time_info->tm_mday,        // Day
      time_info->tm_hour,        // Hour
      time_info->tm_min,         // Minute
      time_info->tm_sec,         // Second
      milliseconds * 1000        // Microsecond
  );
}

PY_MODULE(c104, m) {
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
  bind_BitFlags_ops(py_quality, &Quality_toString);

  auto py_bcrquality =
      py::enum_<BinaryCounterQuality>(
          m, "BinaryCounterQuality",
          "This enum contains all binary counter quality issue bits to "
          "interpret and manipulate counter quality.")
          .value("Adjusted", BinaryCounterQuality::Adjusted)
          .value("Carry", BinaryCounterQuality::Carry)
          .value("Invalid", BinaryCounterQuality::Invalid)
          .def(py::init([]() { return BinaryCounterQuality::None; }));
  bind_BitFlags_ops(py_bcrquality, &BinaryCounterQuality_toString);

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

  bind_BaseNumber<uint8_t, NumberParams<uint8_t>, uint32_t>(m, "UInt5");
  bind_BaseNumber<uint8_t, NumberParamsAlt<uint8_t>, uint32_t>(m, "UInt7");
  bind_BaseNumber<uint16_t, NumberParams<uint16_t>, uint32_t>(m, "UInt16");
  bind_BaseNumber<int8_t, NumberParams<int8_t>, int32_t>(m, "Int7");
  bind_BaseNumber<int16_t, NumberParams<int16_t>, int32_t>(m, "Int16");
  bind_BaseNumber<float, NumberParams<float>, double>(m, "NormalizedFloat");

  py::class_<Byte32>(m, "Byte32")
      .def(py::init<uint32_t>())
      .def(py::init([](const py::bytes &byte_obj) {
        py::buffer_info info(py::buffer(byte_obj).request());

        if (info.size > sizeof(uint32_t)) {
          throw std::runtime_error(
              "Invalid size of bytes object. Expected 4 bytes, got " +
              std::to_string(info.size) + ".");
        }
        uint32_t value = 0;

        // Copy only the available bytes
        std::memcpy(&value, info.ptr, info.size);

        return Byte32(value);
      }))
      .def("__bytes__",
           [](const Byte32 &b) {
             uint32_t value = b.get();
             return py::bytes(reinterpret_cast<const char *>(&value),
                              sizeof(value));
           })
      .def("__str__", &Byte32_toString)
      .def("__repr__", [](const Byte32 &b) {
        return "<Byte32 value=" + Byte32_toString(b) + ">";
      });

  py::class_<Remote::TransportSecurity,
             std::shared_ptr<Remote::TransportSecurity>>(
      m, "TransportSecurity",
      "This class is used to configure transport layer security for server and "
      "clients")
      .def(py::init(&Remote::TransportSecurity::create), R"def(
    __init__(self: c104.TransportSecurity, validate: bool = True, only_known: bool = True) -> None

    Create a new transport layer configuration

    Parameters
    ----------
    validate: bool
        validate certificates of communication partners
    only_known: bool
        accept communication only from partners with certificate added to the list of allowed remote certificates

    Example
    -------
    >>> tls = c104.TransportSecurity(validate=True, only_known=False)
)def",
           "validate"_a = true, "only_known"_a = true)
      .def("set_certificate", &Remote::TransportSecurity::setCertificate, R"def(
    set_certificate(self: c104.TransportSecurity, cert: str, key: str, passphrase: str = "") -> None

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
        If loading the certificate file, loading the private key file or decrypting the private key fails

    Example
    -------
    >>> tls = c104.TransportSecurity(validate=True, only_known=False)
    >>> tls.set_certificate(cert="certs/server.crt", key="certs/server.key")
)def",
           "cert"_a, "key"_a, "passphrase"_a = "")
      .def("set_ca_certificate", &Remote::TransportSecurity::setCACertificate,
           R"def(
    set_ca_certificate(self: c104.TransportSecurity, cert: str) -> None

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
        If loading the certificate file fails

    Example
    -------
    >>> tls = c104.TransportSecurity(validate=True, only_known=False)
    >>> tls.set_ca_certificate(cert="certs/ca.crt")
)def",
           "cert"_a)
      .def("add_allowed_remote_certificate",
           &Remote::TransportSecurity::addAllowedRemoteCertificate, R"def(
    add_allowed_remote_certificate(self: c104.TransportSecurity, cert: str) -> None

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
        If loading the certificate file fails

    Example
    -------
    >>> tls = c104.TransportSecurity(validate=True, only_known=False)
    >>> tls.add_allowed_remote_certificate(cert="certs/client2.crt")
)def",
           "cert"_a)
      .def("set_version", &Remote::TransportSecurity::setVersion, R"def(
    set_version(self: c104.TransportSecurity, min: c104.TlsVersion = c104.TlsVersion.NOT_SELECTED, max: c104.TlsVersion = c104.TlsVersion.NOT_SELECTED) -> None

    set the supported min and/or max TLS version

    Parameters
    ----------
    min: :ref:`c104.TlsVersion`
        minimum required TLS version for communication
    max: :ref:`c104.TlsVersion`
        maximum allowed TLS version for communication

    Returns
    -------
    None

    Example
    -------
    >>> tls = c104.TransportSecurity(validate=True, only_known=False)
    >>> tls.set_version(min=c104.TLSVersion.TLS_1_2, max=c104.TLSVersion.TLS_1_2)
)def",
           "min"_a = TLS_VERSION_NOT_SELECTED,
           "max"_a = TLS_VERSION_NOT_SELECTED)
      .def("__repr__", &Remote::TransportSecurity::toString);

  m.def("explain_bytes", &explain_bytes, R"def(
    explain_bytes(apdu: bytes) -> str

    Interpret 104er APDU bytes and convert it into a human readable interpretation

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
        "explain bytes in a string", "apdu"_a, py::return_value_policy::copy);

  m.def("explain_bytes_dict", &explain_bytes_dict, R"def(
    explain_bytes(apdu: bytes) -> str

    Interpret 104er APDU bytes and extract information into a dictionary

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
    >>>    pprint("SV] -->| {1} [{0}] | SERVER {2}:{3}".format(data.hex(), c104.explain_bytes_dict(apdu=data), server.ip, server.port))
)def",
        "apdu"_a, py::return_value_policy::copy);

  m.def("set_debug_mode", &setDebug, R"def(
    set_debug_mode(mode: c104.Debug) -> None

    set the debug mode

    Parameters
    ----------
    mode: :ref:`c104.Debug`
        debug mode bitset

    Example
    -------
    >>> c104.set_debug_mode(mode=c104.Debug.Client|c104.Debug.Connection)
)def",
        "mode"_a);
  m.def("get_debug_mode", &getDebug, R"def(
    get_debug_mode() -> c104.Debug

    get current debug mode

    Returns
    ----------
    :ref:`c104.Debug`
        debug mode bitset

    Example
    -------
    >>> mode = c104.get_debug_mode()
)def",
        py::return_value_policy::copy);
  m.def("enable_debug", &enableDebug, R"def(
    enable_debug(mode: c104.Debug) -> None

    enable additional debugging modes

    Parameters
    ----------
    mode: :ref:`c104.Debug`
        debug mode bitset

    Example
    -------
    >>> c104.set_debug_mode(mode=c104.Debug.Client|c104.Debug.Connection)
    >>> c104.enable_debug(mode=c104.Debug.Callback|c104.Debug.Gil)
    >>> c104.get_debug_mode() == c104.Debug.Client|c104.Debug.Connection|c104.Debug.Callback|c104.Debug.Gil
)def",
        "mode"_a);
  m.def("disable_debug", &disableDebug, R"def(
    disable_debug(mode: c104.Debug) -> None

    disable debugging modes

    Parameters
    ----------
    mode: :ref:`c104.Debug`
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
      .def(py::init(&Client::create), R"def(
    __init__(self: c104.Client, tick_rate_ms: int = 100, command_timeout_ms: int = 100, transport_security: Optional[c104.TransportSecurity] = None) -> None

    create a new 104er client

    Parameters
    ----------
    tick_rate_ms: int
        client thread update interval
    command_timeout_ms: int
        time to wait for a command response
    transport_security: :ref:`c104.TransportSecurity`
        TLS configuration object

    Example
    -------
    >>> my_client = c104.Client(tick_rate_ms=100, command_timeout_ms=100)
)def",
           "tick_rate_ms"_a = 100, "command_timeout_ms"_a = 100,
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
      .def_property_readonly(
          "open_connection_count", &Client::getOpenConnectionCount,
          "int: get number of open connections to servers (read-only)")
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
          "List[:ref:`c104.Connection`]: list of all remote terminal unit "
          "(server) Connection objects (read-only)")
      .def_property("originator_address", &Client::getOriginatorAddress,
                    &Client::setOriginatorAddress,
                    "int: primary originator address of this client (0-255)",
                    py::return_value_policy::copy)
      .def("start", &Client::start, R"def(
    start(self: c104.Client) -> None

    start client and connect all connections

    Example
    -------
    >>> my_client.start()
)def")
      .def("stop", &Client::stop, R"def(
    stop(self: c104.Client) -> None

    disconnect all connections and stop client

    Example
    -------
    >>> my_client.stop()
)def")
      .def("add_connection", &Client::addConnection, R"def(
    add_connection(self: c104.Client, ip: str, port: int = 2404, init = c104.Init.ALL) -> Optional[c104.Connection]

    add a new remote server connection to this client and return the new connection object

    Parameters
    ----------
    ip: str
        remote terminal units ip address
    port: int
        remote terminal units port
    init: :ref:`c104.Init`
        communication initiation commands

    Returns
    -------
    :ref:`c104.Connection`
        connection object, if added, else None

    Raises
    ------
    ValueError
        If ip or port are invalid

    Example
    -------
    >>> con = my_client.add_connection(ip="192.168.50.3", port=2406, init=c104.Init.ALL)
)def",
           "ip"_a, "port"_a = IEC_60870_5_104_DEFAULT_PORT, "init"_a = INIT_ALL)
      .def("get_connection", &Client::getConnection, R"def(
    get_connection(self: c104.Client, ip: str, port: int = 2404) -> Optional[c104.Connection]

    get a connection by ip and port

    Parameters
    ----------
    ip: str
        remote terminal units ip address
    port: int
        remote terminal units port

    Returns
    -------
    :ref:`c104.Connection`
        connection object, if found else None

    Example
    -------
    >>> con = my_client.get_connection(ip="192.168.50.3", port=2406)
)def",
           "ip"_a, "port"_a = IEC_60870_5_104_DEFAULT_PORT)
      .def("get_connection", &Client::getConnectionFromCommonAddress, R"def(
    get_connection(self: c104.Client, common_address: int) -> Optional[c104.Connection]

    get a connection by common_address

    Parameters
    ----------
    common_address: int
        common address (value between 1 and 65534)

    Returns
    -------
    :ref:`c104.Connection`
        connection object, if found else None

    Example
    -------
    >>> con = my_client.get_connection(common_address=4711)
)def",
           "common_address"_a)
      .def("reconnect_all", &Client::reconnectAll, R"def(
    reconnect_all(self: c104.Client) -> None

    close and reopen all connections

    Example
    -------
    >>> my_client.reconnect_all()
)def")
      .def("disconnect_all", &Client::disconnectAll, R"def(
    disconnect_all(self: c104.Client) -> None

    close all connections

    Example
    -------
    >>> my_client.disconnect_all()
)def")
      .def("on_new_station", &Client::setOnNewStationCallback, R"def(
    on_new_station(self: c104.Client, callable: Callable[[c104.Client, c104.Connection, int], None]) -> None

    set python callback that will be executed on incoming message from unknown station

    **Callable signature**

    Parameters
    ----------
    client: :ref:`c104.Client`
        client instance
    connection: :ref:`c104.Connection`
        connection reporting station
    common_address: int
        station common address (value between 1 and 65534)

    Returns
    -------
    None

    Raises
    ------
    ValueError
        If callable signature does not match exactly

    Example
    -------
    >>> def cl_on_new_station(client: c104.Client, connection: c104.Connection, common_address: int) -> None:
    >>>     print("NEW STATION {0} | CLIENT OA {1}".format(common_address, client.originator_address))
    >>>     connection.add_station(common_address=common_address)
    >>>
    >>> my_client.on_new_station(callable=cl_on_new_station)
)def",
           "callable"_a)
      .def("on_new_point", &Client::setOnNewPointCallback, R"def(
    on_new_point(self: c104.Client, callable: Callable[[c104.Client, c104.Station, int, c104.Type], None]) -> None

    set python callback that will be executed on incoming message from unknown point

    **Callable signature**

    Parameters
    ----------
    client: :ref:`c104.Client`
        client instance
    station: :ref:`c104.Station`
        station reporting point
    io_address: int
        point information object address (value between 0 and 16777215)
    point_type: :ref:`c104.Type`
        point information type

    Returns
    -------
    None

    Raises
    ------
    ValueError
        If callable signature does not match exactly

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
      .def(py::init(&Server::create), R"def(
    __init__(self: c104.Server, ip: str = "0.0.0.0", port: int = 2404, tick_rate_ms: int = 100, select_timeout_ms = 100, max_connections: int = 0, transport_security: Optional[c104.TransportSecurity] = None) -> None

    create a new 104er server

    Parameters
    -------
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
    transport_security: :ref:`c104.TransportSecurity`
        TLS configuration object

    Example
    -------
    >>> my_server = c104.Server(ip="0.0.0.0", port=2404, tick_rate_ms=100, select_timeout_ms=100, max_connections=0)
)def",
           "ip"_a = "0.0.0.0", "port"_a = IEC_60870_5_104_DEFAULT_PORT,
           "tick_rate_ms"_a = 100, "select_timeout_ms"_a = 100,
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
      .def_property_readonly(
          "open_connection_count", &Server::getOpenConnectionCount,
          "int: get number of open connections to clients (read-only)")
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
                             "List[:ref:`c104.Station`]: list of all local "
                             "Station objects (read-only)")
      .def_property("max_connections", &Server::getMaxOpenConnections,
                    &Server::setMaxOpenConnections,
                    "int: maximum number of open connections, 0 = no limit",
                    py::return_value_policy::copy)
      .def("start", &Server::start, R"def(
    start(self: c104.Server) -> None

    open local server socket for incoming connections

    Raises
    ------
    RuntimeError
        If server thread failed to start

    Example
    -------
    >>> my_server.start()
)def")
      .def("stop", &Server::stop, R"def(
    stop(self: c104.Server) -> None

    stop local server socket

    Example
    -------
    >>> my_server.stop()
)def")
      .def("add_station", &Server::addStation, R"def(
    add_station(self: c104.Server, common_address: int) -> Optional[c104.Station]

    add a new station to this server and return the new station object

    Parameters
    ----------
    common_address: int
        station common address (value between 1 and 65534)

    Returns
    -------
    :ref:`c104.Station`
        station object, if station was added, else None

    Example
    -------
    >>> station_1 = my_server.add_station(common_address=15)
)def",
           "common_address"_a)
      .def("get_station", &Server::getStation, R"def(
    get_station(self: c104.Server, common_address: int) -> Optional[c104.Station]

    get a station object via common address

    Parameters
    ----------
    common_address: int
        station common address (value between 1 and 65534)

    Returns
    -------
    :ref:`c104.Station`
        station object, if found, else None

    Example
    -------
    >>> station_2 = my_server.get_connection(common_address=14)
)def",
           "common_address"_a)
      .def("on_receive_raw", &Server::setOnReceiveRawCallback, R"def(
    on_receive_raw(self: c104.Server, callable: Callable[[c104.Server, bytes], None]) -> None

    set python callback that will be executed on incoming message

    **Callable signature**

    Parameters
    ----------
    server: :ref:`c104.Server`
        server instance
    data: bytes
        raw message bytes

    Returns
    -------
    None

    Raises
    ------
    ValueError
        If callable signature does not match exactly

    Example
    -------
    >>> def sv_on_receive_raw(server: c104.Server, data: bytes) -> None:
    >>>     print("-->| {1} [{0}] | SERVER {2}:{3}".format(data.hex(), c104.explain_bytes(apdu=data), server.ip, server.port))
    >>>
    >>> my_server.on_receive_raw(callable=sv_on_receive_raw)
)def",
           "callable"_a)
      .def("on_send_raw", &Server::setOnSendRawCallback, R"def(
    on_send_raw(self: c104.Server, callable: Callable[[c104.Server, bytes], None]) -> None

    set python callback that will be executed on outgoing message

    **Callable signature**

    Parameters
    ----------
    server: :ref:`c104.Server`
        server instance
    data: bytes
        raw message bytes

    Returns
    -------
    None

    Raises
    ------
    ValueError
        If callable signature does not match exactly

    Example
    -------
    >>> def sv_on_send_raw(server: c104.Server, data: bytes) -> None:
    >>>     print("<--| {1} [{0}] | SERVER {2}:{3}".format(data.hex(), c104.explain_bytes(apdu=data), server.ip, server.port))
    >>>
    >>> my_server.on_send_raw(callable=sv_on_send_raw)
)def",
           "callable"_a)
      .def("on_connect", &Server::setOnConnectCallback, R"def(
    on_connect(self: c104.Server, callable: Callable[[c104.Server, ip], bool]) -> None

    set python callback that will be executed on incoming connection requests

    **Callable signature**

    Parameters
    ----------
    server: :ref:`c104.Server`
        server instance
    ip: str
        client connection request ip

    Returns
    -------
    bool
        accept or reject the connection request

    Raises
    ------
    ValueError
        If callable signature does not match exactly

    Example
    -------
    >>> def sv_on_connect(server: c104.Server, ip: str) -> bool:
    >>>     print("<->| {0} | SERVER {1}:{2}".format(ip, server.ip, server.port))
    >>>     return ip == "127.0.0.1"
    >>>
    >>> my_server.on_connect(callable=sv_on_connect)
)def",
           "callable"_a)
      .def("on_clock_sync", &Server::setOnClockSyncCallback, R"def(
    on_clock_sync(self: c104.Server, callable: Callable[[c104.Server, str, datetime.datetime], c104.ResponseState]) -> None

    set python callback that will be executed on incoming clock sync command

    **Callable signature**

    Parameters
    ----------
    server: :ref:`c104.Server`
        server instance
    ip: str
        client connection request ip
    date_time: datetime.datetime
        clients current clock time

    Returns
    -------
    :ref:`c104.ResponseState`
        success or failure of clock sync command

    Raises
    ------
    ValueError
        If callable signature does not match exactly

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
      .def("on_unexpected_message", &Server::setOnUnexpectedMessageCallback,
           R"def(
    on_unexpected_message(self: c104.Server, callable: Callable[[c104.Server, c104.IncomingMessage, c104.Umc], None]) -> None

    set python callback that will be executed on unexpected incoming messages

    **Callable signature**

    Parameters
    ----------
    server: :ref:`c104.Server`
        server instance
    message: :ref:`c104.IncomingMessage`
        incoming message
    cause: :ref:`c104.Umc`
        unexpected message cause

    Returns
    -------
    None

    Raises
    ------
    ValueError
        If callable signature does not match exactly

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
          ":ref:`c104.ConnectionState`: current connection state (read-only)")
      .def_property_readonly(
          "has_stations", &Remote::Connection::hasStations,
          "bool: test if remote server has at least one station (read-only)")
      .def_property_readonly(
          "stations", &Remote::Connection::getStations,
          "List[:ref:`c104.Station`] list of all Station objects (read-only)")
      .def_property_readonly("is_connected", &Remote::Connection::isOpen,
                             "bool: test if connection is opened (read-only)")
      .def_property_readonly("is_muted", &Remote::Connection::isMuted,
                             "bool: test if connection is muted (read-only)")
      .def_property(
          "originator_address", &Remote::Connection::getOriginatorAddress,
          &Remote::Connection::setOriginatorAddress,
          "int: primary originator address of this connection (0-255)")
      .def_property_readonly("connected_at",
                             &Remote::Connection::getConnectedAt,
                             "Optional[datetime.datetime]: datetime of "
                             "disconnect, if connection is closed (read-only)")
      .def_property_readonly("disconnected_at",
                             &Remote::Connection::getDisconnectedAt,
                             "Optional[datetime.datetime]: test if connection "
                             "is muted (read-only)")
      .def("connect", &Remote::Connection::connect, R"def(
    connect(self: c104.Connection) -> None

    initiate connection to remote terminal unit (server) in a background thread (non-blocking)

    Example
    -------
    >>> my_connection.connect()
)def")
      .def("disconnect", &Remote::Connection::disconnect, R"def(
    disconnect(self: c104.Connection) -> None

    close connection to remote terminal unit (server)

    Example
    -------
    >>> my_connection.disconnect()
)def")
      .def("mute", &Remote::Connection::mute, R"def(
    mute(self: c104.Connection) -> bool

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
      .def("unmute", &Remote::Connection::unmute, R"def(
    mute(self: c104.Connection) -> bool

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
      .def("interrogation", &Remote::Connection::interrogation, R"def(
    interrogation(self: c104.Connection, common_address: int, cause: c104.Cot = c104.Cot.ACTIVATION, qualifier: c104.Qoi = c104.Qoi.STATION, wait_for_response: bool = True) -> bool

    send an interrogation command to the remote terminal unit (server)

    Parameters
    ----------
    common_address: int
        station common address (The valid range is 0 to 65535. Using the values 0 or 65535 sends the command to all stations, acting as a wildcard.)
    cause: :ref:`c104.Cot`
        cause of transmission
    qualifier: :ref:`c104.Qoi`
        qualifier of interrogation
    wait_for_response: bool
        block call until command success or failure reponse received?

    Returns
    -------
    bool
        True, if connection is Open, False otherwise

    Raises
    ------
    ValueError
        If qualifier is invalid

    Example
    -------
    >>> if not my_connection.interrogation(common_address=47, cause=c104.Cot.ACTIVATION, qualifier=c104.Qoi.STATION):
    >>>     raise ValueError("Cannot send interrogation command")
)def",
           "common_address"_a, "cause"_a = CS101_COT_ACTIVATION,
           "qualifier"_a = QOI_STATION, "wait_for_response"_a = true,
           py::return_value_policy::copy)
      .def("counter_interrogation", &Remote::Connection::counterInterrogation,
           R"def(
    counter_interrogation(self: c104.Connection, common_address: int, cause: c104.Cot = c104.Cot.ACTIVATION, qualifier: c104.Qoi = c104.Qoi.STATION, wait_for_response: bool = True) -> bool

    send a counter interrogation command to the remote terminal unit (server)

    Parameters
    ----------
    common_address: int
        station common address (The valid range is 0 to 65535. Using the values 0 or 65535 sends the command to all stations, acting as a wildcard.)
    cause: :ref:`c104.Cot`
        cause of transmission
    qualifier: :ref:`c104.Qoi`
        qualifier of interrogation
    wait_for_response: bool
        block call until command success or failure reponse received?

    Returns
    -------
    bool
        True, if connection is Open, False otherwise

    Raises
    ------
    ValueError
        If qualifier is invalid

    Example
    -------
    >>> if not my_connection.counter_interrogation(common_address=47, cause=c104.Cot.ACTIVATION, qualifier=c104.Qoi.STATION):
    >>>     raise ValueError("Cannot send counter interrogation command")
)def",
           "common_address"_a, "cause"_a = CS101_COT_ACTIVATION,
           "qualifier"_a = QOI_STATION, "wait_for_response"_a = true,
           py::return_value_policy::copy)
      .def("clock_sync", &Remote::Connection::clockSync, R"def(
    clock_sync(self: c104.Connection, common_address: int, wait_for_response: bool = True) -> bool

    send a clock synchronization command to the remote terminal unit (server)
    the clients OS time is used

    Parameters
    ----------
    common_address: int
        station common address (The valid range is 0 to 65535. Using the values 0 or 65535 sends the command to all stations, acting as a wildcard.)
    wait_for_response: bool
        block call until command success or failure reponse received?

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
      .def("test", &Remote::Connection::test, R"def(
    test(self: c104.Connection, common_address: int, with_time: bool = True, wait_for_response: bool = True) -> bool

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
      .def("get_station", &Remote::Connection::getStation, R"def(
    get_station(self: c104.Connection, common_address: int) -> Optional[c104.Station]

    get a station object via common address

    Parameters
    ----------
    common_address: int
        station common address (value between 1 and 65534)

    Returns
    -------
    :ref:`c104.Station`
        station object, if found, else None

    Example
    -------
    >>> station_14 = my_connection.get_station(common_address=14)
)def",
           "common_address"_a)
      .def("add_station", &Remote::Connection::addStation, R"def(
    add_station(self: c104.Connection, common_address: int) -> Optional[c104.Station]

    add a new station to this connection and return the new station object

    Parameters
    ----------
    common_address: int
        station common address (value between 1 and 65534)

    Returns
    -------
    :ref:`c104.Station`
        station object, if station was added, else None

    Example
    -------
    >>> station = my_connection.add_station(common_address=15)
)def",
           "common_address"_a)
      .def("on_receive_raw", &Remote::Connection::setOnReceiveRawCallback,
           R"def(
    on_receive_raw(self: c104.Connection, callable: Callable[[c104.Connection, bytes], None]) -> None

    set python callback that will be executed on incoming message

    **Callable signature**

    Parameters
    ----------
    connection: :ref:`c104.Connection`
        connection instance
    data: bytes
        raw message bytes

    Returns
    -------
    None

    Raises
    ------
    ValueError
        If callable signature does not match exactly

    Example
    -------
    >>> def con_on_receive_raw(connection: c104.Connection, data: bytes) -> None:
    >>>     print("-->| {1} [{0}] | CON {2}:{3}".format(data.hex(), c104.explain_bytes(apdu=data), connection.ip, connection.port))
    >>>
    >>> my_connection.on_receive_raw(callable=con_on_receive_raw)
)def",
           "callable"_a)
      .def("on_send_raw", &Remote::Connection::setOnSendRawCallback, R"def(
    on_send_raw(self: c104.Connection, callable: Callable[[c104.Connection, bytes], None]) -> None

    set python callback that will be executed on outgoing message

    **Callable signature**

    Parameters
    ----------
    connection: :ref:`c104.Connection`
        connection instance
    data: bytes
        raw message bytes

    Returns
    -------
    None

    Raises
    ------
    ValueError
        If callable signature does not match exactly

    Example
    -------
    >>> def con_on_send_raw(connection: c104.Connection, data: bytes) -> None:
    >>>     print("<--| {1} [{0}] | CON {2}:{3}".format(data.hex(), c104.explain_bytes(apdu=data), connection.ip, connection.port))
    >>>
    >>> my_connection.on_send_raw(callable=con_on_send_raw)
)def",
           "callable"_a)
      .def("on_state_change", &Remote::Connection::setOnStateChangeCallback,
           R"def(
    on_state_change(self: c104.Connection, callable: Callable[[c104.Connection, c104.ConnectionState], None]) -> None

    set python callback that will be executed on connection state changes

    **Callable signature**

    Parameters
    ----------
    connection: :ref:`c104.Connection`
        connection instance
    state: :ref:`c104.ConnectionState`
        latest connection state

    Returns
    -------
    None

    Raises
    ------
    ValueError
        If callable signature does not match exactly

    Example
    -------
    >>> def con_on_state_change(connection: c104.Connection, state: c104.ConnectionState) -> None:
    >>>     print("CON {0}:{1} STATE changed to {2}".format(connection.ip, connection.port, state))
    >>>
    >>> my_connection.on_state_change(callable=con_on_state_change)
)def",
           "callable"_a)
      .def("__repr__", &Remote::Connection::toString);
  ;

  py::class_<Object::Station, std::shared_ptr<Object::Station>>(
      m, "Station",
      "This class represents local or remote stations and provides access to "
      "meta information and containing points")
      .def_property_readonly("server", &Object::Station::getServer,
                             "Optional[:ref:`c104.Server`]: parent Server of "
                             "local station (read-only)")
      .def_property_readonly("connection", &Object::Station::getConnection,
                             "Optional[:ref:`c104.Connection`]: parent "
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
          "List[:ref:`c104.Point`] list of all Point objects (read-only)")
      .def("get_point", &Object::Station::getPoint, R"def(
    get_point(self: c104.Station, io_address: int) -> Optional[c104.Point]

    get a point object via information object address

    Parameters
    ----------
    io_address: int
        point information object address (value between 0 and 16777215)

    Returns
    -------
    :ref:`c104.Point`
        point object, if found, else None

    Example
    -------
    >>> point_11 = my_station.get_point(io_address=11)
)def",
           "io_address"_a)
      .def("add_point", &Object::Station::addPoint, R"def(
    add_point(self: c104.Station, io_address: int, type: c104.Type, report_ms: int = 0, related_io_address: Optional[int] = None, related_io_autoreturn: bool = False, command_mode: c104.CommandMode = c104.CommandMode.DIRECT) -> Optional[c104.Point]

    add a new point to this station and return the new point object

    Parameters
    ----------
    io_address: int
        point information object address (value between 0 and 16777215)
    type: :ref:`c104.Type`
        point information type
    report_ms: int
        automatic reporting interval in milliseconds (monitoring points server-sided only), 0 = disabled
    related_io_address: Optional[int]
        related monitoring point identified by information object address, that should be auto transmitted on incoming client command (for control points server-sided only)
    related_io_autoreturn: bool
        automatic reporting interval in milliseconds (for control points server-sided only)
    command_mode: :ref:`c104.CommandMode`
        command transmission mode (direct or select-and-execute)

    Returns
    -------
    :ref:`c104.Station`
        station object, if station was added, else None

    Raises
    ------
    ValueError
        If io_address or type is invalid
    ValueError
        If report_ms, related_io_address or related_auto_return is set, but type is not a monitoring type
    ValueError
        If related_auto_return is set, but related_io_address is not set
    ValueError
        If related_auto_return is set, but type is not a control type

    Example
    -------
    >>> point_1 = sv_station_1.add_point(common_address=15, type=c104.Type.M_ME_NC_1)
    >>> point_2 = sv_station_1.add_point(io_address=11, type=c104.Type.M_ME_NC_1, report_ms=1000)
    >>> point_3 = sv_station_1.add_point(io_address=12, type=c104.Type.C_SE_NC_1, report_ms=0, related_io_address=point_2.io_address, related_io_autoreturn=True, command_mode=c104.CommandMode.SELECT_AND_EXECUTE)
)def",
           "io_address"_a, "type"_a, "report_ms"_a = 0,
           "related_io_address"_a = std::nullopt,
           "related_io_autoreturn"_a = false, "command_mode"_a = DIRECT_COMMAND)
      .def("__repr__", &Object::Station::toString);

  py::class_<Object::DataPoint, std::shared_ptr<Object::DataPoint>>(
      m, "Point",
      "This class represents command and measurement data point of a station "
      "and provides access to structured properties of points")
      .def_property_readonly(
          "station", &Object::DataPoint::getStation,
          "Optional[:ref:`c104.Station`]: parent Station object (read-only)")
      .def_property_readonly("io_address",
                             &Object::DataPoint::getInformationObjectAddress,
                             "int: information object address (read-only)")
      .def_property_readonly("type", &Object::DataPoint::getType,
                             ":ref:`c104.Type`: iec60870 data Type (read-only)")
      .def_property(
          "related_io_address",
          &Object::DataPoint::getRelatedInformationObjectAddress,
          &Object::DataPoint::setRelatedInformationObjectAddress,
          "Optional[int]: io_address of a related monitoring point or None")
      .def_property(
          "related_io_autoreturn",
          &Object::DataPoint::getRelatedInformationObjectAutoReturn,
          &Object::DataPoint::setRelatedInformationObjectAutoReturn,
          "bool: toggle automatic return info remote response on or off")
      .def_property("command_mode", &Object::DataPoint::getCommandMode,
                    &Object::DataPoint::setCommandMode,
                    "c104.CommandMode: set direct or select-and-execute "
                    "command transmission mode",
                    py::return_value_policy::copy)
      .def_property_readonly(
          "selected_by", &Object::DataPoint::getSelectedByOriginatorAddress,
          "Optional[int]: originator address (0-255) or None")
      .def_property("report_ms", &Object::DataPoint::getReportInterval_ms,
                    &Object::DataPoint::setReportInterval_ms,
                    "int: interval in milliseconds between periodic "
                    "transmission, 0 = no periodic transmission")
      .def_property_readonly("timer_ms",
                             &Object::DataPoint::getTimerInterval_ms,
                             "int: interval in milliseconds between timer "
                             "callbacks, 0 = no periodic transmission")
      .def_property("info", &Object::DataPoint::getInfo,
                    &Object::DataPoint::setInfo,
                    "ref:`c104.Information`: information object",
                    py::return_value_policy::automatic)
      .def_property("value", &Object::DataPoint::getValue,
                    &Object::DataPoint::setValue,
                    "Any: value (this is just a shortcut to point.info.value)",
                    py::return_value_policy::copy)
      .def_property("quality", &Object::DataPoint::getQuality,
                    &Object::DataPoint::setQuality,
                    "Any: Quality info object (this is just a shortcut to "
                    "point.info.quality)",
                    py::return_value_policy::copy)
      .def_property_readonly(
          "processed_at", &Object::DataPoint::getProcessedAt,
          "datetime.datetime: timestamp with milliseconds of last local "
          "information processing "
          "(read-only)",
          py::return_value_policy::copy)
      .def_property_readonly(
          "recorded_at", &Object::DataPoint::getRecordedAt,
          "Optional[int]: timestamp with milliseconds transported with the "
          "value "
          "itself or None (read-only)",
          py::return_value_policy::copy)
      .def("on_receive", &Object::DataPoint::setOnReceiveCallback, R"def(
    on_receive(self: c104.Point, callable: Callable[[c104.Point, dict, c104.IncomingMessage], c104.ResponseState]) -> None

    set python callback that will be executed on every incoming message
    this can be either a command or an monitoring message

    **Callable signature**

    Parameters
    ----------
    point: :ref:`c104.Point`
        point instance
    previous_info: :ref:`c104.Information`
        Information object containing the state of the point before the command took effect
    message: :ref:`c104.IncomingMessage`
        new command message

    Returns
    -------
    :ref:`c104.ResponseState`
        send command SUCCESS or FAILURE response

    Raises
    ------
    ValueError
        If callable signature does not match exactly

    Example
    -------
    >>> def on_setpoint_command(point: c104.Point, previous_info: c104.Information, message: c104.IncomingMessage) -> c104.ResponseState:
    >>>     print("SV] {0} SETPOINT COMMAND on IOA: {1}, new: {2}, prev: {3}, cot: {4}, quality: {5}".format(point.type, point.io_address, point.value, previous_info, message.cot, point.quality))
    >>>     if point.quality.is_good():
    >>>         if point.related_io_address:
    >>>             print("SV] -> RELATED IO ADDRESS: {}".format(point.related_io_address))
    >>>             related_point = sv_station_2.get_point(point.related_io_address)
    >>>             if related_point:
    >>>                 print("SV] -> RELATED POINT VALUE UPDATE")
    >>>                 related_point.value = point.value
    >>>             else:
    >>>                 print("SV] -> RELATED POINT NOT FOUND!")
    >>>         return c104.ResponseState.SUCCESS
    >>>     return c104.ResponseState.FAILURE
    >>>
    >>> sv_measurement_point = sv_station_2.add_point(io_address=11, type=c104.Type.M_ME_NC_1, report_ms=1000)
    >>> sv_measurement_point.value = 12.34
    >>> sv_command_point = sv_station_2.add_point(io_address=12, type=c104.Type.C_SE_NC_1, report_ms=0, related_io_address=sv_measurement_point.io_address, related_io_autoreturn=True, command_mode=c104.CommandMode.SELECT_AND_EXECUTE)
    >>> sv_command_point.on_receive(callable=on_setpoint_command)
)def",
           "callable"_a)
      .def("on_before_read", &Object::DataPoint::setOnBeforeReadCallback, R"def(
    on_before_read(self: c104.Point, callable: Callable[[c104.Point], None]) -> None

    set python callback that will be called on incoming interrogation or read commands to support polling

    **Callable signature**

    Parameters
    ----------
    point: :ref:`c104.Point`
        point instance

    Returns
    -------
    None

    Raises
    ------
    ValueError
        If callable signature does not match exactly, parent station reference is invalid or function is called from client context

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
      .def("on_before_auto_transmit",
           &Object::DataPoint::setOnBeforeAutoTransmitCallback, R"def(
    on_before_auto_transmit(self: c104.Point, callable: Callable[[c104.Point], None]) -> None

    set python callback that will be called before server reports a measured value interval-based

    **Callable signature**

    Parameters
    ----------
    point: :ref:`c104.Point`
        point instance

    Returns
    -------
    None

    Raises
    ------
    ValueError
        If callable signature does not match exactly, parent station reference is invalid or function is called from client context

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
      .def("on_timer", &Object::DataPoint::setOnTimerCallback, R"def(
    on_timer(self: c104.Point, callable: Callable[[c104.Point], None], int) -> None

    set python callback that will be called in a fixed delay (timer_ms)

    **Callable signature**

    Parameters
    ----------
    point: :ref:`c104.Point`
        point instance
    interval_ms: int
        fixed delay between timer callback execution, default: 0, min: 50

    Returns
    -------
    None

    Raises
    ------
    ValueError
        If callable signature does not match exactly

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
      .def("read", &Object::DataPoint::read, R"def(
    read(self: c104.Point) -> bool

    send read command

    Returns
    -------
    bool
        True if the command was successfully accepted by the server, otherwise False

    Raises
    ------
    ValueError
        If parent station or connection reference is invalid or called from remote terminal unit (server) context

    Example
    -------
    >>> if cl_step_point.read():
    >>>     print("read command successful")
)def",
           py::return_value_policy::copy)
      .def("transmit", &Object::DataPoint::transmit, R"def(
    transmit(self: c104.Point, cause: c104.Cot) -> bool

    **Server-side point**
    report a measurement value to connected clients

    **Client-side point**
    send the command point to the server

    Parameters
    ----------
    cause: :ref:`c104.Cot`
        cause of the transmission

    Raises
    ------
    ValueError
        If parent station, server or connection reference is invalid

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
      .def_property_readonly("value", &Object::Information::getValue,
                             "Any: mixed InfoValue type")
      .def_property_readonly("quality", &Object::Information::getQuality,
                             "Any: mixed InfoQuality type")
      .def_property_readonly(
          "processed_at", &Object::Information::getProcessedAt,
          "datetime.datetime: timestamp with milliseconds of last local "
          "information processing "
          "(read-only)")
      .def_property_readonly(
          "recorded_at", &Object::Information::getRecordedAt,
          "Optional[int]: timestamp with milliseconds transported with the "
          "value "
          "itself or None (read-only)")
      .def("__repr__", &Object::Information::toString);
  ;

  py::class_<Object::SingleInfo, Object::Information,
             std::shared_ptr<Object::SingleInfo>>(
      m, "SingleInfo",
      "This class represents all specific single point information")
      .def(py::init(&Object::SingleInfo::create), R"def(
    __init__(self: c104.SingleInfo, on: bool, quality: c104.Quality = c104.Quality.None, recorded_at: Optional[datetime.datetime] = None) -> None

    create a new single info

    Parameters
    -------
    on: bool
        Single status value
    quality: :ref:`c104.Quality`
        Quality information
    recorded_at: Optional[datetime.datetime]
        Timestamp contained in the protocol message, or None if the protocol message type does not contain a timestamp.

    Example
    -------
    >>> single_info = c104.SingleInfo(on=True, quality=c104.Quality.Invalid, recorded_at=datetime.datetime.utcnow())
)def",
           "on"_a, "quality"_a = Quality::None, "recorded_at"_a = py::none())
      .def_property_readonly("on", &Object::SingleInfo::isOn,
                             "bool: the value (read-only)")
      .def("__repr__", &Object::SingleInfo::toString);

  py::class_<Object::SingleCmd, Object::Information,
             std::shared_ptr<Object::SingleCmd>>(
      m, "SingleCmd",
      "This class represents all specific single command information")
      .def(py::init(&Object::SingleCmd::create), R"def(
    __init__(self: c104.SingleCmd, on: bool, qualifier: c104.Qoc = c104.QoC.NONE, recorded_at: Optional[datetime.datetime] = None) -> None

    create a new single command

    Parameters
    -------
    on: bool
        Single command value
    qualifier: :ref:`c104.Qoc`
        Qualifier of command
    recorded_at: Optional[datetime.datetime]
        Timestamp contained in the protocol message, or None if the protocol message type does not contain a timestamp.

    Example
    -------
    >>> single_cmd = c104.SingleCmd(on=True, qualifier=c104.Qoc.SHORT_PULSE, recorded_at=datetime.datetime.utcnow())
)def",
           "on"_a, "qualifier"_a = CS101_QualifierOfCommand::NONE,
           "recorded_at"_a = py::none())
      .def_property_readonly("on", &Object::SingleCmd::isOn,
                             "bool: the value (read-only)")
      .def_property_readonly(
          "qualifier", &Object::SingleCmd::getQualifier,
          ":ref:`c104.Qoc`: the command qualifier information (read-only)")
      .def("__repr__", &Object::SingleCmd::toString);

  py::class_<Object::DoubleInfo, Object::Information,
             std::shared_ptr<Object::DoubleInfo>>(
      m, "DoubleInfo",
      "This class represents all specific double point information")
      .def(py::init(&Object::DoubleInfo::create), R"def(
    __init__(self: c104.DoubleInfo, state: c104.Double, quality: c104.Quality = c104.Quality.None, recorded_at: Optional[datetime.datetime] = None) -> None

    create a new double info

    Parameters
    -------
    state: :ref:`c104.Double`
        Double point status value
    quality: :ref:`c104.Quality`
        Quality information
    recorded_at: Optional[datetime.datetime]
        Timestamp contained in the protocol message, or None if the protocol message type does not contain a timestamp.

    Example
    -------
    >>> double_info = c104.DoubleInfo(state=c104.Double.ON, quality=c104.Quality.Invalid, recorded_at=datetime.datetime.utcnow())
)def",
           "state"_a, "quality"_a = Quality::None, "recorded_at"_a = py::none())
      .def_property_readonly("state", &Object::DoubleInfo::getState,
                             ":ref:`c104.Double`: the value (read-only)")
      .def("__repr__", &Object::DoubleInfo::toString);

  py::class_<Object::DoubleCmd, Object::Information,
             std::shared_ptr<Object::DoubleCmd>>(
      m, "DoubleCmd",
      "This class represents all specific double command information")
      .def(py::init(&Object::DoubleCmd::create), R"def(
    __init__(self: c104.DoubleCmd, state: c104.Double, qualifier: c104.Qoc = c104.QoC.NONE, recorded_at: Optional[datetime.datetime] = None) -> None

    create a new double command

    Parameters
    -------
    state: :ref:`c104.Double`
        Double command value
    qualifier: :ref:`c104.Qoc`
        Qualifier of command
    recorded_at: Optional[datetime.datetime]
        Timestamp contained in the protocol message, or None if the protocol message type does not contain a timestamp.

    Example
    -------
    >>> double_cmd = c104.DoubleCmd(state=c104.Double.ON, qualifier=c104.Qoc.SHORT_PULSE, recorded_at=datetime.datetime.utcnow())
)def",
           "state"_a, "qualifier"_a = CS101_QualifierOfCommand::NONE,
           "recorded_at"_a = py::none())
      .def_property_readonly("state", &Object::DoubleCmd::getState,
                             ":ref:`c104.Double`: the value (read-only)")
      .def_property_readonly(
          "qualifier", &Object::DoubleCmd::getQualifier,
          ":ref:`c104.Qoc`: the command qualifier information (read-only)")
      .def("__repr__", &Object::DoubleCmd::toString);

  py::class_<Object::StepInfo, Object::Information,
             std::shared_ptr<Object::StepInfo>>(
      m, "StepInfo",
      "This class represents all specific step point information")
      .def(py::init(&Object::StepInfo::create), R"def(
    __init__(self: c104.StepInfo, position: c104.Int7, transient: bool, quality: c104.Quality = c104.Quality.None, recorded_at: Optional[datetime.datetime] = None) -> None

    create a new step info

    Parameters
    -------
    position: :ref:`c104.Int7`
        Current transformer step position value
    transient: bool
        Indicator, if transformer is currently in step change procedure
    quality: :ref:`c104.Quality`
        Quality information
    recorded_at: Optional[datetime.datetime]
        Timestamp contained in the protocol message, or None if the protocol message type does not contain a timestamp.

    Example
    -------
    >>> step_info = c104.StepInfo(position=c104.Int7(2), transient=False, quality=c104.Quality.Invalid, recorded_at=datetime.datetime.utcnow())
)def",
           "state"_a, "transient"_a = false, "quality"_a = Quality::None,
           "recorded_at"_a = py::none())
      .def_property_readonly("position", &Object::StepInfo::getPosition,
                             ":ref:`c104.Int7`: the value (read-only)")
      .def_property_readonly("transient", &Object::StepInfo::isTransient,
                             "bool: if the position is transient (read-only)")
      .def("__repr__", &Object::StepInfo::toString);

  py::class_<Object::StepCmd, Object::Information,
             std::shared_ptr<Object::StepCmd>>(
      m, "StepCmd",
      "This class represents all specific step command information")
      .def(py::init(&Object::StepCmd::create), R"def(
    __init__(self: c104.StepCmd, direction: c104.Step, qualifier: c104.Qoc = c104.QoC.NONE, recorded_at: Optional[datetime.datetime] = None) -> None

    create a new step command

    Parameters
    -------
    direction: :ref:`c104.Step`
        Step command direction value
    qualifier: :ref:`c104.Qoc`
        Qualifier of Command
    recorded_at: Optional[datetime.datetime]
        Timestamp contained in the protocol message, or None if the protocol message type does not contain a timestamp.

    Example
    -------
    >>> step_cmd = c104.StepCmd(direction=c104.Step.HIGHER, qualifier=c104.Qoc.SHORT_PULSE, recorded_at=datetime.datetime.utcnow())
)def",
           "direction"_a, "qualifier"_a = CS101_QualifierOfCommand::NONE,
           "recorded_at"_a = py::none())
      .def_property_readonly("direction", &Object::StepCmd::getStep,
                             ":ref:`c104.Step`: the value (read-only)")
      .def_property_readonly(
          "qualifier", &Object::StepCmd::getQualifier,
          ":ref:`c104.Qoc`: the command qualifier information (read-only)")
      .def("__repr__", &Object::StepCmd::toString);

  py::class_<Object::BinaryInfo, Object::Information,
             std::shared_ptr<Object::BinaryInfo>>(
      m, "BinaryInfo",
      "This class represents all specific binary point information")
      .def(py::init(&Object::BinaryInfo::create), R"def(
    __init__(self: c104.BinaryInfo, blob: c104.Byte32, quality: c104.Quality = c104.Quality.None, recorded_at: Optional[datetime.datetime] = None) -> None

    create a new binary info

    Parameters
    -------
    blob: :ref:`c104.Byte32`
        Binary status value
    quality: :ref:`c104.Quality`
        Quality information
    recorded_at: Optional[datetime.datetime]
        Timestamp contained in the protocol message, or None if the protocol message type does not contain a timestamp.

    Example
    -------
    >>> binary_info = c104.BinaryInfo(blob=c104.Byte32(2345), quality=c104.Quality.Invalid, recorded_at=datetime.datetime.utcnow())
)def",
           "blob"_a, "quality"_a = Quality::None, "recorded_at"_a = py::none())
      .def_property_readonly("blob", &Object::BinaryInfo::getBlob,
                             "bytes: the value (read-only)")
      .def("__repr__", &Object::BinaryInfo::toString);

  py::class_<Object::BinaryCmd, Object::Information,
             std::shared_ptr<Object::BinaryCmd>>(
      m, "BinaryCmd",
      "This class represents all specific binary command information")
      .def(py::init(&Object::BinaryCmd::create), R"def(
    __init__(self: c104.BinaryCmd, blob: c104.Byte32, recorded_at: Optional[datetime.datetime] = None) -> None

    create a new binary command

    Parameters
    -------
    blob: :ref:`c104.Byte32`
        Binary command value
    recorded_at: Optional[datetime.datetime]
        Timestamp contained in the protocol message, or None if the protocol message type does not contain a timestamp.

    Example
    -------
    >>> binary_cmd = c104.BinaryCmd(blob=c104.Byte32(1234), recorded_at=datetime.datetime.utcnow())
)def",
           "blob"_a, "recorded_at"_a = py::none())
      .def_property_readonly("blob", &Object::BinaryCmd::getBlob,
                             "bytes: the value (read-only)")
      .def("__repr__", &Object::BinaryCmd::toString);

  py::class_<Object::NormalizedInfo, Object::Information,
             std::shared_ptr<Object::NormalizedInfo>>(
      m, "NormalizedInfo",
      "This class represents all specific normalized measurement point "
      "information")
      .def(py::init(&Object::NormalizedInfo::create), R"def(
    __init__(self: c104.NormalizedInfo, actual: c104.NormalizedFloat, quality: c104.Quality = c104.Quality.None, recorded_at: Optional[datetime.datetime] = None) -> None

    create a new normalized measurement info

    Parameters
    -------
    actual: :ref:`c104.NormalizedFloat`
        Actual measurement value [-1.f, 1.f]
    quality: :ref:`c104.Quality`
        Quality information
    recorded_at: Optional[datetime.datetime]
        Timestamp contained in the protocol message, or None if the protocol message type does not contain a timestamp.

    Example
    -------
    >>> normalized_info = c104.NormalizedInfo(actual=c104.NormalizedFloat(23.45), quality=c104.Quality.Invalid, recorded_at=datetime.datetime.utcnow())
)def",
           "actual"_a, "quality"_a = Quality::None,
           "recorded_at"_a = py::none())
      .def_property_readonly(
          "actual", &Object::NormalizedInfo::getActual,
          ":ref:`c104.NormalizedFloat`: the value (read-only)")
      .def("__repr__", &Object::NormalizedInfo::toString);

  py::class_<Object::NormalizedCmd, Object::Information,
             std::shared_ptr<Object::NormalizedCmd>>(
      m, "NormalizedCmd",
      "This class represents all specific normalized set point command "
      "information")
      .def(py::init(&Object::NormalizedCmd::create), R"def(
    __init__(self: c104.NormalizedCmd, target: c104.NormalizedFloat, qualifier: c104.UInt7 = c104.UInt7(0), recorded_at: Optional[datetime.datetime] = None) -> None

    create a new normalized set point command

    Parameters
    -------
    target: :ref:`c104.NormalizedFloat`
        Target setpoint value [-1.f, 1.f]
    qualifier: :ref:`c104.UInt7`
        Qualifier of setpoint command
    recorded_at: Optional[datetime.datetime]
        Timestamp contained in the protocol message, or None if the protocol message type does not contain a timestamp.

    Example
    -------
    >>> normalized_cmd = c104.NormalizedCmd(target=c104.NormalizedFloat(23.45), qualifier=c104.UInt7(123), recorded_at=datetime.datetime.utcnow())
)def",
           "target"_a, "qualifier"_a = LimitedUInt7((uint32_t)0),
           "recorded_at"_a = py::none())
      .def_property_readonly(
          "target", &Object::NormalizedCmd::getTarget,
          ":ref:`c104.NormalizedFloat`: the value (read-only)")
      .def_property_readonly(
          "qualifier", &Object::NormalizedCmd::getQualifier,
          ":ref:`c104.UInt7`: the command qualifier information (read-only)")
      .def("__repr__", &Object::NormalizedCmd::toString);

  py::class_<Object::ScaledInfo, Object::Information,
             std::shared_ptr<Object::ScaledInfo>>(
      m, "ScaledInfo",
      "This class represents all specific scaled measurement point information")
      .def(py::init(&Object::ScaledInfo::create), R"def(
    __init__(self: c104.ScaledInfo, actual: c104.Int16, quality: c104.Quality = c104.Quality.None, recorded_at: Optional[datetime.datetime] = None) -> None

    create a new scaled measurement info

    Parameters
    -------
    actual: :ref:`c104.Int16`
        Actual measurement value [-32768, 32767]
    quality: :ref:`c104.Quality`
        Quality information
    recorded_at: Optional[datetime.datetime]
        Timestamp contained in the protocol message, or None if the protocol message type does not contain a timestamp.

    Example
    -------
    >>> scaled_info = c104.ScaledInfo(actual=c104.Int16(-2345), quality=c104.Quality.Invalid, recorded_at=datetime.datetime.utcnow())
)def",
           "actual"_a, "quality"_a = Quality::None,
           "recorded_at"_a = py::none())
      .def_property_readonly("actual", &Object::ScaledInfo::getActual,
                             ":ref:`c104.Int16`: the value (read-only)")
      .def("__repr__", &Object::ScaledInfo::toString);

  py::class_<Object::ScaledCmd, Object::Information,
             std::shared_ptr<Object::ScaledCmd>>(
      m, "ScaledCmd",
      "This class represents all specific scaled set point command information")
      .def(py::init(&Object::ScaledCmd::create), R"def(
    __init__(self: c104.ScaledCmd, target: c104.Int16, qualifier: c104.UInt7 = c104.UInt7(0), recorded_at: Optional[datetime.datetime] = None) -> None

    create a new scaled set point command

    Parameters
    -------
    target: :ref:`c104.Int16`
        Target setpoint value [-32768, 32767]
    qualifier: :ref:`c104.UInt7`
        Qualifier of setpoint command
    recorded_at: Optional[datetime.datetime]
        Timestamp contained in the protocol message, or None if the protocol message type does not contain a timestamp.

    Example
    -------
    >>> scaled_cmd = c104.ScaledCmd(target=c104.Int16(-2345), qualifier=c104.UInt7(123), recorded_at=datetime.datetime.utcnow())
)def",
           "target"_a, "qualifier"_a = LimitedUInt7((uint32_t)0),
           "recorded_at"_a = py::none())
      .def_property_readonly("target", &Object::ScaledCmd::getTarget,
                             ":ref:`c104.Int16`: the value (read-only)")
      .def_property_readonly(
          "qualifier", &Object::ScaledCmd::getQualifier,
          ":ref:`c104.UInt7`: the command qualifier information (read-only)")
      .def("__repr__", &Object::ScaledCmd::toString);

  py::class_<Object::ShortInfo, Object::Information,
             std::shared_ptr<Object::ShortInfo>>(
      m, "ShortInfo",
      "This class represents all specific short measurement point information")
      .def(py::init(&Object::ShortInfo::create), R"def(
    __init__(self: c104.ShortInfo, actual: float, quality: c104.Quality = c104.Quality.None, recorded_at: Optional[datetime.datetime] = None) -> None

    create a new short measurement info

    Parameters
    -------
    actual: float
        Actual measurement value in 32-bit precision
    quality: :ref:`c104.Quality`
        Quality information
    recorded_at: Optional[datetime.datetime]
        Timestamp contained in the protocol message, or None if the protocol message type does not contain a timestamp.

    Example
    -------
    >>> short_info = c104.ShortInfo(actual=23.45, quality=c104.Quality.Invalid, recorded_at=datetime.datetime.utcnow())
)def",
           "actual"_a, "quality"_a = Quality::None,
           "recorded_at"_a = py::none())
      .def_property_readonly("actual", &Object::ShortInfo::getActual,
                             "float: the value (read-only)")
      .def("__repr__", &Object::ShortInfo::toString);

  py::class_<Object::ShortCmd, Object::Information,
             std::shared_ptr<Object::ShortCmd>>(
      m, "ShortCmd",
      "This class represents all specific short set point command information")
      .def(py::init(&Object::ShortCmd::create), R"def(
    __init__(self: c104.ShortCmd, target: float, qualifier: c104.UInt7 = c104.UInt7(0), recorded_at: Optional[datetime.datetime] = None) -> None

    create a new short set point command

    Parameters
    -------
    target: float
        Target setpoint value in 32-bit precision
    qualifier: :ref:`c104.UInt7`
        Qualifier of setpoint command
    recorded_at: Optional[datetime.datetime]
        Timestamp contained in the protocol message, or None if the protocol message type does not contain a timestamp.

    Example
    -------
    >>> short_cmd = c104.ShortCmd(target=-23.45, qualifier=c104.UInt7(123), recorded_at=datetime.datetime.utcnow())
)def",
           "target"_a, "qualifier"_a = LimitedUInt7((uint32_t)0),
           "recorded_at"_a = py::none())
      .def_property_readonly("target", &Object::ShortCmd::getTarget,
                             "float: the value (read-only)")
      .def_property_readonly(
          "qualifier", &Object::ShortCmd::getQualifier,
          ":ref:`c104.UInt7`: the command qualifier information (read-only)")
      .def("__repr__", &Object::ShortCmd::toString);

  py::class_<Object::BinaryCounterInfo, Object::Information,
             std::shared_ptr<Object::BinaryCounterInfo>>(
      m, "BinaryCounterInfo",
      "This class represents all specific integrated totals of binary counter "
      "point information")
      .def(py::init(&Object::BinaryCounterInfo::create), R"def(
    __init__(self: c104.BinaryCounterInfo, counter: int, sequence: c104.UInt5, quality: c104.BinaryCounterQuality = c104.BinaryCounterQuality.None, recorded_at: Optional[datetime.datetime] = None) -> None

    create a new short measurement info

    Parameters
    -------
    counter: int
        Counter value
    sequence: :ref:`c104.UInt5`
        Counter info sequence number
    quality: :ref:`c104.BinaryCounterQuality`
        Binary counter quality information
    recorded_at: Optional[datetime.datetime]
        Timestamp contained in the protocol message, or None if the protocol message type does not contain a timestamp.

    Example
    -------
    >>> counter_info = c104.BinaryCounterInfo(counter=2345, sequence=c104.UInt5(35), quality=c104.Quality.Invalid, recorded_at=datetime.datetime.utcnow())
)def",
           "counter"_a, "sequence"_a = LimitedUInt5((uint32_t)0),
           "quality"_a = Quality::None, "recorded_at"_a = py::none())
      .def_property_readonly("counter", &Object::BinaryCounterInfo::getCounter,
                             "int: the value (read-only)")
      .def_property_readonly(
          "sequence", &Object::BinaryCounterInfo::getSequence,
          ":ref:`c104.UInt5`: the counter sequence number (read-only)")
      .def("__repr__", &Object::BinaryCounterInfo::toString);

  py::class_<Object::ProtectionEquipmentEventInfo, Object::Information,
             std::shared_ptr<Object::ProtectionEquipmentEventInfo>>(
      m, "ProtectionEventInfo",
      "This class represents all specific protection equipment single event "
      "point information")
      .def(py::init(&Object::ProtectionEquipmentEventInfo::create), R"def(
    __init__(self: c104.ProtectionEventInfo, state: c104.EventState, elapsed_ms: c104.UInt16, quality: c104.Quality = c104.Quality.None, recorded_at: Optional[datetime.datetime] = None) -> None

    create a new event info raised by protection equipment

    Parameters
    -------
    state: :ref:`c104.EventState`
        State of the event
    elapsed_ms: :ref:`c104.UInt16`
        Time in milliseconds elapsed
    quality: :ref:`c104.Quality`
        Quality information
    recorded_at: Optional[datetime.datetime]
        Timestamp contained in the protocol message, or None if the protocol message type does not contain a timestamp.

    Example
    -------
    >>> single_event = c104.ProtectionEventInfo(state=c104.EventState.ON, elapsed_ms=c104.UInt16(35000), quality=c104.Quality.Invalid, recorded_at=datetime.datetime.utcnow())
)def",
           "state"_a, "elapsed_ms"_a = LimitedUInt16((uint32_t)0),
           "quality"_a = Quality::None, "recorded_at"_a = py::none())
      .def_property_readonly("state",
                             &Object::ProtectionEquipmentEventInfo::getState,
                             ":ref:`c104.EventState`: the state (read-only)")
      .def_property_readonly(
          "elapsed_ms", &Object::ProtectionEquipmentEventInfo::getElapsed_ms,
          "int: the elapsed time in milliseconds (read-only)")
      .def("__repr__", &Object::ProtectionEquipmentEventInfo::toString);

  py::class_<Object::ProtectionEquipmentStartEventsInfo, Object::Information,
             std::shared_ptr<Object::ProtectionEquipmentStartEventsInfo>>(
      m, "ProtectionStartInfo",
      "This class represents all specific protection equipment packed start "
      "events point information")
      .def(py::init(&Object::ProtectionEquipmentStartEventsInfo::create),
           R"def(
    __init__(self: c104.ProtectionStartInfo, events: c104.StartEvents, relay_duration_ms: c104.UInt16, quality: c104.Quality = c104.Quality.None, recorded_at: Optional[datetime.datetime] = None) -> None

    create a new packed event start info raised by protection equipment

    Parameters
    -------
    events: :ref:`c104.StartEvents`
        Set of start events
    relay_duration_ms: :ref:`c104.UInt16`
        Time in milliseconds of relay duration
    quality: :ref:`c104.Quality`
        Quality information
    recorded_at: Optional[datetime.datetime]
        Timestamp contained in the protocol message, or None if the protocol message type does not contain a timestamp.

    Example
    -------
    >>> start_events = c104.ProtectionStartInfo(events=c104.StartEvents.ON, relay_duration_ms=c104.UInt16(35000), quality=c104.Quality.Invalid, recorded_at=datetime.datetime.utcnow())
)def",
           "events"_a, "relay_duration_ms"_a = LimitedUInt16((uint32_t)0),
           "quality"_a = Quality::None, "recorded_at"_a = py::none())
      .def_property_readonly(
          "events", &Object::ProtectionEquipmentStartEventsInfo::getEvents,
          ":ref:`c104.StartEvents`: the started events (read-only)")
      .def_property_readonly(
          "relay_duration_ms",
          &Object::ProtectionEquipmentStartEventsInfo::getRelayDuration_ms,
          "int: the relay duration information (read-only)")
      .def("__repr__", &Object::ProtectionEquipmentStartEventsInfo::toString);

  py::class_<Object::ProtectionEquipmentOutputCircuitInfo, Object::Information,
             std::shared_ptr<Object::ProtectionEquipmentOutputCircuitInfo>>(
      m, "ProtectionCircuitInfo",
      "This class represents all specific protection equipment output circuit "
      "point information")
      .def(py::init(&Object::ProtectionEquipmentOutputCircuitInfo::create),
           R"def(
    __init__(self: c104.ProtectionCircuitInfo, circuits: c104.OutputCircuits, relay_operating_ms: c104.UInt16, quality: c104.Quality = c104.Quality.None, recorded_at: Optional[datetime.datetime] = None) -> None

    create a new output circuits info raised by protection equipment

    Parameters
    -------
    circuits: :ref:`c104.OutputCircuits`
        Set of output circuits
    relay_operating_ms: :ref:`c104.UInt16`
        Time in milliseconds of relay operation
    quality: :ref:`c104.Quality`
        Quality information
    recorded_at: Optional[datetime.datetime]
        Timestamp contained in the protocol message, or None if the protocol message type does not contain a timestamp.

    Example
    -------
    >>> output_circuits = c104.ProtectionCircuitInfo(events=c104.OutputCircuits.PhaseL1|c104.OutputCircuits.PhaseL2, relay_operating_ms=c104.UInt16(35000), quality=c104.Quality.Invalid, recorded_at=datetime.datetime.utcnow())
)def",
           "events"_a, "relay_duration_ms"_a = LimitedUInt16((uint32_t)0),
           "quality"_a = Quality::None, "recorded_at"_a = py::none())
      .def_property_readonly(
          "circuits",
          &Object::ProtectionEquipmentOutputCircuitInfo::getCircuits,
          ":ref:`c104.OutputCircuits`: the started events (read-only)")
      .def_property_readonly(
          "relay_operating_ms",
          &Object::ProtectionEquipmentOutputCircuitInfo::getRelayOperating_ms,
          "int: the relay operation duration information (read-only)")
      .def("__repr__", &Object::ProtectionEquipmentOutputCircuitInfo::toString);

  py::class_<Object::StatusWithChangeDetection, Object::Information,
             std::shared_ptr<Object::StatusWithChangeDetection>>(
      m, "StatusAndChanged",
      "This class represents all specific packed status point information with "
      "change detection")
      .def(py::init(&Object::StatusWithChangeDetection::create), R"def(
    __init__(self: c104.StatusAndChanged, status: c104.PackedSingle, changed: c104.PackedSingle, quality: c104.Quality = c104.Quality.None, recorded_at: Optional[datetime.datetime] = None) -> None

    create a new event info raised by protection equipment

    Parameters
    -------
    status: :ref:`c104.PackedSingle`
        Set of current single values
    changed: :ref:`c104.PackedSingle`
        Set of changed single values
    quality: :ref:`c104.Quality`
        Quality information
    recorded_at: Optional[datetime.datetime]
        Timestamp contained in the protocol message, or None if the protocol message type does not contain a timestamp.

    Example
    -------
    >>> status_and_changed = c104.StatusAndChanged(status=c104.PackedSingle.I0|c104.PackedSingle.I5, changed=c104.PackedSingle(15), quality=c104.Quality.Invalid, recorded_at=datetime.datetime.utcnow())
)def",
           "status"_a, "changed"_a = FieldSet16(0), "quality"_a = Quality::None,
           "recorded_at"_a = py::none())
      .def_property_readonly(
          "status", &Object::StatusWithChangeDetection::getStatus,
          ":ref:`c104.PackedSingle`: the current status (read-only)")
      .def_property_readonly(
          "changed", &Object::StatusWithChangeDetection::getChanged,
          ":ref:`c104.PackedSingle`: the changed information (read-only)")
      .def("__repr__", &Object::StatusWithChangeDetection::toString);

  py::class_<Remote::Message::IMessageInterface,
             std::shared_ptr<Remote::Message::IMessageInterface>>(
      m, "Message",
      "This class represents all protocol messages and provides access to "
      "structured properties")
      .def("__repr__", &Remote::Message::IMessageInterface::toString);

  py::class_<Remote::Message::IncomingMessage,
             Remote::Message::IMessageInterface,
             std::shared_ptr<Remote::Message::IncomingMessage>>(
      m, "IncomingMessage",
      "This class represents incoming messages and provides access to "
      "structured properties interpreted from incoming messages")
      .def_property_readonly("type", &Remote::Message::IncomingMessage::getType,
                             ":ref:`c104.Type`: iec60870 type (read-only)",
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
          ":ref:`c104.Cot`: cause of transmission (read-only)",
          py::return_value_policy::copy)
      .def_property_readonly("info", &Remote::Message::IncomingMessage::getInfo,
                             ":ref:`c104.Information`: value (read-only)")
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
                             "bytes: asdu message bytes (read-only)",
                             py::return_value_policy::take_ownership)
      .def_property_readonly(
          "raw_explain", &Remote::Message::IncomingMessage::getRawMessageString,
          "str: asdu message bytes explained (read-only)",
          py::return_value_policy::copy)
      .def_property_readonly(
          "number_of_object",
          &Remote::Message::IncomingMessage::getNumberOfObject,
          "int: number of information objects (read-only)",
          py::return_value_policy::copy)
      .def_property_readonly("is_select_command",
                             &Remote::Message::IncomingMessage::isSelectCommand,
                             "bool: test if message is a point command and has "
                             "select flag set (read-only)",
                             py::return_value_policy::copy)
      .def("first", &Remote::Message::IncomingMessage::first, R"def(
    first(self: c104.IncomingMessage) -> None

    reset message information element pointer to first position

    Returns
    -------
    None
)def")
      .def("next", &Remote::Message::IncomingMessage::next, R"def(
    next(self: c104.IncomingMessage) -> bool

    move message information element pointer to next position, starting by first one

    Returns
    -------
    bool
        True, if another information element exists, otherwise False
)def");
  ;

  //*/

  m.attr("__version__") = VERSION_INFO;
}
