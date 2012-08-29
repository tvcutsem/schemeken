                     /*---------------------------------------------*/
                     /*                  >>>Slip<<<                 */
                     /*                 Theo D'Hondt                */
                     /*          VUB Software Languages Lab         */
                     /*                  (c) 2012                   */
                     /*---------------------------------------------*/
                     /*   version 13: completion and optimization   */
                     /*---------------------------------------------*/
                     /*                 main file                   */
                     /*---------------------------------------------*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ken.h"
#include "kenapp.h"

#include "SlipSlip.h"

#include "SlipPersist.h"

#define AL1_error_string "at least one argument required"
#define AL2_error_string "at least two arguments required"
#define AM1_error_string "at most one argument required"
#define AM2_error_string "at most two arguments required"
#define ARC_error_string "arguments restricted to characters"
#define ARN_error_string "arguments restricted to numbers"
#define BFO_error_string "buffer overflow"
#define BND_error_string "binding expected"
#define COR_error_string "character out of range"
#define CRQ_error_string "continuation requires single value"
#define DBZ_error_string "division by zero"
#define DRQ_error_string "digit required"
#define E1X_error_string "exactly one expression required"
#define EX0_error_string "no arguments allowed"
#define EX1_error_string "exactly one argument required"
#define EX2_error_string "exactly two arguments required"
#define EX3_error_string "exactly three arguments required"
#define FRP_error_string "force requires promise"
#define ICL_error_string "invalid character literal"
#define ILC_error_string "illegal character"
#define ILH_error_string "illegal use of #"
#define IMM_error_string "insufficient memory"
#define INF_error_string "input not found"
#define IOR_error_string "index out of range"
#define IPA_error_string "invalid parameter"
#define IRP_error_string "illegal right parenthesis"
#define ITA_error_string "improperly terminated argument list"
#define ITF_error_string "improperly terminated form"
#define IUK_error_string "illegal use of keyword"
#define IUP_error_string "illegal use of period"
#define IVP_error_string "invalid pattern in form"
#define IVV_error_string "invalid variable in form"
#define IXT_error_string "invalid expression type"
#define MNS_error_string "memory not synchronized"
#define MRP_error_string "missing right parenthesis"
#define MSP_error_string "missing pattern in form"
#define MSV_error_string "missing variable in form"
#define NAB_error_string "not a boolean"
#define NAC_error_string "not a character"
#define NAK_error_string "not a ken-id"
#define NAL_error_string "not a list"
#define NAN_error_string "not a number"
#define NAP_error_string "not a pair"
#define NAS_error_string "not a string"
#define NAV_error_string "not a vector"
#define NNN_error_string "non-negative number required"
#define NSY_error_string "not a symbol"
#define NTL_error_string "number too large"
#define NYI_error_string "not yet implemented"
#define OPR_error_string "operator required"
#define PEP_error_string "premature end of program"
#define PNR_error_string "positive number required"
#define QRQ_error_string "quote required"
#define TFO_error_string "too few operands in application"
#define TFX_error_string "too few expressions in form"
#define TGO_error_string "tag overflow"
#define TMO_error_string "too many operands in application"
#define TMR_error_string "too many root registrations"
#define TMT_error_string "too many thread registrations"
#define TMX_error_string "too many expressions in form"
#define UDF_error_string "undefined variable"
#define UQS_error_string "unquote-splicing not from within list"
#define USR_error_string "user error"
#define VNF_error_string "variable not found"
#define VTL_error_string "vector too large"
#define XCT_error_string "excess tokens"

const static char* errors[] =
     { AL1_error_string,
       AL2_error_string,
       AM1_error_string,
       AM2_error_string,
       ARC_error_string,
       ARN_error_string,
       BFO_error_string,
       BND_error_string,
       COR_error_string,
       CRQ_error_string,
       DBZ_error_string,
       DRQ_error_string,
       E1X_error_string,
       EX0_error_string,
       EX1_error_string,
       EX2_error_string,
       EX3_error_string,
       FRP_error_string,
       ICL_error_string,
       ILC_error_string,
       ILH_error_string,
       IMM_error_string,
       INF_error_string,
       IOR_error_string,
       IPA_error_string,
       IRP_error_string,
       ITA_error_string,
       ITF_error_string,
       IUK_error_string,
       IUP_error_string,
       IVP_error_string,
       IVV_error_string,
       IXT_error_string,
       MNS_error_string,
       MRP_error_string,
       MSP_error_string,
       MSV_error_string,
       NAB_error_string,
       NAC_error_string,
       NAK_error_string,
       NAL_error_string,
       NAN_error_string,
       NAP_error_string,
       NAS_error_string,
       NAV_error_string,
       NNN_error_string,
       NSY_error_string,
       NTL_error_string,
       NYI_error_string,
       OPR_error_string,
       PEP_error_string,
       PNR_error_string,
       QRQ_error_string,
       TFO_error_string,
       TFX_error_string,
       TGO_error_string,
       TMO_error_string,
       TMR_error_string,
       TMT_error_string,
       TMX_error_string,
       UDF_error_string,
       UQS_error_string,
       USR_error_string,
       VNF_error_string,
       VTL_error_string,
       XCT_error_string };

enum { Buffer_size       = 50000,
       Print_Buffer_size = 1000 };

static char Print_Buffer[Print_Buffer_size];

static char * Memory;

static int counter;
static FILE * stream;

/* Imported from SlipMain.c */
void Slip_INIT(char * Memory, int Size);
void read_eval_print_C(void);
void Main_Receive_Ken_Message(kenid_t sender);

short Slip_Close(void)
  { short character;
    if (stream == 0)
      return 1;
    character = getc(stream);
    fclose(stream);
    stream = 0;
    return (character == EOF); }

void Slip_Error(short error,
                char* message)
  { if (message == NULL)
      snprintf(Print_Buffer, Print_Buffer_size, "\nerror: %s\n", errors[error]);
    else
      snprintf(Print_Buffer, Print_Buffer_size, "\nerror: %s ... %s\n", errors[error] , message);
    (void)ken_send(kenid_stdout, Print_Buffer, strlen(Print_Buffer));
  }

void Slip_Fail(short error)
  { (void)ken_send(kenid_stdout, errors[error], strlen(errors[error]));
    exit(0); }

void Slip_Log(char* message)
  { return;
    snprintf(Print_Buffer, Print_Buffer_size, "\nLOG%4d: %s", counter++, message);
    (void)ken_send(kenid_stdout, Print_Buffer, strlen(Print_Buffer)); }

short Slip_Open(char* name)
  { if (name == 0)
      { stream = 0;
        return 1; }
    stream = fopen(name, "r");
    if (stream == 0)
      return 0;
    return 1; }

void Slip_Print(const char* string)
  { size_t len = strlen(string);
    if (len > 0)
      (void)ken_send(kenid_stdout, string, len); }

char Slip_Read(void)
  { short character;
    if (stream == 0)
      return getchar();
    character = getc(stream);
    if (character == EOF)
      if (stream)
        { character = '\n';
          fclose(stream);
          stream = 0; }
      else
        character = 0;
    return character; }

static const char * Prompt_rawstring = "\n>>>";

void evaluate_file(const char * filename) {
  stream = fopen(filename, "r");
  if (stream == NULL) {
    fprintf(stderr, "Could not load file '%s': %s\n", filename, strerror(errno));
  } else {
    fprintf(stderr, "Loading file '%s'...\n", filename);
    read_eval_print_C();
  }
}

int64_t ken_handler(void *msg, int32_t len, kenid_t sender) {
  struct Slipken_data * slipken_data = ken_get_app_data();
  if (0 == ken_id_cmp(sender, kenid_NULL) || NULL == slipken_data) {
    fprintf(stderr, "Initializing...\n");
    slipken_data = create_slipken_data();
    Memory = slipken_data->Memory;
    Slip_INIT(Memory, Memory_size);

    if (ken_argc() > 2) {
      const char * filename = ken_argv(2);
      evaluate_file(filename);
    }

    save_slipken_data(slipken_data);
  } else if (slipken_data->my_pid != getpid()) {
    fprintf(stderr, "Restoring state ...\n");
    Memory = slipken_data->Memory;
    load_slipken_data(slipken_data);

    fprintf(stderr, "Welcome back!\n");
    fprintf(stderr, "%s", Prompt_rawstring);
    return ken_handler(msg, len, sender);
  } else {
    if (0 == ken_id_cmp(sender, kenid_stdin)) {
      int slip_pipe[2];
      pipe(slip_pipe);
      write(slip_pipe[1], msg, len);
      close(slip_pipe[1]);

      stream = fdopen(slip_pipe[0], "r");
      read_eval_print_C();

      save_slipken_data(slipken_data);
    } else if (0 == ken_id_cmp(sender, kenid_alarm)) {
      Main_Receive_Ken_Message(sender);
    } else {
      /* Message send from a peer */
      int slip_pipe[2];
      pipe(slip_pipe);
      write(slip_pipe[1], msg, len);
      close(slip_pipe[1]);

      stream = fdopen(slip_pipe[0], "r");
      Main_Receive_Ken_Message(sender);
    }
  }
  Slip_Print(Prompt_rawstring);
  return -1;
}
