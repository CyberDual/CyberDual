#include <exiv2/value.hpp>

int main() {
  Exiv2::TimeValue value;
  if (value.read(std::string("12:34:56-00:30")) != 0)
    return 1;
  return value.getTime().tzMinute != -30;
}
