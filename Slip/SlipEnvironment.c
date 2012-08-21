                     /*---------------------------------------------*/
                     /*                  >>>Slip<<<                 */
                     /*                 Theo D'Hondt                */
                     /*          VUB Software Languages Lab         */
                     /*                  (c) 2012                   */
                     /*---------------------------------------------*/
                     /*   version 13: completion and optimization   */
                     /*---------------------------------------------*/
                     /*                 environment                 */
                     /*---------------------------------------------*/

#include "SlipMain.h"
#include "SlipPersist.h"

#include "SlipCache.h"
#include "SlipEnvironment.h"

/*--------------------------------------------------------------------------------------*/
/*----------------------------------- frame manipulation -------------------------------*/
/*--------------------------------------------------------------------------------------*/

/*------------------------------------ private macros ----------------------------------*/

#define EMPTY_FRAME (FRM_type)Grammar_Empty_Vector

/*----------------------------------- private constants --------------------------------*/

enum { Initial_global_frame_size = 128,
       Global_frame_increment    = 32 };

/*-------------------------------------- opaque types ----------------------------------*/

typedef
  struct FRM { EXP_type exp[1]; } FRM;

/*----------------------------------- private variables --------------------------------*/

static FRM_type Current_frame;
static FRM_type Global_frame;
static VEC_type Global_symbol_vector;

/*----------------------------------- private functions --------------------------------*/

static EXP_type frame_get(FRM_type Frame,
                          UNS_type Offset)
  { EXP_type value_expression;
    value_expression = Frame->exp[Offset];
    return value_expression; }

static NIL_type frame_set(FRM_type Frame,
                          UNS_type Offset,
                          EXP_type Value_expression)
  { Frame->exp[Offset] = Value_expression; }

static UNS_type size_frame(FRM_type Frame)
  { UNS_type size;
    VEC_type frame_vector;
    frame_vector = (VEC_type)Frame;
    size = size_VEC(frame_vector);
    return size; }

/*----------------------------------- public functions ---------------------------------*/

NIL_type Environment_Adjust_Global_Frame_C(UNS_type Global_size)
  { UNS_type adjusted_size,
             increment,
             size;
    VEC_type global_frame,
             global_symbol_vector;
    size = size_frame(Global_frame);
    if (Global_size > size)
      { Slip_Log("extend global frame");
        adjusted_size = Global_size + Global_frame_increment;
        increment = adjusted_size - size;
        adjusted_size *= 2;
        MAIN_CLAIM_DYNAMIC_C(adjusted_size);
        global_frame = (VEC_type)Global_frame;
        global_symbol_vector = Global_symbol_vector;
        Main_Reallocate_Vector_M(&global_frame,
                                 increment);
        Main_Reallocate_Vector_M(&global_symbol_vector,
                                 increment);
        Main_Replace_Vector(Global_symbol_vector,
                            global_symbol_vector);
        Main_Replace_Vector((VEC_type)Global_frame,
                            global_frame); }}

FRM_type Environment_Empty_Frame(NIL_type)
  { return EMPTY_FRAME; }

EXP_type Environment_Frame_Get(FRM_type Frame,
                               UNS_type Offset)
  { EXP_type value_expression;
    value_expression = frame_get(Frame,
                                 Offset);
    return value_expression; }

NIL_type Environment_Frame_Set(FRM_type Frame,
                               UNS_type Offset,
                               EXP_type Value_expression)
  { frame_set(Frame,
              Offset,
              Value_expression); }

UNS_type Environment_Frame_Size(FRM_type Frame)
  { UNS_type size;
    size = size_frame(Frame);
    return size; }

FRM_type Environment_Get_Current_Frame(NIL_type)
  { return Current_frame; }

EXP_type Environment_Global_Frame_Get(UNS_type Offset)
  { return frame_get(Global_frame,
                     Offset); }

NIL_type Environment_Global_Frame_Set(EXP_type Value_expression,
                                      UNS_type Offset)
  { frame_set(Global_frame,
              Offset,
              Value_expression); }

SYM_type Environment_Global_Symbol_Name(UNS_type Offset)
  { SYM_type global_symbol;
    global_symbol = Global_symbol_vector[Offset];
    return global_symbol; }

VEC_type Environment_Global_Symbol_Vector(NIL_type)
  { return Global_symbol_vector; }

EXP_type Environment_Local_Get(NBR_type Offset_number)
  { EXP_type value_expression;
    UNS_type offset;
    offset = get_NBR(Offset_number);
    value_expression = frame_get(Current_frame,
                                 offset);
    return value_expression; }

NIL_type Environment_Local_Set(EXP_type Value_expression,
                               NBR_type Offset_number)
  { UNS_type offset;
    offset = get_NBR(Offset_number);
    frame_set(Current_frame,
              offset,
              Value_expression); }

FRM_type Environment_Make_Frame_C(UNS_type Size)
  { FRM_type frame;
    VEC_type frame_vector;
    frame_vector = Cache_Make_Vector_C(Size);
    frame = (FRM_type)frame_vector;
    return frame; }

FRM_type Environment_Mark_Frame(NIL_type)
  { apply_MRK(Current_frame);
    return Current_frame; }

NIL_type Environment_Release_Current_Frame(NIL_type)
  { VEC_type frame_vector;
    frame_vector = (VEC_type)Current_frame;
    Cache_Release_Vector(frame_vector);
    Current_frame = EMPTY_FRAME; }

NIL_type Environment_Release_Frame(FRM_type Frame)
  { VEC_type frame_vector;
    frame_vector = (VEC_type)Frame;
    Cache_Release_Vector(frame_vector); }

/*--------------------------------------------------------------------------------------*/

static NIL_type initialize_frames_M(NIL_type)
  { VEC_type global_frame_vector;
    global_frame_vector = make_VEC_M(Initial_global_frame_size);
    slipken_persist_init(Global_frame, (FRM_type)global_frame_vector);
    slipken_persist_init(Global_symbol_vector, make_VEC_M(Initial_global_frame_size));
    apply_MRK(Global_frame);
    slipken_persist_init(Current_frame, Global_frame);
    MAIN_REGISTER(Current_frame);
    MAIN_REGISTER(Global_frame);
    MAIN_REGISTER(Global_symbol_vector); }

/*--------------------------------------------------------------------------------------*/
/*-------------------------------- environment manipulation ----------------------------*/
/*--------------------------------------------------------------------------------------*/

/*------------------------------------ private macros ----------------------------------*/

#define EMPTY_ENVIRONMENT (ENV_type)Grammar_Empty_Vector

/*-------------------------------------- opaque types ----------------------------------*/

typedef
  struct ENV { FRM_type frm[1]; } ENV;

/*----------------------------------- private variables --------------------------------*/

static ENV_type Current_environment;

/*----------------------------------- private functions --------------------------------*/

static FRM_type get_frame(ENV_type Environment,
                          UNS_type Scope)
  { FRM_type frame;
    frame = Environment->frm[Scope];
    return frame; }

static NIL_type set_frame(ENV_type Environment,
                          UNS_type Scope,
                          FRM_type Frame)
  { Environment->frm[Scope] = Frame; }

static UNS_type size_environment(ENV_type Environment)
  { UNS_type size;
    VEC_type environment_vector;
    environment_vector = (VEC_type)Environment;
    size = size_VEC(environment_vector);
    return size; }

/*----------------------------------- public functions ---------------------------------*/

ENV_type Environment_Extend_C(NIL_type)
  { ENV_type extended_environment;
    FRM_type frame;
    UNS_type scope,
             size;
    VEC_type extended_environment_vector;
    size = size_environment(Current_environment);
    size += 1;
    extended_environment_vector = Cache_Make_Vector_C(size);
    extended_environment = (ENV_type)extended_environment_vector;
    for (scope = 1;
         scope < size;
         scope += 1)
      { frame = get_frame(Current_environment,
                          scope);
        set_frame(extended_environment,
                  scope,
                  frame); }
    apply_MRK(Current_frame);
    set_frame(extended_environment,
              size,
              Current_frame);
    return extended_environment; }

ENV_type Environment_Get_Current_Environment(NIL_type)
  { return Current_environment; }

UNS_type Environment_Get_Current_Environment_Size(NIL_type)
  { UNS_type size;
    size = size_environment(Current_environment);
    return size; }

EXP_type Environment_Global_Get(NBR_type Scope_number,
                                NBR_type Offset_number)
  { EXP_type value_expression;
    FRM_type frame;
    UNS_type offset,
             scope;
    scope = get_NBR(Scope_number);
    offset = get_NBR(Offset_number);
    frame = get_frame(Current_environment,
                      scope);
    value_expression = frame_get(frame,
                                 offset);
    return value_expression; }

NIL_type Environment_Global_Set(EXP_type Value_expression,
                                NBR_type Scope_number,
                                NBR_type Offset_number)
  { UNS_type offset,
             scope;
    FRM_type frame;
    scope = get_NBR(Scope_number);
    offset = get_NBR(Offset_number);
    frame = get_frame(Current_environment,
                      scope);
    frame_set(frame,
              offset,
              Value_expression); }

NIL_type Environment_Replace_Environment(ENV_type Environment,
                                         FRM_type Frame)
  { Current_environment = Environment;
    Current_frame = Frame; }

NIL_type Environment_Reset(NIL_type)
  { Current_environment = EMPTY_ENVIRONMENT;
    Current_frame = Global_frame; }

/*--------------------------------------------------------------------------------------*/

static NIL_type initialize_environments(NIL_type)
  { slipken_persist_init(Current_environment, EMPTY_ENVIRONMENT);
    MAIN_REGISTER(Current_environment); }

/*--------------------------------------------------------------------------------------*/

NIL_type Environment_Initialize_M(NIL_type)
  { initialize_frames_M();
    initialize_environments(); }
