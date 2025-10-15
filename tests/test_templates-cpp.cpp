#include <iostream>
#include <string>

template <typename T> T process_value(T a, T b) {
  if (a > b) {
    return a;
  } else {
    return b + a;
  }
}

int main() {
  int int_result = process_value(10, 20);
  std::cout << "Int result: " << int_result << std::endl;

  double double_result = process_value(15.5, 9.2);
  std::cout << "Double result: " << double_result << std::endl;

  std::string s1 = "alpha";
  std::string s2 = "beta";
  std::string str_result = process_value(s1, s2);
  std::cout << "String result: " << str_result << std::endl;

  return 0;
}
