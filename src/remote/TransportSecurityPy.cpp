/**
 * Copyright 2025-2025 Fraunhofer Institute for Applied Information Technology
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
 * @file TransportSecurityPy.cpp
 * @brief python binding for TransportSecurity class
 *
 * @package iec104-python
 * @namespace Remote
 *
 * @authors Martin Unkel <martin.unkel@fit.fraunhofer.de>
 *
 */

#include <pybind11/chrono.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include "enums.h"
#include "remote/TransportSecurity.h"

namespace py = pybind11;
using namespace pybind11::literals;

void init_remote_security(py::module_ &m) {

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
          "interval"_a = py::none())
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
          "interval"_a = py::none())
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
}
