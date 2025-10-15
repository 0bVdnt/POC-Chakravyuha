#include <stdbool.h>
#include <stdio.h>
#include <string.h>

bool validate_license(const char *key) {
  const char *secret_product_code = "CHAKRA-OBFUSCATOR-V1";
  int key_length = strlen(key);
  int secret_length = strlen(secret_product_code);

  if (key_length != secret_length) {
    return false;
  }

  int accumulator = 0;

  for (int i = 0; i < key_length; ++i) {
    int key_char = key[i];
    int secret_char = secret_product_code[i];

    if (i % 3 == 0) {
      accumulator += (key_char ^ secret_char);
    } else if (i % 3 == 1) {
      accumulator -= (key_char - secret_char);
    } else {
      accumulator += (key_char + secret_char) / 2;
    }

    accumulator = (accumulator * 3) & 0xFF;
  }

  return accumulator == 180;
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
