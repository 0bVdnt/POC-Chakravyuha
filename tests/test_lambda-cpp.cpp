#include <algorithm>
#include <iostream>
#include <vector>

int main() {
  std::vector<int> numbers = {5, -3, 10, -8, 1, 15};

  int positive_count = 0;
  int negative_sum = 0;

  std::for_each(numbers.begin(), numbers.end(), [&](int n) {
    if (n > 0) {
      positive_count++;
    } else {
      negative_sum += n;
    }
  });

  std::cout << "Positive count: " << positive_count << std::endl;
  std::cout << "Negative sum: " << negative_sum << std::endl;

  std::sort(numbers.begin(), numbers.end(),
            [](int a, int b) { return std::abs(a) < std::abs(b); });

  std::cout << "Sorted by absolute value: ";
  for (int n : numbers) {
    std::cout << n << " ";
  }
  std::cout << std::endl;

  return 0;
}
