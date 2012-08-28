                     /*---------------------------------------------*/
                     /*                  >>>Slip<<<                 */
                     /*                 Theo D'Hondt                */
                     /*          VUB Software Languages Lab         */
                     /*                  (c) 2012                   */
                     /*---------------------------------------------*/
                     /*   version 13: completion and optimization   */
                     /*---------------------------------------------*/
                     /*                   threads                   */
                     /*---------------------------------------------*/

#include <stdio.h>

#include "SlipMain.h"
#include "SlipPersist.h"

#include "SlipCache.h"
#include "SlipThread.h"

/*--------------------------------------------------------------------------------------*/
/*---------------------------------- thread manipulation -------------------------------*/
/*--------------------------------------------------------------------------------------*/

/*------------------------------------ private macros ----------------------------------*/

#define NULL_THREAD (THR_type)Grammar_Null

/*------------------------------------- opaque types -----------------------------------*/

BEGIN_GRAMMAR_FRAME(THR);
  DEFINE_GRAMMAR_SLOT(NBR, tid);
  DEFINE_GRAMMAR_SLOT(THR, thr);
  DEFINE_GRAMMAR_SLOT(EXP, tps);
END_GRAMMAR_FRAME(THR);

/*----------------------------------- private variables --------------------------------*/

static THR_type Current_thread;

/*----------------------------------- private prototypes -------------------------------*/

static NIL_type release_thread(THR_type);

/*----------------------------------- private functions --------------------------------*/

static NIL_type clone_thread_C(NIL_type)
  { EXP_type expression;
    UNS_type index,
             size;
    VEC_type new_thread_vector,
             thread_vector;
    if (is_MRK(Current_thread))
      { thread_vector = (VEC_type)Current_thread;
        size = size_VEC(thread_vector);
        new_thread_vector = Cache_Make_Vector_C(size);
        thread_vector = (VEC_type)Current_thread;
        for (index = 1;
             index <= size;
             index += 1)
         { expression = thread_vector[index];
           new_thread_vector[index] = expression; }
        Current_thread = (THR_type)new_thread_vector; }}

static NIL_type flush_thread(NIL_type)
  { THR_type next_thread;
    for (;
         !is_NUL(Current_thread);
         Current_thread = next_thread)
      { next_thread = Current_thread->thr;
        release_thread(Current_thread); }}
        
static NIL_type mark_thread(THR_type Thread)
  { EXP_type expression;
    TAG_type tag;
    UNS_type index,
             size;
    VEC_type thread_vector;
    thread_vector = (VEC_type)Thread;
    apply_MRK(thread_vector);
    size = size_VEC(thread_vector);
    for (index = 4;
         index <= size;
         index += 1)
      { expression = thread_vector[index];
        tag = Grammar_Tag(expression);
        if (tag == VEC_tag)
          apply_MRK(expression); } }    

static NIL_type release_thread(THR_type Thread)
  { VEC_type thread_vector;
    thread_vector = (VEC_type)Thread;
    Cache_Release_Vector(thread_vector); }

/*----------------------------------- public functions ---------------------------------*/

THR_type Thread_Keep_C(NIL_type)
  { clone_thread_C();
    return Current_thread; }

THR_type Thread_Mark(NIL_type)
  { THR_type thread;
    for (thread = Current_thread;
         !is_NUL(thread);
         thread = thread->thr)
      mark_thread(thread);
    return Current_thread; }

THR_type Thread_Patch_C(NBR_type Thread_id_number)
  { clone_thread_C();
    Current_thread->tid = Thread_id_number;
    return Current_thread; }

THR_type Thread_Peek(NIL_type)
  { return Current_thread; }

THR_type Thread_Poke_C(NBR_type Thread_id_number,
                       EXP_type Tail_call_expression,
                       UNS_type Size)
  { THR_type new_thread,
             next_thread;
    VEC_type new_thread_vector;
    new_thread_vector = Cache_Make_Vector_C(Size);
    new_thread = (THR_type)new_thread_vector;
    next_thread = Current_thread->thr;
    new_thread->tid = Thread_id_number;
    new_thread->thr = next_thread;
    new_thread->tps = Tail_call_expression;
    release_thread(Current_thread);
    Current_thread = new_thread;
    return Current_thread; }

THR_type Thread_Push_C(NBR_type Thread_id_number,
                       EXP_type Tail_call_expression,
                       UNS_type Size)
  { THR_type new_thread;
    VEC_type new_thread_vector;
    new_thread_vector = Cache_Make_Vector_C(Size);
    new_thread = (THR_type)new_thread_vector;
    new_thread->tid = Thread_id_number;
    new_thread->thr = Current_thread;
    new_thread->tps = Tail_call_expression;
    Current_thread = new_thread;
    return Current_thread; }

NIL_type Thread_Replace(THR_type Thread)
  { flush_thread();
    Current_thread = Thread; }

NIL_type Thread_Reset(NIL_type)
  { flush_thread();
    Current_thread = NULL_THREAD; }

EXP_type Thread_Tail_Position(NIL_type)
  { EXP_type boolean_expression;
    boolean_expression = Current_thread->tps;
    return boolean_expression; }

NIL_type Thread_Zap(NIL_type)
  { THR_type next_thread;
    next_thread = Current_thread->thr;
    release_thread(Current_thread);
    Current_thread = next_thread; }

/*--------------------------------------------------------------------------------------*/

static NIL_type initialize_thread_manipulation(NIL_type)
  { slipken_persist_init(Current_thread, NULL_THREAD);
    MAIN_REGISTER(Current_thread); }

/*--------------------------------------------------------------------------------------*/
/*----------------------------------- thread procedures --------------------------------*/
/*--------------------------------------------------------------------------------------*/

/*----------------------------------- private constants --------------------------------*/

enum { Thread_procedure_table_size  = 28 };

/*----------------------------------- private variables --------------------------------*/

static TPR_type * Thread_procedure_table;
static UNS_type Thread_procedure_table_counter;

/*----------------------------------- public functions ---------------------------------*/

NIL_type Thread_Proceed_C(NIL_type)
  { NBR_type thread_id_number;
    TPR_type thread_procedure_C;
    UNS_type thread_id;
    for (;;)
      { thread_id_number = Current_thread->tid;
        thread_id = get_NBR(thread_id_number);
        thread_procedure_C = Thread_procedure_table[thread_id];
        thread_procedure_C(); }}

NBR_type Thread_Register(TPR_type Thread_procedure_C)
  { NBR_type thread_id_number;
    if (Thread_procedure_table_counter == Thread_procedure_table_size)
      Slip_Fail(TMT_error);
    Thread_procedure_table[Thread_procedure_table_counter] = Thread_procedure_C;
    thread_id_number = make_NBR(Thread_procedure_table_counter);
    Thread_procedure_table_counter += 1;
    return thread_id_number; }

/*--------------------------------------------------------------------------------------*/

static NIL_type initialize_thread_procedures(NIL_type)
  { slipken_persist_init(Thread_procedure_table_counter, 0);
    slipken_persist_init(Thread_procedure_table,
        slipken_simple_malloc(Thread_procedure_table_size * sizeof(TPR_type)));
    NTF(Thread_procedure_table != NULL);
  }

/*--------------------------------------------------------------------------------------*/

NIL_type Thread_Initialize(NIL_type)
  { initialize_thread_manipulation();
    initialize_thread_procedures(); }
