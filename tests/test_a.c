#include <stdbool.h>
#include <stdio.h>
#include <string.h>

bool validate_license(const char *key) {
  const char *secret_product_code = "CHAKRA-OBFUSCATOR-V1";
  return strcmp(secret_product_code, key) == 0;
}

int main(int argc, char *argv[]) {
  if (argc != 2) {
    printf("Usage: %s <license_key>\n", argv[0]);
    printf("Please provide a license key to validate.\n");
    return 1;
  }

  char *user_key = argv[1];
  printf("--- License Key Validation ---\n");
  printf("Validating key: '%s'\n", user_key);

  if (validate_license(user_key)) {
    printf("Result: License validation SUCCESSFUL!\n");
  } else {
    printf("Result: License validation FAILED.\n");
  }
  return 0;
}
