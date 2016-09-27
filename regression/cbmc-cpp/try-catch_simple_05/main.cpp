#include <assert.h>

void myfunction () {
  throw 1;
}

int main (void) {
  try {
    myfunction();
  }
  catch (int) { assert(0); }
  catch (float f) { /* float */ }
  return 0;
}
