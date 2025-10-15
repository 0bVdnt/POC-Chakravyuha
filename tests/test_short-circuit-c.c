#include <stdio.h>

int side_effect_a(int *counter) {
  (*counter)++;
  return 1;
}

int side_effect_b(int *counter) {
  (*counter)++;
  return 0;
}

int short_circuit_and(int a, int b) {
  int counter = 0;
  if (a && side_effect_a(&counter) && b && side_effect_b(&counter)) {
    return counter + 100;
  }
  return counter;
}

int short_circuit_or(int a, int b) {
  int counter = 0;
  if (a || side_effect_a(&counter) || b || side_effect_b(&counter)) {
    return counter + 100;
  }
  return counter;
}

int main() {
  printf("AND(0,0) = %d\n", short_circuit_and(0, 0));
  printf("AND(1,0) = %d\n", short_circuit_and(1, 0));
  printf("AND(1,1) = %d\n", short_circuit_and(1, 1));
  printf("OR(0,0) = %d\n", short_circuit_or(0, 0));
  printf("OR(1,0) = %d\n", short_circuit_or(1, 0));
  printf("OR(1,1) = %d\n", short_circuit_or(1, 1));
  return 0;
}
