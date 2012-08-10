                     /*---------------------------------------------*/
                     /*                  >>>Slip<<<                 */
                     /*                 Theo D'Hondt                */
                     /*          VUB Software Languages Lab         */
                     /*                  (c) 2012                   */
                     /*---------------------------------------------*/
                     /*   version 13: completion and optimization   */
                     /*---------------------------------------------*/
                     /*                   scanner                   */
                     /*---------------------------------------------*/

#include <stdlib.h>
#include <string.h>

#include "SlipMain.h"

#include "SlipScan.h"

/*------------------------------------- private types ----------------------------------*/

typedef enum { Apo,                                      /* '                           */
               Att,                                      /* @                           */
               Bkq,                                      /* `                           */
               Bks,                                      /* \                           */
               Com,                                      /* ,                           */
               Dgt,                                      /* 0 1 2 3 4 5 6 7 8 9         */
               Eol,                                      /*                             */
               Exp,                                      /* e E                         */
               Fls,                                      /* f F                         */
               Hsh,                                      /* #                           */
               Ill,                                      /*                             */
               Opr,                                      /* ! $ % & * / : < = > ? ^ _ ~ */
               Lpr,                                      /* (                           */
               Ltr,                                      /* a A b B ... z Z             */
               Mns,                                      /* -                           */
               Per,                                      /* .                           */
               Pls,                                      /* +                           */
               Quo,                                      /* "                           */
               Rpr,                                      /* )                           */
               Smc,                                      /* ;                           */
               Tru,                                      /* t T                         */
               Wsp } Cat_type;                           /* <HT> <FF> <SP>              */

typedef enum { att_allowed = (1<<Att),
               bks_allowed = (1<<Bks),
               del_allowed = (1<<Eol)|
                             (1<<Lpr)|
                             (1<<Quo)|
                             (1<<Rpr)|
                             (1<<Smc)|
                             (1<<Wsp),
               dig_allowed = (1<<Dgt),
               end_allowed = (1<<Eol),
               esc_allowed = (1<<Bks)|
                             (1<<Quo),
               exp_allowed = (1<<Exp),
               fls_allowed = (1<<Fls),
               idt_allowed = (1<<Dgt)|
                             (1<<Exp)|
                             (1<<Fls)|
                             (1<<Ltr)|
                             (1<<Mns)|
                             (1<<Opr)|
                             (1<<Pls)|
                             (1<<Tru),
               lpr_allowed = (1<<Lpr),
               per_allowed = (1<<Per),
               quo_allowed = (1<<Quo),
               sgn_allowed = (1<<Mns)|
                             (1<<Pls),
               trm_allowed = (1<<Eol)|
                             (1<<Quo),
               tru_allowed = (1<<Tru),
               wsp_allowed = (1<<Wsp) } All_type;

/*----------------------------------- private constants --------------------------------*/

enum { Buffer_size = 1024 };

static const RWS_type Newline_char_rawstring = "\n";
static const RWS_type Newline_text_rawstring = "newline";
static const RWS_type Space_char_rawstring   = " ";
static const RWS_type Space_text_rawstring   = "space";

static const Cat_type ASCII_table[] =
      /*NUL SOH STX ETX EOT ENQ ACK BEL BS  HT  LF  VT  FF  CR  SO  SI */
      { Ill,Ill,Ill,Ill,Ill,Ill,Ill,Ill,Ill,Wsp,Eol,Ill,Wsp,Eol,Ill,Ill,
      /*DLE DC1 DC2 DC3 DC4 NAK SYN ETB CAN EM  SUB ESC FS  GS  RS  US */
        Ill,Ill,Ill,Ill,Ill,Ill,Ill,Ill,Ill,Ill,Ill,Ill,Ill,Ill,Ill,Ill,
      /*     !   "   #   $   %   &   '   (   )   *   +   ,   -   .   / */
        Wsp,Opr,Quo,Hsh,Opr,Opr,Opr,Apo,Lpr,Rpr,Opr,Pls,Com,Mns,Per,Opr,
      /* 0   1   2   3   4   5   6   7   8   9   :   ;   <   =   >   ? */
        Dgt,Dgt,Dgt,Dgt,Dgt,Dgt,Dgt,Dgt,Dgt,Dgt,Opr,Smc,Opr,Opr,Opr,Opr,
      /* @   A   B   C   D   E   F   G   H   I   J   K   L   M   N   O */
        Att,Ltr,Ltr,Ltr,Ltr,Exp,Fls,Ltr,Ltr,Ltr,Ltr,Ltr,Ltr,Ltr,Ltr,Ltr,
      /* P   Q   R   S   T   U   V   W   X   Y   Z   [   \   ]   ^   _ */
        Ltr,Ltr,Ltr,Ltr,Tru,Ltr,Ltr,Ltr,Ltr,Ltr,Ltr,Ill,Bks,Ill,Opr,Opr,
      /* `   a   b   c   d   e   f   g   h   i   j   k   l   m   n   o */
        Bkq,Ltr,Ltr,Ltr,Ltr,Exp,Fls,Ltr,Ltr,Ltr,Ltr,Ltr,Ltr,Ltr,Ltr,Ltr,
      /* p   q   r   s   t   u   v   w   x   y   z   {   |   }   ~  DEL*/
        Ltr,Ltr,Ltr,Ltr,Tru,Ltr,Ltr,Ltr,Ltr,Ltr,Ltr,Ill,Ill,Ill,Opr,Ill,
      /*                                                               */
        Ill,Ill,Ill,Ill,Ill,Ill,Ill,Ill,Ill,Ill,Ill,Ill,Ill,Ill,Ill,Ill,
      /*                                                               */
        Ill,Ill,Ill,Ill,Ill,Ill,Ill,Ill,Ill,Ill,Ill,Ill,Ill,Ill,Ill,Ill,
      /*                                                               */
        Ill,Ill,Ill,Ill,Ill,Ill,Ill,Ill,Ill,Ill,Ill,Ill,Ill,Ill,Ill,Ill,
      /*                                                               */
        Ill,Ill,Ill,Ill,Ill,Ill,Ill,Ill,Ill,Ill,Ill,Ill,Ill,Ill,Ill,Ill,
      /*                                                               */
        Ill,Ill,Ill,Ill,Ill,Ill,Ill,Ill,Ill,Ill,Ill,Ill,Ill,Ill,Ill,Ill,
      /*                                                               */
        Ill,Ill,Ill,Ill,Ill,Ill,Ill,Ill,Ill,Ill,Ill,Ill,Ill,Ill,Ill,Ill,
      /*                                                               */
        Ill,Ill,Ill,Ill,Ill,Ill,Ill,Ill,Ill,Ill,Ill,Ill,Ill,Ill,Ill,Ill,
      /*                                                               */
        Ill,Ill,Ill,Ill,Ill,Ill,Ill,Ill,Ill,Ill,Ill,Ill,Ill,Ill,Ill,Ill };

/*----------------------------------- private variables --------------------------------*/

static RWC_type Current_character;
static UNS_type Cursor;

/*----------------------------------- public variables ---------------------------------*/

RWC_type Scan_String[Buffer_size];

/*----------------------------------- private prototypes -------------------------------*/

static NIL_type      copy_char(RWC_type);
static NIL_type next_character(NIL_type);

/*----------------------------------- private functions --------------------------------*/

static BYT_type check(All_type Allowed)
  { return (Allowed >> ASCII_table[Current_character]) & 1; }

static NIL_type copy_and_get_char(NIL_type)
  { copy_char(Current_character);
    next_character(); }

static NIL_type copy_and_get_until(All_type Allowed)
  { do
      copy_and_get_char();
    while (!check(Allowed)); }

static NIL_type copy_and_get_while(All_type Allowed)
  { do
      copy_and_get_char();
    while (check(Allowed)); }

static NIL_type copy_char(RWC_type Rawcharacter)
  { if (Cursor == Buffer_size)
      Scan_Error(BFO_error);
    Scan_String[Cursor] = Rawcharacter;
    Cursor += 1; }

static SCA_type error_and_return(BYT_type Error)
  { Scan_Error(Error);
    return EOL_token; }

static NIL_type get_until(All_type Allowed)
  { do
      next_character();
    while (!check(Allowed)); }

static NIL_type get_while(All_type Allowed)
  { do
      next_character();
    while (check(Allowed)); }

static NIL_type next_character(NIL_type)
  { Current_character = Slip_Read();
    return; }

static SCA_type next_character_return(SCA_type Token)
  { next_character();
    return Token; }

static BYT_type reserved_name(RWS_type rawstring)
  { return (strcmp(Scan_String,
                   rawstring) == 0); }

static NIL_type stop_copy_text(NIL_type)
  { copy_char(0); }

static SCA_type stop_copy_text_return(SCA_type Token)
  { stop_copy_text();
    return Token; }

/*--------------------------------------------------------------------------------------*/

static SCA_type character(NIL_type)
  { next_character();
    copy_and_get_until(del_allowed);
    stop_copy_text();
    if (reserved_name(Space_text_rawstring))
      strcpy(Scan_String,
             Space_char_rawstring);
    else if (reserved_name(Newline_text_rawstring))
      strcpy(Scan_String,
             Newline_char_rawstring);
    else if (strlen(Scan_String) != 1)
      return error_and_return(ICL_error);
    return CHA_token; }

static NIL_type integer(NIL_type)
  { if (!check(dig_allowed))
      Scan_Error(DRQ_error);
    copy_and_get_while(dig_allowed); }

static SCA_type number(NIL_type)
  { SCA_type token;
    token = NBR_token;
    copy_and_get_while(dig_allowed);
    if (check(per_allowed))
      { token = REA_token;
        copy_and_get_char();
        integer(); }
    if (check(exp_allowed))
      { token = REA_token;
        copy_and_get_char();
        if (check(sgn_allowed))
          copy_and_get_char();
        integer(); }
    return stop_copy_text_return(token); }

/*------------------------------------ scan functions ----------------------------------*/

static SCA_type Apo_fun(NIL_type)
  { return next_character_return(QUO_token); }

static SCA_type Hsh_fun(NIL_type)
  { next_character();
    if (check(tru_allowed))
      return next_character_return(TRU_token);
    if (check(fls_allowed))
      return next_character_return(FLS_token);
    if (check(bks_allowed))
      return character();
    if (check(lpr_allowed))
      return next_character_return(VEC_token);
    return error_and_return(ILH_error); }

static SCA_type Idt_fun(NIL_type)
  { copy_and_get_while(idt_allowed);
    return stop_copy_text_return(IDT_token); }

static SCA_type Ill_fun(NIL_type)
  { return error_and_return(ILC_error); }

static SCA_type Lpr_fun(NIL_type)
  { return next_character_return(LPR_token); }

static SCA_type Nbr_fun(NIL_type)
  { return number(); }

static SCA_type Eol_fun(NIL_type)
  { return EOL_token; }

static SCA_type Per_fun(NIL_type)
  { return next_character_return(PER_token); }

static SCA_type Qqu_fun(NIL_type)
  { return next_character_return(QQU_token); }

static SCA_type Rpr_fun(NIL_type)
  { return next_character_return(RPR_token); }

static SCA_type Sgn_fun(NIL_type)
  { copy_and_get_char();
    if (check(dig_allowed))
      return number();
    while (check(idt_allowed))
      copy_and_get_char();
    return stop_copy_text_return(IDT_token); }

static SCA_type Smc_fun(NIL_type)
  { get_until(end_allowed);
    next_character();
    return Scan_Next(); }

static SCA_type Str_fun(NIL_type)
  { next_character();
    while (!check(trm_allowed))
      copy_and_get_char();
    stop_copy_text();
    if (!check(quo_allowed))
      return error_and_return(QRQ_error);
    return next_character_return(STR_token); }

static SCA_type Unq_fun(NIL_type)
  { next_character();
    if (check(att_allowed))
      return next_character_return(UQS_token);
    return UNQ_token; }

static SCA_type Wsp_fun(NIL_type)
  { get_while(wsp_allowed);
    return Scan_Next(); }

static SCA_type next_token(NIL_type)
  { Cat_type category;
    Cursor = 0;
    Scan_String[0] = 0;
    category = ASCII_table[Current_character];
    switch (category)
      { case Apo:
          return Apo_fun();
        case Att:
          return Ill_fun();
        case Bkq:
          return Qqu_fun();
        case Bks:
          return Ill_fun();
        case Com:
          return Unq_fun();
        case Dgt:
          return Nbr_fun();
        case Eol:
          return Eol_fun();
        case Exp:
          return Idt_fun();
        case Fls:
          return Idt_fun();
        case Hsh:
          return Hsh_fun();
        case Ill:
          return Ill_fun();
        case Opr:
          return Idt_fun();
        case Lpr:
          return Lpr_fun();
        case Ltr:
          return Idt_fun();
        case Mns:
          return Sgn_fun();
        case Per:
          return Per_fun();
        case Pls:
          return Sgn_fun();
        case Quo:
          return Str_fun();
        case Rpr:
          return Rpr_fun();
        case Smc:
          return Smc_fun();
        case Tru:
          return Idt_fun();
        case Wsp:
          return Wsp_fun();
        default:
          return Eol_fun(); }}

/*----------------------------------- public functions ---------------------------------*/

SCA_type Scan_Next(NIL_type)
  { return next_token(); }

SCA_type Scan_Preset(NIL_type)
  { next_character();
    return next_token(); }

/*--------------------------------------------------------------------------------------*/

NIL_type Scan_Initialize(NIL_type)
  { return; }
