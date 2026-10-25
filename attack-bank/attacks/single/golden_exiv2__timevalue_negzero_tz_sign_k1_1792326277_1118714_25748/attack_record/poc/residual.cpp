// Reads an ISO 8601 time string through Exiv2's public TimeValue API.
// The timezone-offset part of the input exercises the sign handling of
// TimeValue::read() for offsets with a zero hour magnitude.
#include <exiv2/value.hpp>

#include <iostream>
#include <string>

int main(int argc, char* argv[]) {
  const std::string input = argc > 1 ? argv[1] : "12:34:56-000000000000000000000000";

  Exiv2::TimeValue value;
  const int rc = value.read(input);
  std::cout << "read() returned " << rc << "\n";
  if (rc == 0) {
    const Exiv2::TimeValue::Time& t = value.getTime();
    std::cout << t.hour << ":" << t.minute << ":" << t.second << " tz " << t.tzHour << ":" << t.tzMinute << "\n";
  }
  return 0;
}
