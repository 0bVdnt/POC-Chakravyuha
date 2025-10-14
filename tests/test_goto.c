#include <stdio.h>

int function_with_goto(int x) {
  int result = 0;

  if (x < 0) {
    goto negative;
  }

  result = x * 2;
  goto end;

negative:
  result = -x;

end:
  return result;
}

int main() {
  printf("goto_test(5) = %d\n", function_with_goto(5));
  printf("goto_test(-5) = %d\n", function_with_goto(-5));
  return 0;
}
