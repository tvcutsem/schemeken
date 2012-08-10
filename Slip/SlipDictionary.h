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

/*----------------------------------- public prototypes --------------------------------*/

NIL_type           Dictionary_Checkpoint(NIL_type);
UNS_type             Dictionary_Define_M(SYM_type);
NIL_type Dictionary_Enter_Nested_Scope_C(NIL_type);
NIL_type    Dictionary_Exit_Nested_Scope(NIL_type);
UNS_type       Dictionary_Get_Frame_Size(NIL_type);
UNS_type   Dictionary_Initially_Define_M(SYM_type);
UNS_type    Dictionary_Lexical_Address_M(SYM_type,
                                         UNS_type *);
NIL_type           Dictionary_Rollback_C(NIL_type);
NIL_type  Dictionary_Setup_Symbol_Vector(VEC_type);

/*--------------------------------------------------------------------------------------*/

NIL_type           Dictionary_Initialize(NIL_type);
