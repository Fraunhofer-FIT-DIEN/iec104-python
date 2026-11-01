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
 * @file test_transport_security.cpp
 * @brief Catch2 tests driving src/remote/TransportSecurity.cpp to high line
 * coverage. TransportSecurity is pure TLS *configuration* (it wraps a
 * lib60870 TLSConfiguration) so no live TLS handshake is required.
 */

#include <catch2/catch_test_macros.hpp>

#include <pybind11/pybind11.h> // embedded interpreter requirement (GIL paths)

#include <chrono>
#include <cstdio>
#include <functional>
#include <string>
#include <vector>

#include "enums.h"
#include "remote/TransportSecurity.h"
#include "types.h"

using Remote::TransportSecurity;

namespace {

// The c104_tests executable is run from the build directory while the certs
// live under the source tests/ directory. Probe a few candidate locations so
// the certificate-based tests work regardless of the exact run directory.
std::string resolveCertPath(const std::string &filename) {
  const std::vector<std::string> prefixes = {
      "tests/certs/",    // run from repo root
      "../tests/certs/", // run from covbuild/ (in-tree build dir)
      "C:/Users/unkel/GIT/Fraunhofer-FIT-DIEN/iec104-python/tests/certs/",
      "/home/martin/iec104/tests/certs/",
  };
  for (const auto &prefix : prefixes) {
    std::string candidate = prefix + filename;
    if (std::FILE *f = std::fopen(candidate.c_str(), "r")) {
      std::fclose(f);
      return candidate;
    }
  }
  return prefixes.front() + filename; // fall back; caller asserts failure
}

std::string cert() { return resolveCertPath("server1.crt"); }
std::string key() { return resolveCertPath("server1.key"); }
std::string encryptedKey() { return resolveCertPath("server1_encrypted.key"); }
std::string caCert() { return resolveCertPath("ca.crt"); }
std::string clientCert() { return resolveCertPath("client1.crt"); }

const std::string NOT_EXIST = "/this/path/definitely/does/not/exist.crt";
const std::string ALREADY_PASSED =
    "The configuration has already been passed to a client or server and can "
    "no longer be modified.";

bool throwsAlreadyPassed(const std::function<void()> &fn) {
  try {
    fn();
  } catch (const std::invalid_argument &e) {
    return std::string(e.what()) == ALREADY_PASSED;
  }
  return false;
}

} // namespace

TEST_CASE("TransportSecurity construction configures chain + known-cert policy",
          "[remote::TransportSecurity]") {
  // validate=true, only_known=true
  {
    auto ts = TransportSecurity::create(true, true);
    REQUIRE(ts != nullptr);
    REQUIRE(ts->toString().find("TransportSecurity at 0x") !=
            std::string::npos);
  }
  // validate=false, only_known=false
  {
    auto ts = TransportSecurity::create(false, false);
    REQUIRE(ts != nullptr);
  }
  // mixed combos
  {
    auto ts = TransportSecurity::create(true, false);
    REQUIRE(ts != nullptr);
  }
  {
    auto ts = TransportSecurity::create(false, true);
    REQUIRE(ts != nullptr);
  }
}

TEST_CASE("TransportSecurity::get returns config and toggles readonly",
          "[remote::TransportSecurity]") {
  auto ts = TransportSecurity::create();
  TLSConfiguration config = ts->get();
  REQUIRE(config != nullptr);
}

TEST_CASE("setCertificate valid and throw branches",
          "[remote::TransportSecurity]") {
  auto ts = TransportSecurity::create();

  // valid cert + key, no passphrase
  REQUIRE_NOTHROW(ts->setCertificate(cert(), key()));
  // valid cert + key + explicit empty passphrase (treated as none). A fresh
  // config is required: mbedtls rejects re-parsing a key into an already
  // populated pk context, so setCertificate fails on a repeated load.
  {
    auto ts2 = TransportSecurity::create();
    REQUIRE_NOTHROW(ts2->setCertificate(cert(), key(), ""));
  }

  // empty cert
  REQUIRE_THROWS_AS(ts->setCertificate("", key()), std::invalid_argument);
  // non-existent cert file
  REQUIRE_THROWS_AS(ts->setCertificate(NOT_EXIST, key()),
                    std::invalid_argument);
  // empty key
  REQUIRE_THROWS_AS(ts->setCertificate(cert(), ""), std::invalid_argument);
  // non-existent key file
  REQUIRE_THROWS_AS(ts->setCertificate(cert(), NOT_EXIST),
                    std::invalid_argument);
  // wrong passphrase on an UNENCRYPTED key is ignored by mbedtls (the key has
  // no encryption to decrypt), so it succeeds. This documents that behavior;
  // the wrong-passphrase error branch is exercised below with the encrypted
  // key.
  {
    auto ts2 = TransportSecurity::create();
    REQUIRE_NOTHROW(ts2->setCertificate(cert(), key(), "wrong-passphrase"));
  }

  // Encrypted key (server1_encrypted.key, passphrase "test-pass"): a fresh
  // config is required per load because mbedtls rejects re-parsing a key into
  // an already-populated pk context.
  {
    auto ts3 = TransportSecurity::create();
    REQUIRE_NOTHROW(ts3->setCertificate(cert(), encryptedKey(), "test-pass"));
  }
  // wrong passphrase on the ENCRYPTED key -> fail to decrypt key
  {
    auto ts4 = TransportSecurity::create();
    REQUIRE_THROWS_AS(
        ts4->setCertificate(cert(), encryptedKey(), "wrong-passphrase"),
        std::invalid_argument);
  }
}

TEST_CASE("setCACertificate valid and throw branches",
          "[remote::TransportSecurity]") {
  auto ts = TransportSecurity::create();
  REQUIRE_NOTHROW(ts->setCACertificate(caCert()));
  REQUIRE_THROWS_AS(ts->setCACertificate(""), std::invalid_argument);
  REQUIRE_THROWS_AS(ts->setCACertificate(NOT_EXIST), std::invalid_argument);
}

TEST_CASE("setCipherSuites valid and throw branches",
          "[remote::TransportSecurity]") {
  auto ts = TransportSecurity::create();

  // empty vector -> throw
  REQUIRE_THROWS_AS(ts->setCipherSuites({}), std::invalid_argument);

  // non-empty valid list
  std::vector<TLSCipherSuite> ciphers = {
      TLSCipherSuite::RSA_WITH_AES_128_CBC_SHA,
      TLSCipherSuite::ECDHE_RSA_WITH_AES_128_GCM_SHA256,
  };
  REQUIRE_NOTHROW(ts->setCipherSuites(ciphers));
}

TEST_CASE("setRenegotiationTime in/out of range + nullopt",
          "[remote::TransportSecurity]") {
  auto ts = TransportSecurity::create();

  // nullopt -> disable
  REQUIRE_NOTHROW(ts->setRenegotiationTime(std::nullopt));

  using namespace std::chrono;
  // in range: 5 minutes
  REQUIRE_NOTHROW(ts->setRenegotiationTime(minutes(5)));
  // in range: 24 hours
  REQUIRE_NOTHROW(ts->setRenegotiationTime(hours(24)));
  // in range: midpoint
  REQUIRE_NOTHROW(ts->setRenegotiationTime(hours(1)));

  // out of range: below 5 minutes
  REQUIRE_THROWS_AS(ts->setRenegotiationTime(minutes(4)),
                    std::invalid_argument);
  // out of range: above 24 hours
  REQUIRE_THROWS_AS(ts->setRenegotiationTime(hours(25)), std::invalid_argument);
}

TEST_CASE("setResumptionInterval in/out of range + nullopt",
          "[remote::TransportSecurity]") {
  auto ts = TransportSecurity::create();

  // nullopt -> disable session resumption
  REQUIRE_NOTHROW(ts->setResumptionInterval(std::nullopt));

  using namespace std::chrono;
  // in range: 5 minutes
  REQUIRE_NOTHROW(ts->setResumptionInterval(minutes(5)));
  // in range: 7 days
  REQUIRE_NOTHROW(ts->setResumptionInterval(hours(168)));
  // in range: midpoint
  REQUIRE_NOTHROW(ts->setResumptionInterval(hours(6)));

  // out of range: below 5 minutes
  REQUIRE_THROWS_AS(ts->setResumptionInterval(seconds(1)),
                    std::invalid_argument);
  // out of range: above 7 days
  REQUIRE_THROWS_AS(ts->setResumptionInterval(hours(169)),
                    std::invalid_argument);
}

TEST_CASE("addAllowedRemoteCertificate valid and throw branches",
          "[remote::TransportSecurity]") {
  auto ts = TransportSecurity::create();
  REQUIRE_NOTHROW(ts->addAllowedRemoteCertificate(clientCert()));
  REQUIRE_THROWS_AS(ts->addAllowedRemoteCertificate(""), std::invalid_argument);
  REQUIRE_THROWS_AS(ts->addAllowedRemoteCertificate(NOT_EXIST),
                    std::invalid_argument);
}

TEST_CASE("setHostnameVerification valid and throw branches",
          "[remote::TransportSecurity]") {
  auto ts = TransportSecurity::create();

  // nullopt -> disable hostname verification
  REQUIRE_NOTHROW(ts->setHostnameVerification(std::nullopt));

  // valid hostname
  REQUIRE_NOTHROW(ts->setHostnameVerification(std::string("localhost")));

  // empty hostname -> throw
  REQUIRE_THROWS_AS(ts->setHostnameVerification(std::string("")),
                    std::invalid_argument);
}

TEST_CASE("setVersion valid version combinations",
          "[remote::TransportSecurity]") {
  auto ts = TransportSecurity::create();

  REQUIRE_NOTHROW(ts->setVersion());
  REQUIRE_NOTHROW(
      ts->setVersion(TLS_VERSION_NOT_SELECTED, TLS_VERSION_NOT_SELECTED));
  REQUIRE_NOTHROW(ts->setVersion(TLS_VERSION_TLS_1_2, TLS_VERSION_TLS_1_3));
  REQUIRE_NOTHROW(ts->setVersion(TLS_VERSION_TLS_1_0, TLS_VERSION_TLS_1_2));
}

TEST_CASE("debug print paths in constructor/destructor/get",
          "[remote::TransportSecurity]") {
  setDebug(Debug::Server | Debug::Client);
  {
    auto ts = TransportSecurity::create();
    (void)ts->get();
  }
  setDebug(Debug::None);
}

TEST_CASE("readonly: all setters throw after get()",
          "[remote::TransportSecurity]") {
  auto ts = TransportSecurity::create();
  (void)ts->get(); // flips readonly flag

  REQUIRE(throwsAlreadyPassed([&] { ts->setCertificate(cert(), key()); }));
  REQUIRE(throwsAlreadyPassed([&] { ts->setCACertificate(caCert()); }));
  REQUIRE(throwsAlreadyPassed([&] {
    ts->setCipherSuites({TLSCipherSuite::RSA_WITH_AES_128_CBC_SHA});
  }));
  REQUIRE(throwsAlreadyPassed([&] { ts->setRenegotiationTime(std::nullopt); }));
  REQUIRE(
      throwsAlreadyPassed([&] { ts->setResumptionInterval(std::nullopt); }));
  REQUIRE(throwsAlreadyPassed(
      [&] { ts->addAllowedRemoteCertificate(clientCert()); }));
  REQUIRE(throwsAlreadyPassed([&] { ts->setVersion(); }));
}

TEST_CASE("setHostnameVerification is readonly after get()",
          "[remote::TransportSecurity]") {
  // setHostnameVerification is guarded like the other setters: after get()
  // (the config has been passed to a client/server) it can no longer be
  // modified and throws regardless of the argument.
  auto ts = TransportSecurity::create();
  (void)ts->get();
  REQUIRE(throwsAlreadyPassed(
      [&] { ts->setHostnameVerification(std::string("localhost")); }));
  REQUIRE(
      throwsAlreadyPassed([&] { ts->setHostnameVerification(std::nullopt); }));
}

TEST_CASE("eventHandler with live TLSConfiguration instance",
          "[remote::TransportSecurity]") {
  auto ts = TransportSecurity::create();
  auto *raw = ts.get();

  // Enable debug so the handler's printf path (lines after shared_from_this)
  // is exercised alongside the generic path.
  setDebug(Debug::Server | Debug::Client);

  // con == nullptr path: generic event, no peer address lookup.
  TransportSecurity::eventHandler(raw, TLS_SEC_EVT_WARNING,
                                  TLS_EVENT_CODE_WRN_INSECURE_TLS_VERSION,
                                  "test event", nullptr);
  TransportSecurity::eventHandler(raw, TLS_SEC_EVT_INFO,
                                  TLS_EVENT_CODE_INF_SESSION_ESTABLISHED,
                                  "info", nullptr);
  TransportSecurity::eventHandler(raw, TLS_SEC_EVT_INCIDENT,
                                  TLS_EVENT_CODE_ALM_BAD_CERT, "incident",
                                  nullptr);

  setDebug(Debug::None);
}
