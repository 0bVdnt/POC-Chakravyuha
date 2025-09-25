#include <stdio.h>

int add_numbers(int a, int b) {
    return a + b;
}

int main() {
    int x = 10;
    int y = 20;
    int result = add_numbers(x, y);
    
    printf("Hello World!\n");
    printf("Result: %d\n", result);
    
    return 0;
}