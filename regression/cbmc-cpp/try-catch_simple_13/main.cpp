#include <assert.h>

int main()
{
  try {
    throw 1.1f;
    assert(0);
  }
  catch(int e) {
    return 1;
  }
  return 0;
}
