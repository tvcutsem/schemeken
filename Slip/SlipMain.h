                     /*---------------------------------------------*/
                     /*                  >>>Slip<<<                 */
                     /*                 Theo D'Hondt                */
                     /*          VUB Software Languages Lab         */
                     /*                  (c) 2012                   */
                     /*---------------------------------------------*/
                     /*   version 13: completion and optimization   */
                     /*---------------------------------------------*/
                     /*                    main                     */
                     /*---------------------------------------------*/

#include "SlipGrammar.h"

/*------------------------------------ public macros -----------------------------------*/

#define MAIN_CLAIM_DEFAULT_C()                                                           \
  _Main_Make_Claim_C(Main_Default_Margin)

#define MAIN_CLAIM_DYNAMIC_C(SIZE)                                                       \
  _Main_Make_Claim_C(SIZE + Main_Default_Margin)

/*--------------------------------------------------------------------------------------*/

#define BEGIN_PROTECT(VARIABLE)                                                          \
  { Stack_Push_C(VARIABLE);

#define BEGIN_DOUBLE_PROTECT(VARIABLE_1, VARIABLE_2)                                     \
  { Stack_Double_Push_C(VARIABLE_1,                                                      \
                        VARIABLE_2);

#define END_PROTECT(VARIABLE)                                                            \
  VARIABLE = Stack_Pop(); }

#define END_DOUBLE_PROTECT(VARIABLE_1, VARIABLE_2)                                       \
  VARIABLE_2 = Stack_Pop();                                                              \
  VARIABLE_1 = Stack_Pop(); }

/*--------------------------------------------------------------------------------------*/

#define MAIN_REGISTER(VARIABLE)                                                          \
  _Main_Register_Root_Reference((ERF_type)&VARIABLE);

/*----------------------------------- public constants ---------------------------------*/

enum { Main_Default_Margin = 32,
       Main_Maximum_Number = Memory_Maximum_size,
       Main_Void           = Memory_Void_Value };

/*----------------------------------- public variables ---------------------------------*/

extern UNS_type tag_count[USP_tag + 1];
extern UNS_type dcount;
extern UNS_type tcount;
extern UNS_type ndcount;
extern UNS_type ntcount;

/*----------------------------------- hidden prototypes --------------------------------*/

NIL_type            _Main_Make_Claim_C(UNS_type);
NIL_type           _Main_Release_Claim(UNS_type);

NIL_type _Main_Register_Root_Reference(ERF_type);

/*----------------------------------- public prototypes --------------------------------*/

NIL_type            Main_Error_Handler(BYT_type,
                                       RWS_type);
EXP_type           Main_Get_Expression(NIL_type);
NIL_type      Main_Reallocate_Vector_M(VRF_type,
                                       UNS_type);
NIL_type      Main_Receive_Ken_Message(RWK_type);
NIL_type                Main_Reclaim_C(NIL_type);
NIL_type           Main_Replace_Vector(VEC_type,
                                       VEC_type);
EXP_type                  Main_Reverse(EXP_type);
NIL_type           Main_Set_Expression(EXP_type);
NIL_type                Main_Proceed_C(NIL_type);
NIL_type                     Main_Exit(NIL_type);

