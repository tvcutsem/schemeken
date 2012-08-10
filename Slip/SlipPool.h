                     /*---------------------------------------------*/
                     /*                  >>>Slip<<<                 */
                     /*                 Theo D'Hondt                */
                     /*          VUB Software Languages Lab         */
                     /*                  (c) 2012                   */
                     /*---------------------------------------------*/
                     /*   version 13: completion and optimization   */
                     /*---------------------------------------------*/
                     /*                    pool                     */
                     /*---------------------------------------------*/


/*------------------------------------- public macros ----------------------------------*/

#define KEYWORD(KEY)                                                                     \
  Pool_Keyword_Symbol(Pool_##KEY)

/*----------------------------------- public constants ---------------------------------*/

static const RWS_type Pool_Application_Rawstring = "application";

/*------------------------------------- public types -----------------------------------*/

typedef
  enum { Pool_And              =  1,
         Pool_Begin            =  2,
         Pool_Cond             =  3,
         Pool_Define           =  4,
         Pool_Delay            =  5,
         Pool_Else             =  6,
         Pool_If               =  7,
         Pool_Lambda           =  8,
         Pool_Let              =  9,
         Pool_Or               = 10,
         Pool_Quasiquote       = 11,
         Pool_Quote            = 12,
         Pool_Set              = 13,
         Pool_Unquote          = 14,
         Pool_Unquote_Splicing = 15,
         Pool_While            = 16 } KEY_type;

/*----------------------------------- public variables ---------------------------------*/

extern SYM_type Pool_Anonymous_Symbol;

/*----------------------------------- public prototypes --------------------------------*/

SYM_type    Pool_Enter_Native_M(RWS_type);
SYM_type Pool_Enter_Rawstring_C(RWS_type);
SYM_type    Pool_Enter_String_C(STR_type);
SYM_type    Pool_Keyword_Symbol(KEY_type);
BLN_type   Pool_Validate_Symbol(SYM_type);

/*--------------------------------------------------------------------------------------*/

NIL_type      Pool_Initialize_M(NIL_type);
