#include <iostream>
#include <memory>
#include <string>

class Resource {
private:
  std::string name;

public:
  Resource(const std::string &n) : name(n) {
    std::cout << "Resource '" << name << "' created." << std::endl;
  }
  ~Resource() {
    std::cout << "Resource '" << name << "' DESTROYED." << std::endl;
  }
  void use() { std::cout << "Using resource '" << name << "'." << std::endl; }
};

void test_unique_ptr(int value) {
  if (value > 10) {
    std::unique_ptr<Resource> ptr = std::make_unique<Resource>("unique_in_if");
    ptr->use();
  } else {
    std::cout << "Value was 10 or less, no unique_ptr created here."
              << std::endl;
  }
  std::cout << "test_unique_ptr function finished." << std::endl;
}

void use_shared(std::shared_ptr<Resource> ptr) {
  std::cout << "Inside use_shared, use_count: " << ptr.use_count() << std::endl;
  ptr->use();
}

int main() {
  std::cout << "--- Testing unique_ptr ---" << std::endl;
  test_unique_ptr(15);
  test_unique_ptr(5);

  std::cout << "\n--- Testing shared_ptr ---" << std::endl;
  std::shared_ptr<Resource> shared_ptr1;
  {
    auto ptr = std::make_shared<Resource>("shared_resource");
    shared_ptr1 = ptr;
    std::cout << "Inside scope, use_count: " << shared_ptr1.use_count()
              << std::endl;
    use_shared(shared_ptr1);
    std::cout << "Leaving scope..." << std::endl;
  }
  std::cout << "Outside scope, use_count: " << shared_ptr1.use_count()
            << std::endl;

  std::cout << "Main function finished." << std::endl;
  return 0;
}
