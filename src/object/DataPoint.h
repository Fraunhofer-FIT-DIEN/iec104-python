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
 * @file DataPoint.h
 * @brief abstract data point
 *
 * @package iec104-python
 * @namespace object
 *
 * @authors Martin Unkel <martin.unkel@fit.fraunhofer.de>
 *
 */

#ifndef C104_OBJECT_DATAPOINT_H
#define C104_OBJECT_DATAPOINT_H

#include "module/Callback.h"
#include "module/ScopedGilAcquire.h"
#include "object/Information.h"
#include "types.h"

namespace Object {

class DataPoint : public std::enable_shared_from_this<DataPoint> {
public:
  // noncopyable
  DataPoint(const DataPoint &) = delete;
  DataPoint &operator=(const DataPoint &) = delete;

  /**
   * @brief create a new DataPoint instance
   * @param dp_ioa information object address
   * @param dp_type iec60870-5-104 information type
   * @param dp_station station object reference
   * @param dp_report_ms auto reporting interval
   * @param dp_related_ioa related information object address, if any
   * @param dp_related_auto_return auto transmit related point on command
   * @param dp_cmd_mode command transmission mode (direct or select-and-execute)
   * @param tick_rate_ms outer tick rate
   * @throws std::invalid_argument if type is invalid
   */
  [[nodiscard]] static std::shared_ptr<DataPoint>
  create(std::uint_fast32_t dp_ioa, IEC60870_5_TypeID dp_type,
         std::shared_ptr<Station> dp_station,
         std::uint_fast16_t dp_report_ms = 0,
         std::optional<std::uint_fast32_t> dp_related_ioa = std::nullopt,
         bool dp_related_auto_return = false,
         CommandTransmissionMode dp_cmd_mode = DIRECT_COMMAND,
         std::uint_fast16_t tick_rate_ms = 0) {
    Module::ScopedGilAcquire scoped("DataPoint.create");

    // Not using std::make_shared because the constructor is private.
    return std::shared_ptr<DataPoint>(new DataPoint(
        dp_ioa, dp_type, std::move(dp_station), dp_report_ms, dp_related_ioa,
        dp_related_auto_return, dp_cmd_mode, tick_rate_ms));
  }

  /**
   * @brief Remove DataPoint and owned references
   */
  ~DataPoint();

private:
  /**
   * @brief create a new DataPoint instance
   * @param dp_ioa information object address
   * @param dp_type iec60870-5-104 information type
   * @param dp_station station object reference
   * @param dp_report_ms auto reporting interval
   * @param dp_related_ioa related information object address
   * @param dp_related_auto_return auto transmit related point on command
   * @param dp_cmd_mode command transmission mode (direct or select-and-execute)
   * @param tick_rate_ms outer tick rate
   * @throws std::invalid_argument if arguments provided are not compatible
   */
  DataPoint(std::uint_fast32_t dp_ioa, IEC60870_5_TypeID dp_type,
            std::shared_ptr<Station> dp_station,
            std::uint_fast16_t dp_report_ms,
            std::optional<std::uint_fast32_t> dp_related_ioa,
            bool dp_related_auto_return, CommandTransmissionMode dp_cmd_mode,
            std::uint_fast16_t tick_rate_ms);

  bool is_server{false};

  /// @brief IEC60870-5 remote address of this DataPoint
  const std::uint_fast32_t informationObjectAddress;

  /// @brief IEC60870-5 TypeID for related remote message
  const IEC60870_5_TypeID type;

  /// @brief parent Station object (not owning pointer)
  const std::weak_ptr<Station> station;

  /// @brief IEC60870-5 remote address of a related measurement DataPoint
  std::atomic_uint_fast32_t relatedInformationObjectAddress{0};

  /// @brief configure if related point should be auto transmitted if this point
  /// is a command point that was updated via client
  std::atomic_bool relatedInformationObjectAutoReturn{false};

  /// @brief command transmission mode (direct or select-and-execute)
  std::atomic<CommandTransmissionMode> commandMode{DIRECT_COMMAND};

  /// @brief abstract representation of information
  std::shared_ptr<Information> info{nullptr};

  /// @brief steady clock to calculate nextReportAt
  std::atomic<std::chrono::steady_clock::time_point> lastSentAt;

  const std::uint_fast16_t tickRate_ms;

  /// @brief interval (in milliseconds) between periodic transmissions, 0 => no
  /// periodic transmission
  std::atomic<std::uint_fast16_t> reportInterval_ms;

  /// @brief interval (in milliseconds) between timer execution, 0 => no timer
  std::atomic<std::uint_fast16_t> timerInterval_ms;

  std::atomic<std::chrono::steady_clock::time_point> timerNext{};

  /// @brief python callback function pointer
  Module::Callback<CommandResponseState> py_onReceive{
      "Point.on_receive",
      "(point: c104.Point, previous_info: c104.Information, message: "
      "c104.IncomingMessage) -> c104.ResponseState"};

  /// @brief python callback function pointer
  Module::Callback<void> py_onBeforeRead{"Point.on_before_read",
                                         "(point: c104.Point) -> None"};

  /// @brief python callback function pointer
  Module::Callback<void> py_onBeforeAutoTransmit{
      "Point.on_before_auto_transmit", "(point: c104.Point) -> None"};

  /// @brief python callback function pointer
  Module::Callback<void> py_onTimer{"Point.on_timer",
                                    "(point: c104.Point) -> None"};

public:
  /**
   * @brief Get the NetworkStation that owns this DataPoint
   * @return Pointer to NetworkStation or nullptr
   */
  std::shared_ptr<Station> getStation();

  /**
   * @brief Get the information object address
   * @return IOA
   */
  std::uint_fast32_t getInformationObjectAddress() const;

  /**
   * @brief Get the information object address of a related monitoring point
   * @return IOA
   */
  std::optional<std::uint_fast32_t> getRelatedInformationObjectAddress() const;

  /**
   * @brief Set the information object address of a related monitoring point
   * @throws std::invalid_argument if not a server-sided control point or
   * invalid IOA
   */
  void setRelatedInformationObjectAddress(
      std::optional<std::uint_fast32_t> related_io_address);

  /**
   * @brief Test if a related monitoring point should be auto-transmitted on
   * incoming update of this control point
   * @return if auto-transmit of related point is enabled
   */
  bool getRelatedInformationObjectAutoReturn() const;

  /**
   * @brief Configure if the related monitoring point should be auto-transmitted
   * on incoming update of this control point
   * @throws std::invalid_argument if not a server-sided control point or
   * invalid IOA
   */
  void setRelatedInformationObjectAutoReturn(bool auto_return);

  /**
   * @brief Get command transmission mode
   * @return direct or select-and-execute
   */
  CommandTransmissionMode getCommandMode() const;

  /**
   * @brief Configure command transmission mode to direct or select-and-execute
   */
  void setCommandMode(CommandTransmissionMode mode);

  /**
   * @brief Get select-and-execute lock originator address
   * @return client originator address or zero if no active selection lock
   * exists
   */
  std::optional<std::uint_fast8_t> getSelectedByOriginatorAddress();

  IEC60870_5_TypeID getType() const;

  /**
   * @brief Get automatic report transmission interval of this point
   * @return interval in milliseconds, 0 if disabled
   */
  std::uint_fast16_t getReportInterval_ms() const;

  /**
   * @brief Configure automatic report transmission interval of this monitoring
   * point
   * @throws std::invalid_argument if not a server-sided monitoring point
   */
  void setReportInterval_ms(std::uint_fast16_t interval_ms);

  /**
   * @brief Get automatic timer interval of this point
   * @return interval in milliseconds, 0 if disabled
   */
  std::uint_fast16_t getTimerInterval_ms() const;

  std::shared_ptr<Information> getInfo() const;

  /**
   * @brief Set point value
   */
  void setInfo(std::shared_ptr<Information> new_info);

  InfoValue getValue();

  /**
   * @brief Set point value
   */
  void setValue(InfoValue new_value);

  InfoQuality getQuality();

  /**
   * @brief Set point value
   */
  void setQuality(InfoQuality new_value);

  /**
   * @brief get timestamp bundled with value
   * @return milliseconds since unix-epoch
   */
  std::optional<std::chrono::system_clock::time_point> getRecordedAt() const;

  /**
   * @brief get timestamp of last local processing operation (receiving/sending)
   * @return milliseconds since unix-epoch
   */
  std::chrono::system_clock::time_point getProcessedAt() const;

  /**
   * @brief set timestamp of last local processing operation (receiving/sending)
   */
  void setProcessedAt(std::chrono::system_clock::time_point val);

  /**
   * @brief get next timer event point
   * @return seconds since unix-epoch
   */
  std::optional<std::chrono::steady_clock::time_point> nextReportAt() const;

  /**
   * @brief get next timer event point
   * @return seconds since unix-epoch
   */
  std::optional<std::chrono::steady_clock::time_point> nextTimerAt() const;

  /**
   * @brief handle remote point update, execute python callback
   * @param message incoming messsage information
   * @return response handling information (success, failure or none)
   */
  CommandResponseState
  onReceive(std::shared_ptr<Remote::Message::IncomingMessage> message);

  /**
   * @brief set python callback that will be executed on every incoming message
   * @throws std::invalid_argument if callable signature does not match
   */
  void setOnReceiveCallback(py::object &callable);

  /**
   * @brief handle point value request before automatic read-command response,
   * execute python callback
   */
  void onBeforeRead();

  /**
   * @brief set python callback that will be called on incoming interrogation or
   * read commands to support polling
   * @throws std::invalid_argument if callable signature does not match, parent
   * station reference is invalid or function is called from client context
   */
  void setOnBeforeReadCallback(py::object &callable);

  /**
   * @brief handle point value request before automatic transmission, execute
   * python callback
   */
  void onBeforeAutoTransmit();

  /**
   * @brief set python callback that will be called before server reports a
   * measured value interval-based
   * @throws std::invalid_argument if callable signature does not match, parent
   * station reference is invalid or function is called from client context
   */
  void setOnBeforeAutoTransmitCallback(py::object &callable);

  /**
   * @brief handle timer event, execute python callback
   */
  void onTimer();

  /**
   * @brief set python callback that will be called at a fixed interval
   * @throws std::invalid_argument if callable signature does not match
   */
  void setOnTimerCallback(py::object &callable,
                          std::uint_fast16_t interval_ms = 0);

  /**
   * @brief send read command to update the points value
   * @throws std::invalid_argument if parent station or connection reference is
   * invalid or function is called from server context
   */
  bool read();

  /**
   * @brief transmit point
   * @param cause cause of transmission
   * @param qualifier parameter for command duration
   * @return success information
   * @throws std::invalid_argument if parent station or connection reference is
   * invalid
   */
  bool transmit(CS101_CauseOfTransmission cause = CS101_COT_UNKNOWN_COT);

  std::string toString() const {
    std::ostringstream oss;
    oss << "<c104.Point io_address=" << std::to_string(informationObjectAddress)
        << ", type=" << TypeID_toString(type) << ", info=" << info->name()
        << ", report_ms=" << std::to_string(reportInterval_ms.load())
        << ", related_io_address=";

    if (relatedInformationObjectAddress < MAX_INFORMATION_OBJECT_ADDRESS)
      oss << std::to_string(relatedInformationObjectAddress.load());
    else
      oss << "None";

    oss << ", related_io_autoreturn="
        << bool_toString(relatedInformationObjectAutoReturn.load())
        << ", command_mode=" << CommandTransmissionMode_toString(commandMode)
        << " at " << std::hex << std::showbase
        << reinterpret_cast<std::uintptr_t>(this) << ">";
    return oss.str();
  };
};

/**
 * @brief vector definition of DataPoint objects
 */
typedef std::vector<std::shared_ptr<DataPoint>> DataPointVector;
} // namespace Object

#endif // C104_OBJECT_DATAPOINT_H
