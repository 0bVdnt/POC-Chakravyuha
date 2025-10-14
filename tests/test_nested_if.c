#include <stdio.h>

int nested_conditions(int a, int b) {
  int result = 0;
  if (a > 0) {
    if (b > 0) {
      result = a + b;
    } else {
      result = a - b;
    }
  } else {
    if (b > 0) {
      result = b - a;
    } else {
      result = -a - b;
    }
  }
  return result;
}

int main() {
  printf("nested(5, 3) = %d\n", nested_conditions(5, 3));
  printf("nested(5, -3) = %d\n", nested_conditions(5, -3));
  printf("nested(-5, 3) = %d\n", nested_conditions(-5, 3));
  printf("nested(-5, -3) = %d\n", nested_conditions(-5, -3));
  return 0;
}
