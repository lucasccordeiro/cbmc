#include <assert.h>

int main(){

  try {
    throw 20.1f;
  }

  catch (int i) { assert(0);/* int */ }
  catch (float f) {/* float */}

  return 0;
}
