#include <stdio.h>

int test_for_loop(int n) {
  int sum = 0;
  for (int i = 0; i < n; i++) {
    sum += i;
  }
  return sum;
}

int test_while_loop(int n) {
  int sum = 0;
  int i = 0;
  while (i < n) {
    sum += i;
    i++;
  }
  return sum;
}

int test_do_while(int n) {
  int sum = 0;
  int i = 0;
  do {
    sum += i;
    i++;
  } while (i < n);
  return sum;
}

int main() {
  printf("for_loop(10) = %d\n", test_for_loop(10));
  printf("while_loop(10) = %d\n", test_while_loop(10));
  printf("do_while(10) = %d\n", test_do_while(10));
  return 0;
}
