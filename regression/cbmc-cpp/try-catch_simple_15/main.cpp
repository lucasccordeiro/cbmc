#include <assert.h>
int main() {
  try {
    assert(0);
    throw 2;
    throw 1.1f;
  }
  catch(char e) {
    return 1;
  }
  return 0;
}
