#include <assert.h>

void MyFunc( void );

class CTest {
public:
  CTest() {test=0;};
  ~CTest() {};
  const char *ShowReason() const {
    return "Exception in CTest class.";
  }

private:
  int test;
};

class CDtorDemo {
public:
  CDtorDemo() {};
  ~CDtorDemo() {};
};

void MyFunc() {
  CDtorDemo D;
  throw CTest();
}

int main() {
  try {
    MyFunc();
  }
  catch( CTest E ) { assert(0); }
  catch( char *str ) { /* str */ }

}
