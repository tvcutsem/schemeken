                     /*---------------------------------------------*/
                     /*                  >>>Slip<<<                 */
                     /*                 Theo D'Hondt                */
                     /*          VUB Software Languages Lab         */
                     /*                  (c) 2012                   */
                     /*---------------------------------------------*/
                     /*   version 13: completion and optimization   */
                     /*---------------------------------------------*/
                     /*                    Slip                     */
                     /*---------------------------------------------*/

/*----------------------------------- public constants ---------------------------------*/

enum { AL1_error,                     /* at least one argument required       */
       AL2_error,                     /* at least two arguments required      */
       AM1_error,                     /* at most one argument required        */
       AM2_error,                     /* at most two arguments required       */
       ARC_error,                     /* arguments restricted to characters   */
       ARN_error,                     /* arguments restricted to numbers      */
       BFO_error,                     /* buffer overflow                      */
       BND_error,                     /* binding expected                     */
       COR_error,                     /* character out of range               */
       CRQ_error,                     /* continuation requires single value   */
       DBZ_error,                     /* division by zero                     */
       DRQ_error,                     /* digit required                       */
       E1X_error,                     /* exactly one expression required      */
       EX0_error,                     /* no arguments allowed                 */
       EX1_error,                     /* exactly one argument required        */
       EX2_error,                     /* exactly two arguments required       */
       EX3_error,                     /* exactly three arguments required     */
       FRP_error,                     /* force requires promise               */
       ICL_error,                     /* invalid character literal            */
       ILC_error,                     /* illegal character                    */
       ILH_error,                     /* illegal use of #                     */
       IMM_error,                     /* insufficient memory                  */
       INF_error,                     /* input not found                      */
       IOR_error,                     /* index out of range                   */
       IPA_error,                     /* invalid parameter                    */
       IRP_error,                     /* illegal right parenthesis            */
       ITA_error,                     /* improperly terminated argument list  */
       ITF_error,                     /* improperly terminated form           */
       IUK_error,                     /* illegal use of keyword               */
       IUP_error,                     /* illegal use of period                */
       IVP_error,                     /* invalid pattern in form              */
       IVV_error,                     /* invalid variable in form             */
       IXT_error,                     /* invalid expression type              */
       MNS_error,                     /* memory not synchronized              */
       MRP_error,                     /* missing right parenthesis            */
       MSP_error,                     /* missing pattern in form              */
       MSV_error,                     /* invalid variable in form             */
       NAB_error,                     /* not a boolean                        */
       NAC_error,                     /* not a character                      */
       NAK_error,                     /* not a ken-id                         */
       NAL_error,                     /* not a list                           */
       NAN_error,                     /* not a number                         */
       NAP_error,                     /* not a pair                           */
       NAS_error,                     /* not a string                         */
       NAV_error,                     /* not a vector                         */
       NNN_error,                     /* non-negative number required         */
       NSY_error,                     /* not a symbol                         */
       NTL_error,                     /* number too large                     */
       NYI_error,                     /* not yet implemented                  */
       OPR_error,                     /* operator required                    */
       PEP_error,                     /* premature end of program             */
       PNR_error,                     /* positive number required             */
       QRQ_error,                     /* quote required                       */
       TFO_error,                     /* too few operands in application      */
       TFX_error,                     /* too few expressions in form          */
       TGO_error,                     /* tag overflow                         */
       TMO_error,                     /* too many operands in application     */
       TMR_error,                     /* too many root registrations          */
       TMT_error,                     /* too many thread registrations        */
       TMX_error,                     /* too many expressions in form         */
       UDF_error,                     /* undefined variable                   */
       UQS_error,                     /* unquote-splicing not from within list*/
       USR_error,                     /* user error                           */
       VNF_error,                     /* variable not found                   */
       VTL_error,                     /* vector too large                     */
       XCT_error };                   /* excess tokens                        */

/*----------------------------------- public callbacks ---------------------------------*/

short Slip_Close(void );
void  Slip_Error(short,
                 char*);
void   Slip_Fail(short);
void    Slip_Log(char*);
short  Slip_Open(char*);
void  Slip_Print(const char*);
char   Slip_Read(void );

/*----------------------------------- public prototypes --------------------------------*/

void    Slip_REP(char*,
                 int  );
