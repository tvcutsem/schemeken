                     /*---------------------------------------------*/
                     /*                  >>>Slip<<<                 */
                     /*                 Theo D'Hondt                */
                     /*          VUB Software Languages Lab         */
                     /*                  (c) 2012                   */
                     /*---------------------------------------------*/
                     /*   version 13: completion and optimization   */
                     /*---------------------------------------------*/
                     /*                   evaluate                  */
                     /*---------------------------------------------*/

/*----------------------------------- public prototypes --------------------------------*/

NIL_type    Evaluate_Apply_C(EXP_type,
                             EXP_type,
                             EXP_type);
NIL_type     Evaluate_Eval_C(EXP_type,
                             EXP_type);
NIL_type  Evaluate_Promise_C(PRM_type,
                             EXP_type);

/*--------------------------------------------------------------------------------------*/

NIL_type Evaluate_Initialize(NIL_type);
