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

#include <pybind11/stl.h>

#define STRINGIFY(x) #x
#define MACRO_STRINGIFY(x) STRINGIFY(x)

using namespace pybind11::literals;

// @todo Ubuntu 18 x64, Ubuntu 20 x64, arm7v32, later: arm aarch64

PyObject *
IncomingMessage_getRawBytes(Remote::Message::IncomingMessage *message) {
  unsigned char *msg = message->getRawBytes();
  unsigned char msgSize = 2 + msg[1];

  PyObject *pymemview =
      PyMemoryView_FromMemory((char *)msg, msgSize, PyBUF_READ);

  return PyBytes_FromObject(pymemview);
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

PYBIND11_MODULE(c104, m) {
#ifdef _WIN32
  system("chcp 65001 > nul");
#endif

  py::options options;
  options.disable_function_signatures();
  options.disable_enum_members_docstring();

  py::enum_<InformationType>(
      m, "Data",
      "This enum contains all available information types for a datapoint")
      .value("SINGLE", SINGLE)
      .value("DOUBLE", DOUBLE)
      .value("STEP", STEP)
      .value("BITS", BITS)
      .value("NORMALIZED", NORMALIZED)
      .value("SCALED", SCALED)
      .value("SHORT", SHORT)
      .value("INTEGRATED", INTEGRATED)
      .value("NORMALIZED_PARAMETER", NORMALIZED_PARAMETER)
      .value("SCALED_PARAMETER", SCALED_PARAMETER)
      .value("SHORT_PARAMETER", SHORT_PARAMETER);

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
      m, "Init", "This enum contains all connection init command options.")
      .value("ALL", INIT_ALL)
      .value("INTERROGATION", INIT_INTERROGATION)
      .value("CLOCK_SYNC", INIT_CLOCK_SYNC)
      .value("NONE", INIT_NONE);

  py::enum_<ConnectionState>(m, "ConnectionState",
                             "This enum contains all link states for "
                             "connection state machine behaviour.")
      .value("CLOSED", CLOSED)
      .value("CLOSED_AWAIT_OPEN", CLOSED_AWAIT_OPEN)
      .value("CLOSED_AWAIT_RECONNECT", CLOSED_AWAIT_RECONNECT)
      .value("OPEN_MUTED", OPEN_MUTED)
      .value("OPEN_AWAIT_INTERROGATION", OPEN_AWAIT_INTERROGATION)
      .value("OPEN_AWAIT_CLOCK_SYNC", OPEN_AWAIT_CLOCK_SYNC)
      .value("OPEN", OPEN)
      .value("OPEN_AWAIT_CLOSED", OPEN_AWAIT_CLOSED);

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
      .value("DIRECT", DIRECT_COMMAND)
      .value("SELECT_AND_EXECUTE", SELECT_AND_EXECUTE_COMMAND);

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
          .def(py::init([]() { return Debug::None; }))
          .def("__str__", &Debug_toString, "convert to human readable string")
          .def("__repr__", &Debug_toString, "convert to human readable string")
          .def(
              "__and__", [](const Debug &a, Debug b) { return a & b; },
              py::is_operator())
          .def(
              "__or__", [](const Debug &a, Debug b) { return a | b; },
              py::is_operator())
          .def(
              "__xor__", [](const Debug &a, Debug b) { return a ^ b; },
              py::is_operator())
          .def(
              "__invert__", [](const Debug &a) { return ~a; },
              py::is_operator())
          .def(
              "__iand__",
              [](Debug &a, Debug b) {
                a &= b;
                return a;
              },
              py::is_operator())
          .def(
              "__ior__",
              [](Debug &a, Debug b) {
                a |= b;
                return a;
              },
              py::is_operator())
          .def(
              "__ixor__",
              [](Debug &a, Debug b) {
                a ^= b;
                return a;
              },
              py::is_operator())
          .def(
              "__contains__",
              [](const Debug &mode, const Debug &flag) {
                return test(mode, flag);
              },
              py::is_operator())
          .def(
              "is_any", [](const Debug &mode) { return is_any(mode); },
              "test if any debug mode is enabled")
          .def(
              "is_none", [](const Debug &mode) { return is_none(mode); },
              "test if no debug mode is enabled");

  py_debug.attr("__str__") = py::cpp_function(
      &Debug_toString, py::name("__str__"), py::is_method(py_debug));
  py_debug.attr("__repr__") = py::cpp_function(
      &Debug_toString, py::name("__repr__"), py::is_method(py_debug));

  auto py_quality =
      py::enum_<Quality>(m, "Quality",
                         "This enum contains all quality issue bits to "
                         "interpret and manipulate measurement quality.")
          .value("Overflow", Quality::Overflow)
          .value("Reserved", Quality::Reserved)
          .value("ElapsedTimeInvalid", Quality::ElapsedTimeInvalid)
          .value("Blocked", Quality::Blocked)
          .value("Substituted", Quality::Substituted)
          .value("NonTopical", Quality::NonTopical)
          .value("Invalid", Quality::Invalid)
          .def(py::init([]() { return Quality::None; }))
          .def("__str__", &Quality_toString)
          .def("__repr__", &Quality_toString)
          .def(
              "__and__", [](const Quality &a, Quality b) { return a & b; },
              py::is_operator())
          .def(
              "__rand__", [](const Quality &a, Quality b) { return a & b; },
              py::is_operator())
          .def(
              "__or__", [](const Quality &a, Quality b) { return a | b; },
              py::is_operator())
          .def(
              "__ror__", [](const Quality &a, Quality b) { return a | b; },
              py::is_operator())
          .def(
              "__xor__", [](const Quality &a, Quality b) { return a ^ b; },
              py::is_operator())
          .def(
              "__rxor__", [](const Quality &a, Quality b) { return a ^ b; },
              py::is_operator())
          .def(
              "__invert__", [](const Quality &a) { return ~a; },
              py::is_operator())
          .def(
              "__iand__",
              [](Quality &a, Quality b) {
                a &= b;
                return a;
              },
              py::is_operator())
          .def(
              "__ior__",
              [](Quality &a, Quality b) {
                a |= b;
                return a;
              },
              py::is_operator())
          .def(
              "__ixor__",
              [](Quality &a, Quality b) {
                a ^= b;
                return a;
              },
              py::is_operator())
          .def(
              "__contains__",
              [](const Quality &mode, const Quality &flag) {
                return test(mode, flag);
              },
              py::is_operator())
          .def(
              "is_any", [](const Quality &mode) { return is_any(mode); },
              "test if there are any limitations in terms of quality")
          .def(
              "is_good", [](const Quality &mode) { return is_none(mode); },
              "test if there are no limitations in terms of quality");

  py_quality.attr("__str__") = py::cpp_function(
      &Quality_toString, py::name("__str__"), py::is_method(py_quality));
  py_quality.attr("__repr__") = py::cpp_function(
      &Quality_toString, py::name("__repr__"), py::is_method(py_quality));

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
           "max"_a = TLS_VERSION_NOT_SELECTED);

  // todo remove deprecated in 2.x
  m.def(
      "add_client",
      [](const std::uint_fast32_t tick_rate_ms,
         const std::uint_fast32_t timeout_ms,
         std::shared_ptr<Remote::TransportSecurity> transport_security)
          -> std::shared_ptr<Client> {
        std::cerr << "[c104.add_client] deprecated: simple create a new "
                     "c104.Client() object with relevant arguments"
                  << std::endl;
        return Client::create(tick_rate_ms, timeout_ms,
                              std::move(transport_security));
      },
      R"def(
    add_client(tick_rate_ms: int = 1000, command_timeout_ms: int = 1000, transport_security: Optional[c104.TransportSecurity] = None) -> None

    create a new 104er client

    Parameters
    ----------
    tick_rate_ms: int
        client thread update interval
    command_timeout_ms: int
        time to wait for a command response
    transport_security: :ref:`c104.TransportSecurity`
        TLS configuration object

    Warning
    -------
    Deprecated: Use the default constructor c104.Client(...) instead. Will be removed in 2.x
)def",
      "tick_rate_ms"_a = 1000, "command_timeout_ms"_a = 1000,
      "transport_security"_a = nullptr);
  // todo remove deprecated in 2.x
  m.def(
      "remove_client",
      [](std::shared_ptr<Client> instance) {
        std::cerr
            << "[c104.remove_client] deprecated: simple discard the variable"
            << std::endl;
      },
      R"def(
    remove_client(instance: c104.Client) -> None

    destroy and free a 104er client

    Parameters
    ----------
    instance: :ref:`c104.Client`
        client instance

    Warning
    -------
    Deprecated: Simple remove all references to the instance instead. Will be removed in 2.x
)def",
      "instance"_a);

  // todo remove deprecated in 2.x
  m.def(
      "add_server",
      [](const std::string &bind_ip, const std::uint_fast16_t tcp_port,
         const std::uint_fast32_t tick_rate_ms,
         const uint_fast8_t max_open_connections,
         std::shared_ptr<Remote::TransportSecurity> transport_security)
          -> std::shared_ptr<Server> {
        std::cerr << "[c104.add_server] deprecated: simple create a new "
                     "c104.Server() object with relevant arguments"
                  << std::endl;
        return Server::create(bind_ip, tcp_port, tick_rate_ms,
                              max_open_connections,
                              std::move(transport_security));
      },
      R"def(
    add_server(self: c104.Server, ip: str = "0.0.0.0", port: int = 2404, tick_rate_ms: int = 1000, max_connections: int = 0, transport_security: Optional[c104.TransportSecurity] = None) -> None

    create a new 104er server

    Parameters
    -------
    ip: str
        listening server ip address
    port:int
        listening server port
    tick_rate_ms: int
        server thread update interval
    max_connections: int
        maximum number of clients allowed to connect
    transport_security: :ref:`c104.TransportSecurity`
        TLS configuration object

    Warning
    -------
    Deprecated: Use the default constructor c104.Server(...) instead. Will be removed in 2.x
)def",
      "ip"_a = "0.0.0.0", "port"_a = IEC_60870_5_104_DEFAULT_PORT,
      "tick_rate_ms"_a = 1000, "max_connections"_a = 0,
      "transport_security"_a = nullptr);
  // todo remove deprecated in 2.x
  m.def(
      "remove_server",
      [](std::shared_ptr<Server> instance) -> void {
        std::cerr
            << "[c104.remove_server] deprecated: simple discard the variable"
            << std::endl;
      },
      R"def(
    remove_server(instance: c104.Server) -> None

    destroy and free a 104er server

    Parameters
    ----------
    instance: :ref:`c104.Server`
        server instance

    Warning
    -------
    Deprecated: Simple remove all references to the instance instead. Will be removed in 2.x
)def",
      "instance"_a);

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
    __init__(self: c104.Client, tick_rate_ms: int = 1000, command_timeout_ms: int = 1000, transport_security: Optional[c104.TransportSecurity] = None) -> None

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
    >>> my_client = c104.Client(tick_rate_ms=1000, command_timeout_ms=1000)
)def",
           "tick_rate_ms"_a = 1000, "command_timeout_ms"_a = 1000,
           "transport_security"_a = nullptr)
      .def_property_readonly("is_running", &Client::isRunning,
                             "bool: test if client is running (read-only)",
                             py::return_value_policy::copy)
      .def_property_readonly("has_connections", &Client::hasConnections,
                             "bool: test if client has at least one remote "
                             "server connection (read-only)",
                             py::return_value_policy::copy)
      .def_property_readonly(
          "connections", &Client::getConnections,
          "List[:ref:`c104.Connection`]: list of all remote terminal unit "
          "(server) Connection objects (read-only)",
          py::return_value_policy::copy)
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
        common address (value between 0-65535)

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
        station common address (value between 0-65535)

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
        point information object address (value between 0 and 16777216)
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
           "callable"_a);

  py::class_<Server, std::shared_ptr<Server>>(
      m, "Server",
      "This class represents a local server and provides access to meta "
      "information and containing stations")
      .def(py::init(&Server::create), R"def(
    __init__(self: c104.Server, ip: str = "0.0.0.0", port: int = 2404, tick_rate_ms: int = 1000, max_connections: int = 0, transport_security: Optional[c104.TransportSecurity] = None) -> None

    create a new 104er server

    Parameters
    -------
    ip: str
        listening server ip address
    port:int
        listening server port
    tick_rate_ms: int
        server thread update interval
    max_connections: int
        maximum number of clients allowed to connect
    transport_security: :ref:`c104.TransportSecurity`
        TLS configuration object

    Example
    -------
    >>> my_server = c104.Server(ip="0.0.0.0", port=2404, tick_rate_ms=1000, max_connections=0)
)def",
           "ip"_a = "0.0.0.0", "port"_a = IEC_60870_5_104_DEFAULT_PORT,
           "tick_rate_ms"_a = 1000, "max_connections"_a = 0,
           "transport_security"_a = nullptr)
      .def_property_readonly("ip", &Server::getIP,
                             "str: ip address the server will accept "
                             "connections on, \"0.0.0.0\" = any (read-only)",
                             py::return_value_policy::copy)
      .def_property_readonly(
          "port", &Server::getPort,
          "int: port number the server will accept connections on (read-only)",
          py::return_value_policy::copy)
      .def_property_readonly("is_running", &Server::isRunning,
                             "bool: test if server is running (read-only)",
                             py::return_value_policy::copy)
      .def_property_readonly(
          "has_open_connections", &Server::hasOpenConnections,
          "bool: test if Server has open connections to clients (read-only)",
          py::return_value_policy::copy)
      .def_property_readonly(
          "open_connection_count", &Server::getOpenConnectionCount,
          "int: get number of open connections to clients (read-only)",
          py::return_value_policy::copy)
      .def_property_readonly("has_active_connections",
                             &Server::hasActiveConnections,
                             "bool: test if Server has active (open and not "
                             "muted) connections to clients (read-only)",
                             py::return_value_policy::copy)
      .def_property_readonly("active_connection_count",
                             &Server::getActiveConnectionCount,
                             "int: get number of active (open and not muted) "
                             "connections to clients (read-only)",
                             py::return_value_policy::copy)
      .def_property_readonly(
          "has_stations", &Server::hasStations,
          "bool: test if local server has at least one station (read-only)",
          py::return_value_policy::copy)
      .def_property_readonly("stations", &Server::getStations,
                             "List[:ref:`c104.Station`]: list of all local "
                             "Station objects (read-only)",
                             py::return_value_policy::copy)
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
        station common address (value between 0 and 65535)

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
        station common address (value between 0 and 65535)

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
           "callable"_a);

  py::class_<Remote::Connection, std::shared_ptr<Remote::Connection>>(
      m, "Connection",
      "This class represents connections from a client to a remote server and "
      "provides access to meta information and containing stations")
      .def_property_readonly(
          "ip", &Remote::Connection::getIP,
          "str: remote terminal units (server) ip (read-only)",
          py::return_value_policy::copy)
      .def_property_readonly(
          "port", &Remote::Connection::getPort,
          "int: remote terminal units (server) port (read-only)",
          py::return_value_policy::copy)
      .def_property_readonly(
          "has_stations", &Remote::Connection::hasStations,
          "bool: test if remote server has at least one station (read-only)",
          py::return_value_policy::copy)
      .def_property_readonly(
          "stations", &Remote::Connection::getStations,
          "List[:ref:`c104.Station`] list of all Station objects (read-only)",
          py::return_value_policy::copy)
      .def_property_readonly("is_connected", &Remote::Connection::isOpen,
                             "bool: test if connection is opened (read-only)",
                             py::return_value_policy::copy)
      .def_property_readonly("is_muted", &Remote::Connection::isMuted,
                             "bool: test if connection is muted (read-only)",
                             py::return_value_policy::copy)
      .def_property(
          "originator_address", &Remote::Connection::getOriginatorAddress,
          &Remote::Connection::setOriginatorAddress,
          "int: primary originator address of this connection (0-255)",
          py::return_value_policy::copy)
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
    interrogation(self: c104.Connection, common_address: int, cause: c104.Cot = c104.Cot.ACTIVATION, qualifier: c104.Qoi = c104.Qoi.Station, wait_for_response: bool = True) -> bool

    send an interrogation command to the remote terminal unit (server)

    Parameters
    ----------
    common_address: int
        station common address (value between 0 and 65535)
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
    >>> if not my_connection.interrogation(common_address=47, cause=c104.Cot.ACTIVATION, qualifier=c104.Qoi.Station):
    >>>     raise ValueError("Cannot send interrogation command")
)def",
           "common_address"_a, "cause"_a = CS101_COT_ACTIVATION,
           "qualifier"_a = QOI_STATION, "wait_for_response"_a = true,
           py::return_value_policy::copy)
      .def("counter_interrogation", &Remote::Connection::counterInterrogation,
           R"def(
    counter_interrogation(self: c104.Connection, common_address: int, cause: c104.Cot = c104.Cot.ACTIVATION, qualifier: c104.Qoi = c104.Qoi.Station, wait_for_response: bool = True) -> bool

    send a counter interrogation command to the remote terminal unit (server)

    Parameters
    ----------
    common_address: int
        station common address (value between 0 and 65535)
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
    >>> if not my_connection.counter_interrogation(common_address=47, cause=c104.Cot.ACTIVATION, qualifier=c104.Qoi.Station):
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
        station common address (value between 0 and 65535)
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
        station common address (value between 0 and 65535)
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
        station common address (value between 0 and 65535)

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
        station common address (value between 0 and 65535)

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
           "callable"_a);

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
          "int: common address of this station (0-65535) (read-only)",
          py::return_value_policy::copy)
      .def_property_readonly("is_local", &Object::Station::isLocal,
                             "bool: test if station is local (has sever) or "
                             "remote (has connection) one (read-only)",
                             py::return_value_policy::copy)
      .def_property_readonly(
          "has_points", &Object::Station::hasPoints,
          "bool: test if station has at least one point (read-only)",
          py::return_value_policy::copy)
      .def_property_readonly(
          "points", &Object::Station::getPoints,
          "List[:ref:`c104.Point`] list of all Point objects (read-only)",
          py::return_value_policy::copy)
      .def("get_point", &Object::Station::getPoint, R"def(
    get_point(self: c104.Station, io_address: int) -> Optional[c104.Point]

    get a point object via information object address

    Parameters
    ----------
    io_address: int
        point information object address (value between 0 and 16777216)

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
    add_point(self: c104.Station, io_address: int, type: c104.Type, report_ms: int = 0, related_io_address: int = 0, related_io_autoreturn: bool = False, command_mode: c104.CommandMode = c104.CommandMode.DIRECT) -> Optional[c104.Point]

    add a new point to this station and return the new point object

    Parameters
    ----------
    io_address: int
        point information object address (value between 0 and 16777216)
    type: :ref:`c104.Type`
        point information type
    report_ms: int
        automatic reporting interval in milliseconds (monitoring points server-sided only)
    related_io_address: int
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
           "related_io_address"_a = 0, "related_io_autoreturn"_a = false,
           "command_mode"_a = DIRECT_COMMAND);

  py::class_<Object::DataPoint, std::shared_ptr<Object::DataPoint>>(
      m, "Point",
      "This class represents command and measurement data point of a station "
      "and provides access to structured properties of points")
      .def_property_readonly(
          "station", &Object::DataPoint::getStation,
          "Optional[:ref:`c104.Station`]: parent Station object (read-only)")
      .def_property_readonly("io_address",
                             &Object::DataPoint::getInformationObjectAddress,
                             "int: information object address (read-only)",
                             py::return_value_policy::copy)
      .def_property_readonly("type", &Object::DataPoint::getType,
                             ":ref:`c104.Type`: iec60870 data Type (read-only)",
                             py::return_value_policy::copy)
      .def_property("quality", &Object::DataPoint::getQuality,
                    &Object::DataPoint::setQuality,
                    ":ref:`c104.Quality`: Quality bitset object",
                    py::return_value_policy::copy)
      .def_property("related_io_address",
                    &Object::DataPoint::getRelatedInformationObjectAddress,
                    &Object::DataPoint::setRelatedInformationObjectAddress,
                    "int: io_address of a related monitoring point",
                    py::return_value_policy::copy)
      .def_property(
          "related_io_autoreturn",
          &Object::DataPoint::getRelatedInformationObjectAutoReturn,
          &Object::DataPoint::setRelatedInformationObjectAutoReturn,
          "bool: toggle automatic return info remote response on or off",
          py::return_value_policy::copy)
      .def_property("command_mode", &Object::DataPoint::getCommandMode,
                    &Object::DataPoint::setCommandMode,
                    "c104.CommandMode: set direct or select-and-execute "
                    "command transmission mode",
                    py::return_value_policy::copy)
      .def_property(
          "selected_by", &Object::DataPoint::getSelectedByOriginatorAddress,
          &Object::DataPoint::setSelectedByOriginatorAddress,
          "int: originator address (1-255) = SELECTED, 0 = NOT SELECTED",
          py::return_value_policy::copy)
      .def_property("report_ms", &Object::DataPoint::getReportInterval_ms,
                    &Object::DataPoint::setReportInterval_ms,
                    "int: interval in milliseconds between periodic "
                    "transmission, 0 = no periodic transmission",
                    py::return_value_policy::copy)
      .def_property(
          "value", &Object::DataPoint::getValue,
          [](Object::DataPoint &d1, const py::object &o) {
            if (py::isinstance<StepCommandValue>(o)) {
              d1.setValue(static_cast<int>(py::cast<StepCommandValue>(o)));
              return;
            }
            if (py::isinstance<DoublePointValue>(o)) {
              d1.setValue(static_cast<int>(py::cast<DoublePointValue>(o)));
              return;
            }
            d1.setValue(py::cast<double>(o));
          },
          "float: value", py::return_value_policy::copy)
      .def_property_readonly(
          "value_uint32", &Object::DataPoint::getValueAsUInt32,
          "int: value formatted as unsigned integer (read-only)",
          py::return_value_policy::copy)
      .def_property_readonly(
          "value_int32", &Object::DataPoint::getValueAsInt32,
          "int: value formatted as signed integer (read-only)",
          py::return_value_policy::copy)
      .def_property_readonly("value_float", &Object::DataPoint::getValueAsFloat,
                             "float: value formatted as float (read-only)",
                             py::return_value_policy::copy)
      .def_property_readonly( // todo convert to datetime.datetime
          "updated_at_ms", &Object::DataPoint::getUpdatedAt_ms,
          "int: timestamp in milliseconds of last value update (read-only)",
          py::return_value_policy::copy)
      .def_property_readonly( // todo convert to datetime.datetime
          "reported_at_ms", &Object::DataPoint::getReportedAt_ms,
          "int: timestamp in milliseconds of last transmission (read-only)",
          py::return_value_policy::copy)
      .def_property_readonly( // todo convert to datetime.datetime
          "received_at_ms", &Object::DataPoint::getReceivedAt_ms,
          "int: timestamp in milliseconds of last incoming message (read-only)",
          py::return_value_policy::copy)
      .def_property_readonly( // todo convert to datetime.datetime
          "sent_at_ms", &Object::DataPoint::getSentAt_ms,
          "int: timestamp in milliseconds of last outgoing message (read-only)",
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
    previous_state: dict
        dictionary containing the state of the point before the command took effect :code:`{"value": float, "quality": :ref:`c104.Quality`, updatedAt_ms: int}`
    message: c104.IncomingMessage
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
    >>> def on_setpoint_command(point: c104.Point, previous_state: dict, message: c104.IncomingMessage) -> c104.ResponseState:
    >>>     print("SV] {0} SETPOINT COMMAND on IOA: {1}, new: {2}, prev: {3}, cot: {4}, quality: {5}".format(point.type, point.io_address, point.value, previous_state, message.cot, point.quality))
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
    >>> def on_before_read_steppoint(point: c104.Point) -> None:
    >>>     print("SV] {0} READ COMMAND on IOA: {1}".format(point.type, point.io_address))
    >>>     point.value = random.randint(-64,63)  # import random
    >>>
    >>> step_point = sv_station_2.add_point(io_address=31, type=c104.Type.M_ST_TB_1, report_ms=2000)
    >>> step_point.on_before_auto_transmit(callable=on_before_read_steppoint)
)def",
           "callable"_a)
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
      .def("set", &Object::DataPoint::setValueEx, R"def(
    set(self: c104.Point, value: float, quality: c104.Quality, timestamp_ms: int) -> None

    set value, quality and timestamp

    Parameters
    ----------
    value: float
        point value
    quality: :ref:`c104.Quality`
        quality restrictions if any
    timestamp_ms: int
        modification timestamp in milliseconds

    Returns
    -------
    None

    Example
    -------
    >>> sv_measurement_point.set(value=-1234.56, quality=c104.Quality.Invalid, timestamp_ms=int(time.time() * 1000))
)def",
           "value"_a, "quality"_a, "timestamp_ms"_a)
      .def("transmit", &Object::DataPoint::transmit, R"def(
    transmit(self: c104.Point, cause: c104.Cot = c104.Cot.UNKNOWN_COT) -> bool

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

    Warning
    -------
    It is recommended to specify a cause and not to use UNKNOWN_COT.

    Returns
    -------
    bool
        True if the command was successfully send (server-side) or accepted by the server (client-side), otherwise False

    Example
    -------
    >>> sv_measurement_point.transmit(cause=c104.Cot.SPONTANEOUS)
)def",
           "cause"_a = CS101_COT_UNKNOWN_COT, py::return_value_policy::copy);

  py::class_<Remote::Message::IncomingMessage,
             std::shared_ptr<Remote::Message::IncomingMessage>>(
      m, "IncomingMessage",
      "This class represents incoming messages and provides access to "
      "structured properties interpreted from incoming messages")
      .def_property_readonly("type", &Remote::Message::IncomingMessage::getType,
                             ":ref:`c104.Type`: iec60870 type (read-only)",
                             py::return_value_policy::copy)
      .def_property_readonly(
          "common_address", &Remote::Message::IncomingMessage::getCommonAddress,
          "int: common address (0-65535) (read-only)",
          py::return_value_policy::copy)
      .def_property_readonly(
          "originator_address",
          &Remote::Message::IncomingMessage::getOriginatorAddress,
          "int: originator address (0-255) (read-only)",
          py::return_value_policy::copy)
      .def_property_readonly("io_address",
                             &Remote::Message::IncomingMessage::getIOA,
                             "int: information object address (read-only)",
                             py::return_value_policy::copy)
      .def_property_readonly(
          "connection_string",
          &Remote::Message::IncomingMessage::getConnectionString,
          "str: connection string in format :code:`ip:port` (read-only)",
          py::return_value_policy::copy)
      .def_property_readonly(
          "cot", &Remote::Message::IncomingMessage::getCauseOfTransmission,
          ":ref:`c104.Cot`: cause of transmission (read-only)",
          py::return_value_policy::copy)
      .def_property_readonly(
          "value", &Remote::Message::IncomingMessage::getValue,
          "float: value (read-only)", py::return_value_policy::copy)
      .def_property_readonly(
          "quality", &Remote::Message::IncomingMessage::getQuality,
          ":ref:`c104.Quality`: quality restrictions bitset object (read-only)",
          py::return_value_policy::copy)
      .def_property_readonly("is_test",
                             &Remote::Message::IncomingMessage::isTest,
                             "bool: test if test flag is set (read-only)",
                             py::return_value_policy::copy)
      .def_property_readonly("is_sequence",
                             &Remote::Message::IncomingMessage::isSequence,
                             "bool: test if sequence flag is set (read-only)",
                             py::return_value_policy::copy)
      .def_property_readonly("is_negative",
                             &Remote::Message::IncomingMessage::isNegative,
                             "bool: test if negative flag is set (read-only)",
                             py::return_value_policy::copy)
      .def_property_readonly("raw", &IncomingMessage_getRawBytes,
                             "bytes: asdu message bytes (read-only)",
                             py::return_value_policy::copy)
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

  //*/
#ifdef VERSION_INFO
  m.attr("__version__") = MACRO_STRINGIFY(VERSION_INFO);
#else
  m.attr("__version__") = "dev";
#endif
}
