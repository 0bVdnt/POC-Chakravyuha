#include <algorithm>
#include <functional>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

static const std::string HELLO = "Hello from example.cpp";
static const std::string PROMPT = "Processing value = ";

class Processor {
public:
  Processor(int v) : value(v) {}
  virtual ~Processor() = default;

  virtual int step() {
    int r = value;
    if (r % 5 == 0)
      r += transform(r);
    else if (r % 3 == 0)
      r -= transform(r);
    else
      r ^= transform(r);
    return r;
  }

  virtual int transform(int x) { return (x * 13 + 7) % 97; }

  int getValue() const { return value; }

private:
  int value;
};

class FancyProcessor : public Processor {
public:
  FancyProcessor(int v) : Processor(v) {}
  int step() override {
    int r = getValue();
    try {
      if (r < 0)
        throw std::runtime_error("negative input");
      auto f = [r](int a) { return (a + r) / 2; };
      return f(transform(r));
    } catch (...) {
      return 0;
    }
  }
};

void dead_function_a() {
  volatile long long s = 0;
  for (int i = 1; i < 1000; ++i)
    s += (i * i) % (i + 3);
  (void)s;
}

int dead_function_b(int x) {
  int res = 1;
  for (int i = 2; i < 20; ++i)
    res = (res * x + i) % 10007;
  return res;
}

void trivial_noop() { asm(""); }

std::string make_message(const std::string &base, int v) {
  return base + std::to_string(v);
}

int main(int argc, char **argv) {
  (void)argc;
  int v = 7;
  if (argc > 1) {
    try {
      v = std::stoi(argv[1]);
    } catch (...) {
      v = 7;
    }
  }

  std::cout << HELLO << std::endl;
  std::cout << make_message(PROMPT, v) << std::endl;

  std::vector<std::unique_ptr<Processor>> list;
  list.emplace_back(std::make_unique<Processor>(v));
  list.emplace_back(std::make_unique<FancyProcessor>(v * 2));
  list.emplace_back(std::make_unique<Processor>(v + 3));

  for (auto &p : list) {
    int out = p->step();
    std::cout << "processor -> " << out << std::endl;
  }

  std::vector<int> arr = {1, 2, 3, 4, 5, 6};
  std::transform(arr.begin(), arr.end(), arr.begin(),
                 [](int x) { return x * x + 1; });

  for (int x : arr) {
    if (x % 2 == 0)
      std::cout << x << " even\n";
    else
      std::cout << x << " odd\n";
  }

  trivial_noop();

  return 0;
}
