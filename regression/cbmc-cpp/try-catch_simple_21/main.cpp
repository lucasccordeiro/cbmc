#include <assert.h>
bool nondet_bool();
int main() {
  bool a=0;//nondet_bool();
  try {
    if (a)
      throw 2;
    else
      assert(0);
  }
  catch(char e) {
    return 1; 
 }
  return 0;
}
