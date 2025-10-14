// test_switch.c - Switch statement handling
#include <stdio.h>

int switch_basic(int option) {
  int result = 0;

  switch (option) {
  case 1:
    result = 100;
    printf("Option 1 selected\n");
    break;
  case 2:
    result = 200;
    printf("Option 2 selected\n");
    break;
  case 3:
    result = 300;
    printf("Option 3 selected\n");
    break;
  case 4:
  case 5:
    result = 500;
    printf("Option 4 or 5 selected\n");
    break;
  default:
    result = -1;
    printf("Invalid option\n");
    break;
  }

  return result;
}

int switch_fallthrough(int value) {
  int score = 0;

  switch (value) {
  case 10:
    score += 10;
    // fallthrough
  case 9:
    score += 9;
    // fallthrough
  case 8:
    score += 8;
    break;
  case 7:
  case 6:
    score = value * 10;
    break;
  default:
    score = value;
  }

  return score;
}

int nested_switch(int x, int y) {
  int result = 0;

  switch (x) {
  case 1:
    switch (y) {
    case 1:
      result = 11;
      break;
    case 2:
      result = 12;
      break;
    default:
      result = 10;
      break;
    }
    break;
  case 2:
    switch (y) {
    case 1:
      result = 21;
      break;
    case 2:
      result = 22;
      break;
    default:
      result = 20;
      break;
    }
    break;
  default:
    result = 0;
  }

  return result;
}

int main() {
  printf("Basic switch(2): %d\n", switch_basic(2));
  printf("Basic switch(10): %d\n", switch_basic(10));
  printf("Fallthrough switch(10): %d\n", switch_fallthrough(10));
  printf("Fallthrough switch(7): %d\n", switch_fallthrough(7));
  printf("Nested switch(1,2): %d\n", nested_switch(1, 2));
  return 0;
}
