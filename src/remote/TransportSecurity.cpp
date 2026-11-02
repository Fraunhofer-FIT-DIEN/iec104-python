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

static void debug_handler(void *ctx, int level, const char *file, int line,
                          const char *str) {
  fprintf(stderr, "[c104.TransportSecurity] TLS(%d): %s", level, str);
}

void TransportSecurity::eventHandler(void *parameter, TLSEventLevel eventLevel,
                                     int eventCode, const char *msg,
                                     TLSConnection con) {
  std::shared_ptr<TransportSecurity> instance{};
  try {
    instance = static_cast<TransportSecurity *>(parameter)->shared_from_this();
  } catch (const std::bad_weak_ptr &e) {
    if (DEBUG_ANY(Debug::Server | Debug::Client)) {
      std::cout << "[c104.TransportSecurity] failed to handle event: instance "
                   "already removed"
                << std::endl;
    }
    return;
  }

  char peerAddrBuf[60] = {0};
  const char *peerAddr = nullptr;
  const char *tlsVersion = "unknown";

  if (con) {
    peerAddr = TLSConnection_getPeerAddress(con, peerAddrBuf);
    tlsVersion = TLSConfigVersion_toString(TLSConnection_getTLSVersion(con));
  }
  if (!peerAddr) {
    peerAddr = "";
  }
  if (!msg) {
    msg = "";
  }

  if (DEBUG_ANY(Debug::Server | Debug::Client)) {
    printf("[c104.TransportSecurity] %s (t: %i, c: %i, version: %s remote-ip: "
           "%s)\n",
           msg, eventLevel, eventCode, tlsVersion, peerAddr);
  }

  if (instance->py_onEvent.is_set()) {
    DEBUG_PRINT(Debug::Callback, "CALLBACK TransportSecurity.on_event");
    Module::ScopedGilAcquire const scoped("TransportSecurity.on_event");
    std::string const message(msg);
    std::string const peer(peerAddr);
    // a raising handler is reported via sys.unraisablehook and stays
    // registered; the TLS decision itself is made by lib60870 and unaffected
    instance->py_onEvent.call(instance, eventLevel, eventCode, message, peer);
  }
}

void TransportSecurity::setOnEventCallback(py::object &callable) {
  py_onEvent.reset(callable);
}

TransportSecurity::TransportSecurity(const bool validate,
                                     const bool only_known) {
  config = TLSConfiguration_create();
  if (!config) {
    throw std::runtime_error("Failed to create TLS configuration");
  }
  TLSConfiguration_setEventHandler(config, &TransportSecurity::eventHandler,
                                   this);
  TLSConfiguration_setChainValidation(config, validate);
  TLSConfiguration_setTimeValidation(config, validate);
  TLSConfiguration_setAllowOnlyKnownCertificates(config, only_known);

  if (DEBUG_ANY(Debug::Server | Debug::Client)) {
    std::cout << "[c104.TransportSecurity] Created" << std::endl;
  }
}

TransportSecurity::~TransportSecurity() {
  TLSConfiguration_destroy(config);

  if (DEBUG_ANY(Debug::Server | Debug::Client)) {
    std::cout << "[c104.TransportSecurity] Removed" << std::endl;
  }
}

void TransportSecurity::setCertificate(const std::string &cert,
                                       const std::string &key,
                                       const std::string &passphrase) {
  if (readonly.load()) {
    throw std::invalid_argument(
        "The configuration has already been passed to a client or server and "
        "can no longer be modified.");
  }
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
    throw std::invalid_argument("Provided key filepath does not exist: " + key);
  }

  std::lock_guard<std::mutex> const lock(own_certificate_mutex);
  // lib60870 offers no way to clear the own certificate chain or key: a second
  // key load is rejected by mbedtls and a second certificate load would be
  // appended to the chain, so the own identity can be configured only once
  if (ownCertificateState == OwnCertificateState::LOADED) {
    throw std::invalid_argument(
        "The own certificate has already been set and cannot be replaced, "
        "create a new TransportSecurity instance instead.");
  }
  if (ownCertificateState == OwnCertificateState::FAILED) {
    throw std::invalid_argument(
        "A previous attempt to load the own certificate failed and left the "
        "configuration in an unusable state, create a new TransportSecurity "
        "instance instead.");
  }

  // a failed key load leaves the key context empty (mbedtls frees it), so the
  // call can be retried
  if (!TLSConfiguration_setOwnKeyFromFile(
          config, key.c_str(),
          passphrase.empty() ? nullptr : passphrase.c_str())) {
    throw std::invalid_argument(
        passphrase.empty()
            ? "Failed to load the private key file (invalid key file or "
              "passphrase required to decrypt the key): " +
                  key
            : "Failed to load the private key file (invalid key file or "
              "invalid passphrase to decrypt the key): " +
                  key);
  }

  // the key is set now and cannot be unset again: any failure from here on
  // poisons the instance, so it is never used with a half-loaded identity
  if (!TLSConfiguration_setOwnCertificateFromFile(config, cert.c_str())) {
    ownCertificateState = OwnCertificateState::FAILED;
    throw std::invalid_argument("Failed to load the certificate file: " + cert);
  }
  ownCertificateState = OwnCertificateState::LOADED;
}

void TransportSecurity::setCACertificate(const std::string &cert) {
  if (readonly.load()) {
    throw std::invalid_argument(
        "The configuration has already been passed to a client or server and "
        "can no longer be modified.");
  }
  if (cert.empty()) {
    throw std::invalid_argument("Missing value for cert argument");
  }
  if (!file_exists(cert)) {
    throw std::invalid_argument(
        "Provided certificate filepath does not exist: " + cert);
  }
  // note: certificates parsed before a broken one in a PEM bundle stay loaded
  if (!TLSConfiguration_addCACertificateFromFile(config, cert.c_str())) {
    throw std::invalid_argument("Failed to load the CA certificate file: " +
                                cert);
  }
}

void TransportSecurity::setCipherSuites(
    const std::vector<TLSCipherSuite> &ciphers) {
  if (readonly.load()) {
    throw std::invalid_argument(
        "The configuration has already been passed to a client or server and "
        "can no longer be modified.");
  }
  if (ciphers.empty()) {
    throw std::invalid_argument("Missing value for allowed ciphers");
  }

  TLSConfiguration_clearCipherSuiteList(config);

  for (const auto &cipher : ciphers) {
    TLSConfiguration_addCipherSuite(config, static_cast<int>(cipher));
  }
}

void TransportSecurity::setRenegotiationTime(
    const std::optional<std::chrono::milliseconds> &interval) {
  if (readonly.load()) {
    throw std::invalid_argument(
        "The configuration has already been passed to a client or server and "
        "can no longer be modified.");
  }
  if (interval.has_value()) {
    if (interval.value() < std::chrono::minutes(5) ||
        interval.value() > std::chrono::hours(24)) {
      throw std::invalid_argument(
          "The value must be between 5 minutes and 1 day.");
    }
    TLSConfiguration_setRenegotiationTime(config, interval.value().count());
  } else {
    // default: no automatic renegotiation (-1)
    TLSConfiguration_setRenegotiationTime(config, -1);
  }
}

void TransportSecurity::setResumptionInterval(
    const std::optional<std::chrono::seconds> &interval) {
  if (readonly.load()) {
    throw std::invalid_argument(
        "The configuration has already been passed to a client or server and "
        "can no longer be modified.");
  }
  if (interval.has_value()) {
    if (interval.value() < std::chrono::minutes(5) ||
        interval.value() > std::chrono::hours(168)) {
      throw std::invalid_argument(
          "The value must be between 1 second and 7 days.");
    }
    // default: 6 hours
    TLSConfiguration_enableSessionResumption(config, true);
    TLSConfiguration_setSessionResumptionInterval(config,
                                                  interval.value().count());
  } else {
    TLSConfiguration_enableSessionResumption(config, false);
  }
}

void TransportSecurity::addAllowedRemoteCertificate(const std::string &cert) {
  if (readonly.load()) {
    throw std::invalid_argument(
        "The configuration has already been passed to a client or server and "
        "can no longer be modified.");
  }
  if (cert.empty()) {
    throw std::invalid_argument("Missing value for cert argument");
  }
  if (!file_exists(cert)) {
    throw std::invalid_argument(
        "Provided certificate filepath does not exist: " + cert);
  }
  if (!TLSConfiguration_addAllowedCertificateFromFile(config, cert.c_str())) {
    throw std::invalid_argument("Failed to load the certificate file: " + cert);
  }
}

void TransportSecurity::setHostnameVerification(
    std::optional<std::string> hostname) {
  if (readonly.load()) {
    throw std::invalid_argument(
        "The configuration has already been passed to a client or server and "
        "can no longer be modified.");
  }
  if (hostname.has_value()) {
    if (hostname.value().empty()) {
      throw std::invalid_argument("Provided hostname is empty, please provide "
                                  "explicitly None to disable verification");
    }
    if (!TLSConfiguration_setHostnameVerification(config,
                                                  hostname.value().c_str())) {
      throw std::invalid_argument(
          "Failed to set hostname verification, failed to allocate memory");
    }
  } else {
    TLSConfiguration_setHostnameVerification(config, nullptr);
  }
}

void TransportSecurity::setVersion(const TLSConfigVersion min,
                                   const TLSConfigVersion max) {
  if (readonly.load()) {
    throw std::invalid_argument(
        "The configuration has already been passed to a client or server and "
        "can no longer be modified.");
  }
  TLSConfiguration_setMinTlsVersion(config, min);
  TLSConfiguration_setMaxTlsVersion(config, max);
}

TLSConfiguration TransportSecurity::get() {
  {
    std::lock_guard<std::mutex> const lock(own_certificate_mutex);
    if (ownCertificateState == OwnCertificateState::FAILED) {
      throw std::invalid_argument(
          "Loading the own certificate failed, the TransportSecurity "
          "configuration is unusable.");
    }
    readonly.store(true);
  }
  // set the debug handler for the whole lifetime of this configuration
  if (DEBUG_ANY(Debug::Server | Debug::Client)) {
    TLSConfiguration_setDebugHandler(config, debug_handler);
  } else {
    TLSConfiguration_setDebugHandler(config, nullptr);
  }
  return config;
}
