                     /*---------------------------------------------*/
                     /*                  >>>Slip<<<                 */
                     /*                 Theo D'Hondt                */
                     /*          VUB Software Languages Lab         */
                     /*                  (c) 2012                   */
                     /*---------------------------------------------*/
                     /*   version 13: completion and optimization   */
                     /*---------------------------------------------*/
                     /*                   natives                   */
                     /*---------------------------------------------*/

#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "SlipMain.h"

#include "SlipCompile.h"
#include "SlipEnvironment.h"
#include "SlipEvaluate.h"
#include "SlipNative.h"
#include "SlipPool.h"
#include "SlipPrint.h"
#include "SlipRead.h"
#include "SlipStack.h"
#include "SlipThread.h"
#include "SlipPersist.h"

/* Scheme compatibility

STANDARD
YES -
YES *
YES /
YES +
YES <  → also char<?  and string<?
YES <= → also char<=? and string<=?
YES =  → also char=?  and string=?
YES >  → also char>?  and string>?
YES >= → also char>=? and string>=?
NO  abs
NO  acos
NO  angle
NO  append
YES apply → single argument list only
NO  asin
YES assoc
NO  assq
NO  assv
NO  atan
YES boolean?
NO  caaaar
NO  caaadr
NO  caaar
NO  caadar
NO  caaddr
NO  caadr
NO  caar
NO  cadaar
NO  cadadr
NO  cadar
NO  caddar
NO  cadddr
NO  caddr
NO  cadr
YES call-with-current-continuation
NO  call-with-input-file
NO  call-with-output-file
YES car
NO  cdaaar
NO  cdaadr
NO  cdaar
NO  cdadar
NO  cdaddr
NO  cdadr
NO  cdar
NO  cddaar
NO  cddadr
NO  cddar
NO  cdddar
NO  cddddr
NO  cdddr
NO  cddr
YES cdr
NO  ceiling
YES char->integer
NO  char-alphabetic?
NO  char-ci<?
NO  char-ci<=?
NO  char-ci=?
NO  char-ci>?
NO  char-ci>=?
NO  char-downcase
NO  char-lower-case?
NO  char-numeric?
NO  char-ready?
NO  char-upcase
NO  char-upper-case?
NO  char-whitespace?
YES char?
NO  char<?
NO  char<=?
NO  char=?
NO  char>?
NO  char>=?
NO  close-input-port
NO  close-output-port
NO  complex?
YES cons
YES cos
NO  current-input-port
NO  current-output-port
NO  denominator
YES display
NO  dynamic-wind
NO  eof-object?
YES eq?
NO  equal?
NO  eqv?
YES eval → no environment argument
NO  even?
NO  exact->inexact
NO  exact?
YES exp
YES expt
NO  floor
YES for-each → single argument operator only
YES force
NO  gcd
NO  imag-part
NO  inexact->exact
NO  inexact?
NO  input-port?
NO  integer->char
YES integer?
NO  interaction-environment
NO  lcm
YES length
YES list
NO  list->string
NO  list->vector
NO  list-ref
NO  list-tail
NO  list?
NO  load
YES log
NO  magnitude
NO  make-polar
NO  make-rectangular
NO  make-string
YES make-vector
YES map → single argument operator only
NO  max
NO  member
NO  memq
NO  memv
NO  min
NO  modulo
NO  negative?
YES newline
YES not
NO  null-environment
YES null?
NO  number->string
YES number?
NO  numerator
NO  odd?
NO  open-input-file
NO  open-output-file
NO  output-port?
YES pair?
NO  peek-char
NO  positive?
YES procedure?
YES quotient
NO  rational?
NO  rationalize
YES read
NO  read-char
NO  real-part
YES real?
YES remainder
NO  reverse
NO  round
NO  scheme-report-environment
YES set-car!
YES set-cdr!
YES sin
YES sqrt
NO  string
NO  string->list
NO  string->number
YES string->symbol
NO  string-append
NO  string-ci<?
NO  string-ci<=?
NO  string-ci=?
NO  string-ci>?
NO  string-ci>=?
NO  string-copy
NO  string-fill!
YES string-length
YES string-ref
YES string-set!
YES string?
NO  string<?
NO  string<=?
NO  string=?
NO  string>?
NO  string>=?
NO  substring
YES symbol->string
YES symbol?
YES tan
NO  transcript-off
NO  transcript-on
NO  truncate
NO  values
YES vector
NO  vector->list
NO  vector-fill!
YES vector-length
YES vector-ref
YES vector-set!
YES vector?
NO  with-input-from-file
NO  with-output-to-file
NO  write
NO  write-char
NO  zero?

NON-STANDARD
YES circularity-level
YES clock
YES collect
YES error
YES pretty
YES random

*/

/*------------------------------------ private functions -------------------------------*/

static NIL_type native_error(BYT_type Error,
                             RWS_type Rawstring)
  { Main_Error_Handler(Error,
                       Rawstring); }

/*--------------------------------------------------------------------------------------*/

static NIL_type enter_native_name(RWS_type Rawstring,
                                  UNS_type Offset,
                                  EXP_type value_expression)
  { Environment_Global_Frame_Set(value_expression,
                                 Offset); }

static NBR_type make_number_C(RWL_type Rawlongnumber,
                              RWS_type Rawstring)
  { NBR_type number;
    if (llabs(Rawlongnumber) > Main_Maximum_Number)
      native_error(NTL_error,
                   Rawstring);
    number = make_NBR(Rawlongnumber);
    return number; }

static REA_type make_real_C(RWR_type rawreal,
                            RWS_type Rawstring)
  { REA_type real;
    MAIN_CLAIM_DEFAULT_C();
    real = make_REA_M(rawreal);
    return real; }

static NIL_type native_define_M(RWS_type Rawstring,
                                RNF_type Raw_native_function)
  { NAT_type native;
    SYM_type symbol;
    UNS_type offset;
    symbol = Pool_Enter_Native_M(Rawstring);
    native = make_NAT_M(Raw_native_function,
                        Rawstring);
    offset = Compile_Define_Global_Symbol_M(symbol);
    enter_native_name(Rawstring,
                      offset,
                      native); }

static NIL_type require_0_arguments(FRM_type Argument_frame,
                                    RWS_type Message_rawstring)
  { UNS_type number_of_arguments;
    number_of_arguments = Environment_Frame_Size(Argument_frame);
    if (number_of_arguments != 0)
      native_error(EX0_error,
                   Message_rawstring); }


static NIL_type require_1_argument(FRM_type   Argument_frame,
                                   EXP_type * Reference,
                                   RWS_type   Message_rawstring)
  { UNS_type number_of_arguments;
    number_of_arguments = Environment_Frame_Size(Argument_frame);
    if (number_of_arguments != 1)
      native_error(EX1_error,
                   Message_rawstring);
    *Reference = Environment_Frame_Get(Argument_frame,
                                       1);
    Environment_Release_Frame(Argument_frame); }

static NIL_type require_2_arguments(FRM_type   Argument_frame,
                                    EXP_type * Left_reference,
                                    EXP_type * Right_reference,
                                    RWS_type   Message_rawstring)
  { UNS_type number_of_arguments;
    number_of_arguments = Environment_Frame_Size(Argument_frame);
    if (number_of_arguments != 2)
      native_error(EX2_error,
                   Message_rawstring);
    *Left_reference  = Environment_Frame_Get(Argument_frame,
                                             1);
    *Right_reference = Environment_Frame_Get(Argument_frame,
                                             2);
    Environment_Release_Frame(Argument_frame); }

static NIL_type require_3_arguments(FRM_type   Argument_frame,
                                    EXP_type * First_reference,
                                    EXP_type * Second_reference,
                                    EXP_type * Third_reference,
                                    RWS_type   Message_rawstring)
  { UNS_type number_of_arguments;
    number_of_arguments = Environment_Frame_Size(Argument_frame);
    if (number_of_arguments != 3)
      native_error(EX3_error,
                   Message_rawstring);
    *First_reference  = Environment_Frame_Get(Argument_frame,
                                              1);
    *Second_reference = Environment_Frame_Get(Argument_frame,
                                              2);
    *Third_reference  = Environment_Frame_Get(Argument_frame,
                                              3);
    Environment_Release_Frame(Argument_frame); }

/*--------------------------------------------------------------------------------------*/
/*-------------------------------------- variables -------------------------------------*/
/*--------------------------------------------------------------------------------------*/

/*--------------------------------- meta-circularity-level -----------------------------*/

static const RWS_type mcl_rawstring = "circularity-level";

static NIL_type initialize_circularity_level_M(NIL_type)
  { SYM_type symbol;
    UNS_type offset;
    symbol = Pool_Enter_Native_M(mcl_rawstring);
    offset = Compile_Define_Global_Symbol_M(symbol);
    enter_native_name(mcl_rawstring,
                      offset,
                      Grammar_Zero_Number); }

/*--------------------------------------------------------------------------------------*/
/*------------------------------------- arithmetic -------------------------------------*/
/*--------------------------------------------------------------------------------------*/

/*----------------------------------------- + ------------------------------------------*/

static const RWS_type add_rawstring = "+";

static EXP_type add_reals_C(RWR_type Rawreal,
                            FRM_type Argument_frame,
                            UNS_type Index)
  { EXP_type value_expression;
    REA_type real;
    RWN_type rawnumber;
    RWR_type accumulated_rawreal,
             rawreal;
    TAG_type tag;
    UNS_type index,
             number_of_arguments;
    accumulated_rawreal = Rawreal;
    number_of_arguments = Environment_Frame_Size(Argument_frame);
    for (index = Index;
         index <= number_of_arguments;
         index += 1)
      { value_expression = Environment_Frame_Get(Argument_frame,
                                                 index);
        tag = Grammar_Tag(value_expression);
        switch (tag)
          { case NBR_tag:
              rawnumber = get_NBR(value_expression);
              accumulated_rawreal += rawnumber;
              break;
            case REA_tag:
              real = value_expression;
              rawreal = real->rwr;
              accumulated_rawreal += rawreal;
              break;
            default:
              native_error(ARN_error,
                           add_rawstring); }}
    real = make_real_C(accumulated_rawreal,
                       add_rawstring);
    return real; }

static EXP_type add_numbers_C(NBR_type Number,
                              FRM_type Argument_frame)
  { EXP_type value_expression;
    NBR_type number;
    REA_type real;
    RWL_type accumulated_rawlongnumber;
    RWN_type rawnumber;
    RWR_type accumulated_rawreal,
             rawreal;
    TAG_type tag;
    UNS_type index,
             number_of_arguments;
    accumulated_rawlongnumber = get_NBR(Number);
    number_of_arguments = Environment_Frame_Size(Argument_frame);
    for (index = 2;
         index <= number_of_arguments;
         index += 1)
      { value_expression = Environment_Frame_Get(Argument_frame,
                                                 index);
        tag = Grammar_Tag(value_expression);
        switch (tag)
          { case NBR_tag:
              rawnumber = get_NBR(value_expression);
              accumulated_rawlongnumber += rawnumber;
              break;
            case REA_tag:
              real = value_expression;
              rawreal = real->rwr;
              accumulated_rawreal = accumulated_rawlongnumber + rawreal;
              real = add_reals_C(accumulated_rawreal,
                                 Argument_frame,
                                 index + 1);
              return real;
            default:
              native_error(ARN_error,
                           add_rawstring); }}
    number = make_number_C(accumulated_rawlongnumber,
                           add_rawstring);
    return number; }

static NIL_type add_native_C(FRM_type Argument_frame,
                             EXP_type Tail_call_expression)
  { EXP_type value_expression;
    REA_type real;
    RWR_type rawreal;
    TAG_type tag;
    UNS_type number_of_arguments;
    number_of_arguments = Environment_Frame_Size(Argument_frame);
    if (number_of_arguments == 0)
      value_expression = Grammar_Zero_Number;
    else
      { value_expression = Environment_Frame_Get(Argument_frame,
                                                 1);
        tag = Grammar_Tag(value_expression);
        switch (tag)
          { case NBR_tag:
              if (number_of_arguments > 1)
                value_expression = add_numbers_C(value_expression,
                                                 Argument_frame);
              break;
            case REA_tag:
              if (number_of_arguments > 1)
                { real = value_expression;
                  rawreal = real->rwr;
                  value_expression = add_reals_C(rawreal,
                                                 Argument_frame,
                                                 2); }
              break;
           default:
             native_error(ARN_error,
                          add_rawstring); }
            Environment_Release_Frame(Argument_frame); }
    Main_Set_Expression(value_expression); }

/*--------------------------------------------------------------------------------------*/

static NIL_type initialize_add_native_M(NIL_type)
  { native_define_M(add_rawstring,
                    add_native_C); }

/*----------------------------------------- - ------------------------------------------*/

static const RWS_type sub_rawstring = "-";

static EXP_type subtract_reals_C(RWR_type Rawreal,
                                 FRM_type Argument_frame,
                                 UNS_type Index)
  { EXP_type value_expression;
    REA_type real;
    RWN_type rawnumber;
    RWR_type accumulated_rawreal,
             rawreal;
    TAG_type tag;
    UNS_type index,
             number_of_arguments;
    accumulated_rawreal = Rawreal;
    number_of_arguments = Environment_Frame_Size(Argument_frame);
    for (index = Index;
         index <= number_of_arguments;
         index += 1)
      { value_expression = Environment_Frame_Get(Argument_frame,
                                                 index);
        tag = Grammar_Tag(value_expression);
        switch (tag)
          { case NBR_tag:
              rawnumber = get_NBR(value_expression);
              accumulated_rawreal -= rawnumber;
              break;
            case REA_tag:
              real = value_expression;
              rawreal = real->rwr;
              accumulated_rawreal -= rawreal;
              break;
            default:
              native_error(ARN_error,
                           sub_rawstring); }}
    real = make_real_C(accumulated_rawreal,
                       sub_rawstring);
    return real; }

static EXP_type subtract_numbers_C(NBR_type Number,
                                   FRM_type Argument_frame)
  { EXP_type value_expression;
    NBR_type number;
    REA_type real;
    RWL_type accumulated_rawlongnumber;
    RWN_type rawnumber;
    RWR_type accumulated_rawreal,
             rawreal;
    TAG_type tag;
    UNS_type index,
             number_of_arguments;
    accumulated_rawlongnumber = get_NBR(Number);
    number_of_arguments = Environment_Frame_Size(Argument_frame);
    for (index = 2;
         index <= number_of_arguments;
         index += 1)
      { value_expression = Environment_Frame_Get(Argument_frame,
                                                 index);
        tag = Grammar_Tag(value_expression);
        switch (tag)
          { case NBR_tag:
              rawnumber = get_NBR(value_expression);
              accumulated_rawlongnumber -= rawnumber;
              break;
            case REA_tag:
              real = value_expression;
              rawreal = real->rwr;
              accumulated_rawreal = accumulated_rawlongnumber - rawreal;
              real = subtract_reals_C(accumulated_rawreal,
                                      Argument_frame,
                                      index + 1);
              return real;
            default:
              native_error(ARN_error,
                           sub_rawstring); }}
    number = make_number_C(accumulated_rawlongnumber,
                           sub_rawstring);
    return number; }

static NIL_type subtract_native_C(FRM_type Argument_frame,
                                  EXP_type Tail_call_expression)
  { EXP_type value_expression;
    REA_type real;
    RWN_type rawnumber;
    RWR_type rawreal;
    TAG_type tag;
    UNS_type number_of_arguments;
    number_of_arguments = Environment_Frame_Size(Argument_frame);
    if (number_of_arguments == 0)
      native_error(AL1_error,
                   sub_rawstring);
    value_expression = Environment_Frame_Get(Argument_frame,
                                             1);
    tag = Grammar_Tag(value_expression);
    switch (tag)
      { case NBR_tag:
          if (number_of_arguments > 1)
            value_expression = subtract_numbers_C(value_expression,
                                                  Argument_frame);
          else
            { rawnumber = get_NBR(value_expression);
              rawnumber = - rawnumber;
              value_expression = make_NBR(rawnumber); }
          break;
        case REA_tag:
          real = value_expression;
          rawreal = real->rwr;
          if (number_of_arguments > 1)
            value_expression = subtract_reals_C(rawreal,
                                                Argument_frame,
                                                2);
          else
            { rawreal = - rawreal;
              value_expression = make_real_C(rawreal,
                                             sub_rawstring); }
          break;
       default:
         native_error(ARN_error,
                      sub_rawstring); }
    Environment_Release_Frame(Argument_frame);
    Main_Set_Expression(value_expression); }

/*--------------------------------------------------------------------------------------*/

static NIL_type initialize_subtract_native_M(NIL_type)
  { native_define_M(sub_rawstring,
                    subtract_native_C); }

/*----------------------------------------- * ------------------------------------------*/

static const RWS_type mul_rawstring = "*";

static EXP_type multiply_reals_C(RWR_type Rawreal,
                                 FRM_type Argument_frame,
                                 UNS_type Index)
  { EXP_type value_expression;
    REA_type real;
    RWN_type rawnumber;
    RWR_type accumulated_rawreal,
             rawreal;
    TAG_type tag;
    UNS_type index,
             number_of_arguments;
    accumulated_rawreal = Rawreal;
    number_of_arguments = Environment_Frame_Size(Argument_frame);
    for (index = Index;
         index <= number_of_arguments;
         index += 1)
      { value_expression = Environment_Frame_Get(Argument_frame,
                                                 index);
        tag = Grammar_Tag(value_expression);
        switch (tag)
          { case NBR_tag:
              rawnumber = get_NBR(value_expression);
              accumulated_rawreal *= rawnumber;
              break;
            case REA_tag:
              real = value_expression;
              rawreal = real->rwr;
              accumulated_rawreal *= rawreal;
              break;
            default:
              native_error(ARN_error,
                           mul_rawstring); }}
    real = make_real_C(accumulated_rawreal,
                       mul_rawstring);
    return real; }

static EXP_type multiply_numbers_C(NBR_type Number,
                                   FRM_type Argument_frame)
  { EXP_type value_expression;
    NBR_type number;
    REA_type real;
    RWL_type accumulated_rawlongnumber;
    RWN_type rawnumber;
    RWR_type accumulated_rawreal,
             rawreal;
    TAG_type tag;
    UNS_type index,
             number_of_arguments;
    accumulated_rawlongnumber = get_NBR(Number);
    number_of_arguments = Environment_Frame_Size(Argument_frame);
    for (index = 2;
         index <= number_of_arguments;
         index += 1)
      { value_expression  = Environment_Frame_Get(Argument_frame,
                                                  index);
        tag = Grammar_Tag(value_expression);
        switch (tag)
          { case NBR_tag:
              rawnumber = get_NBR(value_expression);
              accumulated_rawlongnumber *= rawnumber;
              break;
            case REA_tag:
              real = value_expression;
              rawreal = real->rwr;
              accumulated_rawreal = accumulated_rawlongnumber * rawreal;
              real = multiply_reals_C(accumulated_rawreal,
                                      Argument_frame,
                                      index + 1);
              return real;
            default:
              native_error(ARN_error,
                           mul_rawstring); }}
    number = make_number_C(accumulated_rawlongnumber,
                           mul_rawstring);
    return number; }

static NIL_type multiply_native_C(FRM_type Argument_frame,
                                  EXP_type Tail_call_expression)
  { EXP_type value_expression;
    REA_type real;
    RWR_type rawreal;
    TAG_type tag;
    UNS_type number_of_arguments;
    number_of_arguments = Environment_Frame_Size(Argument_frame);
    if (number_of_arguments == 0)
      value_expression = Grammar_One_Number;
    else
      { value_expression = Environment_Frame_Get(Argument_frame,
                                                 1);
        tag = Grammar_Tag(value_expression);
        switch (tag)
          { case NBR_tag:
              if (number_of_arguments > 1)
                value_expression = multiply_numbers_C(value_expression,
                                                      Argument_frame);
              break;
            case REA_tag:
              if (number_of_arguments > 1)
                { real = value_expression;
                  rawreal = real->rwr;
                  value_expression = multiply_reals_C(rawreal,
                                                      Argument_frame,
                                                      2); }
              break;
            default:
              native_error(ARN_error,
                           mul_rawstring); }
            Environment_Release_Frame(Argument_frame); }
    Main_Set_Expression(value_expression); }

/*--------------------------------------------------------------------------------------*/

static NIL_type initialize_multiply_native_M(NIL_type)
  { native_define_M(mul_rawstring,
                    multiply_native_C); }

/*----------------------------------------- / ------------------------------------------*/

static const RWS_type div_rawstring = "/";

static EXP_type divide_reals_C(RWR_type Rawreal,
                               FRM_type Argument_frame)
  { EXP_type value_expression;
    REA_type real;
    RWN_type rawnumber;
    RWR_type accumulated_rawreal,
             rawreal;
    TAG_type tag;
    UNS_type index,
             number_of_arguments;
    accumulated_rawreal = Rawreal;
    number_of_arguments = Environment_Frame_Size(Argument_frame);
    for (index = 2;
         index <= number_of_arguments;
         index += 1)
      { value_expression = Environment_Frame_Get(Argument_frame,
                                                 index);
        tag = Grammar_Tag(value_expression);
        switch (tag)
          { case NBR_tag:
              rawnumber = get_NBR(value_expression);
              if (rawnumber == 0)
                native_error(DBZ_error,
                             div_rawstring);
              accumulated_rawreal /= rawnumber;
              break;
            case REA_tag:
              real = value_expression;
              rawreal = real->rwr;
              if (rawreal == 0)
                native_error(DBZ_error,
                             div_rawstring);
              accumulated_rawreal /= rawreal;
              break;
            default:
              native_error(ARN_error,
                           div_rawstring); }}
    real = make_real_C(accumulated_rawreal,
                       div_rawstring);
    return real; }

static NIL_type divide_native_C(FRM_type Argument_frame,
                                EXP_type Tail_call_expression)
  { EXP_type value_expression;
    REA_type real;
    RWN_type rawnumber;
    RWR_type rawreal;
    TAG_type tag;
    UNS_type number_of_arguments;
    number_of_arguments = Environment_Frame_Size(Argument_frame);
    if (number_of_arguments == 0)
      native_error(AL1_error,
                   div_rawstring);
    value_expression = Environment_Frame_Get(Argument_frame,
                                             1);
    tag = Grammar_Tag(value_expression);
    switch (tag)
      { case NBR_tag:
          rawnumber = get_NBR(value_expression);
          if (number_of_arguments > 1)
            value_expression = divide_reals_C(rawnumber,
                                              Argument_frame);
          else if (rawnumber == 0)
            native_error(DBZ_error,
                         div_rawstring);
          else
            { rawreal = 1.0 / rawnumber;
              value_expression = make_real_C(rawreal,
                                             div_rawstring); }
          break;
        case REA_tag:
          real = value_expression;
          rawreal = real->rwr;
          if (number_of_arguments > 1)
            value_expression = divide_reals_C(rawreal,
                                              Argument_frame);
          else if (rawreal == 0)
            native_error(DBZ_error,
                         div_rawstring);
          else
            { rawreal = 1.0 / rawreal;
              value_expression = make_real_C(rawreal,
                                             div_rawstring); }
          break;
        default:
          native_error(ARN_error,
                       div_rawstring); }
    Environment_Release_Frame(Argument_frame);
    Main_Set_Expression(value_expression); }

/*--------------------------------------------------------------------------------------*/

static NIL_type initialize_divide_native_M(NIL_type)
  { native_define_M(div_rawstring,
                    divide_native_C); }

/*-------------------------------------- quotient --------------------------------------*/

static const RWS_type quo_rawstring = "quotient";

static NIL_type quotient_native(FRM_type Argument_frame,
                                EXP_type Tail_call_expression)
  { RWN_type left_rawnumber,
             rawnumber,
             right_rawnumber;
    NBR_type left_number,
             number,
             right_number;
    require_2_arguments(Argument_frame,
                        &left_number,
                        &right_number,
                        quo_rawstring);
    if (!is_NBR(left_number) || !is_NBR(right_number))
      native_error(ARN_error,
                   quo_rawstring);
    left_rawnumber = get_NBR(left_number);
    right_rawnumber = get_NBR(right_number);
    if (right_rawnumber == 0)
      native_error(DBZ_error,
                   quo_rawstring);
    else
      { rawnumber = left_rawnumber / right_rawnumber;
        number = make_NBR(rawnumber);
        Main_Set_Expression(number); }}

/*--------------------------------------------------------------------------------------*/

static NIL_type initialize_quotient_native_M(NIL_type)
  { native_define_M(quo_rawstring,
                    quotient_native); }

/*------------------------------------- remainder --------------------------------------*/

static const RWS_type rem_rawstring = "remainder";

static NIL_type remainder_native(FRM_type Argument_frame,
                                 EXP_type Tail_call_expression)
  { RWN_type left_rawnumber,
             rawnumber,
             right_rawnumber;
    NBR_type left_number,
             number,
             right_number;
    require_2_arguments(Argument_frame,
                        &left_number,
                        &right_number,
                        rem_rawstring);
    if (!is_NBR(left_number) || !is_NBR(right_number))
      native_error(ARN_error,
                   rem_rawstring);
    left_rawnumber = get_NBR(left_number);
    right_rawnumber = get_NBR(right_number);
    if (right_rawnumber == 0)
      native_error(DBZ_error,
                   rem_rawstring);
    else
      { rawnumber = left_rawnumber % right_rawnumber;
        number = make_NBR(rawnumber);
        Main_Set_Expression(number); }}

/*--------------------------------------------------------------------------------------*/

static NIL_type initialize_remainder_native_M(NIL_type)
  { native_define_M(rem_rawstring,
                    remainder_native); }

/*--------------------------------------------------------------------------------------*/
/*------------------------------ transcendent functions --------------------------------*/
/*--------------------------------------------------------------------------------------*/

/*---------------------------------------- cos -----------------------------------------*/

static const RWS_type cos_rawstring = "cos";

static NIL_type cos_native_C(FRM_type Argument_frame,
                             EXP_type Tail_call_expression)
  { EXP_type value_expression;
    REA_type real;
    RWN_type rawnumber;
    RWR_type rawreal;
    TAG_type tag;
    require_1_argument(Argument_frame,
                       &value_expression,
                       cos_rawstring);
    tag = Grammar_Tag(value_expression);
    switch (tag)
      { case NBR_tag:
          rawnumber = get_NBR(value_expression);
          rawreal = cos(rawnumber);
          break;
        case REA_tag:
          real = value_expression;
          rawreal = real->rwr;
          rawreal = cos(rawreal);
          break;
        default:
          rawreal = 0;
          native_error(ARN_error,
                       cos_rawstring); }
    real = make_real_C(rawreal,
                       cos_rawstring);
    Main_Set_Expression(real); }

/*--------------------------------------------------------------------------------------*/

static NIL_type initialize_cos_native_M(NIL_type)
  { native_define_M(cos_rawstring,
                    cos_native_C); }

/*---------------------------------------- exp -----------------------------------------*/

static const RWS_type exp_rawstring = "exp";

static NIL_type exp_native_C(FRM_type Argument_frame,
                             EXP_type Tail_call_expression)
  { EXP_type value_expression;
    REA_type real;
    RWN_type rawnumber;
    RWR_type rawreal;
    TAG_type tag;
    require_1_argument(Argument_frame,
                       &value_expression,
                       exp_rawstring);
    tag = Grammar_Tag(value_expression);
    switch (tag)
      { case NBR_tag:
          rawnumber = get_NBR(value_expression);
          rawreal = exp(rawnumber);
          break;
        case REA_tag:
          real = value_expression;
          rawreal = real->rwr;
          rawreal = exp(rawreal);
          break;
        default:
          rawreal = 0;
          native_error(ARN_error,
                       exp_rawstring); }
    real = make_real_C(rawreal,
                       exp_rawstring);
    Main_Set_Expression(real); }

/*--------------------------------------------------------------------------------------*/

static NIL_type initialize_exp_native_M(NIL_type)
  { native_define_M(exp_rawstring,
                    exp_native_C); }

/*---------------------------------------- expt ----------------------------------------*/

static const RWS_type ext_rawstring = "expt";

static NIL_type expt_native_C(FRM_type Argument_frame,
                              EXP_type Tail_call_expression)
  { EXP_type base_expression,
             exponent_expression;
    REA_type real;
    RWN_type base_rawnumber,
             exponent_rawnumber;
    RWR_type base_rawreal,
             exponent_rawreal,
             rawreal;
    TAG_type tag;
    require_2_arguments(Argument_frame,
                        &base_expression,
                        &exponent_expression,
                        ext_rawstring);
    tag = Grammar_Tag(base_expression);
    switch (tag)
      { case NBR_tag:
          base_rawnumber = get_NBR(base_expression);
          tag = Grammar_Tag(exponent_expression);
          switch (tag)
            { case NBR_tag:
                exponent_rawnumber = get_NBR(exponent_expression);
                if (base_rawnumber == 0)
                  if (exponent_rawnumber <= 0)
                    native_error(DBZ_error,
                                 ext_rawstring);
                rawreal = pow(base_rawnumber,
                              exponent_rawnumber);
                break;
              case REA_tag:
                real = exponent_expression;
                exponent_rawreal = real->rwr;
                if (base_rawnumber == 0)
                  if (exponent_rawreal <= 0)
                    native_error(DBZ_error,
                                 ext_rawstring);
                if (base_rawnumber < 0)
                  if (modf(exponent_rawreal,
                         &rawreal) != 0)
                    native_error(NNN_error,
                                 ext_rawstring);
                rawreal = pow(base_rawnumber,
                              exponent_rawreal);
                break;
              default:
                rawreal = 0;
                native_error(ARN_error,
                             ext_rawstring); }
          break;
        case REA_tag:
          real = base_expression;
          base_rawreal = real->rwr;
          tag = Grammar_Tag(exponent_expression);
          switch (tag)
            { case NBR_tag:
                exponent_rawnumber = get_NBR(exponent_expression);
                if (base_rawreal == 0)
                  if (exponent_rawnumber <= 0)
                    native_error(DBZ_error,
                                 ext_rawstring);
                rawreal = pow(base_rawreal,
                              exponent_rawnumber);
                break;
              case REA_tag:
                real = exponent_expression;
                exponent_rawreal = real->rwr;
                if (base_rawreal == 0)
                  if (exponent_rawreal <= 0)
                    native_error(DBZ_error,
                                 ext_rawstring);
               if (base_rawreal < 0)
                  if (modf(exponent_rawreal,
                         &rawreal) != 0)
                    native_error(NNN_error,
                                 ext_rawstring);
                rawreal = pow(base_rawreal,
                              exponent_rawreal);
                break;
              default:
                rawreal = 0;
                native_error(ARN_error,
                             ext_rawstring); }
          break;
        default:
          rawreal = 0;
          native_error(ARN_error,
                       ext_rawstring); }
    real = make_real_C(rawreal,
                       ext_rawstring);
    Main_Set_Expression(real); }

/*--------------------------------------------------------------------------------------*/

static NIL_type initialize_expt_native_M(NIL_type)
  { native_define_M(ext_rawstring,
                    expt_native_C); }

/*---------------------------------------- log -----------------------------------------*/

static const RWS_type log_rawstring = "log";

static NIL_type log_native_C(FRM_type Argument_frame,
                             EXP_type Tail_call_expression)
  { EXP_type value_expression;
    REA_type real;
    RWN_type rawnumber;
    RWR_type rawreal;
    TAG_type tag;
    require_1_argument(Argument_frame,
                       &value_expression,
                       log_rawstring);
    tag = Grammar_Tag(value_expression);
    switch (tag)
      { case NBR_tag:
          rawnumber = get_NBR(value_expression);
          if (rawnumber <= 0)
            native_error(PNR_error,
                         log_rawstring);
          rawreal = log(rawnumber);
          break;
        case REA_tag:
          real = value_expression;
          rawreal = real->rwr;
          if (rawreal <= 0)
            native_error(PNR_error,
                         log_rawstring);
          rawreal = log(rawreal);
          break;
        default:
          rawreal = 0;
          native_error(ARN_error,
                       log_rawstring); }
    real = make_real_C(rawreal,
                       log_rawstring);
    Main_Set_Expression(real); }

/*--------------------------------------------------------------------------------------*/

static NIL_type initialize_log_native_M(NIL_type)
  { native_define_M(log_rawstring,
                    log_native_C); }

/*---------------------------------------- sin -----------------------------------------*/

static const RWS_type sin_rawstring = "sin";

static NIL_type sin_native_C(FRM_type Argument_frame,
                             EXP_type Tail_call_expression)
  { EXP_type value_expression;
    REA_type real;
    RWN_type rawnumber;
    RWR_type rawreal;
    TAG_type tag;
    require_1_argument(Argument_frame,
                       &value_expression,
                       sin_rawstring);
    tag = Grammar_Tag(value_expression);
    switch (tag)
      { case NBR_tag:
          rawnumber = get_NBR(value_expression);
          rawreal = sin(rawnumber);
          break;
        case REA_tag:
          real = value_expression;
          rawreal = real->rwr;
          rawreal = sin(rawreal);
          break;
        default:
          rawreal = 0;
          native_error(ARN_error,
                       sin_rawstring); }
    real = make_real_C(rawreal,
                       sin_rawstring);
    Main_Set_Expression(real); }

/*--------------------------------------------------------------------------------------*/

static NIL_type initialize_sin_native_M(NIL_type)
  { native_define_M(sin_rawstring,
                    sin_native_C); }

/*---------------------------------------- sqrt ----------------------------------------*/

static const RWS_type sqt_rawstring = "sqrt";

static NIL_type sqrt_native_C(FRM_type Argument_frame,
                              EXP_type Tail_call_expression)
  { EXP_type value_expression;
    REA_type real;
    RWN_type rawnumber;
    RWR_type rawreal;
    TAG_type tag;
    require_1_argument(Argument_frame,
                       &value_expression,
                       sqt_rawstring);
    tag = Grammar_Tag(value_expression);
    switch (tag)
      { case NBR_tag:
          rawnumber = get_NBR(value_expression);
          if (rawnumber < 0)
            native_error(NNN_error,
                         sqt_rawstring);
          rawreal = sqrt(rawnumber);
          break;
        case REA_tag:
          real = value_expression;
          rawreal = real->rwr;
          if (rawreal < 0)
            native_error(NNN_error,
                         sqt_rawstring);
          rawreal = sqrt(rawreal);
          break;
        default:
          rawreal = 0;
          native_error(ARN_error,
                       sqt_rawstring); }
    real = make_real_C(rawreal,
                       sqt_rawstring);
    Main_Set_Expression(real); }

/*--------------------------------------------------------------------------------------*/

static NIL_type initialize_sqrt_native_M(NIL_type)
  { native_define_M(sqt_rawstring,
                    sqrt_native_C); }

/*---------------------------------------- tan -----------------------------------------*/

static const RWS_type tan_rawstring = "tan";

static NIL_type tan_native_C(FRM_type Argument_frame,
                             EXP_type Tail_call_expression)
  { EXP_type value_expression;
    REA_type real;
    RWN_type rawnumber;
    RWR_type rawreal;
    TAG_type tag;
    require_1_argument(Argument_frame,
                       &value_expression,
                       tan_rawstring);
    tag = Grammar_Tag(value_expression);
    switch (tag)
      { case NBR_tag:
          rawnumber = get_NBR(value_expression);
          rawreal = tan(rawnumber);
          break;
        case REA_tag:
          real = value_expression;
          rawreal = real->rwr;
          rawreal = tan(rawreal);
          break;
        default:
          rawreal = 0;
          native_error(ARN_error,
                       tan_rawstring); }
    real = make_real_C(rawreal,
                       tan_rawstring);
    Main_Set_Expression(real); }

/*--------------------------------------------------------------------------------------*/

static NIL_type initialize_tan_native_M(NIL_type)
  { native_define_M(tan_rawstring,
                    tan_native_C); }

/*--------------------------------------------------------------------------------------*/
/*------------------------------------- characters -------------------------------------*/
/*--------------------------------------------------------------------------------------*/

/*------------------------------------ integer->char -----------------------------------*/

static const RWS_type itc_rawstring = "integer->char";

static NIL_type integer_to_char_native(FRM_type Argument_frame,
                                       EXP_type Tail_call_expression)
  { CHA_type character;
    EXP_type number_expression;
    RWN_type rawnumber;
    require_1_argument(Argument_frame,
                       &number_expression,
                       itc_rawstring);
    if (!is_NBR(number_expression))
      native_error(NAN_error,
                   itc_rawstring);
    rawnumber = get_NBR(number_expression);
    if ((rawnumber < 0) &&
        (rawnumber > 255))
      native_error(COR_error,
                   itc_rawstring);
    character = make_CHA(rawnumber);
    Main_Set_Expression(character); }

/*--------------------------------------------------------------------------------------*/

static NIL_type initialize_integer_to_char_native_M(NIL_type)
  { native_define_M(itc_rawstring,
                    integer_to_char_native); }

/*------------------------------------ char->integer -----------------------------------*/

static const RWS_type cti_rawstring = "char->integer";

static NIL_type char_to_integer_native_C(FRM_type Argument_frame,
                                         EXP_type Tail_call_expression)
  { EXP_type character_expression;
    NBR_type number;
    UNS_type ordinal;
    require_1_argument(Argument_frame,
                       &character_expression,
                       cti_rawstring);
    if (!is_CHA(character_expression))
      native_error(NAC_error,
                   cti_rawstring);
    ordinal = get_CHA(character_expression);
    number = make_NBR(ordinal);
    Main_Set_Expression(number); }

/*--------------------------------------------------------------------------------------*/

static NIL_type initialize_char_to_integer_native_M(NIL_type)
  { native_define_M(cti_rawstring,
                    char_to_integer_native_C); }

/*--------------------------------------------------------------------------------------*/
/*--------------------------------------- strings --------------------------------------*/
/*--------------------------------------------------------------------------------------*/

/*------------------------------------ string-length -----------------------------------*/

static const RWS_type stl_rawstring = "string-length";

static NIL_type string_length_native(FRM_type Argument_frame,
                                     EXP_type Tail_call_expression)
  { EXP_type string_expression;
    STR_type string;
    RWS_type rawstring;
    NBR_type size_number;
    UNS_type number_of_arguments;
    require_1_argument(Argument_frame,
                       &string_expression,
                       stl_rawstring);
    if (!is_STR(string_expression))
      native_error(NAS_error,
                   stl_rawstring);
    string = string_expression;
    rawstring = string->rws;
    number_of_arguments = strlen(rawstring);
    size_number = make_NBR(number_of_arguments);
    Main_Set_Expression(size_number); }

/*--------------------------------------------------------------------------------------*/

static NIL_type initialize_string_length_M(NIL_type)
  { native_define_M(stl_rawstring,
                    string_length_native); }

/*------------------------------------- string-ref -------------------------------------*/

static const RWS_type srf_rawstring = "string-ref";

static NIL_type string_ref_native(FRM_type Argument_frame,
                                  EXP_type Tail_call_expression)
  { EXP_type number_expression,
             string_expression;
    CHA_type character;
    RWC_type rawcharacter;
    RWN_type rawnumber;
    RWS_type rawstring;
    STR_type string;
    UNS_type string_length;
    require_2_arguments(Argument_frame,
                        &string_expression,
                        &number_expression,
                        srf_rawstring);
    if (!is_STR(string_expression))
      native_error(NAS_error,
                   srf_rawstring);
    if (!is_NBR(number_expression))
      native_error(NAN_error,
                   srf_rawstring);
    rawnumber = get_NBR(number_expression);
    if (rawnumber < 0)
      native_error(NNN_error,
                   srf_rawstring);
    string = string_expression;
    rawstring = string->rws;
    string_length = strlen(rawstring);
    if (rawnumber >= string_length)
      native_error(IOR_error,
                   srf_rawstring);
    rawcharacter = rawstring[rawnumber];
    character = make_CHA(rawcharacter);
    Main_Set_Expression(character); }

/*--------------------------------------------------------------------------------------*/

static NIL_type initialize_string_ref_native_M(NIL_type)
  { native_define_M(srf_rawstring,
                    string_ref_native); }

/*------------------------------------- string-set! ------------------------------------*/

static const RWS_type sst_rawstring = "string-set!";

static NIL_type string_set_native(FRM_type Argument_frame,
                                  EXP_type Tail_call_expression)
  { EXP_type character_expression,
             number_expression,
             string_expression;
    RWC_type rawcharacter;
    RWN_type rawnumber;
    RWS_type rawstring;
    STR_type string;
    UNS_type string_length;
    require_3_arguments(Argument_frame,
                        &string_expression,
                        &number_expression,
                        &character_expression,
                        sst_rawstring);
    if (!is_STR(string_expression))
      native_error(NAS_error,
                   sst_rawstring);
    if (!is_NBR(number_expression))
      native_error(NAN_error,
                   sst_rawstring);
    if (!is_CHA(character_expression))
      native_error(NAC_error,
                   sst_rawstring);
    rawnumber = get_NBR(number_expression);
    if (rawnumber < 0)
      native_error(NNN_error,
                   sst_rawstring);
    string = string_expression;
    rawstring = string->rws;
    string_length = strlen(rawstring);
    if (rawnumber >= string_length)
      native_error(IOR_error,
                   sst_rawstring);
    rawcharacter = get_CHA(character_expression);
    rawstring[rawnumber] = rawcharacter;
    Main_Set_Expression(string); }

/*--------------------------------------------------------------------------------------*/

static NIL_type initialize_string_set_native_M(NIL_type)
  { native_define_M(sst_rawstring,
                    string_set_native); }

/*----------------------------------- string->symbol -----------------------------------*/

static const RWS_type sts_rawstring = "string->symbol";

static NIL_type string_to_symbol_native(FRM_type Argument_frame,
                                        EXP_type Tail_call_expression)
  { EXP_type string_expression;
    STR_type string;
    SYM_type symbol;
    require_1_argument(Argument_frame,
                       &string_expression,
                       cos_rawstring);
    if (!is_STR(string_expression))
      native_error(NAS_error,
                   sts_rawstring);
    string = string_expression;
    symbol = Pool_Enter_String_C(string);
    Main_Set_Expression(symbol); }

/*--------------------------------------------------------------------------------------*/

static NIL_type initialize_string_to_symbol_native_M(NIL_type)
  { native_define_M(sts_rawstring,
                    string_to_symbol_native); }

/*----------------------------------- symbol->string -----------------------------------*/

static const RWS_type sys_rawstring = "symbol->string";

static NIL_type symbol_to_string_native(FRM_type Argument_frame,
                                        EXP_type Tail_call_expression)
  { EXP_type symbol_expression;
    RWS_type rawstring;
    STR_type string;
    SYM_type symbol;
    UNS_type symbol_size;
    require_1_argument(Argument_frame,
                       &symbol_expression,
                       sys_rawstring);
    if (!is_SYM(symbol_expression))
      native_error(NSY_error,
                   sys_rawstring);
    symbol = symbol_expression;
    rawstring = symbol->rws;
    symbol_size = size_SYM(rawstring);
    BEGIN_PROTECT(symbol)
      MAIN_CLAIM_DYNAMIC_C(symbol_size);
    END_PROTECT(symbol)
    rawstring = symbol->rws;
    string = make_STR_M(rawstring);
    Main_Set_Expression(string); }

/*--------------------------------------------------------------------------------------*/

static NIL_type initialize_symbol_to_string_native_M(NIL_type)
  { native_define_M(sys_rawstring,
                    symbol_to_string_native); }

/*--------------------------------------------------------------------------------------*/
/*------------------------------------- comparison -------------------------------------*/
/*--------------------------------------------------------------------------------------*/

/*----------------------------------------- = ------------------------------------------*/

static const RWS_type eql_rawstring = "=";

static EXP_type equal_reals(RWR_type,
                            FRM_type,
                            UNS_type);

/*--------------------------------------------------------------------------------------*/

static EXP_type equal_characters(RWC_type Rawcharacter,
                                 FRM_type Argument_frame)
  { EXP_type value_expression;
    RWC_type previous_rawcharacter,
             rawcharacter;
    UNS_type index,
             number_of_arguments;
    rawcharacter = previous_rawcharacter = Rawcharacter;
    number_of_arguments = Environment_Frame_Size(Argument_frame);
    for (index = 2;
         index <= number_of_arguments;
         index += 1)
      { value_expression = Environment_Frame_Get(Argument_frame,
                                                 index);
        if (is_CHA(value_expression))
          rawcharacter = get_CHA(value_expression);
        else
          native_error(ARC_error,
                       eql_rawstring);
        if (previous_rawcharacter != rawcharacter)
          return Grammar_False;
        previous_rawcharacter = rawcharacter; }
    return Grammar_True; }

static EXP_type equal_numbers(NBR_type Number,
                              FRM_type Argument_frame)
  { EXP_type value_expression;
    RWR_type rawreal;
    RWN_type rawnumber,
             previous_rawnumber;
    REA_type real;
    TAG_type tag;
    UNS_type index,
             number_of_arguments;
    rawnumber = previous_rawnumber = get_NBR(Number);
    number_of_arguments = Environment_Frame_Size(Argument_frame);
    for (index = 2;
         index <= number_of_arguments;
         index += 1)
      { value_expression = Environment_Frame_Get(Argument_frame,
                                                 index);
        tag = Grammar_Tag(value_expression);
        switch (tag)
          { case NBR_tag:
              rawnumber = get_NBR(value_expression);
              if (previous_rawnumber != rawnumber)
                return Grammar_False;
              break;
            case REA_tag:
              real = value_expression;
              rawreal = real->rwr;
              if (previous_rawnumber != rawreal)
                return Grammar_False;
              return equal_reals(rawreal,
                                 Argument_frame,
                                 index + 1);
            default:
              native_error(ARN_error,
                           eql_rawstring); }
        previous_rawnumber = rawnumber; }
    return Grammar_True; }

static EXP_type equal_reals(RWR_type Rawreal,
                            FRM_type Argument_frame,
                            UNS_type Index)
  { EXP_type value_expression;
    REA_type real;
    RWR_type previous_rawreal,
             rawreal;
    TAG_type tag;
    UNS_type index,
             number_of_arguments;
    rawreal = previous_rawreal = Rawreal;
    number_of_arguments = Environment_Frame_Size(Argument_frame);
    for (index = Index;
         index <= number_of_arguments;
         index += 1)
      { value_expression = Environment_Frame_Get(Argument_frame,
                                                 index);
        tag = Grammar_Tag(value_expression);
        switch (tag)
          { case NBR_tag:
              rawreal = get_NBR(value_expression);
              break;
            case REA_tag:
              real = value_expression;
              rawreal = real->rwr;
              break;
            default:
              native_error(ARN_error,
                           eql_rawstring); }
        if (previous_rawreal != rawreal)
          return Grammar_False;
        previous_rawreal = rawreal; }
    return Grammar_True; }

static EXP_type equal_strings(RWS_type Rawstring,
                              FRM_type Argument_frame)
  { EXP_type value_expression;
    RWS_type previous_rawstring,
             rawstring;
    STR_type string;
    UNS_type index,
             number_of_arguments;
    rawstring = previous_rawstring = Rawstring;
    number_of_arguments = Environment_Frame_Size(Argument_frame);
    for (index = 2;
         index <= number_of_arguments;
         index += 1)
      { value_expression = Environment_Frame_Get(Argument_frame,
                                                 index);
        if (is_STR(value_expression))
          { string = value_expression;
            rawstring = string->rws; }
        else
          native_error(ARC_error,
                       eql_rawstring);
        if (strcmp(previous_rawstring,
                   rawstring) != 0)
          return Grammar_False;
        previous_rawstring = rawstring; }
    return Grammar_True; }

static NIL_type equal_native(FRM_type Argument_frame,
                             EXP_type Tail_call_expression)
  { EXP_type boolean_expression,
             value_expression;
    REA_type real;
    RWC_type rawcharacter;
    RWR_type rawreal;
    RWS_type rawstring;
    STR_type string;
    TAG_type tag;
    UNS_type number_of_arguments;
    number_of_arguments = Environment_Frame_Size(Argument_frame);
    if (number_of_arguments < 2)
      native_error(AL2_error,
                   eql_rawstring);
    value_expression = Environment_Frame_Get(Argument_frame,
                                             1);
    tag = Grammar_Tag(value_expression);
    switch (tag)
      { case CHA_tag:
          rawcharacter = get_CHA(value_expression);
          boolean_expression = equal_characters(rawcharacter,
                                                Argument_frame);
          break;
        case NBR_tag:
          boolean_expression = equal_numbers(value_expression,
                                             Argument_frame);
          break;
        case REA_tag:
          real = value_expression;
          rawreal = real->rwr;
          boolean_expression = equal_reals(rawreal,
                                           Argument_frame,
                                           2);
          break;
        case STR_tag:
          string = value_expression;
          rawstring = string->rws;
          boolean_expression = equal_strings(rawstring,
                                             Argument_frame);
          break;
        default:
          boolean_expression = Grammar_False;
          native_error(ARN_error,
                       eql_rawstring); }
    Environment_Release_Frame(Argument_frame);
    Main_Set_Expression(boolean_expression); }

/*--------------------------------------------------------------------------------------*/

static NIL_type initialize_equal_native_M(NIL_type)
  { native_define_M(eql_rawstring,
                    equal_native); }

/*----------------------------------------- > ------------------------------------------*/

static const RWS_type grt_rawstring = ">";

static EXP_type greater_reals(RWR_type,
                              FRM_type,
                              UNS_type);

/*--------------------------------------------------------------------------------------*/

static EXP_type greater_characters(RWC_type Rawcharacter,
                                   FRM_type Argument_frame)
  { EXP_type value_expression;
    RWC_type previous_rawcharacter,
             rawcharacter;
    UNS_type index,
             number_of_arguments;
    rawcharacter = previous_rawcharacter = Rawcharacter;
    number_of_arguments = Environment_Frame_Size(Argument_frame);
    for (index = 2;
         index <= number_of_arguments;
         index += 1)
      { value_expression = Environment_Frame_Get(Argument_frame,
                                                 index);
        if (is_CHA(value_expression))
          rawcharacter = get_CHA(value_expression);
        else
          native_error(ARC_error,
                       grt_rawstring);
        if (previous_rawcharacter <= rawcharacter)
          return Grammar_False;
        previous_rawcharacter = rawcharacter; }
    return Grammar_True; }

static EXP_type greater_numbers(NBR_type Number,
                                FRM_type Argument_frame)
  { EXP_type value_expression;
    REA_type real;
    RWN_type rawnumber,
             previous_rawnumber;
    RWR_type rawreal;
    TAG_type tag;
    UNS_type index,
             number_of_arguments;
    rawnumber = previous_rawnumber = get_NBR(Number);
    number_of_arguments = Environment_Frame_Size(Argument_frame);
    for (index = 2;
         index <= number_of_arguments;
         index += 1)
      { value_expression = Environment_Frame_Get(Argument_frame,
                                                 index);
        tag = Grammar_Tag(value_expression);
        switch (tag)
          { case NBR_tag:
              rawnumber = get_NBR(value_expression);
              if (previous_rawnumber <= rawnumber)
                return Grammar_False;
              break;
            case REA_tag:
              real = value_expression;
              rawreal = real->rwr;
              if (previous_rawnumber <= rawreal)
                return Grammar_False;
              return greater_reals(rawreal,
                                   Argument_frame,
                                   index + 1);
            default:
              native_error(ARN_error,
                           grt_rawstring); }
        previous_rawnumber = rawnumber; }
    return Grammar_True; }

static EXP_type greater_reals(RWR_type Rawreal,
                              FRM_type Argument_frame,
                              UNS_type Index)
  { EXP_type value_expression;
    REA_type real;
    RWR_type previous_rawreal,
             rawreal;
    TAG_type tag;
    UNS_type index,
             number_of_arguments;
    rawreal = previous_rawreal = Rawreal;
    number_of_arguments = Environment_Frame_Size(Argument_frame);
    for (index = Index;
         index <= number_of_arguments;
         index += 1)
      { value_expression = Environment_Frame_Get(Argument_frame,
                                                 index);
        tag = Grammar_Tag(value_expression);
        switch (tag)
          { case NBR_tag:
              rawreal = get_NBR(value_expression);
              break;
            case REA_tag:
              real = value_expression;
              rawreal = real->rwr;
              break;
            default:
              native_error(ARN_error,
                           grt_rawstring); }
        if (previous_rawreal <= rawreal)
          return Grammar_False;
        previous_rawreal = rawreal; }
   return Grammar_True; }

static EXP_type greater_strings(RWS_type Rawstring,
                                FRM_type Argument_frame)
  { EXP_type value_expression;
    RWS_type previous_rawstring,
             rawstring;
    STR_type string;
    UNS_type index,
             number_of_arguments;
    rawstring = previous_rawstring = Rawstring;
    number_of_arguments = Environment_Frame_Size(Argument_frame);
    for (index = 2;
         index <= number_of_arguments;
         index += 1)
      { value_expression = Environment_Frame_Get(Argument_frame,
                                                 index);
        if (is_STR(value_expression))
          { string = value_expression;
            rawstring = string->rws; }
        else
          native_error(ARC_error,
                       grt_rawstring);
        if (strcmp(previous_rawstring,
                   rawstring) <= 0)
          return Grammar_False;
        previous_rawstring = rawstring; }
    return Grammar_True; }

static NIL_type greater_native(FRM_type Argument_frame,
                               EXP_type Tail_call_expression)
  { EXP_type boolean_expression,
             value_expression;
    REA_type real;
    RWC_type rawcharacter;
    RWR_type rawreal;
    RWS_type rawstring;
    STR_type string;
    TAG_type tag;
    UNS_type number_of_arguments;
    number_of_arguments = Environment_Frame_Size(Argument_frame);
    if (number_of_arguments < 2)
      native_error(AL2_error,
                   grt_rawstring);
    value_expression = Environment_Frame_Get(Argument_frame,
                                             1);
    tag = Grammar_Tag(value_expression);
    switch (tag)
      { case CHA_tag:
          rawcharacter = get_CHA(value_expression);
          boolean_expression = greater_characters(rawcharacter,
                                                  Argument_frame);
          break;
        case NBR_tag:
          boolean_expression = greater_numbers(value_expression,
                                               Argument_frame);
          break;
        case REA_tag:
          real = value_expression;
          rawreal = real->rwr;
          boolean_expression = greater_reals(rawreal,
                                             Argument_frame,
                                             2);
          break;
        case STR_tag:
          string = value_expression;
          rawstring = string->rws;
          boolean_expression = greater_strings(rawstring,
                                               Argument_frame);
          break;
        default:
          boolean_expression = Grammar_False;
          native_error(ARN_error,
                       grt_rawstring); }
    Environment_Release_Frame(Argument_frame);
    Main_Set_Expression(boolean_expression); }

/*--------------------------------------------------------------------------------------*/

static NIL_type initialize_greater_native_M(NIL_type)
  { native_define_M(grt_rawstring,
                    greater_native); }

/*----------------------------------------- < ------------------------------------------*/

static const RWS_type lss_rawstring = "<";

static EXP_type less_reals(RWR_type,
                           FRM_type,
                           UNS_type);

/*--------------------------------------------------------------------------------------*/

static EXP_type less_characters(RWC_type Rawcharacter,
                                FRM_type Argument_frame)
  { EXP_type value_expression;
    RWC_type previous_rawcharacter,
             rawcharacter;
    UNS_type index,
             number_of_arguments;
    rawcharacter = previous_rawcharacter = Rawcharacter;
    number_of_arguments = Environment_Frame_Size(Argument_frame);
    for (index = 2;
         index <= number_of_arguments;
         index += 1)
      { value_expression = Environment_Frame_Get(Argument_frame,
                                                 index);
        if (is_CHA(value_expression))
          rawcharacter = get_CHA(value_expression);
        else
          native_error(ARC_error,
                       lss_rawstring);
        if (previous_rawcharacter >= rawcharacter)
          return Grammar_False;
        previous_rawcharacter = rawcharacter; }
    return Grammar_True; }

static EXP_type less_numbers(NBR_type Number,
                             FRM_type Argument_frame)
  { EXP_type value_expression;
    REA_type real;
    RWN_type rawnumber,
             previous_rawnumber;
    RWR_type rawreal;
    TAG_type tag;
    UNS_type index,
             number_of_arguments;
    rawnumber = previous_rawnumber = get_NBR(Number);
    number_of_arguments = Environment_Frame_Size(Argument_frame);
    for (index = 2;
         index <= number_of_arguments;
         index += 1)
      { value_expression = Environment_Frame_Get(Argument_frame,
                                                 index);
        tag = Grammar_Tag(value_expression);
        switch (tag)
          { case NBR_tag:
              rawnumber = get_NBR(value_expression);
              if (previous_rawnumber >= rawnumber)
                return Grammar_False;
              break;
            case REA_tag:
              real = value_expression;
              rawreal = real->rwr;
              if (previous_rawnumber >= rawreal)
                return Grammar_False;
              return less_reals(rawreal,
                                Argument_frame,
                                index + 1);
            default:
              native_error(ARN_error,
                           lss_rawstring); }
        previous_rawnumber = rawnumber; }
    return Grammar_True; }

static EXP_type less_reals(RWR_type Rawreal,
                           FRM_type Argument_frame,
                           UNS_type Index)
  { EXP_type value_expression;
    REA_type real;
    RWR_type previous_rawreal,
             rawreal;
    TAG_type tag;
    UNS_type index,
             number_of_arguments;
    rawreal = previous_rawreal = Rawreal;
    number_of_arguments = Environment_Frame_Size(Argument_frame);
    for (index = Index;
         index <= number_of_arguments;
         index += 1)
      { value_expression = Environment_Frame_Get(Argument_frame,
                                                 index);
        tag = Grammar_Tag(value_expression);
        switch (tag)
          { case NBR_tag:
              rawreal = get_NBR(value_expression);
              break;
            case REA_tag:
              real = value_expression;
              rawreal = real->rwr;
              break;
            default:
              native_error(ARN_error,
                           lss_rawstring); }
        if (previous_rawreal >= rawreal)
          return Grammar_False;
        previous_rawreal = rawreal; }
   return Grammar_True; }

static EXP_type less_strings(RWS_type Rawstring,
                             FRM_type Argument_frame)
  { EXP_type value_expression;
    RWS_type previous_rawstring,
             rawstring;
    STR_type string;
    UNS_type index,
             number_of_arguments;
    rawstring = previous_rawstring = Rawstring;
    number_of_arguments = Environment_Frame_Size(Argument_frame);
    for (index = 2;
         index <= number_of_arguments;
         index += 1)
      { value_expression = Environment_Frame_Get(Argument_frame,
                                                 index);
        if (is_STR(value_expression))
          { string = value_expression;
            rawstring = string->rws; }
        else
          native_error(ARC_error,
                       lss_rawstring);
        if (strcmp(previous_rawstring,
                   rawstring) >= 0)
          return Grammar_False;
        previous_rawstring = rawstring; }
    return Grammar_True; }

static NIL_type less_native(FRM_type Argument_frame,
                            EXP_type Tail_call_expression)
  { EXP_type boolean_expression,
             value_expression;
    REA_type real;
    RWC_type rawcharacter;
    RWR_type rawreal;
    RWS_type rawstring;
    STR_type string;
    TAG_type tag;
    UNS_type number_of_arguments;
    number_of_arguments = Environment_Frame_Size(Argument_frame);
    if (number_of_arguments < 2)
      native_error(AL2_error,
                   lss_rawstring);
    value_expression = Environment_Frame_Get(Argument_frame,
                                             1);
    tag = Grammar_Tag(value_expression);
    switch (tag)
      { case CHA_tag:
          rawcharacter = get_CHA(value_expression);
          boolean_expression = less_characters(rawcharacter,
                                               Argument_frame);
          break;
        case NBR_tag:
          boolean_expression = less_numbers(value_expression,
                                            Argument_frame);
          break;
        case REA_tag:
          real = value_expression;
          rawreal = real->rwr;
          boolean_expression = less_reals(rawreal,
                                          Argument_frame,
                                          2);
          break;
        case STR_tag:
          string = value_expression;
          rawstring = string->rws;
          boolean_expression = less_strings(rawstring,
                                            Argument_frame);
          break;
        default:
          boolean_expression = Grammar_False;
          native_error(ARN_error,
                       lss_rawstring); }
    Environment_Release_Frame(Argument_frame);
    Main_Set_Expression(boolean_expression); }

/*--------------------------------------------------------------------------------------*/

static NIL_type initialize_less_native_M(NIL_type)
  { native_define_M(lss_rawstring,
                    less_native); }

/*----------------------------------------- >= -----------------------------------------*/

static const RWS_type geq_rawstring = ">=";

static EXP_type greater_or_equal_reals(RWR_type,
                                       FRM_type,
                                       UNS_type);

/*--------------------------------------------------------------------------------------*/

static EXP_type greater_or_equal_characters(RWC_type Rawcharacter,
                                            FRM_type Argument_frame)
  { EXP_type value_expression;
    RWC_type previous_rawcharacter,
             rawcharacter;
    UNS_type index,
             number_of_arguments;
    rawcharacter = previous_rawcharacter = Rawcharacter;
    number_of_arguments = Environment_Frame_Size(Argument_frame);
    for (index = 2;
         index <= number_of_arguments;
         index += 1)
      { value_expression = Environment_Frame_Get(Argument_frame,
                                                 index);
        if (is_CHA(value_expression))
          rawcharacter = get_CHA(value_expression);
        else
          native_error(ARC_error,
                       geq_rawstring);
        if (previous_rawcharacter < rawcharacter)
          return Grammar_False;
        previous_rawcharacter = rawcharacter; }
    return Grammar_True; }

static EXP_type greater_or_equal_numbers(NBR_type Number,
                                         FRM_type Argument_frame)
  { EXP_type value_expression;
    REA_type real;
    RWN_type rawnumber,
             previous_rawnumber;
    RWR_type rawreal;
    TAG_type tag;
    UNS_type index,
             number_of_arguments;
    rawnumber = previous_rawnumber = get_NBR(Number);
    number_of_arguments = Environment_Frame_Size(Argument_frame);
    for (index = 2;
         index <= number_of_arguments;
         index += 1)
      { value_expression = Environment_Frame_Get(Argument_frame,
                                                 index);
        tag = Grammar_Tag(value_expression);
        switch (tag)
          { case NBR_tag:
              rawnumber = get_NBR(value_expression);
              if (previous_rawnumber < rawnumber)
                return Grammar_False;
              break;
            case REA_tag:
              real = value_expression;
              rawreal = real->rwr;
              if (previous_rawnumber < rawreal)
                return Grammar_False;
              return greater_or_equal_reals(rawreal,
                                            Argument_frame,
                                            index + 1);
            default:
              native_error(ARN_error,
                           geq_rawstring); }
        previous_rawnumber = rawnumber; }
    return Grammar_True; }

static EXP_type greater_or_equal_reals(RWR_type Rawreal,
                                       FRM_type Argument_frame,
                                       UNS_type Index)
  { EXP_type value_expression;
    REA_type real;
    RWR_type previous_rawreal,
             rawreal;
    TAG_type tag;
    UNS_type index,
             number_of_arguments;
    rawreal = previous_rawreal = Rawreal;
    number_of_arguments = Environment_Frame_Size(Argument_frame);
    for (index = Index;
         index <= number_of_arguments;
         index += 1)
      { value_expression = Environment_Frame_Get(Argument_frame,
                                                 index);
        tag = Grammar_Tag(value_expression);
        switch (tag)
          { case NBR_tag:
              rawreal = get_NBR(value_expression);
              break;
            case REA_tag:
              real = value_expression;
              rawreal = real->rwr;
              break;
            default:
              native_error(ARN_error,
                           geq_rawstring); }
        if (previous_rawreal < rawreal)
          return Grammar_False;
        previous_rawreal = rawreal; }
   return Grammar_True; }

static EXP_type greater_or_equal_strings(RWS_type Rawstring,
                                         FRM_type Argument_frame)
  { EXP_type value_expression;
    RWS_type previous_rawstring,
             rawstring;
    STR_type string;
    UNS_type index,
             number_of_arguments;
    rawstring = previous_rawstring = Rawstring;
    number_of_arguments = Environment_Frame_Size(Argument_frame);
    for (index = 2;
         index <= number_of_arguments;
         index += 1)
      { value_expression = Environment_Frame_Get(Argument_frame,
                                                 index);
        if (is_STR(value_expression))
          { string = value_expression;
            rawstring = string->rws; }
        else
          native_error(ARC_error,
                       geq_rawstring);
        if (strcmp(previous_rawstring,
                   rawstring) < 0)
          return Grammar_False;
        previous_rawstring = rawstring; }
    return Grammar_True; }

static NIL_type greater_or_equal_native(FRM_type Argument_frame,
                                        EXP_type Tail_call_expression)
  { EXP_type boolean_expression,
             value_expression;
    REA_type real;
    RWC_type rawcharacter;
    RWR_type rawreal;
    RWS_type rawstring;
    STR_type string;
    TAG_type tag;
    UNS_type number_of_arguments;
    number_of_arguments = Environment_Frame_Size(Argument_frame);
    if (number_of_arguments < 2)
      native_error(AL2_error,
                   geq_rawstring);
    value_expression = Environment_Frame_Get(Argument_frame,
                                             1);
    tag = Grammar_Tag(value_expression);
    switch (tag)
      { case CHA_tag:
          rawcharacter = get_CHA(value_expression);
          boolean_expression = greater_or_equal_characters(rawcharacter,
                                                           Argument_frame);
          break;
        case NBR_tag:
          boolean_expression = greater_or_equal_numbers(value_expression,
                                                        Argument_frame);
          break;
        case REA_tag:
          real = value_expression;
          rawreal = real->rwr;
          boolean_expression = greater_or_equal_reals(rawreal,
                                                      Argument_frame,
                                                      2);
          break;
        case STR_tag:
          string = value_expression;
          rawstring = string->rws;
          boolean_expression = greater_or_equal_strings(rawstring,
                                                          Argument_frame);
          break;
        default:
          boolean_expression = Grammar_False;
          native_error(ARN_error,
                       geq_rawstring); }
    Environment_Release_Frame(Argument_frame);
    Main_Set_Expression(boolean_expression); }

/*--------------------------------------------------------------------------------------*/

static NIL_type initialize_greater_or_equal_native_M(NIL_type)
  { native_define_M(geq_rawstring,
                    greater_or_equal_native); }

/*----------------------------------------- <= -----------------------------------------*/

static const RWS_type leq_rawstring = "<=";

static EXP_type less_or_equal_reals(RWR_type,
                                    FRM_type,
                                    UNS_type);

/*--------------------------------------------------------------------------------------*/

static EXP_type less_or_equal_characters(RWC_type Rawcharacter,
                                         FRM_type Argument_frame)
  { EXP_type value_expression;
    RWC_type previous_rawcharacter,
             rawcharacter;
    UNS_type index,
             number_of_arguments;
    rawcharacter = previous_rawcharacter = Rawcharacter;
    number_of_arguments = Environment_Frame_Size(Argument_frame);
    for (index = 2;
         index <= number_of_arguments;
         index += 1)
      { value_expression = Environment_Frame_Get(Argument_frame,
                                                 index);
        if (is_CHA(value_expression))
          rawcharacter = get_CHA(value_expression);
        else
          native_error(ARC_error,
                       leq_rawstring);
        if (previous_rawcharacter > rawcharacter)
          return Grammar_False;
        previous_rawcharacter = rawcharacter; }
    return Grammar_True; }

static EXP_type less_or_equal_numbers(NBR_type Number,
                                      FRM_type Argument_frame)
  { EXP_type value_expression;
    REA_type real;
    RWN_type rawnumber,
             previous_rawnumber;
    RWR_type rawreal;
    TAG_type tag;
    UNS_type index,
             number_of_arguments;
    rawnumber = previous_rawnumber = get_NBR(Number);
    number_of_arguments = Environment_Frame_Size(Argument_frame);
    for (index = 2;
         index <= number_of_arguments;
         index += 1)
      { value_expression = Environment_Frame_Get(Argument_frame,
                                                 index);
        tag = Grammar_Tag(value_expression);
        switch (tag)
          { case NBR_tag:
              rawnumber = get_NBR(value_expression);
              if (previous_rawnumber > rawnumber)
                return Grammar_False;
              break;
            case REA_tag:
              real = value_expression;
              rawreal = real->rwr;
              if (previous_rawnumber > rawreal)
                return Grammar_False;
              return less_or_equal_reals(rawreal,
                                         Argument_frame,
                                         index + 1);
            default:
              native_error(ARN_error,
                           grt_rawstring); }
        previous_rawnumber = rawnumber; }
    return Grammar_True; }

static EXP_type less_or_equal_reals(RWR_type Rawreal,
                                    FRM_type Argument_frame,
                                    UNS_type Index)
  { EXP_type value_expression;
    REA_type real;
    RWR_type previous_rawreal,
             rawreal;
    TAG_type tag;
    UNS_type index,
             number_of_arguments;
    rawreal = previous_rawreal = Rawreal;
    number_of_arguments = Environment_Frame_Size(Argument_frame);
    for (index = Index;
         index <= number_of_arguments;
         index += 1)
      { value_expression = Environment_Frame_Get(Argument_frame,
                                                 index);
        tag = Grammar_Tag(value_expression);
        switch (tag)
          { case NBR_tag:
              rawreal = get_NBR(value_expression);
              break;
            case REA_tag:
              real = value_expression;
              rawreal = real->rwr;
              break;
            default:
              native_error(ARN_error,
                           leq_rawstring); }
        if (previous_rawreal > rawreal)
          return Grammar_False;
        previous_rawreal = rawreal; }
   return Grammar_True; }

static EXP_type less_or_equal_strings(RWS_type Rawstring,
                                      FRM_type Argument_frame)
  { EXP_type value_expression;
    RWS_type previous_rawstring,
             rawstring;
    STR_type string;
    UNS_type index,
             number_of_arguments;
    rawstring = previous_rawstring = Rawstring;
    number_of_arguments = Environment_Frame_Size(Argument_frame);
    for (index = 2;
         index <= number_of_arguments;
         index += 1)
      { value_expression = Environment_Frame_Get(Argument_frame,
                                                 index);
        if (is_STR(value_expression))
          { string = value_expression;
            rawstring = string->rws; }
        else
          native_error(ARC_error,
                       leq_rawstring);
        if (strcmp(previous_rawstring,
                   rawstring) > 0)
          return Grammar_False;
        previous_rawstring = rawstring; }
    return Grammar_True; }

static NIL_type less_or_equal_native(FRM_type Argument_frame,
                                     EXP_type Tail_call_expression)
  { EXP_type boolean_expression,
             value_expression;
    REA_type real;
    RWC_type rawcharacter;
    RWR_type rawreal;
    RWS_type rawstring;
    STR_type string;
    TAG_type tag;
    UNS_type number_of_arguments;
    number_of_arguments = Environment_Frame_Size(Argument_frame);
    if (number_of_arguments < 2)
      native_error(AL2_error,
                   leq_rawstring);
    value_expression = Environment_Frame_Get(Argument_frame,
                                             1);
    tag = Grammar_Tag(value_expression);
    switch (tag)
      { case CHA_tag:
          rawcharacter = get_CHA(value_expression);
          boolean_expression = less_or_equal_characters(rawcharacter,
                                                        Argument_frame);
          break;
        case NBR_tag:
          boolean_expression = less_or_equal_numbers(value_expression,
                                                     Argument_frame);
          break;
        case REA_tag:
          real = value_expression;
          rawreal = real->rwr;
          boolean_expression = less_or_equal_reals(rawreal,
                                                   Argument_frame,
                                                   2);
          break;
        case STR_tag:
          string = value_expression;
          rawstring = string->rws;
          boolean_expression = less_or_equal_strings(rawstring,
                                                     Argument_frame);
          break;
        default:
          boolean_expression = Grammar_False;
          native_error(ARN_error,
                       leq_rawstring); }
    Environment_Release_Frame(Argument_frame);
    Main_Set_Expression(boolean_expression); }

/*--------------------------------------------------------------------------------------*/

static NIL_type initialize_less_or_equal_native_M(NIL_type)
  { native_define_M(leq_rawstring,
                    less_or_equal_native); }

/*----------------------------------------- eq? ----------------------------------------*/

static const RWS_type eqq_rawstring = "eq?";

static NIL_type equivalent_native(FRM_type Argument_frame,
                                  EXP_type Tail_call_expression)
  { EXP_type boolean_expression,
             left_expression,
             right_expression;
    require_2_arguments(Argument_frame,
                        &left_expression,
                        &right_expression,
                        eqq_rawstring);
    if (left_expression == right_expression)
      boolean_expression = Grammar_True;
    else
      boolean_expression = Grammar_False;
    Main_Set_Expression(boolean_expression); }

/*--------------------------------------------------------------------------------------*/

static NIL_type initialize_equivalent_native_M(NIL_type)
  { native_define_M(eqq_rawstring,
                    equivalent_native); }

/*--------------------------------------------------------------------------------------*/
/*-------------------------------------- booleans --------------------------------------*/
/*--------------------------------------------------------------------------------------*/

/*----------------------------------------- not ----------------------------------------*/

static const RWS_type not_rawstring = "not";

static NIL_type not_native(FRM_type Argument_frame,
                           EXP_type Tail_call_expression)
  { EXP_type boolean_expression;
    require_1_argument(Argument_frame,
                       &boolean_expression,
                       not_rawstring);
    if (is_FLS(boolean_expression))
      boolean_expression = Grammar_True;
    else if (is_TRU(boolean_expression))
      boolean_expression = Grammar_False;
    else
      native_error(NAB_error,
                   not_rawstring);
    Main_Set_Expression(boolean_expression); }

/*--------------------------------------------------------------------------------------*/

static NIL_type initialize_not_native_M(NIL_type)
  { native_define_M(not_rawstring,
                    not_native); }

/*--------------------------------------------------------------------------------------*/
/*------------------------------------- predicates -------------------------------------*/
/*--------------------------------------------------------------------------------------*/

/*--------------------------------------- boolean? -------------------------------------*/

static const RWS_type ibl_rawstring = "boolean?";

static NIL_type is_boolean_native(FRM_type Argument_frame,
                                  EXP_type Tail_call_expression)
  { EXP_type boolean_expression,
             value_expression;
    require_1_argument(Argument_frame,
                       &value_expression,
                       ibl_rawstring);
    if (is_TRU(value_expression))
      boolean_expression = Grammar_True;
    else if (is_FLS(value_expression))
      boolean_expression = Grammar_True;
    else
      boolean_expression = Grammar_False;
    Main_Set_Expression(boolean_expression); }

/*--------------------------------------------------------------------------------------*/

static NIL_type initialize_is_boolean_native_M(NIL_type)
  { native_define_M(ibl_rawstring,
                    is_boolean_native); }

/*-------------------------------------- character? ------------------------------------*/

static const RWS_type ich_rawstring = "char?";

static NIL_type is_char_native(FRM_type Argument_frame,
                               EXP_type Tail_call_expression)
  { EXP_type boolean_expression,
             value_expression;
    require_1_argument(Argument_frame,
                       &value_expression,
                       ich_rawstring);
    if (is_CHA(value_expression))
      boolean_expression = Grammar_True;
    else
      boolean_expression = Grammar_False;
    Main_Set_Expression(boolean_expression); }

/*--------------------------------------------------------------------------------------*/

static NIL_type initialize_is_char_native_M(NIL_type)
  { native_define_M(ich_rawstring,
                    is_char_native); }

/*--------------------------------------- integer? -------------------------------------*/

static const RWS_type iin_rawstring = "integer?";

static NIL_type is_integer_native(FRM_type Argument_frame,
                                  EXP_type Tail_call_expression)
  { EXP_type boolean_expression,
             value_expression;
    require_1_argument(Argument_frame,
                       &value_expression,
                       iin_rawstring);
    if (is_NBR(value_expression))
      boolean_expression = Grammar_True;
    else
      boolean_expression = Grammar_False;
    Main_Set_Expression(boolean_expression); }

/*--------------------------------------------------------------------------------------*/

static NIL_type initialize_is_integer_native_M(NIL_type)
  { native_define_M(iin_rawstring,
                    is_integer_native); }

/*---------------------------------------- null? ---------------------------------------*/

static const RWS_type inl_rawstring = "null?";

static NIL_type is_null_native(FRM_type Argument_frame,
                               EXP_type Tail_call_expression)
  { EXP_type boolean_expression,
             value_expression;
    require_1_argument(Argument_frame,
                       &value_expression,
                       inl_rawstring);
    if (is_NUL(value_expression))
      boolean_expression = Grammar_True;
    else
      boolean_expression = Grammar_False;
    Main_Set_Expression(boolean_expression); }

/*--------------------------------------------------------------------------------------*/

static NIL_type initialize_is_null_native_M(NIL_type)
  { native_define_M(inl_rawstring,
                    is_null_native); }

/*--------------------------------------- number? --------------------------------------*/

static const RWS_type inb_rawstring = "number?";

static NIL_type is_number_native(FRM_type Argument_frame,
                                 EXP_type Tail_call_expression)
  { EXP_type boolean_expression,
             value_expression;
    require_1_argument(Argument_frame,
                       &value_expression,
                       inb_rawstring);
    if (is_NBR(value_expression))
      boolean_expression = Grammar_True;
    else if (is_REA(value_expression))
      boolean_expression = Grammar_True;
    else
      boolean_expression = Grammar_False;
    Main_Set_Expression(boolean_expression); }

/*--------------------------------------------------------------------------------------*/

static NIL_type initialize_is_number_native_M(NIL_type)
  { native_define_M(inb_rawstring,
                    is_number_native); }

/*---------------------------------------- pair? ---------------------------------------*/

static const RWS_type ipa_rawstring = "pair?";

static NIL_type is_pair_native(FRM_type Argument_frame,
                               EXP_type Tail_call_expression)
  { EXP_type boolean_expression,
             value_expression;
    require_1_argument(Argument_frame,
                       &value_expression,
                       ipa_rawstring);
    if (is_PAI(value_expression))
      boolean_expression = Grammar_True;
    else
      boolean_expression = Grammar_False;
    Main_Set_Expression(boolean_expression); }

/*--------------------------------------------------------------------------------------*/

static NIL_type initialize_is_pair_native_M(NIL_type)
  { native_define_M(ipa_rawstring,
                    is_pair_native); }

/*------------------------------------- procedure? -------------------------------------*/

static const RWS_type ipr_rawstring = "procedure?";

static NIL_type is_procedure_native(FRM_type Argument_frame,
                                    EXP_type Tail_call_expression)
  { EXP_type boolean_expression,
             value_expression;
    require_1_argument(Argument_frame,
                       &value_expression,
                       ipr_rawstring);
    if (is_PRC(value_expression))
      boolean_expression = Grammar_True;
    else if (is_PRV(value_expression))
      boolean_expression = Grammar_True;
    else if (is_NAT(value_expression))
      boolean_expression = Grammar_True;
    else
      boolean_expression = Grammar_False;
    Main_Set_Expression(boolean_expression); }

/*--------------------------------------------------------------------------------------*/

static NIL_type initialize_is_procedure_native_M(NIL_type)
  { native_define_M(ipr_rawstring,
                    is_procedure_native); }

/*---------------------------------------- real? ---------------------------------------*/

static const RWS_type ire_rawstring = "real?";

static NIL_type is_real_native(FRM_type Argument_frame,
                               EXP_type Tail_call_expression)
  { EXP_type boolean_expression,
             value_expression;
    require_1_argument(Argument_frame,
                       &value_expression,
                       ire_rawstring);
    if (is_REA(value_expression))
      boolean_expression = Grammar_True;
    else
      boolean_expression = Grammar_False;
    Main_Set_Expression(boolean_expression); }

/*--------------------------------------------------------------------------------------*/

static NIL_type initialize_is_real_native_M(NIL_type)
  { native_define_M(ire_rawstring,
                    is_real_native); }

/*--------------------------------------- string? --------------------------------------*/

static const RWS_type str_rawstring = "string?";

static NIL_type is_string_native(FRM_type Argument_frame,
                                 EXP_type Tail_call_expression)
  { EXP_type boolean_expression,
             value_expression;
    require_1_argument(Argument_frame,
                       &value_expression,
                       str_rawstring);
    if (is_STR(value_expression))
      boolean_expression = Grammar_True;
    else
      boolean_expression = Grammar_False;
    Main_Set_Expression(boolean_expression); }

/*--------------------------------------------------------------------------------------*/

static NIL_type initialize_is_string_native_M(NIL_type)
  { native_define_M(str_rawstring,
                    is_string_native); }

/*--------------------------------------- symbol? --------------------------------------*/

static const RWS_type ism_rawstring = "symbol?";

static NIL_type is_symbol_native(FRM_type Argument_frame,
                                 EXP_type Tail_call_expression)
  { EXP_type boolean_expression,
             value_expression;
    require_1_argument(Argument_frame,
                       &value_expression,
                       ism_rawstring);
    if (is_SYM(value_expression))
      boolean_expression = Grammar_True;
    else
      boolean_expression = Grammar_False;
    Main_Set_Expression(boolean_expression); }

/*--------------------------------------------------------------------------------------*/

static NIL_type initialize_is_symbol_native_M(NIL_type)
  { native_define_M(ism_rawstring,
                    is_symbol_native); }

/*--------------------------------------- vector? --------------------------------------*/

static const RWS_type ivc_rawstring = "vector?";

static NIL_type is_vector_native(FRM_type Argument_frame,
                                 EXP_type Tail_call_expression)
  { EXP_type boolean_expression,
             value_expression;
    require_1_argument(Argument_frame,
                       &value_expression,
                       ivc_rawstring);
    if (is_VEC(value_expression))
      boolean_expression = Grammar_True;
    else
      boolean_expression = Grammar_False;
    Main_Set_Expression(boolean_expression); }

/*--------------------------------------------------------------------------------------*/

static NIL_type initialize_is_vector_native_M(NIL_type)
  { native_define_M(ivc_rawstring,
                    is_vector_native); }

/*--------------------------------------------------------------------------------------*/
/*--------------------------------------- pairs ----------------------------------------*/
/*--------------------------------------------------------------------------------------*/

/*---------------------------------------- cons ----------------------------------------*/

static const RWS_type cns_rawstring = "cons";

static NIL_type cons_native_C(FRM_type Argument_frame,
                              EXP_type Tail_call_expression)
  { EXP_type car_expression,
             cdr_expression;
    PAI_type pair;
    BEGIN_PROTECT(Argument_frame)
      MAIN_CLAIM_DEFAULT_C();
    END_PROTECT(Argument_frame)
    require_2_arguments(Argument_frame,
                        &car_expression,
                        &cdr_expression,
                        cns_rawstring);
    pair = make_PAI_M(car_expression,
                      cdr_expression);
    Main_Set_Expression(pair); }

/*--------------------------------------------------------------------------------------*/

static NIL_type initialize_cons_native_M(NIL_type)
  { native_define_M(cns_rawstring,
                    cons_native_C); }

/*----------------------------------------- car ----------------------------------------*/

static const RWS_type car_rawstring = "car";

static NIL_type car_native(FRM_type Argument_frame,
                           EXP_type Tail_call_expression)
  { EXP_type pair_expression,
             value_expression;
    PAI_type pair;
    require_1_argument(Argument_frame,
                       &pair_expression,
                       car_rawstring);
    if (!is_PAI(pair_expression))
      native_error(NAP_error,
                   car_rawstring);
    pair = pair_expression;
    value_expression = pair->car;
    Main_Set_Expression(value_expression); }

/*--------------------------------------------------------------------------------------*/

static NIL_type initialize_car_native_M(NIL_type)
  { native_define_M(car_rawstring,
                    car_native); }

/*----------------------------------------- cdr ----------------------------------------*/

static const RWS_type cdr_rawstring = "cdr";

static NIL_type cdr_native(FRM_type Argument_frame,
                           EXP_type Tail_call_expression)
  { EXP_type pair_expression,
             value_expression;
    PAI_type pair;
    require_1_argument(Argument_frame,
                       &pair_expression,
                       cdr_rawstring);
    if (!is_PAI(pair_expression))
      native_error(NAP_error,
                   cdr_rawstring);
    pair = pair_expression;
    value_expression = pair->cdr;
    Main_Set_Expression(value_expression); }

/*--------------------------------------------------------------------------------------*/

static NIL_type initialize_cdr_native_M(NIL_type)
  { native_define_M(cdr_rawstring,
                    cdr_native); }

/*-------------------------------------- set-car! --------------------------------------*/

static const RWS_type sca_rawstring = "set-car!";

static NIL_type set_car_native(FRM_type Argument_frame,
                               EXP_type Tail_call_expression)
  { EXP_type pair_expression,
             value_expression;
    PAI_type pair;
    require_2_arguments(Argument_frame,
                        &pair_expression,
                        &value_expression,
                        sca_rawstring);
    if (!is_PAI(pair_expression))
      native_error(NAP_error,
                   sca_rawstring);
    pair = pair_expression;
    pair->car = value_expression;
    Main_Set_Expression(pair); }

/*--------------------------------------------------------------------------------------*/

static NIL_type initialize_set_car_native_M(NIL_type)
  { native_define_M(sca_rawstring,
                    set_car_native); }

/*-------------------------------------- set-cdr! --------------------------------------*/

static const RWS_type scd_rawstring = "set-cdr!";

static NIL_type set_cdr_native(FRM_type Argument_frame,
                               EXP_type Tail_call_expression)
  { EXP_type pair_expression,
             value_expression;
    PAI_type pair;
    require_2_arguments(Argument_frame,
                        &pair_expression,
                        &value_expression,
                        scd_rawstring);
    if (!is_PAI(pair_expression))
      native_error(NAP_error,
                   scd_rawstring);
    pair = pair_expression;
    pair->cdr = value_expression;
    Main_Set_Expression(pair); }

/*--------------------------------------------------------------------------------------*/

static NIL_type initialize_set_cdr_native_M(NIL_type)
  { native_define_M(scd_rawstring,
                    set_cdr_native); }

/*---------------------------------------- list ----------------------------------------*/

static const RWS_type list_rawstring = "list";

static NIL_type list_native_C(FRM_type Argument_frame,
                              EXP_type Tail_call_expression)
  { EXP_type value_expression;
    PAI_type list_pair;
    UNS_type index,
             number_of_arguments;
    number_of_arguments = Environment_Frame_Size(Argument_frame);
    if (number_of_arguments == 0)
      { Environment_Release_Frame(Argument_frame);
        Main_Set_Expression(Grammar_Null);
        return; }
    list_pair = Grammar_Null;
    for (index = number_of_arguments;
         index > 0 ;
         index -= 1)
      { BEGIN_DOUBLE_PROTECT(Argument_frame,
                             list_pair)
          MAIN_CLAIM_DEFAULT_C();
        END_DOUBLE_PROTECT(Argument_frame,
                           list_pair)
        value_expression = Environment_Frame_Get(Argument_frame,
                                                 index);
        list_pair = make_PAI_M(value_expression,
                               list_pair); }
    Environment_Release_Frame(Argument_frame);
    Main_Set_Expression(list_pair); }

/*--------------------------------------------------------------------------------------*/

static NIL_type initialize_list_native_M(NIL_type)
  { native_define_M(list_rawstring,
                    list_native_C); }

/*------------------------------------- list-length ------------------------------------*/

static const RWS_type len_rawstring = "length";

static NIL_type length_native(FRM_type Argument_frame,
                              EXP_type Tail_call_expression)
  { EXP_type pair_expression;
    NBR_type number;
    PAI_type pair;
    UNS_type list_length;
    require_1_argument(Argument_frame,
                       &pair_expression,
                       len_rawstring);
    for (list_length = 0;
         is_PAI(pair_expression);
         list_length += 1)
      { pair = pair_expression;
        pair_expression = pair->cdr; }
    if (!is_NUL(pair_expression))
      native_error(NAL_error,
                   len_rawstring);
    number = make_NBR(list_length);
    Main_Set_Expression(number); }

/*--------------------------------------------------------------------------------------*/

static NIL_type initialize_length_native_M(NIL_type)
  { native_define_M(len_rawstring,
                    length_native); }

/*---------------------------------------- assoc ---------------------------------------*/

static const RWS_type ass_rawstring = "assoc";

static NIL_type assoc_native(FRM_type Argument_frame,
                             EXP_type Tail_call_expression)
  { EXP_type list_expression,
             value_expression;
    PAI_type list_pair,
             pair;
    TAG_type tag;
    require_2_arguments(Argument_frame,
                        &value_expression,
                        &list_expression,
                        ass_rawstring);
    for (;;)
      { tag = Grammar_Tag(list_expression);
        switch (tag)
          { case NUL_tag:
              Main_Set_Expression(Grammar_False);
              return;
            case PAI_tag:
              list_pair = list_expression;
              pair = list_pair->car;
              if (!is_PAI(pair))
                native_error(NAP_error,
                             ass_rawstring);
              if (value_expression == pair->car)
                { Main_Set_Expression(pair);
                  return; }
              list_expression = list_pair->cdr;
              break;
            default:
              native_error(NAL_error,
                           ass_rawstring); }}}

/*--------------------------------------------------------------------------------------*/

static NIL_type initialize_assoc_native_M(NIL_type)
  { native_define_M(ass_rawstring,
                    assoc_native); }

/*--------------------------------------------------------------------------------------*/
/*-------------------------------------- vectors ---------------------------------------*/
/*--------------------------------------------------------------------------------------*/

/*------------------------------------- make-vector ------------------------------------*/

static const RWS_type mkv_rawstring = "make-vector";

static NIL_type make_vector_native_C(FRM_type Argument_frame,
                                     EXP_type Tail_call_expression)
  { EXP_type value_expression;
    NBR_type number;
    RWN_type size;
    UNS_type index,
             number_of_arguments;
    VEC_type vector;
    number_of_arguments = Environment_Frame_Size(Argument_frame);
    if (number_of_arguments == 0)
      native_error(AL1_error,
                   mkv_rawstring);
    if (number_of_arguments > 2)
      native_error(AM2_error,
                   mkv_rawstring);
    number = Environment_Frame_Get(Argument_frame,
                                   1);
    if (!is_NBR(number))
      native_error(NAN_error,
                   mkv_rawstring);
    size = get_NBR(number);
    if (size < 0)
      native_error(NNN_error,
                   mkv_rawstring);
    if (size == 0)
      { Main_Set_Expression(Grammar_Empty_Vector);
        return; }
    if (number_of_arguments == 1)
      value_expression = Grammar_Unspecified;
    else
      value_expression = Environment_Frame_Get(Argument_frame,
                                               2);
    Environment_Release_Frame(Argument_frame);
    BEGIN_PROTECT(value_expression)
      MAIN_CLAIM_DYNAMIC_C(size);
    END_PROTECT(value_expression)
    vector = make_VEC_Z(size);
    for (index = 1;
         index <= size;
         index += 1)
      vector[index] = value_expression;
    Main_Set_Expression(vector); }

/*--------------------------------------------------------------------------------------*/

static NIL_type initialize_make_vector_native_M(NIL_type)
  { native_define_M(mkv_rawstring,
                    make_vector_native_C); }

/*------------------------------------- vector-ref -------------------------------------*/

static const RWS_type vrf_rawstring = "vector-ref";

static NIL_type vector_ref_native(FRM_type Argument_frame,
                                  EXP_type Tail_call_expression)
  { EXP_type number_expression,
             value_expression,
             vector_expression;
    RWN_type rawnumber;
    UNS_type vector_size;
    VEC_type vector;
    require_2_arguments(Argument_frame,
                        &vector_expression,
                        &number_expression,
                        vrf_rawstring);
    if (!is_VEC(vector_expression))
      native_error(NAV_error,
                   vrf_rawstring);
    if (!is_NBR(number_expression))
      native_error(NAN_error,
                   vrf_rawstring);
    rawnumber = get_NBR(number_expression);
    if (rawnumber < 0)
      native_error(NNN_error,
                   vrf_rawstring);
    vector = vector_expression;
    vector_size = size_VEC(vector);
    if (rawnumber >= vector_size)
      native_error(IOR_error,
                   vrf_rawstring);
    value_expression = vector[rawnumber + 1];
    Main_Set_Expression(value_expression); }

/*--------------------------------------------------------------------------------------*/

static NIL_type initialize_vector_ref_native_M(NIL_type)
  { native_define_M(vrf_rawstring,
                    vector_ref_native); }

/*------------------------------------- vector-set! ------------------------------------*/

static const RWS_type vst_rawstring = "vector-set!";

static NIL_type vector_set_native(FRM_type Argument_frame,
                                  EXP_type Tail_call_expression)
  { EXP_type number_expression,
             value_expression,
             vector_expression;
    RWN_type rawnumber;
    UNS_type vector_size;
    VEC_type vector;
    require_3_arguments(Argument_frame,
                        &vector_expression,
                        &number_expression,
                        &value_expression,
                        sst_rawstring);
    if (!is_VEC(vector_expression))
      native_error(NAV_error,
                   vst_rawstring);
    if (!is_NBR(number_expression))
      native_error(NAN_error,
                   vst_rawstring);
    rawnumber = get_NBR(number_expression);
    if (rawnumber < 0)
      native_error(NNN_error,
                   vst_rawstring);
    vector = vector_expression;
    vector_size = size_VEC(vector);
    if (rawnumber >= vector_size)
      native_error(IOR_error,
                   vst_rawstring);
    vector[rawnumber + 1] = value_expression;
    Main_Set_Expression(vector); }

/*--------------------------------------------------------------------------------------*/

static NIL_type initialize_vector_set_native_M(NIL_type)
  { native_define_M(vst_rawstring,
                    vector_set_native); }

/*--------------------------------------- vector ---------------------------------------*/

static const RWS_type vec_rawstring = "vector";

static NIL_type vector_native(FRM_type Argument_frame,
                              EXP_type Tail_call_expression)
  { VEC_type vector;
    vector = (VEC_type)Argument_frame;
    Main_Set_Expression(vector); }

/*--------------------------------------------------------------------------------------*/

static NIL_type initialize_vector_native_M(NIL_type)
  { native_define_M(vec_rawstring,
                    vector_native); }

/*------------------------------------ vector-length -----------------------------------*/

static const RWS_type vcl_rawstring = "vector-length";

static NIL_type vector_length_native(FRM_type Argument_frame,
                                     EXP_type Tail_call_expression)
  { EXP_type vector_expression;
    NBR_type number;
    UNS_type vector_size;
    require_1_argument(Argument_frame,
                       &vector_expression,
                       vcl_rawstring);
    if (!is_VEC(vector_expression))
      native_error(NAV_error,
                   vcl_rawstring);
    vector_size = size_VEC(vector_expression);
    number = make_NBR(vector_size);
    Main_Set_Expression(number); }

/*--------------------------------------------------------------------------------------*/

static NIL_type initialize_vector_length_native_M(NIL_type)
  { native_define_M(vcl_rawstring,
                    vector_length_native); }

/*--------------------------------------------------------------------------------------*/
/*-------------------------------------- control ---------------------------------------*/
/*--------------------------------------------------------------------------------------*/

/*--------------------------------------- apply ----------------------------------------*/

static const RWS_type apl_rawstring = "apply";

static NIL_type apply_native_C(FRM_type Argument_frame,
                               EXP_type Tail_call_expression)
  { EXP_type argument_expression,
             procedure_expression;
    require_2_arguments(Argument_frame,
                        &procedure_expression,
                        &argument_expression,
                        apl_rawstring);
    Evaluate_Apply_C(procedure_expression,
                     argument_expression,
                     Tail_call_expression); }

/*--------------------------------------------------------------------------------------*/

static NIL_type initialize_apply_native_M(NIL_type)
  { native_define_M(apl_rawstring,
                    apply_native_C); }

/*---------------------------- call-with-current-continuation --------------------------*/

static const RWS_type ccc_rawstring = "call-with-current-continuation";

static NIL_type call_with_current_continuation_native_C(FRM_type Argument_frame,
                                                        EXP_type Tail_call_expression)
  { CNT_type continuation;
    ENV_type environment;
    EXP_type procedure_expression;
    FRM_type frame;
    PAI_type argument_pair;
    THR_type thread;
    BEGIN_PROTECT(Argument_frame)
      MAIN_CLAIM_DEFAULT_C();
    END_PROTECT(Argument_frame)
    require_1_argument(Argument_frame,
                       &procedure_expression,
                       ccc_rawstring);
    environment = Environment_Get_Current_Environment();
    frame = Environment_Mark_Frame();
    thread = Thread_Mark();
    continuation = make_CNT_M(environment,
                              frame,
                              thread);
    argument_pair = make_PAI_M(continuation,
                               Grammar_Null);
    Evaluate_Apply_C(procedure_expression,
                     argument_pair,
                     Tail_call_expression); }

/*--------------------------------------------------------------------------------------*/

static NIL_type initialize_call_with_current_continuation_native_M(NIL_type)
  { native_define_M(ccc_rawstring,
                    call_with_current_continuation_native_C); }

/*---------------------------------------- eval ----------------------------------------*/

static const RWS_type evl_rawstring = "eval";

static NBR_type Continue_eval;

THREAD_BEGIN_FRAME(Eva);
  THREAD_FRAME_SLOT(ENV, env);
  THREAD_FRAME_SLOT(FRM, frm);
THREAD_END_FRAME(Eva);

static NIL_type continue_eval(NIL_type)
  { Eva_type eval_thread;
    ENV_type environment;
    THR_type thread;
    FRM_type frame;
    thread = Thread_Peek();
    eval_thread = (Eva_type)thread;
    environment  = eval_thread->env;
    frame = eval_thread->frm;
    Thread_Zap();
    Environment_Release_Current_Frame();
    Environment_Replace_Environment(environment,
                                    frame); }

static NIL_type eval_native_C(FRM_type Argument_frame,
                              EXP_type Tail_call_expression)
  { Eva_type eval_thread;
    ENV_type environment;
    EXP_type compiled_expression,
             value_expression;
    THR_type thread;
    FRM_type frame;
    require_1_argument(Argument_frame,
                       &value_expression,
                       evl_rawstring);
    compiled_expression = Compile_Compile_C(value_expression);
    BEGIN_PROTECT(compiled_expression)
      thread = Thread_Push_C(Continue_eval,
                             Grammar_False,
                             Eva_size);
    END_PROTECT(compiled_expression)
    eval_thread = (Eva_type)thread;
    environment = Environment_Get_Current_Environment();
    frame = Environment_Get_Current_Frame();
    eval_thread->env = environment;
    eval_thread->frm = frame;
    Environment_Reset();
    Evaluate_Eval_C(compiled_expression,
                    Tail_call_expression); }

/*--------------------------------------------------------------------------------------*/

static NIL_type initialize_eval_native_M(NIL_type)
  { slipken_persist_init(Continue_eval, Thread_Register(continue_eval));
    native_define_M(evl_rawstring,
                    eval_native_C); }

/*--------------------------------------- force ----------------------------------------*/

static const RWS_type frc_rawstring = "force";

static NBR_type Continue_force;

THREAD_BEGIN_FRAME(Frc);
  PRM_type prm;
THREAD_END_FRAME(Frc);

static NIL_type continue_force(NIL_type)
  { Frc_type force_thread;
    EXP_type value_expression;
    PRM_type promise;
    THR_type thread;
    value_expression = Main_Get_Expression();
    thread = Thread_Peek();
    force_thread = (Frc_type)thread;
    promise = force_thread->prm;
    Thread_Zap();
    convert_FRC(promise,
                value_expression);
    Main_Set_Expression(value_expression); }

static NIL_type force_native_C(FRM_type Argument_frame,
                               EXP_type Tail_call_expression)
  { Frc_type force_thread;
    EXP_type value_expression;
    FRC_type force;
    PRM_type promise;
    TAG_type tag;
    THR_type thread;
    require_1_argument(Argument_frame,
                       &value_expression,
                       frc_rawstring);
    tag = Grammar_Tag(value_expression);
    switch (tag)
      { case FRC_tag:
          force = value_expression;
          value_expression = force->val;
          Main_Set_Expression(value_expression);
          return;
        case PRM_tag:
          BEGIN_PROTECT(value_expression);
            thread = Thread_Push_C(Continue_force,
                                   Grammar_False,
                                   Frc_size);
          END_PROTECT(promise)
          force_thread = (Frc_type)thread;
          force_thread->prm = promise;
          Evaluate_Promise_C(promise,
                             Tail_call_expression);
        default:
          native_error(FRP_error,
                       frc_rawstring); }}

/*--------------------------------------------------------------------------------------*/

static NIL_type initialize_force_native_M(NIL_type)
  { Continue_force = Thread_Register(continue_force);
    native_define_M(frc_rawstring,
                    force_native_C); }

/*-------------------------------------- for-each --------------------------------------*/

static const RWS_type for_rawstring = "for-each";

static NBR_type Continue_for_each;

THREAD_BEGIN_FRAME(For);
  THREAD_FRAME_SLOT(EXP, prc);
  THREAD_FRAME_SLOT(EXP, lst);
  THREAD_FRAME_SLOT(PAI, arg);
THREAD_END_FRAME(For);

static NIL_type continue_for_each_C(NIL_type)
  { For_type for_each_thread;
    EXP_type list_expression,
             procedure_expression,
             tail_call_expression,
             value_expression;
    PAI_type argument_pair,
             list_pair;
    TAG_type tag;
    THR_type thread;
    thread = Thread_Peek();
    tail_call_expression = Thread_Tail_Position();
    for_each_thread = (For_type)thread;
    list_expression = for_each_thread->lst;
    tag = Grammar_Tag(list_expression);
    switch (tag)
      { case NUL_tag:
          Thread_Zap();
          value_expression = Main_Get_Expression();
          Main_Set_Expression(value_expression);
          return;
        case PAI_tag:
          thread = Thread_Keep_C();
          for_each_thread = (For_type)thread;
          procedure_expression = for_each_thread->prc;
          list_pair            = for_each_thread->lst;
          argument_pair        = for_each_thread->arg;
          value_expression = list_pair->car;
          list_expression  = list_pair->cdr;
          for_each_thread->lst = list_expression;
          argument_pair->car = value_expression;
          if (!is_NUL(list_expression))
            tail_call_expression = Grammar_False;
          Evaluate_Apply_C(procedure_expression,
                           argument_pair,
                           tail_call_expression);
          return;
        default:
          native_error(NAL_error,
                       for_rawstring); }}

/*--------------------------------------------------------------------------------------*/

static NIL_type for_each_native_C(FRM_type Argument_frame,
                                  EXP_type Tail_call_expression)
  { For_type for_each_thread;
    EXP_type list_expression,
             procedure_expression,
             tail_call_expression,
             value_expression;
    PAI_type argument_pair,
             list_pair;
    TAG_type tag;
    THR_type thread;
    require_2_arguments(Argument_frame,
                        &procedure_expression,
                        &list_expression,
                        for_rawstring);
    tag = Grammar_Tag(list_expression);
    switch (tag)
      { case NUL_tag:
          Main_Set_Expression(Grammar_Unspecified);
          return;
        case PAI_tag:
          BEGIN_DOUBLE_PROTECT(procedure_expression,
                               list_expression);
            Thread_Push_C(Continue_for_each,
                          Tail_call_expression,
                          For_size);
            MAIN_CLAIM_DEFAULT_C();
          END_DOUBLE_PROTECT(procedure_expression,
                             list_pair);
          value_expression = list_pair->car;
          list_expression  = list_pair->cdr;
          argument_pair = make_PAI_M(value_expression,
                                     Grammar_Null);
          thread = Thread_Peek();
          for_each_thread = (For_type)thread;
          for_each_thread->prc = procedure_expression;
          for_each_thread->lst = list_expression;
          for_each_thread->arg = argument_pair;
          tail_call_expression = is_NUL(list_expression)
                                   ? Tail_call_expression
                                   : Grammar_False;
          Evaluate_Apply_C(procedure_expression,
                           argument_pair,
                           tail_call_expression);
          return;
        default:
          native_error(NAL_error,
                       for_rawstring); }}

/*--------------------------------------------------------------------------------------*/

static NIL_type initialize_for_each_native_M(NIL_type)
  { slipken_persist_init(Continue_for_each, Thread_Register(continue_for_each_C));
    native_define_M(for_rawstring,
                    for_each_native_C); }

/*---------------------------------------- map -----------------------------------------*/

static const RWS_type map_rawstring = "map";

static NBR_type Continue_map;

THREAD_BEGIN_FRAME(Map);
  THREAD_FRAME_SLOT(EXP, prc);
  THREAD_FRAME_SLOT(EXP, lst);
  THREAD_FRAME_SLOT(PAI, arg);
  THREAD_FRAME_SLOT(EXP, res);
THREAD_END_FRAME(Map);

static NIL_type continue_map_C(NIL_type)
  { Map_type map_thread;
    EXP_type list_expression,
             procedure_expression,
             result_expression,
             tail_call_expression,
             value_expression;
    PAI_type argument_pair,
             list_pair,
             result_pair;
    TAG_type tag;
    THR_type thread;
    MAIN_CLAIM_DEFAULT_C();
    thread = Thread_Peek();
    tail_call_expression = Thread_Tail_Position();
    map_thread = (Map_type)thread;
    list_expression   = map_thread->lst;
    result_expression = map_thread->res;
    value_expression = Main_Get_Expression();
    result_pair = make_PAI_M(value_expression,
                             result_expression);
    tag = Grammar_Tag(list_expression);
    switch (tag)
      { case NUL_tag:
          Thread_Zap();
          result_pair = Main_Reverse(result_pair);
          Main_Set_Expression(result_pair);
          return;
        case PAI_tag:
          map_thread->res = result_pair;
          thread = Thread_Keep_C();
          map_thread = (Map_type)thread;
          procedure_expression = map_thread->prc;
          list_pair            = map_thread->lst;
          argument_pair        = map_thread->arg;
          value_expression = list_pair->car;
          list_expression  = list_pair->cdr;
          map_thread->lst = list_expression;
          argument_pair->car = value_expression;
          if (!is_NUL(list_expression))
            tail_call_expression = Grammar_False;
          Evaluate_Apply_C(procedure_expression,
                           argument_pair,
                           tail_call_expression);
          return;
        default:
          native_error(NAL_error,
                       map_rawstring); }}

/*--------------------------------------------------------------------------------------*/

static NIL_type map_native_C(FRM_type Argument_frame,
                             EXP_type Tail_call_expression)
  { Map_type map_thread;
    EXP_type list_expression,
             procedure_expression,
             tail_call_expression,
             value_expression;
    PAI_type argument_pair,
             list_pair;
    TAG_type tag;
    THR_type thread;
    require_2_arguments(Argument_frame,
                        &procedure_expression,
                        &list_expression,
                        for_rawstring);
    tag = Grammar_Tag(list_expression);
    switch (tag)
      { case NUL_tag:
          Main_Set_Expression(Grammar_Null);
          return;
        case PAI_tag:
          BEGIN_DOUBLE_PROTECT(procedure_expression,
                               list_expression);
            Thread_Push_C(Continue_map,
                          Tail_call_expression,
                          Map_size);
            MAIN_CLAIM_DEFAULT_C();
          END_DOUBLE_PROTECT(procedure_expression,
                             list_pair);
          value_expression = list_pair->car;
          list_expression  = list_pair->cdr;
          argument_pair = make_PAI_M(value_expression,
                                     Grammar_Null);
          thread = Thread_Peek();
          map_thread = (Map_type)thread;
          map_thread->prc = procedure_expression;
          map_thread->lst = list_expression;
          map_thread->arg = argument_pair;
          map_thread->res = Grammar_Null;
          tail_call_expression = is_NUL(list_expression)
                                   ? Tail_call_expression
                                   : Grammar_False;
          Evaluate_Apply_C(procedure_expression,
                           argument_pair,
                           tail_call_expression);
          return;
        default:
          native_error(NAL_error,
                       map_rawstring); }}

/*--------------------------------------------------------------------------------------*/

static NIL_type initialize_map_native_M(NIL_type)
  { slipken_persist_init(Continue_map, Thread_Register(continue_map_C));
    native_define_M(map_rawstring,
                    map_native_C); }

/*--------------------------------------------------------------------------------------*/
/*--------------------------------------- input ----------------------------------------*/
/*--------------------------------------------------------------------------------------*/

/*---------------------------------------- read ----------------------------------------*/

static const RWS_type rdd_rawstring = "read";

static NIL_type read_native_C(FRM_type Argument_frame,
                              EXP_type Tail_call_expression)
  { EXP_type value_expression;
    STR_type name_string;
    RWS_type name_rawstring;
    UNS_type number_of_arguments;
    number_of_arguments = Environment_Frame_Size(Argument_frame);
    if (number_of_arguments > 1)
      native_error(AM1_error,
                   rdd_rawstring);
    if (number_of_arguments == 0)
      name_rawstring = Main_Void;
    else
      { name_string = Environment_Frame_Get(Argument_frame,
                                            1);
        Environment_Release_Frame(Argument_frame);
        if (!is_STR(name_string))
          native_error(NAS_error,
                       rdd_rawstring);
        name_rawstring = name_string->rws; }
    value_expression = Read_Parse_C(name_rawstring);
    Main_Set_Expression(value_expression); }

/*--------------------------------------------------------------------------------------*/

static NIL_type initialize_read_native_M(NIL_type)
  { native_define_M(rdd_rawstring,
                    read_native_C); }

/*--------------------------------------------------------------------------------------*/
/*--------------------------------------- output ---------------------------------------*/
/*--------------------------------------------------------------------------------------*/

/*--------------------------------------- display --------------------------------------*/

static const RWS_type dis_rawstring = "display";

static NIL_type display_native(FRM_type Argument_frame,
                               EXP_type Tail_call_expression)
  { EXP_type value_expression;
    UNS_type index,
             number_of_arguments;
    number_of_arguments = Environment_Frame_Size(Argument_frame);
    for (index = 1;
         index <= number_of_arguments;
         index += 1)
      { value_expression = Environment_Frame_Get(Argument_frame,
                                                 index);
        Print_Display(value_expression); }
    Main_Set_Expression(Grammar_Unspecified); }

/*--------------------------------------------------------------------------------------*/

static NIL_type initialize_display_native_M(NIL_type)
  { native_define_M(dis_rawstring,
                    display_native); }

/*--------------------------------------- newline --------------------------------------*/

static const RWS_type nwl_rawstring = "newline";

static NIL_type newline_native(FRM_type Argument_frame,
                               EXP_type Tail_call_expression)
  { require_0_arguments(Argument_frame,
                        nwl_rawstring);
    Print_Display(Grammar_Newline_String);
    Main_Set_Expression(Grammar_Unspecified); }

/*--------------------------------------------------------------------------------------*/

static NIL_type initialize_newline_native_M(NIL_type)
  { native_define_M(nwl_rawstring,
                    newline_native); }

/*--------------------------------------- pretty ---------------------------------------*/

static const RWS_type prt_rawstring = "pretty";

static NIL_type pretty_native_C(FRM_type Argument_frame,
                                EXP_type Tail_call_expression)
  { EXP_type compiled_expression,
             expression;
    require_1_argument(Argument_frame,
                       &expression,
                       prt_rawstring);
    compiled_expression = Compile_Compile_C(expression);
    Print_Print(compiled_expression);
    Main_Set_Expression(Grammar_Unspecified); }

/*--------------------------------------------------------------------------------------*/

static NIL_type initialize_pretty_native_M(NIL_type)
  { native_define_M(prt_rawstring,
                    pretty_native_C); }

/*--------------------------------------------------------------------------------------*/
/*-------------------------------------- service ---------------------------------------*/
/*--------------------------------------------------------------------------------------*/

/*--------------------------------------- clock ----------------------------------------*/

static const RWS_type clk_rawstring = "clock";

static RWL_type start_time;

static NIL_type clock_native(FRM_type Argument_frame,
                             EXP_type Tail_call_expression)
  { REA_type time_real;
    RWL_type ticks;
    RWR_type raw_time;
    require_0_arguments(Argument_frame,
                        clk_rawstring);
    ticks = clock();
    ticks -= start_time;
    raw_time = (RWR_type)ticks/CLOCKS_PER_SEC;
    time_real = make_REA_M(raw_time);
    Main_Set_Expression(time_real); }

/*--------------------------------------------------------------------------------------*/

static NIL_type initialize_clock_native_M(NIL_type)
  { slipken_persist_init(start_time, clock());
    native_define_M(clk_rawstring,
                    clock_native); }

/*-------------------------------------- collect ---------------------------------------*/

static const RWS_type col_rawstring = "collect";

static NIL_type collect_native_C(FRM_type Argument_frame,
                                 EXP_type Tail_call_expression)
  { require_0_arguments(Argument_frame,
                        col_rawstring);
    Main_Reclaim_C();
    Main_Set_Expression(Grammar_Unspecified); }

/*--------------------------------------------------------------------------------------*/

static NIL_type initialize_collect_native_M(NIL_type)
  { native_define_M(col_rawstring,
                    collect_native_C); }

/*---------------------------------------- error ---------------------------------------*/

static const RWS_type err_rawstring = "error";

static NIL_type error_native(FRM_type Argument_frame,
                             EXP_type Tail_call_expression)
  { EXP_type value_expression;
    RWS_type rawstring;
    STR_type string;
    require_1_argument(Argument_frame,
                       &value_expression,
                       err_rawstring);
    if (!is_STR(value_expression))
      native_error(NAS_error,
                   err_rawstring);
    string = value_expression;
    rawstring = string->rws;
    return native_error(USR_error,
                        rawstring); }

/*--------------------------------------------------------------------------------------*/

static NIL_type initialize_error_native_M(NIL_type)
  { native_define_M(err_rawstring,
                    error_native); }

/*--------------------------------------- memory ---------------------------------------*/

static const RWS_type mem_rawstring = "memory";

static NIL_type memory_native_C(FRM_type Argument_frame,
                                EXP_type Tail_call_expression)
  { DBL_type consumption;
    NBR_type collect_count_number;
    PAI_type consumption_pair;
    REA_type consumption_real;
    UNS_type collect_count;
    require_0_arguments(Argument_frame,
                        mem_rawstring);
    consumption = Memory_Consumption();
    collect_count = Memory_Collect_Count();
    collect_count_number = make_NBR(collect_count);
    MAIN_CLAIM_DEFAULT_C();
    consumption_real = make_REA_M(consumption);
    consumption_pair = make_PAI_M(consumption_real,
                                  collect_count_number);
    Main_Set_Expression(consumption_pair); }

/*--------------------------------------------------------------------------------------*/

static NIL_type initialize_memory_native_M(NIL_type)
  { native_define_M(mem_rawstring,
                    memory_native_C); }

/*--------------------------------------- random ---------------------------------------*/

static const RWS_type rnd_rawstring = "random";

static NIL_type random_native(FRM_type Argument_frame,
                              EXP_type Tail_call_expression)
  { NBR_type number;
    UNS_type random;
    require_0_arguments(Argument_frame,
                        rnd_rawstring);
    random = rand();
    random >>= 2;
    number = make_NBR(random);
    Main_Set_Expression(number); }

/*--------------------------------------------------------------------------------------*/

static NIL_type initialize_random_native_M(NIL_type)
  { native_define_M(rnd_rawstring,
                    random_native); }

/*--------------------------------------- ken-id ---------------------------------------*/

static const RWS_type kid_rawstring = "ken-id";
static KID_type my_ken_id;

static NIL_type ken_id_native(FRM_type Argument_frame,
                              EXP_type Tail_call_expression)
  { require_0_arguments(Argument_frame,
                        kid_rawstring);
    Main_Set_Expression(my_ken_id); }

/*--------------------------------------------------------------------------------------*/

static NIL_type initialize_ken_id_M(NIL_type)
  { RWK_type kid = ken_id();
    slipken_persist_init(my_ken_id, make_KID_M(kid));
    native_define_M(kid_rawstring,
                    ken_id_native); }

/*-------------------------------------- ken-send --------------------------------------*/

static const RWS_type ksd_rawstring = "ken-send";

static NIL_type ken_send_native(FRM_type Argument_frame,
                                EXP_type Tail_call_expression)
  { EXP_type argument_expression, kid_expression;
    KID_type kid;
    STR_type argument;
    RWS_type rawstring;
    require_2_arguments(Argument_frame,
                        &kid_expression,
                        &argument_expression,
                        ksd_rawstring);
    if (!is_KID(kid_expression))
      native_error(NAK_error,
                   ksd_rawstring);
    if (!is_STR(argument_expression))
      native_error(NAS_error,
                   ksd_rawstring);
    argument = argument_expression;
    kid = kid_expression;
    rawstring = argument->rws;
    (void) ken_send(kid->rwk, rawstring, strlen(rawstring));
    Main_Set_Expression(Grammar_Unspecified); }

/*--------------------------------------------------------------------------------------*/

static NIL_type initialize_ken_send_M(NIL_type)
  { native_define_M(ksd_rawstring,
                    ken_send_native); }

/*---------------------------------- ken-receive-handler -------------------------------*/

static const RWS_type krh_rawstring = "ken-receive-handler";
static UNS_type Receive_handler_offset;

static NIL_type default_receive_handler(FRM_type Argument_frame,
                                        EXP_type Tail_call_expression)
  { EXP_type argument_expression, kid_expression;
    KID_type kid;
    RWS_type rawstring;
    require_2_arguments(Argument_frame,
                        &kid_expression,
                        &argument_expression,
                        krh_rawstring);
    if (!is_KID(kid_expression))
      native_error(NAK_error,
                   krh_rawstring);

    kid = (KID_type) kid_expression;
    Slip_Print("Received from ");
    Print_Display(kid);
    Slip_Print(": ");
    Print_Print(argument_expression);
    Slip_Print("\n");

    Main_Set_Expression(Grammar_Unspecified); }

static NIL_type initialize_ken_receive_handler_M(NIL_type)
  { SYM_type symbol;
    UNS_type offset;
    symbol = Pool_Enter_Native_M(krh_rawstring);
    slipken_persist_init(Receive_handler_offset,
                         offset);
    native_define_M(krh_rawstring,
                    default_receive_handler); }

/*--------------------------------------------------------------------------------------*/

NIL_type Native_Receive_Ken_Message(RWK_type sender, EXP_type msg)
  { KID_type kid;
    PAI_type arguments;
    EXP_type procedure_expression;
    kid = make_KID_M(sender);
    arguments = make_PAI_M(msg, Grammar_Null);
    arguments = make_PAI_M(kid, arguments);
    procedure_expression = Environment_Global_Frame_Get(Receive_handler_offset);
    Evaluate_Apply_C(procedure_expression,
                     arguments,
                     Grammar_True);
}

NIL_type Native_Initialize_M(NIL_type)
  { initialize_circularity_level_M();
    initialize_add_native_M();
    initialize_apply_native_M();
    initialize_assoc_native_M();
    initialize_call_with_current_continuation_native_M();
    initialize_car_native_M();
    initialize_cdr_native_M();
    initialize_cos_native_M();
    initialize_char_to_integer_native_M();
    initialize_clock_native_M();
    initialize_collect_native_M();
    initialize_cons_native_M();
    initialize_display_native_M();
    initialize_divide_native_M();
    initialize_equal_native_M();
    initialize_equivalent_native_M();
    initialize_exp_native_M();
    initialize_expt_native_M();
    initialize_error_native_M();
    initialize_eval_native_M();
    initialize_for_each_native_M();
    initialize_force_native_M();
    initialize_greater_native_M();
    initialize_greater_or_equal_native_M();
    initialize_integer_to_char_native_M();
    initialize_is_boolean_native_M();
    initialize_is_char_native_M();
    initialize_is_integer_native_M();
    initialize_is_null_native_M();
    initialize_is_number_native_M();
    initialize_is_pair_native_M();
    initialize_is_procedure_native_M();
    initialize_is_real_native_M();
    initialize_is_string_native_M();
    initialize_is_symbol_native_M();
    initialize_is_vector_native_M();
    initialize_length_native_M();
    initialize_less_native_M();
    initialize_less_or_equal_native_M();
    initialize_list_native_M();
    initialize_log_native_M();
    initialize_make_vector_native_M();
    initialize_map_native_M();
    initialize_memory_native_M();
    initialize_multiply_native_M();
    initialize_newline_native_M();
    initialize_not_native_M();
    initialize_pretty_native_M();
    initialize_quotient_native_M();
    initialize_random_native_M();
    initialize_read_native_M();
    initialize_remainder_native_M();
    initialize_set_car_native_M();
    initialize_set_cdr_native_M();
    initialize_sin_native_M();
    initialize_sqrt_native_M();
    initialize_string_length_M();
    initialize_string_ref_native_M();
    initialize_string_set_native_M();
    initialize_string_to_symbol_native_M();
    initialize_symbol_to_string_native_M();
    initialize_subtract_native_M();
    initialize_tan_native_M();
    initialize_vector_length_native_M();
    initialize_vector_native_M();
    initialize_vector_ref_native_M();
    initialize_vector_set_native_M();

    /* Ken primitives */
    initialize_ken_id_M();
    initialize_ken_send_M();
    initialize_ken_receive_handler_M(); }
