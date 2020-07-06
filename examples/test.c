#include <stdio.h>
#include "../headers/litterbin.h"

int call_method(int *arr) {
    int i;
    for(i = 0; i < 10; i++) {
        arr[i] = i + 5;
    }

    return arr[5];
}

int main(void) {
    int *arr = (int*)mmalloc(sizeof(int) * 10);
    
    int value = call_method(arr);
    printf("value: %d\n", value);
    
    return 0;
}
