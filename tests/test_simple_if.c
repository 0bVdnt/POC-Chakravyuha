#include <stdio.h>

int simple_if(int x) {
  int result = 0;
  if (x > 10) {
    result = x * 2;
  } else {
    result = x + 5;
  }
  return result;
}

int main() {
  printf("simple_if(5) = %d\n", simple_if(5));
  printf("simple_if(15) = %d\n", simple_if(15));
  return 0;
}
