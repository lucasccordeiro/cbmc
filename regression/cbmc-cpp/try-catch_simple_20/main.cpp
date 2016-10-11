#include <assert.h>

int main (void) {
  try {
    throw 5;
    goto ab;
  }
  catch (float) { assert(0); }

ab: assert(0);
}

