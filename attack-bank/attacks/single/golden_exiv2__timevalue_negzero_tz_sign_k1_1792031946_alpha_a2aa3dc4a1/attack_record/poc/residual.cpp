// Read an IPTC time (HHMMSS±HHMM) whose timezone hour is negative zero.
// "235502-0030" is the 11-byte basic form used by Iptc.Application2.TimeCreated.
#include <exiv2/value.hpp>
#include <iostream>

int main() {
  Exiv2::TimeValue value;
  const std::string hms("235502-0030");
  if (value.read(hms) != 0) {
    std::cerr << "read failed\n";
    return 2;
  }
  const auto& t = value.getTime();
  std::cout << "hour=" << t.hour << " minute=" << t.minute << " second=" << t.second
            << " tzHour=" << t.tzHour << " tzMinute=" << t.tzMinute << "\n";
  return 0;
}
