                     /*---------------------------------------------*/
                     /*                  >>>Slip<<<                 */
                     /*                 Theo D'Hondt                */
                     /*          VUB Software Languages Lab         */
                     /*                  (c) 2012                   */
                     /*---------------------------------------------*/
                     /*   version 13: completion and optimization   */
                     /*---------------------------------------------*/
                     /*                   evaluate                  */
                     /*---------------------------------------------*/

#include "SlipMain.h"

#include "SlipEnvironment.h"
#include "SlipEvaluate.h"
#include "SlipNative.h"
#include "SlipPool.h"
#include "SlipStack.h"
#include "SlipThread.h"
#include "SlipPersist.h"

/* Scheme compatibility

KEYWORDS
NO  =>
YES and
YES begin
NO  case
YES cond
YES define
NO  define-syntax
YES delay
NO  do
YES else
YES if
YES lambda
YES let → let*
NO  let*
NO  letrec
NO  let-syntax
NO  letrec-syntax
YES or
YES quasiquote
YES quote
YES set!
NO  syntax-rules
YES unquote
YES unquote-splicing

*/

/*------------------------------------ private prototypes ------------------------------*/

static NIL_type evaluation_symbol_error(BYT_type,
                                        SYM_type);

/*------------------------------------ private functions -------------------------------*/

static NIL_type evaluation_error(BYT_type Error)
  { Main_Error_Handler(Error,
                       Main_Void); }

static NIL_type evaluation_rawstring_error(BYT_type Error,
                                           RWS_type Rawstring)
  { Main_Error_Handler(Error,
                       Rawstring); }

static NIL_type evaluation_keyword_error(BYT_type Error,
                                         KEY_type Key)
  { SYM_type symbol;
    symbol = Pool_Keyword_Symbol(Key);
    evaluation_symbol_error(Error,
                            symbol); }

static NIL_type evaluation_procedure_error(BYT_type Error,
                                           PRC_type Procedure)
  { SYM_type symbol;
    symbol = Procedure->nam;
    evaluation_symbol_error(Error,
                            symbol); }

static NIL_type evaluation_procedure_vararg_error(BYT_type Error,
                                                  PRV_type Procedure_vararg)
  { SYM_type symbol;
    symbol = Procedure_vararg->nam;
    evaluation_symbol_error(Error,
                            symbol); }

static NIL_type evaluation_symbol_error(BYT_type Error,
                                        SYM_type Symbol)
  { RWS_type rawstring;
    rawstring = Symbol->rws;
    Main_Error_Handler(Error,
                       rawstring); }

/*--------------------------------------------------------------------------------------*/
/*--------------------------------------------------------------------------------------*/
/*--------------------------------------- B O D Y --------------------------------------*/
/*--------------------------------------------------------------------------------------*/
/*--------------------------------------------------------------------------------------*/

static NBR_type Continue_body_number;

THREAD_BEGIN_FRAME(Bod);
  THREAD_FRAME_SLOT(ENV, env);
  THREAD_FRAME_SLOT(FRM, frm);
THREAD_END_FRAME(Bod);

/*--------------------------------------------------------------------------------------*/

static NIL_type evaluate_expression_C(EXP_type,
                                      EXP_type);

/*--------------------------------------------------------------------------------------*/

static NIL_type store_environment_and_frame_in_thread(THR_type Thread)
  { Bod_type body_thread;
    ENV_type environment;
    FRM_type frame;
    environment = Environment_Get_Current_Environment();
    frame = Environment_Get_Current_Frame();
    body_thread = (Bod_type)Thread;
    body_thread->env = environment;
    body_thread->frm = frame; }

/*--------------------------------------------------------------------------------------*/

static NIL_type continue_body(NIL_type)
  { Bod_type body_thread;
    ENV_type environment;
    FRM_type frame;
    THR_type thread;
    thread = Thread_Peek();
    body_thread = (Bod_type)thread;
    environment = body_thread->env;
    frame       = body_thread->frm;
    Thread_Zap();
    Environment_Release_Current_Frame();
    Environment_Replace_Environment(environment,
                                    frame); }

/*--------------------------------------------------------------------------------------*/

static NIL_type poke_environment_and_frame_C(NIL_type)
  { THR_type thread;
    thread = Thread_Poke_C(Continue_body_number,
                           Grammar_False,
                           Bod_size);
    store_environment_and_frame_in_thread(thread); }

static NIL_type push_environment_and_frame_C(NIL_type)
  { THR_type thread;
    thread = Thread_Push_C(Continue_body_number,
                           Grammar_False,
                           Bod_size);
    store_environment_and_frame_in_thread(thread); }

/*--------------------------------------------------------------------------------------*/

static NIL_type initialize_body(NIL_type)
  { Continue_body_number = Thread_Register(continue_body); }

/*--------------------------------------------------------------------------------------*/
/*--------------------------------------------------------------------------------------*/
/*-------------------------------------- A P P L Y -------------------------------------*/
/*--------------------------------------------------------------------------------------*/
/*--------------------------------------------------------------------------------------*/

/*--------------------------------------------------------------------------------------*/
/*------------------------------------ continuation ------------------------------------*/
/*--------------------------------------------------------------------------------------*/

static NBR_type Continue_continuation_number;

THREAD_BEGIN_FRAME(Cnt);
  THREAD_FRAME_SLOT(CNT, cnt);
THREAD_END_FRAME(Cnt);

/*--------------------------------------------------------------------------------------*/

static NIL_type evaluate_expression_C(EXP_type,
                                      EXP_type);

/*--------------------------------------------------------------------------------------*/

static NIL_type activate_continuation(CNT_type Continuation)
  { ENV_type environment;
    FRM_type frame;
    THR_type thread;
    environment = Continuation->env;
    frame       = Continuation->frm;
    thread      = Continuation->thr;
    Environment_Release_Current_Frame();
    Environment_Replace_Environment(environment,
                                    frame);
    Thread_Replace(thread); }

/*--------------------------------------------------------------------------------------*/

static NIL_type continue_continuation(NIL_type)
  { Cnt_type continuation_thread;
    CNT_type continuation;
    THR_type thread;
    thread = Thread_Peek();
    continuation_thread = (Cnt_type)thread;
    continuation = continuation_thread->cnt;
    Thread_Zap();
    activate_continuation(continuation); }

/*--------------------------------------------------------------------------------------*/

static NIL_type apply_continuation_C(VEC_type Operand_vector,
                                     EXP_type Tail_call_expression)
  { Cnt_type continuation_thread;
    CNT_type continuation;
    EXP_type expression;
    UNS_type operand_vector_size;
    THR_type thread;
    operand_vector_size = size_VEC(Operand_vector);
    if (operand_vector_size != 1)
      evaluation_error(CRQ_error);
    expression = Operand_vector[1];
    BEGIN_PROTECT(expression);
      thread = Thread_Push_C(Continue_continuation_number,
                             Tail_call_expression,
                             Cnt_size);
    END_PROTECT(expression)
    continuation = Main_Get_Expression();
    continuation_thread = (Cnt_type)thread;
    continuation_thread->cnt = continuation;
    evaluate_expression_C(expression,
                          Grammar_True); }

static NIL_type apply_direct_continuation(CNT_type Continuation,
                                          EXP_type Argument_expression,
                                          EXP_type Tail_call_expression)
  { EXP_type value_expression;
    PAI_type argument_list_pair;
    if (is_PAI(Argument_expression))
      { argument_list_pair = Argument_expression;
        value_expression    = argument_list_pair->car;
        Argument_expression = argument_list_pair->cdr;
        if (is_NUL(Argument_expression))
          { Main_Set_Expression(value_expression);
            activate_continuation(Continuation);
            return; }}
    evaluation_error(CRQ_error); }

/*--------------------------------------------------------------------------------------*/

static NIL_type initialize_continuation(NIL_type)
  { Continue_continuation_number = Thread_Register(continue_continuation); }

/*--------------------------------------------------------------------------------------*/
/*---------------------------------------- native --------------------------------------*/
/*--------------------------------------------------------------------------------------*/

static NBR_type Continue_native_number;

THREAD_BEGIN_FRAME(Nat);
  THREAD_FRAME_SLOT(NAT, nat);
  THREAD_FRAME_SLOT(VEC, opd);
  THREAD_FRAME_SLOT(FRM, frm);
  THREAD_FRAME_SLOT(NBR, pos);
THREAD_END_FRAME(Nat);

/*--------------------------------------------------------------------------------------*/

static EXP_type evaluate_direct_expression(EXP_type);
static NIL_type      evaluate_expression_C(EXP_type,
                                           EXP_type);

/*--------------------------------------------------------------------------------------*/

static NIL_type call_raw_native_function_C(NAT_type Native,
                                           FRM_type Frame,
                                           EXP_type Tail_call_expression)
  { RNF_type raw_native_function_C;
    raw_native_function_C = Native->rnf;
    raw_native_function_C(Frame,
                          Tail_call_expression); }

/*--------------------------------------------------------------------------------------*/

static UNS_type direct_expressions(VEC_type Operand_vector,
                                   FRM_type Frame,
                                   UNS_type Position)
  { EXP_type expression;
    UNS_type operand_vector_size,
             position;
    operand_vector_size = size_VEC(Operand_vector);
    for (position = Position;
         position <= operand_vector_size;
         position++)
      { expression = Operand_vector[position];
        expression = evaluate_direct_expression(expression);
        if (is_USP(expression))
          return position;
        Environment_Frame_Set(Frame,
                              position,
                              expression); }
    return 0; }

/*--------------------------------------------------------------------------------------*/

static NIL_type continue_native_C(NIL_type)
  { Nat_type native_thread;
    EXP_type expression,
             tail_call_expression,
             value_expression;
    FRM_type frame;
    NAT_type native;
    NBR_type position_number;
    THR_type thread;
    UNS_type frame_size,
             position;
    VEC_type operand_vector;
    value_expression = Main_Get_Expression();
    thread = Thread_Peek();
    tail_call_expression = Thread_Tail_Position();
    native_thread = (Nat_type)thread;
    frame           = native_thread->frm;
    position_number = native_thread->pos;
    position = get_NBR(position_number);
    Environment_Frame_Set(frame,
                          position,
                          value_expression);
    frame_size = Environment_Frame_Size(frame);
    if (position == frame_size)
      { native = native_thread->nat;
        Thread_Zap();
        call_raw_native_function_C(native,
                                   frame,
                                   tail_call_expression);
        return; }
    operand_vector = native_thread->opd;
    position += 1;
    position = direct_expressions(operand_vector,
                                  frame,
                                  position);
    if (position > 0)
      { thread = Thread_Keep_C();
        native_thread = (Nat_type)thread;
        operand_vector = native_thread->opd;
        expression = operand_vector[position];
        position_number = make_NBR(position);
        native_thread->pos = position_number;
        if (position < frame_size)
          tail_call_expression = Grammar_False;
        evaluate_expression_C(expression,
                              tail_call_expression);
        return; }
    native = native_thread->nat;
    Thread_Zap();
    call_raw_native_function_C(native,
                               frame,
                               tail_call_expression); }

/*--------------------------------------------------------------------------------------*/

static NIL_type apply_direct_native_C(NAT_type Native,
                                      EXP_type Argument_expression,
                                      EXP_type Tail_call_expression)
  { EXP_type value_expression;
    FRM_type frame;
    PAI_type argument_list_pair;
    RWS_type rawstring;
    UNS_type frame_size,
             position;
    frame_size = 0;
    for (argument_list_pair = Argument_expression;
         is_PAI(argument_list_pair);
         argument_list_pair = argument_list_pair->cdr)
      frame_size += 1;
    if (!is_NUL(argument_list_pair))
      { rawstring = Native->nam;
        evaluation_rawstring_error(ITA_error,
                                   rawstring); }
    if (frame_size == 0)
      { frame = Environment_Empty_Frame();
        call_raw_native_function_C(Native,
                                   frame,
                                   Tail_call_expression);
        return; }
    BEGIN_DOUBLE_PROTECT(Native,
                         Argument_expression)
      frame = Environment_Make_Frame_C(frame_size);
    END_DOUBLE_PROTECT(Native,
                       argument_list_pair)
    for (position = 1;
         position <= frame_size;
         position += 1)
      { value_expression   = argument_list_pair->car;
        argument_list_pair = argument_list_pair->cdr;
        Environment_Frame_Set(frame,
                              position,
                              value_expression); }
    call_raw_native_function_C(Native,
                               frame,
                               Tail_call_expression); }

static NIL_type apply_native_C(VEC_type Operand_vector,
                               EXP_type Tail_call_expression)
  { Nat_type native_thread;
    EXP_type expression,
             tail_call_expression;
    FRM_type frame;
    NAT_type native;
    NBR_type position_number;
    THR_type thread;
    UNS_type operand_vector_size,
             position;
    operand_vector_size = size_VEC(Operand_vector);
    if (operand_vector_size == 0)
      { native = Main_Get_Expression();
        frame = Environment_Empty_Frame();
        call_raw_native_function_C(native,
                                   frame,
                                   Tail_call_expression);
        return; }
    BEGIN_PROTECT(Operand_vector)
      frame = Environment_Make_Frame_C(operand_vector_size);
    END_PROTECT(Operand_vector)
    position = direct_expressions(Operand_vector,
                                  frame,
                                  1);
    if (position > 0)
      { position_number = make_NBR(position);
        BEGIN_DOUBLE_PROTECT(Operand_vector,
                             frame)
          thread = Thread_Push_C(Continue_native_number,
                                 Tail_call_expression,
                                 Nat_size);
        END_DOUBLE_PROTECT(Operand_vector,
                           frame)
        native = Main_Get_Expression();
        native_thread = (Nat_type)thread;
        native_thread->nat = native;
        native_thread->opd = Operand_vector;
        native_thread->frm = frame;
        native_thread->pos = position_number;
        expression = Operand_vector[position];
        Main_Set_Expression(expression);
        tail_call_expression = (position < operand_vector_size)
                                 ? Grammar_False
                                 : Tail_call_expression;
        evaluate_expression_C(expression,
                              tail_call_expression);
        return; }
    native = Main_Get_Expression();
    call_raw_native_function_C(native,
                               frame,
                               Tail_call_expression); }

/*--------------------------------------------------------------------------------------*/

static NIL_type initialize_native(NIL_type)
  { Continue_native_number = Thread_Register(continue_native_C); }

/*--------------------------------------------------------------------------------------*/
/*--------------------------------------- procedure ------------------------------------*/
/*--------------------------------------------------------------------------------------*/

static NBR_type Continue_procedure_number;

THREAD_BEGIN_FRAME(Prc);
  THREAD_FRAME_SLOT(PRC, prc);
  THREAD_FRAME_SLOT(VEC, opd);
  THREAD_FRAME_SLOT(FRM, frm);
  THREAD_FRAME_SLOT(NBR, pos);
THREAD_END_FRAME(Prc);

/*--------------------------------------------------------------------------------------*/

static NIL_type        evaluate_expression_C(EXP_type,
                                             EXP_type);
static NIL_type poke_environment_and_frame_C(NIL_type);
static NIL_type push_environment_and_frame_C(NIL_type);

/*--------------------------------------------------------------------------------------*/

static NIL_type procedure_body_C(FRM_type Frame)
  { ENV_type environment;
    EXP_type body_expression;
    PRC_type procedure;
    procedure = Main_Get_Expression();
    environment     = procedure->env;
    body_expression = procedure->bod;
    Environment_Replace_Environment(environment,
                                    Frame);
    evaluate_expression_C(body_expression,
                          Grammar_True); }

static NIL_type procedure_body_with_poke_C(FRM_type Frame,
                                           EXP_type Tail_call_expression)
  { if (is_FLS(Tail_call_expression))
      BEGIN_PROTECT(Frame)
        ntcount++;
        poke_environment_and_frame_C();
      END_PROTECT(Frame)
    else
      { tcount++;
        Environment_Release_Current_Frame();
        Thread_Zap(); }
    procedure_body_C(Frame); }

static NIL_type procedure_body_with_push_C(FRM_type Frame,
                                           EXP_type Tail_call_expression)
  { if (is_FLS(Tail_call_expression))
      BEGIN_PROTECT(Frame)
        ntcount++;
        push_environment_and_frame_C();
      END_PROTECT(Frame)
    else
      { tcount++;
        Environment_Release_Current_Frame(); }
    procedure_body_C(Frame); }

/*--------------------------------------------------------------------------------------*/

static NIL_type continue_procedure_C(NIL_type)
  { Prc_type procedure_thread;
    EXP_type expression,
             tail_call_expression,
             value_expression;
    FRM_type frame;
    NBR_type parameter_count_number,
             position_number;
    PRC_type procedure;
    THR_type thread;
    UNS_type parameter_count,
             position;
    VEC_type operand_vector;
    value_expression = Main_Get_Expression();
    thread = Thread_Peek();
    tail_call_expression = Thread_Tail_Position();
    procedure_thread = (Prc_type)thread;
    procedure       = procedure_thread->prc;
    frame           = procedure_thread->frm;
    position_number = procedure_thread->pos;
    position = get_NBR(position_number);
    Environment_Frame_Set(frame,
                          position,
                          value_expression);
    parameter_count_number = procedure->par;
    parameter_count = get_NBR(parameter_count_number);
    if (position == parameter_count)
      { Main_Set_Expression(procedure);
        procedure_body_with_poke_C(frame,
                                   tail_call_expression);
        return; }
    operand_vector = procedure_thread->opd;
    position += 1;
    position = direct_expressions(operand_vector,
                                  frame,
                                  position);
    if (position > 0)
      { thread = Thread_Keep_C();
        procedure_thread = (Prc_type)thread;
        operand_vector = procedure_thread->opd;
        expression = operand_vector[position];
        position_number = make_NBR(position);
        procedure_thread->pos = position_number;
        if (position < parameter_count)
          tail_call_expression = Grammar_False;
        evaluate_expression_C(expression,
                              tail_call_expression);
        return; }
    Main_Set_Expression(procedure);
    procedure_body_with_poke_C(frame,
                               tail_call_expression); }

/*--------------------------------------------------------------------------------------*/

static NIL_type apply_direct_procedure_C(PRC_type Procedure,
                                         EXP_type Argument_expression,
                                         EXP_type Tail_call_expression)
  { EXP_type value_expression;
    FRM_type frame;
    NBR_type frame_size_number,
             parameter_count_number;
    PAI_type argument_list_pair;
    TAG_type tag;
    UNS_type frame_size,
             parameter_count,
             position;
    frame_size_number = Procedure->siz;
    frame_size = get_NBR(frame_size_number);
    BEGIN_DOUBLE_PROTECT(Procedure,
                         Argument_expression)
      frame = Environment_Make_Frame_C(frame_size);
    END_DOUBLE_PROTECT(Procedure,
                       Argument_expression)
    parameter_count_number = Procedure->par;
    parameter_count = get_NBR(parameter_count_number);
    for (position = 1;
         position <= parameter_count;
         position += 1)
      { tag = Grammar_Tag(Argument_expression);
        switch (tag)
          { case NUL_tag:
              evaluation_procedure_error(TFO_error,
                                         Procedure);
            case PAI_tag:
              argument_list_pair = Argument_expression;
              value_expression    = argument_list_pair->car;
              Argument_expression = argument_list_pair->cdr;
              Environment_Frame_Set(frame,
                                    position,
                                    value_expression);
              break;
            default:
              evaluation_procedure_error(ITA_error,
                                         Procedure); }}
    if (!is_NUL(Argument_expression))
      evaluation_procedure_error(TMO_error,
                                 Procedure);
    Main_Set_Expression(Procedure);
    procedure_body_with_push_C(frame,
                               Tail_call_expression); }

static NIL_type apply_procedure_C(VEC_type Operand_vector,
                                  EXP_type Tail_call_expression)
  { Prc_type procedure_thread;
    EXP_type expression,
             tail_call_expression;
    FRM_type frame;
    NBR_type frame_size_number,
             parameter_count_number,
             position_number;
    PRC_type procedure;
    THR_type thread;
    UNS_type operand_vector_size,
             position,
             frame_size,
             parameter_count;
    procedure = Main_Get_Expression();
    parameter_count_number = procedure->par;
    frame_size_number      = procedure->siz;
    parameter_count = get_NBR(parameter_count_number);
    frame_size = get_NBR(frame_size_number);
    operand_vector_size = size_VEC(Operand_vector);
    if (parameter_count > operand_vector_size)
      evaluation_procedure_error(TFO_error,
                                 procedure);
    if (parameter_count < operand_vector_size)
      evaluation_procedure_error(TMO_error,
                                 procedure);
    if (parameter_count == 0)
      { frame = Environment_Make_Frame_C(frame_size);
        procedure_body_with_push_C(frame,
                                   Tail_call_expression);
        return; }
    BEGIN_PROTECT(Operand_vector)
      Thread_Push_C(Continue_procedure_number,
                    Tail_call_expression,
                    Prc_size);
      frame = Environment_Make_Frame_C(frame_size);
    END_PROTECT(Operand_vector)
    procedure = Main_Get_Expression();
    position = direct_expressions(Operand_vector,
                                  frame,
                                  1);
    if (position > 0)
      { position_number = make_NBR(position);
        thread = Thread_Peek();
        procedure_thread = (Prc_type)thread;
        procedure_thread->prc = procedure;
        procedure_thread->opd = Operand_vector;
        procedure_thread->frm = frame;
        procedure_thread->pos = position_number;
        expression = Operand_vector[position];
        tail_call_expression = (position < parameter_count)
                                 ? Grammar_False
                                 : Tail_call_expression;
        evaluate_expression_C(expression,
                              tail_call_expression);
        return; }
    procedure_body_with_poke_C(frame,
                               Tail_call_expression); }

/*--------------------------------------------------------------------------------------*/

static NIL_type initialize_procedure(NIL_type)
  { Continue_procedure_number = Thread_Register(continue_procedure_C); }

/*--------------------------------------------------------------------------------------*/
/*------------------------------------ procedure vararg --------------------------------*/
/*--------------------------------------------------------------------------------------*/

static NBR_type Continue_vararg_number,
                Continue_procedure_vararg_number;

THREAD_BEGIN_FRAME(Prv);
  THREAD_FRAME_SLOT(PRV, prv);
  THREAD_FRAME_SLOT(VEC, opd);
  THREAD_FRAME_SLOT(FRM, frm);
  THREAD_FRAME_SLOT(EXP, lst);
  THREAD_FRAME_SLOT(NBR, pos);
THREAD_END_FRAME(Prv);

/*--------------------------------------------------------------------------------------*/

static NIL_type        evaluate_expression_C(EXP_type,
                                             EXP_type);
static NIL_type poke_environment_and_frame_C(NIL_type);
static NIL_type push_environment_and_frame_C(NIL_type);

/*--------------------------------------------------------------------------------------*/

static NIL_type procedure_vararg_body_C(FRM_type Frame)
  { ENV_type environment;
    EXP_type body_expression;
    PRV_type procedure_vararg;
    procedure_vararg = Main_Get_Expression();
    environment     = procedure_vararg->env;
    body_expression = procedure_vararg->bod;
    Environment_Replace_Environment(environment,
                                    Frame);
    evaluate_expression_C(body_expression,
                          Grammar_True); }

static NIL_type procedure_vararg_body_with_poke_C(FRM_type Frame,
                                                  EXP_type Tail_call_expression)
  { if (is_FLS(Tail_call_expression))
      BEGIN_PROTECT(Frame)
        ntcount++;
        poke_environment_and_frame_C();
      END_PROTECT(Frame)
    else
      { tcount++;
        Environment_Release_Current_Frame();
        Thread_Zap(); }
    procedure_vararg_body_C(Frame); }

static NIL_type procedure_vararg_body_with_push_C(FRM_type Frame,
                                                  EXP_type Tail_call_expression)
  { if (is_FLS(Tail_call_expression))
      BEGIN_PROTECT(Frame)
        ntcount++;
        push_environment_and_frame_C();
      END_PROTECT(Frame)
    else
      { tcount++;
        Environment_Release_Current_Frame(); }
    procedure_vararg_body_C(Frame); }

/*--------------------------------------------------------------------------------------*/

static NIL_type continue_vararg_C(NIL_type)
  { Prv_type procedure_vararg_thread;
    EXP_type expression,
             list_expression,
             tail_call_expression,
             value_expression;
    FRM_type frame;
    NBR_type parameter_count_number,
             position_number;
    PRV_type procedure_vararg;
    THR_type thread;
    UNS_type operand_vector_size,
             parameter_count,
             position;
    VEC_type operand_vector;
    MAIN_CLAIM_DEFAULT_C();
    value_expression = Main_Get_Expression();
    thread = Thread_Peek();
    tail_call_expression = Thread_Tail_Position();
    procedure_vararg_thread = (Prv_type)thread;
    operand_vector   = procedure_vararg_thread->opd;
    list_expression  = procedure_vararg_thread->lst;
    position_number  = procedure_vararg_thread->pos;
    list_expression = make_PAI_M(value_expression,
                                 list_expression);
    position = get_NBR(position_number);
    operand_vector_size = size_VEC(operand_vector);
    if (position == operand_vector_size)
      { procedure_vararg = procedure_vararg_thread->prv;
        Main_Set_Expression(procedure_vararg);
        parameter_count_number = procedure_vararg->par;
        parameter_count = get_NBR(parameter_count_number);
        frame = procedure_vararg_thread->frm;
        list_expression = Main_Reverse(list_expression);
        Environment_Frame_Set(frame,
                              parameter_count + 1,
                              list_expression);
        procedure_vararg_body_with_poke_C(frame,
                                          tail_call_expression);
        return; }
    position += 1;
    BEGIN_PROTECT(list_expression)
      thread = Thread_Keep_C();
    END_PROTECT(list_expression)
    procedure_vararg_thread = (Prv_type)thread;
    operand_vector = procedure_vararg_thread->opd;
    position_number = make_NBR(position);
    procedure_vararg_thread->lst = list_expression;
    procedure_vararg_thread->pos = position_number;
    expression = operand_vector[position];
    if (position < operand_vector_size)
      tail_call_expression = Grammar_False;
    evaluate_expression_C(expression,
                          tail_call_expression); }

static NIL_type continue_procedure_vararg_C(NIL_type)
  { Prv_type procedure_vararg_thread;
    EXP_type expression,
             tail_call_expression,
             value_expression;
    FRM_type frame;
    NBR_type parameter_count_number,
             position_number;
    PRV_type procedure_vararg;
    THR_type thread;
    UNS_type operand_vector_size,
             parameter_count,
             position;
    VEC_type operand_vector;
    value_expression = Main_Get_Expression();
    thread = Thread_Peek();
    tail_call_expression = Thread_Tail_Position();
    procedure_vararg_thread = (Prv_type)thread;
    procedure_vararg = procedure_vararg_thread->prv;
    operand_vector   = procedure_vararg_thread->opd;
    frame            = procedure_vararg_thread->frm;
    position_number  = procedure_vararg_thread->pos;
    position = get_NBR(position_number);
    Environment_Frame_Set(frame,
                          position,
                          value_expression);
    operand_vector_size = size_VEC(operand_vector);
    if (position == operand_vector_size)
      { Main_Set_Expression(procedure_vararg);
        position += 1;
        Environment_Frame_Set(frame,
                              position,
                              Grammar_Null);
        procedure_vararg_body_with_poke_C(frame,
                                          tail_call_expression);
        return; }
    parameter_count_number = procedure_vararg->par;
    parameter_count = get_NBR(parameter_count_number);
    if (position < parameter_count)
      thread = Thread_Keep_C();
    else
      thread = Thread_Patch_C(Continue_vararg_number);
    procedure_vararg_thread = (Prv_type)thread;
    operand_vector = procedure_vararg_thread->opd;
    position += 1;
    expression = operand_vector[position];
    position_number = make_NBR(position);
    procedure_vararg_thread->pos = position_number;
    if (position < operand_vector_size)
      tail_call_expression = Grammar_False;
    evaluate_expression_C(expression,
                          tail_call_expression); }

/*--------------------------------------------------------------------------------------*/

static NIL_type apply_direct_procedure_vararg_C(PRV_type Procedure_vararg,
                                                EXP_type Argument_expression,
                                                EXP_type Tail_call_expression)
  { EXP_type value_expression;
    FRM_type frame;
    NBR_type frame_size_number,
             parameter_count_number;
    PAI_type argument_list_pair;
    UNS_type frame_size,
             parameter_count,
             position;
    frame_size_number = Procedure_vararg->siz;
    frame_size = get_NBR(frame_size_number);
    BEGIN_DOUBLE_PROTECT(Procedure_vararg,
                         Argument_expression)
      frame = Environment_Make_Frame_C(frame_size);
    END_DOUBLE_PROTECT(Procedure_vararg,
                       Argument_expression)
    parameter_count_number = Procedure_vararg->par;
    parameter_count = get_NBR(parameter_count_number);
    for (position = 1;
         position <= parameter_count;
         position += 1)
      { if (!is_PAI(Argument_expression))
          evaluation_procedure_vararg_error(TFO_error,
                                            Procedure_vararg);
        argument_list_pair = Argument_expression;
        value_expression    = argument_list_pair->car;
        Argument_expression = argument_list_pair->cdr;
        Environment_Frame_Set(frame,
                              position,
                              value_expression); }
    Environment_Frame_Set(frame,
                          position,
                          Argument_expression);
    while (is_PAI(Argument_expression))
      { argument_list_pair = Argument_expression;
        Argument_expression = argument_list_pair->cdr; }
    if (!is_NUL(Argument_expression))
      evaluation_procedure_vararg_error(ITA_error,
                                        Procedure_vararg);
    Main_Set_Expression(Procedure_vararg);
    procedure_vararg_body_with_push_C(frame,
                                      Tail_call_expression); }

/*--------------------------------------------------------------------------------------*/

static NIL_type apply_procedure_vararg_C(VEC_type Operand_vector,
                                         EXP_type Tail_call_expression)
  { Prv_type procedure_vararg_thread;
    EXP_type expression,
             tail_call_expression;
    FRM_type frame;
    NBR_type c_function_number,
             frame_size_number,
             parameter_count_number;
    PRV_type procedure_vararg;
    THR_type thread;
    UNS_type operand_vector_size,
             frame_size,
             parameter_count;
    procedure_vararg = Main_Get_Expression();
    parameter_count_number = procedure_vararg->par;
    frame_size_number      = procedure_vararg->siz;
    parameter_count = get_NBR(parameter_count_number);
    frame_size = get_NBR(frame_size_number);
    operand_vector_size = size_VEC(Operand_vector);
    if (parameter_count > operand_vector_size)
      evaluation_procedure_vararg_error(TFO_error,
                                        procedure_vararg);
    if (operand_vector_size == 0)
      { frame = Environment_Make_Frame_C(frame_size);
        Environment_Frame_Set(frame,
                              1,
                              Grammar_Null);
        procedure_vararg_body_with_push_C(frame,
                                          Tail_call_expression);
        return; }
    c_function_number = (parameter_count == 0)
                          ? Continue_vararg_number
                          : Continue_procedure_vararg_number;
    BEGIN_PROTECT(Operand_vector)
      Thread_Push_C(c_function_number,
                    Tail_call_expression,
                    Prv_size);
      frame = Environment_Make_Frame_C(frame_size);
    END_PROTECT(Operand_vector)
    procedure_vararg = Main_Get_Expression();
    thread = Thread_Peek();
    procedure_vararg_thread = (Prv_type)thread;
    procedure_vararg_thread->prv = procedure_vararg;
    procedure_vararg_thread->opd = Operand_vector;
    procedure_vararg_thread->frm = frame;
    procedure_vararg_thread->lst = Grammar_Null;
    procedure_vararg_thread->pos = Grammar_One_Number;
    expression = Operand_vector[1];
    tail_call_expression = (operand_vector_size > 1)
                             ? Grammar_False
                             : Tail_call_expression;
    evaluate_expression_C(expression,
                          tail_call_expression); }

/*--------------------------------------------------------------------------------------*/

static NIL_type initialize_procedure_vararg(NIL_type)
  { Continue_procedure_vararg_number = Thread_Register(continue_procedure_vararg_C);
    Continue_vararg_number = Thread_Register(continue_vararg_C); }

/*--------------------------------------------------------------------------------------*/
/*--------------------------------------------------------------------------------------*/
/*----------------------------------- E V A L U A T E ----------------------------------*/
/*--------------------------------------------------------------------------------------*/
/*--------------------------------------------------------------------------------------*/

/*--------------------------------------------------------------------------------------*/
/*------------------------------------------ and ---------------------------------------*/
/*--------------------------------------------------------------------------------------*/

static NBR_type Continue_and_number;

THREAD_BEGIN_FRAME(And);
  THREAD_FRAME_SLOT(VEC, prd);
  THREAD_FRAME_SLOT(NBR, pos);
THREAD_END_FRAME(And);

/*--------------------------------------------------------------------------------------*/

static NIL_type evaluate_expression_C(EXP_type,
                                      EXP_type);

/*--------------------------------------------------------------------------------------*/

static NIL_type continue_and_C(NIL_type)
  { And_type and_thread;
    EXP_type expression,
             predicate_value_expression;
    NBR_type position_number;
    THR_type thread;
    UNS_type position,
             predicate_vector_size;
    VEC_type predicate_vector;
    predicate_value_expression = Main_Get_Expression();
    if (is_FLS(predicate_value_expression))
      { Thread_Zap();
        return; }
    if (is_TRU(predicate_value_expression))
      { thread = Thread_Peek();
        and_thread = (And_type)thread;
        predicate_vector = and_thread->prd;
        position_number  = and_thread->pos;
        predicate_vector_size = size_VEC(predicate_vector);
        position = get_NBR(position_number);
        if (position == predicate_vector_size)
          { Thread_Zap();
            return; }
        thread = Thread_Keep_C();
        and_thread = (And_type)thread;
        position += 1;
        position_number = make_NBR(position);
        and_thread->pos = position_number;
        expression = predicate_vector[position];
        evaluate_expression_C(expression,
                              Grammar_False);
        return; }
    evaluation_keyword_error(NAB_error,
                             Pool_And); }

/*--------------------------------------------------------------------------------------*/

static NIL_type evaluate_and_C(EXP_type Tail_call_expression)
  { And_type and_thread;
    AND_type and;
    EXP_type expression;
    THR_type thread;
    UNS_type predicate_vector_size;
    VEC_type predicate_vector;
    and = Main_Get_Expression();
    predicate_vector = and->prd;
    predicate_vector_size = size_VEC(predicate_vector);
    if (predicate_vector_size == 0)
      { Main_Set_Expression(Grammar_True);
        return; }
    thread = Thread_Push_C(Continue_and_number,
                           Tail_call_expression,
                           And_size);
    and = Main_Get_Expression();
    predicate_vector = and->prd;
    and_thread = (And_type)thread;
    and_thread->prd = predicate_vector;
    and_thread->pos = Grammar_One_Number;
    expression = predicate_vector[1];
    evaluate_expression_C(expression,
                          Grammar_False); }

/*--------------------------------------------------------------------------------------*/

static NIL_type initialize_and(NIL_type)
  { Continue_and_number = Thread_Register(continue_and_C); }

/*--------------------------------------------------------------------------------------*/
/*------------------------------------- application ------------------------------------*/
/*--------------------------------------------------------------------------------------*/

static NBR_type Continue_application_number;

THREAD_BEGIN_FRAME(Apl);
  THREAD_FRAME_SLOT(VEC, opd);
THREAD_END_FRAME(Apl);

/*--------------------------------------------------------------------------------------*/

static NIL_type evaluate_expression_C(EXP_type,
                                      EXP_type);

/*--------------------------------------------------------------------------------------*/

static NIL_type apply_C(VEC_type Operand_vector,
                        EXP_type Tail_call_expression)
  { EXP_type operator_value_expression;
    TAG_type tag;
    operator_value_expression = Main_Get_Expression();
    tag = Grammar_Tag(operator_value_expression);
    tag_count[tag]++;
    switch (tag)
      { case CNT_tag:
          apply_continuation_C(Operand_vector,
                               Tail_call_expression);
          return;
        case NAT_tag:
          apply_native_C(Operand_vector,
                         Tail_call_expression);
          return;
        case PRC_tag:
          apply_procedure_C(Operand_vector,
                            Tail_call_expression);
          return;
        case PRV_tag:
          apply_procedure_vararg_C(Operand_vector,
                                   Tail_call_expression);
          return;
        default:
          evaluation_rawstring_error(OPR_error,
                                     Pool_Application_Rawstring); }}

static NIL_type continue_application_C(NIL_type)
  { Apl_type application_thread;
    EXP_type tail_call_expression;
    THR_type thread;
    VEC_type operand_vector;
    thread = Thread_Peek();
    tail_call_expression = Thread_Tail_Position();
    application_thread = (Apl_type)thread;
    operand_vector = application_thread->opd;
    Thread_Zap();
    apply_C(operand_vector,
            tail_call_expression); }

static NIL_type direct_apply_C(EXP_type Procedure_expression,
                               EXP_type Argument_expression,
                               EXP_type Tail_call_expression)
  { TAG_type tag;
    tag = Grammar_Tag(Procedure_expression);
    tag_count[tag]++;
    switch (tag)
      { case CNT_tag:
          apply_direct_continuation(Procedure_expression,
                                    Argument_expression,
                                    Tail_call_expression);
          return;
        case NAT_tag:
          apply_direct_native_C(Procedure_expression,
                                Argument_expression,
                                Tail_call_expression);
          return;
        case PRC_tag:
          apply_direct_procedure_C(Procedure_expression,
                                   Argument_expression,
                                   Tail_call_expression);
          return;
        case PRV_tag:
          apply_direct_procedure_vararg_C(Procedure_expression,
                                          Argument_expression,
                                          Tail_call_expression);
          return;
        default:
          evaluation_rawstring_error(OPR_error,
                                     Pool_Application_Rawstring); }}

/*--------------------------------------------------------------------------------------*/

static NIL_type evaluate_application_C(EXP_type Tail_call_expression)
  { Apl_type application_thread;
    APL_type application;
    EXP_type expression;
    THR_type thread;
    VEC_type operand_vector;
    thread = Thread_Push_C(Continue_application_number,
                           Tail_call_expression,
                           Apl_size);
    application = Main_Get_Expression();
    expression     = application->exp;
    operand_vector = application->opd;
    application_thread = (Apl_type)thread;
    application_thread->opd = operand_vector;
    evaluate_expression_C(expression,
                          Grammar_False); }

/*--------------------------------------------------------------------------------------*/

static NIL_type initialize_application(NIL_type)
  { Continue_application_number = Thread_Register(continue_application_C); }

/*--------------------------------------------------------------------------------------*/
/*---------------------------------------- begin ---------------------------------------*/
/*--------------------------------------------------------------------------------------*/

static NIL_type evaluate_expression_C(EXP_type,
                                      EXP_type);

/*--------------------------------------------------------------------------------------*/

static NIL_type evaluate_begin_C(EXP_type Tail_call_expression)
  { BEG_type begin;
    EXP_type body_expression;
    begin = Main_Get_Expression();
    body_expression = begin->bod;
    evaluate_expression_C(body_expression,
                          Tail_call_expression); }

/*--------------------------------------------------------------------------------------*/
/*---------------------------------------- cond ----------------------------------------*/
/*--------------------------------------------------------------------------------------*/

static NBR_type Continue_cond_number,
                Continue_cond_else_number;

THREAD_BEGIN_FRAME(Cnd);
  THREAD_FRAME_SLOT(VEC, prd);
  THREAD_FRAME_SLOT(VEC, bod);
  THREAD_FRAME_SLOT(NBR, pos);
THREAD_END_FRAME(Cnd);

THREAD_BEGIN_FRAME(Cne);
  THREAD_FRAME_SLOT(VEC, prd);
  THREAD_FRAME_SLOT(VEC, bod);
  THREAD_FRAME_SLOT(EXP, els);
  THREAD_FRAME_SLOT(NBR, pos);
THREAD_END_FRAME(Cne);

/*--------------------------------------------------------------------------------------*/

static NIL_type evaluate_expression_C(EXP_type,
                                      EXP_type);

/*--------------------------------------------------------------------------------------*/

static NIL_type continue_cond_C(NIL_type)
  { Cnd_type cond_thread;
    EXP_type body_expression,
             predicate_expression,
             predicate_value_expression,
             tail_call_expression;
    NBR_type position_number;
    THR_type thread;
    UNS_type position,
             body_vector_size;
    VEC_type body_vector,
             predicate_vector;
    predicate_value_expression = Main_Get_Expression();
    thread = Thread_Peek();
    tail_call_expression = Thread_Tail_Position();
    cond_thread = (Cnd_type)thread;
    body_vector     = cond_thread->bod;
    position_number = cond_thread->pos;
    position = get_NBR(position_number);
    if (is_FLS(predicate_value_expression))
      { body_vector_size = size_VEC(body_vector);
        if (position == body_vector_size)
          { Thread_Zap();
            Main_Set_Expression(Grammar_Unspecified); }
        else
          { thread = Thread_Keep_C();
            cond_thread = (Cnd_type)thread;
            position += 1;
            position_number = make_NBR(position);
            predicate_vector = cond_thread->prd;
            cond_thread->pos = position_number;
            predicate_expression = predicate_vector[position];
            evaluate_expression_C(predicate_expression,
                                  Grammar_False); }
        return; }
    if (is_TRU(predicate_value_expression))
      { Thread_Zap();
        body_expression = body_vector[position];
        evaluate_expression_C(body_expression,
                              tail_call_expression);
        return; }
    evaluation_keyword_error(NAB_error,
                             Pool_Cond); }

static NIL_type continue_cond_else_C(NIL_type)
  { Cne_type cond_else_thread;
    EXP_type body_expression,
             else_expression,
             predicate_expression,
             predicate_value_expression,
             tail_call_expression;
    NBR_type position_number;
    THR_type thread;
    UNS_type position,
             body_vector_size;
    VEC_type body_vector,
             predicate_vector;
    predicate_value_expression = Main_Get_Expression();
    thread = Thread_Peek();
    tail_call_expression = Thread_Tail_Position();
    cond_else_thread = (Cne_type)thread;
    body_vector     = cond_else_thread->bod;
    position_number = cond_else_thread->pos;
    position = get_NBR(position_number);
    if (is_FLS(predicate_value_expression))
      { body_vector_size = size_VEC(body_vector);
        if (position == body_vector_size)
          { else_expression = cond_else_thread->els;
            Thread_Zap();
            evaluate_expression_C(else_expression,
                                  tail_call_expression); }
        else
          { thread = Thread_Keep_C();
            cond_else_thread = (Cne_type)thread;
            position += 1;
            position_number = make_NBR(position);
            predicate_vector = cond_else_thread->prd;
            cond_else_thread->pos = position_number;
            predicate_expression = predicate_vector[position];
            evaluate_expression_C(predicate_expression,
                                  Grammar_False); }
        return; }
    if (is_TRU(predicate_value_expression))
      { Thread_Zap();
        body_expression = body_vector[position];
        evaluate_expression_C(body_expression,
                              tail_call_expression);
        return; }
    evaluation_keyword_error(NAB_error,
                             Pool_Cond); }

/*--------------------------------------------------------------------------------------*/

static NIL_type evaluate_cond_C(EXP_type Tail_call_expression)
  { Cnd_type cond_thread;
    CND_type cond;
    EXP_type predicate_expression;
    THR_type thread;
    VEC_type body_vector,
             predicate_vector;
    thread = Thread_Push_C(Continue_cond_number,
                           Tail_call_expression,
                           Cnd_size);
    cond = Main_Get_Expression();
    predicate_vector = cond->prd;
    body_vector      = cond->bod;
    cond_thread = (Cnd_type)thread;
    cond_thread->prd = predicate_vector;
    cond_thread->bod = body_vector;
    cond_thread->pos = Grammar_One_Number;
    predicate_expression = predicate_vector[1];
    evaluate_expression_C(predicate_expression,
                          Grammar_False); }

static NIL_type evaluate_cond_else_C(EXP_type Tail_call_expression)
  { Cne_type cond_else_thread;
    CNE_type cond_else;
    EXP_type else_expression,
             predicate_expression;
    THR_type thread;
    UNS_type predicate_vector_size;
    VEC_type body_vector,
             predicate_vector;
    cond_else = Main_Get_Expression();
    predicate_vector = cond_else->prd;
    predicate_vector_size = size_VEC(predicate_vector);
    if (predicate_vector_size == 0)
      { else_expression  = cond_else->els;
        evaluate_expression_C(else_expression,
                              Tail_call_expression);
        return; }
    thread = Thread_Push_C(Continue_cond_else_number,
                           Tail_call_expression,
                           Cne_size);
    cond_else = Main_Get_Expression();
    predicate_vector = cond_else->prd;
    body_vector      = cond_else->bod;
    else_expression  = cond_else->els;
    cond_else_thread = (Cne_type)thread;
    cond_else_thread->prd = predicate_vector;
    cond_else_thread->bod = body_vector;
    cond_else_thread->els = else_expression;
    cond_else_thread->pos = Grammar_One_Number;
    predicate_expression = predicate_vector[1];
    evaluate_expression_C(predicate_expression,
                          Grammar_False); }

/*--------------------------------------------------------------------------------------*/

static NIL_type initialize_cond(NIL_type)
  { Continue_cond_number = Thread_Register(continue_cond_C);
    Continue_cond_else_number = Thread_Register(continue_cond_else_C); }

/*--------------------------------------------------------------------------------------*/
/*---------------------------------------- define --------------------------------------*/
/*--------------------------------------------------------------------------------------*/

static NBR_type Continue_define_variable_number;

THREAD_BEGIN_FRAME(Dfv);
  THREAD_FRAME_SLOT(NBR, ofs);
THREAD_END_FRAME(Dfv);

/*--------------------------------------------------------------------------------------*/

static EXP_type evaluate_direct_expression(EXP_type);
static NIL_type      evaluate_expression_C(EXP_type,
                                           EXP_type);

/*--------------------------------------------------------------------------------------*/

static NIL_type continue_define_variable(NIL_type)
  { Dfv_type define_variable_thread;
    EXP_type value_expression;
    NBR_type offset_number;
    THR_type thread;
    value_expression = Main_Get_Expression();
    thread = Thread_Peek();
    define_variable_thread = (Dfv_type)thread;
    offset_number = define_variable_thread->ofs;
    Thread_Zap();
    Environment_Local_Set(value_expression,
                          offset_number); }

/*--------------------------------------------------------------------------------------*/

static NIL_type evaluate_define_function_C(EXP_type Tail_call_expression)
  { DFF_type define_function;
    ENV_type environment;
    EXP_type body_expression;
    NBR_type frame_size_number,
             offset_number,
             parameter_count_number;
    PRC_type procedure;
    SYM_type function_symbol;
    VEC_type symbol_vector;
    environment = Environment_Extend_C();
    BEGIN_PROTECT(environment);
      MAIN_CLAIM_DEFAULT_C();
    END_PROTECT(environment)
    define_function = Main_Get_Expression();
    function_symbol        = define_function->nam;
    offset_number          = define_function->ofs;
    parameter_count_number = define_function->par;
    frame_size_number      = define_function->siz;
    body_expression        = define_function->bod;
    symbol_vector          = define_function->smv;
    procedure = make_PRC_M(function_symbol,
                           parameter_count_number,
                           frame_size_number,
                           body_expression,
                           environment,
                           symbol_vector);
    Main_Set_Expression(procedure);
    Environment_Local_Set(procedure,
                          offset_number); }

/*--------------------------------------------------------------------------------------*/

static NIL_type evaluate_define_function_vararg_C(EXP_type Tail_call_expression)
  { DFZ_type define_function_vararg;
    ENV_type environment;
    EXP_type body_expression;
    NBR_type frame_size_number,
             offset_number,
             parameter_count_number;
    PRV_type procedure_vararg;
    SYM_type function_symbol;
    VEC_type symbol_vector;
    environment = Environment_Extend_C();
    BEGIN_PROTECT(environment);
      MAIN_CLAIM_DEFAULT_C();
    END_PROTECT(environment)
    define_function_vararg = Main_Get_Expression();
    function_symbol        = define_function_vararg->nam;
    offset_number          = define_function_vararg->ofs;
    parameter_count_number = define_function_vararg->par;
    frame_size_number      = define_function_vararg->siz;
    body_expression        = define_function_vararg->bod;
    symbol_vector          = define_function_vararg->smv;
    procedure_vararg = make_PRV_M(function_symbol,
                                  parameter_count_number,
                                  frame_size_number,
                                  body_expression,
                                  environment,
                                  symbol_vector);
    Main_Set_Expression(procedure_vararg);
    Environment_Local_Set(procedure_vararg,
                          offset_number); }

/*--------------------------------------------------------------------------------------*/

static NIL_type evaluate_define_variable_C(EXP_type Tail_call_expression)
  { Dfv_type define_variable_thread;
    DFV_type define_variable;
    EXP_type expression,
             value_expression;
    NBR_type offset_number;
    THR_type thread;
    define_variable = Main_Get_Expression();
    expression = define_variable->exp;
    value_expression = evaluate_direct_expression(expression);
    if (is_USP(value_expression))
      { thread = Thread_Push_C(Continue_define_variable_number,
                               Grammar_False,
                               Dfv_size);
        define_variable = Main_Get_Expression();
        offset_number = define_variable->ofs;
        expression    = define_variable->exp;
        define_variable_thread = (Dfv_type)thread;
        define_variable_thread->ofs = offset_number;
        evaluate_expression_C(expression,
                              Grammar_False); }
    else
      { offset_number = define_variable->ofs;
        Environment_Local_Set(value_expression,
                              offset_number);
        Main_Set_Expression(value_expression); }}

/*--------------------------------------------------------------------------------------*/

static NIL_type initialize_define_variable(NIL_type)
  { Continue_define_variable_number = Thread_Register(continue_define_variable); }

/*--------------------------------------------------------------------------------------*/
/*---------------------------------------- delay ---------------------------------------*/
/*--------------------------------------------------------------------------------------*/

static NIL_type evaluate_delay_C(EXP_type Tail_call_expression)
  { DEL_type delay;
    ENV_type environment;
    EXP_type expression;
    NBR_type frame_size_number;
    PRM_type promise;
    VEC_type symbol_vector;
    environment = Environment_Extend_C();
    BEGIN_PROTECT(environment);
      MAIN_CLAIM_DEFAULT_C();
    END_PROTECT(environment)
    delay = Main_Get_Expression();
    frame_size_number = delay->siz;
    expression        = delay->exp;
    symbol_vector     = delay->smv;
    promise = make_PRM_M(frame_size_number,
                         expression,
                         environment,
                         symbol_vector);
    Main_Set_Expression(promise); }

/*--------------------------------------------------------------------------------------*/
/*---------------------------------- global application --------------------------------*/
/*--------------------------------------------------------------------------------------*/

static NIL_type apply_C(VEC_type,
                        EXP_type);

/*--------------------------------------------------------------------------------------*/

static EXP_type global_variable(GLB_type Global)
  { EXP_type value_expression;
    NBR_type offset_number,
             scope_number;
    SYM_type variable_symbol;
    UNS_type offset,
             scope;
    scope_number  = Global->scp;
    offset_number = Global->ofs;
    value_expression = Environment_Global_Get(scope_number,
                                              offset_number);
    if (is_UDF(value_expression))
      { scope = get_NBR(scope_number);
        if (scope == 1)
          { offset = get_NBR(offset_number);
            variable_symbol = Environment_Global_Symbol_Name(offset);
            evaluation_symbol_error(UDF_error,
                                    variable_symbol); }
        evaluation_error(UDF_error); }
    return value_expression; }

/*--------------------------------------------------------------------------------------*/

static NIL_type evaluate_global_application_C(EXP_type Tail_call_expression)
  { GLA_type global_application;
    EXP_type operator_expression;
    GLB_type global;
    VEC_type operand_vector;
    global_application = Main_Get_Expression();
    global         = global_application->glb;
    operand_vector = global_application->opd;
    operator_expression = global_variable(global);
    Main_Set_Expression(operator_expression);
    apply_C(operand_vector,
            Tail_call_expression); }

/*--------------------------------------------------------------------------------------*/
/*----------------------------------- global variable ----------------------------------*/
/*--------------------------------------------------------------------------------------*/

static EXP_type global_variable(GLB_type );

/*--------------------------------------------------------------------------------------*/

static NIL_type evaluate_global_variable(EXP_type Tail_call_expression)
  { EXP_type value_expression;
    GLB_type global;
    global = Main_Get_Expression();
    value_expression = global_variable(global);
    Main_Set_Expression(value_expression); }

/*--------------------------------------------------------------------------------------*/
/*----------------------------------------- if -----------------------------------------*/
/*--------------------------------------------------------------------------------------*/

static NBR_type Continue_if_double_number,
                Continue_if_single_number;

THREAD_BEGIN_FRAME(Ifd);
  THREAD_FRAME_SLOT(EXP, cns);
  THREAD_FRAME_SLOT(EXP, alt);
THREAD_END_FRAME(Ifd);

THREAD_BEGIN_FRAME(Ifs);
  THREAD_FRAME_SLOT(EXP, cns);
THREAD_END_FRAME(Ifs);

/*--------------------------------------------------------------------------------------*/

static NIL_type evaluate_expression_C(EXP_type,
                                      EXP_type);

/*--------------------------------------------------------------------------------------*/

static NIL_type continue_if_double_C(NIL_type)
  { Ifd_type if_thread;
    EXP_type expression,
             predicate_value_expression,
             tail_call_expression;
    THR_type thread;
    predicate_value_expression = Main_Get_Expression();
    thread = Thread_Peek();
    tail_call_expression = Thread_Tail_Position();
    if_thread = (Ifd_type)thread;
    if (is_FLS(predicate_value_expression))
      { expression = if_thread->alt;
        Thread_Zap();
        evaluate_expression_C(expression,
                              tail_call_expression);
        return; }
    if (is_TRU(predicate_value_expression))
      { expression = if_thread->cns;
        Thread_Zap();
        evaluate_expression_C(expression,
                              tail_call_expression);
        return; }
    evaluation_keyword_error(NAB_error,
                             Pool_If); }

static NIL_type continue_if_single_C(NIL_type)
  { Ifs_type if_thread;
    EXP_type expression,
             predicate_value_expression,
             tail_call_expression;
    THR_type thread;
    predicate_value_expression = Main_Get_Expression();
    thread = Thread_Peek();
    tail_call_expression = Thread_Tail_Position();
    if_thread = (Ifs_type)thread;
    if (is_FLS(predicate_value_expression))
      { Thread_Zap();
        Main_Set_Expression(Grammar_Unspecified);
        return; }
    if (is_TRU(predicate_value_expression))
      { expression = if_thread->cns;
        Thread_Zap();
        evaluate_expression_C(expression,
                              tail_call_expression);
        return; }
    evaluation_keyword_error(NAB_error,
                             Pool_If); }

/*--------------------------------------------------------------------------------------*/

static NIL_type evaluate_if_double_C(EXP_type Tail_call_expression)
  { Ifd_type if_thread;
    IFD_type if_double;
    EXP_type alternative_expression,
             consequent_expression,
             predicate_expression;
    THR_type thread;
    thread = Thread_Push_C(Continue_if_double_number,
                           Tail_call_expression,
                           Ifd_size);
    if_double = Main_Get_Expression();
    predicate_expression   = if_double->prd;
    consequent_expression  = if_double->cns;
    alternative_expression = if_double->alt;
    if_thread = (Ifd_type)thread;
    if_thread->cns = consequent_expression;
    if_thread->alt = alternative_expression;
    evaluate_expression_C(predicate_expression,
                          Grammar_False); }

/*--------------------------------------------------------------------------------------*/

static NIL_type evaluate_if_single_C(EXP_type Tail_call_expression)
  { Ifs_type if_thread;
    IFS_type if_single;
    EXP_type consequent_expression,
             predicate_expression;
    THR_type thread;
    thread = Thread_Push_C(Continue_if_single_number,
                           Tail_call_expression,
                           Ifs_size);
    if_single = Main_Get_Expression();
    predicate_expression  = if_single->prd;
    consequent_expression = if_single->cns;
    if_thread = (Ifs_type)thread;
    if_thread->cns = consequent_expression;
    evaluate_expression_C(predicate_expression,
                          Grammar_False); }

static NIL_type initialize_if(NIL_type)
  { Continue_if_double_number = Thread_Register(continue_if_double_C);
    Continue_if_single_number = Thread_Register(continue_if_single_C); }

/*--------------------------------------------------------------------------------------*/
/*---------------------------------------- lambda --------------------------------------*/
/*--------------------------------------------------------------------------------------*/

static NIL_type evaluate_lambda_C(EXP_type Tail_call_expression)
  { ENV_type environment;
    EXP_type body_expression;
    LMB_type lambda;
    NBR_type frame_size_number,
             parameter_count_number;
    PRC_type procedure;
    VEC_type symbol_vector;
    environment = Environment_Extend_C();
    BEGIN_PROTECT(environment);
      MAIN_CLAIM_DEFAULT_C();
    END_PROTECT(environment)
    lambda = Main_Get_Expression();
    parameter_count_number = lambda->par;
    frame_size_number      = lambda->siz;
    body_expression        = lambda->bod;
    symbol_vector          = lambda->smv;
    procedure = make_PRC_M(Pool_Anonymous_Symbol,
                           parameter_count_number,
                           frame_size_number,
                           body_expression,
                           environment,
                           symbol_vector);
    Main_Set_Expression(procedure); }

static NIL_type evaluate_lambda_vararg_C(EXP_type Tail_call_expression)
  { ENV_type environment;
    EXP_type body_expression;
    LMV_type lambda_vararg;
    NBR_type frame_size_number,
             parameter_count_number;
    PRV_type procedure_vararg;
    VEC_type symbol_vector;
    environment = Environment_Extend_C();
    BEGIN_PROTECT(environment);
      MAIN_CLAIM_DEFAULT_C();
    END_PROTECT(environment)
    lambda_vararg = Main_Get_Expression();
    parameter_count_number = lambda_vararg->par;
    frame_size_number      = lambda_vararg->siz;
    body_expression        = lambda_vararg->bod;
    symbol_vector          = lambda_vararg->smv;
    procedure_vararg = make_PRV_M(Pool_Anonymous_Symbol,
                                  parameter_count_number,
                                  frame_size_number,
                                  body_expression,
                                  environment,
                                  symbol_vector);
    Main_Set_Expression(procedure_vararg); }

/*--------------------------------------------------------------------------------------*/
/*------------------------------------------ let ---------------------------------------*/
/*--------------------------------------------------------------------------------------*/

static NBR_type Continue_let_number;

THREAD_BEGIN_FRAME(Let);
  THREAD_FRAME_SLOT(VEC, bnd);
  THREAD_FRAME_SLOT(VEC, bod);
  THREAD_FRAME_SLOT(FRM, frm);
  THREAD_FRAME_SLOT(NBR, pos);
THREAD_END_FRAME(Let);

/*--------------------------------------------------------------------------------------*/

static NIL_type        evaluate_expression_C(EXP_type,
                                             EXP_type);
static NIL_type push_environment_and_frame_C(NIL_type);

/*--------------------------------------------------------------------------------------*/

static NIL_type continue_let_C(NIL_type)
  { Let_type let_thread;
    EXP_type body_expression,
             expression,
             value_expression;
    FRM_type frame;
    NBR_type position_number;
    THR_type thread;
    UNS_type binding_vector_size,
             position;
    VEC_type binding_vector;
    value_expression = Main_Get_Expression();
    thread = Thread_Peek();
    let_thread = (Let_type)thread;
    binding_vector  = let_thread->bnd;
    frame           = let_thread->frm;
    position_number = let_thread->pos;
    binding_vector_size = size_VEC(binding_vector);
    position = get_NBR(position_number);
    Environment_Frame_Set(frame,
                          position,
                          value_expression);
    if (position == binding_vector_size)
      { body_expression = let_thread->bod;
        Thread_Zap();
        evaluate_expression_C(body_expression,
                              Grammar_True);
        return; }
    position += 1;
    position = direct_expressions(binding_vector,
                                  frame,
                                  position);
    if (position > 0)
      { thread = Thread_Keep_C();
        let_thread = (Let_type)thread;
        binding_vector = let_thread->bnd;
        position_number = make_NBR(position);
        let_thread->pos = position_number;
        expression = binding_vector[position];
        evaluate_expression_C(expression,
                              Grammar_False);
        return; }
    body_expression = let_thread->bod;
    Thread_Zap();
    evaluate_expression_C(body_expression,
                          Grammar_True); }

/*--------------------------------------------------------------------------------------*/

static NIL_type evaluate_let_C(EXP_type Tail_call_expression)
  { Let_type let_thread;
    ENV_type environment;
    EXP_type body_expression,
             expression;
    FRM_type frame;
    LET_type let;
    NBR_type frame_size_number,
             position_number;
    THR_type thread;
    UNS_type binding_vector_size,
             frame_size,
             position;
    VEC_type binding_vector;
    let = Main_Get_Expression();
    binding_vector           = let->bnd;
    frame_size_number = let->siz;
    frame_size = get_NBR(frame_size_number);
    binding_vector_size = size_VEC(binding_vector);
    frame = Environment_Make_Frame_C(frame_size);
    BEGIN_PROTECT(frame);
      if (is_FLS(Tail_call_expression))
        { ntcount++;
          push_environment_and_frame_C(); }
      else
        tcount++;
      environment = Environment_Extend_C();
      frame = Stack_Peek();
      Environment_Replace_Environment(environment,
                                      frame);
      if (binding_vector_size == 0)
        { let = Main_Get_Expression();
          Stack_Zap();
          body_expression = let->bod;
          evaluate_expression_C(body_expression,
                                Grammar_True);
          return; }
      thread = Thread_Push_C(Continue_let_number,
                             Tail_call_expression,
                             Let_size);
    END_PROTECT(frame)
    let = Main_Get_Expression();
    binding_vector  = let->bnd;
    body_expression = let->bod;
    position = direct_expressions(binding_vector,
                                  frame,
                                  1);
    if (position > 0)
      { let_thread = (Let_type)thread;
        let_thread->bnd = binding_vector;
        let_thread->bod = body_expression;
        let_thread->frm = frame;
        position_number = make_NBR(position);
        let_thread->pos = position_number;
        expression = binding_vector[position];
        evaluate_expression_C(expression,
                              Grammar_False);
        return; }
    Thread_Zap();
    evaluate_expression_C(body_expression,
                          Grammar_True); }

/*--------------------------------------------------------------------------------------*/

static NIL_type initialize_let(NIL_type)
  { Continue_let_number = Thread_Register(continue_let_C); }

/*--------------------------------------------------------------------------------------*/
/*-------------------------------------- literals --------------------------------------*/
/*--------------------------------------------------------------------------------------*/

static EXP_type clone_literal_C(EXP_type);

static PAI_type clone_list_literal_C(PAI_type Literal_pair)
  { EXP_type car_expression,
             cdr_expression,
             cloned_expression;
    PAI_type cloned_pair;
    UNS_type count;
    count = 0;
    do
      { count += 1;
        Stack_Push_C(Literal_pair);
        Literal_pair = Stack_Peek();
        car_expression = Literal_pair->car;
        cloned_expression = clone_literal_C(car_expression);
        Stack_Push_C(cloned_expression);
        Literal_pair = Stack_Reduce();
        Literal_pair = Literal_pair->cdr; }
    while (is_PAI(Literal_pair));
    cloned_expression = clone_literal_C(Literal_pair);
    Stack_Push_C(cloned_expression);
    for (;
         count > 0;
         count -= 1)
      { MAIN_CLAIM_DEFAULT_C();
        cdr_expression = Stack_Pop();
        car_expression = Stack_Peek();
        cloned_pair = make_PAI_M(car_expression,
                                 cdr_expression);
        Stack_Poke(cloned_pair); }
    Stack_Zap();
    return cloned_pair; }

static STR_type clone_string_literal_C(STR_type Literal_string)
  { RWS_type rawstring;
    STR_type cloned_string;
    UNS_type size;
    rawstring = Literal_string->rws;
    size = size_STR(rawstring);
    BEGIN_PROTECT(Literal_string);
      MAIN_CLAIM_DYNAMIC_C(size);
    END_PROTECT(Literal_string)
    rawstring = Literal_string->rws;
    cloned_string = make_STR_M(rawstring);
    return cloned_string; }

static VEC_type clone_vector_literal_C(VEC_type Literal_vector)
  { EXP_type cloned_expression,
             literal_expression;
    UNS_type position,
             size;
    VEC_type cloned_vector;
    size = size_VEC(Literal_vector);
    if (size == 0)
      return Grammar_Empty_Vector;
    BEGIN_PROTECT(Literal_vector);
      MAIN_CLAIM_DYNAMIC_C(size);
    END_PROTECT(Literal_vector)
    cloned_vector = make_VEC_M(size);
    for (position = 1;
         position <= size;
         position += 1)
      { literal_expression = Literal_vector[position];
        BEGIN_PROTECT(Literal_vector);
          cloned_expression = clone_literal_C(literal_expression);
        END_PROTECT(Literal_vector)
        cloned_vector[position] = cloned_expression; }
    return cloned_vector; }

static EXP_type clone_literal_C(EXP_type Literal_expression)
  { EXP_type cloned_expression;
    TAG_type tag;
    tag = Grammar_Tag(Literal_expression);
    switch (tag)
      { case PAI_tag:
          cloned_expression = clone_list_literal_C(Literal_expression);
          return cloned_expression;
        case STR_tag:
          cloned_expression = clone_string_literal_C(Literal_expression);
          return cloned_expression;
        case VEC_tag:
          cloned_expression = clone_vector_literal_C(Literal_expression);
          return cloned_expression;
        case CHA_tag:
        case FLS_tag:
        case KID_tag:
        case NBR_tag:
        case NUL_tag:
        case REA_tag:
        case SYM_tag:
        case TRU_tag:
          return Literal_expression;
        default:
          evaluation_keyword_error(IXT_error,
                                   Pool_Quote);
          return Grammar_Unspecified; }}

/*--------------------------------------------------------------------------------------*/

static NIL_type evaluate_immutable_literal(EXP_type Tail_call_expression)
  { return; }

static NIL_type evaluate_mutable_literal(EXP_type Tail_call_expression)
  { EXP_type literal_expression,
             cloned_expression;
    literal_expression = Main_Get_Expression();
    cloned_expression = clone_literal_C(literal_expression);
    Main_Set_Expression(cloned_expression); }

/*--------------------------------------------------------------------------------------*/
/*---------------------------------- local application ---------------------------------*/
/*--------------------------------------------------------------------------------------*/

static NIL_type apply_C(VEC_type,
                        EXP_type);

/*--------------------------------------------------------------------------------------*/

static EXP_type local_variable(LCL_type Local)
  { EXP_type value_expression;
    NBR_type offset_number;
    SYM_type variable_symbol;
    UNS_type offset;
    offset_number = Local->ofs;
    value_expression = Environment_Local_Get(offset_number);
    if (is_UDF(value_expression))
      { offset = get_NBR(offset_number);
        variable_symbol = Environment_Global_Symbol_Name(offset);
        evaluation_symbol_error(UDF_error,
                                variable_symbol); }
    return value_expression; }

/*--------------------------------------------------------------------------------------*/

static NIL_type evaluate_local_application_C(EXP_type Tail_call_expression)
  { LCA_type local_application;
    LCL_type local;
    EXP_type operator_expression;
    VEC_type operand_vector;
    local_application = Main_Get_Expression();
    local          = local_application->lcl;
    operand_vector = local_application->opd;
    operator_expression = local_variable(local);
    Main_Set_Expression(operator_expression);
    apply_C(operand_vector,
            Tail_call_expression); }

/*--------------------------------------------------------------------------------------*/
/*------------------------------------ local variable ----------------------------------*/
/*--------------------------------------------------------------------------------------*/

static EXP_type local_variable(LCL_type);

/*--------------------------------------------------------------------------------------*/

static NIL_type evaluate_local_variable(EXP_type Tail_call_expression)
  { EXP_type value_expression;
    LCL_type local;
    local = Main_Get_Expression();
    value_expression = local_variable(local);
    Main_Set_Expression(value_expression); }

/*--------------------------------------------------------------------------------------*/
/*------------------------------------------ or ----------------------------------------*/
/*--------------------------------------------------------------------------------------*/

static NBR_type Continue_or_number;

THREAD_BEGIN_FRAME(Orr);
  THREAD_FRAME_SLOT(VEC, prd);
  THREAD_FRAME_SLOT(NBR, pos);
THREAD_END_FRAME(Orr);

/*--------------------------------------------------------------------------------------*/

static NIL_type evaluate_expression_C(EXP_type,
                                      EXP_type);

/*--------------------------------------------------------------------------------------*/

static NIL_type continue_or_C(NIL_type)
  { Orr_type or_thread;
    EXP_type expression,
             predicate_value_expression;
    NBR_type position_number;
    THR_type thread;
    UNS_type position,
             predicate_vector_size;
    VEC_type predicate_vector;
    predicate_value_expression = Main_Get_Expression();
    if (is_FLS(predicate_value_expression))
      { thread = Thread_Peek();
        or_thread = (Orr_type)thread;
        predicate_vector = or_thread->prd;
        position_number  = or_thread->pos;
        predicate_vector_size = size_VEC(predicate_vector);
        position = get_NBR(position_number);
        if (position == predicate_vector_size)
          { Thread_Zap();
            return; }
        thread = Thread_Keep_C();
        or_thread = (Orr_type)thread;
        position += 1;
        position_number = make_NBR(position);
        or_thread->pos = position_number;
        expression = predicate_vector[position];
        evaluate_expression_C(expression,
                              Grammar_False);
        return; }
    if (is_TRU(predicate_value_expression))
      { Thread_Zap();
        return; }
    evaluation_keyword_error(NAB_error,
                             Pool_Or); }

/*--------------------------------------------------------------------------------------*/

static NIL_type evaluate_or_C(EXP_type Tail_call_expression)
  { Orr_type or_thread;
    EXP_type expression;
    ORR_type or;
    THR_type thread;
    UNS_type predicate_vector_size;
    VEC_type predicate_vector;
    or = Main_Get_Expression();
    predicate_vector = or->prd;
    predicate_vector_size = size_VEC(predicate_vector);
    if (predicate_vector_size == 0)
      { Main_Set_Expression(Grammar_False);
        return; }
    thread = Thread_Push_C(Continue_or_number,
                           Tail_call_expression,
                           Orr_size);
    or = Main_Get_Expression();
    predicate_vector = or->prd;
    or_thread = (Orr_type)thread;
    or_thread->prd = predicate_vector;
    or_thread->pos = Grammar_One_Number;
    expression = predicate_vector[1];
    evaluate_expression_C(expression,
                          Grammar_False); }

/*--------------------------------------------------------------------------------------*/

static NIL_type initialize_or(NIL_type)
  { Continue_or_number = Thread_Register(continue_or_C); }

/*--------------------------------------------------------------------------------------*/
/*------------------------------------- quasiquote -------------------------------------*/
/*--------------------------------------------------------------------------------------*/

static NBR_type Continue_quasiquote_list_number,
                Continue_quasiquote_tail_number,
                Continue_unquote_splicing_number;

THREAD_BEGIN_FRAME(Qqu);
  THREAD_FRAME_SLOT(PAI, fst);
  THREAD_FRAME_SLOT(PAI, dst);
  THREAD_FRAME_SLOT(EXP, res);
THREAD_END_FRAME(Qqu);

/*--------------------------------------------------------------------------------------*/

static NIL_type quasiquote_C(BLN_type);

/*--------------------------------------------------------------------------------------*/

static NIL_type continue_quasiquote_list_C(NIL_type)
  { Qqu_type quasiquote_thread;
    EXP_type head_expression,
             quasiquoted_expression,
             residue_expression,
             tail_expression;
    PAI_type destination_pair,
             new_pair,
             source_pair;
    THR_type thread;
    quasiquoted_expression = Main_Get_Expression();
    thread = Thread_Peek();
    quasiquote_thread = (Qqu_type)thread;
    destination_pair   = quasiquote_thread->dst;
    residue_expression = quasiquote_thread->res;
    destination_pair->car = quasiquoted_expression;
    if (is_PAI(residue_expression))
      { Thread_Keep_C();
        MAIN_CLAIM_DEFAULT_C();
        new_pair = make_PAI_M(Grammar_Null,
                              Grammar_Null);
        thread = Thread_Peek();
        quasiquote_thread = (Qqu_type)thread;
        destination_pair = quasiquote_thread->dst;
        source_pair      = quasiquote_thread->res;
        destination_pair->cdr = new_pair;
        head_expression = source_pair->car;
        tail_expression = source_pair->cdr;
        quasiquote_thread->dst = new_pair;
        quasiquote_thread->res = tail_expression;
        Main_Set_Expression(head_expression);
        quasiquote_C(Grammar_Is_True); }
    else
      { Main_Set_Expression(residue_expression);
        Thread_Patch_C(Continue_quasiquote_tail_number);
        quasiquote_C(Grammar_Is_False); }}

static NIL_type continue_quasiquote_tail_C(NIL_type)
  { Qqu_type quasiquote_thread;
    EXP_type quasiquoted_expression;
    PAI_type destination_pair,
             first_pair;
    THR_type thread;
    quasiquoted_expression = Main_Get_Expression();
    thread = Thread_Peek();
    quasiquote_thread = (Qqu_type)thread;
    first_pair       = quasiquote_thread->fst;
    destination_pair = quasiquote_thread->dst;
    destination_pair->cdr = quasiquoted_expression;
    Thread_Zap();
    Main_Set_Expression(first_pair); }

static NIL_type continue_unquote_splicing_C(NIL_type)
  { EXP_type unquoted_splicing_expression;
    unquoted_splicing_expression = Main_Get_Expression();
    if (!is_NUL(unquoted_splicing_expression))
      if (!is_PAI(unquoted_splicing_expression))
        evaluation_keyword_error(NAP_error,
                                 Pool_Unquote_Splicing);
    evaluation_keyword_error(NYI_error,
                             Pool_Unquote_Splicing); }

/*--------------------------------------------------------------------------------------*/

static NIL_type quasiquote_list_C(NIL_type)
  { Qqu_type quasiquote_thread;
    EXP_type head_expression,
             tail_expression;
    PAI_type destination_pair,
             first_pair,
             source_pair;
    THR_type thread;
    Thread_Push_C(Continue_quasiquote_list_number,
                  Grammar_False,
                  Qqu_size);
    MAIN_CLAIM_DEFAULT_C();
    first_pair = make_PAI_M(Grammar_Null,
                            Grammar_Null);
    source_pair = Main_Get_Expression();
    destination_pair = first_pair;
    head_expression = source_pair->car;
    tail_expression = source_pair->cdr;
    thread = Thread_Peek();
    quasiquote_thread = (Qqu_type)thread;
    quasiquote_thread->fst = first_pair;
    quasiquote_thread->dst = destination_pair;
    quasiquote_thread->res = tail_expression;
    Main_Set_Expression(head_expression);
    quasiquote_C(Grammar_Is_True); }

static NIL_type quasiquote_string_C(NIL_type)
  { RWS_type rawstring;
    STR_type quoted_string,
             string;
    UNS_type size;
    string = Main_Get_Expression();
    rawstring = string->rws;
    size = size_STR(rawstring);
    MAIN_CLAIM_DYNAMIC_C(size);
    string = Main_Get_Expression();
    rawstring = string->rws;
    quoted_string = make_STR_M(rawstring);
    Main_Set_Expression(quoted_string); }

static NIL_type quasiquote_vector_C(NIL_type)
  { UNS_type index,
             size;
    VEC_type quoted_vector,
             vector;
    vector = Main_Get_Expression();
    size = size_VEC(vector);
    if (size == 0)
      quoted_vector = Grammar_Empty_Vector;
    else
      { MAIN_CLAIM_DYNAMIC_C(size);
        quoted_vector = make_VEC_Z(size);
        vector = Main_Get_Expression();
        for (index = 1;
             index <= size;
             index++)
          quoted_vector[index] = vector[index]; }
    Main_Set_Expression(quoted_vector); }

static NIL_type unquote_C(NIL_type)
  { EXP_type expression;
    UNQ_type unquote;
    unquote = Main_Get_Expression();
    expression = unquote->exp;
    evaluate_expression_C(expression,
                          Grammar_False);  }

static NIL_type unquote_splicing_C(NIL_type)
  { EXP_type expression;
    UQS_type unquote_splicing;
    Thread_Patch_C(Continue_unquote_splicing_number);
    unquote_splicing = Main_Get_Expression();
    expression = unquote_splicing->exp;
    evaluate_expression_C(expression,
                          Grammar_False);  }

/*--------------------------------------------------------------------------------------*/

static NIL_type quasiquote_C(BLN_type In_list)
  { EXP_type expression;
    TAG_type tag;
    expression = Main_Get_Expression();
    tag = Grammar_Tag(expression);
    switch (tag)
      { case PAI_tag:
          quasiquote_list_C();
          return;
        case STR_tag:
          quasiquote_string_C();
          return;
        case UNQ_tag:
          unquote_C();
          return;
        case UQS_tag:
          if (In_list)
            unquote_splicing_C();
          else
            evaluation_keyword_error(UQS_error,
                                     Pool_Quasiquote);
          return;
        case VEC_tag:
          quasiquote_vector_C();
          return;
        case CHA_tag:
        case FLS_tag:
        case KID_tag:
        case NBR_tag:
        case NUL_tag:
        case REA_tag:
        case SYM_tag:
        case TRU_tag:
          return;
        default:
          evaluation_keyword_error(IXT_error,
                                   Pool_Quasiquote); }}

/*--------------------------------------------------------------------------------------*/

static NIL_type evaluate_quasiquote(EXP_type Tail_call_expression)
  { EXP_type expression;
    QQU_type quasiquote;
    quasiquote = Main_Get_Expression();
    expression = quasiquote->exp;
    Main_Set_Expression(expression);
    quasiquote_C(Grammar_Is_False); }

/*--------------------------------------------------------------------------------------*/

static NIL_type initialize_quasiquote(NIL_type)
  { Continue_quasiquote_list_number = Thread_Register(continue_quasiquote_list_C);
    Continue_quasiquote_tail_number = Thread_Register(continue_quasiquote_tail_C);
    Continue_unquote_splicing_number = Thread_Register(continue_unquote_splicing_C); }

/*--------------------------------------------------------------------------------------*/
/*--------------------------------------- sequence -------------------------------------*/
/*--------------------------------------------------------------------------------------*/

static NBR_type Continue_sequence_number;

THREAD_BEGIN_FRAME(Seq);
  THREAD_FRAME_SLOT(SEQ, seq);
  THREAD_FRAME_SLOT(NBR, pos);
THREAD_END_FRAME(Seq);

/*--------------------------------------------------------------------------------------*/

static NIL_type evaluate_expression_C(EXP_type,
                                      EXP_type);

/*--------------------------------------------------------------------------------------*/

static NIL_type continue_sequence_C(NIL_type)
  { Seq_type sequence_thread;
    EXP_type expression,
             tail_call_expression;
    NBR_type position_number;
    SEQ_type sequence;
    THR_type thread;
    UNS_type position,
             sequence_size;
    thread = Thread_Peek();
    tail_call_expression = Thread_Tail_Position();
    sequence_thread = (Seq_type)thread;
    sequence        = sequence_thread->seq;
    position_number = sequence_thread->pos;
    position = get_NBR(position_number);
    sequence_size = size_SEQ(sequence);
    position += 1;
    if (position < sequence_size)
      { thread = Thread_Keep_C();
        sequence_thread = (Seq_type)thread;
        sequence = sequence_thread->seq;
        position_number = make_NBR(position);
        sequence_thread->pos = position_number;
        tail_call_expression = Grammar_False; }
    else
      Thread_Zap();
    expression = sequence[position];
    evaluate_expression_C(expression,
                          tail_call_expression); }

/*--------------------------------------------------------------------------------------*/

static NIL_type evaluate_sequence_C(EXP_type Tail_call_expression)
  { Seq_type sequence_thread;
    EXP_type expression,
             tail_call_expression;
    SEQ_type sequence;
    THR_type thread;
    UNS_type sequence_size;
    sequence = Main_Get_Expression();
    sequence_size = size_SEQ(sequence);
    if (sequence_size > 1)
      { thread = Thread_Push_C(Continue_sequence_number,
                               Tail_call_expression,
                               Seq_size);
        sequence = Main_Get_Expression();
        sequence_thread = (Seq_type)thread;
        sequence_thread->seq = sequence;
        sequence_thread->pos = Grammar_One_Number;
        tail_call_expression = Grammar_False; }
    else
      tail_call_expression = Tail_call_expression;
    expression = sequence[1];
    evaluate_expression_C(expression,
                          tail_call_expression); }

/*--------------------------------------------------------------------------------------*/

static NIL_type initialize_sequence(NIL_type)
  { Continue_sequence_number = Thread_Register(continue_sequence_C); }

/*--------------------------------------------------------------------------------------*/
/*-------------------------------------- set global ------------------------------------*/
/*--------------------------------------------------------------------------------------*/

static NBR_type Continue_set_global_number;

THREAD_BEGIN_FRAME(Stg);
  THREAD_FRAME_SLOT(NBR, scp);
  THREAD_FRAME_SLOT(NBR, ofs);
THREAD_END_FRAME(Stg);

/*--------------------------------------------------------------------------------------*/

static EXP_type evaluate_direct_expression(EXP_type);
static NIL_type      evaluate_expression_C(EXP_type,
                                           EXP_type);

/*--------------------------------------------------------------------------------------*/

static NIL_type continue_set_global(NIL_type)
  { Stg_type set_global_thread;
    EXP_type value_expression;
    NBR_type offset_number,
             scope_number;
    THR_type thread;
    value_expression = Main_Get_Expression();
    thread = Thread_Peek();
    set_global_thread = (Stg_type)thread;
    scope_number  = set_global_thread->scp;
    offset_number = set_global_thread->ofs;
    Thread_Zap();
    Environment_Global_Set(value_expression,
                           scope_number,
                           offset_number); }

/*--------------------------------------------------------------------------------------*/

static NIL_type evaluate_set_global_C(EXP_type Tail_call_expression)
  { Stg_type set_global_thread;
    EXP_type expression,
             value_expression;
    NBR_type offset_number,
             scope_number;
    STG_type set_global;
    THR_type thread;
    set_global = Main_Get_Expression();
    expression = set_global->exp;
    value_expression = evaluate_direct_expression(expression);
    if (is_USP(value_expression))
      { thread = Thread_Push_C(Continue_set_global_number,
                               Grammar_False,
                               Stg_size);
        set_global = Main_Get_Expression();
        scope_number  = set_global->scp;
        offset_number = set_global->ofs;
        expression    = set_global->exp;
        set_global_thread = (Stg_type)thread;
        set_global_thread->scp = scope_number;
        set_global_thread->ofs = offset_number;
        evaluate_expression_C(expression,
                              Grammar_False); }
    else
      { scope_number  = set_global->scp;
        offset_number = set_global->ofs;
        Environment_Global_Set(value_expression,
                               scope_number,
                               offset_number);
        Main_Set_Expression(value_expression); }}

/*--------------------------------------------------------------------------------------*/

static NIL_type initialize_set_global(NIL_type)
  { Continue_set_global_number = Thread_Register(continue_set_global); }

/*--------------------------------------------------------------------------------------*/
/*-------------------------------------- set local -------------------------------------*/
/*--------------------------------------------------------------------------------------*/

static NBR_type Continue_set_local_number;

THREAD_BEGIN_FRAME(Stl);
  THREAD_FRAME_SLOT(NBR, ofs);
THREAD_END_FRAME(Stl);

/*--------------------------------------------------------------------------------------*/

static EXP_type evaluate_direct_expression(EXP_type);
static NIL_type      evaluate_expression_C(EXP_type,
                                           EXP_type);

/*--------------------------------------------------------------------------------------*/

static NIL_type continue_set_local(NIL_type)
  { Stl_type set_local_thread;
    EXP_type value_expression;
    NBR_type offset_number;
    THR_type thread;
    value_expression = Main_Get_Expression();
    thread = Thread_Peek();
    set_local_thread = (Stl_type)thread;
    offset_number = set_local_thread->ofs;
    Thread_Zap();
    Environment_Local_Set(value_expression,
                          offset_number); }

/*--------------------------------------------------------------------------------------*/

static NIL_type evaluate_set_local_C(EXP_type Tail_call_expression)
  { Stl_type set_local_thread;
    EXP_type expression,
             value_expression;
    NBR_type offset_number;
    STL_type set_local;
    THR_type thread;
    set_local = Main_Get_Expression();
    expression = set_local->exp;
    value_expression = evaluate_direct_expression(expression);
    if (is_USP(value_expression))
      { thread = Thread_Push_C(Continue_set_local_number,
                               Grammar_False,
                               Stl_size);
        set_local = Main_Get_Expression();
        offset_number = set_local->ofs;
        expression    = set_local->exp;
        set_local_thread = (Stl_type)thread;
        set_local_thread->ofs = offset_number;
        evaluate_expression_C(expression,
                              Grammar_False); }
    else
      { offset_number = set_local->ofs;
        Environment_Local_Set(value_expression,
                              offset_number);
        Main_Set_Expression(value_expression); }}

/*--------------------------------------------------------------------------------------*/

static NIL_type initialize_set_local(NIL_type)
  { Continue_set_local_number = Thread_Register(continue_set_local); }

/*--------------------------------------------------------------------------------------*/
/*---------------------------------------- thunk ---------------------------------------*/
/*--------------------------------------------------------------------------------------*/

static NIL_type        evaluate_expression_C(EXP_type,
                                             EXP_type);
static NIL_type push_environment_and_frame_C(NIL_type);

/*--------------------------------------------------------------------------------------*/

static NIL_type evaluate_thunk_C(EXP_type Tail_call_expression)
  { ENV_type environment;
    EXP_type body_expression;
    FRM_type frame;
    NBR_type frame_size_number;
    THK_type thunk;
    UNS_type frame_size;
    thunk = Main_Get_Expression();
    frame_size_number = thunk->siz;
    frame_size = get_NBR(frame_size_number);
    if (is_FLS(Tail_call_expression))
      { ntcount++;
        push_environment_and_frame_C(); }
    else
      tcount++;
    environment = Environment_Extend_C();
    BEGIN_PROTECT(environment);
      frame = Environment_Make_Frame_C(frame_size);
    END_PROTECT(environment)
    Environment_Replace_Environment(environment,
                                    frame);
    thunk = Main_Get_Expression();
    body_expression = thunk->bod;
    evaluate_expression_C(body_expression,
                          Grammar_True); }

/*--------------------------------------------------------------------------------------*/
/*---------------------------------------- while ---------------------------------------*/
/*--------------------------------------------------------------------------------------*/

static NBR_type Continue_while_body_number,
                Continue_while_predicate_number;

THREAD_BEGIN_FRAME(Whi);
  THREAD_FRAME_SLOT(EXP, prd);
  THREAD_FRAME_SLOT(EXP, bod);
  THREAD_FRAME_SLOT(EXP, res);
THREAD_END_FRAME(Whi);

/*--------------------------------------------------------------------------------------*/

static NIL_type evaluate_expression_C(EXP_type,
                                      EXP_type);

/*--------------------------------------------------------------------------------------*/

static NIL_type continue_while_body_C(NIL_type)
  { Whi_type while_thread;
    EXP_type predicate_expression,
             value_expression;
    THR_type thread;
    thread = Thread_Patch_C(Continue_while_predicate_number);
    while_thread = (Whi_type)thread;
    value_expression = Main_Get_Expression();
    while_thread->res = value_expression;
    predicate_expression = while_thread->prd;
    evaluate_expression_C(predicate_expression,
                          Grammar_False); }

static NIL_type continue_while_predicate_C(NIL_type)
  { Whi_type while_thread;
    EXP_type body_expression,
             predicate_value_expression,
             result_value_expression;
    THR_type thread;
    predicate_value_expression = Main_Get_Expression();
    if (is_FLS(predicate_value_expression))
      { thread = Thread_Peek();
        while_thread = (Whi_type)thread;
        result_value_expression = while_thread->res;
        Thread_Zap();
        Main_Set_Expression(result_value_expression);
        return; }
    if (is_TRU(predicate_value_expression))
      { thread = Thread_Patch_C(Continue_while_body_number);
        while_thread = (Whi_type)thread;
        body_expression = while_thread->bod;
        evaluate_expression_C(body_expression,
                              Grammar_False);
        return; }
    evaluation_keyword_error(NAB_error,
                             Pool_While); }

/*--------------------------------------------------------------------------------------*/

static NIL_type evaluate_while_C(EXP_type Tail_call_expression)
  { Whi_type while_thread;
    EXP_type body_expression,
             predicate_expression;
    THR_type thread;
    WHI_type while_;
    thread = Thread_Push_C(Continue_while_predicate_number,
                           Grammar_False,
                           Whi_size);
    while_ = Main_Get_Expression();
    predicate_expression = while_->prd;
    body_expression      = while_->bod;
    while_thread = (Whi_type)thread;
    while_thread->prd = predicate_expression;
    while_thread->bod = body_expression;
    while_thread->res = Grammar_Unspecified;
    evaluate_expression_C(predicate_expression,
                          Grammar_False); }

/*--------------------------------------------------------------------------------------*/

static NIL_type initialize_while(NIL_type)
  { Continue_while_body_number = Thread_Register(continue_while_body_C);
    Continue_while_predicate_number = Thread_Register(continue_while_predicate_C); }

/*--------------------------------------------------------------------------------------*/
/*-------------------------------------- expression ------------------------------------*/
/*--------------------------------------------------------------------------------------*/

static NIL_type                    evaluate_and_C(EXP_type);
static NIL_type            evaluate_application_C(EXP_type);
static NIL_type                  evaluate_begin_C(EXP_type);
static NIL_type                   evaluate_cond_C(EXP_type);
static NIL_type              evaluate_cond_else_C(EXP_type);
static NIL_type                  evaluate_delay_C(EXP_type);
static NIL_type        evaluate_define_function_C(EXP_type);
static NIL_type        evaluate_define_variable_C(EXP_type);
static NIL_type evaluate_define_function_vararg_C(EXP_type);
static NIL_type     evaluate_global_application_C(EXP_type);
static NIL_type          evaluate_global_variable(EXP_type);
static NIL_type              evaluate_if_double_C(EXP_type);
static NIL_type              evaluate_if_single_C(EXP_type);
static NIL_type      evaluate_local_application_C(EXP_type);
static NIL_type           evaluate_local_variable(EXP_type);
static NIL_type                    evaluate_let_C(EXP_type);
static NIL_type                 evaluate_lambda_C(EXP_type);
static NIL_type          evaluate_lambda_vararg_C(EXP_type);
static NIL_type                     evaluate_or_C(EXP_type);
static NIL_type               evaluate_quasiquote(EXP_type);
static NIL_type               evaluate_sequence_C(EXP_type);
static NIL_type             evaluate_set_global_C(EXP_type);
static NIL_type              evaluate_set_local_C(EXP_type);
static NIL_type                  evaluate_thunk_C(EXP_type);
static NIL_type                  evaluate_while_C(EXP_type);
static NIL_type        evaluate_immutable_literal(EXP_type);
static NIL_type          evaluate_mutable_literal(EXP_type);
static EXP_type                   global_variable(GLB_type);
static EXP_type                    local_variable(LCL_type);
static NIL_type      push_environment_and_frame_C(NIL_type);

/*--------------------------------------------------------------------------------------*/

static EXP_type evaluate_direct_expression(EXP_type Expression)
  { EXP_type expression;
    TAG_type tag;
    tag = Grammar_Tag(Expression);
    tag_count[tag]++;
    switch (tag)
      { case GLB_tag:
          dcount++;
          expression = global_variable(Expression);
          return expression;
        case LCL_tag:
          dcount++;
          expression = local_variable(Expression);
          return expression;
        case CHA_tag:
        case CNT_tag:
        case FLS_tag:
        case FRC_tag:
        case NAT_tag:
        case NBR_tag:
        case NUL_tag:
        case PAI_tag:
        case PRC_tag:
        case PRM_tag:
        case PRV_tag:
        case REA_tag:
        case STR_tag:
        case SYM_tag:
        case TRU_tag:
        case USP_tag:
        case VEC_tag:
          dcount++;
          return Expression;
        default:
          ndcount++;
          return Grammar_Unspecified; }}

/*--------------------------------------------------------------------------------------*/

static NIL_type evaluate_expression_C(EXP_type Expression,
                                      EXP_type Tail_call_expression)
  { TAG_type tag;
    Main_Set_Expression(Expression);
    tag = Grammar_Tag(Expression);
    tag_count[tag]++;
    switch (tag)
      { case AND_tag:
          evaluate_and_C(Tail_call_expression);
          return;
        case APL_tag:
          evaluate_application_C(Tail_call_expression);
          return;
        case BEG_tag:
          evaluate_begin_C(Tail_call_expression);
          return;
        case CND_tag:
          evaluate_cond_C(Tail_call_expression);
          return;
        case CNE_tag:
          evaluate_cond_else_C(Tail_call_expression);
          return;
        case DEL_tag:
          evaluate_delay_C(Tail_call_expression);
          return;
        case DFF_tag:
          evaluate_define_function_C(Tail_call_expression);
          return;
        case DFV_tag:
          evaluate_define_variable_C(Tail_call_expression);
          return;
        case DFZ_tag:
          evaluate_define_function_vararg_C(Tail_call_expression);
          return;
        case GLA_tag:
          evaluate_global_application_C(Tail_call_expression);
          return;
        case GLB_tag:
          evaluate_global_variable(Tail_call_expression);
          return;
        case IFD_tag:
          evaluate_if_double_C(Tail_call_expression);
          return;
        case IFS_tag:
          evaluate_if_single_C(Tail_call_expression);
          return;
        case LCA_tag:
          evaluate_local_application_C(Tail_call_expression);
          return;
        case LCL_tag:
          evaluate_local_variable(Tail_call_expression);
          return;
        case LET_tag:
          evaluate_let_C(Tail_call_expression);
          return;
        case LMB_tag:
          evaluate_lambda_C(Tail_call_expression);
          return;
        case LMV_tag:
          evaluate_lambda_vararg_C(Tail_call_expression);
          return;
        case ORR_tag:
          evaluate_or_C(Tail_call_expression);
          return;
        case QQU_tag:
          evaluate_quasiquote(Tail_call_expression);
          return;
        case SEQ_tag:
          evaluate_sequence_C(Tail_call_expression);
          return;
        case STG_tag:
          evaluate_set_global_C(Tail_call_expression);
          return;
        case STL_tag:
          evaluate_set_local_C(Tail_call_expression);
          return;
        case THK_tag:
          evaluate_thunk_C(Tail_call_expression);
          return;
        case WHI_tag:
          evaluate_while_C(Tail_call_expression);
          return;
        case CHA_tag:
        case FLS_tag:
        case KID_tag:
        case NBR_tag:
        case NUL_tag:
        case REA_tag:
        case SYM_tag:
        case TRU_tag:
          evaluate_immutable_literal(Tail_call_expression);
          return;
        case PAI_tag:
        case STR_tag:
        case VEC_tag:
          evaluate_mutable_literal(Tail_call_expression);
          return;
        case CNT_tag:
        case FRC_tag:
        case NAT_tag:
        case PRC_tag:
        case PRM_tag:
        case PRV_tag:
        case UDF_tag:
        case USP_tag:
          evaluation_error(IXT_error);
        default:
          evaluation_error(IXT_error); }}

static NIL_type evaluate_promise_C(PRM_type Promise,
                                   EXP_type Tail_call_expression)
  { ENV_type environment;
    EXP_type expression;
    FRM_type frame;
    NBR_type frame_size_number;
    UNS_type frame_size;
    frame_size_number = Promise->siz;
    frame_size = get_NBR(frame_size_number);
    BEGIN_PROTECT(Promise);
      if (is_FLS(Tail_call_expression))
        { ntcount++;
          push_environment_and_frame_C(); }
      else
        { tcount++;
          Environment_Release_Current_Frame(); }
      frame = Environment_Make_Frame_C(frame_size);
    END_PROTECT(Promise)
    expression  = Promise->exp;
    environment = Promise->env;
    Environment_Replace_Environment(environment,
                                    frame);
    evaluate_expression_C(expression,
                          Grammar_True); }

/*----------------------------------- public functions ---------------------------------*/

NIL_type Evaluate_Apply_C(EXP_type Procedure_expression,
                          EXP_type Argument_expression,
                          EXP_type Tail_call_expression)
  { direct_apply_C(Procedure_expression,
                   Argument_expression,
                   Tail_call_expression); }

NIL_type Evaluate_Eval_C(EXP_type Expression,
                         EXP_type Tail_call_expression)
  { evaluate_expression_C(Expression,
                          Tail_call_expression); }

NIL_type Evaluate_Promise_C(PRM_type Promise,
                            EXP_type Tail_call_expression)
  { evaluate_promise_C(Promise,
                       Tail_call_expression); }

/*--------------------------------------------------------------------------------------*/


NIL_type Evaluate_Initialize(NIL_type)
  { initialize_and();
    initialize_application();
    initialize_body();
    initialize_cond();
    initialize_continuation();
    initialize_define_variable();
    initialize_if();
    initialize_let();
    initialize_native();
    initialize_or();
    initialize_procedure();
    initialize_procedure_vararg();
    initialize_sequence();
    initialize_set_global();
    initialize_set_local();
    initialize_quasiquote();
    initialize_while();

    slipken_persist(Continue_and_number);
    slipken_persist(Continue_application_number);
    slipken_persist(Continue_body_number);
    slipken_persist(Continue_cond_else_number);
    slipken_persist(Continue_cond_number);
    slipken_persist(Continue_continuation_number);
    slipken_persist(Continue_define_variable_number);
    slipken_persist(Continue_if_double_number);
    slipken_persist(Continue_if_single_number);
    slipken_persist(Continue_let_number);
    slipken_persist(Continue_native_number);
    slipken_persist(Continue_or_number);
    slipken_persist(Continue_procedure_number);
    slipken_persist(Continue_procedure_vararg_number);
    slipken_persist(Continue_quasiquote_list_number);
    slipken_persist(Continue_quasiquote_tail_number);
    slipken_persist(Continue_sequence_number);
    slipken_persist(Continue_set_global_number);
    slipken_persist(Continue_set_local_number);
    slipken_persist(Continue_unquote_splicing_number);
    slipken_persist(Continue_vararg_number);
    slipken_persist(Continue_while_body_number);
    slipken_persist(Continue_while_predicate_number);
  }
