#include <stdio.h>
#include "../export/cLitterBin.h"

/* 'bin' is the global element used to save stuff */
/* We declare it out of the users scope so that we dont need to
    pass it as a parameter due to it being sort of a singleton */
struct litterbin bin;

int call_method(int *arr) {
    int i;
    for(i = 0; i < 10; i++) {
        arr[i] = i + 5;
    }

    return arr[5];
}

int main(void) {
    void *dummy = NULL;
    litterbin_new(&bin, dummy);

        int *arr = (int*)mmalloc(sizeof(int) * 10);
    
        int value = call_method(arr);
        printf("value: %d\n", value);

    litterbin_terminate(&bin);
    return 0;
}
