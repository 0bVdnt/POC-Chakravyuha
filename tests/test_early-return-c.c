#include <stdio.h>

int early_returns(int x, int y) {
  if (x < 0) {
    return -1;
  }

  if (y < 0) {
    return -2;
  }

  if (x == 0) {
    return 0;
  }

  if (y == 0) {
    return 1;
  }

  int result = x + y;

  if (result > 100) {
    return 100;
  }

  return result;
}

int main() {
  printf("early_returns(-5, 10) = %d\n", early_returns(-5, 10));
  printf("early_returns(5, -10) = %d\n", early_returns(5, -10));
  printf("early_returns(0, 10) = %d\n", early_returns(0, 10));
  printf("early_returns(5, 0) = %d\n", early_returns(5, 0));
  printf("early_returns(50, 60) = %d\n", early_returns(50, 60));
  printf("early_returns(20, 30) = %d\n", early_returns(20, 30));
  return 0;
}
