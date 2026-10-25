// PoC: Exiv2::TimeValue::read() heap/stack memory-safety reproducer.
// Public API, in-contract input (a time string the parser must gracefully
// accept or reject). Aborts under AddressSanitizer on the patched tree;
// cleanly returns 1 on the parent revision.
#include <exiv2/value.hpp>

#include <cstdio>

int main() {
  Exiv2::TimeValue value;
  // ISO 8601 extended time with a negative zero-hour UTC offset carrying
  // surplus minute digits: invalid, must be rejected with return code 1.
  const int rc = value.read("23:55:02-00:3000");
  std::printf("TimeValue::read returned %d\n", rc);
  return 0;
}
