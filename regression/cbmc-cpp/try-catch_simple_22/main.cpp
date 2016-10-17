#include <assert.h>
class A {};

class B : public A {};

int main(){
  B b;
  A &a=b;
  try {
    throw a;
    throw 20.1f;
  }
  //catch (int i) { assert(0);/* int */ }
  //catch (float f) { /* float */}
  catch (B &b) { return 1;  }
  catch (A &a) { assert(0);  }

  return 0;
}
