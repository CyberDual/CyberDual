#include <exiv2/value.hpp>

#include <string>

int main() {
  Exiv2::TimeValue value;
  const std::string text = "23:55:02-00:30:00:00:000";
  return value.read(text);
}
