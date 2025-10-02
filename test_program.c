#include <stdio.h>
// A simple function that contains a critical data string.
void returnPassword() { printf("SUPER_SECRET_STRING"); }

int main() {
  returnPassword();
  return 0;
}
