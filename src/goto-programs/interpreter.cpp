/*******************************************************************\

Module: Interpreter for GOTO Programs

Author: Daniel Kroening, kroening@kroening.com

\*******************************************************************/

#include <cctype>
#include <cstdio>
#include <iostream>
#include <fstream>
#include <algorithm>
#include <string.h>

#include <util/std_types.h>
#include <util/symbol_table.h>
#include <util/ieee_float.h>
#include <util/fixedbv.h>
#include <util/std_expr.h>
#include <util/message.h>
#include <util/string2int.h>
#include <ansi-c/c_types.h>
#include <json/json_parser.h>

#include "interpreter.h"
#include "interpreter_class.h"

/*******************************************************************\

Function: interpretert::operator()

  Inputs:

 Outputs:

 Purpose:

\*******************************************************************/

void interpretert::operator()()
{
  show=true;
  std::cout << "Initialize:";
  initialise(true);
  try {
    std::cout << "Type h for help" << std::endl;
    while(!done) command();
    std::cout << "Program End." << std::endl;
  } catch (const char *e) {
    std::cout << e << std::endl;
  }
  while(!done) command();
}

/*******************************************************************

Function: interpretert::initialise

 Inputs:

 Outputs:

 Purpose:

 \*******************************************************************/

void interpretert::initialise(bool init) {
  build_memory_map();
  
  const goto_functionst::function_mapt::const_iterator
    main_it=goto_functions.function_map.find(goto_functionst::entry_point());

  if(main_it==goto_functions.function_map.end())
    throw "main not found";
  
  const goto_functionst::goto_functiont &goto_function=main_it->second;
  
  if(!goto_function.body_available())
    throw "main has no body";

  PC=goto_function.body.instructions.begin();
  function=main_it;
    
  done=false;
  if(init) {
    stack_depth=call_stack.size()+1;
    show_state();
    step();
    while(!done && (stack_depth<=call_stack.size()) && (stack_depth>=0)) {
      show_state();
      step();
    }
    while(!done && (call_stack.size()==0))
    {
      show_state();
      step();
    }
    clear_input_flags();
    input_vars.clear();
  }
}

/*******************************************************************\

Function: interpretert::show_state

  Inputs:

 Outputs:

 Purpose:

\*******************************************************************/

void interpretert::show_state()
{
  if(!show) return;
  std::cout << std::endl;
  std::cout << "----------------------------------------------------"
            << std::endl;

  if(PC==function->second.body.instructions.end())
  {
    std::cout << "End of function `"
              << function->first << "'" << std::endl;
  }
  else
    function->second.body.output_instruction(ns, function->first, std::cout, PC);
}

/*******************************************************************\

Function: interpretert::command

  Inputs:

 Outputs:

 Purpose:

\*******************************************************************/

void interpretert::command()
{
  #define BUFSIZE 100
  char command[BUFSIZE]; 
  if(fgets(command, BUFSIZE-1, stdin)==NULL)
  {
    done=true;
    return;
  }

  char ch=tolower(command[0]);
  if(ch=='q')
    done=true;
  else if(ch=='h')
  {
    std::cout << "Interpreter help" << std::endl;
    std::cout << "h: display this menu" << std::endl;
    std::cout << "i: output program inputs" << std::endl;
    std::cout << "id: output program inputs with det values for don cares" << std::endl;
    std::cout << "in: output program inputs with non-det values for don cares" << std::endl;
    std::cout << "it: output program inputs for last trace" << std::endl;
    std::cout << "if: output program inputs ids for non-bodied function" << std::endl;
    std::cout << "i file: output program inputs for [json] file trace" << std::endl;
    std::cout << "j: output json trace" << std::endl;
    std::cout << "m: output memory dump" << std::endl;
    std::cout << "o: output goto trace" << std::endl;
    std::cout << "q: quit" << std::endl;
    std::cout << "r: run until completion" << std::endl;
    std::cout << "s#: step a number of instructions" << std::endl;
    std::cout << "sa: step across a function" << std::endl;
    std::cout << "so: step out of a function" << std::endl;
  }
  else if(ch=='i')
  {
    ch=tolower(command[1]);
    if(ch=='d')      list_inputs(false);
    else if(ch=='n') list_inputs(true);
    else if(ch=='t') {
      list_input_varst ignored;
      load_counter_example_inputs(steps, ignored);
    }
    else if(ch==' ') load_counter_example_inputs(command+3);
    else if(ch=='f') {
      list_non_bodied();
      show_state();
      return;
    }
    print_inputs();
  }
  else if(ch=='j')
  {
    jsont json_steps;
    convert(ns, steps, json_steps);
    ch=tolower(command[1]);
    if(ch==' ') {
      std::ofstream file;
      file.open(command+2);
      if(file.is_open())
      {
        json_steps.output(file);
        file.close();
        return;
      }
    }
    json_steps.output(std::cout);
  }
  else if(ch=='m')
  {
    ch=tolower(command[1]);
    print_memory(ch=='i');
  }
  else if(ch=='o')
  {
    ch=tolower(command[1]);
    if(ch==' ')
    {
      std::ofstream file;
      file.open(command+2);
      if(file.is_open())
      {
        steps.output(ns, file);
        file.close();
        return;
      }
    }
    steps.output(ns, std::cout);
  }
  else if(ch=='r')
  {
    ch=tolower(command[1]);
    initialise(ch!='0');
  }
  else if((ch=='s') || (ch==0))
  {
    num_steps=1;
    stack_depth=-1;
    ch=tolower(command[1]);
    if(ch=='e')
      num_steps=-1;
    else if(ch=='o')
      stack_depth=call_stack.size();
    else if(ch=='a')
      stack_depth=call_stack.size()+1;
    else {
      num_steps=atoi(command+1);
      if(num_steps==0) num_steps=1;
    }
    while(!done && ((num_steps<0) || ((num_steps--)>0)))
    {
      step();
      show_state();
    }
    while(!done && (stack_depth<=call_stack.size())
       && (stack_depth>=0))
    {
      step();
      show_state();
    }
    return;
  }
  show_state();
}

/*******************************************************************\

Function: interpretert::step

  Inputs:

 Outputs:

 Purpose:

\*******************************************************************/

void interpretert::step()
{
  if(PC==function->second.body.instructions.end())
  {
    if(call_stack.empty())
      done=true;
    else
    {
      PC=call_stack.top().return_PC;
      function=call_stack.top().return_function;
      //stack_pointer=call_stack.top().old_stack_pointer; TODO: this increases memory size quite quickly.need to check alternatives
      call_stack.pop();
    }

    return;
  }
  
  next_PC=PC;
  next_PC++; 

  steps.add_step(goto_trace_stept());
  goto_trace_stept &trace_step=steps.get_last_step();
  trace_step.thread_nr=thread_id;
  trace_step.pc=PC;
  switch(PC->type)
  {
  case GOTO:
    trace_step.type=goto_trace_stept::GOTO;
    execute_goto();
    break;
  
  case ASSUME:
    trace_step.type=goto_trace_stept::ASSUME;
    execute_assume();
    break;
  
  case ASSERT:
    trace_step.type=goto_trace_stept::ASSERT;
    execute_assert();
    break;
  
  case OTHER:
    execute_other();
    break;
  
  case DECL:
    trace_step.type=goto_trace_stept::DECL;
    execute_decl();
    break;
  
  case SKIP:
  case LOCATION:
    trace_step.type=goto_trace_stept::LOCATION;
    break;
  case END_FUNCTION:
    trace_step.type=goto_trace_stept::FUNCTION_RETURN;
    break;
  
  case RETURN:
    trace_step.type=goto_trace_stept::FUNCTION_RETURN;
    if(call_stack.empty())
      throw "RETURN without call";

    if(PC->code.operands().size()==1 &&
       call_stack.top().return_value_address!=0)
    {
      std::vector<mp_integer> rhs;
      evaluate(PC->code.op0(), rhs);
      assign(call_stack.top().return_value_address, rhs);
    }

    next_PC=function->second.body.instructions.end();
    break;
    
  case ASSIGN:
    trace_step.type=goto_trace_stept::ASSIGNMENT;
    execute_assign();
    break;
    
  case FUNCTION_CALL:
    trace_step.type=goto_trace_stept::FUNCTION_CALL;
    execute_function_call();
    break;
  
  case START_THREAD:
    trace_step.type=goto_trace_stept::SPAWN;
    throw "START_THREAD not yet implemented";
  
  case END_THREAD:
    throw "END_THREAD not yet implemented";
    break;

  case ATOMIC_BEGIN:
    trace_step.type=goto_trace_stept::ATOMIC_BEGIN;
    throw "ATOMIC_BEGIN not yet implemented";
    
  case ATOMIC_END:
    trace_step.type=goto_trace_stept::ATOMIC_END;
    throw "ATOMIC_END not yet implemented";
    
  case DEAD:
    trace_step.type=goto_trace_stept::DEAD;
    break;//throw "DEAD not yet implemented";
  default:
    throw "encountered instruction with undefined instruction type";
  }
  
  PC=next_PC;
}

/*******************************************************************\

Function: interpretert::execute_goto

  Inputs:

 Outputs:

 Purpose:

\*******************************************************************/

void interpretert::execute_goto()
{
  if(evaluate_boolean(PC->guard))
  {
    if(PC->targets.empty())
      throw "taken goto without target";
    
    if(PC->targets.size()>=2)
      throw "non-deterministic goto encountered";

    next_PC=PC->targets.front();
  }
}

/*******************************************************************\

Function: interpretert::execute_other

  Inputs:

 Outputs:

 Purpose:

\*******************************************************************/

void interpretert::execute_other()
{
  const irep_idt &statement=PC->code.get_statement();
  if(statement==ID_expression)
  {
    assert(PC->code.operands().size()==1);
    std::vector<mp_integer> rhs;
    evaluate(PC->code.op0(), rhs);
  }
  else if(statement==ID_array_set)
  {
    //TODO: need to fill the array with value
  }
  else
    throw "unexpected OTHER statement: "+id2string(statement);
}

/*******************************************************************\

Function: interpretert::execute_decl

  Inputs:

 Outputs:

 Purpose:

\*******************************************************************/

void interpretert::execute_decl()
{
  assert(PC->code.get_statement()==ID_decl);
}

/*******************************************************************

Function: interpretert::get_component_id

 Inputs:

 Outputs:

 Purpose:

 \*******************************************************************/
irep_idt interpretert::get_component_id(const irep_idt &object,unsigned offset)
{
  const symbolt &symbol=ns.lookup(object);
  const typet real_type=ns.follow(symbol.type);
  if(real_type.id()!=ID_struct)
    throw "request for member of non-struct";
  const struct_typet &struct_type=to_struct_type(real_type);
  const struct_typet::componentst &components=struct_type.components();
  for(struct_typet::componentst::const_iterator it=components.begin();
      it!=components.end();++it) {
    if(offset<=0) return it->id();
    unsigned size=get_size(it->type());
    assert(size>=0);
    offset -= size;
  }
  return object;
}

/*******************************************************************

Function: interpretert::get_type

 Inputs:

 Outputs:

 Purpose:

 \*******************************************************************/
typet interpretert::get_type(const irep_idt &id)
{
  dynamic_typest::const_iterator it=dynamic_types.find(id);
  if (it==dynamic_types.end()) return symbol_table.lookup(id).type;
  return it->second;
}

/*******************************************************************

Function: interpretert::get_value

 Inputs:

 Outputs:

 Purpose:

 \*******************************************************************/
exprt interpretert::get_value(const typet &type, unsigned offset,bool use_non_det)
{
  const typet real_type=ns.follow(type);
  if(real_type.id()==ID_struct) {
    exprt result=struct_exprt(real_type);
    const struct_typet &struct_type=to_struct_type(real_type);
    const struct_typet::componentst &components=struct_type.components();
    result.reserve_operands(components.size());
    for(struct_typet::componentst::const_iterator it=components.begin();
        it!=components.end();++it) {
      unsigned size=get_size(it->type());
      assert(size>=0);
      const exprt operand=get_value(it->type(), offset);
      offset += size;
      result.copy_to_operands(operand);
    }
    return result;
  } else if(real_type.id()==ID_array) {
    exprt result=array_exprt(to_array_type(real_type));
    const exprt &size_expr=static_cast<const exprt &>(type.find(ID_size));
    unsigned subtype_size=get_size(type.subtype());
    mp_integer mp_count;
    to_integer(size_expr, mp_count);
    unsigned count=integer2unsigned(mp_count);
    result.reserve_operands(count);
    for(unsigned i=0;i<count;i++) {
      const exprt operand=get_value(type.subtype(),
          offset+i * subtype_size);
      result.copy_to_operands(operand);
    }
    return result;
  }
  if(use_non_det && (memory[offset].initialised>=0))
    return side_effect_expr_nondett(type);
  std::vector<mp_integer> rhs;
  rhs.push_back(memory[offset].value);
  return get_value(type, rhs);
}

/*******************************************************************
 Function: interpretert::get_value

 Inputs:

 Outputs:

 Purpose:

 \*******************************************************************/
exprt interpretert::get_value(const typet &type, std::vector<mp_integer> &rhs,unsigned offset)
{
  const typet real_type=ns.follow(type);

  if(real_type.id()==ID_struct) {
    exprt result=struct_exprt(real_type);
    const struct_typet &struct_type=to_struct_type(real_type);
    const struct_typet::componentst &components=struct_type.components();

    result.reserve_operands(components.size());
    for(struct_typet::componentst::const_iterator it=components.begin();
        it!=components.end();++it) {
      unsigned size=get_size(it->type());
      assert(size>=0);
      const exprt operand=get_value(it->type(), rhs, offset);
      offset += size;
      result.copy_to_operands(operand);
    }

    return result;
  } else if(real_type.id()==ID_array) {
    exprt result(ID_constant, type);
    const exprt &size_expr=static_cast<const exprt &>(type.find(ID_size));
    unsigned subtype_size=get_size(type.subtype());
    mp_integer mp_count;
    to_integer(size_expr, mp_count);
    unsigned count=integer2unsigned(mp_count);
    result.reserve_operands(count);
    for(unsigned i=0;i<count;i++) {
      const exprt operand=get_value(type.subtype(), rhs,
          offset+i * subtype_size);
      result.copy_to_operands(operand);
    }
    return result;
  } else if(real_type.id()==ID_floatbv) {
    ieee_floatt f;
    f.spec=to_floatbv_type(type);
    f.unpack(rhs[offset]);
    return f.to_expr();
  }
  else if(real_type.id()==ID_fixedbv)
  {
    fixedbvt f;
    f.from_integer(rhs[offset]);
    return f.to_expr();
  }
  else if(real_type.id()==ID_bool)
  {
    if(rhs[offset]!=0)
      return true_exprt();
    else
      return false_exprt();
  }
  else if((real_type.id()==ID_pointer) || (real_type.id()==ID_address_of))
  {
    if(rhs[offset]==0)
    {
      constant_exprt result(type);
      result.set_value(ID_NULL);
      return result;
    }
    if(rhs[offset]<memory.size())
    {
      memory_cellt &cell=memory[integer2unsigned(rhs[offset])];
      const typet type=get_type(cell.identifier);
      exprt symbol_expr(ID_symbol, type);
      symbol_expr.set(ID_identifier, cell.identifier);
      if(cell.offset==0) return address_of_exprt(symbol_expr);

      if(ns.follow(type).id()==ID_struct) {
        irep_idt member_id=get_component_id(cell.identifier,cell.offset);
        member_exprt member_expr(symbol_expr, member_id);
        return address_of_exprt(member_expr);
      }
      index_exprt index_expr(symbol_expr,from_integer(cell.offset, integer_typet()));
      return index_expr;
    }
    std::cout << "pointer out of memory " << rhs[offset] << ">"
        << memory.size() << std::endl;
    throw "pointer out of memory";
  }
  return from_integer(rhs[offset], type);
}

/*******************************************************************

Function: interpretert::execute_assign

  Inputs:

 Outputs:

 Purpose:

\*******************************************************************/

void interpretert::execute_assign()
{
  const code_assignt &code_assign=
    to_code_assign(PC->code);

  std::vector<mp_integer> rhs;
  evaluate(code_assign.rhs(), rhs);
  
  if(!rhs.empty())
  {
    mp_integer address=evaluate_address(code_assign.lhs()); 
    unsigned size=get_size(code_assign.lhs().type());

    if(size!=rhs.size())
      std::cout << "!! failed to obtain rhs ("
                << rhs.size() << " vs. "
                << size << ")" << std::endl;
    else
    {
      goto_trace_stept &trace_step=steps.get_last_step();
      assign(address, rhs);
      trace_step.full_lhs=code_assign.lhs();

      // TODO: need to look at other cases on ssa_exprt (derference should be handled on ssa
      const exprt &expr=trace_step.full_lhs;
      if((expr.id()==ID_member && to_member_expr(expr).struct_op().id()!=ID_dereference) || (expr.id()==ID_index) ||(expr.id()==ID_symbol))
      {
        trace_step.lhs_object=ssa_exprt(trace_step.full_lhs);
      }
      trace_step.full_lhs_value=get_value(trace_step.full_lhs.type(),rhs);
      trace_step.lhs_object_value=trace_step.full_lhs_value;
    }
  }
  else if(code_assign.rhs().id()==ID_side_effect)
  {
    side_effect_exprt side_effect=to_side_effect_expr(code_assign.rhs());
    if(side_effect.get_statement()==ID_nondet)
    {
      unsigned address=integer2unsigned(evaluate_address(code_assign.lhs()));
      unsigned size=get_size(code_assign.lhs().type());
      for (int i=0;i<size;i++,address++)
      {
        memory[address].initialised=-1;
      }
    }
  }
}

/*******************************************************************\

Function: interpretert::assign

  Inputs:

 Outputs:

 Purpose:

\*******************************************************************/

void interpretert::assign(
  mp_integer address,
  const std::vector<mp_integer> &rhs)
{
  for(unsigned i=0;i<rhs.size();i++, ++address)
  {
    if(address<memory.size())
    {
      memory_cellt &cell=memory[integer2unsigned(address)];
      if(show) {
        std::cout << "** assigning " << cell.identifier << "["
            << cell.offset << "]:=" << rhs[i] << std::endl;
      }
      cell.value=rhs[i];
      if(cell.initialised==0) cell.initialised=1;
    }
  }
}

/*******************************************************************\

Function: interpretert::execute_assume

  Inputs:

 Outputs:

 Purpose:

\*******************************************************************/

void interpretert::execute_assume()
{
  if(!evaluate_boolean(PC->guard))
    throw "assumption failed";
}

/*******************************************************************\

Function: interpretert::execute_assert

  Inputs:

 Outputs:

 Purpose:

\*******************************************************************/

void interpretert::execute_assert()
{
  if(!evaluate_boolean(PC->guard))
  {
    if ((targetAssert==PC) || stop_on_assertion)
      throw "assertion failed";
    else if (show)
      std::cout << "assertion failed" << std::endl;
  }
}

/*******************************************************************\

Function: interpretert::execute_function_call

  Inputs:

 Outputs:

 Purpose:

\*******************************************************************/

void interpretert::execute_function_call()
{
  const code_function_callt &function_call=
    to_code_function_call(PC->code);

  // function to be called
  mp_integer a=evaluate_address(function_call.function());

  if(a==0)
    throw "function call to NULL";
  else if(a>=memory.size())
    throw "out-of-range function call";

  goto_trace_stept &trace_step=steps.get_last_step();
  const memory_cellt &cell=memory[integer2long(a)];
  const irep_idt &identifier=cell.identifier;
  trace_step.identifier=identifier;

  const goto_functionst::function_mapt::const_iterator f_it=
    goto_functions.function_map.find(identifier);

  if(f_it==goto_functions.function_map.end())
    throw "failed to find function "+id2string(identifier);
    
  // return value
  mp_integer return_value_address;

  if(function_call.lhs().is_not_nil())
    return_value_address=
      evaluate_address(function_call.lhs());
  else
    return_value_address=0;
    
  // values of the arguments
  std::vector<std::vector<mp_integer>>argument_values;
  
  argument_values.resize(function_call.arguments().size());
  
  for(std::size_t i=0;i<function_call.arguments().size();i++)
    evaluate(function_call.arguments()[i], argument_values[i]);

  // do the call

  if(f_it->second.body_available())
  {
    call_stack.push(stack_framet());
    stack_framet &frame=call_stack.top();
    
    frame.return_PC=next_PC;
    frame.return_function=function;
    frame.old_stack_pointer=stack_pointer;
    frame.return_value_address=return_value_address;
    
    // local variables
    std::set<irep_idt> locals;
    get_local_identifiers(f_it->second, locals);
                    
    for(std::set<irep_idt>::const_iterator
        it=locals.begin();
        it!=locals.end();
        it++)
    {
      const irep_idt &id=*it;     
      const symbolt &symbol=ns.lookup(id);
      frame.local_map[id]=integer2unsigned(build_memory_map(id,symbol.type));

    }
        
    // assign the arguments
    const code_typet::parameterst &parameters=
      to_code_type(f_it->second.type).parameters();

    if(argument_values.size()<parameters.size())
      throw "not enough arguments";

    for(unsigned i=0;i<parameters.size();i++)
    {
      const code_typet::parametert &a=parameters[i];
      exprt symbol_expr(ID_symbol, a.type());
      symbol_expr.set(ID_identifier, a.get_identifier());
      assert(i<argument_values.size());
      assign(evaluate_address(symbol_expr), argument_values[i]);
    }

    // set up new PC
    function=f_it;
    next_PC=f_it->second.body.instructions.begin();   
  }
  else
  {
    list_input_varst::iterator it=function_input_vars.find(function_call.function().get(ID_identifier));
    if (it!=function_input_vars.end())
    {
      std::vector<mp_integer> value;
      assert(it->second.size()!=0);
      assert(it->second.front().assignments.size()!=0);
      evaluate(it->second.front().assignments.back().value,value);
      if (return_value_address>0)
      {
        assign(return_value_address,value);
      }
      if(function_call.lhs().is_not_nil())
      {
      }
      it->second.pop_front();
      return;
    }
    if (show)
      std::cout << "no body for "+id2string(identifier);//TODO:used to be throw. need some better approach? need to check state of buffers (and by refs)
  }
}

/*******************************************************************\

Function: interpretert::build_memory_map

  Inputs:

 Outputs:

 Purpose:

\*******************************************************************/

void interpretert::build_memory_map()
{
  // put in a dummy for NULL
  memory.resize(1);
  memory[0].offset=0;
  memory[0].identifier="NULL-OBJECT";
  memory[0].initialised=0;

  num_dynamic_objects=0;
  dynamic_types.clear();

  // now do regular static symbols
  for(symbol_tablet::symbolst::const_iterator
      it=symbol_table.symbols.begin();
      it!=symbol_table.symbols.end();
      it++)
    build_memory_map(it->second);
    
  // for the locals
  stack_pointer=memory.size();
}

/*******************************************************************\

Function: interpretert::build_memory_map

  Inputs:

 Outputs:

 Purpose:

\*******************************************************************/

void interpretert::build_memory_map(const symbolt &symbol)
{
  unsigned size=0;

  if(symbol.type.id()==ID_code)
  {
    size=1;
  }
  else if(symbol.is_static_lifetime)
  {
    size=get_size(symbol.type);
  }

  if(size!=0)
  {
    unsigned address=memory.size();
    memory.resize(address+size);
    memory_map[symbol.name]=address;
    
    for(unsigned i=0;i<size;i++)
    {
      memory_cellt &cell=memory[address+i];
      cell.identifier=symbol.name;
      cell.offset=i;
      cell.value=0;
      cell.initialised=0;
    }
  }
}

/*******************************************************************\

Function: interpretert::build_memory_map

  Inputs:

 Outputs:

 Purpose:

\*******************************************************************/
mp_integer interpretert::build_memory_map(const irep_idt &id,const typet &type) const
{
  if (dynamic_types.find(id)!=dynamic_types.end()) return memory_map[id];
  unsigned size=get_size(type);

  if(size!=0)
  {
    unsigned address=memory.size();
    memory.resize(address+size);
    memory_map[id]=address;
    dynamic_types.insert(std::pair<const irep_idt,typet>(id,type));

    for(unsigned i=0;i<size;i++)
    {
      memory_cellt &cell=memory[address+i];
      cell.identifier=id;
      cell.offset=i;
      cell.value=0;
      cell.initialised=0;
    }
    return address;
  }
  else throw "zero size dynamic object";
  return 0;
}

/*******************************************************************\

Function: interpretert::get_size

  Inputs:

 Outputs:

 Purpose:

\*******************************************************************/

unsigned interpretert::get_size(const typet &type) const
{
  if(type.id()==ID_struct)
  {
    const struct_typet::componentst &components=
      to_struct_type(type).components();

    unsigned sum=0;

    for(struct_typet::componentst::const_iterator
        it=components.begin();
        it!=components.end();
        it++)
    {
      const typet &sub_type=it->type();

      if(sub_type.id()!=ID_code)
        sum+=get_size(sub_type);
    }
    
    return sum;
  }
  else if(type.id()==ID_union)
  {
    const union_typet::componentst &components=
      to_union_type(type).components();

    unsigned max_size=0;

    for(union_typet::componentst::const_iterator
        it=components.begin();
        it!=components.end();
        it++)
    {
      const typet &sub_type=it->type();

      if(sub_type.id()!=ID_code)
        max_size=std::max(max_size, get_size(sub_type));
    }

    return max_size;   
  }
  else if(type.id()==ID_array)
  {
    const exprt &size_expr=static_cast<const exprt &>(type.find(ID_size));

    unsigned subtype_size=get_size(type.subtype());

    mp_integer i;
    if(!to_integer(size_expr, i))
      return subtype_size*integer2unsigned(i);
    else
      return subtype_size;
  }
  else if(type.id()==ID_symbol)
  {
    return get_size(ns.follow(type));
  }
    return 1;
}

/*******************************************************************

Function: list_non_bodied

 Inputs:

 Outputs:

 Purpose:

 \*******************************************************************/
void interpretert::list_non_bodied() {
  int funcs=0;
  function_input_vars.clear();
  for(goto_functionst::function_mapt::const_iterator f_it =
       goto_functions.function_map.begin();
       f_it!=goto_functions.function_map.end(); f_it++)
  {
    if(f_it->second.body_available()) 
    {
      list_non_bodied(f_it->second.body.instructions);
    }
  }

  std::cout << "non bodied varibles " << funcs << std::endl;
  std::map<const irep_idt,const irep_idt>::const_iterator it;
/*for(it=function_input_vars.begin(); it!=function_input_vars.end(); it++)
  {
    std::cout << it->first << "=" << it->second.front() << std::endl;
  }*/
}

char interpretert::is_opaque_function(const goto_programt::instructionst::const_iterator &it, irep_idt &id)
{
  goto_programt::instructionst::const_iterator pc=it;
  if (pc->is_assign())
  {
    const code_assignt &code_assign=to_code_assign(pc->code);
    if(code_assign.rhs().id()==ID_side_effect)
    {
      side_effect_exprt side_effect=to_side_effect_expr(code_assign.rhs());
      if(side_effect.get_statement()==ID_nondet)
      {
        pc--;//TODO: need to check if pc is not already at begin
        if(pc->is_return()) pc--;
      }
    }
  }
  if(pc->type!=FUNCTION_CALL) return 0;
  const code_function_callt &function_call=to_code_function_call(pc->code);
  id=function_call.function().get(ID_identifier);
  const goto_functionst::function_mapt::const_iterator f_it=goto_functions.function_map.find(id);
  if(f_it==goto_functions.function_map.end())
    throw "failed to find function "+id2string(id);
  if(f_it->second.body_available()) return 0;
  if(function_call.lhs().is_not_nil()) return 1;
  return -1;
}

void interpretert::list_non_bodied(const goto_programt::instructionst &instructions) 
{
  for(goto_programt::instructionst::const_iterator f_it =
    instructions.begin(); f_it!=instructions.end(); f_it++) 
  {
    irep_idt f_id;
    if(is_opaque_function(f_it,f_id)>0)
    {
      const code_assignt &code_assign=to_code_assign(f_it->code);
      unsigned return_address=integer2unsigned(evaluate_address(code_assign.lhs()));
      if((return_address > 0) && (return_address<memory.size()))
      {
	function_assignmentt retval={irep_idt(), get_value(code_assign.lhs().type(),return_address)};
	function_assignmentst defnlist={ retval };
	// Add an actual calling context instead of a blank irep if our caller needs it.
        function_input_vars[f_id].push_back({irep_idt(), defnlist});
      }
    }
  }
}

/*******************************************************************

Function: fill_inputs

 Inputs:

 Outputs:

 Purpose:

 \*******************************************************************/
void interpretert::fill_inputs(input_varst &inputs) 
{
  for(input_varst::iterator it=inputs.begin(); it!=inputs.end(); it++)
  {
    std::vector<mp_integer > rhs;
    evaluate(it->second, rhs);
    if(rhs.empty()) continue;
    memory_mapt::const_iterator m_it1=memory_map.find(it->first);
    if(m_it1==memory_map.end()) continue;
    mp_integer address=m_it1->second;
    unsigned size=get_size(it->second.type());
    if(size!=rhs.size()) continue;
    assign(address, rhs);
  }
  clear_input_flags();
}

/*******************************************************************
 Function: list_inputs

 Inputs:

 Outputs:

 Purpose:

 \*******************************************************************/
void interpretert::list_inputs(bool use_non_det) {
  input_vars.clear();
  for(unsigned long i=0;i<memory.size();i++) {
    const memory_cellt &cell=memory[i];
    if(cell.initialised>=0)
      continue;
    if(strncmp(cell.identifier.c_str(), "__CPROVER", 9)==0)
      continue;

    try {
      typet symbol_type=get_type(cell.identifier);
      if(use_non_det) {
        exprt value=get_value(symbol_type, i - cell.offset);
        input_vars.insert(
            std::pair<irep_idt, exprt>(cell.identifier, value));
      } else {
        std::vector<mp_integer> rhs;
        while(memory[i].offset>0)
          i--;
        rhs.push_back(memory[i].value);
        for(unsigned long j=i+1;j<memory.size();j++) {
          const memory_cellt &cell=memory[j];
          if(cell.offset==0)
            break;
          rhs.push_back(cell.value);
        }
        exprt value=get_value(symbol_type, rhs);
        input_vars.insert(
            std::pair<irep_idt, exprt>(cell.identifier, value));
      }
    } catch (const char *e) {
    } catch (const std::string e) {
    }
    for(unsigned long j=i+1;
        (j<memory.size() && memory[j].offset!=0);j++)
      i++;
  }
}

/*******************************************************************
 Function: list_inputs

 Inputs:

 Outputs:

 Purpose:

 \*******************************************************************/
void interpretert::list_inputs(input_varst &inputs) {
  input_vars.clear();
  for(unsigned long i=0;i<memory.size();i++) {
    const memory_cellt &cell=memory[i];
    if(cell.initialised>=0) continue;
    if (strncmp(cell.identifier.c_str(), "__CPROVER", 9)==0) continue;
    if(inputs.find(cell.identifier)!=inputs.end())
    {
      input_vars[cell.identifier]=inputs[cell.identifier];
    }
    unsigned j=i+1;
    while((j<memory.size()) && (memory[j].offset>0)) j++;
    i=j-1;
  }
  for (input_varst::iterator it=inputs.begin();it!=inputs.end();it++)
  {
    if ((it->second.type().id()==ID_pointer) && (it->second.has_operands()))
    {
      const exprt& op=it->second.op0();
      if((op.id()==ID_address_of) && (op.has_operands()))
      {
        mp_integer address=evaluate_address(op.op0());
        irep_idt id=memory[integer2unsigned(address)].identifier;
        if(strncmp(id.c_str(),"symex_dynamic::dynamic_object",28)==0)
        {
          input_vars[it->first]=inputs[it->first];
        }
      }
    }
  }
}

/*******************************************************************
 Function: print_inputs

 Inputs:

 Outputs:

 Purpose:

 \*******************************************************************/
void interpretert::print_inputs() {
  if(input_vars.size()<=0)
    list_inputs();
  for(input_varst::iterator it=input_vars.begin();it!=input_vars.end();
      it++) {
    std::cout << it->first << "=" << from_expr(ns, it->first, it->second)
        << "[" << it->second.type().id() << "]" << std::endl;
  }
}

/*******************************************************************
 Function: print_memory

 Inputs:

 Outputs:

 Purpose:

 \*******************************************************************/
void interpretert::print_memory(bool input_flags) {
  for(unsigned i=0;i<memory.size();i++)
  {
    memory_cellt &cell=memory[i];
    std::cout << cell.identifier << "[" << cell.offset << "]"
              << "=" << cell.value;
    if(input_flags) std::cout << "(" << (int)cell.initialised << ")";
    std::cout << std::endl;
  }
}

/*******************************************************************
 Function: getPC

 Inputs:

 Outputs:

 Purpose: retrieves the PC pointer for the given location

 \*******************************************************************/
goto_programt::const_targett interpretert::getPC(const unsigned location,bool &ok)
{
  goto_programt::const_targett no_location;
  for(goto_functionst::function_mapt::const_iterator f_it =
      goto_functions.function_map.begin();
      f_it!=goto_functions.function_map.end(); f_it++)
  {
    if(f_it->second.body_available())
    {
      for(goto_programt::instructionst::const_iterator it =
    	  f_it->second.body.instructions.begin();
          it!=f_it->second.body.instructions.end(); it++)
      {
        if (it->location_number==location)
        {
          ok=true;
          return it;
        }
      }
    }
  }
  ok=false;
  return no_location;
}

/*******************************************************************
 Function: prune_inputs

 Inputs:

 Outputs:

 Purpose: cleans the list of inputs organising std vectors into arrays and filtering non-inputs if requested

 \*******************************************************************/
void interpretert::prune_inputs(input_varst &inputs,list_input_varst& function_inputs, const bool filter)
{
  for(list_input_varst::iterator it=function_inputs.begin(); it!=function_inputs.end();it++) {
    const exprt size=from_integer(it->second.size(), integer_typet());
    assert(it->second.front().assignments.size()!=0);
    const auto& first_function_assigns=it->second.front().assignments;
    const auto& toplevel_definition=first_function_assigns.back().value;
    array_typet type=array_typet(toplevel_definition.type(),size);
    array_exprt list(type);
    list.reserve_operands(it->second.size());
    for(auto l_it : it->second)
    {
      list.copy_to_operands(l_it.assignments.back().value);
    }
    inputs[it->first]=list;
  }
  input_vars=inputs;
  function_input_vars=function_inputs;
  if(filter)
  {
    try
    {
      fill_inputs(inputs);
      while(!done) {
        show_state();
        step();
      }
    }
    catch (const char *e)
    {
      std::cout << e << std::endl;
    }
    list_inputs();
    list_inputs(inputs);
  }
}

/*******************************************************************
 Function: load_counter_example_inputs

 Inputs:

 Outputs:

 Purpose:

 \*******************************************************************/

interpretert::input_varst& interpretert::load_counter_example_inputs(
    const std::string &filename) {
  jsont counter_example;
  message_clientt message_client;

  if(parse_json(filename,message_client.get_message_handler(),counter_example))
  {
	bool ok;
    show=false;
    stop_on_assertion=false;

    input_varst inputs;
    list_input_varst function_inputs;

    function=goto_functions.function_map.find(goto_functionst::entry_point());
    if(function==goto_functions.function_map.end())
      throw "main not found";

    initialise(true);
    jsont::objectt::const_reverse_iterator it=counter_example.object.rbegin();
    if(it!=counter_example.object.rend())
    {
      unsigned location=integer2unsigned(string2integer(it->second["location"].value));
      targetAssert=getPC(location,ok);
    }

    while(it!=counter_example.object.rend())
    {
      const jsont &pc_object=it->second["location"];
      if (pc_object.kind==jsont::J_NULL) continue;
      unsigned location=integer2unsigned(string2integer(pc_object.value));
      goto_programt::const_targett pc=getPC(location,ok);
      if (!ok) continue;
      const jsont &lhs_object=it->second["lhs"];
      if (lhs_object.kind==jsont::J_NULL) continue;
      irep_idt id=lhs_object.value;
      const jsont &val_object=it->second["value"];
      if (val_object.kind==jsont::J_NULL) continue;
      if(pc->is_other() || pc->is_assign() || pc->is_function_call())
      {
    	const code_assignt &code_assign=to_code_assign(PC->code);//TODO: the other and function_call may be different
        mp_integer address;
        const exprt &lhs=code_assign.lhs();
        exprt value=to_expr(ns, id, val_object.value);
        symbol_exprt symbol_expr=to_symbol_expr(
            (lhs.id()==ID_member) ? to_member_expr(lhs).symbol() : lhs);
        irep_idt id=symbol_expr.get_identifier();
        address=evaluate_address(lhs);
        if(address==0) {
          address=build_memory_map(id,symbol_expr.type());
        }
        std::vector<mp_integer> rhs;
        evaluate(value,rhs);
        assign(address, rhs);
        if(lhs.id()==ID_member)
        {
          address=evaluate_address(symbol_expr);
          inputs[id]=get_value(symbol_expr.type(),integer2unsigned(address));
        }
        else
        {
          inputs[id]=value;
        }
        irep_idt f_id;
        if(is_opaque_function(pc,f_id)!=0)
        {
	  // TODO: save/restore the full data structure?
          function_inputs[f_id].push_front({ irep_idt(), { { irep_idt(), inputs[id] } } });
        }
      }
      it++;
    }
    prune_inputs(inputs,function_inputs,true);
    print_inputs();
    show=true;
    stop_on_assertion=true;
  }
  return input_vars;
}

/*******************************************************************
 Function: get_assigned_symbol

 Inputs: Trace step

 Outputs: Symbol expression that the step assigned to

 Purpose: Looks through memberof expressions etc to find a root symbol.

 \*******************************************************************/

static symbol_exprt get_assigned_symbol(const exprt& expr)
{
  if(expr.id() == ID_symbol)
    return to_symbol_expr(expr);

  if(expr.id() == ID_member ||
     expr.id() == ID_dereference ||
     expr.id() == ID_typecast ||
     expr.id() == ID_index ||
     expr.id() == ID_byte_extract_little_endian ||
     expr.id() == ID_byte_extract_big_endian)
    return get_assigned_symbol(expr.op0());

  // Try to handle opcodes I haven't thought of by looking for the
  // only pointer-typed operand:

  const exprt* unique_pointer = 0;
  
  for(const auto& op : expr.operands())
  {
    if(op.type().id() == ID_pointer)
    {
      if(!unique_pointer)
	unique_pointer = &op;
      else
      {
	unique_pointer = 0;
	break;
      }
    }
  }

  if(unique_pointer)
    return get_assigned_symbol(*unique_pointer);
  else
  {
    std::string error = "Failed to look through '" + as_string(expr.id())
      + "' in get_assigned_symbol.";
    throw error;
  }
}

static symbol_exprt get_assigned_symbol(const goto_trace_stept& step)
{
  return get_assigned_symbol(step.full_lhs);
}

/*******************************************************************
 Function: calls_opaque_stub

 Inputs: Call instruction and symbol table

 Outputs: Returns COMPLEX_OPAQUE_STUB if the given instruction
          calls a function with a synthetic stub modelling its
          behaviour, or SIMPLE_OPAQUE_STUB if it is not available at all,
          or NOT_OPAQUE_STUB otherwise.
          In the first case, sets capture_symbol to the symbol name
          the stub defines. In either stub case, sets f_id to the function
          called.

 Purpose: Determine whether a call instruction targets an internal
          function or some form of opaque function.

 \*******************************************************************/

enum calls_opaque_stub_ret { NOT_OPAQUE_STUB, SIMPLE_OPAQUE_STUB, COMPLEX_OPAQUE_STUB };

calls_opaque_stub_ret calls_opaque_stub(const code_function_callt& callinst,
  const symbol_tablet& symbol_table, irep_idt& f_id, irep_idt& capture_symbol)
{
  f_id=callinst.function().get(ID_identifier);
  const symbolt& called_func=symbol_table.lookup(f_id);
  capture_symbol=called_func.type.get("opaque_method_capture_symbol");
  if(capture_symbol!=irep_idt())
    return COMPLEX_OPAQUE_STUB;
  else if(called_func.value.id()==ID_nil)
    return SIMPLE_OPAQUE_STUB;
  else
    return NOT_OPAQUE_STUB;
}

/*******************************************************************
 Function: get_value_tree

 Inputs: Symbol to capture, map of current symbol ("input") values.

 Outputs: Populates captured with a bottom-up-ordered list of symbol
          values containing every value reachable from capture_symbol.
          For example, if capture_symbol contains a pointer to some
          object x, captured will be populated with x's value and then
          capture_symbol's.

 Purpose: Take a snapshot of state reachable from capture_symbol.

 \*******************************************************************/

// Get the current value of capture_symbol plus
// the values of any symbols referenced in its fields.
// Store them in 'captured' in bottom-up order.
void interpretert::get_value_tree(const irep_idt& capture_symbol,
  const input_varst& inputs, function_assignmentst& captured)
{

  // Circular reference?
  for(auto already_captured : captured)
    if(already_captured.id==capture_symbol)
      return;

  auto findit=inputs.find(capture_symbol);
  if(findit==inputs.end())
  {
    std::cout << "Stub method returned without defining " << capture_symbol
	      << ". Did the program trace end inside a stub?\n";
    return;
  }

  exprt defined=findit->second;

  bool isnull=false;
  
  if(defined.type().id()==ID_pointer)
  {
  
    auto struct_ty=defined.type().subtype();
    assert(struct_ty.id()==ID_struct);

    std::vector<mp_integer> pointer_as_bytes;
    evaluate(defined, pointer_as_bytes);
    isnull=true;
    for(auto b : pointer_as_bytes)
      if(!b.is_zero())
	isnull=false;

    if(!isnull)
    {
	std::vector<mp_integer> struct_as_bytes;
	evaluate(dereference_exprt(defined, struct_ty), struct_as_bytes);
	defined=get_value(struct_ty, struct_as_bytes);
    }

  }

  if(!isnull)
  {
  
    const auto& defined_struct=to_struct_expr(defined);
    const auto& struct_type=to_struct_type(defined.type());
    const auto& members=struct_type.components();
    unsigned idx=0;
    assert(defined_struct.operands().size()==members.size());
    // Assumption: all object trees captured this way refer directly to particular
    // symex::dynamic_object expressions, which are always address-of-symbol constructions.
    forall_operands(opit, defined_struct) {
      if(members[idx].type().id()==ID_pointer)
	{
	  const auto& referee=to_symbol_expr(to_address_of_expr(*opit).object()).get_identifier();
	  get_value_tree(referee, inputs, captured);
	}
      ++idx;
    }

  }

  captured.push_back({capture_symbol, defined});

}

/*******************************************************************
 Function: load_counter_example_inputs

 Inputs:

 Outputs:

 Purpose:

 \*******************************************************************/

interpretert::input_varst& interpretert::load_counter_example_inputs(
    const goto_tracet &trace, list_input_varst& function_inputs, const bool filtered) {
  show=false;
  stop_on_assertion=false;

  input_varst inputs;

  function=goto_functions.function_map.find(goto_functionst::entry_point());
  if(function==goto_functions.function_map.end())
    throw "main not found";

  irep_idt previous_assigned_symbol;
 
  initialise(true);
  goto_tracet::stepst::const_reverse_iterator it=trace.steps.rbegin();
  if(it!=trace.steps.rend()) targetAssert=it->pc;
  for(;it!=trace.steps.rend();++it) {
    if(it->is_function_call())      
    {
      irep_idt called, capture_symbol;
      switch(calls_opaque_stub(to_code_function_call(it->pc->code),
			       symbol_table,called,capture_symbol))
      {
	
      case NOT_OPAQUE_STUB:
	break;
      case SIMPLE_OPAQUE_STUB:
	{
	  // Simple opaque function that returns a primitive. The assignment after
	  // this (before it in trace order) will have given the value
	  // assigned to its return nondet.
	  if(previous_assigned_symbol!=irep_idt())
	  {
	    function_assignmentst single_defn=
	      { { previous_assigned_symbol,inputs[previous_assigned_symbol] } };
	    function_inputs[called].push_front({ it->pc->function,single_defn });
	  }
	  break;
	}
      case COMPLEX_OPAQUE_STUB:
	{
	  // Complex stub: capture the value of capture_symbol instead of whatever happened
	  // to have been defined most recently. Also capture any other referenced objects.
	  function_assignmentst defined;
	  get_value_tree(capture_symbol,inputs,defined);
	  if(defined.size()!=0) // Definition found?
	    function_inputs[called].push_front({ it->pc->function,defined });
	  break;
	}
	
      } // End switch on stub type

    } // End if-is-function-call
    else if(goto_trace_stept::ASSIGNMENT==it->type
	    && (it->pc->is_other() || it->pc->is_assign()
		|| it->pc->is_function_call()))
    {
   
      mp_integer address;

      symbol_exprt symbol_expr=get_assigned_symbol(*it);
      irep_idt id=symbol_expr.get_identifier();

      address=evaluate_address(it->full_lhs);
      if(address==0) {
        address=build_memory_map(id,symbol_expr.type());
      }
      std::vector<mp_integer> rhs;
      evaluate(it->full_lhs_value,rhs);
      assign(address,rhs);

      mp_integer whole_lhs_object_address=evaluate_address(symbol_expr);
      inputs[id]=get_value(symbol_expr.type(),integer2unsigned(whole_lhs_object_address));
      input_first_assignments[id]=it->pc->function;

      previous_assigned_symbol=id;
      
    }
  }
  prune_inputs(inputs,function_inputs,filtered);
  print_inputs();
  show=true;
  stop_on_assertion=true;
  return input_vars;
}

/*******************************************************************

Function: interpreter

  Inputs:

 Outputs:

 Purpose:

\*******************************************************************/

void interpreter(
  const symbol_tablet &symbol_table,
  const goto_functionst &goto_functions)
{
  interpretert interpreter(symbol_table,goto_functions);
  interpreter();
}
