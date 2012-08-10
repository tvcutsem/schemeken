                     /*---------------------------------------------*/
                     /*                  >>>Slip<<<                 */
                     /*                 Theo D'Hondt                */
                     /*          VUB Software Languages Lab         */
                     /*                  (c) 2012                   */
                     /*---------------------------------------------*/
                     /*   version 13: completion and optimization   */
                     /*---------------------------------------------*/
                     /*                    pool                     */
                     /*---------------------------------------------*/

#include <string.h>

#include "SlipMain.h"

#include "SlipPool.h"
#include "SlipStack.h"
#include "SlipPersist.h"

/*------------------------------------ private macros ----------------------------------*/

#define NULL_SYMBOL (SYM_type)Grammar_Null

#define ENTER_KEYWORD_M(KEY)                                                             \
  Keyword_vector[Pool_##KEY] = Pool_Enter_Native_M(KEY##_rawstring)

/*----------------------------------- private constants --------------------------------*/

static const RWS_type And_rawstring              = "and";
static const RWS_type Begin_rawstring            = "begin";
static const RWS_type Cond_rawstring             = "cond";
static const RWS_type Define_rawstring           = "define";
static const RWS_type Delay_rawstring            = "delay";
static const RWS_type Else_rawstring             = "else";
static const RWS_type If_rawstring               = "if";
static const RWS_type Lambda_rawstring           = "lambda";
static const RWS_type Let_rawstring              = "let";
static const RWS_type Or_rawstring               = "or";
static const RWS_type Quasiquote_rawstring       = "quasiquote";
static const RWS_type Quote_rawstring            = "quote";
static const RWS_type Set_rawstring              = "set!";
static const RWS_type Unquote_rawstring          = "unquote";
static const RWS_type Unquote_Splicing_rawstring = "unquote-splicing";
static const RWS_type While_rawstring            = "while";

const static RWS_type Anonymous_rawstring        = "anonymous";
const static RWS_type Application_rawstring      = "application";

const static UNS_type Keyword_vector_size        = Pool_While - Pool_And + 1;

enum { Initial_pool_size   = 128,
       Pool_size_increment = 32 };

/*----------------------------------- private variables --------------------------------*/

static VEC_type Keyword_vector;
static VEC_type Pool_vector;
static UNS_type Top_of_pool;

/*----------------------------------- private functions --------------------------------*/

static SYM_type enter_symbol_M(RWS_type Rawstring)
  { SYM_type symbol;
    symbol = make_SYM_M(Rawstring);
    Pool_vector[Top_of_pool] = symbol;
    Top_of_pool += 1;
    return symbol; }

static SYM_type search_in_pool(RWS_type Rawstring)
  { RWS_type rawstring;
    SYM_type symbol;
    UNS_type index;
    for (index = 1;
         index < Top_of_pool;
         index += 1)
      { symbol = Pool_vector[index];
        rawstring = symbol->rws;
        if (strcmp(Rawstring,
                   rawstring) == 0)
          return symbol; }
    return NULL_SYMBOL; }

static NIL_type extend_pool_if_necessary_C(NIL_type)
  { UNS_type size;
    size = size_VEC(Pool_vector);
    if (Top_of_pool == size)
      { Slip_Log("extend pool");
        size += Pool_size_increment;
        MAIN_CLAIM_DYNAMIC_C(size);
        Main_Reallocate_Vector_M(&Pool_vector,
                                 Pool_size_increment); }}

/*----------------------------------- public variables ---------------------------------*/

SYM_type Pool_Anonymous_Symbol;

/*----------------------------------- public functions ---------------------------------*/

SYM_type Pool_Enter_Native_M(RWS_type Rawstring)
  { SYM_type symbol;
    symbol = enter_symbol_M(Rawstring);
    return symbol; }

SYM_type Pool_Enter_Rawstring_C(RWS_type Rawstring)
  { SYM_type symbol;
    UNS_type size;
    symbol = search_in_pool(Rawstring);
    if (!is_NUL(symbol))
      return symbol;
    extend_pool_if_necessary_C();
    size = size_SYM(Rawstring);
    MAIN_CLAIM_DYNAMIC_C(size);
    symbol = enter_symbol_M(Rawstring);
    return symbol; }

SYM_type Pool_Enter_String_C(STR_type String)
  { RWS_type rawstring;
    SYM_type symbol;
    UNS_type size;
    rawstring = String->rws;
    size = size_SYM(rawstring);
    symbol = search_in_pool(rawstring);
    if (!is_NUL(symbol))
      return symbol;
    BEGIN_PROTECT(String)
      extend_pool_if_necessary_C();
      MAIN_CLAIM_DYNAMIC_C(size);
    END_PROTECT(String)
    rawstring = String->rws;
    symbol = make_SYM_M(rawstring);
    Pool_vector[Top_of_pool] = symbol;
    Top_of_pool += 1;
    return symbol; }

SYM_type Pool_Keyword_Symbol(KEY_type Key)
  { SYM_type symbol;
    symbol = Keyword_vector[Key];
    return symbol; }

BLN_type Pool_Validate_Symbol(SYM_type Symbol)
  { UNS_type keyword_vector_index;
    for (keyword_vector_index = 1;
         keyword_vector_index <= Keyword_vector_size;
         keyword_vector_index += 1)
      if (Symbol == Keyword_vector[keyword_vector_index])
         return Grammar_Is_False;
    return Grammar_Is_True; }

/*--------------------------------------------------------------------------------------*/

NIL_type Pool_Initialize_M(NIL_type)
  { slipken_persist_init(Top_of_pool,  1);
    slipken_persist_init(Keyword_vector, make_VEC_M(Keyword_vector_size));
    slipken_persist_init(Pool_vector,  make_VEC_M(Initial_pool_size));
    ENTER_KEYWORD_M(And);
    ENTER_KEYWORD_M(Begin);
    ENTER_KEYWORD_M(Cond);
    ENTER_KEYWORD_M(Define);
    ENTER_KEYWORD_M(Delay);
    ENTER_KEYWORD_M(Else);
    ENTER_KEYWORD_M(If);
    ENTER_KEYWORD_M(Lambda);
    ENTER_KEYWORD_M(Let);
    ENTER_KEYWORD_M(Or);
    ENTER_KEYWORD_M(Quasiquote);
    ENTER_KEYWORD_M(Quote);
    ENTER_KEYWORD_M(Set);
    ENTER_KEYWORD_M(Unquote);
    ENTER_KEYWORD_M(Unquote_Splicing);
    ENTER_KEYWORD_M(While);
    slipken_persist_init(Pool_Anonymous_Symbol, Pool_Enter_Native_M(Anonymous_rawstring));
    MAIN_REGISTER(Keyword_vector);
    MAIN_REGISTER(Pool_vector);
    MAIN_REGISTER(Pool_Anonymous_Symbol); }
