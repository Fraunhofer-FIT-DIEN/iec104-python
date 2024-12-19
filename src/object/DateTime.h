/**
 * Copyright 2024-2024 Fraunhofer Institute for Applied Information Technology
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
 * @file DateTime.h
 * @brief date time with extra flags
 *
 * @package iec104-python
 * @namespace object
 *
 * @authors Martin Unkel <martin.unkel@fit.fraunhofer.de>
 *
 */

#ifndef C104_OBJECT_DATETIME_H
#define C104_OBJECT_DATETIME_H

#include <chrono>
#include <iec60870_common.h>

class DateTime {
public:
  explicit DateTime(std::chrono::system_clock::time_point t);

  explicit DateTime(CP56Time2a t);

  [[nodiscard]] std::chrono::system_clock::time_point getTime() const {
    return time;
  }

  [[nodiscard]] bool isSubstituted() const { return substituted; }

  [[nodiscard]] bool isInvalid() const { return invalid; }

  [[nodiscard]] bool isSummertime() const { return summertime; }

protected:
  std::chrono::system_clock::time_point time;
  bool substituted;
  bool invalid;
  bool summertime;
  /*

   month
   get: >encodedValue[5] & 0x0f
   set: encodedValue[5] = (uint8_t) ((encodedValue[5] & 0xf0) + (value & 0x0f))



   encodedValue[0] + (encodedValue[1] << 8)
   -> SECOND in ms:

   encodedValue[2]
   -> MINUTE lower 6 bits
   -> Bit(6) = Substituted-Flag
   -> Bit(7) = Invalid-Flag

   encodedValue[3]
   -> HOUR lower 5 bits
   -> Bit(5) = ???
   -> Bit(6) = ???
   -> Bit(7) = SummerTime-Flag

   encodedValue[4]
   -> DAY-of-Month lower 5 bits
   -> Day-of-Week upper 3 bits (0=unused, 1=monday ... 7=sunday)

   encodedValue[5]
   -> MONTH lower 4 bits
   -> ??? upper 4 bits

   encodedValue[6]
   -> YEAR lower 7 bits
   -> Bit(7) = ???

   century?te

   Das Bit RES1 darf in Überwachungsrichtung benutzt werden, um anzugeben, ob
  dem INFORMATIONSOBJEKT die Zeitmarke beim Empfang durch die
  Fernwirkeinrichtung einer Unterstation (RTU) angehängt wurde (Echt- zeit) oder
  die Zeitmarke durch Zwischeneinrichtungen wie Konzentratorstationen oder durch
  die Zentral- station selbst ersetzt wurde (Ersatzzeit).

  add to docs
       „Das Bit SU für die Sommerzeit darf wahlweise benutzt werden, dies wird
  aber nicht empfohlen. Eine Zeitmarke mit ge- setzter SU-Markierung zeigt
  denselben Zeitwert wie eine Zeitmarke mit rückgesetzter SU-Markierung und
  Anzeige eines um genau eine Stunde früheren Zeitwerts. Dies kann hilfreich
  sein, um INFORMATIONSOBJEKTEN, die während der ersten Stunde nach der
  Umstellung von Sommerzeit auf Normalzeit erzeugt werden, die korrekte Stunde
  anzuzeigen.“

   add to docs
      Bei Systemen, die Zeitgrenzen überschreiten, wird für alle Zeitmarken die
  Übernahme von UTC empfohlen.

   */
};

#endif // C104_OBJECT_DATETIME_H
