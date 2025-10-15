#include <iostream>
#include <string>

class Greeter {
private:
  std::string greeting;

public:
  Greeter(const std::string &msg) : greeting(msg) {}

  void sayHello(const std::string &name) {
    if (!name.empty()) {
      std::cout << greeting << ", " << name << "!" << std::endl;
    } else {
      std::cout << "Hello, anonymous user!" << std::endl;
    }
  }
};

int main() {
  Greeter g("Welcome to the test suite");
  g.sayHello("Alice");
  g.sayHello("");
  return 0;
}
