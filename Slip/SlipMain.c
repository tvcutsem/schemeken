                     /*---------------------------------------------*/
                     /*                  >>>Slip<<<                 */
                     /*                 Theo D'Hondt                */
                     /*          VUB Software Languages Lab         */
                     /*                  (c) 2012                   */
                     /*---------------------------------------------*/
                     /*   version 13: completion and optimization   */
                     /*---------------------------------------------*/
                     /*                    main                     */
                     /*---------------------------------------------*/

#include <setjmp.h>
#include <stdio.h>
#include <time.h>

#include "SlipMain.h"

#include "SlipCache.h"
#include "SlipCompile.h"
#include "SlipDictionary.h"
#include "SlipEnvironment.h"
#include "SlipEvaluate.h"
#include "SlipNative.h"
#include "SlipPool.h"
#include "SlipPrint.h"
#include "SlipRead.h"
#include "SlipStack.h"
#include "SlipThread.h"
#include "SlipPersist.h"

/*----------------------------------- private constants --------------------------------*/

enum { Minimum_number_of_cells = 4096,
       Print_Buffer_size       = 128,
       Root_size               = 17 };

static const RWS_type Prompt_rawstring = "\n>>>";


/*----------------------------------- public variables ---------------------------------*/

UNS_type tag_count[USP_tag + 1];
UNS_type dcount;
UNS_type ndcount;
UNS_type ntcount;
UNS_type tcount;

/*------------------------------------- private types ----------------------------------*/

typedef
  enum { Initiate_REP  = 0,
         Terminate_REP = 1 } REP_type;

typedef jmp_buf Exi_type;

/*----------------------------------- private variables --------------------------------*/

static EXP_type Intermediate_expression;
static VEC_type Root_vector;

static Exi_type Exit;

static RWC_type Print_Buffer[Print_Buffer_size];

static UNS_type Root_counter;

static ERF_type * Root_references;

/*--------------------------------------------------------------------------------------*/
/*----------------------------------------- print --------------------------------------*/
/*--------------------------------------------------------------------------------------*/

static NBR_type Continue_read_eval_print_number;

THREAD_BEGIN_FRAME(Rep);
THREAD_END_FRAME(Rep);

/*--------------------------------------------------------------------------------------*/

static NIL_type continue_read_eval_print()
  { EXP_type expression;
    expression = Main_Get_Expression();
    Print_Print(expression);
    longjmp(Exit,
            Terminate_REP); }

/*--------------------------------------------------------------------------------------*/

static NIL_type initialize_print(NIL_type)
  { slipken_persist_init(Continue_read_eval_print_number, Thread_Register(continue_read_eval_print)); }

/*--------------------------------------------------------------------------------------*/

NIL_type Main_Proceed_C(NIL_type)
  { REP_type status;
    if ((status = setjmp(Exit)) == Initiate_REP)
      { Thread_Proceed_C(); } }

NIL_type read_eval_print_C(NIL_type)
  { DBL_type consumption;
    EXP_type compiled_expression,
             expression;
    REP_type status;
    RWL_type start_time,
             ticks;
    RWR_type raw_time;
    UNS_type collect_count;
    start_time = clock();
    Dictionary_Checkpoint();
    for (UNS_type tag = AND_tag;
         tag <= USP_tag;
         tag++)
      tag_count[tag] = 0;
    dcount = ndcount = ntcount = tcount = 0;
    if ((status = setjmp(Exit)) == Initiate_REP)
      { Thread_Push_C(Continue_read_eval_print_number,
                      Grammar_False,
                      Rep_size);
        expression = Read_Parse_C(Main_Void);
        compiled_expression = Compile_Compile_C(expression);
        Evaluate_Eval_C(compiled_expression,
                        Grammar_False);
        Thread_Proceed_C(); }
    ticks = clock();
    ticks -= start_time;
    raw_time = (RWR_type)ticks/CLOCKS_PER_SEC;
    snprintf(Print_Buffer,  
             Print_Buffer_size, 
             "elapsed   = %9.6f seconds", 
             raw_time);
    Slip_Log(Print_Buffer);
    consumption = Memory_Consumption();
    snprintf(Print_Buffer,  
             Print_Buffer_size, 
             "consumed  = %9.6f Mcells", 
             consumption / 1000000);
    Slip_Log(Print_Buffer);
    collect_count = Memory_Collect_Count();
    snprintf(Print_Buffer,  
             Print_Buffer_size, 
             "collected = %9d times", 
             collect_count);
    Slip_Log(Print_Buffer);
    Slip_Log("tag frequency:");
    for (UNS_type tag = AND_tag;
         tag <= USP_tag;
         tag++)
      if (tag_count[tag] > 0)
        { snprintf(Print_Buffer,  
                   Print_Buffer_size, 
                   "tag = %2d - count = %4d", 
                   tag, 
                   tag_count[tag]);
          Slip_Log(Print_Buffer); }
    snprintf(Print_Buffer,  
             Print_Buffer_size, 
             "ndcount = %8d", 
             ndcount);
    Slip_Log(Print_Buffer);
    snprintf(Print_Buffer,  
             Print_Buffer_size, 
             "dcount  = %8d", 
             dcount);
    Slip_Log(Print_Buffer);
    snprintf(Print_Buffer,  
             Print_Buffer_size, 
             "ntcount = %8d", 
             ntcount);
    Slip_Log(Print_Buffer);
    snprintf(Print_Buffer,  
             Print_Buffer_size, 
             "tcount  = %8d", 
             tcount);
    Slip_Log(Print_Buffer); }
    
static NIL_type vector_copy(VEC_type Old_vector,
                            VEC_type New_vector)
  { UNS_type index,
             new_size,
             old_size;
    old_size = size_VEC(Old_vector);
    new_size = size_VEC(New_vector);
    for (index = 1;
         (index <= old_size)&&(index <= new_size);
         index += 1)
      New_vector[index] = Old_vector[index];
    for (;
         index <= new_size;
         index += 1)
      New_vector[index] = Grammar_Unspecified; }

/*----------------------------------- hidden functions ---------------------------------*/

NIL_type _Main_Make_Claim_C(UNS_type Size)
  { if (Memory_Available() < Size)
      { Memory_Collect_C();
        if (Memory_Available() < Size)
          Slip_Fail(IMM_error); }}

NIL_type _Main_Register_Root_Reference(ERF_type Reference)
  { if (Root_counter == Root_size)
      Slip_Fail(TMR_error);
    Root_references[Root_counter] = Reference;
    Root_counter += 1; }

/*--------------------------------- implemented callbacks ------------------------------*/

NIL_type Memory_After_Reclaim(PTR_type New_root)
  { EXP_type expression;
    ERF_type reference;
    UNS_type index;
    Root_vector = (EXP_type)New_root;
    for (index = 0;
         index < Root_counter;
         index += 1)
      { reference = Root_references[index];
        expression = Root_vector[index + 1];
        *reference = expression; } }

PTR_type Memory_Before_Reclaim(NIL_type)
  { ERF_type reference;
    UNS_type index;
    for (index = 0;
         index < Root_counter;
         index += 1)
      { reference = Root_references[index];
        Root_vector[index + 1] = *reference; }
    return (PTR_type)Root_vector; }

/*----------------------------------- public functions ---------------------------------*/

NIL_type Main_Error_Handler(BYT_type Error,
                            RWS_type Rawstring)
  { Dictionary_Rollback_C(); 
    Environment_Release_Current_Frame();
    Environment_Reset();
    Thread_Reset();
    Stack_Reset();
    Main_Set_Expression(Grammar_Unspecified);
    Slip_Error(Error,
               Rawstring);
    longjmp(Exit,
            Terminate_REP); }

EXP_type Main_Get_Expression(NIL_type)
  { return Intermediate_expression; }

NIL_type Main_Reallocate_Vector_M(VRF_type Vector_reference,
                                  UNS_type Increment)
  { UNS_type new_size,
             old_size;
    VEC_type new_vector,
             old_vector;
    old_vector = *Vector_reference;
    old_size = size_VEC(old_vector);
    new_size = old_size + Increment;
    new_vector = make_VEC_Z(new_size);
    vector_copy(old_vector,
                new_vector);
    *Vector_reference = new_vector; }

NIL_type Main_Reclaim_C(NIL_type)
  { Memory_Collect_C(); }

NIL_type Main_Replace_Vector(VEC_type Old_vector,
                             VEC_type New_vector)
  { Memory_Replace((PTR_type)Old_vector,
                   (PTR_type)New_vector);
    vector_copy(Old_vector,
                New_vector); }

EXP_type Main_Reverse(EXP_type List_epression)
  { EXP_type next_expression,
             previous_expression;
    PAI_type pair;
    previous_expression = Grammar_Null;
    for (pair = List_epression;
         is_PAI(pair);
         pair = next_expression)
      { next_expression = pair->cdr;
        pair->cdr = previous_expression;
        previous_expression = pair; }
    return previous_expression; }

NIL_type Main_Set_Expression(EXP_type Expression)
  { Intermediate_expression = Expression; }

NIL_type Main_Receive_Ken_Message(RWK_type sender)
  { EXP_type expression;
    if (0 == ken_id_cmp(sender, kenid_alarm))
      expression = Grammar_Unspecified;
    else
      expression = Read_Parse_C(Main_Void);
    Native_Receive_Ken_Message(sender, expression); }

/*----------------------------------- exported functions -------------------------------*/

void Root_Initialize() {
  slipken_persist_init(Root_counter, 0);
  slipken_persist_init(Root_vector, make_VEC_M(Root_size));
  slipken_persist_init(Root_references,
      slipken_simple_malloc(Root_size * sizeof(ERF_type)));
  NTF(Root_references != NULL);
}

void Slip_INIT(char * Memory,
               int    Size)
  { UNS_type tag_boundary,
             total_number_of_cells;
    total_number_of_cells = Size / CEL_SIZE;
    if (((UNS_type)Memory % CEL_SIZE) != 0)
      Slip_Fail(MNS_error);
    if (total_number_of_cells < Minimum_number_of_cells)
      Slip_Fail(IMM_error);
    tag_boundary = Grammar_Boundary();
    Memory_Initialize((ADR_type)Memory,
                      total_number_of_cells,
                      tag_boundary);
    Root_Initialize();
    Grammar_Initialize();
    slipken_persist_init(Intermediate_expression, Grammar_Unspecified);
    Pool_Initialize_M();
    Cache_Initialize_M();
    Compile_Initialize();
    Dictionary_Initialize();
    Environment_Initialize_M();
    Thread_Initialize();
    Evaluate_Initialize();
    Native_Initialize_M();
    Print_Initialize();
    Read_Initialize();
    Stack_Initialize_M();
    initialize_print();
    MAIN_REGISTER(Grammar_Empty_Vector);
    MAIN_REGISTER(Grammar_Newline_String);
    MAIN_REGISTER(Intermediate_expression);
/*    Slip_Print("cpSlip/c version 13: completion and optimization");*/
/*    for (;;)*/
/*      read_eval_print_C();*/
    }
