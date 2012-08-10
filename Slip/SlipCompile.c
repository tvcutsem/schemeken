                     /*---------------------------------------------*/
                     /*                  >>>Slip<<<                 */
                     /*                 Theo D'Hondt                */
                     /*          VUB Software Languages Lab         */
                     /*                  (c) 2012                   */
                     /*---------------------------------------------*/
                     /*   version 13: completion and optimization   */
                     /*---------------------------------------------*/
                     /*                   compiler                  */
                     /*---------------------------------------------*/

#include "SlipMain.h"

#include "SlipCompile.h"
#include "SlipDictionary.h"
#include "SlipEnvironment.h"
#include "SlipPool.h"
#include "SlipStack.h"

/*----------------------------------- private functions --------------------------------*/

static NIL_type compilation_symbol_error(BYT_type,
                                         SYM_type);

/*--------------------------------------------------------------------------------------*/

static NIL_type compilation_keyword_error(BYT_type Error,
                                          KEY_type Key)
  { SYM_type symbol;
    symbol = Pool_Keyword_Symbol(Key);
    compilation_symbol_error(Error,
                             symbol); }

static NIL_type compilation_rawstring_error(BYT_type Error,
                                            RWS_type Rawstring)
  { Main_Error_Handler(Error,
                       Rawstring); }

static NIL_type compilation_symbol_error(BYT_type Error,
                                         SYM_type Symbol)
  { RWS_type rawstring;
    rawstring = Symbol->rws;
    compilation_rawstring_error(Error,
                                rawstring); }

/*--------------------------------------------------------------------------------------*/
/*----------------------------------------- and ----------------------------------------*/
/*--------------------------------------------------------------------------------------*/

static NIL_type compile_expression_C(EXP_type);

/*--------------------------------------------------------------------------------------*/

static VEC_type build_symbol_vector_M(UNS_type Frame_size)
  { VEC_type symbol_vector;
    if (Frame_size == 0)
      symbol_vector = Grammar_Empty_Vector;
    else
      symbol_vector = make_VEC_Z(Frame_size);
    Dictionary_Setup_Symbol_Vector(symbol_vector);
    return symbol_vector; }

static NIL_type expression_inline_C(EXP_type Expression)
  { EXP_type compiled_expression;
    NBR_type frame_size_number;
    THK_type thunk;
    UNS_type frame_size;
    VEC_type symbol_vector;
    Stack_Push_C(Expression);
    Dictionary_Enter_Nested_Scope_C();
    Expression = Stack_Peek();
    compile_expression_C(Expression);
    frame_size = Dictionary_Get_Frame_Size();
    if (frame_size == 0)
      { Stack_Zap();
        Expression = Stack_Pop();
        Dictionary_Exit_Nested_Scope();
        compile_expression_C(Expression); }
    else
      { frame_size_number = make_NBR(frame_size);
        MAIN_CLAIM_DYNAMIC_C(frame_size);
        symbol_vector = build_symbol_vector_M(frame_size);
        Dictionary_Exit_Nested_Scope();
        compiled_expression = Stack_Pop();
        thunk = make_THK_M(frame_size_number,
                           compiled_expression,
                           symbol_vector);
        Stack_Poke(thunk); }}

static NIL_type predicate_list_C(EXP_type Predicates_expression,
                                 KEY_type Keyword)
  { EXP_type predicate_expression;
    PAI_type predicates_pair;
    UNS_type count;
    for (count = 0;
         is_PAI(Predicates_expression);
         count += 1)
      { Stack_Push_C(Predicates_expression);
        predicates_pair = Stack_Peek();
        predicate_expression  = predicates_pair->car;
        expression_inline_C(predicate_expression);
        predicates_pair = Stack_Reduce();
        Predicates_expression = predicates_pair->cdr; }
    if (!is_NUL(Predicates_expression))
      compilation_keyword_error(ITF_error,
                                Keyword);
    Stack_Collapse_C(count); }

/*--------------------------------------------------------------------------------------*/

static NIL_type compile_and_C(EXP_type Operand_list_expression)
  { AND_type and;
    VEC_type compiled_predicate_vector;
    predicate_list_C(Operand_list_expression,
                     Pool_And);
    MAIN_CLAIM_DEFAULT_C();
    compiled_predicate_vector = Stack_Peek();
    and = make_AND_M(compiled_predicate_vector);
    Stack_Poke(and); }

/*--------------------------------------------------------------------------------------*/
/*------------------------------------- application ------------------------------------*/
/*--------------------------------------------------------------------------------------*/

static NIL_type compile_expression_C(EXP_type);

/*--------------------------------------------------------------------------------------*/

static NIL_type argument_list_C(EXP_type Argument_list_expression)
  { EXP_type argument_expression;
    PAI_type argument_list_pair;
    UNS_type count;
    count = 0;
    for (argument_list_pair = Argument_list_expression;
         is_PAI(argument_list_pair);
         argument_list_pair = argument_list_pair->cdr)
      { Stack_Push_C(argument_list_pair);
        argument_list_pair = Stack_Peek();
        argument_expression = argument_list_pair->car;
        compile_expression_C(argument_expression);
        argument_list_pair = Stack_Reduce();
        count += 1; }
    if (!is_NUL(argument_list_pair))
      compilation_rawstring_error(ITF_error,
                                  Pool_Application_Rawstring);
    Stack_Collapse_C(count); }

/*--------------------------------------------------------------------------------------*/

static NIL_type compile_application_C(PAI_type Application_pair)
  { EXP_type application;
    EXP_type argument_list_expression,
             compiled_operator_expression,
             operator_expression;
    TAG_type tag;
    VEC_type compiled_argument_frame;
    Stack_Push_C(Application_pair);
    Application_pair = Stack_Peek();
    operator_expression = Application_pair->car;
    compile_expression_C(operator_expression);
    Application_pair = Stack_Reduce();
    argument_list_expression = Application_pair->cdr;
    argument_list_C(argument_list_expression);
    MAIN_CLAIM_DEFAULT_C();
    compiled_argument_frame = Stack_Pop();
    compiled_operator_expression = Stack_Peek();
    tag = Grammar_Tag(compiled_operator_expression);
    switch (tag)
      { case GLB_tag:
          application = make_GLA_M(compiled_operator_expression,
                                   compiled_argument_frame);
          break;
        case LCL_tag:
          application = make_LCA_M(compiled_operator_expression,
                                   compiled_argument_frame);
          break;
        default:
          application = make_APL_M(compiled_operator_expression,
                                   compiled_argument_frame); }
    Stack_Poke(application); }

/*--------------------------------------------------------------------------------------*/
/*---------------------------------------- begin ---------------------------------------*/
/*--------------------------------------------------------------------------------------*/

static NIL_type compile_expression_C(EXP_type);

/*--------------------------------------------------------------------------------------*/

static NIL_type sequence_C(EXP_type Operand_list_expression,
                           KEY_type Keyword)
  { EXP_type compiled_expression,
             expression;
    PAI_type sequence_pair;
    SEQ_type sequence;
    TAG_type tag;
    UNS_type count;
    tag = Grammar_Tag(Operand_list_expression);
    switch (tag)
      { case NUL_tag:
          compilation_keyword_error(AL1_error,
                                    Keyword);
        case PAI_tag:
          count = 0;
          do
            { Stack_Push_C(Operand_list_expression);
              sequence_pair = Stack_Peek();
              expression = sequence_pair->car;
              compile_expression_C(expression);
              sequence_pair = Stack_Reduce();
              Operand_list_expression = sequence_pair->cdr;
              count += 1; }
          while (is_PAI(Operand_list_expression));
          if (!is_NUL(Operand_list_expression))
            compilation_keyword_error(ITF_error,
                                      Keyword);
          if (count > 1)
            { MAIN_CLAIM_DYNAMIC_C(count);
              sequence = make_SEQ_Z(count);
              while (count > 0)
                { compiled_expression = Stack_Pop();
                  sequence[count--] = compiled_expression; }
              Stack_Push_C(sequence); }
          return;
        default:
          compilation_keyword_error(ITF_error,
                                    Keyword); }}

/*--------------------------------------------------------------------------------------*/

static NIL_type compile_begin_C(EXP_type Operand_list_expression)
  { BEG_type begin;
    SEQ_type compiled_sequence;
    sequence_C(Operand_list_expression,
               Pool_Begin);
    compiled_sequence = Stack_Peek();
    begin = make_BEG_M(compiled_sequence);
    Stack_Poke(begin); }

/*--------------------------------------------------------------------------------------*/
/*---------------------------------------- cond ----------------------------------------*/
/*--------------------------------------------------------------------------------------*/

static NIL_type compile_expression_C(EXP_type);
static NIL_type           sequence_C(EXP_type,
                                     KEY_type);
static NIL_type  expression_inline_C(EXP_type);

/*--------------------------------------------------------------------------------------*/

static NIL_type single_expression_C(EXP_type Operand_list_expression,
                                    KEY_type Keyword)
  { EXP_type expression,
             residue_expression;
    PAI_type operand_list_pair;
    TAG_type tag;
    tag = Grammar_Tag(Operand_list_expression);
    switch (tag)
      { case NUL_tag:
          compilation_keyword_error(E1X_error,
                                    Keyword);
        case PAI_tag:
          operand_list_pair  = Operand_list_expression;
          expression         = operand_list_pair->car;
          residue_expression = operand_list_pair->cdr;
          if (!is_NUL(residue_expression))
            compilation_keyword_error(TMX_error,
                                      Keyword);
          Stack_Push_C(expression);
          return;
        default:
          compilation_keyword_error(ITF_error,
                                    Keyword); }}

static NIL_type sequence_inline_C(EXP_type Sequence_expression,
                                  KEY_type Keyword)
  { EXP_type compiled_sequence_expression;
    NBR_type frame_size_number;
    THK_type thunk;
    UNS_type frame_size;
    VEC_type symbol_vector;
    Stack_Push_C(Sequence_expression);
    Dictionary_Enter_Nested_Scope_C();
    Sequence_expression = Stack_Peek();
    sequence_C(Sequence_expression,
               Keyword);
    frame_size = Dictionary_Get_Frame_Size();
    if (frame_size == 0)
      { Stack_Zap();
        Sequence_expression = Stack_Pop();
        Dictionary_Exit_Nested_Scope();
        sequence_C(Sequence_expression,
                   Keyword); }
    else
      { frame_size_number = make_NBR(frame_size);
        MAIN_CLAIM_DYNAMIC_C(frame_size);
        symbol_vector = build_symbol_vector_M(frame_size);
        Dictionary_Exit_Nested_Scope();
        compiled_sequence_expression = Stack_Pop();
        thunk = make_THK_M(frame_size_number,
                           compiled_sequence_expression,
                           symbol_vector);
        Stack_Poke(thunk); }}

static UNS_type clause_predicates_C(EXP_type Clause_list_expression)
  { EXP_type predicate_expression;
    PAI_type clause_list_pair,
             clause_pair;
    UNS_type size;
    size = 0;
    predicate_expression = Grammar_Null;
    for (clause_list_pair = Clause_list_expression;
         is_PAI(clause_list_pair);
         clause_list_pair = clause_list_pair->cdr)
      { Stack_Push_C(clause_list_pair);
        clause_list_pair = Stack_Peek();
        clause_pair = clause_list_pair->car;
        if (!is_PAI(clause_pair))
          compilation_keyword_error(NAP_error,
                                    Pool_Cond);
        predicate_expression = clause_pair->car;
        if (predicate_expression == KEYWORD(Else))
          { Stack_Zap();
            break; }
        expression_inline_C(predicate_expression);
        clause_list_pair = Stack_Reduce();
        size += 1; }
    if (!is_NUL(clause_list_pair))
      if (predicate_expression != KEYWORD(Else))
        compilation_keyword_error(ITF_error,
                                  Pool_Cond);
    if (size == 0)
      if (predicate_expression != KEYWORD(Else))
        compilation_keyword_error(AL1_error,
                                  Pool_Cond);
    Stack_Collapse_C(size);
    return size; }

static PAI_type clause_bodies_C(PAI_type Clause_list_pair,
                                UNS_type Size)
  { EXP_type body_expression,
             compiled_body_expression,
             residue_expression;
    PAI_type clause_list_pair,
             clause_pair;
    UNS_type count;
    VEC_type compiled_body_vector;
    residue_expression = Clause_list_pair;
    for (count = 1;
         count <= Size;
         count += 1)
      { Stack_Push_C(residue_expression);
        clause_list_pair = Stack_Peek();
        clause_pair = clause_list_pair->car;
        body_expression = clause_pair->cdr;
        sequence_inline_C(body_expression,
                          Pool_Cond);
        compiled_body_expression = Stack_Pop();
        clause_list_pair = Stack_Pop();
        compiled_body_vector = Stack_Peek();
        compiled_body_vector[count] = compiled_body_expression;
        residue_expression = clause_list_pair->cdr; }
    return residue_expression; }

static NIL_type else_clause_C(EXP_type Clause_list_expression)
  { PAI_type else_body_pair,
             else_clause_pair;
    single_expression_C(Clause_list_expression,
                        Pool_Else);
    else_clause_pair = Stack_Pop();
    else_body_pair = else_clause_pair->cdr;
    if (!is_PAI(else_body_pair))
      compilation_keyword_error(AL1_error,
                                Pool_Else);
    sequence_inline_C(else_body_pair,
                      Pool_Else); }

/*--------------------------------------------------------------------------------------*/

static NIL_type compile_cond_C(EXP_type Operand_list_expression)
  { CND_type cond;
    CNE_type cond_else;
    EXP_type clause_list_expression,
             compiled_else_expression,
             residue_expression;
    UNS_type size;
    VEC_type compiled_body_vector,
             compiled_predicate_vector;
    Stack_Push_C(Operand_list_expression);
    clause_list_expression = Stack_Peek();
    size = clause_predicates_C(clause_list_expression);
    residue_expression = Stack_Reduce();
    if (size == 0)
      Stack_Poke(Grammar_Empty_Vector);
    else
      { Stack_Push_C(residue_expression);
        MAIN_CLAIM_DYNAMIC_C(size);
        compiled_body_vector = make_VEC_M(size);
        Stack_Push_C(compiled_body_vector);
        clause_list_expression = Stack_Reduce();
        residue_expression = clause_bodies_C(clause_list_expression,
                                             size); }
    if (is_NUL(residue_expression))
      { MAIN_CLAIM_DEFAULT_C();
        compiled_body_vector = Stack_Pop();
        compiled_predicate_vector = Stack_Peek();
        cond = make_CND_M(compiled_predicate_vector,
                          compiled_body_vector);
        Stack_Poke(cond); }
    else
      { else_clause_C(residue_expression);
        MAIN_CLAIM_DEFAULT_C();
        compiled_else_expression = Stack_Pop();
        compiled_body_vector = Stack_Pop();
        compiled_predicate_vector = Stack_Peek();
        cond_else = make_CNE_M(compiled_predicate_vector,
                               compiled_body_vector,
                               compiled_else_expression);
        Stack_Poke(cond_else); }}

/*--------------------------------------------------------------------------------------*/
/*--------------------------------------- define ---------------------------------------*/
/*--------------------------------------------------------------------------------------*/

static VEC_type build_symbol_vector_M(UNS_type);
static NIL_type  compile_expression_C(EXP_type);
static NIL_type            sequence_C(EXP_type,
                                      KEY_type);
static NIL_type   single_expression_C(EXP_type,
                                      KEY_type);

/*--------------------------------------------------------------------------------------*/

static NIL_type parameter_list_C(PAI_type Parameter_list_pair,
                                 KEY_type Keyword)
  { SYM_type parameter_symbol;
    Stack_Push_C(Parameter_list_pair);
    Parameter_list_pair = Stack_Peek();
    while (is_PAI(Parameter_list_pair))
      { MAIN_CLAIM_DEFAULT_C();
        Parameter_list_pair = Stack_Peek();
        parameter_symbol = Parameter_list_pair->car;
        if (!is_SYM(parameter_symbol))
          compilation_keyword_error(IPA_error,
                                    Keyword);
        Dictionary_Define_M(parameter_symbol);
        Parameter_list_pair = Parameter_list_pair->cdr;
        Stack_Poke(Parameter_list_pair); }}

static NIL_type define_standard_function_C(UNS_type Offset,
                                           UNS_type Parameter_count)
  { DFF_type define_function;
    EXP_type body_expression,
             compiled_body_expression;
    NBR_type frame_size_number,
             offset_number,
             parameter_count_number;
    PAI_type operand_list_pair,
             pattern_pair;
    SYM_type name_symbol;
    UNS_type frame_size;
    VEC_type symbol_vector;
    Stack_Zap();
    operand_list_pair = Stack_Peek();
    body_expression = operand_list_pair->cdr;
    sequence_C(body_expression,
               Pool_Delay);
    frame_size = Dictionary_Get_Frame_Size();
    offset_number = make_NBR(Offset);
    parameter_count_number = make_NBR(Parameter_count);
    frame_size_number = make_NBR(frame_size);
    MAIN_CLAIM_DYNAMIC_C(frame_size);
    symbol_vector = build_symbol_vector_M(frame_size);
    Dictionary_Exit_Nested_Scope();
    compiled_body_expression = Stack_Pop();
    operand_list_pair = Stack_Peek();
    pattern_pair = operand_list_pair->car;
    name_symbol = pattern_pair->car;
    define_function = make_DFF_M(name_symbol,
                                 offset_number,
                                 parameter_count_number,
                                 frame_size_number,
                                 compiled_body_expression,
                                 symbol_vector);
    Stack_Poke(define_function); }

static NIL_type define_variable_arity_function_C(UNS_type Offset,
                                                 UNS_type Parameter_count)
  { DFZ_type define_function_vararg;
    EXP_type body_expression,
             compiled_body_expression;
    NBR_type frame_size_number,
             offset_number,
             parameter_count_number;
    PAI_type operand_list_pair,
             pattern_pair;
    SYM_type name_symbol,
             vararg_symbol;
    UNS_type frame_size;
    VEC_type symbol_vector;
    MAIN_CLAIM_DEFAULT_C();
    vararg_symbol = Stack_Pop();
    Dictionary_Define_M(vararg_symbol);
    operand_list_pair = Stack_Peek();
    body_expression = operand_list_pair->cdr;
    sequence_C(body_expression,
               Pool_Delay);
    frame_size = Dictionary_Get_Frame_Size();
    offset_number = make_NBR(Offset);
    parameter_count_number = make_NBR(Parameter_count);
    frame_size_number = make_NBR(frame_size);
    MAIN_CLAIM_DYNAMIC_C(frame_size);
    symbol_vector = build_symbol_vector_M(frame_size);
    Dictionary_Exit_Nested_Scope();
    compiled_body_expression = Stack_Pop();
    operand_list_pair = Stack_Peek();
    pattern_pair = operand_list_pair->car;
    name_symbol = pattern_pair->car;
    define_function_vararg = make_DFZ_M(name_symbol,
                                        offset_number,
                                        parameter_count_number,
                                        frame_size_number,
                                        compiled_body_expression,
                                        symbol_vector);
    Stack_Poke(define_function_vararg); }

static NIL_type define_function_C(PAI_type Operand_list_pair)
  { EXP_type residue_expression;
    PAI_type parameter_list_pair,
             pattern_pair;
    SYM_type name_symbol;
    TAG_type tag;
    UNS_type offset,
             parameter_count;
    Stack_Push_C(Operand_list_pair);
    MAIN_CLAIM_DEFAULT_C();
    Operand_list_pair = Stack_Peek();
    pattern_pair = Operand_list_pair->car;
    name_symbol = pattern_pair->car;
    if (!is_SYM(name_symbol))
      compilation_keyword_error(IVV_error,
                                Pool_Delay);
    offset = Dictionary_Define_M(name_symbol);
    Dictionary_Enter_Nested_Scope_C();
    Operand_list_pair = Stack_Peek();
    pattern_pair = Operand_list_pair->car;
    parameter_list_pair = pattern_pair->cdr;
    parameter_list_C(parameter_list_pair,
                     Pool_Delay);
    parameter_count = Dictionary_Get_Frame_Size();
    residue_expression = Stack_Peek();
    tag = Grammar_Tag(residue_expression);
    switch (tag)
      { case NUL_tag:
          define_standard_function_C(offset,
                                     parameter_count);
          return;
        case SYM_tag:
          define_variable_arity_function_C(offset,
                                           parameter_count);
          return;
        default:
          compilation_keyword_error(IPA_error,
                                    Pool_Delay); }}

static NIL_type compile_define_variable_C(PAI_type Operand_list_pair)
  { DFV_type define_variable;
    EXP_type compiled_expression,
             expression,
             residue_expression;
    NBR_type offset_number;
    SYM_type variable_symbol;
    UNS_type offset;
    Stack_Push_C(Operand_list_pair);
    Operand_list_pair = Stack_Peek();
    residue_expression = Operand_list_pair->cdr;
    single_expression_C(residue_expression,
                        Pool_Delay);
    expression = Stack_Pop();
    compile_expression_C(expression);
    MAIN_CLAIM_DEFAULT_C();
    Operand_list_pair = Stack_Reduce();
    variable_symbol = Operand_list_pair->car;
    offset = Dictionary_Define_M(variable_symbol);
    offset_number = make_NBR(offset);
    compiled_expression = Stack_Peek();
    define_variable = make_DFV_M(offset_number,
                                 compiled_expression);
    Stack_Poke(define_variable); }

/*--------------------------------------------------------------------------------------*/

static NIL_type compile_define_C(EXP_type Operand_list_expression)
  { EXP_type expression;
    PAI_type operand_list_pair;
    TAG_type tag;
    tag = Grammar_Tag(Operand_list_expression);
    switch (tag)
      { case NUL_tag:
          compilation_keyword_error(MSP_error,
                                    Pool_Delay);
        case PAI_tag:
          operand_list_pair = Operand_list_expression;
          expression = operand_list_pair->car;
          tag = Grammar_Tag(expression);
          switch (tag)
            { case PAI_tag:
                define_function_C(operand_list_pair);
                return;
              case SYM_tag:
                compile_define_variable_C(operand_list_pair);
                return;
              default:
                compilation_keyword_error(IVP_error,
                                          Pool_Delay); }
          return;
        default:
          compilation_keyword_error(ITF_error,
                                    Pool_Delay); }}

/*--------------------------------------------------------------------------------------*/
/*---------------------------------------- delay ---------------------------------------*/
/*--------------------------------------------------------------------------------------*/

static VEC_type build_symbol_vector_M(UNS_type);
static NIL_type  compile_expression_C(EXP_type);
static NIL_type   single_expression_C(EXP_type,
                                      KEY_type);

/*--------------------------------------------------------------------------------------*/

static NIL_type compile_delay_C(EXP_type Operand_list_expression)
  { DEL_type delay_expression;    /* use thunk ... */
    EXP_type compiled_expression,
             expression;
    NBR_type frame_size_number;
    UNS_type frame_size;
    VEC_type symbol_vector;
    single_expression_C(Operand_list_expression,
                        Pool_Delay);
    Dictionary_Enter_Nested_Scope_C();
    expression = Stack_Peek();
    compile_expression_C(expression);
    frame_size = Dictionary_Get_Frame_Size();
    frame_size_number = make_NBR(frame_size);
    MAIN_CLAIM_DYNAMIC_C(frame_size);
    symbol_vector = build_symbol_vector_M(frame_size);
    Dictionary_Exit_Nested_Scope();
    compiled_expression = Stack_Pop();
    delay_expression = make_DEL_M(frame_size_number,
                                  compiled_expression,
                                  symbol_vector);
    Stack_Poke(delay_expression); }

/*--------------------------------------------------------------------------------------*/
/*----------------------------------------- if -----------------------------------------*/
/*--------------------------------------------------------------------------------------*/

static VEC_type build_symbol_vector_M(UNS_type);
static NIL_type  compile_expression_C(EXP_type);
static NIL_type   expression_inline_C(EXP_type);

/*--------------------------------------------------------------------------------------*/

static NIL_type compile_if_C(EXP_type Operand_list_expression)
  { EXP_type alternative_expression,
             clause_list_expression,
             compiled_alternative_expression,
             compiled_consequent_expression,
             compiled_predicate_expression,
             consequent_expression,
             predicate_expression,
             residue_expression;
    IFD_type if_double;
    IFS_type if_single;
    PAI_type alternative_pair,
             clause_list_pair,
             operand_list_pair;
    TAG_type tag;
    tag = Grammar_Tag(Operand_list_expression);
    switch (tag)
      { case NUL_tag:
          compilation_keyword_error(TFX_error,
                                    Pool_If);
        case PAI_tag:
          operand_list_pair = Operand_list_expression;
          clause_list_expression = operand_list_pair->cdr;
          tag = Grammar_Tag(clause_list_expression);
          switch (tag)
            { case NUL_tag:
                compilation_keyword_error(TFX_error,
                                          Pool_If);
              case PAI_tag:
                Stack_Push_C(operand_list_pair);
                operand_list_pair = Stack_Peek();
                predicate_expression = operand_list_pair->car;
                compile_expression_C(predicate_expression);
                operand_list_pair = Stack_Reduce();
                clause_list_pair = operand_list_pair->cdr;
                alternative_pair = clause_list_pair->cdr;
                tag = Grammar_Tag(alternative_pair);
                switch (tag)
                  { case NUL_tag:
                      consequent_expression = clause_list_pair->car;
                      expression_inline_C(consequent_expression);
                      MAIN_CLAIM_DEFAULT_C();
                      compiled_consequent_expression = Stack_Pop();
                      compiled_predicate_expression = Stack_Peek();
                      if_single = make_IFS_M(compiled_predicate_expression,
                                             compiled_consequent_expression);
                      Stack_Poke(if_single);
                      return;
                    case PAI_tag:
                      residue_expression = alternative_pair->cdr;
                      if (!is_NUL(residue_expression))
                        compilation_keyword_error(TMX_error,
                                                  Pool_If);
                      Stack_Push_C(clause_list_pair);
                      clause_list_pair = Stack_Peek();
                      consequent_expression = clause_list_pair->car;
                      expression_inline_C(consequent_expression);
                      clause_list_pair = Stack_Reduce();
                      alternative_pair = clause_list_pair->cdr;
                      alternative_expression = alternative_pair->car;
                      expression_inline_C(alternative_expression);
                      MAIN_CLAIM_DEFAULT_C();
                      compiled_alternative_expression = Stack_Pop();
                      compiled_consequent_expression = Stack_Pop();
                      compiled_predicate_expression = Stack_Peek();
                      if_double = make_IFD_M(compiled_predicate_expression,
                                             compiled_consequent_expression,
                                             compiled_alternative_expression);
                      Stack_Poke(if_double);
                      return;
                    default:
                      compilation_keyword_error(ITF_error,
                                                Pool_If); }
                return;
              default:
                compilation_keyword_error(ITF_error,
                                          Pool_If); }
          return;
        default:
          compilation_keyword_error(ITF_error,
                                    Pool_If); }}

/*--------------------------------------------------------------------------------------*/
/*---------------------------------------- lambda --------------------------------------*/
/*--------------------------------------------------------------------------------------*/

static VEC_type build_symbol_vector_M(UNS_type);
static NIL_type      parameter_list_C(PAI_type,
                                      KEY_type);
static NIL_type            sequence_C(EXP_type,
                                      KEY_type);

/*--------------------------------------------------------------------------------------*/

static NIL_type compile_lambda_C(EXP_type Operand_list_expression)
  { EXP_type body_expression,
             compiled_body_expression,
             parameter_list_expression,
             residue_expression;
    LMB_type lambda;
    LMV_type lambda_vararg;
    NBR_type frame_size_number,
             parameter_count_number;
    PAI_type operand_list_pair;
    SYM_type vararg_symbol;
    TAG_type tag;
    UNS_type frame_size,
             parameter_count;
    VEC_type symbol_vector;
    tag = Grammar_Tag(Operand_list_expression);
    switch (tag)
      { case NUL_tag:
          compilation_keyword_error(TFX_error,
                                    Pool_Lambda);
        case PAI_tag:
          Stack_Push_C(Operand_list_expression);
          Dictionary_Enter_Nested_Scope_C();
          operand_list_pair = Stack_Peek();
          body_expression = operand_list_pair->cdr;
          if (!is_PAI(body_expression))
            compilation_keyword_error(AL1_error,
                                      Pool_Lambda);
          parameter_list_expression = operand_list_pair->car;
          parameter_list_C(parameter_list_expression,
                           Pool_Lambda);
          parameter_count = Dictionary_Get_Frame_Size();
          residue_expression = Stack_Peek();
          tag = Grammar_Tag(residue_expression);
          switch (tag)
            { case NUL_tag:
                Stack_Zap();
                operand_list_pair = Stack_Peek();
                body_expression = operand_list_pair->cdr;
                sequence_C(body_expression,
                           Pool_Lambda);
                frame_size = Dictionary_Get_Frame_Size();
                parameter_count_number = make_NBR(parameter_count);
                frame_size_number = make_NBR(frame_size);
                MAIN_CLAIM_DYNAMIC_C(frame_size);
                symbol_vector = build_symbol_vector_M(frame_size);
                Dictionary_Exit_Nested_Scope();
                compiled_body_expression = Stack_Pop();
                lambda = make_LMB_M(parameter_count_number,
                                    frame_size_number,
                                    compiled_body_expression,
                                    symbol_vector);
                Stack_Poke(lambda);
                return;
              case SYM_tag:
                MAIN_CLAIM_DEFAULT_C();
                vararg_symbol = Stack_Pop();
                Dictionary_Define_M(vararg_symbol);
                          operand_list_pair = Stack_Peek();
                body_expression = operand_list_pair->cdr;
                sequence_C(body_expression,
                           Pool_Lambda);
                frame_size = Dictionary_Get_Frame_Size();
                parameter_count_number = make_NBR(parameter_count);
                frame_size_number = make_NBR(frame_size);
                MAIN_CLAIM_DYNAMIC_C(frame_size);
                symbol_vector = build_symbol_vector_M(frame_size);
                Dictionary_Exit_Nested_Scope();
                compiled_body_expression = Stack_Pop();
                lambda_vararg = make_LMV_M(parameter_count_number,
                                           frame_size_number,
                                           compiled_body_expression,
                                           symbol_vector);
                Stack_Poke(lambda_vararg);
                return;
              default:
                compilation_keyword_error(IPA_error,
                                          Pool_Lambda); }
          return;
        default:
          compilation_keyword_error(ITF_error,
                                    Pool_Lambda); }}

/*--------------------------------------------------------------------------------------*/
/*----------------------------------------- let ----------------------------------------*/
/*--------------------------------------------------------------------------------------*/

static VEC_type build_symbol_vector_M(UNS_type);
static NIL_type  compile_expression_C(EXP_type);
static NIL_type            sequence_C(EXP_type,
                                      KEY_type);

/*--------------------------------------------------------------------------------------*/

static NIL_type binding_list_C(EXP_type Operand_list_expression)
  { EXP_type expression,
             residue_expression;
    PAI_type binding_pair,
             operand_list_pair,
             pair;
    SYM_type variable;
    UNS_type count;
    count = 0;
    for (operand_list_pair = Operand_list_expression;
         is_PAI(operand_list_pair);
         operand_list_pair = operand_list_pair->cdr)
      { Stack_Push_C(operand_list_pair);
        operand_list_pair = Stack_Peek();
        binding_pair = operand_list_pair->car;
        if (!is_PAI(binding_pair))
          compilation_keyword_error(BND_error,
                                    Pool_Let);


        pair = binding_pair->cdr;
        if (!is_PAI(pair))
          compilation_keyword_error(BND_error,
                                    Pool_Let);
        expression         = pair->car;
        residue_expression = pair->cdr;
        if (!is_NUL(residue_expression))
          compilation_keyword_error(BND_error,
                                    Pool_Let);
        compile_expression_C(expression);
        MAIN_CLAIM_DEFAULT_C();
        operand_list_pair = Stack_Reduce();
        binding_pair = operand_list_pair->car;
        variable = binding_pair->car;
        if (!is_SYM(variable))
          compilation_keyword_error(IVV_error,
                                    Pool_Let);
        Dictionary_Define_M(variable);
        count += 1; }
    if (!is_NUL(operand_list_pair))
      compilation_keyword_error(ITF_error,
                                Pool_Let);
    Stack_Collapse_C(count); }

/*--------------------------------------------------------------------------------------*/

static NIL_type compile_let_C(EXP_type Operand_list_expression)
  { EXP_type body_expression,
             compiled_body_expression;
    LET_type let;
    NBR_type frame_size_number;
    PAI_type binding_list_pair,
             operand_list_pair;
    TAG_type tag;
    UNS_type frame_size;
    VEC_type binding_vector,
             symbol_vector;
    tag = Grammar_Tag(Operand_list_expression);
    switch (tag)
      { case NUL_tag:
          compilation_keyword_error(TFX_error,
                                    Pool_Let);
        case PAI_tag:
          Stack_Push_C(Operand_list_expression);
          Dictionary_Enter_Nested_Scope_C();
          operand_list_pair = Stack_Peek();
          binding_list_pair = operand_list_pair->car;
          binding_list_C(binding_list_pair);
          operand_list_pair = Stack_Reduce();
          body_expression = operand_list_pair->cdr;
          sequence_C(body_expression,
                     Pool_Let);
          frame_size = Dictionary_Get_Frame_Size();
          frame_size_number = make_NBR(frame_size);
          MAIN_CLAIM_DYNAMIC_C(frame_size);
          symbol_vector = build_symbol_vector_M(frame_size);
          Dictionary_Exit_Nested_Scope();
          compiled_body_expression = Stack_Pop();
          binding_vector = Stack_Peek();
          let = make_LET_M(frame_size_number,
                           binding_vector,
                           compiled_body_expression,
                           symbol_vector);
          Stack_Poke(let);
          return;
        default:
          compilation_keyword_error(ITF_error,
                                    Pool_Let); }}

/*--------------------------------------------------------------------------------------*/
/*------------------------------------------ or ----------------------------------------*/
/*--------------------------------------------------------------------------------------*/

static NIL_type predicate_list_C(EXP_type,
                                 KEY_type);

/*--------------------------------------------------------------------------------------*/

static NIL_type compile_or_C(EXP_type Operand_list_expression)
  { ORR_type or;
    VEC_type compiled_predicate_vector;
    predicate_list_C(Operand_list_expression,
                     Pool_Or);
    MAIN_CLAIM_DEFAULT_C();
    compiled_predicate_vector = Stack_Peek();
    or = make_ORR_M(compiled_predicate_vector);
    Stack_Poke(or); }

/*--------------------------------------------------------------------------------------*/
/*------------------------------------- quasiquote -------------------------------------*/
/*--------------------------------------------------------------------------------------*/

static NIL_type        quasiquote_C(EXP_type,
                                    BLN_type);
static NIL_type single_expression_C(EXP_type,
                                    KEY_type);

/*--------------------------------------------------------------------------------------*/

static NIL_type quoted_quasiquote_C(EXP_type Operand_list_expression)
  { EXP_type expression;
    PAI_type pair;
    SYM_type symbol;
    single_expression_C(Operand_list_expression,
                        Pool_Quasiquote);
    MAIN_CLAIM_DEFAULT_C();
    expression = Stack_Peek();
    symbol = KEYWORD(Quasiquote);
    pair = make_PAI_M(expression,
                      Grammar_Null);
    pair = make_PAI_M(symbol,
                      pair);
    Stack_Poke(pair); }

static NIL_type unquote_C(EXP_type Operand_list_expression)
  { EXP_type compiled_expression,
             expression;
    UNQ_type unquote;
    single_expression_C(Operand_list_expression,
                        Pool_Unquote);
    expression = Stack_Pop();
    compile_expression_C(expression);
    MAIN_CLAIM_DEFAULT_C();
    compiled_expression = Stack_Peek();
    unquote = make_UNQ_M(compiled_expression);
    Stack_Poke(unquote); }

static NIL_type unquote_splicing_C(EXP_type Operand_list_expression)
  { EXP_type compiled_expression,
             expression;
    UQS_type unquote_splicing;
    single_expression_C(Operand_list_expression,
                        Pool_Unquote);
    expression = Stack_Pop();
    compile_expression_C(expression);
    MAIN_CLAIM_DEFAULT_C();
    compiled_expression = Stack_Peek();
    unquote_splicing = make_UQS_M(compiled_expression);
    Stack_Poke(unquote_splicing); }

static NIL_type quasiquote_list_C(PAI_type Pair)
  { EXP_type expression,
             head_expression,
             tail_expression;
    PAI_type pair;
    UNS_type count;
    pair = Pair;
    for (count = 1;
         ;
         count += 1)
      { head_expression = pair->car;
        tail_expression = pair->cdr;
        quasiquote_C(head_expression,
                     Grammar_Is_True);
        if (! is_PAI(tail_expression))
          break;
        pair = tail_expression; }
    quasiquote_C(tail_expression,
                 Grammar_Is_False);
    for (;
         count > 0;
         count--)
      { MAIN_CLAIM_DEFAULT_C();
        pair = Stack_Pop();
        expression = Stack_Peek();
        pair = make_PAI_M(expression,
                          pair);
        Stack_Poke(pair); }}

static NIL_type quasiquote_form_C(PAI_type Form_pair,
                                  BLN_type In_list)
  { EXP_type operand_list_expression,
             operator_expression;
    operator_expression     = Form_pair->car;
    operand_list_expression = Form_pair->cdr;
    if (operator_expression == KEYWORD(Quasiquote))
      quoted_quasiquote_C(operand_list_expression);
    else if (operator_expression == KEYWORD(Unquote))
      unquote_C(operand_list_expression);
    else if (operator_expression == KEYWORD(Unquote_Splicing))
      if (In_list)
        unquote_splicing_C(operand_list_expression);
      else
        compilation_keyword_error(UQS_error,
                                  Pool_Quasiquote);
    else
      quasiquote_list_C(Form_pair); }

static NIL_type quasiquote_C(EXP_type Expression,
                             BLN_type In_list)
  { TAG_type tag;
    tag = Grammar_Tag(Expression);
    switch (tag)
      { case PAI_tag:
          quasiquote_form_C(Expression,
                            In_list);
          return;
        case VEC_tag:
          Stack_Push_C(Expression);
          return;
        case CHA_tag:
        case FLS_tag:
        case NBR_tag:
        case NUL_tag:
        case REA_tag:
        case STR_tag:
        case SYM_tag:
        case TRU_tag:
          Stack_Push_C(Expression);
          return;
        default:
          compilation_keyword_error(IXT_error,
                                    Pool_Quasiquote); }}

/*--------------------------------------------------------------------------------------*/

static NIL_type compile_quasiquote_C(EXP_type Operand_list_expression)
  { EXP_type expression,
             quasiquoted_expression;
    QQU_type quasiquote;
    single_expression_C(Operand_list_expression,
                        Pool_Quasiquote);
    expression = Stack_Pop();
    quasiquote_C(expression,
                 Grammar_Is_False);
    MAIN_CLAIM_DEFAULT_C();
    quasiquoted_expression = Stack_Peek();
    quasiquote = make_QQU_M(quasiquoted_expression);
    Stack_Poke(quasiquote); }

/*--------------------------------------------------------------------------------------*/
/*---------------------------------------- quote ---------------------------------------*/
/*--------------------------------------------------------------------------------------*/

static NIL_type single_expression_C(EXP_type,
                                    KEY_type);

/*--------------------------------------------------------------------------------------*/

static NIL_type compile_quote_C(EXP_type Operand_list_expression)
  { single_expression_C(Operand_list_expression,
                        Pool_Quote); }

/*--------------------------------------------------------------------------------------*/
/*----------------------------------------- set! ---------------------------------------*/
/*--------------------------------------------------------------------------------------*/

static NIL_type compile_expression_C(EXP_type);
static NIL_type  single_expression_C(EXP_type,
                                     KEY_type);

/*--------------------------------------------------------------------------------------*/

static NIL_type compile_set_C(EXP_type Operand_list_expression)
  { EXP_type compiled_expression,
             expression,
             residue_expression;
    NBR_type offset_number,
             scope_number;
    PAI_type operands_pair;
    STG_type set_global;
    STL_type set_local;
    SYM_type variable_symbol;
    TAG_type tag;
    UNS_type offset,
             scope;
    tag = Grammar_Tag(Operand_list_expression);
    switch (tag)
      { case NUL_tag:
          compilation_keyword_error(MSV_error,
                                    Pool_Set);
        case PAI_tag:
          operands_pair = Operand_list_expression;
          variable_symbol = operands_pair->car;
          if (!is_SYM(variable_symbol))
            compilation_keyword_error(IVV_error,
                                      Pool_Set);
          offset = Dictionary_Lexical_Address_M(variable_symbol,
                                                &scope);
          if (offset == 0)
            compilation_symbol_error(VNF_error,
                                     variable_symbol);
          residue_expression = operands_pair->cdr;
          single_expression_C(residue_expression,
                              Pool_Set);
          expression = Stack_Pop();
          compile_expression_C(expression);
          MAIN_CLAIM_DEFAULT_C();
          offset_number = make_NBR(offset);
          compiled_expression = Stack_Peek();
          if (scope == 0)
            { set_local = make_STL_M(offset_number,
                                     compiled_expression);
              Stack_Poke(set_local); }
          else
            { scope_number = make_NBR(scope);
              set_global = make_STG_M(scope_number,
                                      offset_number,
                                      compiled_expression);
              Stack_Poke(set_global); }
          return;
        default:
          compilation_keyword_error(ITF_error,
                                    Pool_Set); }}

/*--------------------------------------------------------------------------------------*/
/*---------------------------------------- while ---------------------------------------*/
/*--------------------------------------------------------------------------------------*/

static NIL_type expression_inline_C(EXP_type);
static NIL_type   sequence_inline_C(EXP_type,
                                    KEY_type);

/*--------------------------------------------------------------------------------------*/

static NIL_type compile_while_C(EXP_type Operand_list_expression)
  { EXP_type body_expression,
             compiled_body_expression,
             compiled_predicate_expression,
             predicate_expression;
    PAI_type operand_list_pair;
    TAG_type tag;
    WHI_type while_;
    tag = Grammar_Tag(Operand_list_expression);
    switch (tag)
      { case NUL_tag:
          compilation_keyword_error(TFX_error,
                                    Pool_While);
        case PAI_tag:
          Stack_Push_C(Operand_list_expression);
          operand_list_pair = Stack_Peek();
          predicate_expression = operand_list_pair->car;
          body_expression = operand_list_pair->cdr;
          tag = Grammar_Tag(body_expression);
          switch (tag)
            { case NUL_tag:
                compilation_keyword_error(TFX_error,
                                          Pool_While);
              case PAI_tag:
                expression_inline_C(predicate_expression);
                operand_list_pair = Stack_Reduce();
                body_expression = operand_list_pair->cdr;
                sequence_inline_C(body_expression,
                                  Pool_While);
                MAIN_CLAIM_DEFAULT_C();
                compiled_body_expression = Stack_Pop();
                compiled_predicate_expression = Stack_Peek();
                while_ = make_WHI_M(compiled_predicate_expression,
                                    compiled_body_expression);
                Stack_Poke(while_);
                return;
              default:
                compilation_keyword_error(ITF_error,
                                          Pool_While); }
          return;
        default:
          compilation_keyword_error(ITF_error,
                                    Pool_While); }}

/*--------------------------------------------------------------------------------------*/
/*----------------------------------------- form ---------------------------------------*/
/*--------------------------------------------------------------------------------------*/

static NIL_type    compile_and_C(EXP_type);
static NIL_type  compile_begin_C(EXP_type);
static NIL_type   compile_cond_C(EXP_type);
static NIL_type compile_define_C(EXP_type);
static NIL_type  compile_delay_C(EXP_type);
static NIL_type compile_lambda_C(EXP_type);
static NIL_type    compile_let_C(EXP_type);
static NIL_type     compile_or_C(EXP_type);
static NIL_type  compile_quote_C(EXP_type);
static NIL_type    compile_set_C(EXP_type);
static NIL_type  compile_while_C(EXP_type);

/*--------------------------------------------------------------------------------------*/

static NIL_type compile_form_C(PAI_type Form_pair)
  { EXP_type operand_list_expression,
             operator_expression;
    operator_expression     = Form_pair->car;
    operand_list_expression = Form_pair->cdr;
    if (operator_expression == KEYWORD(And))
      compile_and_C(operand_list_expression);
    else if (operator_expression == KEYWORD(Begin))
      compile_begin_C(operand_list_expression);
    else if (operator_expression == KEYWORD(Cond))
      compile_cond_C(operand_list_expression);
    else if (operator_expression == KEYWORD(Define))
      compile_define_C(operand_list_expression);
    else if (operator_expression == KEYWORD(Delay))
      compile_delay_C(operand_list_expression);
    else if (operator_expression == KEYWORD(If))
      compile_if_C(operand_list_expression);
    else if (operator_expression == KEYWORD(Lambda))
      compile_lambda_C(operand_list_expression);
    else if (operator_expression == KEYWORD(Let))
      compile_let_C(operand_list_expression);
    else if (operator_expression == KEYWORD(Or))
      compile_or_C(operand_list_expression);
    else if (operator_expression == KEYWORD(Quasiquote))
      compile_quasiquote_C(operand_list_expression);
    else if (operator_expression == KEYWORD(Quote))
      compile_quote_C(operand_list_expression);
    else if (operator_expression == KEYWORD(Set))
      compile_set_C(operand_list_expression);
    else if (operator_expression == KEYWORD(While))
      compile_while_C(operand_list_expression);
    else
      compile_application_C(Form_pair); }

/*--------------------------------------------------------------------------------------*/
/*--------------------------------------- literal --------------------------------------*/
/*--------------------------------------------------------------------------------------*/

static NIL_type compile_literal_C(EXP_type Literal_expression)
  { Stack_Push_C(Literal_expression); }

/*--------------------------------------------------------------------------------------*/
/*-------------------------------------- variable --------------------------------------*/
/*--------------------------------------------------------------------------------------*/

static NIL_type compile_variable_C(SYM_type Symbol)
  { GLB_type global;
    LCL_type local;
    NBR_type offset_number,
             scope_number;
    UNS_type offset,
             scope;
    if (Pool_Validate_Symbol(Symbol) == Grammar_Is_False)
      compilation_symbol_error(IUK_error,
                               Symbol);
    offset = Dictionary_Lexical_Address_M(Symbol,
                                          &scope);
    if (offset == 0)
      compilation_symbol_error(VNF_error,
                               Symbol);
    offset_number = make_NBR(offset);
    MAIN_CLAIM_DEFAULT_C();
    if (scope == 0)
      { local = make_LCL_M(offset_number);
        Stack_Push_C(local); }
    else
      { scope_number = make_NBR(scope);
        global = make_GLB_M(scope_number,
                            offset_number);
        Stack_Push_C(global); }}

/*--------------------------------------------------------------------------------------*/
/*-------------------------------------- expression ------------------------------------*/
/*--------------------------------------------------------------------------------------*/

static NIL_type     compile_form_C(PAI_type);
static NIL_type  compile_literal_C(EXP_type);
static NIL_type compile_variable_C(SYM_type);

/*--------------------------------------------------------------------------------------*/

static NIL_type compile_expression_C(EXP_type Expression)
  { TAG_type tag;
    tag = Grammar_Tag(Expression);
    switch (tag)
      { case PAI_tag:
          compile_form_C(Expression);
          return;
        case SYM_tag:
          compile_variable_C(Expression);
          return;
        case CHA_tag:
        case FLS_tag:
        case NBR_tag:
        case NUL_tag:
        case REA_tag:
        case STR_tag:
        case TRU_tag:
        case VEC_tag:
          compile_literal_C(Expression);
          return;
        default:
          compilation_rawstring_error(IXT_error,
                                      Main_Void); }}

/*----------------------------------- public functions ---------------------------------*/

EXP_type Compile_Compile_C(EXP_type Expression)
  { EXP_type compiled_expression;
    UNS_type frame_size;
    VEC_type global_symbol_vector;
    compile_expression_C(Expression);
    frame_size = Dictionary_Get_Frame_Size();
    Environment_Adjust_Global_Frame_C(frame_size);
    global_symbol_vector = Environment_Global_Symbol_Vector();
    Dictionary_Setup_Symbol_Vector(global_symbol_vector);
    compiled_expression = Stack_Pop();
    return compiled_expression; }

UNS_type Compile_Define_Global_Symbol_M(SYM_type Symbol)
  { UNS_type offset;
    offset = Dictionary_Define_M(Symbol);
    return offset; }

/*--------------------------------------------------------------------------------------*/

NIL_type Compile_Initialize(NIL_type)
  { return; }
