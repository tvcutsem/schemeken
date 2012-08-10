                     /*---------------------------------------------*/
                     /*                  >>>Slip<<<                 */
                     /*                 Theo D'Hondt                */
                     /*          VUB Software Languages Lab         */
                     /*                  (c) 2012                   */
                     /*---------------------------------------------*/
                     /*   version 13: completion and optimization   */
                     /*---------------------------------------------*/
                     /*                   scanner                   */
                     /*---------------------------------------------*/

/*------------------------------------- public types -----------------------------------*/

typedef enum { CHA_token,                                /* character         */
               EOL_token,                                /* eof               */
               FLS_token,                                /* false             */
               IDT_token,                                /* identifier        */
               LPR_token,                                /* left parenthesis  */
               NBR_token,                                /* number            */
               PER_token,                                /* period            */
               QQU_token,                                /* quasiquote        */
               QUO_token,                                /* quote             */
               REA_token,                                /* real              */
               RPR_token,                                /* right parenthesis */
               STR_token,                                /* string            */
               TRU_token,                                /* true              */
               UNQ_token,                                /* unquote           */
               UQS_token,                                /* unquote splicing  */
               VEC_token } SCA_type;                     /* vector            */

/*----------------------------------- public variables ---------------------------------*/

extern RWC_type Scan_String[];

/*----------------------------------- public callbacks ---------------------------------*/

NIL_type      Scan_Error(BYT_type);

/*----------------------------------- public prototypes --------------------------------*/

SCA_type       Scan_Next(NIL_type);
SCA_type     Scan_Preset(NIL_type);

/*--------------------------------------------------------------------------------------*/

NIL_type Scan_Initialize(NIL_type);
