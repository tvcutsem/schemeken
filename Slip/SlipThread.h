                     /*---------------------------------------------*/
                     /*                  >>>Slip<<<                 */
                     /*                 Theo D'Hondt                */
                     /*          VUB Software Languages Lab         */
                     /*                  (c) 2012                   */
                     /*---------------------------------------------*/
                     /*   version 13: completion and optimization   */
                     /*---------------------------------------------*/
                     /*                   threads                   */
                     /*---------------------------------------------*/

/*------------------------------------ public macros -----------------------------------*/

#define THREAD_BEGIN_FRAME(SIGNATURE)                                                    \
  DEFINE_GRAMMAR_FRAME(SIGNATURE);                                                       \
  BEGIN_GRAMMAR_FRAME(SIGNATURE);                                                        \
  DEFINE_GRAMMAR_SLOT(EXP, _1_);                                                         \
  DEFINE_GRAMMAR_SLOT(EXP, _2_);                                                         \
  DEFINE_GRAMMAR_SLOT(EXP, _3_)

#define THREAD_END_FRAME(SIGNATURE)                                                      \
  END_GRAMMAR_FRAME(SIGNATURE)

#define THREAD_FRAME_SLOT(SIGNATURE, NAME)                                               \
  DEFINE_GRAMMAR_SLOT(SIGNATURE, NAME)

/*------------------------------------- opaque types -----------------------------------*/

typedef NIL_type (* TPR_type) (NIL_type);

/*----------------------------------- public prototypes --------------------------------*/

THR_type        Thread_Keep_C(NIL_type);
THR_type          Thread_Mark(NIL_type);
THR_type          Thread_Peek(NIL_type);
THR_type       Thread_Patch_C(NBR_type);
THR_type        Thread_Poke_C(NBR_type,
                              EXP_type,
                              UNS_type);
THR_type        Thread_Push_C(NBR_type,
                              EXP_type,
                              UNS_type);
NIL_type       Thread_Replace(THR_type);
NIL_type         Thread_Reset(NIL_type);
EXP_type Thread_Tail_Position(NIL_type);
NIL_type           Thread_Zap(NIL_type);

/*--------------------------------------------------------------------------------------*/

NIL_type     Thread_Proceed_C(NIL_type);
NBR_type      Thread_Register(TPR_type);

/*--------------------------------------------------------------------------------------*/

NIL_type    Thread_Initialize(NIL_type);
