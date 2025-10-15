#include <stdio.h>

int complex_control_flow(int x, int y, int z) {
  int result = 0;
  int temp = 0;
  if (x > 0) {
    goto middle;
  }

  for (int i = 0; i < 10; i++) {
    temp += i;

  middle:
    if (y > 5) {
      temp *= 2;

      if (z > 10) {
        result = temp + z;
        goto end;
      }
    } else {
      temp -= 1;
    }

    if (temp > 100) {
      break;
    }
  }

  result = temp;

end:
  return result;
}

int multiple_returns(int a, int b, int c) {
  if (a < 0) {
    return -1;
  }

  if (b < 0) {
    if (c < 0) {
      return -2;
    }
    return -3;
  }

  for (int i = 0; i < a; i++) {
    if (i > b) {
      return i;
    }

    if (i == c) {
      return c * 2;
    }
  }

  return a + b + c;
}

int short_circuit_evaluation(int x, int y, int z) {
  int result = 0;

  if (x > 0 && y > 0 && z > 0) {
    result = x + y + z;
  } else if (x > 0 || y > 0 || z > 0) {
    result = (x > 0 ? x : 0) + (y > 0 ? y : 0) + (z > 0 ? z : 0);
  } else {
    result = -1;
  }

  result = (result > 100) ? 100 : (result < 0) ? 0 : result;

  return result;
}

int main() {
  printf("Complex control flow(5,6,11): %d\n", complex_control_flow(5, 6, 11));
  printf("Complex control flow(0,4,5): %d\n", complex_control_flow(0, 4, 5));
  printf("Multiple returns(10,5,7): %d\n", multiple_returns(10, 5, 7));
  printf("Multiple returns(-1,5,7): %d\n", multiple_returns(-1, 5, 7));
  printf("Short circuit(10,20,30): %d\n", short_circuit_evaluation(10, 20, 30));
  return 0;
}
