#include <assert.h>
int main() {
  try {
    throw 2;
    assert(0);
    throw 1.1f;
  }
  catch(char e) {
    return 1;
  }
  return 0;
}
