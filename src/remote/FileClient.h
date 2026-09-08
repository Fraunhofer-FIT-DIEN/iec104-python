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
 * @file FileClient.h
 * @brief IEC 60870-5-104 file transfer client implementation
 *
 * @package iec104-python
 * @namespace Remote
 *
 * @authors Martin Unkel <martin.unkel@fit.fraunhofer.de>
 *
 */

#ifndef C104_REMOTE_FILECLIENT_H
#define C104_REMOTE_FILECLIENT_H

#include "module/GilAwareMutex.h"
#include "types.h"
#include <condition_variable>
#include <cstdint>
#include <memory>
#include <mutex>
#include <vector>

namespace Remote {

class Connection;

/**
 * @brief Directory entry from F_DR_TA_1 response
 */
struct DirectoryEntry {
  uint32_t ioa;           ///< Information Object Address (file identifier)
  uint16_t nof;           ///< Name Of File (file type: 1=transparent, 2=disturbance)
  uint32_t lengthOfFile;  ///< File size in bytes
  uint8_t sof;            ///< Status Of File byte (raw)
  bool lastFile;          ///< LFD flag: Last File of Directory
  bool isDirectory;       ///< FOR flag: File OR directory (0=file, 1=dir)
  bool fileActive;        ///< FA flag: File is being transferred
  uint64_t creationTime;  ///< File creation timestamp (milliseconds since epoch)
};

/**
 * @brief File transfer state machine states
 */
enum class FileClientState {
  IDLE,                  ///< No transfer in progress
  // Download states
  SELECTING,             ///< Sent F_SC_NA_1 (SCQ=1), waiting for F_FR_NA_1
  WAITING_FILE_READY,    ///< Waiting for file ready response
  CALLING_FILE,          ///< Sent F_SC_NA_1 (SCQ=2), waiting for F_SR_NA_1
  WAITING_SECTION_READY, ///< Waiting for section ready response
  CALLING_SECTION,       ///< Sent F_SC_NA_1 (SCQ=6), waiting for F_SG_NA_1
  RECEIVING_SEGMENTS,    ///< Receiving file segments
  SENDING_SECTION_ACK,   ///< Sending F_AF_NA_1 for section
  SENDING_FILE_ACK,      ///< Sending F_AF_NA_1 for file
  COMPLETE,              ///< Transfer completed successfully
  ERROR,                 ///< Transfer failed
  // Directory browsing states
  REQUESTING_DIRECTORY,  ///< Sent directory request, waiting for F_DR_TA_1
  RECEIVING_DIRECTORY,   ///< Receiving F_DR_TA_1 entries
  // Upload states (control direction)
  UPLOADING_FILE_READY,    ///< Sending F_FR_NA_1 (File Ready) to server
  UPLOADING_SECTION_READY, ///< Sending F_SR_NA_1 (Section Ready) to server
  SENDING_SEGMENTS,        ///< Sending F_SG_NA_1 segments to server
  SENDING_LAST_SEGMENT,    ///< Sending F_LS_NA_1 with checksum
  WAITING_FOR_ACK          ///< Waiting for F_AF_NA_1 acknowledgment from server
};

/**
 * @brief File transfer error codes
 */
enum class FileClientError {
  NONE,                ///< No error
  TIMEOUT,             ///< Operation timed out
  FILE_NOT_READY,      ///< Server reported file not ready
  SECTION_NOT_READY,   ///< Server reported section not ready
  CHECKSUM_MISMATCH,   ///< Checksum validation failed
  PROTOCOL_ERROR,      ///< Protocol violation
  CONNECTION_LOST,     ///< Connection lost during transfer
  ABORTED_BY_SERVER,   ///< Transfer aborted by server
  INVALID_RESPONSE     ///< Unexpected response from server
};

/**
 * @brief Convert FileClientState to string representation
 */
std::string FileClientState_toString(FileClientState state);

/**
 * @brief Convert FileClientError to string representation
 */
std::string FileClientError_toString(FileClientError error);

/**
 * @brief IEC 60870-5-104 file transfer client
 *
 * Implements the client-side file transfer protocol according to IEC 60870-5-7.
 * Handles the complete file download state machine including:
 * - File selection (F_SC_NA_1 with SCQ=1)
 * - File call (F_SC_NA_1 with SCQ=2)
 * - Section call (F_SC_NA_1 with SCQ=6)
 * - Segment reception (F_SG_NA_1)
 * - Last segment handling (F_LS_NA_1)
 * - Acknowledgments (F_AF_NA_1)
 */
class FileClient : public std::enable_shared_from_this<FileClient> {
public:
  // noncopyable
  FileClient(const FileClient &) = delete;
  FileClient &operator=(const FileClient &) = delete;

  /**
   * @brief Create a new FileClient instance
   * @param connection The connection to use for file transfer
   * @return Shared pointer to the new FileClient instance
   */
  [[nodiscard]] static std::shared_ptr<FileClient>
  create(std::weak_ptr<Connection> connection) {
    return std::shared_ptr<FileClient>(new FileClient(std::move(connection)));
  }

  ~FileClient();

  /**
   * @brief Download a file from the remote server (blocking)
   *
   * This method implements the complete file download protocol:
   * 1. Send F_SC_NA_1 (SCQ=1) to select the file
   * 2. Wait for F_FR_NA_1 (file ready) response
   * 3. Send F_SC_NA_1 (SCQ=2) to request the file
   * 4. For each section:
   *    a. Wait for F_SR_NA_1 (section ready)
   *    b. Send F_SC_NA_1 (SCQ=6) to request section data
   *    c. Receive F_SG_NA_1 segments
   *    d. Receive F_LS_NA_1 (last segment) with checksum
   *    e. Validate checksum and send F_AF_NA_1 acknowledgment
   * 5. Send final F_AF_NA_1 to acknowledge complete file
   *
   * @param commonAddress Station common address
   * @param ioa Information object address of the file
   * @param timeout_ms Maximum time to wait for complete transfer
   * @return Vector containing the file data, empty on failure
   */
  std::vector<uint8_t> downloadFile(uint16_t commonAddress, uint32_t ioa,
                                    uint32_t timeout_ms = 30000);

  /**
   * @brief Browse remote directory (blocking)
   *
   * Sends F_SC_NA_1 with COT=REQUEST to request directory listing.
   * Waits for F_DR_TA_1 responses until last entry (LFD=1) is received.
   *
   * @param commonAddress Station common address
   * @param ioa Information object address (typically 0 for root directory)
   * @param timeout_ms Maximum time to wait for complete directory
   * @return Vector of directory entries, empty on failure
   */
  std::vector<DirectoryEntry> browseDirectory(uint16_t commonAddress,
                                              uint32_t ioa,
                                              uint32_t timeout_ms = 30000);

  /**
   * @brief Upload a file to the remote server (blocking)
   *
   * This method implements the complete file upload protocol (control direction):
   * 1. Send F_FR_NA_1 (File Ready) with file length
   * 2. Send F_SR_NA_1 (Section Ready) with section length
   * 3. Send F_SG_NA_1 segments (max 240 bytes each)
   * 4. Send F_LS_NA_1 (Last Segment) with checksum
   * 5. Wait for F_AF_NA_1 acknowledgment from server
   *
   * WARNING: This is a WRITE operation that modifies the remote device!
   *
   * @param commonAddress Station common address
   * @param ioa Information object address for the file
   * @param nof Name of file (1=transparent, 2=disturbance)
   * @param data File data to upload
   * @param timeout_ms Maximum time to wait for acknowledgment
   * @return True if upload completed successfully
   */
  bool uploadFile(uint16_t commonAddress, uint32_t ioa, uint16_t nof,
                  const std::vector<uint8_t>& data, uint32_t timeout_ms = 30000);

  /**
   * @brief Get the current state of the file transfer
   * @return Current FileClientState
   */
  FileClientState getState() const;

  /**
   * @brief Get the last error that occurred
   * @return Last FileClientError
   */
  FileClientError getLastError() const;

  /**
   * @brief Check if a transfer is currently in progress
   * @return True if transfer is active
   */
  bool isTransferActive() const;

  /**
   * @brief Cancel any ongoing transfer
   */
  void cancelTransfer();

  // ASDU handlers - called by Connection::asduHandler

  /**
   * @brief Handle F_FR_NA_1 (File Ready) response
   * @param nof Name of file
   * @param lengthOfFile Total file size in bytes
   * @param frq File ready qualifier
   * @param positive True if file is ready
   */
  void handleFileReady(uint16_t nof, uint32_t lengthOfFile, uint8_t frq,
                       bool positive);

  /**
   * @brief Handle F_SR_NA_1 (Section Ready) response
   * @param nof Name of file
   * @param nos Name of section (section number)
   * @param lengthOfSection Section size in bytes
   * @param srq Section ready qualifier
   * @param notReady True if section is not ready
   */
  void handleSectionReady(uint16_t nof, uint8_t nos, uint32_t lengthOfSection,
                          uint8_t srq, bool notReady);

  /**
   * @brief Handle F_SG_NA_1 (File Segment) message
   * @param nof Name of file
   * @param nos Name of section (section number)
   * @param data Pointer to segment data
   * @param length Length of segment data
   */
  void handleSegment(uint16_t nof, uint8_t nos, const uint8_t *data,
                     uint8_t length);

  /**
   * @brief Handle F_LS_NA_1 (Last Segment) message
   * @param nof Name of file
   * @param nos Name of section (section number)
   * @param lsq Last segment qualifier
   * @param chs Checksum
   */
  void handleLastSegmentOrSection(uint16_t nof, uint8_t nos, uint8_t lsq,
                                  uint8_t chs);

  /**
   * @brief Handle F_DR_TA_1 (Directory) response
   * @param ioa Information Object Address of the file
   * @param nof Name of file (file type)
   * @param lengthOfFile File size in bytes
   * @param sof Status of File byte (contains LFD, FOR, FA flags)
   * @param creationTime File creation timestamp in milliseconds
   */
  void handleDirectoryEntry(uint32_t ioa, uint16_t nof, uint32_t lengthOfFile,
                            uint8_t sof, uint64_t creationTime);

  /**
   * @brief Handle F_AF_NA_1 (File Acknowledgment) response
   *
   * Called when server acknowledges a file/section during upload.
   *
   * @param nof Name of file
   * @param nos Name of section (section number)
   * @param afq Acknowledge File Qualifier (1=pos_file, 2=neg_file, 3=pos_section, 4=neg_section)
   * @param positive True if positive acknowledgment
   */
  void handleFileAck(uint16_t nof, uint8_t nos, uint8_t afq, bool positive);

private:
  /**
   * @brief Private constructor
   * @param connection The connection to use
   */
  explicit FileClient(std::weak_ptr<Connection> connection);

  /**
   * @brief Send F_SC_NA_1 with specified SCQ value
   * @param scq Select/Call Qualifier (1=select, 2=call, 6=call_section)
   * @param nos Name of section (for SCQ=6)
   * @return True if message was sent successfully
   */
  bool sendFileCommand(uint8_t scq, uint8_t nos = 0);

  /**
   * @brief Send F_AF_NA_1 acknowledgment
   * @param afq Acknowledge File Qualifier (1=pos_file, 2=neg_file,
   * 3=pos_section, 4=neg_section)
   * @param nos Name of section (for section acknowledgment)
   * @return True if message was sent successfully
   */
  bool sendFileAck(uint8_t afq, uint8_t nos = 0);

  /**
   * @brief Calculate checksum for received data
   * @param data Pointer to data
   * @param length Length of data
   * @return Calculated checksum (sum modulo 256)
   */
  static uint8_t calculateChecksum(const uint8_t *data, size_t length);

  /**
   * @brief Set state and notify waiters
   * @param newState New state to set
   */
  void setState(FileClientState newState);

  /**
   * @brief Set error and transition to ERROR state
   * @param error The error that occurred
   */
  void setError(FileClientError error);

  /// @brief Weak reference to owning connection
  std::weak_ptr<Connection> connection;

  /// @brief Mutex for state access
  mutable Module::GilAwareMutex state_mutex{"FileClient::state_mutex"};

  /// @brief Condition variable for state changes
  std::condition_variable_any state_changed;

  /// @brief Current transfer state
  std::atomic<FileClientState> state{FileClientState::IDLE};

  /// @brief Last error
  std::atomic<FileClientError> lastError{FileClientError::NONE};

  /// @brief Common address for current transfer
  uint16_t currentCA{0};

  /// @brief IOA for current transfer
  uint32_t currentIOA{0};

  /// @brief Name of file (NOF) for current transfer
  uint16_t currentNOF{0};

  /// @brief Expected total file size
  uint32_t expectedFileSize{0};

  /// @brief Current section number
  uint8_t currentSection{0};

  /// @brief Expected section size
  uint32_t expectedSectionSize{0};

  /// @brief Running checksum for current section
  uint8_t runningChecksum{0};

  /// @brief Accumulated file data
  std::vector<uint8_t> fileData;

  /// @brief Section data buffer
  std::vector<uint8_t> sectionData;

  // Directory browsing state
  /// @brief Directory entries received
  std::vector<DirectoryEntry> directoryEntries;

  /// @brief Flag indicating directory browsing is complete (LFD=1 received)
  bool directoryComplete{false};

  // Upload state
  /// @brief Data being uploaded
  std::vector<uint8_t> uploadData;

  /// @brief Current offset in upload data
  size_t uploadOffset{0};
};

} // namespace Remote

#endif // C104_REMOTE_FILECLIENT_H
