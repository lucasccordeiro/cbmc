#include <assert.h>
int main() {
  try {
    throw 2;
    throw 1.1f;
//    assert(0);
  }
  catch(char e) {
    return 1;
  }
  return 0;
}
