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
 * @file FileClient.cpp
 * @brief IEC 60870-5-104 file transfer client implementation
 *
 * @package iec104-python
 * @namespace Remote
 *
 * @authors Martin Unkel <martin.unkel@fit.fraunhofer.de>
 *
 */

#include "FileClient.h"
#include "Connection.h"
#include "module/ScopedGilRelease.h"

using namespace Remote;
using namespace std::chrono_literals;

// SCQ (Select/Call Qualifier) values
constexpr uint8_t SCQ_SELECT_FILE = 1;
constexpr uint8_t SCQ_REQUEST_FILE = 2;
constexpr uint8_t SCQ_DEACTIVATE_FILE = 3;
constexpr uint8_t SCQ_REQUEST_SECTION = 6;
constexpr uint8_t SCQ_DEACTIVATE_SECTION = 7;

// AFQ (Acknowledge File Qualifier) values
constexpr uint8_t AFQ_POS_ACK_FILE = 1;
constexpr uint8_t AFQ_NEG_ACK_FILE = 2;
constexpr uint8_t AFQ_POS_ACK_SECTION = 3;
constexpr uint8_t AFQ_NEG_ACK_SECTION = 4;

// LSQ (Last Segment Qualifier) values
constexpr uint8_t LSQ_FILE_TRANSFER_WITHOUT_DEACT = 1;
constexpr uint8_t LSQ_FILE_TRANSFER_WITH_DEACT = 2;
constexpr uint8_t LSQ_SECTION_TRANSFER_WITHOUT_DEACT = 3;
constexpr uint8_t LSQ_SECTION_TRANSFER_WITH_DEACT = 4;

std::string Remote::FileClientState_toString(FileClientState state) {
  switch (state) {
  case FileClientState::IDLE:
    return "IDLE";
  case FileClientState::SELECTING:
    return "SELECTING";
  case FileClientState::WAITING_FILE_READY:
    return "WAITING_FILE_READY";
  case FileClientState::CALLING_FILE:
    return "CALLING_FILE";
  case FileClientState::WAITING_SECTION_READY:
    return "WAITING_SECTION_READY";
  case FileClientState::CALLING_SECTION:
    return "CALLING_SECTION";
  case FileClientState::RECEIVING_SEGMENTS:
    return "RECEIVING_SEGMENTS";
  case FileClientState::SENDING_SECTION_ACK:
    return "SENDING_SECTION_ACK";
  case FileClientState::SENDING_FILE_ACK:
    return "SENDING_FILE_ACK";
  case FileClientState::COMPLETE:
    return "COMPLETE";
  case FileClientState::ERROR:
    return "ERROR";
  case FileClientState::REQUESTING_DIRECTORY:
    return "REQUESTING_DIRECTORY";
  case FileClientState::RECEIVING_DIRECTORY:
    return "RECEIVING_DIRECTORY";
  // Upload states
  case FileClientState::UPLOADING_FILE_READY:
    return "UPLOADING_FILE_READY";
  case FileClientState::UPLOADING_SECTION_READY:
    return "UPLOADING_SECTION_READY";
  case FileClientState::SENDING_SEGMENTS:
    return "SENDING_SEGMENTS";
  case FileClientState::SENDING_LAST_SEGMENT:
    return "SENDING_LAST_SEGMENT";
  case FileClientState::WAITING_FOR_ACK:
    return "WAITING_FOR_ACK";
  }
  return "UNKNOWN";
}

std::string Remote::FileClientError_toString(FileClientError error) {
  switch (error) {
  case FileClientError::NONE:
    return "NONE";
  case FileClientError::TIMEOUT:
    return "TIMEOUT";
  case FileClientError::FILE_NOT_READY:
    return "FILE_NOT_READY";
  case FileClientError::SECTION_NOT_READY:
    return "SECTION_NOT_READY";
  case FileClientError::CHECKSUM_MISMATCH:
    return "CHECKSUM_MISMATCH";
  case FileClientError::PROTOCOL_ERROR:
    return "PROTOCOL_ERROR";
  case FileClientError::CONNECTION_LOST:
    return "CONNECTION_LOST";
  case FileClientError::ABORTED_BY_SERVER:
    return "ABORTED_BY_SERVER";
  case FileClientError::INVALID_RESPONSE:
    return "INVALID_RESPONSE";
  }
  return "UNKNOWN";
}

FileClient::FileClient(std::weak_ptr<Connection> conn)
    : connection(std::move(conn)) {
  DEBUG_PRINT(Debug::Connection, "FileClient created");
}

FileClient::~FileClient() {
  cancelTransfer();
  DEBUG_PRINT(Debug::Connection, "FileClient destroyed");
}

FileClientState FileClient::getState() const { return state.load(); }

FileClientError FileClient::getLastError() const { return lastError.load(); }

bool FileClient::isTransferActive() const {
  FileClientState s = state.load();
  return s != FileClientState::IDLE && s != FileClientState::COMPLETE &&
         s != FileClientState::ERROR;
}

void FileClient::cancelTransfer() {
  std::lock_guard<Module::GilAwareMutex> lock(state_mutex);
  if (isTransferActive()) {
    setError(FileClientError::ABORTED_BY_SERVER);
  }
}

void FileClient::setState(FileClientState newState) {
  FileClientState prev = state.load();
  if (prev != newState) {
    state.store(newState);
    DEBUG_PRINT(Debug::Connection, "FileClient state: " +
                                       FileClientState_toString(prev) + " -> " +
                                       FileClientState_toString(newState));
    state_changed.notify_all();
  }
}

void FileClient::setError(FileClientError error) {
  lastError.store(error);
  state.store(FileClientState::ERROR);
  DEBUG_PRINT(Debug::Connection,
              "FileClient error: " + FileClientError_toString(error));
  state_changed.notify_all();
}

uint8_t FileClient::calculateChecksum(const uint8_t *data, size_t length) {
  uint8_t sum = 0;
  for (size_t i = 0; i < length; i++) {
    sum += data[i];
  }
  return sum;
}

bool FileClient::sendFileCommand(uint8_t scq, uint8_t nos) {
  auto conn = connection.lock();
  if (!conn || !conn->isOpen()) {
    setError(FileClientError::CONNECTION_LOST);
    return false;
  }

  bool result = false;
  switch (scq) {
  case SCQ_SELECT_FILE:
    result = conn->fileSelect(currentCA, currentIOA, false);
    break;
  case SCQ_REQUEST_FILE:
    result = conn->fileCall(currentCA, currentIOA, currentNOF);
    break;
  case SCQ_REQUEST_SECTION:
    result = conn->sectionCall(currentCA, currentIOA, currentNOF, nos);
    break;
  default:
    DEBUG_PRINT(Debug::Connection, "FileClient: Unknown SCQ=" + std::to_string(scq));
    return false;
  }

  DEBUG_PRINT(Debug::Connection, "FileClient sendFileCommand SCQ=" +
                                     std::to_string(scq) +
                                     " NOS=" + std::to_string(nos) +
                                     " Result=" + std::to_string(result));
  return result;
}

bool FileClient::sendFileAck(uint8_t afq, uint8_t nos) {
  auto conn = connection.lock();
  if (!conn || !conn->isOpen()) {
    setError(FileClientError::CONNECTION_LOST);
    return false;
  }

  bool result = conn->fileAck(currentCA, currentIOA, currentNOF, nos, afq);

  DEBUG_PRINT(Debug::Connection, "FileClient sendFileAck AFQ=" +
                                     std::to_string(afq) +
                                     " NOS=" + std::to_string(nos) +
                                     " Result=" + std::to_string(result));
  return result;
}

std::vector<uint8_t> FileClient::downloadFile(uint16_t commonAddress,
                                              uint32_t ioa,
                                              uint32_t timeout_ms) {
  Module::ScopedGilRelease const scoped("FileClient.downloadFile");

  // Check if already transferring
  if (isTransferActive()) {
    DEBUG_PRINT(Debug::Connection,
                "FileClient: Transfer already in progress");
    return {};
  }

  auto conn = connection.lock();
  if (!conn || !conn->isOpen()) {
    DEBUG_PRINT(Debug::Connection, "FileClient: Connection not available");
    return {};
  }

  // Initialize transfer state
  {
    std::lock_guard<Module::GilAwareMutex> lock(state_mutex);
    currentCA = commonAddress;
    currentIOA = ioa;
    currentNOF = CS101_NOF_TRANSPARENT_FILE;
    currentSection = 1;  // Section numbers are 1-indexed per IEC 60870-5-7
    expectedFileSize = 0;
    expectedSectionSize = 0;
    runningChecksum = 0;
    fileData.clear();
    sectionData.clear();
    lastError.store(FileClientError::NONE);
  }

  auto deadline =
      std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout_ms);

  // Step 1: Send SELECT (SCQ=1)
  DEBUG_PRINT(Debug::Connection,
              "FileClient: Step 1 - Sending SELECT for CA=" +
                  std::to_string(commonAddress) +
                  " IOA=" + std::to_string(ioa));

  setState(FileClientState::SELECTING);

  // Use the existing fileSelect method on Connection
  if (!conn->fileSelect(commonAddress, ioa, false)) {
    setError(FileClientError::PROTOCOL_ERROR);
    return {};
  }

  // Wait for F_FR_NA_1 (File Ready)
  {
    std::unique_lock<Module::GilAwareMutex> lock(state_mutex);
    while (state.load() == FileClientState::SELECTING) {
      if (state_changed.wait_until(lock, deadline) ==
          std::cv_status::timeout) {
        setError(FileClientError::TIMEOUT);
        return {};
      }
    }
  }

  // Check if we got file ready
  if (state.load() == FileClientState::ERROR) {
    return {};
  }

  // Step 2: Send CALL (SCQ=2) to request the file
  DEBUG_PRINT(Debug::Connection, "FileClient: Step 2 - Sending CALL FILE");
  setState(FileClientState::CALLING_FILE);

  if (!sendFileCommand(SCQ_REQUEST_FILE, 0)) {
    return {};
  }

  // Main transfer loop
  while (state.load() != FileClientState::COMPLETE &&
         state.load() != FileClientState::ERROR) {

    std::unique_lock<Module::GilAwareMutex> lock(state_mutex);
    FileClientState currentState = state.load();

    // Wait for next state change or timeout
    if (state_changed.wait_until(lock, deadline) == std::cv_status::timeout) {
      setError(FileClientError::TIMEOUT);
      return {};
    }

    currentState = state.load();

    switch (currentState) {
    case FileClientState::WAITING_SECTION_READY:
      // Wait for F_SR_NA_1
      break;

    case FileClientState::CALLING_SECTION:
      // Send SCQ=6 to request section data
      lock.unlock();
      if (!sendFileCommand(SCQ_REQUEST_SECTION, currentSection)) {
        return {};
      }
      setState(FileClientState::RECEIVING_SEGMENTS);
      break;

    case FileClientState::RECEIVING_SEGMENTS:
      // Wait for F_SG_NA_1 segments
      break;

    case FileClientState::SENDING_SECTION_ACK:
      // Send positive acknowledgment for section
      lock.unlock();
      if (!sendFileAck(AFQ_POS_ACK_SECTION, currentSection)) {
        return {};
      }
      // Move to next section or complete
      if (currentSection < 255) {
        currentSection++;
        setState(FileClientState::WAITING_SECTION_READY);
      } else {
        setState(FileClientState::SENDING_FILE_ACK);
      }
      break;

    case FileClientState::SENDING_FILE_ACK:
      // Send positive acknowledgment for complete file
      lock.unlock();
      if (!sendFileAck(AFQ_POS_ACK_FILE, 0)) {
        return {};
      }
      setState(FileClientState::COMPLETE);
      break;

    default:
      break;
    }
  }

  if (state.load() == FileClientState::COMPLETE) {
    DEBUG_PRINT(Debug::Connection,
                "FileClient: Transfer complete, " +
                    std::to_string(fileData.size()) + " bytes");
    setState(FileClientState::IDLE);
    return fileData;
  }

  DEBUG_PRINT(Debug::Connection,
              "FileClient: Transfer failed: " +
                  FileClientError_toString(lastError.load()));
  setState(FileClientState::IDLE);
  return {};
}

void FileClient::handleFileReady(uint16_t nof, uint32_t lengthOfFile,
                                 uint8_t frq, bool positive) {
  std::lock_guard<Module::GilAwareMutex> lock(state_mutex);

  DEBUG_PRINT(Debug::Connection,
              "FileClient: F_FR_NA_1 received - NOF=" + std::to_string(nof) +
                  " Length=" + std::to_string(lengthOfFile) +
                  " Positive=" + std::to_string(positive));

  if (state.load() != FileClientState::SELECTING) {
    DEBUG_PRINT(Debug::Connection,
                "FileClient: Unexpected F_FR_NA_1 in state " +
                    FileClientState_toString(state.load()));
    return;
  }

  if (!positive) {
    setError(FileClientError::FILE_NOT_READY);
    return;
  }

  currentNOF = nof;
  expectedFileSize = lengthOfFile;
  fileData.reserve(lengthOfFile);

  setState(FileClientState::CALLING_FILE);
}

void FileClient::handleSectionReady(uint16_t nof, uint8_t nos,
                                    uint32_t lengthOfSection, uint8_t srq,
                                    bool notReady) {
  std::lock_guard<Module::GilAwareMutex> lock(state_mutex);

  DEBUG_PRINT(Debug::Connection,
              "FileClient: F_SR_NA_1 received - NOF=" + std::to_string(nof) +
                  " NOS=" + std::to_string(nos) +
                  " Length=" + std::to_string(lengthOfSection) +
                  " NotReady=" + std::to_string(notReady));

  FileClientState currentState = state.load();
  if (currentState != FileClientState::CALLING_FILE &&
      currentState != FileClientState::WAITING_SECTION_READY) {
    DEBUG_PRINT(Debug::Connection,
                "FileClient: Unexpected F_SR_NA_1 in state " +
                    FileClientState_toString(currentState));
    return;
  }

  if (notReady) {
    setError(FileClientError::SECTION_NOT_READY);
    return;
  }

  currentSection = nos;
  expectedSectionSize = lengthOfSection;
  sectionData.clear();
  sectionData.reserve(lengthOfSection);
  runningChecksum = 0;

  setState(FileClientState::CALLING_SECTION);
}

void FileClient::handleSegment(uint16_t nof, uint8_t nos, const uint8_t *data,
                               uint8_t length) {
  std::lock_guard<Module::GilAwareMutex> lock(state_mutex);

  DEBUG_PRINT(Debug::Connection,
              "FileClient: F_SG_NA_1 received - NOF=" + std::to_string(nof) +
                  " NOS=" + std::to_string(nos) +
                  " Length=" + std::to_string(length));

  if (state.load() != FileClientState::RECEIVING_SEGMENTS) {
    DEBUG_PRINT(Debug::Connection,
                "FileClient: Unexpected F_SG_NA_1 in state " +
                    FileClientState_toString(state.load()));
    return;
  }

  // Append data to section buffer
  sectionData.insert(sectionData.end(), data, data + length);

  // Update running checksum
  runningChecksum += calculateChecksum(data, length);

  DEBUG_PRINT(Debug::Connection,
              "FileClient: Section progress: " +
                  std::to_string(sectionData.size()) + "/" +
                  std::to_string(expectedSectionSize) + " bytes");
}

void FileClient::handleLastSegmentOrSection(uint16_t nof, uint8_t nos,
                                            uint8_t lsq, uint8_t chs) {
  std::lock_guard<Module::GilAwareMutex> lock(state_mutex);

  DEBUG_PRINT(Debug::Connection,
              "FileClient: F_LS_NA_1 received - NOF=" + std::to_string(nof) +
                  " NOS=" + std::to_string(nos) +
                  " LSQ=" + std::to_string(lsq) +
                  " CHS=" + std::to_string(chs));

  if (state.load() != FileClientState::RECEIVING_SEGMENTS) {
    DEBUG_PRINT(Debug::Connection,
                "FileClient: Unexpected F_LS_NA_1 in state " +
                    FileClientState_toString(state.load()));
    return;
  }

  // Validate checksum
  if (runningChecksum != chs) {
    DEBUG_PRINT(Debug::Connection,
                "FileClient: Checksum mismatch - Expected=" +
                    std::to_string(chs) +
                    " Got=" + std::to_string(runningChecksum));
    setError(FileClientError::CHECKSUM_MISMATCH);
    return;
  }

  // Append section data to file
  fileData.insert(fileData.end(), sectionData.begin(), sectionData.end());

  DEBUG_PRINT(Debug::Connection,
              "FileClient: Section " + std::to_string(nos) + " complete, " +
                  "Total: " + std::to_string(fileData.size()) + "/" +
                  std::to_string(expectedFileSize) + " bytes");

  // Check LSQ to determine next action
  switch (lsq) {
  case LSQ_SECTION_TRANSFER_WITHOUT_DEACT:
    // Section complete, more sections to come
    setState(FileClientState::SENDING_SECTION_ACK);
    break;

  case LSQ_SECTION_TRANSFER_WITH_DEACT:
    // Server aborted section transfer
    DEBUG_PRINT(Debug::Connection,
                "FileClient: Server aborted section transfer (LSQ=4)");
    setError(FileClientError::ABORTED_BY_SERVER);
    break;

  case LSQ_FILE_TRANSFER_WITHOUT_DEACT:
    // Last section of file, transfer complete
    setState(FileClientState::SENDING_FILE_ACK);
    break;

  case LSQ_FILE_TRANSFER_WITH_DEACT:
    // Server aborted file transfer
    DEBUG_PRINT(Debug::Connection,
                "FileClient: Server aborted file transfer (LSQ=2)");
    setError(FileClientError::ABORTED_BY_SERVER);
    break;

  default:
    DEBUG_PRINT(Debug::Connection,
                "FileClient: Unknown LSQ value: " + std::to_string(lsq));
    // Assume section complete, continue cautiously
    setState(FileClientState::SENDING_SECTION_ACK);
    break;
  }
}

std::vector<DirectoryEntry> FileClient::browseDirectory(uint16_t commonAddress,
                                                        uint32_t ioa,
                                                        uint32_t timeout_ms) {
  Module::ScopedGilRelease const scoped("FileClient.browseDirectory");

  // Check if already transferring
  if (isTransferActive()) {
    DEBUG_PRINT(Debug::Connection,
                "FileClient: Transfer already in progress");
    return {};
  }

  auto conn = connection.lock();
  if (!conn || !conn->isOpen()) {
    DEBUG_PRINT(Debug::Connection, "FileClient: Connection not available");
    return {};
  }

  // Initialize directory browsing state
  {
    std::lock_guard<Module::GilAwareMutex> lock(state_mutex);
    currentCA = commonAddress;
    currentIOA = ioa;
    directoryEntries.clear();
    directoryComplete = false;
    lastError.store(FileClientError::NONE);
  }

  auto deadline =
      std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout_ms);

  // Send directory request (F_SC_NA_1 with COT=REQUEST)
  DEBUG_PRINT(Debug::Connection,
              "FileClient: Sending directory request for CA=" +
                  std::to_string(commonAddress) +
                  " IOA=" + std::to_string(ioa));

  setState(FileClientState::REQUESTING_DIRECTORY);

  if (!conn->directoryRequest(commonAddress, ioa)) {
    setError(FileClientError::PROTOCOL_ERROR);
    return {};
  }

  setState(FileClientState::RECEIVING_DIRECTORY);

  // Wait for F_DR_TA_1 responses until LFD=1 (last file)
  {
    std::unique_lock<Module::GilAwareMutex> lock(state_mutex);
    while (!directoryComplete && state.load() != FileClientState::ERROR) {
      if (state_changed.wait_until(lock, deadline) ==
          std::cv_status::timeout) {
        setError(FileClientError::TIMEOUT);
        break;
      }
    }
  }

  if (state.load() == FileClientState::ERROR) {
    DEBUG_PRINT(Debug::Connection,
                "FileClient: Directory request failed: " +
                    FileClientError_toString(lastError.load()));
    setState(FileClientState::IDLE);
    return {};
  }

  DEBUG_PRINT(Debug::Connection,
              "FileClient: Directory complete, " +
                  std::to_string(directoryEntries.size()) + " entries");

  std::vector<DirectoryEntry> result = std::move(directoryEntries);
  directoryEntries.clear();
  directoryComplete = false;
  setState(FileClientState::IDLE);

  return result;
}

void FileClient::handleDirectoryEntry(uint32_t ioa, uint16_t nof,
                                      uint32_t lengthOfFile, uint8_t sof,
                                      uint64_t creationTime) {
  std::lock_guard<Module::GilAwareMutex> lock(state_mutex);

  DEBUG_PRINT(Debug::Connection,
              "FileClient: F_DR_TA_1 received - IOA=" + std::to_string(ioa) +
                  " NOF=" + std::to_string(nof) +
                  " Length=" + std::to_string(lengthOfFile) +
                  " SOF=0x" + std::to_string(sof));

  FileClientState currentState = state.load();
  if (currentState != FileClientState::RECEIVING_DIRECTORY &&
      currentState != FileClientState::REQUESTING_DIRECTORY) {
    DEBUG_PRINT(Debug::Connection,
                "FileClient: Unexpected F_DR_TA_1 in state " +
                    FileClientState_toString(currentState));
    return;
  }

  // Transition to receiving state if we were requesting
  if (currentState == FileClientState::REQUESTING_DIRECTORY) {
    state.store(FileClientState::RECEIVING_DIRECTORY);
  }

  // Parse SOF (Status Of File) bits
  // Bit 5 (0x20): LFD - Last File of Directory
  // Bit 6 (0x40): FOR - File OR directory (0=file, 1=directory)
  // Bit 7 (0x80): FA - File Active (being transferred)
  bool lastFile = (sof & 0x20) != 0;
  bool isDirectory = (sof & 0x40) != 0;
  bool fileActive = (sof & 0x80) != 0;

  DirectoryEntry entry;
  entry.ioa = ioa;
  entry.nof = nof;
  entry.lengthOfFile = lengthOfFile;
  entry.sof = sof;
  entry.lastFile = lastFile;
  entry.isDirectory = isDirectory;
  entry.fileActive = fileActive;
  entry.creationTime = creationTime;

  directoryEntries.push_back(entry);

  DEBUG_PRINT(Debug::Connection,
              "FileClient: Directory entry - IOA=" + std::to_string(ioa) +
                  " Size=" + std::to_string(lengthOfFile) +
                  " IsDir=" + std::to_string(isDirectory) +
                  " LastFile=" + std::to_string(lastFile));

  if (lastFile) {
    directoryComplete = true;
    setState(FileClientState::COMPLETE);
  }
}

// Maximum segment size per IEC 60870-5-104
constexpr size_t MAX_SEGMENT_SIZE = 240;

bool FileClient::uploadFile(uint16_t commonAddress, uint32_t ioa, uint16_t nof,
                            const std::vector<uint8_t>& data,
                            uint32_t timeout_ms) {
  Module::ScopedGilRelease const scoped("FileClient.uploadFile");

  if (isTransferActive()) {
    DEBUG_PRINT(Debug::Connection, "FileClient: Transfer already in progress");
    return false;
  }

  auto conn = connection.lock();
  if (!conn || !conn->isOpen()) {
    DEBUG_PRINT(Debug::Connection, "FileClient: Connection not available");
    return false;
  }

  // Initialize upload state
  {
    std::lock_guard<Module::GilAwareMutex> lock(state_mutex);
    currentCA = commonAddress;
    currentIOA = ioa;
    currentNOF = nof;
    uploadData = data;
    uploadOffset = 0;
    currentSection = 1;
    lastError.store(FileClientError::NONE);
  }

  auto deadline =
      std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout_ms);

  DEBUG_PRINT(Debug::Connection,
              "FileClient: Upload starting - CA=" + std::to_string(commonAddress) +
              " IOA=" + std::to_string(ioa) + " NOF=" + std::to_string(nof) +
              " Size=" + std::to_string(data.size()));

  // Step 1: Send F_FR_NA_1 (File Ready)
  setState(FileClientState::UPLOADING_FILE_READY);
  if (!conn->sendFileReady(commonAddress, ioa, nof, data.size())) {
    setError(FileClientError::PROTOCOL_ERROR);
    return false;
  }

  // Step 2: Send F_SR_NA_1 (Section Ready)
  setState(FileClientState::UPLOADING_SECTION_READY);
  if (!conn->sendSectionReady(commonAddress, ioa, nof, currentSection, data.size())) {
    setError(FileClientError::PROTOCOL_ERROR);
    return false;
  }

  // Step 3: Send segments
  setState(FileClientState::SENDING_SEGMENTS);
  size_t offset = 0;
  uint8_t sectionChecksum = 0;

  while (offset < data.size()) {
    size_t remaining = data.size() - offset;
    size_t segmentSize = std::min(remaining, MAX_SEGMENT_SIZE);

    // Calculate checksum for this segment
    for (size_t i = 0; i < segmentSize; i++) {
      sectionChecksum += data[offset + i];
    }

    if (!conn->sendSegment(commonAddress, ioa, nof, currentSection,
                           &data[offset], segmentSize)) {
      setError(FileClientError::PROTOCOL_ERROR);
      return false;
    }

    DEBUG_PRINT(Debug::Connection,
                "FileClient: Sent segment " + std::to_string(offset) + "-" +
                std::to_string(offset + segmentSize) + " / " +
                std::to_string(data.size()));

    offset += segmentSize;
  }

  // Step 4: Send F_LS_NA_1 (Last Segment) with checksum
  setState(FileClientState::SENDING_LAST_SEGMENT);
  if (!conn->sendLastSegment(commonAddress, ioa, nof, currentSection,
                             LSQ_FILE_TRANSFER_WITHOUT_DEACT, sectionChecksum)) {
    setError(FileClientError::PROTOCOL_ERROR);
    return false;
  }

  DEBUG_PRINT(Debug::Connection,
              "FileClient: Last segment sent, checksum=" +
              std::to_string(sectionChecksum));

  // Step 5: Wait for F_AF_NA_1 acknowledgment
  setState(FileClientState::WAITING_FOR_ACK);
  {
    std::unique_lock<Module::GilAwareMutex> lock(state_mutex);
    while (state.load() == FileClientState::WAITING_FOR_ACK) {
      if (state_changed.wait_until(lock, deadline) == std::cv_status::timeout) {
        setError(FileClientError::TIMEOUT);
        return false;
      }
    }
  }

  bool success = state.load() == FileClientState::COMPLETE;

  if (success) {
    DEBUG_PRINT(Debug::Connection,
                "FileClient: Upload complete, " + std::to_string(data.size()) +
                " bytes uploaded");
  } else {
    DEBUG_PRINT(Debug::Connection,
                "FileClient: Upload failed: " +
                FileClientError_toString(lastError.load()));
  }

  setState(FileClientState::IDLE);
  return success;
}

void FileClient::handleFileAck(uint16_t nof, uint8_t nos, uint8_t afq,
                               bool positive) {
  std::lock_guard<Module::GilAwareMutex> lock(state_mutex);

  DEBUG_PRINT(Debug::Connection,
              "FileClient: F_AF_NA_1 received - NOF=" + std::to_string(nof) +
              " NOS=" + std::to_string(nos) + " AFQ=" + std::to_string(afq) +
              " Positive=" + std::to_string(positive));

  FileClientState currentState = state.load();
  if (currentState != FileClientState::WAITING_FOR_ACK) {
    DEBUG_PRINT(Debug::Connection,
                "FileClient: Unexpected F_AF_NA_1 in state " +
                FileClientState_toString(currentState));
    return;
  }

  // AFQ values:
  // 1 = positive acknowledge of file (AFQ_POS_ACK_FILE)
  // 2 = negative acknowledge of file (AFQ_NEG_ACK_FILE)
  // 3 = positive acknowledge of section (AFQ_POS_ACK_SECTION)
  // 4 = negative acknowledge of section (AFQ_NEG_ACK_SECTION)
  if (positive && (afq == AFQ_POS_ACK_FILE || afq == AFQ_POS_ACK_SECTION)) {
    setState(FileClientState::COMPLETE);
  } else {
    setError(FileClientError::ABORTED_BY_SERVER);
  }
}
