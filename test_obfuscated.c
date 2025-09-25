#include <stdio.h>

int add_numbers(int a, int b) {
    return a + b;
}


// === BOGUS FUNCTIONS ===

static int _bogus_func_2199(int x) {
    int result = x;
    for(int i = 0; i < 5; i++) {
        result = result * 2 + 1;
        result = result % 1000;
    }
    return result;
}

static int _bogus_func_1997(int x) {
    int result = x;
    for(int i = 0; i < 5; i++) {
        result = result * 2 + 1;
        result = result % 1000;
    }
    return result;
}

int main() {
    // Fake calls to confuse analysis
    volatile int _fake_result = 0;
    _fake_result += _bogus_func_2199(4);
    _fake_result += _bogus_func_1997(3);

    int x = 10;
    int y = 20;
    int result = add_numbers(x, y);
    
    printf("Hello World!\n");
    printf("Result: %d\n", result);
    
    return 0;
}