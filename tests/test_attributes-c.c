#include <setjmp.h>
#include <stdio.h>

int with_inline_asm(int x) {
  int result;
#if defined(__x86_64__) || defined(__i386__)
  __asm__ volatile("movl %1, %0\n\t"
                   "addl $10, %0"
                   : "=r"(result)
                   : "r"(x));
#else
  result = x + 10;
#endif
  return result;
}

__attribute__((always_inline)) inline int always_inline_func(int x) {
  return x * 2;
}

__attribute__((noinline)) int no_inline_func(int x) { return x + 10; }

int volatile_operations(volatile int *ptr) {
  volatile int temp = *ptr;
  temp = temp * 2;
  *ptr = temp;
  return temp;
}

jmp_buf jump_buffer;

int setjmp_test(int x) {
  int val = setjmp(jump_buffer);

  if (val == 0) {
    printf("First time through\n");
    if (x > 10) {
      longjmp(jump_buffer, 1);
    }
    return x;
  } else {
    printf("Jumped back\n");
    return -x;
  }
}

void run_safe_tests() {
  printf("Always inline(10): %d\n", always_inline_func(10));
  printf("No inline(20): %d\n", no_inline_func(20));

  volatile int val = 5;
  printf("Volatile ops: %d\n", volatile_operations(&val));
}

int main() {
  printf("Inline asm(5): %d\n", with_inline_asm(5));
  printf("Setjmp test(15): %d\n", setjmp_test(15));
  printf("Setjmp test(5): %d\n", setjmp_test(5));

  run_safe_tests();

  return 0;
}
