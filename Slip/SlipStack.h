                     /*---------------------------------------------*/
                     /*                  >>>Slip<<<                 */
                     /*                 Theo D'Hondt                */
                     /*          VUB Software Languages Lab         */
                     /*                  (c) 2012                   */
                     /*---------------------------------------------*/
                     /*   version 13: completion and optimization   */
                     /*---------------------------------------------*/
                     /*                    stack                    */
                     /*---------------------------------------------*/

/*----------------------------------- public prototypes --------------------------------*/

NIL_type    Stack_Collapse_C(UNS_type);
NIL_type Stack_Double_Push_C(EXP_type,
                             EXP_type);
EXP_type          Stack_Peek(NIL_type);
NIL_type          Stack_Poke(EXP_type);
EXP_type           Stack_Pop(NIL_type);
NIL_type        Stack_Push_C(EXP_type);
EXP_type        Stack_Reduce(NIL_type);
NIL_type         Stack_Reset(NIL_type);
NIL_type           Stack_Zap(NIL_type);

/*--------------------------------------------------------------------------------------*/

NIL_type  Stack_Initialize_M(NIL_type);
