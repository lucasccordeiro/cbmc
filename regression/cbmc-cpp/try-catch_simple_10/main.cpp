#include <assert.h>

int main (void) {
  try {
    char a='a';
    throw a;   // throws char
  }
  catch (int) { return 1; }
  catch (char) { assert(0); return 2; }
  return 0;
}