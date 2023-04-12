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
 * @file TransportSecurity.h
 * @brief manage transport layer security for 60870-5-104 communication
 *
 * @package iec104-python
 * @namespace Remote
 *
 * @authors Martin Unkel <martin.unkel@fit.fraunhofer.de>
 *
 */

#ifndef C104_REMOTE_TRANSPORTSECURITY_H
#define C104_REMOTE_TRANSPORTSECURITY_H

#include "tls_config.h"
#include "types.h"

namespace Remote {
class TransportSecurity
    : public std::enable_shared_from_this<TransportSecurity> {
public:
  // noncopyable
  TransportSecurity(const TransportSecurity &) = delete;
  TransportSecurity &operator=(const TransportSecurity &) = delete;

  [[nodiscard]] static std::shared_ptr<TransportSecurity>
  create(bool validate = true, bool only_known = true) {
    // Not using std::make_shared because the constructor is private.
    return std::shared_ptr<TransportSecurity>(
        new TransportSecurity(validate, only_known));
  }

  static void eventHandler(void *parameter, TLSEventLevel eventLevel,
                           int eventCode, const char *msg, TLSConnection con);

  ~TransportSecurity();

  /**
   * @brief load x509 certificate from file with (optional encrypted) key from
   * file used to encrypt the connection
   * @param cert path to certificate file
   * @param key path to certificate private key file
   * @param passphrase passphrase to decrypt the certificate private key
   * @throws std::invalid_argument if loading the certificate, the key or
   * decrypting the key fails
   */
  void setCertificate(const std::string &cert, const std::string &key,
                      const std::string &passphrase = "");

  /**
   * @brief load x509 certificate of trusted authority from file
   * @param cert path to certificate file
   * @throws std::invalid_argument if loading the certificate fails
   */
  void setCACertificate(const std::string &cert);

  /**
   * @brief add a trusted communication partners x509 certificate from file
   * @param cert path to certificate file
   * @throws std::invalid_argument if loading the certificate fails
   */
  void addAllowedRemoteCertificate(const std::string &cert);

  /**
   * @brief set the supported min and/or max TLS version
   * @param min minimum required TLS version for communication
   * @param max maximum allowed TLS version for communication
   */
  void setVersion(TLSConfigVersion min = TLS_VERSION_NOT_SELECTED,
                  TLSConfigVersion max = TLS_VERSION_NOT_SELECTED);

  TLSConfiguration get();

private:
  TransportSecurity(bool validate, bool only_known);

  TLSConfiguration config{nullptr};
};
} // namespace Remote

#endif // C104_REMOTE_TRANSPORTSECURITY_H
