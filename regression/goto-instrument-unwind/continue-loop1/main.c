
void f() {}

int main()
{
  /**
   * This is a test case for the unwind operation of goto-instrument;
   * every loop will be unwound K times
   **/
  const unsigned K=10;
  
  const unsigned n=100;
  unsigned c=0, i;
  unsigned tres=K/2;;

  for(i=1; i<=n; i++)
  {
    f();
    if(i>tres)
      continue;
    c++;
  }

  unsigned eva=n;
  if(K<eva) eva=K;
  if(tres<eva) eva=tres;
  
  __CPROVER_assert(c==eva, "continue in a loop unwind (1)");

}
