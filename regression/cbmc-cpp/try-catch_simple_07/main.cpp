#include <assert.h>

void throwException()
{
  int numerador = 2;
  int denominador = 0;

  try {
    if (denominador == 0)
      throw 1;
  }
  catch ( int ) {
    denominador = 1;
  }

  int result = numerador/denominador;
  assert(result==2);
}

int main()
{
  throwException();
  return 0;
}
