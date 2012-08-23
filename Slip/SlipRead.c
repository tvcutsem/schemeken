                     /*---------------------------------------------*/
                     /*                  >>>Slip<<<                 */
                     /*                 Theo D'Hondt                */
                     /*          VUB Software Languages Lab         */
                     /*                  (c) 2012                   */
                     /*---------------------------------------------*/
                     /*   version 13: completion and optimization   */
                     /*---------------------------------------------*/
                     /*                   reader                    */
                     /*---------------------------------------------*/

#include <stdlib.h>
#include <string.h>

#include "SlipMain.h"

#include "SlipPool.h"
#include "SlipRead.h"
#include "SlipScan.h"
#include "SlipStack.h"

/*----------------------------------- private variables --------------------------------*/

static SCA_type Token;

/*----------------------------------- private prototypes -------------------------------*/

static NIL_type read_expression_C(NIL_type);

/*----------------------------------- private functions --------------------------------*/

static NIL_type next_token(NIL_type)
  { Token = Scan_Next(); }

static NIL_type next_token_and_push_C(EXP_type Expression)
  { next_token();
    Stack_Push_C(Expression); }

static NIL_type read_error(BYT_type Error)
  { Main_Error_Handler(Error,
                       Main_Void); }

static NIL_type read_rawstring_error(BYT_type Error,
                                     RWS_type Rawstring)
  {  Main_Error_Handler(Error,
                        Rawstring); }
                        
static NIL_type quote_pattern_C(KEY_type Key)
  { EXP_type expression;
    PAI_type pair;
    SYM_type symbol;
    next_token();
    read_expression_C();
    MAIN_CLAIM_DEFAULT_C();
    expression = Stack_Peek();
    symbol = Pool_Keyword_Symbol(Key);
    pair = make_PAI_M(expression,
                      Grammar_Null);
    pair = make_PAI_M(symbol,
                      pair);
    Stack_Poke(pair); }

static BLN_type looks_like_ken_id(RWS_type Rawstring)
  { UNS_type size;
    UNS_type r;
    UNS_type ip[4], port;

    size = strlen(Rawstring);
    if (KEN_ID_BUF_SIZE < size || 8 > size)
      return Grammar_Is_False;
    r = sscanf(Rawstring, "%d.%d.%d.%d:%d", &ip[0], &ip[1], &ip[2], &ip[3], &port);
    if (r != 5)
      return Grammar_Is_False;
    if (   (   0 <= ip[0] &&        256 >  ip[0])
        && (   0 <= ip[1] &&        256 >  ip[1])
        && (   0 <= ip[2] &&        256 >  ip[2])
        && (   0 <= ip[3] &&        256 >  ip[3])
        && (1024 <= port  && UINT16_MAX >= port))
        return Grammar_Is_True;
    return Grammar_Is_False;
  }
/*--------------------------------------------------------------------------------------*/

static NIL_type read_character_C(NIL_type)
  { CHA_type character;
    RWC_type rawcharacter;
    rawcharacter = Scan_String[0];
    character = make_CHA(rawcharacter);
    next_token_and_push_C(character); }

static NIL_type read_end_of_line(NIL_type)
  { Token = Scan_Preset();
    read_expression_C(); }

static NIL_type read_false_C(NIL_type)
  { next_token_and_push_C(Grammar_False); }

static NIL_type read_list_C(NIL_type)
  { EXP_type expression;
    PAI_type pair;
    UNS_type count;
    next_token();
    for (count = 0;
         ;
         count += 1)
      if (Token == RPR_token)
        { Stack_Push_C(Grammar_Null);
          break; }
      else
        { read_expression_C();
          if (Token == PER_token)
            { next_token();
              read_expression_C();
              if (Token != RPR_token)
                read_error(MRP_error);
              count += 1;
              break; }}
    next_token();
    for (;
         count > 0;
         count-= 1)
      { MAIN_CLAIM_DEFAULT_C();
        pair = Stack_Pop();
        expression = Stack_Peek();
        pair = make_PAI_M(expression,
                          pair);
        Stack_Poke(pair); }}

static NIL_type read_number_C(NIL_type)
  { RWL_type rawlongnumber;
    NBR_type number;
    rawlongnumber = atoll(Scan_String);
    if (llabs(rawlongnumber) > Main_Maximum_Number)
      read_rawstring_error(NTL_error,
                           Scan_String);
    number = make_NBR(rawlongnumber);
    next_token_and_push_C(number); }

static NIL_type read_parenthesis(NIL_type)
  { read_error(IRP_error); }

static NIL_type read_period(NIL_type)
  { read_error(IUP_error); }

static NIL_type read_quasiquote_C(NIL_type)
  { quote_pattern_C(Pool_Quasiquote); }

static NIL_type read_quote_C(NIL_type)
  { EXP_type expression;
    PAI_type pair;
    SYM_type quote_symbol;
    next_token();
    read_expression_C();
    MAIN_CLAIM_DEFAULT_C();
    expression = Stack_Peek();
    quote_symbol = Pool_Keyword_Symbol(Pool_Quote);
    pair = make_PAI_M(expression,
                      Grammar_Null);
    pair = make_PAI_M(quote_symbol,
                      pair);
    Stack_Poke(pair); }
    
static NIL_type read_real_C(NIL_type)
  { RWR_type rawreal;
    REA_type real;
    rawreal = atof(Scan_String);
    MAIN_CLAIM_DEFAULT_C();
    real = make_REA_M(rawreal);
    next_token_and_push_C(real); }

static NIL_type read_string_C(NIL_type)
  { STR_type string;
    UNS_type size;
    size = size_STR(Scan_String);
    MAIN_CLAIM_DYNAMIC_C(size);
    string = make_STR_M(Scan_String);
    next_token_and_push_C(string); }

static NIL_type read_symbol_C(NIL_type)
  { SYM_type symbol;
    symbol = Pool_Enter_Rawstring_C(Scan_String);
    next_token_and_push_C(symbol); }

static NIL_type read_true_C(NIL_type)
  { next_token_and_push_C(Grammar_True); }

static NIL_type read_unquote_C(NIL_type)
  { quote_pattern_C(Pool_Unquote); }

static NIL_type read_unquote_splicing_C(NIL_type)
  { quote_pattern_C(Pool_Unquote_Splicing); }

static NIL_type read_vector_C(SCA_type End_token)
  { UNS_type count;
    next_token();
    for (count = 0;
         Token != End_token;
         count += 1)
      read_expression_C();
    Stack_Collapse_C(count);
    next_token(); }

static NIL_type read_ken_id_C(NIL_type)
  { RWK_type ken_id;
    KID_type kid;
    if (!looks_like_ken_id(Scan_String))
      read_rawstring_error(NAK_error,
                           Scan_String);
    ken_id = ken_id_from_string(Scan_String);
    MAIN_CLAIM_DYNAMIC_C(sizeof(RWK_type));
    kid = make_KID_M(ken_id);
    next_token_and_push_C(kid); }

static NIL_type read_expression_C(NIL_type)
  { switch (Token)
      { case CHA_token:
          read_character_C();
          return;
        case EOL_token:
          read_end_of_line();
          return;
        case FLS_token:
          read_false_C();
          return;
        case IDT_token:
          read_symbol_C();
          return;
        case KID_token:
          read_ken_id_C();
          return;
        case LPR_token:
          read_list_C();
          return;
        case NBR_token:
          read_number_C();
          return;
        case PER_token:
          read_period();
          return;
        case QQU_token:
          read_quasiquote_C();
          return;
        case QUO_token:
          read_quote_C();
          return;
        case RPR_token:
          read_parenthesis();
          return;
        case REA_token:
          read_real_C();
          return;
        case STR_token:
          read_string_C();
          return;
        case TRU_token:
          read_true_C();
          return;
        case UNQ_token:
          read_unquote_C();
          return;
        case UQS_token:
          read_unquote_splicing_C();
          return;
        case VEC_token:
          read_vector_C(RPR_token);
          return;
        default:
          read_error(IXT_error); }}

/*--------------------------------- implemented callbacks ------------------------------*/

NIL_type Scan_Error(BYT_type Error)
  { read_error(Error); }

/*----------------------------------- public functions ---------------------------------*/

EXP_type Read_Parse_C(RWS_type Input_rawstring)
  { BYT_type result;
    EXP_type expression;
    /*result = Slip_Open(Input_rawstring);
    if (result == 0)
      read_rawstring_error(INF_error,
                           Input_rawstring); */
    Token = Scan_Preset();
    read_expression_C();
    result = Slip_Close();
    if (result == 0)
      read_error(XCT_error);
    expression = Stack_Pop();
    return expression; }

/*--------------------------------------------------------------------------------------*/

NIL_type Read_Initialize(NIL_type)
  { Scan_Initialize(); }
