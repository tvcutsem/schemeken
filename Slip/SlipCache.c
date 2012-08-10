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

#include "SlipMain.h"

#include "SlipCache.h"
#include "SlipPersist.h"

/*------------------------------------ private macros ----------------------------------*/

#define NULL_VECTOR (VEC_type)Grammar_Null

/*----------------------------------- private constants --------------------------------*/

enum { Cache_size = 32 };

/*----------------------------------- private variables --------------------------------*/

static VEC_type Cache_vector;

/*----------------------------------- private functions --------------------------------*/

static NIL_type flush_cache(NIL_type)
  { UNS_type index;
    for (index = 1;
         index <= Cache_size;
         index += 1)
      Cache_vector[index] = NULL_VECTOR; }

/*------------------------------------ public functions --------------------------------*/

VEC_type Cache_Make_Vector_C(UNS_type Size)
  { VEC_type next_vector,
             vector;
    if (Size == 0)
      return Grammar_Empty_Vector;
    if (Size <= Cache_size)
      { vector = Cache_vector[Size];
        if (!is_NUL(vector))
          { next_vector = vector[1];
            vector[1] = Grammar_Null;
            Cache_vector[Size] = next_vector;
            return vector; }}
    MAIN_CLAIM_DYNAMIC_C(Size);
    vector = make_VEC_M(Size);
    return vector; }

NIL_type Cache_Release_Vector(VEC_type Vector)
  { UNS_type index,
             size;
    VEC_type next_vector;
    size = size_VEC(Vector);
    if (size == 0)
      return;
    if (size <= Cache_size)
      if (!is_MRK(Vector))
        { for (index = 2;
               index <= size;
               index += 1)
            Vector[index] = Grammar_Null;
          next_vector = Cache_vector[size];
          Vector[1] = next_vector;
          Cache_vector[size] = Vector; }}

/*--------------------------------------------------------------------------------------*/

NIL_type Cache_Initialize_M(NIL_type)
  { slipken_persist_init(Cache_vector, make_VEC_Z(Cache_size));
    flush_cache();
    MAIN_REGISTER(Cache_vector); }
