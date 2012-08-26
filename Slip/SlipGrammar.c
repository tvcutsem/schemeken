                     /*---------------------------------------------*/
                     /*                  >>>Slip<<<                 */
                     /*                 Theo D'Hondt                */
                     /*          VUB Software Languages Lab         */
                     /*                  (c) 2012                   */
                     /*---------------------------------------------*/
                     /*   version 13: completion and optimization   */
                     /*---------------------------------------------*/
                     /*                    grammar                  */
                     /*---------------------------------------------*/

#include <string.h>

#include "SlipMain.h"
#include "SlipPersist.h"

/*------------------------------------ private macros ----------------------------------*/

#define MAKE_CHUNK_WITH_OFFSET_M(SIGNATURE, SIZE)                                        \
  (SIGNATURE##_type)Memory_Make_Chunk_M(SIGNATURE##_tag,                                 \
                                         SIGNATURE##_size + SIZE)

#define MAKE_CHUNK_M(SIGNATURE)                                                          \
  MAKE_CHUNK_WITH_OFFSET_M(SIGNATURE, 0)

/*----------------------------------- private constants --------------------------------*/

enum { Immediate_null_value        = Memory_Void_Value +   0*Memory_Cell_Bias,
       Immediate_true_value        = Memory_Void_Value +   1*Memory_Cell_Bias,
       Immediate_false_value       = Memory_Void_Value +   2*Memory_Cell_Bias,
       Immediate_unspecified_value = Memory_Void_Value +   3*Memory_Cell_Bias,
       Immediate_undefined_value   = Memory_Void_Value +   4*Memory_Cell_Bias,
       Immediate_character         = Memory_Void_Value +   5*Memory_Cell_Bias,
       Immediate_boundary          = Memory_Void_Value + 260*Memory_Cell_Bias };

enum { Ken_ID_Pool_Size = 32 };

static const RWS_type Empty_rawstring   = "";
static const RWS_type Newline_rawstring = "\n";

/*----------------------------------- public variables ---------------------------------*/

STR_type Grammar_Empty_String;
VEC_type Grammar_Empty_Vector;
FLS_type Grammar_False;
STR_type Grammar_Newline_String;
NUL_type Grammar_Null;
NBR_type Grammar_One_Number;
TRU_type Grammar_True;
UDF_type Grammar_Undefined;
USP_type Grammar_Unspecified;
NBR_type Grammar_Zero_Number;

/*----------------------------------- private functions --------------------------------*/

static BLN_type member_of(EXP_type Expression,
                          TAG_type Tag)
  { TAG_type tag;
    tag = Grammar_Tag(Expression);
    return (tag == Tag); }

static UNS_type string_to_expression_size(RWS_type Rawstring)
  { UNS_type size;
    size = strlen(Rawstring) / CEL_SIZE + 1;
    return size; }

/*----------------------------------- public functions ---------------------------------*/

TAG_type Grammar_Boundary(NIL_type)
  { return _RG_tag - 1; }

TAG_type Grammar_Tag(EXP_type Expression)
  { TAG_type tag;
    UNS_type immediate;
    if (Memory_Is_Immediate(Expression))
      return NBR_tag;
    immediate = (UNS_type)Expression;
    if (immediate <= Immediate_boundary)
      switch (immediate)
        { case Immediate_null_value:
            return NUL_tag;
          case Immediate_true_value:
            return TRU_tag;
          case Immediate_false_value:
            return FLS_tag;
            case Immediate_undefined_value:
                return UDF_tag;
            case Immediate_unspecified_value:
                return USP_tag;
          default:
            return CHA_tag; }
    tag = (TAG_type)Memory_Get_Tag((PTR_type)Expression);
    return tag; }

/*----------------------------------- AND declarations ---------------------------------*/

BLN_type is_AND(EXP_type Expression)
  { return member_of(Expression,
                     AND_tag); }

AND_type make_AND_M(VEC_type Predicate_vector)
  { AND_type and;
    and = MAKE_CHUNK_M(AND);
    and->prd = Predicate_vector;
    return and; }

/*----------------------------------- APL declarations ---------------------------------*/

BLN_type is_APL(EXP_type Expression)
  { return member_of(Expression,
                     APL_tag); }

APL_type make_APL_M(EXP_type Expression,
                    VEC_type Operand_vector)
  { APL_type application;
    application = MAKE_CHUNK_M(APL);
    application->exp = Expression;
    application->opd = Operand_vector;
    return application; }

/*----------------------------------- BEG declarations ---------------------------------*/

BLN_type is_BEG(EXP_type Expression)
  { return member_of(Expression,
                     BEG_tag); }

BEG_type make_BEG_M(EXP_type Body_expression)
  { BEG_type begin;
    begin = MAKE_CHUNK_M(BEG);
    begin->bod = Body_expression;
    return begin; }

/*----------------------------------- CHA declarations ---------------------------------*/

BLN_type is_CHA(EXP_type Expression)
  { return member_of(Expression,
                     CHA_tag); }

CHA_type make_CHA(RWC_type Rawcharacter)
  { CHA_type character;
    UNS_type immediate_value;
    immediate_value = Immediate_character + Rawcharacter * Memory_Cell_Bias;
    character = (CHA_type)immediate_value;
    return character; }

RWC_type get_CHA(CHA_type Character)
  { RWC_type rawcharacter;
    UNS_type immediate;
    immediate = (UNS_type)Character - Immediate_character;
    immediate /= Memory_Cell_Bias;
    rawcharacter = (RWC_type)immediate;
    return rawcharacter; }

/*----------------------------------- CND declarations ---------------------------------*/

BLN_type is_CND(EXP_type Expression)
  { return member_of(Expression,
                     CND_tag); }

CND_type make_CND_M(VEC_type Predicate_vector,
                    VEC_type Body_vector)
  { CND_type cond;
    cond = MAKE_CHUNK_M(CND);
    cond->prd = Predicate_vector;
    cond->bod = Body_vector;
    return cond; }

/*----------------------------------- CNE declarations ---------------------------------*/

BLN_type is_CNE(EXP_type Expression)
  { return member_of(Expression,
                     CNE_tag); }

CNE_type make_CNE_M(VEC_type Predicate_vector,
                    VEC_type Body_vector,
                    EXP_type Else_expression)
  { CNE_type cond_else;
    cond_else = MAKE_CHUNK_M(CNE);
    cond_else->prd = Predicate_vector;
    cond_else->bod = Body_vector;
    cond_else->els = Else_expression;
    return cond_else; }

/*----------------------------------- CNT declarations ---------------------------------*/

BLN_type is_CNT(EXP_type Expression)
  { return member_of(Expression,
                     CNT_tag); }

CNT_type make_CNT_M(ENV_type Environment,
                    FRM_type Frame,
                    THR_type Thread)
  { CNT_type continuation;
    continuation = MAKE_CHUNK_M(CNT);
    continuation->env = Environment;
    continuation->frm = Frame;
    continuation->thr = Thread;
    return continuation; }

/*----------------------------------- DEL declarations ---------------------------------*/

BLN_type is_DEL(EXP_type Expression)
  { return member_of(Expression,
                     DEL_tag); }

DEL_type make_DEL_M(NBR_type Frame_vector_size_number,
                    EXP_type Expression,
                    VEC_type Symbol_vector)
  { DEL_type delay;
    delay = MAKE_CHUNK_M(DEL);
    delay->siz = Frame_vector_size_number;
    delay->exp = Expression;
    delay->smv = Symbol_vector;
    return delay; }

/*----------------------------------- DFF declarations ---------------------------------*/

BLN_type is_DFF(EXP_type Expression)
  { return member_of(Expression,
                     DFF_tag); }

DFF_type make_DFF_M(SYM_type Name_symbol,
                    NBR_type Offset_number,
                    NBR_type Parameter_count_number,
                    NBR_type Frame_vector_size_number,
                    EXP_type Body_expression,
                    VEC_type Symbol_vector)
  { DFF_type define_function;
    define_function = MAKE_CHUNK_M(DFF);
    define_function->nam = Name_symbol;
    define_function->ofs = Offset_number;
    define_function->par = Parameter_count_number;
    define_function->siz = Frame_vector_size_number;
    define_function->bod = Body_expression;
    define_function->smv = Symbol_vector;
    return define_function; }

/*----------------------------------- DFV declarations ---------------------------------*/

BLN_type is_DFV(EXP_type Expression)
  { return member_of(Expression,
                     DFV_tag); }

DFV_type make_DFV_M(NBR_type Offset_number,
                    EXP_type Expression)
  { DFV_type define_variable;
    define_variable = MAKE_CHUNK_M(DFV);
    define_variable->ofs = Offset_number;
    define_variable->exp = Expression;
    return define_variable; }

/*----------------------------------- DFZ declarations ---------------------------------*/

BLN_type is_DFZ(EXP_type Expression)
  { return member_of(Expression,
                     DFZ_tag); }

DFZ_type make_DFZ_M(SYM_type Name_symbol,
                    NBR_type Offset_number,
                    NBR_type Parameter_count_number,
                    NBR_type Frame_vector_size_number,
                    EXP_type Body_expression,
                    VEC_type Symbol_vector)
  { DFZ_type define_function_vararg;
    define_function_vararg = MAKE_CHUNK_M(DFZ);
    define_function_vararg->nam = Name_symbol;
    define_function_vararg->ofs = Offset_number;
    define_function_vararg->par = Parameter_count_number;
    define_function_vararg->siz = Frame_vector_size_number;
    define_function_vararg->bod = Body_expression;
    define_function_vararg->smv = Symbol_vector;
    return define_function_vararg; }

/*----------------------------------- FLS declarations ---------------------------------*/

BLN_type is_FLS(EXP_type Expression)
  { return (Expression == (EXP_type)Immediate_false_value); }

FLS_type make_FLS(NIL_type)
  { FLS_type false_;
    false_ = (FLS_type)Immediate_false_value;
    return false_; }

/*----------------------------------- FRC declarations ---------------------------------*/

BLN_type is_FRC(EXP_type Expression)
  { return member_of(Expression,
                     FRC_tag); }

NIL_type convert_FRC(PRM_type Promise,
                     EXP_type Value_expression)
  { FRC_type force;
    force = (FRC_type)Promise;
    Memory_Set_Tag((PTR_type)force,
                   FRC_tag);
    force->val = Value_expression; }

/*----------------------------------- GLA declarations ---------------------------------*/

BLN_type is_GLA(EXP_type Expression)
  { return member_of(Expression,
                     GLA_tag); }

GLA_type make_GLA_M(GLB_type Global,
                    VEC_type Operand_vector)
  { GLA_type global_application;
    global_application = MAKE_CHUNK_M(GLA);
    global_application->glb = Global;
    global_application->opd = Operand_vector;
    return global_application; }

/*----------------------------------- GLB declarations ---------------------------------*/

BLN_type is_GLB(EXP_type Expression)
  { return member_of(Expression,
                     GLB_tag); }

GLB_type make_GLB_M(NBR_type Scope_number,
                    NBR_type Offset_number)
  { GLB_type global;
    global = MAKE_CHUNK_M(GLB);
    global->scp = Scope_number;
    global->ofs = Offset_number;
    return global; }

/*----------------------------------- IFD declarations ---------------------------------*/

BLN_type is_IFD(EXP_type Expression)
  { return member_of(Expression,
                     IFD_tag); }

IFD_type make_IFD_M(EXP_type Predicate_expression,
                    EXP_type Consequent_expression,
                    EXP_type Alternative_expression)
  { IFD_type if_double;
    if_double = MAKE_CHUNK_M(IFD);
    if_double->prd = Predicate_expression;
    if_double->cns = Consequent_expression;
    if_double->alt = Alternative_expression;
    return if_double; }

/*----------------------------------- IFS declarations ---------------------------------*/

BLN_type is_IFS(EXP_type Expression)
  { return member_of(Expression,
                     IFS_tag); }

IFS_type make_IFS_M(EXP_type Predicate_expression,
                    EXP_type Consequent_expression)
  { IFS_type if_single;
    if_single = MAKE_CHUNK_M(IFS);
    if_single->prd = Predicate_expression;
    if_single->cns = Consequent_expression;
    return if_single; }

/*----------------------------------- KID declarations ---------------------------------*/

BLN_type is_KID(EXP_type Expression)
  { return member_of(Expression,
                     KID_tag); }

static VEC_type Ken_ID_Pool;

KID_type make_KID_M(RWK_type Rawid)
  { KID_type kid_value;
    EXP_type exp;
    UNS_type index;
    for (index = 1;
         index <= Ken_ID_Pool_Size;
         index += 1)
      { exp = Ken_ID_Pool[index];
        if (exp == Grammar_Undefined)
          { kid_value = MAKE_CHUNK_M(KID);
            kid_value->rwk = Rawid;
            Ken_ID_Pool[index] = kid_value;
            return kid_value; }
        kid_value = (KID_type)exp;
        if (0 == ken_id_cmp(kid_value->rwk, Rawid))
          return kid_value; }
    kid_value = MAKE_CHUNK_M(KID);
    kid_value->rwk = Rawid;
    return kid_value; }

NIL_type initialize_Ken_ID_Pool_M()
  { slipken_persist_init(Ken_ID_Pool, make_VEC_M(Ken_ID_Pool_Size));
    make_KID_M(ken_id());
    make_KID_M(kenid_NULL);
    make_KID_M(kenid_stdout);
    make_KID_M(kenid_alarm);
    make_KID_M(kenid_stdin);
    MAIN_REGISTER(Ken_ID_Pool); }

/*----------------------------------- LCA declarations ---------------------------------*/

BLN_type is_LCA(EXP_type Expression)
  { return member_of(Expression,
                     LCA_tag); }

LCA_type make_LCA_M(LCL_type Local,
                    VEC_type Operand_vector)
  { LCA_type local_application;
    local_application = MAKE_CHUNK_M(LCA);
    local_application->lcl = Local;
    local_application->opd = Operand_vector;
    return local_application; }

/*----------------------------------- LCL declarations ---------------------------------*/

BLN_type is_LCL(EXP_type Expression)
  { return member_of(Expression,
                     LCL_tag); }

LCL_type make_LCL_M(NBR_type Offset_number)
  { LCL_type local;
    local = MAKE_CHUNK_M(LCL);
    local->ofs = Offset_number;
    return local; }

/*----------------------------------- LET declarations ---------------------------------*/

BLN_type is_LET(EXP_type Expression)
  { return member_of(Expression,
                     LET_tag); }

LET_type make_LET_M(NBR_type Frame_vector_size_number,
                    VEC_type Binding_vector,
                    EXP_type Body_expression,
                    VEC_type Symbol_vector)
  { LET_type let;
    let = MAKE_CHUNK_M(LET);
    let->siz = Frame_vector_size_number;
    let->bnd = Binding_vector;
    let->bod = Body_expression;
    let->smv = Symbol_vector;
    return let; }

/*----------------------------------- LMB declarations ---------------------------------*/

BLN_type is_LMB(EXP_type Expression)
  { return member_of(Expression,
                     LMB_tag); }

LMB_type make_LMB_M(NBR_type Parameter_count_number,
                    NBR_type Frame_vector_size_number,
                    EXP_type Body_expression,
                    VEC_type Symbol_vector)
  { LMB_type lambda;
    lambda = MAKE_CHUNK_M(LMB);
    lambda->par = Parameter_count_number;
    lambda->siz = Frame_vector_size_number;
    lambda->bod = Body_expression;
    lambda->smv = Symbol_vector;
    return lambda; }

/*----------------------------------- LMV declarations ---------------------------------*/

BLN_type is_LMV(EXP_type Expression)
  { return member_of(Expression,
                     LMV_tag); }

LMV_type make_LMV_M(NBR_type Parameter_count_number,
                    NBR_type Frame_vector_size_number,
                    EXP_type Body_expression,
                    VEC_type Symbol_vector)
  { LMV_type lambda_vararg;
    lambda_vararg = MAKE_CHUNK_M(LMV);
    lambda_vararg->par = Parameter_count_number;
    lambda_vararg->siz = Frame_vector_size_number;
    lambda_vararg->bod = Body_expression;
    lambda_vararg->smv = Symbol_vector;
    return lambda_vararg; }

/*----------------------------------- MRK declarations ---------------------------------*/

BLN_type is_MRK(EXP_type Expression)
  { return member_of(Expression,
                     MRK_tag); }

NIL_type apply_MRK(EXP_type Expression)
  { Memory_Set_Tag((PTR_type)Expression,
                   MRK_tag); }

/*----------------------------------- NAT declarations ---------------------------------*/

BLN_type is_NAT(EXP_type Expression)
  { return member_of(Expression,
                     NAT_tag); }

NAT_type make_NAT_M(RNF_type Raw_native_function,
                    RWS_type Rawstring)
  { NAT_type native;
    UNS_type size;
    size = string_to_expression_size(Rawstring);
    native = MAKE_CHUNK_WITH_OFFSET_M(NAT,
                                      size);
    native->rnf = Raw_native_function;
    strcpy(native->nam,
           Rawstring);
    return native; }

/*----------------------------------- NBR declarations ---------------------------------*/

BLN_type is_NBR(EXP_type Expression)
  { return Memory_Is_Immediate((PTR_type)Expression); }

NBR_type make_NBR(RWN_type Rawnumber)
  { NBR_type number;
    number = Memory_Make_Immediate(Rawnumber);
    return number; }

RWN_type get_NBR(NBR_type Number)
  { RWN_type Rawnumber;
    Rawnumber = Memory_Get_Immediate(Number);
    return Rawnumber; }

/*----------------------------------- NUL declarations ---------------------------------*/

BLN_type is_NUL(EXP_type Expression)
  { return (Expression == (EXP_type)Immediate_null_value); }

NUL_type make_NUL(NIL_type)
  { NUL_type null;
    null = (NUL_type)Immediate_null_value;
    return null; }

/*----------------------------------- ORR declarations ---------------------------------*/

BLN_type is_ORR(EXP_type Expression)
  { return member_of(Expression,
                     ORR_tag); }

ORR_type make_ORR_M(VEC_type Predicate_vector)
  { ORR_type or;
    or = MAKE_CHUNK_M(ORR);
    or->prd = Predicate_vector;
    return or; }

/*----------------------------------- PAI declarations ---------------------------------*/

BLN_type is_PAI(EXP_type Expression)
  { return member_of(Expression,
                     PAI_tag); }

PAI_type make_PAI_M(EXP_type Car_expression,
                    EXP_type Cdr_expression)
  { PAI_type pair;
    pair = MAKE_CHUNK_M(PAI);
    pair->car = Car_expression;
    pair->cdr = Cdr_expression;
    return pair; }

/*----------------------------------- PRC declarations ---------------------------------*/

BLN_type is_PRC(EXP_type Expression)
  { return member_of(Expression,
                     PRC_tag); }

PRC_type make_PRC_M(SYM_type Name_symbol,
                    NBR_type Parameter_count_number,
                    NBR_type Frame_vector_size_number,
                    EXP_type Body_expression,
                    ENV_type Environment,
                    VEC_type Symbol_vector)
  { PRC_type procedure;
    procedure = MAKE_CHUNK_M(PRC);
    procedure->nam = Name_symbol;
    procedure->par = Parameter_count_number;
    procedure->siz = Frame_vector_size_number;
    procedure->bod = Body_expression;
    procedure->env = Environment;
    procedure->smv = Symbol_vector;
    return procedure; }

/*----------------------------------- PRM declarations ---------------------------------*/

BLN_type is_PRM(EXP_type Expression)
  { return member_of(Expression,
                     PRM_tag); }

PRM_type make_PRM_M(NBR_type Frame_vector_size_number,
                    EXP_type Expression,
                    ENV_type Environment,
                    VEC_type Symbol_vector)
  { PRM_type promise;
    promise = MAKE_CHUNK_M(PRM);
    promise->siz = Frame_vector_size_number;
    promise->exp = Expression;
    promise->env = Environment;
    promise->smv = Symbol_vector;
    return promise; }

/*----------------------------------- PRV declarations ---------------------------------*/

BLN_type is_PRV(EXP_type Expression)
  { return member_of(Expression,
                     PRV_tag); }

PRV_type make_PRV_M(SYM_type Name_symbol,
                    NBR_type Parameter_count_number,
                    NBR_type Frame_vector_size_number,
                    EXP_type Body_expression,
                    ENV_type Environment,
                    VEC_type Symbol_vector)
  { PRV_type procedure_vararg;
    procedure_vararg = MAKE_CHUNK_M(PRV);
    procedure_vararg->nam = Name_symbol;
    procedure_vararg->par = Parameter_count_number;
    procedure_vararg->siz = Frame_vector_size_number;
    procedure_vararg->bod = Body_expression;
    procedure_vararg->env = Environment;
    procedure_vararg->smv = Symbol_vector;
    return procedure_vararg; }

/*----------------------------------- QQU declarations ---------------------------------*/

BLN_type is_QQU(EXP_type Expression)
  { return member_of(Expression,
                     QQU_tag); }

QQU_type make_QQU_M(EXP_type Expression)
  { QQU_type quasiquote;
    quasiquote = MAKE_CHUNK_M(QQU);
    quasiquote->exp = Expression;
    return quasiquote; }

/*----------------------------------- REA declarations ---------------------------------*/

BLN_type is_REA(EXP_type Expression)
  { return member_of(Expression,
                     REA_tag); }

REA_type make_REA_M(RWR_type Rawreal)
  { REA_type real_value;
    real_value = MAKE_CHUNK_M(REA);
    real_value->rwr = Rawreal;
    return real_value; }

/*----------------------------------- SEQ declarations ---------------------------------*/

static const UNS_type SEQ_size = 0;

BLN_type is_SEQ(EXP_type Expression)
  { return member_of(Expression,
                     SEQ_tag); }

SEQ_type make_SEQ_Z(UNS_type Size)
  { SEQ_type sequence_expression;
    sequence_expression = (SEQ_type)MAKE_CHUNK_WITH_OFFSET_M(SEQ,
                                                             Size);
    return sequence_expression; }

UNS_type size_SEQ(SEQ_type Sequence)
  { return (UNS_type) Memory_Get_Size((PTR_type)Sequence); }

/*----------------------------------- STG declarations ---------------------------------*/

BLN_type is_STG(EXP_type Expression)
  { return member_of(Expression,
                     STG_tag); }

STG_type make_STG_M(NBR_type Scope_number,
                    NBR_type Offset_number,
                    EXP_type Expression)
  { STG_type set_global;
    set_global = MAKE_CHUNK_M(STG);
    set_global->scp = Scope_number;
    set_global->ofs = Offset_number;
    set_global->exp = Expression;
    return set_global; }

/*----------------------------------- STL declarations ---------------------------------*/

BLN_type is_STL(EXP_type Expression)
  { return member_of(Expression,
                     STL_tag); }

STL_type make_STL_M(NBR_type Offset_number,
                    EXP_type Expression)
  { STL_type set_local;
    set_local = MAKE_CHUNK_M(STL);
    set_local->ofs = Offset_number;
    set_local->exp = Expression;
    return set_local; }

/*----------------------------------- STR declarations ---------------------------------*/

BLN_type is_STR(EXP_type Expression)
  { return member_of(Expression,
                     STR_tag); }

STR_type make_STR_M(RWS_type Rawstring)
  { STR_type string;
    UNS_type size;
    size = string_to_expression_size(Rawstring);
    string = MAKE_CHUNK_WITH_OFFSET_M(STR,
                                      size);
    strcpy(string->rws,
           Rawstring);
    return string; }

UNS_type size_STR(RWS_type Rawstring)
  { return (UNS_type)string_to_expression_size(Rawstring); }

/*----------------------------------- SYM declarations ---------------------------------*/

BLN_type is_SYM(EXP_type Expression)
  { return member_of(Expression,
                     SYM_tag); }

SYM_type make_SYM_M(RWS_type Rawstring)
  { SYM_type symbol;
    UNS_type size;
    size = string_to_expression_size(Rawstring);
    symbol = MAKE_CHUNK_WITH_OFFSET_M(SYM,
                                      size);
    strcpy(symbol->rws,
           Rawstring);
    return symbol; }

UNS_type size_SYM(RWS_type Rawstring)
  { return (UNS_type)string_to_expression_size(Rawstring); }

/*----------------------------------- THK declarations ---------------------------------*/

BLN_type is_THK(EXP_type Expression)
  { return member_of(Expression,
                     THK_tag); }

THK_type make_THK_M(NBR_type Frame_vector_size_number,
                    EXP_type Body_expression,
                    VEC_type Symbol_vector)
  { THK_type thunk;
    thunk = MAKE_CHUNK_M(THK);
    thunk->siz = Frame_vector_size_number;
    thunk->bod = Body_expression;
    thunk->smv = Symbol_vector;
    return thunk; }

/*----------------------------------- TRU declarations ---------------------------------*/

BLN_type is_TRU(EXP_type Expression)
  { return (Expression == (EXP_type)Immediate_true_value); }

TRU_type make_TRU(NIL_type)
  { TRU_type true_;
    true_ = (TRU_type)Immediate_true_value;
    return true_; }

/*----------------------------------- UNQ declarations ---------------------------------*/

BLN_type is_UNQ(EXP_type Expression)
  { return member_of(Expression,
                     UNQ_tag); }

UNQ_type make_UNQ_M(EXP_type Expression)
  { UNQ_type unquote;
    unquote = MAKE_CHUNK_M(UNQ);
    unquote->exp = Expression;
    return unquote; }

/*----------------------------------- UQS declarations ---------------------------------*/

BLN_type is_UQS(EXP_type Expression)
  { return member_of(Expression,
                     UQS_tag); }

UQS_type make_UQS_M(EXP_type Expression)
  { UQS_type unquote_splicing;
    unquote_splicing = MAKE_CHUNK_M(UQS);
    unquote_splicing->exp = Expression;
    return unquote_splicing; }

/*----------------------------------- USP declarations ---------------------------------*/

BLN_type is_USP(EXP_type Expression)
  { return (Expression == (EXP_type)Immediate_unspecified_value); }

USP_type make_USP(NIL_type)
  { USP_type unspecified;
    unspecified = (USP_type)Immediate_unspecified_value;
    return unspecified; }

/*----------------------------------- UDF declarations ---------------------------------*/

BLN_type is_UDF(EXP_type Expression)
  { return (Expression == (EXP_type)Immediate_undefined_value); }

UDF_type make_UDF(NIL_type)
  { UDF_type undefined;
    undefined = (UDF_type)Immediate_undefined_value;
    return undefined; }

/*----------------------------------- VEC declarations ---------------------------------*/

static const UNS_type VEC_size = 0;

BLN_type is_VEC(EXP_type Expression)
  { return member_of(Expression,
                     VEC_tag); }

VEC_type make_VEC_M(UNS_type Size)
  { UNS_type index;
    VEC_type vector;
    if (Size > Memory_Maximum_size)
      Slip_Fail(VTL_error);
    vector = (VEC_type)MAKE_CHUNK_WITH_OFFSET_M(VEC,
                                                Size);
    for (index = 1;
         index <= Size;
         index += 1)
      vector[index] = Grammar_Undefined;
    return vector; }

VEC_type make_VEC_Z(UNS_type Size)
  { VEC_type vector;
    if (Size > Memory_Maximum_size)
      Slip_Fail(VTL_error);
    vector = (VEC_type)MAKE_CHUNK_WITH_OFFSET_M(VEC,
                                                Size);
    return vector; }

UNS_type size_VEC(VEC_type Vector)
  { UNS_type size;
    size = (UNS_type) Memory_Get_Size((PTR_type)Vector);
    return size; }

/*----------------------------------- WHI declarations ---------------------------------*/

BLN_type is_WHI(EXP_type Expression)
  { return member_of(Expression,
                     WHI_tag); }

WHI_type make_WHI_M(EXP_type Predicate_expression,
                    EXP_type Body_expression)
  { WHI_type while_;
    while_ = MAKE_CHUNK_M(WHI);
    while_->prd = Predicate_expression;
    while_->bod = Body_expression;
    return while_; }

/*--------------------------------------------------------------------------------------*/

NIL_type Grammar_Initialize(NIL_type)
  { if (_RG_tag > _RW_tag + 1)
      Slip_Fail(TGO_error);
    slipken_persist_init(Grammar_Empty_String   , make_STR_M(Empty_rawstring));
    slipken_persist_init(Grammar_Empty_Vector   , make_VEC_Z(0));
    slipken_persist_init(Grammar_False          , make_FLS());
    slipken_persist_init(Grammar_Newline_String , make_STR_M(Newline_rawstring));
    slipken_persist_init(Grammar_Null           , make_NUL());
    slipken_persist_init(Grammar_One_Number     , make_NBR(1));
    slipken_persist_init(Grammar_True           , make_TRU());
    slipken_persist_init(Grammar_Undefined      , make_UDF());
    slipken_persist_init(Grammar_Unspecified    , make_USP());
    slipken_persist_init(Grammar_Zero_Number    , make_NBR(0));
    initialize_Ken_ID_Pool_M(); }
