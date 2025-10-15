#include <stdio.h>

int func_a(int x) {
  if (x > 0) {
    return x * 2;
  }
  return x - 1;
}

int func_b(int x, int y) {
  int result = 0;
  for (int i = 0; i < x; i++) {
    if (i % 2 == 0) {
      result += func_a(y);
    } else {
      result -= func_a(y);
    }
  }
  return result;
}

int func_c(int n) {
  int a = 1, b = 1, c;
  if (n <= 2)
    return 1;

  for (int i = 3; i <= n; i++) {
    c = a + b;
    a = b;
    b = c;
  }
  return b;
}

int main() {
  printf("func_a(5) = %d\n", func_a(5));
  printf("func_a(-5) = %d\n", func_a(-5));
  printf("func_b(5, 3) = %d\n", func_b(5, 3));
  printf("func_c(10) = %d\n", func_c(10));
  return 0;
}
