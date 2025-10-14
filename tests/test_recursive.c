// test_recursive.c - Recursive functions (should be skipped by pass)
#include <stdio.h>

int factorial(int n) {
  if (n <= 1)
    return 1;
  return n * factorial(n - 1);
}

int fibonacci(int n) {
  if (n <= 1)
    return n;
  return fibonacci(n - 1) + fibonacci(n - 2);
}

int ackermann(int m, int n) {
  if (m == 0)
    return n + 1;
  if (n == 0)
    return ackermann(m - 1, 1);
  return ackermann(m - 1, ackermann(m, n - 1));
}

// Mutual recursion
int is_even(int n);
int is_odd(int n);

int is_even(int n) {
  if (n == 0)
    return 1;
  return is_odd(n - 1);
}

int is_odd(int n) {
  if (n == 0)
    return 0;
  return is_even(n - 1);
}

int main() {
  printf("Factorial(5): %d\n", factorial(5));
  printf("Fibonacci(10): %d\n", fibonacci(10));
  printf("Ackermann(2,3): %d\n", ackermann(2, 3));
  printf("Is even(10): %d\n", is_even(10));
  printf("Is odd(10): %d\n", is_odd(10));
  return 0;
}
