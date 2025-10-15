#include <stdio.h>
int test_function(int a) {
  int x = 0;
  if (a > 10) {
    for (int i = 0; i < a; ++i) {
      printf("%d\n", x += i);
    }
  } else {
    x = -1;
  }
  return x;
}

int main() { printf("%d", test_function(100)); }
