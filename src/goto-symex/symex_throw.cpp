/*******************************************************************\

Module: Symbolic Execution

Author: Daniel Kroening, kroening@kroening.com

\*******************************************************************/

#include <iostream>
#include "goto_symex.h"

/*******************************************************************\

Function: goto_symext::symex_throw

  Inputs:

 Outputs:

 Purpose:

\*******************************************************************/

bool goto_symext::symex_throw(statet &state)
{
  //this is used to debug the throw expression
  const goto_programt::const_targett *catch_insn = NULL;

  const goto_programt::instructiont &instruction=*state.source.pc;

  assert(instruction.code.operands().size()==1);

  // get the list of exceptions thrown
  const irept::subt &exceptions_thrown=
    instruction.code.op0().find(ID_exception_list).get_sub();

  // Log
  std::cout << "*** Exception thrown of type "
    << exceptions_thrown.begin()->id_string()
    << " at file " << instruction.source_location.get_file()
    << " line " << instruction.source_location.get_line() << std::endl;

  // Get the list of catchs
  goto_symex_statet::exceptiont* except=&stack_catch.top();

  for (std::vector<irept>::const_iterator it = exceptions_thrown.begin();\
       it != exceptions_thrown.end(); it++)
  {
    // Search for a catch with a matching type
    goto_symex_statet::exceptiont::catch_mapt::const_iterator
      c_it=except->catch_map.find(it->id());

    // Do we have a catch for it?
    if(c_it!=except->catch_map.end())
    {
      // make sure that these are always forward gotos
      assert(!instruction.is_backwards_goto());
      target.goto_instruction(state.guard.as_expr(), true_exprt(), state.source);
      state.source.pc=c_it->second;

      // for debug purpose only
      catch_insn = &c_it->second;

      if (catch_insn == NULL)
      {
        // Log
        std::cout << "*** Throwing an exception of type " +
          exceptions_thrown.begin()->id_string() + " but there is not catch for it." << std::endl;
        return false;
      }

      // Log
      std::cout << "*** Caught by catch("
        << c_it->first << ") at file "
        << (*catch_insn)->source_location.get_file()
        << " line " << (*catch_insn)->source_location.get_line() << std::endl;

      return true;
    }
    else // We don't have a catch for it yet
    {
      // An un-caught exception.
      const std::string &msg="Throwing an exception of type " +
            exceptions_thrown.begin()->id_string() +
            " but there is not catch for it.";

      // Generate an assertion to fail
      vcc(false_exprt(), msg, state);

      // Behaves like assume(0);
      symex_assume(state, false_exprt());
    }
  }

  return false;
}
