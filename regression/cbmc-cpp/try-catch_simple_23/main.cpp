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
  catch (B &b) { assert(0);  }
  catch (A &a) { return 2;  }

  return 0;
}
