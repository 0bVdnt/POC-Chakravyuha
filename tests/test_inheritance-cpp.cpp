#include <iostream>
#include <string>

class Animal {
public:
  virtual std::string makeSound() { return "Generic animal sound"; }
  virtual ~Animal() {}
};

class Dog : public Animal {
public:
  std::string makeSound() override { return "Woof!"; }
};

class Cat : public Animal {
public:
  std::string makeSound() override { return "Meow!"; }
};

void printSound(Animal *animal) {
  if (animal) {
    std::cout << "The animal says: " << animal->makeSound() << std::endl;
  }
}

int main() {
  Dog myDog;
  Cat myCat;
  Animal genericAnimal;

  printSound(&myDog);
  printSound(&myCat);
  printSound(&genericAnimal);

  return 0;
}
