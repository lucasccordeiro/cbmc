#include <assert.h>

void myfunction () {
  throw 1.1f;
}

int main (void) {
  try {
    myfunction();
  }
  catch (int) { /* int */ }
  catch (float f) { assert(0); }
  return 0;
}
