/**
 * Copyright 2020-2023 Fraunhofer Institute for Applied Information Technology
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
 * @file TransportSecurity.cpp
 * @brief manage transport layer security for 60870-5-104 communication
 *
 * @package iec104-python
 * @namespace Remote
 *
 * @authors Martin Unkel <martin.unkel@fit.fraunhofer.de>
 *
 */

#include "TransportSecurity.h"
#include "Helper.h"

using namespace Remote;

void TransportSecurity::eventHandler(void *parameter, TLSEventLevel eventLevel,
                                     int eventCode, const char *msg,
                                     TLSConnection con) {
  std::shared_ptr<TransportSecurity> instance =
      static_cast<TransportSecurity *>(parameter)->shared_from_this();

  char peerAddrBuf[60];
  char *peerAddr = nullptr;
  const char *tlsVersion = "unknown";

  if (con) {
    peerAddr = TLSConnection_getPeerAddress(con, peerAddrBuf);
    tlsVersion = TLSConfigVersion_toString(TLSConnection_getTLSVersion(con));
  }

  if (DEBUG_TEST(Debug::Server) || DEBUG_TEST(Debug::Client)) {
    printf("TransportSecurity.event] %s (t: %i, c: %i, version: %s remote-ip: "
           "%s)\n",
           msg, eventLevel, eventCode, tlsVersion, peerAddr);
  }
}

TransportSecurity::TransportSecurity(const bool validate,
                                     const bool only_known) {
  config = TLSConfiguration_create();
  TLSConfiguration_setEventHandler(config, &TransportSecurity::eventHandler,
                                   this);
  TLSConfiguration_setChainValidation(config, validate);
  TLSConfiguration_setAllowOnlyKnownCertificates(config, only_known);
  TLSConfiguration_enableSessionResumption(config, true);
  TLSConfiguration_setSessionResumptionInterval(config, 21600); // 6 hours

  // todo add setter for renegotiation time
  // default: no automatic renegotiation (-1)
  // TLSConfiguration_setRenegotiationTime(config, 3600000); // 1 hour
  if (DEBUG_TEST(Debug::Server) || DEBUG_TEST(Debug::Client)) {
    std::cout << "[c104.TransportSecurity] Created" << std::endl;
  }
}

TransportSecurity::~TransportSecurity() {
  TLSConfiguration_destroy(config);

  if (DEBUG_TEST(Debug::Server) || DEBUG_TEST(Debug::Client)) {
    std::cout << "[c104.TransportSecurity] Removed" << std::endl;
  }
}

void TransportSecurity::setCertificate(const std::string &cert,
                                       const std::string &key,
                                       const std::string &passphrase) {
  if (cert.empty()) {
    throw std::invalid_argument("Missing value for cert argument");
  }
  if (!file_exists(cert)) {
    throw std::invalid_argument(
        "Provided certificate filepath does not exist: " + cert);
  }
  if (key.empty()) {
    throw std::invalid_argument("Missing value for key argument");
  }
  if (!file_exists(key)) {
    throw std::invalid_argument("Provided key filepath does not exist: " +
                                cert);
  }
  if (passphrase.empty()) {
    if (!TLSConfiguration_setOwnKeyFromFile(config, key.c_str(), nullptr)) {
      throw std::invalid_argument("Passphrase required to decrypt the key");
    }
  } else {
    if (!TLSConfiguration_setOwnKeyFromFile(config, key.c_str(),
                                            passphrase.c_str())) {
      throw std::invalid_argument("Invalid passphrase to decrypt the key");
    }
  }
  TLSConfiguration_setOwnCertificateFromFile(config, cert.c_str());
}

void TransportSecurity::setCACertificate(const std::string &cert) {
  if (cert.empty()) {
    throw std::invalid_argument("Missing value for cert argument");
  }
  if (!file_exists(cert)) {
    throw std::invalid_argument(
        "Provided certificate filepath does not exist: " + cert);
  }
  TLSConfiguration_addCACertificateFromFile(config, cert.c_str());
}

void TransportSecurity::addAllowedRemoteCertificate(const std::string &cert) {
  if (cert.empty()) {
    throw std::invalid_argument("Missing value for cert argument");
  }
  if (!file_exists(cert)) {
    throw std::invalid_argument(
        "Provided certificate filepath does not exist: " + cert);
  }
  TLSConfiguration_addAllowedCertificateFromFile(config, cert.c_str());
}

void TransportSecurity::setVersion(const TLSConfigVersion min,
                                   const TLSConfigVersion max) {
  TLSConfiguration_setMinTlsVersion(config, min);
  TLSConfiguration_setMaxTlsVersion(config, max);
}

TLSConfiguration TransportSecurity::get() { return config; }
