#include <iostream>
#include <map>
#include <string>
#include <vector>

int process_vector(const std::vector<int> &vec) {
  int sum = 0;
  for (int x : vec) {
    if (x % 2 == 0)
      sum += x;
  }
  return sum;
}

void process_map(const std::map<std::string, std::string> &m) {
  std::string key = "secret_key";
  auto it = m.find(key);
  if (it != m.end()) {
    std::cout << "Found key '" << it->first << "'. Value: " << it->second
              << std::endl;
  } else {
    std::cout << "Key '" << key << "' not found." << std::endl;
  }
}

int main() {
  std::vector<int> data = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
  std::cout << "Sum of even numbers: " << process_vector(data) << std::endl;

  std::map<std::string, std::string> secrets = {{"public_key", "value_abc"},
                                                {"secret_key", "value_xyz"},
                                                {"user_id", "user_123"}};
  process_map(secrets);

  return 0;
}
