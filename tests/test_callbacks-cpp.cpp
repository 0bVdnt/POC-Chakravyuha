#include <functional>
#include <iostream>
#include <string>

class Processor {
private:
  std::function<void(const std::string &)> callback;

public:
  Processor(std::function<void(const std::string &)> cb) : callback(cb) {}

  void run(const std::string &data) {
    if (callback) {
      callback(data);
    } else {
      std::cout << "No callback registered." << std::endl;
    }
  }
};

void print_uppercase(const std::string &s) {
  std::string result;
  for (char c : s) {
    result += std::toupper(c);
  }
  std::cout << "UPPERCASE: " << result << std::endl;
}

void print_reverse(const std::string &s) {
  std::string result(s.rbegin(), s.rend());
  std::cout << "REVERSED: " << result << std::endl;
}

int main() {
  std::string my_data = "CallbackTest";

  Processor p1(print_uppercase);
  Processor p2(print_reverse);

  Processor p3([](const std::string &s) {
    std::cout << "LAMBDA: " << s.length() << " characters long." << std::endl;
  });

  int choice = 2;

  switch (choice) {
  case 1:
    std::cout << "Running processor 1..." << std::endl;
    p1.run(my_data);
    break;
  case 2:
    std::cout << "Running processor 2..." << std::endl;
    p2.run(my_data);
    break;
  case 3:
    std::cout << "Running processor 3..." << std::endl;
    p3.run(my_data);
    break;
  default:
    std::cout << "Invalid choice." << std::endl;
    break;
  }

  return 0;
}
