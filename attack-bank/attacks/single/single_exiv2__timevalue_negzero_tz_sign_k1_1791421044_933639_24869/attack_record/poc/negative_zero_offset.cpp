#include <exiv2/value.hpp>
#include <string>

int main() {
  Exiv2::TimeValue value;
  const std::string input = "23:55:02-00:30";
  return value.read(input);
}
