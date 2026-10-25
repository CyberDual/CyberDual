// SPDX-License-Identifier: GPL-2.0-or-later
//
// Exercises Exiv2::TimeValue::read() through the public API with a time
// string whose UTC offset carries one more component than the usual ±hh:mm
// pair. The string only contains characters the reader accepts and the
// reader parses it successfully (read returns 0).

#include <exiv2/exiv2.hpp>

#include <iostream>

int main() {
  Exiv2::TimeValue time;
  const std::string hms("23:55:02+04:06:06");
  if (time.read(hms) != 0) {
    std::cerr << "time value was rejected\n";
    return 1;
  }
  const auto& t = time.getTime();
  std::cout << "parsed " << t.hour << ":" << t.minute << ":" << t.second << " tz " << t.tzHour << ":" << t.tzMinute
            << "\n";
  return 0;
}
