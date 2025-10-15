#include <stdio.h>

int phi_simple(int x, int y) {
  int result;

  if (x > y) {
    result = x - y;
  } else {
    result = y - x;
  }

  return result * 2;
}

int phi_loop(int n) {
  int sum = 0;
  int i = 0;

  while (i < n) {
    if (i % 2 == 0) {
      sum += i;
    } else {
      sum += i * 2;
    }
    i++;
  }

  return sum;
}

int phi_complex(int a, int b, int c) {
  int x, y, z;

  if (a > 0) {
    x = a * 2;
    y = b + 10;
    z = c - 5;
  } else if (b > 0) {
    x = a + 5;
    y = b * 2;
    z = c + 10;
  } else {
    x = a - 10;
    y = b - 5;
    z = c * 2;
  }

  int result = x + y + z;

  if (result > 100) {
    result = 100;
  } else if (result < 0) {
    result = 0;
  }

  return result;
}

int main() {
  printf("PHI simple(10, 5): %d\n", phi_simple(10, 5));
  printf("PHI loop(10): %d\n", phi_loop(10));
  printf("PHI complex(10, 20, 30): %d\n", phi_complex(10, 20, 30));
  printf("PHI complex(-5, 10, 20): %d\n", phi_complex(-5, 10, 20));
  return 0;
}
