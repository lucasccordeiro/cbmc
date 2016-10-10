#include <assert.h>

int main()
{
  try {
    int x=10;
    int *array;
    array=&x;
    throw array;
  }
  catch(int *) { assert(0); }
  return 0;
}
