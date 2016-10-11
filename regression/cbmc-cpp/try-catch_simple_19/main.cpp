#include <assert.h>
int main() {
  try {
    throw 2;
    assert(0);
    throw 1.1f;
    assert(0);
  }
  catch(char e) {
    assert(0);
    return 1; 
 }
  assert(0);
  return 0;
}
