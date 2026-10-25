#include <exiv2/value.hpp>

int main() {
  Exiv2::TimeValue value;
  return value.read("23:55:02-00:04") == 0 ? 0 : 1;
}
