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
/**
 * @class TransportSecurity
 * @brief Manages the configuration and enforcement of TLS security settings for
 * secure communication.
 *
 * This class provides functionalities for managing certificates, configuring
 * TLS settings, enforcing secure communication protocols, and handling
 * TLS-related events. It is designed to ensure secure transport protocols are
 * adhered to, supporting features like cipher suite customization, session
 * renegotiation intervals, and restricting communication to trusted parties.
 */
class TransportSecurity
    : public std::enable_shared_from_this<TransportSecurity> {
public:
  // noncopyable
  TransportSecurity(const TransportSecurity &) = delete;
  TransportSecurity &operator=(const TransportSecurity &) = delete;

  /**
   * @brief Creates a new instance of the TransportSecurity object with the
   * specified configuration.
   * @param validate Whether to enable validation for the TransportSecurity
   * instance. Default is true.
   * @param only_known Whether to restrict communication to only known and
   * trusted certificates. Default is true.
   * @return A shared pointer to the newly created TransportSecurity instance.
   */
  [[nodiscard]] static std::shared_ptr<TransportSecurity>
  create(bool validate = true, bool only_known = true) {
    // Not using std::make_shared because the constructor is private.
    return std::shared_ptr<TransportSecurity>(
        new TransportSecurity(validate, only_known));
  }

  /**
   * @brief Destructor for the TransportSecurity class.
   *
   * Clean up the TLS configuration structures
   */
  ~TransportSecurity();

  /**
   * @brief Handles TLS-related events during the TransportSecurity lifetime.
   * @param parameter A pointer to user-defined data or context associated with
   * the event.
   * @param eventLevel The severity level of the TLS event.
   * @param eventCode A code representing the specific type of TLS event.
   * @param msg A descriptive message providing details about the event.
   * @param con The associated TLS connection instance for the event.
   */
  static void eventHandler(void *parameter, TLSEventLevel eventLevel,
                           int eventCode, const char *msg, TLSConnection con);

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
   * @brief Set the list of allowed TLS cipher suites for communication.
   * @param ciphers A vector of TLSCipherSuite representing the allowed cipher
   * suites.
   * @throws std::invalid_argument if the configuration has already been
   * finalized or if the provided cipher list is empty.
   */
  void setCipherSuites(const std::vector<TLSCipherSuite> &ciphers);

  /**
   * @brief Set the interval for automatic TLS session renegotiation.
   * @param interval The desired renegotiation interval in milliseconds. If no
   * value is provided, automatic renegotiation is disabled by default.
   * @throws std::invalid_argument if the configuration has already been passed
   * to a client or server and can no longer be modified.
   */
  void setRenegotiationTime(
      const std::optional<std::chrono::milliseconds> &interval);

  /**
   * @brief Set the interval for session resumption in TLS configuration.
   * @param interval The desired session resumption interval in seconds. If no
   * value is provided, session resumption is disabled.
   * @throws std::invalid_argument if the configuration has already been
   * finalized or made read-only.
   */
  void
  setResumptionInterval(const std::optional<std::chrono::seconds> &interval);

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

  /**
   * @brief Retrieves the current TLS configuration for use by client and server
   * instances.
   *
   * Invoking this method transitions the associated TransportSecurity object to
   * a readonly state. This transition is required because the client and server
   * will invoke setupComplete, after which modifications are no longer allowed.
   *
   * @return The current TLSConfiguration object containing the TLS security
   * settings.
   */
  TLSConfiguration get();

private:
  /**
   * @brief Constructs a TransportSecurity instance with specified validation
   * and certificate policies.
   *
   * Initializes the configuration for TLS settings, setting up event handlers
   * and defining certificate chain and time validation preferences. It also
   * configures whether to restrict communication strictly to known
   * certificates.
   *
   * @param validate A boolean value indicating whether certificate chain and
   * time validation should be enforced.
   * @param only_known A boolean value specifying whether only known
   * certificates should be allowed for secure communication.
   * @return An instance of TransportSecurity configured with the specified
   * security policies.
   */
  TransportSecurity(bool validate, bool only_known);

  /// @brief actual configuration structure
  TLSConfiguration config{nullptr};

  /// @brief toggle, if this config can still be modified or not
  std::atomic_bool readonly{false};

public:
  /**
   * @brief Converts the TransportSecurity object to its string representation.
   *
   * This method generates a string containing the memory address of the
   * TransportSecurity object. Useful for debugging and logging purposes.
   *
   * @return A string describing the TransportSecurity object, including its
   * memory address.
   */
  std::string toString() const {
    std::ostringstream oss;
    oss << "<104.TransportSecurity at " << std::hex << std::showbase
        << reinterpret_cast<std::uintptr_t>(this) << ">";
    return oss.str();
  };
};
} // namespace Remote

#endif // C104_REMOTE_TRANSPORTSECURITY_H
