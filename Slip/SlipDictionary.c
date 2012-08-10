                     /*---------------------------------------------*/
                     /*                  >>>Slip<<<                 */
                     /*                 Theo D'Hondt                */
                     /*          VUB Software Languages Lab         */
                     /*                  (c) 2012                   */
                     /*---------------------------------------------*/
                     /*   version 13: completion and optimization   */
                     /*---------------------------------------------*/
                     /*                 dictionary                  */
                     /*---------------------------------------------*/

#include "SlipMain.h"

#include "SlipDictionary.h"
#include "SlipPersist.h"

/*------------------------------------ private macros ----------------------------------*/

#define NULL_DICTIONARY (Dct_type)Grammar_Null
#define NULL_FRAME      (Frm_type)Grammar_Null

/*------------------------------------ Frm delarations ---------------------------------*/

DEFINE_GRAMMAR_FRAME(Frm);

BEGIN_GRAMMAR_FRAME(Frm);
  DEFINE_GRAMMAR_SLOT(SYM, sym);
  DEFINE_GRAMMAR_SLOT(Frm, frm);
END_GRAMMAR_FRAME(Frm);

static Frm_type make_frame_M(SYM_type Symbol,
                             Frm_type Frame_vector)
  { Frm_type frame;
    VEC_type vector;
    vector = make_VEC_Z(Frm_size);
    frame = (Frm_type)vector;
    frame->sym = Symbol;
    frame->frm = Frame_vector;
    return frame; }

/*------------------------------------ Dct delarations ---------------------------------*/

DEFINE_GRAMMAR_FRAME(Dct);

BEGIN_GRAMMAR_FRAME(Dct);
  DEFINE_GRAMMAR_SLOT(Frm, frm);
  DEFINE_GRAMMAR_SLOT(NBR, frs);
  DEFINE_GRAMMAR_SLOT(Dct, dct);
END_GRAMMAR_FRAME(Dct);

static Dct_type make_dictionary_M(Frm_type Frame_vector,
                                  NBR_type Frame_vector_size_number,
                                  Dct_type Dictionary)
  { Dct_type dictionary;
    VEC_type vector;
    vector = make_VEC_Z(Dct_size);
    dictionary = (Dct_type)vector;
    dictionary->frm = Frame_vector;
    dictionary->frs = Frame_vector_size_number;
    dictionary->dct = Dictionary;
    return dictionary; }

/*----------------------------------- private variables --------------------------------*/

static Dct_type Current_dictionary;
static Frm_type Current_frame;
static Frm_type Global_frame;

static UNS_type Current_frame_size;
static UNS_type Current_scope_level;
static UNS_type Global_frame_size;

/*----------------------------------- private functions --------------------------------*/

static UNS_type get_offset(SYM_type Symbol,
                           UNS_type Size,
                           Frm_type Frame_vector)
  { Frm_type frame;
    UNS_type offset;
    offset = Size;
    for (frame = Frame_vector;
         !is_NUL(frame);
         frame = frame->frm)
      { if (frame->sym == Symbol)
          return offset;
        offset -= 1; }
    return 0; }

/*----------------------------------- public functions ---------------------------------*/

NIL_type Dictionary_Checkpoint(NIL_type)
  { Global_frame = Current_frame;
    Global_frame_size = Current_frame_size; }

UNS_type Dictionary_Define_M(SYM_type Symbol)
  { UNS_type offset;
    offset = get_offset(Symbol,
                        Current_frame_size,
                        Current_frame);
    if (offset > 0)
      return offset;
    Current_frame = make_frame_M(Symbol,
                                 Current_frame);
    Current_frame_size += 1;
    return Current_frame_size; }

NIL_type Dictionary_Enter_Nested_Scope_C(NIL_type)
  { NBR_type frame_size_number;
    MAIN_CLAIM_DEFAULT_C();
    frame_size_number = make_NBR(Current_frame_size);
    Current_dictionary = make_dictionary_M(Current_frame,
                                           frame_size_number,
                                           Current_dictionary);
    Current_frame = NULL_FRAME;
    Current_frame_size = 0;
    Current_scope_level += 1; }

NIL_type Dictionary_Exit_Nested_Scope(NIL_type)
  { NBR_type frame_size_number;
    frame_size_number  = Current_dictionary->frs;
    Current_frame      = Current_dictionary->frm;
    Current_dictionary = Current_dictionary->dct;
    Current_scope_level -= 1;
    Current_frame_size = get_NBR(frame_size_number); }

UNS_type Dictionary_Get_Frame_Size(NIL_type)
  { return Current_frame_size; }

UNS_type Dictionary_Initially_Define_M(SYM_type Symbol)
  { Current_frame = make_frame_M(Symbol,
                                 Current_frame);
    Current_frame_size += 1;
    return Current_frame_size; }

UNS_type Dictionary_Lexical_Address_M(SYM_type   Symbol,
                                      UNS_type * Scope_reference)
  { Dct_type dictionary,
             next_dictionary;
    Frm_type frame;
    NBR_type frame_size_number;
    UNS_type frame_size,
             offset,
             scope;
    offset = get_offset(Symbol,
                        Current_frame_size,
                        Current_frame);
    *Scope_reference = 0;
    if (offset > 0)
      return offset;
    if (Current_scope_level == 0)
      return 0;
    next_dictionary = Current_dictionary;
    for (scope = Current_scope_level;
         scope > 0;
         scope -= 1)
      { dictionary = next_dictionary;
        frame_size_number = dictionary->frs;
        frame = dictionary->frm;
        frame_size = get_NBR(frame_size_number);
        offset = get_offset(Symbol,
                            frame_size,
                            frame);
        if (offset > 0)
          { *Scope_reference = scope;
            return offset; }
        next_dictionary = dictionary->dct; }
    frame = make_frame_M(Symbol,
                         frame);
    frame_size += 1;
    frame_size_number = make_NBR(frame_size);
    dictionary->frm = frame;
    dictionary->frs = frame_size_number;
    *Scope_reference = 1;
    return frame_size; }

NIL_type Dictionary_Rollback_C(NIL_type)
  { NBR_type frame_size_number;
    Current_frame = Global_frame;
    Current_frame_size = Global_frame_size;
    frame_size_number = make_NBR(Current_frame_size);
    MAIN_CLAIM_DEFAULT_C();
    Current_dictionary = make_dictionary_M(Current_frame,
                                           frame_size_number,
                                           NULL_DICTIONARY);
    Current_scope_level = 0; }

NIL_type Dictionary_Setup_Symbol_Vector(VEC_type Symbol_vector)
  { Frm_type frame;
    SYM_type symbol;
    UNS_type index;
    index = Current_frame_size;
    for (frame = Current_frame;
         !is_NUL(frame);
         frame = frame->frm)
      { symbol = frame->sym;
        Symbol_vector[index] = symbol;
        index -= 1; }}

/*--------------------------------------------------------------------------------------*/

NIL_type Dictionary_Initialize(NIL_type)
  { slipken_persist_init(Current_dictionary, NULL_DICTIONARY);
    slipken_persist_init(Current_scope_level, 0);
    slipken_persist_init(Current_frame, NULL_FRAME);
    slipken_persist_init(Global_frame, NULL_FRAME);
    slipken_persist_init(Current_frame_size, 0);
    slipken_persist_init(Global_frame_size, 0);
    MAIN_REGISTER(Current_dictionary);
    MAIN_REGISTER(Current_frame);
    MAIN_REGISTER(Global_frame); }
