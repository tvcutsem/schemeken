                     /*---------------------------------------------*/
                     /*                  >>>Slip<<<                 */
                     /*                 Theo D'Hondt                */
                     /*          VUB Software Languages Lab         */
                     /*                  (c) 2012                   */
                     /*---------------------------------------------*/
                     /*   version 13: completion and optimization   */
                     /*---------------------------------------------*/
                     /*                    print                    */
                     /*---------------------------------------------*/

#include <stdio.h>

#include "SlipMain.h"

#include "SlipEnvironment.h"
#include "SlipPool.h"
#include "SlipPrint.h"

/*------------------------------------ private macros ----------------------------------*/

#define FORMAT(STRING, VALUE)                                                            \
  { snprintf(Print_Buffer,                                                               \
             Print_Buffer_size,                                                          \
             STRING,                                                                     \
             VALUE);                                                                     \
    Slip_Print(Print_Buffer); }

/*----------------------------------- private constants --------------------------------*/

enum { Print_Buffer_size = 128 };

static const RWS_type   Continuation_display_rawstring = "<continuation>";
static const RWS_type     Empty_list_display_rawstring = "()";
static const RWS_type          Error_display_rawstring = "display";
static const RWS_type          False_display_rawstring = "#f";
static const RWS_type          Force_display_rawstring = "<force>";
static const RWS_type        Leftpar_display_rawstring = "(";
static const RWS_type        Leftvec_display_rawstring = "#(";
static const RWS_type         Native_display_rawstring = "<native %s>";
static const RWS_type        Newline_display_rawstring = "\n";
static const RWS_type         Period_display_rawstring = " . ";
static const RWS_type      Procedure_display_rawstring = "<procedure %s>";
static const RWS_type        Promise_display_rawstring = "<promise>";
static const RWS_type   Rawcharacter_display_rawstring = "%c";
static const RWS_type      Rawnumber_display_rawstring = "%ld";
static const RWS_type        Rawreal_display_rawstring = "%#.10g";
static const RWS_type      Rawstring_display_rawstring = "%s";
static const RWS_type       Rightpar_display_rawstring = ")";
static const RWS_type       Rightvec_display_rawstring = ")";
static const RWS_type          Space_display_rawstring = " ";
static const RWS_type         String_display_rawstring = "%s";
static const RWS_type           True_display_rawstring = "#t";
static const RWS_type      Undefined_display_rawstring = "<undefined>";
static const RWS_type    Unspecified_display_rawstring = "<unspecified>";

static const RWS_type     Continuation_print_rawstring = "<continuation>";
static const RWS_type       C_function_print_rawstring = "<C function>";
static const RWS_type       Empty_list_print_rawstring = "()";
static const RWS_type            Error_print_rawstring = "print";
static const RWS_type            False_print_rawstring = "#f";
static const RWS_type            Force_print_rawstring = "<force>";
static const RWS_type          Keyword_print_rawstring = "%s";
static const RWS_type           Ken_id_print_rawstring = "<ken-id %s>";
static const RWS_type          Leftpar_print_rawstring = "(";
static const RWS_type          Leftvec_print_rawstring = "#(";
static const RWS_type           Native_print_rawstring = "<native %s>";
static const RWS_type          Newline_print_rawstring = "\n";
static const RWS_type           Period_print_rawstring = " . ";
static const RWS_type        Procedure_print_rawstring = "<procedure %s>";
static const RWS_type          Promise_print_rawstring = "<promise>";
static const RWS_type       Quasiquote_print_rawstring = "`";
static const RWS_type            Quote_print_rawstring = "'";
static const RWS_type     Rawcharacter_print_rawstring = "#\\%c";
static const RWS_type        Rawnumber_print_rawstring = "%ld";
static const RWS_type          Rawreal_print_rawstring = "%#.10g";
static const RWS_type        Rawstring_print_rawstring = "%s";
static const RWS_type         Rightpar_print_rawstring = ")";
static const RWS_type         Rightvec_print_rawstring = ")";
static const RWS_type            Space_print_rawstring = " ";
static const RWS_type           String_print_rawstring = "\"%s\"";
static const RWS_type           Symbol_print_rawstring = "%s";
static const RWS_type             True_print_rawstring = "#t";
static const RWS_type          Unquote_print_rawstring = ",";
static const RWS_type        Undefined_print_rawstring = "<undefined>";
static const RWS_type      Unspecified_print_rawstring = "";
static const RWS_type Unquote_splicing_print_rawstring = ",@";
static const RWS_type         Variable_print_rawstring = "var_%u";
static const RWS_type     Variable_bis_print_rawstring = "_%u";

/*----------------------------------- private variables --------------------------------*/

static RWC_type Print_Buffer[Print_Buffer_size];

/*----------------------------------- private prototypes -------------------------------*/

static NIL_type display_expression(EXP_type);
static NIL_type   print_expression(EXP_type);
static NIL_type     print_sequence(SEQ_type);
static NIL_type       print_symbol(SYM_type);
static NIL_type     print_variable(UNS_type,
                                   UNS_type);

/*----------------------------------- private functions --------------------------------*/

static NIL_type print_error(BYT_type Error,
                            RWS_type Rawstring)
  { Main_Error_Handler(Error,
                       Rawstring); }

/*--------------------------------private display functions ----------------------------*/

static NIL_type display_rawstring(RWS_type Rawstring)
  { FORMAT(Rawstring_display_rawstring,
           Rawstring); }

/*--------------------------------------------------------------------------------------*/

static NIL_type display_character(CHA_type Character)
  { RWC_type rawcharacter;
    rawcharacter = get_CHA(Character);
    FORMAT(Rawcharacter_display_rawstring,
           rawcharacter); }

static NIL_type display_continuation(CNT_type Continuation)
  { display_rawstring(Continuation_display_rawstring); }

static NIL_type display_false(NIL_type)
  { display_rawstring(False_display_rawstring); }

static NIL_type display_force(FRC_type Force)
  { display_rawstring(Force_display_rawstring); }

static NIL_type display_ken_id(KID_type Ken_id)
  { RWC_type ken_id_buf[22];
    ken_id_to_string(Ken_id->rwk, ken_id_buf, 22);
    FORMAT(Ken_id_print_rawstring,
           ken_id_buf); }

static NIL_type display_native(NAT_type Native)
  { RWS_type rawstring;
    rawstring = Native->nam;
    FORMAT(Native_display_rawstring,
           rawstring); }

static NIL_type display_null(NIL_type)
  { display_rawstring(Empty_list_display_rawstring); }

static NIL_type display_number(NBR_type Number)
  { RWN_type rawnumber;
    rawnumber = get_NBR(Number);
    FORMAT(Rawnumber_display_rawstring,
           rawnumber); }

static NIL_type display_pair(PAI_type Pair)
  { EXP_type residue_expression,
             value_expression;
    PAI_type pair;
    TAG_type tag;
    display_rawstring(Leftpar_display_rawstring);
    for (pair = Pair;
         ;
         )
      { value_expression   = pair->car;
        residue_expression = pair->cdr;
        display_expression(value_expression);
        tag = Grammar_Tag(residue_expression);
        switch (tag)
          { case NUL_tag:
              display_rawstring(Rightpar_display_rawstring);
              return;
            case PAI_tag:
              display_rawstring(Space_display_rawstring);
              pair = residue_expression;
              break;
            default:
              display_rawstring(Period_display_rawstring);
              display_expression(residue_expression);
              display_rawstring(Rightpar_display_rawstring);
              return; }}}

static NIL_type display_procedure(PRC_type Procedure)
  { SYM_type name_symbol;
    RWS_type rawstring;
    name_symbol = Procedure->nam;
    rawstring = name_symbol->rws;
    FORMAT(Procedure_display_rawstring,
           rawstring); }

static NIL_type display_procedure_vararg(PRV_type Procedure_vararg)
  { SYM_type name_symbol;
    RWS_type rawstring;
    name_symbol = Procedure_vararg->nam;
    rawstring = name_symbol->rws;
    FORMAT(Procedure_display_rawstring,
           rawstring); }

static NIL_type display_promise(NIL_type)
  { display_rawstring(Promise_display_rawstring); }

static NIL_type display_real(REA_type Real)
  { RWR_type rawreal;
    rawreal = Real->rwr;
    FORMAT(Rawreal_display_rawstring,
           rawreal); }

static NIL_type display_string(STR_type String)
  { RWS_type rawstring;
    rawstring = String->rws;
    FORMAT(String_display_rawstring,
           rawstring); }

static NIL_type display_symbol(SYM_type Symbol)
  { RWS_type rawstring;
    rawstring = Symbol->rws;
    display_rawstring(rawstring); }

static NIL_type display_thunk(THK_type Thunk)
  { EXP_type body_expression;
    body_expression = Thunk->bod;
    display_expression(body_expression); }

static NIL_type display_true(NIL_type)
  { display_rawstring(True_display_rawstring); }

static NIL_type display_undefined(NIL_type)
  { display_rawstring(Undefined_display_rawstring); }

static NIL_type display_unspecified(NIL_type)
  { display_rawstring(Unspecified_display_rawstring); }

static NIL_type display_vector(VEC_type Vector)
  { EXP_type value_expression;
    UNS_type index,
             size;
    size = size_VEC(Vector);
    display_rawstring(Leftvec_display_rawstring);
    for (index = 1;
         index <= size;
         index += 1)
      { value_expression = Vector[index];
        display_expression(value_expression);
        if (index < size)
          display_rawstring(Space_display_rawstring); }
    display_rawstring(Rightvec_display_rawstring); }

/*--------------------------------------------------------------------------------------*/

static NIL_type display_expression(EXP_type Value_expression)
  { TAG_type tag;
    tag = Grammar_Tag(Value_expression);
    switch (tag)
      { case CHA_tag:
          display_character(Value_expression);
          return;
        case CNT_tag:
          display_continuation(Value_expression);
          return;
        case FLS_tag:
          display_false();
          return;
        case FRC_tag:
          display_force(Value_expression);
          return;
        case KID_tag:
          display_ken_id(Value_expression);
          return;
        case NAT_tag:
          display_native(Value_expression);
          return;
        case NBR_tag:
          display_number(Value_expression);
          return;
        case NUL_tag:
          display_null();
          return;
        case PAI_tag:
          display_pair(Value_expression);
          return;
        case PRC_tag:
          display_procedure(Value_expression);
          return;
        case PRM_tag:
          display_promise();
          return;
        case PRV_tag:
          display_procedure_vararg(Value_expression);
          return;
        case REA_tag:
          display_real(Value_expression);
          return;
        case STR_tag:
          display_string(Value_expression);
          return;
        case SYM_tag:
          display_symbol(Value_expression);
          return;
        case THK_tag:
          display_thunk(Value_expression);
          return;
        case TRU_tag:
          display_true();
          return;
        case UDF_tag:
          display_undefined();
        case USP_tag:
          display_unspecified();
          return;
        case VEC_tag:
          display_vector(Value_expression);
          return;
        default:
          print_error(IXT_error,
                      Error_display_rawstring); }}

/*-------------------------------- private print functions -----------------------------*/

static UNS_type Scope;
static UNS_type Indentation;

static NIL_type enter_scope(VEC_type Symbol_vector)
  { Scope += 1; }

static NIL_type exit_scope(NIL_type)
  { Scope -= 1; }

static NIL_type indent(NIL_type)
  { Indentation += 2; }

static NIL_type outdent(NIL_type)
  { Indentation -= 2; }

/*--------------------------------------------------------------------------------------*/

static NIL_type print_rawstring(RWS_type Rawstring)
  { FORMAT(Rawstring_print_rawstring,
           Rawstring); }

static NIL_type print_binding_vector(VEC_type Binding_vector)
  { EXP_type expression;
    UNS_type index,
             size;
    size = size_VEC(Binding_vector);
    if (size > 0)
      for (index = 1;
           index <= size;
           index += 1)
        { expression = Binding_vector[index];
          if (index > 1)
            print_rawstring(Space_print_rawstring);
          print_rawstring(Leftpar_print_rawstring);
          print_variable(Scope,
                         index);
          print_rawstring(Space_print_rawstring);
          print_expression(expression);
          print_rawstring(Rightpar_print_rawstring); }}

static NIL_type print_newline(NIL_type)
  { UNS_type index;
    print_rawstring(Newline_print_rawstring);
    for (index = 0;
         index < Indentation;
         index += 1)
       print_rawstring(Space_print_rawstring); }

static NIL_type print_operand_vector(VEC_type Operand_vector)
  { EXP_type expression;
    UNS_type size,
             index;
 	  size = size_VEC(Operand_vector);
    for (index = 1;
         index <= size;
         index += 1)
      { print_rawstring(Space_print_rawstring);
        expression = Operand_vector[index];
        print_expression(expression); }}

static NIL_type print_pattern(UNS_type Parameter_count)
  { UNS_type index;
    if (Parameter_count > 0)
      for (index = 1;
           index <= Parameter_count;
           index += 1)
        { if (index > 1)
            print_rawstring(Space_print_rawstring);
          print_variable(Scope,
                         index); }}

static NIL_type print_variable(UNS_type Scope,
                               UNS_type Offset)
  { SYM_type variable_symbol;
    if (Scope == 1)
      { variable_symbol = Environment_Global_Symbol_Name(Offset);
        print_symbol(variable_symbol);
        return; }
    FORMAT(Variable_print_rawstring,
           Scope);
    FORMAT(Variable_bis_print_rawstring,
           Offset);  }

/*--------------------------------------------------------------------------------------*/

static NIL_type print_keyword(KEY_type Key)
  { SYM_type symbol;
    RWS_type rawstring;
    symbol = Pool_Keyword_Symbol(Key);
    rawstring = symbol->rws;
    FORMAT(Keyword_print_rawstring,
           rawstring); }

static NIL_type print_and(AND_type And)
  { VEC_type predicate_vector;
    predicate_vector = And->prd;
    print_rawstring(Leftpar_print_rawstring);
    print_keyword(Pool_And);
    print_operand_vector(predicate_vector);
    print_rawstring(Rightpar_print_rawstring); }

static NIL_type print_application(APL_type Application)
  { EXP_type operator;
    VEC_type operand_vector;
    operator = Application->exp;
    operand_vector = Application->opd;
    print_rawstring(Leftpar_print_rawstring);
    print_expression(operator);
    print_operand_vector(operand_vector);
    print_rawstring(Rightpar_print_rawstring); }

static NIL_type print_begin(BEG_type Begin)
  { EXP_type body_expression;
    body_expression = Begin->bod;
    print_rawstring(Leftpar_print_rawstring);
    print_keyword(Pool_Begin);
    indent();
    print_newline();
    print_expression(body_expression);
    outdent();
    print_rawstring(Rightpar_print_rawstring); }

static NIL_type print_character(CHA_type Character)
  { RWC_type rawcharacter;
    rawcharacter = get_CHA(Character);
    FORMAT(Rawcharacter_print_rawstring,
           rawcharacter); }

static NIL_type print_clauses(VEC_type Predicate_vector,
                              VEC_type Body_vector)
  { EXP_type body_expression,
             predicate_expression;
    UNS_type size,
             index;
 	  size = size_VEC(Predicate_vector);
    indent();
    for (index = 1;
         index <= size;
         index += 1)
      { print_newline();
        print_rawstring(Leftpar_print_rawstring);
        predicate_expression = Predicate_vector[index];
        body_expression = Body_vector[index];
        print_expression(predicate_expression);
        print_rawstring(Space_print_rawstring);
        print_expression(body_expression);
        print_rawstring(Rightpar_print_rawstring); }
    outdent(); }

static NIL_type print_clause_else(EXP_type Else_expression)
  { indent();
    print_newline();
    print_rawstring(Leftpar_print_rawstring);
    print_keyword(Pool_Else);
    print_rawstring(Space_print_rawstring);
    print_expression(Else_expression);
    print_rawstring(Rightpar_print_rawstring);
    outdent(); }

static NIL_type print_cond_with_else(CNE_type Cond_else)
  { EXP_type else_expression;
    VEC_type body_vector,
             predicate_vector;
    predicate_vector = Cond_else->prd;
    body_vector      = Cond_else->bod;
    else_expression  = Cond_else->els;
    print_rawstring(Leftpar_print_rawstring);
    print_keyword(Pool_Cond);
    print_clauses(predicate_vector,
                  body_vector);
    print_clause_else(else_expression);
    print_rawstring(Rightpar_print_rawstring); }

static NIL_type print_cond_w_o_else(CND_type Cond)
  { VEC_type body_vector,
             predicate_vector;
    predicate_vector = Cond->prd;
    body_vector      = Cond->bod;
    print_rawstring(Leftpar_print_rawstring);
    print_keyword(Pool_Cond);
    print_clauses(predicate_vector,
                  body_vector);
    print_rawstring(Rightpar_print_rawstring); }

static NIL_type print_continuation(CNT_type Continuation)
  { print_rawstring(Continuation_print_rawstring); }

static NIL_type print_define_function(DFF_type Define_function_expression)
  { EXP_type body_expression;
    NBR_type offset_number,
             parameter_count_number;
    UNS_type offset,
             parameter_count;
    VEC_type symbol_vector;
    offset_number          = Define_function_expression->ofs;
    parameter_count_number = Define_function_expression->par;
    body_expression        = Define_function_expression->bod;
    symbol_vector          = Define_function_expression->smv;
    offset = get_NBR(offset_number);
    parameter_count = get_NBR(parameter_count_number);
    print_rawstring(Leftpar_print_rawstring);
    print_keyword(Pool_Define);
    print_rawstring(Space_print_rawstring);
    print_rawstring(Leftpar_print_rawstring);
    print_variable(Scope,
                   offset);
    enter_scope(symbol_vector);
    print_rawstring(Space_print_rawstring);
    print_pattern(parameter_count);
    print_rawstring(Rightpar_print_rawstring);
    indent();
    print_newline();
    print_expression(body_expression);
    outdent();
    exit_scope();
    print_rawstring(Rightpar_print_rawstring);  }

static NIL_type print_define_function_vararg(DFZ_type Define_function_vararg)
  { EXP_type body_expression;
    NBR_type offset_number,
             parameter_count_number;
    UNS_type offset,
             parameter_count;
    VEC_type symbol_vector;
    offset_number          = Define_function_vararg->ofs;
    parameter_count_number = Define_function_vararg->par;
    body_expression        = Define_function_vararg->bod;
    symbol_vector          = Define_function_vararg->smv;
    offset = get_NBR(offset_number);
    parameter_count = get_NBR(parameter_count_number);
    print_rawstring(Leftpar_print_rawstring);
    print_keyword(Pool_Define);
    print_rawstring(Space_print_rawstring);
    print_rawstring(Leftpar_print_rawstring);
    print_variable(Scope,
                   offset);
    enter_scope(symbol_vector);
    print_rawstring(Space_print_rawstring);
    print_pattern(parameter_count);
    print_rawstring(Period_print_rawstring);
    print_variable(Scope,
                   parameter_count + 1);
    print_rawstring(Rightpar_print_rawstring);
    indent();
    print_newline();
    print_expression(body_expression);
    outdent();
    exit_scope();
    print_rawstring(Rightpar_print_rawstring);  }

static NIL_type print_define_variable(DFV_type Define_variable)
  { EXP_type expression;
    NBR_type offset_number;
    UNS_type offset;
    offset_number = Define_variable->ofs;
    expression    = Define_variable->exp;
    offset = get_NBR(offset_number);
    print_rawstring(Leftpar_print_rawstring);
    print_keyword(Pool_Define);
    print_rawstring(Space_print_rawstring);
    print_variable(Scope,
                   offset);
    print_rawstring(Space_print_rawstring);
    print_expression(expression);
    print_rawstring(Rightpar_print_rawstring); }

static NIL_type print_delay(DEL_type Delay)
  { EXP_type expression;
    expression = Delay->exp;
    print_rawstring(Leftpar_print_rawstring);
    print_keyword(Pool_Delay);
    print_rawstring(Space_print_rawstring);
    print_expression(expression);
    print_rawstring(Rightpar_print_rawstring); }

static NIL_type print_false(NIL_type)
  { print_rawstring(False_print_rawstring); }

static NIL_type print_force(FRC_type Force)
  { print_rawstring(Force_print_rawstring); }

static NIL_type print_global(GLB_type Global)
  { NBR_type offset_number,
             scope_number;
    UNS_type offset,
             scope;
    scope_number  = Global->scp;
    offset_number = Global->ofs;
    scope = get_NBR(scope_number);
    offset = get_NBR(offset_number);
    print_variable(scope,
                   offset); }

static NIL_type print_global_application(GLA_type Global_application)
  { GLB_type global;
    VEC_type operand_vector;
    global = Global_application->glb;
    operand_vector = Global_application->opd;
    print_rawstring(Leftpar_print_rawstring);
    print_global(global);
    print_operand_vector(operand_vector);
    print_rawstring(Rightpar_print_rawstring); }

static NIL_type print_if_double(IFD_type If_double)
  { EXP_type alternative_expression,
             consequent_expression,
             predicate_expression;
    predicate_expression   = If_double->prd;
    consequent_expression  = If_double->cns;
    alternative_expression = If_double->alt;
    print_rawstring(Leftpar_print_rawstring );
    print_keyword(Pool_If);
    print_rawstring(Space_print_rawstring);
    print_expression(predicate_expression);
    indent();
    print_newline();
    print_expression(consequent_expression);
    print_newline();
    print_expression(alternative_expression);
    print_rawstring(Rightpar_print_rawstring);
    outdent(); }

static NIL_type print_if_single(IFS_type If_single)
  { EXP_type consequent_expression,
             predicate_expression;
    predicate_expression  = If_single->prd;
    consequent_expression = If_single->cns;
    print_rawstring(Leftpar_print_rawstring );
    print_keyword(Pool_If);
    print_rawstring(Space_print_rawstring);
    print_expression(predicate_expression);
    indent();
    print_newline();
    print_expression(consequent_expression);
    print_rawstring(Rightpar_print_rawstring);
    outdent(); }

static NIL_type print_ken_id(KID_type Ken_id)
  { RWC_type ken_id_buf[22];
    ken_id_to_string(Ken_id->rwk, ken_id_buf, 22);
    FORMAT(Ken_id_print_rawstring,
           ken_id_buf); }

static NIL_type print_let(LET_type Let)
  { EXP_type body_expression;
    VEC_type binding_vector,
             symbol_vector;
    binding_vector  = Let->bnd;
    body_expression = Let->bod;
    symbol_vector   = Let->smv;
    print_rawstring(Leftpar_print_rawstring);
    print_keyword(Pool_Let);
    print_rawstring(Space_print_rawstring);
    enter_scope(symbol_vector);
    print_rawstring(Leftpar_print_rawstring);
    print_binding_vector(binding_vector);
    print_rawstring(Rightpar_print_rawstring);
    print_rawstring(Space_print_rawstring);
    indent();
    print_newline();
    print_expression(body_expression);
    exit_scope();
    print_rawstring(Rightpar_print_rawstring);
    outdent();  }

static NIL_type print_lambda(LMB_type Lambda)
  { EXP_type body_expression;
    NBR_type parameter_count_number;
    UNS_type parameter_count;
    VEC_type symbol_vector;
    parameter_count_number = Lambda->par;
    body_expression        = Lambda->bod;
    symbol_vector          = Lambda->smv;
    parameter_count = get_NBR(parameter_count_number);
    print_rawstring(Leftpar_print_rawstring);
    print_keyword(Pool_Lambda);
    print_rawstring(Space_print_rawstring);
    print_rawstring(Leftpar_print_rawstring);
    enter_scope(symbol_vector);
    print_pattern(parameter_count);
    print_rawstring(Rightpar_print_rawstring);
    print_expression(body_expression);
    exit_scope();
    print_rawstring(Rightpar_print_rawstring);  }

static NIL_type print_lambda_vararg(LMV_type Lambda_vararg)
  { EXP_type body_expression;
    NBR_type parameter_count_number;
    UNS_type parameter_count;
    VEC_type symbol_vector;
    parameter_count_number = Lambda_vararg->par;
    body_expression        = Lambda_vararg->bod;
    symbol_vector          = Lambda_vararg->smv;
    parameter_count = get_NBR(parameter_count_number);
    print_rawstring(Leftpar_print_rawstring);
    print_keyword(Pool_Lambda);
    print_rawstring(Space_print_rawstring);
    enter_scope(symbol_vector);
    if (parameter_count == 0)
      print_variable(Scope,
                     1);
    else
      { print_rawstring(Leftpar_print_rawstring);
        print_pattern(parameter_count);
        print_rawstring(Period_print_rawstring);
        print_variable(Scope,
                       parameter_count + 1);
        print_rawstring(Rightpar_print_rawstring); }
    print_expression(body_expression);
    exit_scope();
    print_rawstring(Rightpar_print_rawstring);  }

static NIL_type print_local(LCL_type Local)
  { NBR_type offset_number;
    UNS_type offset;
    offset_number = Local->ofs;
    offset = get_NBR(offset_number);
    print_variable(Scope,
                   offset); }

static NIL_type print_local_application(LCA_type Local_application)
  { LCL_type local;
    VEC_type operand_vector;
    local = Local_application->lcl;
    operand_vector = Local_application->opd;
    print_rawstring(Leftpar_print_rawstring);
    print_local(local);
    print_operand_vector(operand_vector);
    print_rawstring(Rightpar_print_rawstring); }

static NIL_type print_native(NAT_type Native)
  { RWS_type rawstring;
    rawstring = Native->nam;
    FORMAT(Native_print_rawstring,
           rawstring); }

static NIL_type print_null(NIL_type)
  { print_rawstring(Empty_list_print_rawstring); }

static NIL_type print_number(NBR_type Number)
  { RWN_type rawnumber;
    rawnumber = get_NBR(Number);
    FORMAT(Rawnumber_print_rawstring,
           rawnumber); }

static NIL_type print_or(AND_type Or)
  { VEC_type predicate_vector;
    predicate_vector = Or->prd;
    print_rawstring(Leftpar_print_rawstring);
    print_keyword(Pool_Or);
    print_operand_vector(predicate_vector);
    print_rawstring(Rightpar_print_rawstring); }

static NIL_type print_pair(PAI_type Pair)
  { EXP_type residue_expression,
             value_expression;
    PAI_type pair;
    TAG_type tag;
    print_rawstring(Quote_print_rawstring);                 /* only top level */
    print_rawstring(Leftpar_print_rawstring);
    for (pair = Pair;
         ;
         )
      { value_expression   = pair->car;
        residue_expression = pair->cdr;
        print_expression(value_expression);
        tag = Grammar_Tag(residue_expression);
        switch (tag)
          { case NUL_tag:
              print_rawstring(Rightpar_print_rawstring);
              return;
            case PAI_tag:
              print_rawstring(Space_print_rawstring);
              pair = residue_expression;
              break;
            default:
              print_rawstring(Period_print_rawstring);
              print_expression(residue_expression);
              print_rawstring(Rightpar_print_rawstring);
              return; }}}

static NIL_type print_procedure(PRC_type Procedure)
  { RWS_type rawstring;
    SYM_type name_symbol;
    name_symbol = Procedure->nam;
    rawstring = name_symbol->rws;
    FORMAT(Procedure_print_rawstring,
           rawstring); }

static NIL_type print_procedure_vararg(PRV_type Procedure_vararg)
  { RWS_type rawstring;
    SYM_type name_symbol;
    name_symbol = Procedure_vararg->nam;
    rawstring = name_symbol->rws;
    FORMAT(Procedure_print_rawstring,
           rawstring); }

static NIL_type print_promise(PRM_type Promise)
  { print_rawstring(Promise_print_rawstring); }

static NIL_type print_quasiquote(QQU_type Quasiquote)
  { EXP_type expression;
    expression = Quasiquote->exp;
    print_rawstring(Quasiquote_print_rawstring);
    print_expression(expression); }

static NIL_type print_unquote(UNQ_type Unquote)
  { EXP_type expression;
    expression = Unquote->exp;
    print_rawstring(Unquote_print_rawstring);
    print_expression(expression); }

static NIL_type print_unquote_splicing(UQS_type Unquote_splicing)
  { EXP_type expression;
    expression = Unquote_splicing->exp;
    print_rawstring(Unquote_splicing_print_rawstring);
    print_expression(expression); }

static NIL_type print_real(REA_type Real)
  { RWR_type rawreal;
    rawreal = Real->rwr;
    FORMAT(Rawreal_print_rawstring,
           rawreal); }

static NIL_type print_sequence(SEQ_type Sequence)
  { EXP_type expression;
    UNS_type index,
             size;
    size = size_SEQ(Sequence);
    expression = Sequence[1];
    print_expression(expression);
    for (index = 2;
         index <= size;
         index += 1)
      { print_newline();
        expression = Sequence[index];
        print_expression(expression); }}

static NIL_type print_set_global(STG_type Set_global)
  { EXP_type expression;
    NBR_type offset_number,
             scope_number;
    UNS_type offset,
             scope;
    scope_number  = Set_global->scp;
    offset_number = Set_global->ofs;
    expression    = Set_global->exp;
    scope = get_NBR(scope_number);
    offset = get_NBR(offset_number);
    print_rawstring(Leftpar_print_rawstring);
    print_keyword(Pool_Set);
    print_rawstring(Space_print_rawstring);
    print_variable(scope,
                   offset);
    print_rawstring(Space_print_rawstring);
    print_expression(expression);
    print_rawstring(Rightpar_print_rawstring); }

static NIL_type print_set_local(STL_type Set_local)
  { EXP_type expression;
    NBR_type offset_number;
    UNS_type offset;
    offset_number = Set_local->ofs;
    expression    = Set_local->exp;
    offset = get_NBR(offset_number);
    print_rawstring(Leftpar_print_rawstring);
    print_keyword(Pool_Set);
    print_rawstring(Space_print_rawstring);
    print_variable(Scope,
                   offset);
    print_rawstring(Space_print_rawstring);
    print_expression(expression);
    print_rawstring(Rightpar_print_rawstring); }

static NIL_type print_string(STR_type String)
  { RWS_type rawstring;
    rawstring = String->rws;
    FORMAT(String_print_rawstring,
           rawstring); }

static NIL_type print_symbol(SYM_type Symbol)
  { RWS_type rawstring;
    rawstring = Symbol->rws;
    FORMAT(Symbol_print_rawstring,
           rawstring); }

static NIL_type print_thunk(THK_type Thunk)
  { EXP_type body_expression;
    VEC_type symbol_vector;
    body_expression = Thunk->bod;
    symbol_vector   = Thunk->smv;
    enter_scope(symbol_vector);
    print_expression(body_expression);
    exit_scope(); }

static NIL_type print_true(NIL_type)
  { print_rawstring(True_print_rawstring); }

static NIL_type print_undefined(NIL_type)
  { print_rawstring(Undefined_print_rawstring); }

static NIL_type print_unspecified(NIL_type)
  { print_rawstring(Unspecified_print_rawstring); }

static NIL_type print_vector(VEC_type Vector)
  { EXP_type value_expression;
    UNS_type index,
             size;
    size = size_VEC(Vector);
    print_rawstring(Leftvec_print_rawstring);
    for (index = 1;
         index <= size;
         index += 1)
      { value_expression = Vector[index];
        print_expression(value_expression);
        if (index < size)
          print_rawstring(Space_print_rawstring); }
    print_rawstring(Rightvec_print_rawstring); }

static NIL_type print_while(WHI_type While_)
  { EXP_type body_expression,
             predicate_expression;
    predicate_expression = While_->prd;
    body_expression      = While_->bod;
    print_rawstring(Leftpar_print_rawstring);
    print_keyword(Pool_While);
    print_rawstring(Space_print_rawstring);
    print_expression(predicate_expression);
    indent();
    print_newline();
    print_expression(body_expression);
    outdent();
    print_rawstring(Rightpar_print_rawstring); }

/*--------------------------------------------------------------------------------------*/

static NIL_type print_expression(EXP_type Expression)
  { TAG_type tag;
    tag = Grammar_Tag(Expression);
    switch (tag)
      { case AND_tag:
          print_and(Expression);
          return;
        case APL_tag:
          print_application(Expression);
          return;
        case BEG_tag:
          print_begin(Expression);
          return;
        case CHA_tag:
          print_character(Expression);
          return;
        case CND_tag:
          print_cond_w_o_else(Expression);
          return;
        case CNE_tag:
          print_cond_with_else(Expression);
          return;
        case CNT_tag:
          print_continuation(Expression);
          return;
        case DEL_tag:
          print_delay(Expression);
          return;
        case DFF_tag:
          print_define_function(Expression);
          return;
        case DFV_tag:
          print_define_variable(Expression);
          return;
        case DFZ_tag:
          print_define_function_vararg(Expression);
          return;
        case FLS_tag:
          print_false();
          return;
        case FRC_tag:
          print_force(Expression);
          return;
        case GLA_tag:
          print_global_application(Expression);
          return;
        case GLB_tag:
          print_global(Expression);
          return;
        case IFD_tag:
          print_if_double(Expression);
          return;
        case IFS_tag:
          print_if_single(Expression);
          return;
        case KID_tag:
          print_ken_id(Expression);
          return;
        case LCA_tag:
          print_local_application(Expression);
          return;
        case LCL_tag:
          print_local(Expression);
          return;
        case LET_tag:
          print_let(Expression);
          return;
        case LMB_tag:
          print_lambda(Expression);
          return;
        case LMV_tag:
          print_lambda_vararg(Expression);
          return;
        case NAT_tag:
          print_native(Expression);
          return;
        case NBR_tag:
          print_number(Expression);
          return;
        case NUL_tag:
          print_null();
          return;
        case ORR_tag:
          print_or(Expression);
          return;
        case PAI_tag:
          print_pair(Expression);
          return;
        case PRC_tag:
          print_procedure(Expression);
          return;
        case PRM_tag:
          print_promise(Expression);
          return;
        case PRV_tag:
          print_procedure_vararg(Expression);
          return;
        case QQU_tag:
          print_quasiquote(Expression);
          return;
        case REA_tag:
          print_real(Expression);
          return;
        case SEQ_tag:
          print_sequence(Expression);
          return;
        case STG_tag:
          print_set_global(Expression);
          return;
        case STL_tag:
          print_set_local(Expression);
          return;
        case STR_tag:
          print_string(Expression);
          return;
        case SYM_tag:
          print_symbol(Expression);
          return;
        case THK_tag:
          print_thunk(Expression);
          return;
        case TRU_tag:
          print_true();
          return;
        case UNQ_tag:
          print_unquote(Expression);
          return;
        case UQS_tag:
          print_unquote_splicing(Expression);
          return;
        case UDF_tag:
          print_undefined();
          return;
        case USP_tag:
          print_unspecified();
          return;
        case VEC_tag:
          print_vector(Expression);
          return;
        case WHI_tag:
          print_while(Expression);
          return;
        default:
          print_error(IXT_error,
                      Error_print_rawstring); }}

/*----------------------------------- public functions ---------------------------------*/

NIL_type Print_Display(EXP_type Value_expression)
  { display_expression(Value_expression); }

NIL_type Print_Print(EXP_type Expression)
  { Indentation = 0;
    Scope = 1;
    print_expression(Expression); }

/*--------------------------------------------------------------------------------------*/

NIL_type Print_Initialize(NIL_type)
  { return; }
