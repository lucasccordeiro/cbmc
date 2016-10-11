/*******************************************************************\

Module: Symbolic Execution

Author: Daniel Kroening, kroening@kroening.com

\*******************************************************************/

#include <iostream>
#include "goto_symex.h"

/*******************************************************************\

Function: goto_symext::symex_catch

  Inputs:

 Outputs:

 Purpose:

\*******************************************************************/

bool goto_symext::symex_catch(statet &state)
{
  // there are two variants: 'push' and 'pop'
  const goto_programt::instructiont &instruction=*state.source.pc;

  if(instruction.targets.empty()) // pop
  {
    if(stack_catch.empty())
      throw "catch-pop on empty call stack";

    // Copy the exception before pop
    goto_symex_statet::exceptiont exception=stack_catch.top();

    // Pop from the stack
    stack_catch.pop();

    if (exception.get_uncaught_exception())
    {
      // An un-caught exception.
      const std::string &msg="Throwing exception of type " +
    		exception.get_uncaught_exception_list() + ">" +
            " but there is not catch for it.";

      //set_uncaught_exception_msg(msg);
      set_uncaught_exception(true,msg);
      return true;
    }
    else if (get_uncaught_exception())
    {
      assert(!instruction.is_backwards_goto());
      target.goto_instruction(state.guard.as_expr(), true_exprt(), state.source);
      state.source.pc=exception.catch_map.begin()->second;
      return false;
    }
  }
  else // push
  {
    goto_symex_statet::exceptiont exception;

    // copy targets
    const irept::subt &exception_list=
      instruction.code.find(ID_exception_list).get_sub();

    assert(exception_list.size()==instruction.targets.size());

    // Fill the map with the catch type and the target
    unsigned i=0;
    for(goto_programt::targetst::const_iterator
        it=instruction.targets.begin();
        it!=instruction.targets.end();
        it++, i++)
    {
      // Log
      std::cout << "*** Exception " << exception_list[i].id()
                << " at target " << (*it)->target_number << std::endl;

      exception.catch_map[exception_list[i].id()]=*it;
    }

    // Stack it
    stack_catch.push(exception);
  }

  return true;
}

/*******************************************************************\

Function: goto_symext::check_uncaught_exception

  Inputs:

 Outputs:

 Purpose:

\*******************************************************************/

bool goto_symext::check_all_uncaught_exceptions(statet &state, std::string &msg)
{
  // Do we have an un-caught exception?
  if (get_uncaught_exception())
  {
	// Do we have an assertion related to an un-caught exception?
	if (msg.compare("un-caught exception"))
	{
	  // This assertion is located after an un-caught exception
	  state.source.pc++;
	  return true;
	}
    // Update the message with all un-caught exceptions
	msg=get_uncaught_exception_msg();
  }

  return false;
}

