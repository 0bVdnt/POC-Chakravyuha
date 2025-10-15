#include <stdio.h>
#include <stdlib.h>

const char *greeting = "Hello from example.c!";
const char *info = "Compute the special value for n=";

int compute_special(int n) {
  int acc = 0;
  for (int i = 1; i <= n; ++i) {
    if (i % 2 == 0) {
      acc += i * 2;
    } else if (i % 3 == 0) {
      acc += i * 3;
    } else {
      acc += i;
    }
  }
  return acc;
}

typedef int (*op_fn)(int);

int op_add(int x) { return x + 1; }
int op_mul2(int x) { return x * 2; }
int op_square(int x) { return x * x; }

int dispatch_op(int opcode, int value) {
  op_fn table[3];
  table[0] = op_add;
  table[1] = op_mul2;
  table[2] = op_square;

  if (opcode < 0 || opcode > 2)
    return value;
  return table[opcode](value);
}

void unused_helper_print(void) {
  puts("This helper is not used by main logic.");
}

int unused_calc(int x) {
  int r = 1;
  for (int i = 1; i < x; ++i)
    r = (r + i) % (i + 7);
  return r;
}

void no_op_side_effect(void) {
  /* pure no-op placeholder */
  volatile int v = 0;
  v++;
  (void)v;
}

int main(int argc, char **argv) {
  (void)argc;
  int n = 10;
  if (argc > 1) {
    n = atoi(argv[1]);
    if (n <= 0)
      n = 10;
  }

  puts(greeting);

  char buf[128];
  snprintf(buf, sizeof(buf), "%s %d", info, n);
  puts(buf);

  int value = compute_special(n);
  printf("special(%d) = %d\n", n, value);

  for (int opcode = 0; opcode < 3; ++opcode) {
    int out = dispatch_op(opcode, value % 10 + 1);
    printf("dispatch(%d) -> %d\n", opcode, out);
  }

  if (value % 2 == 0) {
    puts("Result is even.");
  } else {
    puts("Result is odd.");
  }

  return 0;
}
