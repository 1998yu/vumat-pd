#include "SimpleJson.hpp"

#include <iostream>
#include <stdexcept>
#include <string>

int main() {
  try {
    for (const char *token : {"-", "+1", "01", "1.", "1e", "1e+", "1-2", "1.2.3", "1e309"}) {
      bool rejected = false;
      try {
        vumat::json::parse(std::string("{\"value\":") + token + "}");
      } catch (const std::runtime_error &) {
        rejected = true;
      }
      if (!rejected)
        throw std::runtime_error(std::string("Accepted invalid numeric input: ") + token);
    }
    for (const char *token : {"1.5", "2147483648", "-2147483649", "1e100"}) {
      bool rejected = false;
      try {
        auto input = vumat::json::parse(std::string("{\"value\":") + token + "}");
        vumat::json::getInt(input, "value");
      } catch (const std::runtime_error &) {
        rejected = true;
      }
      if (!rejected)
        throw std::runtime_error(std::string("Accepted invalid integer input: ") + token);
    }
    const auto input = vumat::json::parse("{\"dt\":2.5e-5,\"steps\":4.8e3,\"zero\":-0.0}");
    if (vumat::json::getDouble(input, "dt") != 2.5e-5 ||
        vumat::json::getInt(input, "steps") != 4800 || vumat::json::getDouble(input, "zero") != 0.0)
      throw std::runtime_error("Valid scientific-notation input changed.");
    std::cout << "PASS numeric grammar, finite values and exact integer conversion\n";
    return 0;
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
