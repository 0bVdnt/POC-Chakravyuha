#include <stdio.h>

int main() {
  // This is the secret I want to hide
  const char *my_secret = "This is a super secret password!";

  printf("The secret is: %s\n", my_secret);

  return 0;
}
