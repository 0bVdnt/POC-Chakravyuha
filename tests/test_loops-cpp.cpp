#include <iostream>
#include <string>
#include <vector>

int test_range_based_for(const std::vector<int> &data) {
  int sum = 0;
  for (int x : data) {
    sum += x;
  }
  return sum;
}

int test_iterator_loop(const std::string &str) {
  int count = 0;
  for (auto it = str.begin(); it != str.end(); ++it) {
    if (*it == 'a' || *it == 'e' || *it == 'i' || *it == 'o' || *it == 'u') {
      count++;
    }
  }
  return count;
}

int main() {
  std::vector<int> numbers = {10, 20, 30, 40};
  std::cout << "Range-based for sum: " << test_range_based_for(numbers)
            << std::endl;

  std::string text = "a simple string for testing iterators";
  std::cout << "Iterator loop vowel count: " << test_iterator_loop(text)
            << std::endl;

  return 0;
}
