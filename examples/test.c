#include "../export/EmeraldsCollector.h" /* IWYU pragma: keep */

#include <stdio.h>

/* 'gc' is the global element used to save stuff */
/* We declare it out of the users scope so that we dont need to
    pass it as a parameter due to it being sort of a singleton */
EmeraldsCollector gc;

int call_method(int *arr) {
  int i;
  for(i = 0; i < 10; i++) {
    arr[i] = i + 5;
  }

  return arr[5];
}

int main(void) {
  void *dummy = NULL;
  int *arr    = NULL;
  int value;

  collector_new(&gc, dummy);

  arr = (int *)mmalloc(sizeof(int) * 10);

  value = call_method(arr);
  printf("value: %d\n", value);

  collector_terminate(&gc);
  return 0;
}
