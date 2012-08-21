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

/*----------------------------------- public prototypes --------------------------------*/

NIL_type        Environment_Adjust_Global_Frame_C(UNS_type);
FRM_type                  Environment_Empty_Frame(NIL_type);
EXP_type                    Environment_Frame_Get(FRM_type,
                                                  UNS_type);
NIL_type                    Environment_Frame_Set(FRM_type,
                                                  UNS_type,
                                                  EXP_type);
UNS_type                   Environment_Frame_Size(FRM_type);
FRM_type            Environment_Get_Current_Frame(NIL_type);
EXP_type             Environment_Global_Frame_Get(UNS_type);
NIL_type             Environment_Global_Frame_Set(EXP_type,
                                                  UNS_type);
SYM_type           Environment_Global_Symbol_Name(UNS_type);
VEC_type         Environment_Global_Symbol_Vector(NIL_type);
EXP_type                    Environment_Local_Get(NBR_type);
NIL_type                    Environment_Local_Set(NBR_type,
                                                  EXP_type);
FRM_type                 Environment_Make_Frame_C(UNS_type);
FRM_type                   Environment_Mark_Frame(NIL_type);
NIL_type        Environment_Release_Current_Frame(NIL_type);
NIL_type                Environment_Release_Frame(FRM_type);

/*--------------------------------------------------------------------------------------*/

ENV_type      Environment_Get_Current_Environment(NIL_type);
UNS_type Environment_Get_Current_Environment_Size(NIL_type);
ENV_type                     Environment_Extend_C(NIL_type);
EXP_type                   Environment_Global_Get(NBR_type,
                                                  NBR_type);
NIL_type                   Environment_Global_Set(NBR_type,
                                                  NBR_type,
                                                  EXP_type);
NIL_type          Environment_Replace_Environment(ENV_type,
                                                  FRM_type);
NIL_type                        Environment_Reset(NIL_type);

/*--------------------------------------------------------------------------------------*/

NIL_type                 Environment_Initialize_M(NIL_type);
