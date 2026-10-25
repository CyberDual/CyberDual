// SPDX-License-Identifier: GPL-2.0-or-later
//
// PoC driving TimeValue::read() through the public exiv2 API.
//
// Input: an ISO 8601 Basic-format time string HHMMSS±HHMM whose timezone
// offset has hour magnitude 00 and a zero-padded minute field
// ("235502-000000000000030" denotes 23:55:02-00:30). The string passes
// TimeValue::read()'s character-set and range validation and is accepted
// (return code 0) by both the parent revision and the tree carrying the
// upstream fix for the -00:MM/-00MM sign handling.
//
// Build against an AddressSanitizer-enabled libexiv2 and run:
//   c++ -std=c++20 -fsanitize=address -g -O1 \
//       -I<repo>/include -I<builddir> residual.cpp \
//       -L<builddir>/lib -lexiv2 -Wl,-rpath,<builddir>/lib -o residual
//   ./residual
#include <exiv2/value.hpp>
#include <iostream>
#include <string>

int main() {
  const std::string hms("235502-000000000000030");

  // The factory used when time-typed datasets (e.g. Iptc.Application2.TimeCreated)
  // are read.
  auto value = Exiv2::Value::create(Exiv2::time);
  const int rc = value->read(hms);
  std::cout << "factory: rc=" << rc << "\n";

  // TimeValue directly.
  Exiv2::TimeValue timeValue;
  const int rc2 = timeValue.read(hms);
  const auto& t = timeValue.getTime();
  std::cout << "direct:  rc=" << rc2 << " tzHour=" << t.tzHour << " tzMinute=" << t.tzMinute << "\n";
  return 0;
}
