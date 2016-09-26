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

void goto_symext::symex_throw(statet &state)
{
  //these are used to debug the throw expression
  irep_idt catch_name = "missing";
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
      //make sure that these are always forward gotos
      assert(!instruction.is_backwards_goto());
      update_throw_target(state, c_it->second);

      // for debug purpose only
      catch_insn = &c_it->second;
      catch_name = c_it->first;

      // Log
      std::cout << "*** Caught by catch("
        << catch_name << ") at file "
        << (*catch_insn)->source_location.get_file()
        << " line " << (*catch_insn)->source_location.get_line() << std::endl;
    }
    else // We don't have a catch for it
      assert(0);
  }
}

/*******************************************************************\

Function: goto_symext::update_throw_target

  Inputs:

 Outputs:

 Purpose:

\*******************************************************************/

void goto_symext::update_throw_target(statet &state,
                                      const goto_programt::const_targett throw_target)
{
  target.goto_instruction(state.guard.as_expr(), true_exprt(), state.source);
  goto_programt::const_targett new_state_pc, state_pc;

  // update target
  new_state_pc=throw_target;
  state_pc=state.source.pc;
  state_pc++;
  state.source.pc=state_pc;

  // put into state-queue
  statet::goto_state_listt &goto_state_list=
    state.top().goto_state_map[new_state_pc];
  goto_state_list.push_back(statet::goto_statet(state));
}
