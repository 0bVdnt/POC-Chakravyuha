#include <iostream>
#include <string>
#include <vector>

class Buffer {
private:
  std::string name;
  int *data;

public:
  // Constructor
  Buffer(const std::string &n) : name(n), data(new int[10]) {
    std::cout << "Constructor for " << name << std::endl;
  }

  // Destructor
  ~Buffer() {
    delete[] data;
    std::cout << "Destructor for " << name << std::endl;
  }

  // Copy Constructor (disabled)
  Buffer(const Buffer &) = delete;

  // Move Constructor
  Buffer(Buffer &&other) noexcept
      : name(std::move(other.name)), data(other.data) {
    // This is a new function the obfuscator will see.
    // Let's add control flow to test if it gets flattened.
    if (data) {
      std::cout << "Move constructor for " << name << " (successful)."
                << std::endl;
    } else {
      std::cout << "Move constructor for " << name << " (from empty source)."
                << std::endl;
    }
    other.data = nullptr; // Invalidate the source
  }
};

std::vector<Buffer> create_buffers() {
  std::vector<Buffer> buffers;
  // Emplace_back will construct a Buffer in place, then the vector might move
  // it.
  buffers.emplace_back("Buffer_A");
  buffers.emplace_back("Buffer_B");

  std::cout << "Returning vector from function (will trigger move)..."
            << std::endl;
  return buffers; // This triggers a move, not a copy (due to RVO/NRVO)
}

int main() {
  std::cout << "--- Testing Move Semantics ---" << std::endl;
  std::vector<Buffer> my_buffers = create_buffers();
  std::cout << "Buffers received in main." << std::endl;

  // The Buffer destructors will be called when my_buffers goes out of scope
  // here.
  return 0;
}
