#include <iostream>
#include <stdexcept>

void exception_handler(int value) {
  try {
    if (value < 0) {
      throw std::invalid_argument("Value cannot be negative.");
    }
    if (value == 0) {
      throw "Value cannot be zero.";
    }
    std::cout << "Value is valid: " << value << std::endl;
  } catch (const std::invalid_argument &e) {
    std::cerr << "Caught invalid_argument: " << e.what() << std::endl;
  } catch (const char *msg) {
    std::cerr << "Caught C-style exception: " << msg << std::endl;
  }
}

int main() {
  exception_handler(10);
  exception_handler(-5);
  exception_handler(0);
  return 0;
}
