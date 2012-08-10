                     /*---------------------------------------------*/
                     /*                  >>>Slip<<<                 */
                     /*                 Theo D'Hondt                */
                     /*          VUB Software Languages Lab         */
                     /*                  (c) 2012                   */
                     /*---------------------------------------------*/
                     /*   version 13: completion and optimization   */
                     /*---------------------------------------------*/
                     /*                    memory                   */
                     /*---------------------------------------------*/

#include "SlipSlip.h"

/*----------------------------------- public constants ---------------------------------*/

enum { Memory_Cell_Bias    = 0x00000004,
       Memory_Maximum_size = 0x00FFFFFF,
       Memory_Void_Value   = 0x00000000 };

/*------------------------------------- public types -----------------------------------*/

typedef           void * ADR_type;
typedef short unsigned   BYT_type;
typedef           long   CEL_type;
typedef         double   DBL_type;
typedef           void   NIL_type;
typedef         signed   SGN_type;
typedef       unsigned   UNS_type;

/*------------------------------------- opaque types -----------------------------------*/

typedef      union PTR * PTR_type;

/*----------------------------------- public callbacks ---------------------------------*/

NIL_type  Memory_After_Reclaim(PTR_type);
PTR_type Memory_Before_Reclaim(NIL_type);

/*----------------------------------- public prototypes --------------------------------*/

UNS_type      Memory_Available(NIL_type);
NIL_type      Memory_Collect_C(NIL_type);
UNS_type  Memory_Collect_Count(NIL_type);
DBL_type    Memory_Consumption(NIL_type);
SGN_type  Memory_Get_Immediate(PTR_type);
UNS_type       Memory_Get_Size(PTR_type);
BYT_type        Memory_Get_Tag(PTR_type);
BYT_type   Memory_Is_Immediate(PTR_type);
PTR_type   Memory_Make_Chunk_M(BYT_type,
                               UNS_type);
PTR_type Memory_Make_Immediate(UNS_type);
NIL_type        Memory_Replace(PTR_type,
                               PTR_type);
NIL_type        Memory_Set_Tag(PTR_type,
                               BYT_type);

/*--------------------------------------------------------------------------------------*/

NIL_type     Memory_Initialize(ADR_type,
                               UNS_type,
                               BYT_type);
