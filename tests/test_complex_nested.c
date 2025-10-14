#include <stdio.h>

int complex_control_flow(int x, int y, int z) {
  int result = 0;

  for (int i = 0; i < x; i++) {
    if (i % 2 == 0) {
      for (int j = 0; j < y; j++) {
        if (j % 3 == 0) {
          result += i * j;
        } else {
          result += i + j;
        }
      }
    } else {
      int k = 0;
      while (k < z) {
        result += k;
        k += 2;
      }
    }
  }

  if (result > 100) {
    result = result % 100;
  } else if (result > 50) {
    result = result * 2;
  } else {
    result = result + 50;
  }

  return result;
}

int main() {
  printf("complex(5, 4, 3) = %d\n", complex_control_flow(5, 4, 3));
  printf("complex(3, 3, 3) = %d\n", complex_control_flow(3, 3, 3));
  printf("complex(10, 5, 2) = %d\n", complex_control_flow(10, 5, 2));
  return 0;
}
