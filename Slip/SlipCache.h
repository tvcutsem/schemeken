                     /*---------------------------------------------*/
                     /*                  >>>Slip<<<                 */
                     /*                 Theo D'Hondt                */
                     /*          VUB Software Languages Lab         */
                     /*                  (c) 2012                   */
                     /*---------------------------------------------*/
                     /*   version 13: completion and optimization   */
                     /*---------------------------------------------*/
                     /*                    cache                    */
                     /*---------------------------------------------*/

/*------------------------------------- opaque types -----------------------------------*/

DEFINE_GRAMMAR_FRAME(CCH);

/*----------------------------------- public prototypes --------------------------------*/

VEC_type  Cache_Make_Vector_C(UNS_type);
NIL_type Cache_Release_Vector(VEC_type);

/*--------------------------------------------------------------------------------------*/

NIL_type   Cache_Initialize_M(NIL_type);
