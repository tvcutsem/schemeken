                     /*---------------------------------------------*/
                     /*                  >>>Slip<<<                 */
                     /*                 Theo D'Hondt                */
                     /*          VUB Software Languages Lab         */
                     /*                  (c) 2012                   */
                     /*---------------------------------------------*/
                     /*   version 13: completion and optimization   */
                     /*---------------------------------------------*/
                     /*                    stack                    */
                     /*---------------------------------------------*/

#include "SlipMain.h"

#include "SlipStack.h"
#include "SlipPersist.h"

/*---------------------------------- private constants ---------------------------------*/

enum { Initial_stack_size   = 64,
       Stack_margin         = 2,
       Stack_size_increment = 32 };

/*---------------------------------- private variables ---------------------------------*/

static VEC_type Stack_vector;

static UNS_type Top_of_stack;

/*---------------------------------- private functions ---------------------------------*/

static NIL_type flush_stack(NIL_type)
  { UNS_type index,
             stack_size;
    Top_of_stack = 0;
    stack_size = size_VEC(Stack_vector);
    for (index = 1;
         index <= stack_size;
         index += 1)
      Stack_vector[index] = Grammar_Null; }

static NIL_type prevent_stack_overflow_C(NIL_type)
  { UNS_type stack_size;
    stack_size = size_VEC(Stack_vector);
    if ((stack_size - Top_of_stack) > Stack_margin)
      return;
    Slip_Log("stack reallocation");
    MAIN_CLAIM_DYNAMIC_C(stack_size + Stack_size_increment);
    Main_Reallocate_Vector_M(&Stack_vector,
                             Stack_size_increment); }

/*----------------------------------- public functions ---------------------------------*/

NIL_type Stack_Collapse_C(UNS_type Count)
  { EXP_type expression;
    UNS_type index;
    VEC_type vector;
    if (Count == 0)
      { Stack_vector[++Top_of_stack] = Grammar_Empty_Vector;
        prevent_stack_overflow_C();
        return; }
    MAIN_CLAIM_DYNAMIC_C(Count);
    vector = make_VEC_Z(Count);
    for (index = Count;
         index > 0;
         index--)
      { expression = Stack_vector[Top_of_stack--];
        vector[index] = expression; }
    Stack_vector[++Top_of_stack] = vector; }

EXP_type Stack_Peek(NIL_type)
  { EXP_type expression;
    expression = Stack_vector[Top_of_stack];
    return expression; }

NIL_type Stack_Poke(EXP_type Expression)
  { Stack_vector[Top_of_stack] = Expression; }

EXP_type Stack_Pop(NIL_type)
  { EXP_type expression;
    expression = Stack_vector[Top_of_stack];
    Top_of_stack -= 1;
    return expression; }

NIL_type Stack_Push_C(EXP_type Expression)
  { Stack_vector[++Top_of_stack] = Expression;
    prevent_stack_overflow_C(); }

NIL_type Stack_Double_Push_C(EXP_type Expression_1,
                             EXP_type Expression_2)
  { Stack_vector[++Top_of_stack] = Expression_1;
    Stack_vector[++Top_of_stack] = Expression_2;
    prevent_stack_overflow_C(); }

EXP_type Stack_Reduce(NIL_type)
  { EXP_type expression,
             result_expression;
    expression = Stack_vector[Top_of_stack];
    Top_of_stack -= 1;
    result_expression = Stack_vector[Top_of_stack];
    Stack_vector[Top_of_stack] = expression;
    return result_expression; }

NIL_type Stack_Reset(NIL_type)
  { flush_stack(); }

NIL_type Stack_Zap(NIL_type)
  { Top_of_stack -= 1; }

/*--------------------------------------------------------------------------------------*/

NIL_type Stack_Initialize_M(NIL_type)
  { slipken_persist_init(Stack_vector, make_VEC_M(Initial_stack_size));
    slipken_persist(Top_of_stack);
    flush_stack();
    MAIN_REGISTER(Stack_vector); }
