                     /*---------------------------------------------*/
                     /*                  >>>Slip<<<                 */
                     /*                 Theo D'Hondt                */
                     /*          VUB Software Languages Lab         */
                     /*                  (c) 2012                   */
                     /*---------------------------------------------*/
                     /*   version 13: completion and optimization   */
                     /*---------------------------------------------*/
                     /*                    grammar                  */
                     /*---------------------------------------------*/

#include "SlipMemory.h"
#include "ken.h"

/*------------------------------------ hidden macros -----------------------------------*/

#define _REGULAR_TAG(TAG) 0x00 + _##TAG

#define _RAW_TAG(TAG)     0x3F - _##TAG

#define _INLINE_TAG(TAG)  0x40 + _##TAG

/*------------------------------------ public macros -----------------------------------*/

#define CEL_SIZE sizeof(CEL_type)

#define DEFINE_GRAMMAR_FRAME(SIGNATURE)                                                  \
  typedef struct SIGNATURE * SIGNATURE##_type

#define BEGIN_GRAMMAR_FRAME(SIGNATURE)                                                   \
  typedef struct SIGNATURE {                                                             \
  DEFINE_GRAMMAR_SLOT(CEL, hdr)

#define DEFINE_GRAMMAR_SLOT(SIGNATURE, NAME)                                             \
  SIGNATURE##_type NAME

#define END_GRAMMAR_FRAME(SIGNATURE)                                                     \
  } SIGNATURE;                                                                           \
                                                                                         \
static const UNS_type SIGNATURE##_size                                                   \
                      = (sizeof(SIGNATURE)/CEL_SIZE - 1)

/*----------------------------------- hidden constants ---------------------------------*/

enum { _AND,                                                 /* regular expression tags */
       _APL,
       _BEG,
       _CND,
       _CNE,
       _CNT,
       _DEL,
       _DFF,
       _DFV,
       _DFZ,
       _FRC,
       _GLA,
       _GLB,
       _IFD,
       _IFS,
       _LCA,
       _LCL,
       _LET,
       _LMB,
       _LMV,
       _MRK,
       _ORR,
       _PAI,
       _PRC,
       _PRM,
       _PRV,
       _QQU,
       _SEQ,
       _STG,
       _STL,
       _THK,
       _UNQ,
       _UQS,
       _VEC,
       _WHI,
       __RG };

enum { _NAT,                                                     /* raw expression tags */
       _REA,
       _STR,
       _SYM,
       _KID,
       __RW };

enum { _CHA,                                                  /* inline expression tags */
       _FLS,
       _NUL,
       _NBR,
       _TRU,
       _UDF,
       _USP };

/*------------------------------------- hidden types -----------------------------------*/

DEFINE_GRAMMAR_FRAME(ENV);
DEFINE_GRAMMAR_FRAME(FRM);
DEFINE_GRAMMAR_FRAME(THR);

/*------------------------------------- public types -----------------------------------*/

typedef enum { AND_tag = _REGULAR_TAG(AND),                                      /* and */
               APL_tag = _REGULAR_TAG(APL),                              /* application */
               BEG_tag = _REGULAR_TAG(BEG),                                    /* begin */
               CND_tag = _REGULAR_TAG(CND),                            /* cond w/o else */
               CNE_tag = _REGULAR_TAG(CNE),                           /* cond with else */
               CNT_tag = _REGULAR_TAG(CNT),                             /* continuation */
               DEL_tag = _REGULAR_TAG(DEL),                                    /* delay */
               DFF_tag = _REGULAR_TAG(DFF),                          /* define function */
               DFV_tag = _REGULAR_TAG(DFV),                          /* define variable */
               DFZ_tag = _REGULAR_TAG(DFZ),             /* define function with varargs */
               FRC_tag = _REGULAR_TAG(FRC),                            /* force wrapper */
               GLA_tag = _REGULAR_TAG(GLA),                       /* global application */
               GLB_tag = _REGULAR_TAG(GLB),                          /* global variable */
               IFD_tag = _REGULAR_TAG(IFD),                            /* two-branch if */
               IFS_tag = _REGULAR_TAG(IFS),                            /* one-branch if */
               LCA_tag = _REGULAR_TAG(LCA),                        /* local application */
               LCL_tag = _REGULAR_TAG(LCL),                           /* local variable */
               LET_tag = _REGULAR_TAG(LET),                                      /* let */
               LMB_tag = _REGULAR_TAG(LMB),                                   /* lambda */
               LMV_tag = _REGULAR_TAG(LMV),                      /* lambda with varargs */
               MRK_tag = _REGULAR_TAG(MRK),                                     /* mark */
               ORR_tag = _REGULAR_TAG(ORR),                                       /* or */
               PAI_tag = _REGULAR_TAG(PAI),                                     /* pair */
               PRC_tag = _REGULAR_TAG(PRC),                                /* procedure */
               PRM_tag = _REGULAR_TAG(PRM),                                  /* promise */
               PRV_tag = _REGULAR_TAG(PRV),                   /* procedure with varargs */
               QQU_tag = _REGULAR_TAG(QQU),                               /* quasiquote */
               SEQ_tag = _REGULAR_TAG(SEQ),                                 /* sequence */
               STG_tag = _REGULAR_TAG(STG),                    /* store global variable */
               STL_tag = _REGULAR_TAG(STL),                     /* store local variable */
               THK_tag = _REGULAR_TAG(THK),                                    /* thunk */
               UNQ_tag = _REGULAR_TAG(UNQ),                                  /* unquote */
               UQS_tag = _REGULAR_TAG(UQS),                         /* unquote-splicing */
               VEC_tag = _REGULAR_TAG(VEC),                                   /* vector */
               WHI_tag = _REGULAR_TAG(WHI),                                    /* while */
               _RG_tag = _REGULAR_TAG(_RG),             /* upper bound for regular tags */

               NAT_tag = _RAW_TAG(NAT),                                       /* native */
               REA_tag = _RAW_TAG(REA),                                         /* real */
               STR_tag = _RAW_TAG(STR),                                       /* string */
               SYM_tag = _RAW_TAG(SYM),                                       /* symbol */
               KID_tag = _RAW_TAG(KID),
               _RW_tag = _RAW_TAG(_RW),                     /* lower bound for raw tags */

               CHA_tag = _INLINE_TAG(CHA),                                 /* character */
               FLS_tag = _INLINE_TAG(FLS),                                     /* false */
               NUL_tag = _INLINE_TAG(NUL),                                      /* null */
               NBR_tag = _INLINE_TAG(NBR),                                    /* number */
               TRU_tag = _INLINE_TAG(TRU),                                      /* true */
               UDF_tag = _INLINE_TAG(UDF),                                 /* undefined */
               USP_tag = _INLINE_TAG(USP) } TAG_type;                    /* unspecified */

DEFINE_GRAMMAR_FRAME(AND);
DEFINE_GRAMMAR_FRAME(APL);
DEFINE_GRAMMAR_FRAME(BEG);
DEFINE_GRAMMAR_FRAME(CHA);
DEFINE_GRAMMAR_FRAME(CND);
DEFINE_GRAMMAR_FRAME(CNE);
DEFINE_GRAMMAR_FRAME(CNT);
DEFINE_GRAMMAR_FRAME(DEL);
DEFINE_GRAMMAR_FRAME(DFF);
DEFINE_GRAMMAR_FRAME(DFV);
DEFINE_GRAMMAR_FRAME(DFZ);
DEFINE_GRAMMAR_FRAME(FRC);
DEFINE_GRAMMAR_FRAME(GLA);
DEFINE_GRAMMAR_FRAME(GLB);
DEFINE_GRAMMAR_FRAME(IFD);
DEFINE_GRAMMAR_FRAME(IFS);
DEFINE_GRAMMAR_FRAME(KID);
DEFINE_GRAMMAR_FRAME(LCA);
DEFINE_GRAMMAR_FRAME(LCL);
DEFINE_GRAMMAR_FRAME(LET);
DEFINE_GRAMMAR_FRAME(LMB);
DEFINE_GRAMMAR_FRAME(LMV);
DEFINE_GRAMMAR_FRAME(NAT);
DEFINE_GRAMMAR_FRAME(ORR);
DEFINE_GRAMMAR_FRAME(PAI);
DEFINE_GRAMMAR_FRAME(QQU);
DEFINE_GRAMMAR_FRAME(PRC);
DEFINE_GRAMMAR_FRAME(PRM);
DEFINE_GRAMMAR_FRAME(PRV);
DEFINE_GRAMMAR_FRAME(REA);
DEFINE_GRAMMAR_FRAME(STG);
DEFINE_GRAMMAR_FRAME(STL);
DEFINE_GRAMMAR_FRAME(STR);
DEFINE_GRAMMAR_FRAME(SYM);
DEFINE_GRAMMAR_FRAME(THK);
DEFINE_GRAMMAR_FRAME(UNQ);
DEFINE_GRAMMAR_FRAME(UQS);
DEFINE_GRAMMAR_FRAME(WHI);

typedef  NIL_type * EXP_type;
typedef  EXP_type * SEQ_type;
typedef  EXP_type * VEC_type;

typedef  EXP_type   FLS_type;
typedef  EXP_type   NBR_type;
typedef  EXP_type   NUL_type;
typedef  EXP_type   TRU_type;
typedef  EXP_type   UDF_type;
typedef  EXP_type   USP_type;

typedef  EXP_type * ERF_type;
typedef  VEC_type * VRF_type;

typedef      char   RWC_type;
typedef   kenid_t   RWK_type;
typedef long long   RWL_type;
typedef      long   RWN_type;
typedef    double   RWR_type;
typedef      char * RWS_type;
typedef NIL_type (* RNF_type) (FRM_type,
                               EXP_type);

typedef
  enum { Grammar_Is_False = 0,
         Grammar_Is_True  = 1 } BLN_type;

/*----------------------------------- public variables ---------------------------------*/

extern STR_type Grammar_Empty_String;
extern VEC_type Grammar_Empty_Vector;
extern FLS_type Grammar_False;
extern STR_type Grammar_Newline_String;
extern NUL_type Grammar_Null;
extern NBR_type Grammar_One_Number;
extern TRU_type Grammar_True;
extern UDF_type Grammar_Undefined;
extern USP_type Grammar_Unspecified;
extern NBR_type Grammar_Zero_Number;

/*----------------------------------- public prototypes --------------------------------*/

TAG_type Grammar_Boundary(NIL_type);
TAG_type      Grammar_Tag(EXP_type);

/*----------------------------------- AND declarations ---------------------------------*/

BEGIN_GRAMMAR_FRAME(AND);
  DEFINE_GRAMMAR_SLOT(VEC, prd);
END_GRAMMAR_FRAME(AND);

BLN_type      is_AND(EXP_type);
AND_type  make_AND_M(VEC_type);

/*----------------------------------- APL declarations ---------------------------------*/

BEGIN_GRAMMAR_FRAME(APL);
  DEFINE_GRAMMAR_SLOT(EXP, exp);
  DEFINE_GRAMMAR_SLOT(VEC, opd);
END_GRAMMAR_FRAME(APL);

BLN_type      is_APL(EXP_type);
APL_type  make_APL_M(EXP_type,
                     VEC_type);

/*----------------------------------- BEG declarations ---------------------------------*/

BEGIN_GRAMMAR_FRAME(BEG);
  DEFINE_GRAMMAR_SLOT(EXP, bod);
END_GRAMMAR_FRAME(BEG);

BLN_type      is_BEG(EXP_type);
BEG_type  make_BEG_M(EXP_type);

/*----------------------------------- CHA declarations ---------------------------------*/

BLN_type      is_CHA(EXP_type);
CHA_type    make_CHA(RWC_type);
RWC_type     get_CHA(CHA_type);

/*----------------------------------- CND declarations ---------------------------------*/

BEGIN_GRAMMAR_FRAME(CND);
  DEFINE_GRAMMAR_SLOT(VEC, prd);
  DEFINE_GRAMMAR_SLOT(VEC, bod);
END_GRAMMAR_FRAME(CND);

BLN_type      is_CND(EXP_type);
CND_type  make_CND_M(VEC_type,
                     VEC_type);

/*----------------------------------- CNE declarations ---------------------------------*/

BEGIN_GRAMMAR_FRAME(CNE);
  DEFINE_GRAMMAR_SLOT(VEC, prd);
  DEFINE_GRAMMAR_SLOT(VEC, bod);
  DEFINE_GRAMMAR_SLOT(EXP, els);
END_GRAMMAR_FRAME(CNE);

BLN_type      is_CNE(EXP_type);
CNE_type  make_CNE_M(VEC_type,
                     VEC_type,
                     EXP_type);

/*----------------------------------- CNT declarations ---------------------------------*/

BEGIN_GRAMMAR_FRAME(CNT);
  DEFINE_GRAMMAR_SLOT(ENV, env);
  DEFINE_GRAMMAR_SLOT(FRM, frm);
  DEFINE_GRAMMAR_SLOT(THR, thr);
  DEFINE_GRAMMAR_SLOT(VEC, smv);
END_GRAMMAR_FRAME(CNT);

BLN_type      is_CNT(EXP_type);
CNT_type  make_CNT_M(ENV_type,
                     FRM_type,
                     THR_type);

/*----------------------------------- DEL declarations ---------------------------------*/

BEGIN_GRAMMAR_FRAME(DEL);
  DEFINE_GRAMMAR_SLOT(NBR, siz);
  DEFINE_GRAMMAR_SLOT(EXP, exp);
  DEFINE_GRAMMAR_SLOT(VEC, smv);
END_GRAMMAR_FRAME(DEL);

BLN_type      is_DEL(EXP_type);
DEL_type  make_DEL_M(NBR_type,
                     EXP_type,
                     VEC_type);

/*----------------------------------- DFF declarations ---------------------------------*/

BEGIN_GRAMMAR_FRAME(DFF);
  DEFINE_GRAMMAR_SLOT(SYM, nam);
  DEFINE_GRAMMAR_SLOT(NBR, ofs);
  DEFINE_GRAMMAR_SLOT(NBR, par);
  DEFINE_GRAMMAR_SLOT(NBR, siz);
  DEFINE_GRAMMAR_SLOT(EXP, bod);
  DEFINE_GRAMMAR_SLOT(VEC, smv);
END_GRAMMAR_FRAME(DFF);

BLN_type      is_DFF(EXP_type);
DFF_type  make_DFF_M(SYM_type,
                     NBR_type,
                     NBR_type,
                     NBR_type,
                     EXP_type,
                     VEC_type);

/*----------------------------------- DFV declarations ---------------------------------*/

BEGIN_GRAMMAR_FRAME(DFV);
  DEFINE_GRAMMAR_SLOT(NBR, ofs);
  DEFINE_GRAMMAR_SLOT(EXP, exp);
END_GRAMMAR_FRAME(DFV);

BLN_type      is_DFV(EXP_type);
DFV_type  make_DFV_M(NBR_type,
                     EXP_type);

/*----------------------------------- DFZ declarations ---------------------------------*/

BEGIN_GRAMMAR_FRAME(DFZ);
  DEFINE_GRAMMAR_SLOT(SYM, nam);
  DEFINE_GRAMMAR_SLOT(NBR, ofs);
  DEFINE_GRAMMAR_SLOT(NBR, par);
  DEFINE_GRAMMAR_SLOT(NBR, siz);
  DEFINE_GRAMMAR_SLOT(EXP, bod);
  DEFINE_GRAMMAR_SLOT(VEC, smv);
END_GRAMMAR_FRAME(DFZ);

BLN_type      is_DFZ(EXP_type);
DFZ_type  make_DFZ_M(SYM_type,
                     NBR_type,
                     NBR_type,
                     NBR_type,
                     EXP_type,
                     VEC_type);

/*----------------------------------- FLS declarations ---------------------------------*/

BLN_type      is_FLS(EXP_type);
FLS_type    make_FLS(NIL_type);

/*----------------------------------- FRC declarations ---------------------------------*/

BEGIN_GRAMMAR_FRAME(FRC);
  DEFINE_GRAMMAR_SLOT(EXP, val);
END_GRAMMAR_FRAME(FRC);

BLN_type      is_FRC(EXP_type);
NIL_type convert_FRC(PRM_type,
                     EXP_type);

/*----------------------------------- GLA declarations ---------------------------------*/

BEGIN_GRAMMAR_FRAME(GLA);
  DEFINE_GRAMMAR_SLOT(GLB, glb);
  DEFINE_GRAMMAR_SLOT(VEC, opd);
END_GRAMMAR_FRAME(GLA);

BLN_type      is_GLA(EXP_type);
GLA_type  make_GLA_M(GLB_type,
                     VEC_type);

/*----------------------------------- GLB declarations ---------------------------------*/

BEGIN_GRAMMAR_FRAME(GLB);
  DEFINE_GRAMMAR_SLOT(NBR, scp);
  DEFINE_GRAMMAR_SLOT(NBR, ofs);
END_GRAMMAR_FRAME(GLB);

BLN_type      is_GLB(EXP_type);
GLB_type  make_GLB_M(NBR_type,
                     NBR_type);

/*----------------------------------- IFD declarations ---------------------------------*/

BEGIN_GRAMMAR_FRAME(IFD);
  DEFINE_GRAMMAR_SLOT(EXP, prd);
  DEFINE_GRAMMAR_SLOT(EXP, cns);
  DEFINE_GRAMMAR_SLOT(EXP, alt);
END_GRAMMAR_FRAME(IFD);

BLN_type      is_IFD(EXP_type);
IFD_type  make_IFD_M(EXP_type,
                     EXP_type,
                     EXP_type);

/*----------------------------------- IFS declarations ---------------------------------*/

BEGIN_GRAMMAR_FRAME(IFS);
  DEFINE_GRAMMAR_SLOT(EXP, prd);
  DEFINE_GRAMMAR_SLOT(EXP, cns);
END_GRAMMAR_FRAME(IFS);

BLN_type      is_IFS(EXP_type);
IFS_type  make_IFS_M(EXP_type,
                     EXP_type);

/*----------------------------------- KID declarations ---------------------------------*/

BEGIN_GRAMMAR_FRAME(KID);
  DEFINE_GRAMMAR_SLOT(RWK, rwk);
END_GRAMMAR_FRAME(KID);

BLN_type      is_KID(EXP_type);
KID_type  make_KID_M(RWK_type);

/*----------------------------------- LCA declarations ---------------------------------*/

BEGIN_GRAMMAR_FRAME(LCA);
  DEFINE_GRAMMAR_SLOT(LCL, lcl);
  DEFINE_GRAMMAR_SLOT(VEC, opd);
END_GRAMMAR_FRAME(LCA);

BLN_type      is_LCA(EXP_type);
LCA_type  make_LCA_M(LCL_type,
                     VEC_type);

/*----------------------------------- LCL declarations ---------------------------------*/

BEGIN_GRAMMAR_FRAME(LCL);
  DEFINE_GRAMMAR_SLOT(NBR, ofs);
END_GRAMMAR_FRAME(LCL);

BLN_type      is_LCL(EXP_type);
LCL_type  make_LCL_M(NBR_type);

/*----------------------------------- LET declarations ---------------------------------*/

BEGIN_GRAMMAR_FRAME(LET);
  DEFINE_GRAMMAR_SLOT(NBR, siz);
  DEFINE_GRAMMAR_SLOT(VEC, bnd);
  DEFINE_GRAMMAR_SLOT(EXP, bod);
  DEFINE_GRAMMAR_SLOT(VEC, smv);
END_GRAMMAR_FRAME(LET);

BLN_type      is_LET(EXP_type);
LET_type  make_LET_M(NBR_type,
                     VEC_type,
                     EXP_type,
                     VEC_type);

/*----------------------------------- LMB declarations ---------------------------------*/

BEGIN_GRAMMAR_FRAME(LMB);
  DEFINE_GRAMMAR_SLOT(NBR, par);
  DEFINE_GRAMMAR_SLOT(NBR, siz);
  DEFINE_GRAMMAR_SLOT(EXP, bod);
  DEFINE_GRAMMAR_SLOT(VEC, smv);
END_GRAMMAR_FRAME(LMB);

BLN_type      is_LMB(EXP_type);
LMB_type  make_LMB_M(NBR_type,
                     NBR_type,
                     EXP_type,
                     VEC_type);

/*----------------------------------- LMV declarations ---------------------------------*/

BEGIN_GRAMMAR_FRAME(LMV);
  DEFINE_GRAMMAR_SLOT(NBR, par);
  DEFINE_GRAMMAR_SLOT(NBR, siz);
  DEFINE_GRAMMAR_SLOT(EXP, bod);
  DEFINE_GRAMMAR_SLOT(VEC, smv);
END_GRAMMAR_FRAME(LMV);

BLN_type      is_LMV(EXP_type);
LMV_type  make_LMV_M(NBR_type,
                     NBR_type,
                     EXP_type,
                     VEC_type);

/*----------------------------------- MRK declarations ---------------------------------*/

BLN_type      is_MRK(EXP_type);
NIL_type   apply_MRK(EXP_type);

/*----------------------------------- NAT declarations ---------------------------------*/

BEGIN_GRAMMAR_FRAME(NAT);
  DEFINE_GRAMMAR_SLOT(RNF, rnf);
  DEFINE_GRAMMAR_SLOT(RWC, nam[]);
END_GRAMMAR_FRAME(NAT);

BLN_type      is_NAT(EXP_type);
NAT_type  make_NAT_M(RNF_type,
                     RWS_type);

/*----------------------------------- NBR declarations ---------------------------------*/

BLN_type      is_NBR(EXP_type);
NBR_type    make_NBR(RWN_type);
RWN_type     get_NBR(NBR_type);

/*----------------------------------- NUL declarations ---------------------------------*/

BLN_type      is_NUL(EXP_type);
NUL_type    make_NUL(NIL_type);

/*----------------------------------- ORR declarations ---------------------------------*/

BEGIN_GRAMMAR_FRAME(ORR);
  DEFINE_GRAMMAR_SLOT(VEC, prd);
END_GRAMMAR_FRAME(ORR);

BLN_type      is_ORR(EXP_type);
ORR_type  make_ORR_M(VEC_type);

/*----------------------------------- PAI declarations ---------------------------------*/

BEGIN_GRAMMAR_FRAME(PAI);
  DEFINE_GRAMMAR_SLOT(EXP, car);
  DEFINE_GRAMMAR_SLOT(EXP, cdr);
END_GRAMMAR_FRAME(PAI);

BLN_type      is_PAI(EXP_type);
PAI_type  make_PAI_M(EXP_type,
                     EXP_type);

/*----------------------------------- PRC declarations ---------------------------------*/

BEGIN_GRAMMAR_FRAME(PRC);
  DEFINE_GRAMMAR_SLOT(SYM, nam);
  DEFINE_GRAMMAR_SLOT(NBR, par);
  DEFINE_GRAMMAR_SLOT(NBR, siz);
  DEFINE_GRAMMAR_SLOT(EXP, bod);
  DEFINE_GRAMMAR_SLOT(ENV, env);
  DEFINE_GRAMMAR_SLOT(VEC, smv);
END_GRAMMAR_FRAME(PRC);

BLN_type      is_PRC(EXP_type);
PRC_type  make_PRC_M(SYM_type,
                     NBR_type,
                     NBR_type,
                     EXP_type,
                     ENV_type,
                     VEC_type);

/*----------------------------------- PRM declarations ---------------------------------*/

BEGIN_GRAMMAR_FRAME(PRM);
  DEFINE_GRAMMAR_SLOT(NBR, siz);
  DEFINE_GRAMMAR_SLOT(EXP, exp);
  DEFINE_GRAMMAR_SLOT(ENV, env);
  DEFINE_GRAMMAR_SLOT(VEC, smv);
END_GRAMMAR_FRAME(PRM);

BLN_type      is_PRM(EXP_type);
PRM_type  make_PRM_M(NBR_type,
                     EXP_type,
                     ENV_type,
                     VEC_type);

/*----------------------------------- PRV declarations ---------------------------------*/

BEGIN_GRAMMAR_FRAME(PRV);
  DEFINE_GRAMMAR_SLOT(SYM, nam);
  DEFINE_GRAMMAR_SLOT(NBR, par);
  DEFINE_GRAMMAR_SLOT(NBR, siz);
  DEFINE_GRAMMAR_SLOT(EXP, bod);
  DEFINE_GRAMMAR_SLOT(ENV, env);
  DEFINE_GRAMMAR_SLOT(VEC, smv);
END_GRAMMAR_FRAME(PRV);

BLN_type      is_PRV(EXP_type);
PRV_type  make_PRV_M(SYM_type,
                     NBR_type,
                     NBR_type,
                     EXP_type,
                     ENV_type,
                     VEC_type);

/*----------------------------------- QQU declarations ---------------------------------*/

BEGIN_GRAMMAR_FRAME(QQU);
  DEFINE_GRAMMAR_SLOT(EXP, exp);
END_GRAMMAR_FRAME(QQU);

BLN_type      is_QQU(EXP_type);
QQU_type  make_QQU_M(EXP_type);

/*----------------------------------- REA declarations ---------------------------------*/

BEGIN_GRAMMAR_FRAME(REA);
  DEFINE_GRAMMAR_SLOT(RWR, rwr);
END_GRAMMAR_FRAME(REA);

BLN_type      is_REA(EXP_type);
REA_type  make_REA_M(RWR_type);

/*----------------------------------- SEQ declarations ---------------------------------*/

BLN_type      is_SEQ(EXP_type);
SEQ_type  make_SEQ_Z(UNS_type);
UNS_type    size_SEQ(VEC_type);

/*----------------------------------- STG declarations ---------------------------------*/

BEGIN_GRAMMAR_FRAME(STG);
  DEFINE_GRAMMAR_SLOT(NBR, scp);
  DEFINE_GRAMMAR_SLOT(NBR, ofs);
  DEFINE_GRAMMAR_SLOT(EXP, exp);
END_GRAMMAR_FRAME(STG);

BLN_type      is_STG(EXP_type);
STG_type  make_STG_M(NBR_type,
                     NBR_type,
                     EXP_type);

/*----------------------------------- STL declarations ---------------------------------*/

BEGIN_GRAMMAR_FRAME(STL);
  DEFINE_GRAMMAR_SLOT(NBR, ofs);
  DEFINE_GRAMMAR_SLOT(EXP, exp);
END_GRAMMAR_FRAME(STL);

BLN_type      is_STL(EXP_type);
STL_type  make_STL_M(NBR_type,
                     EXP_type);

/*----------------------------------- STR declarations ---------------------------------*/

BEGIN_GRAMMAR_FRAME(STR);
  DEFINE_GRAMMAR_SLOT(RWC, rws[]);
END_GRAMMAR_FRAME(STR);

BLN_type      is_STR(EXP_type);
STR_type  make_STR_M(RWS_type);
UNS_type    size_STR(RWS_type);

/*----------------------------------- SYM declarations ---------------------------------*/

BEGIN_GRAMMAR_FRAME(SYM);
  DEFINE_GRAMMAR_SLOT(RWC, rws[]);
END_GRAMMAR_FRAME(SYM);

BLN_type      is_SYM(EXP_type);
SYM_type  make_SYM_M(RWS_type);
UNS_type    size_SYM(RWS_type);

/*----------------------------------- THK declarations ---------------------------------*/

BEGIN_GRAMMAR_FRAME(THK);
  DEFINE_GRAMMAR_SLOT(NBR, siz);
  DEFINE_GRAMMAR_SLOT(EXP, bod);
  DEFINE_GRAMMAR_SLOT(VEC, smv);
END_GRAMMAR_FRAME(THK);

BLN_type      is_THK(EXP_type);
THK_type  make_THK_M(NBR_type,
                     EXP_type,
                     VEC_type);

/*----------------------------------- TRU declarations ---------------------------------*/

BLN_type      is_TRU(EXP_type);
TRU_type    make_TRU(NIL_type);

/*----------------------------------- UNQ declarations ---------------------------------*/

BEGIN_GRAMMAR_FRAME(UNQ);
  DEFINE_GRAMMAR_SLOT(EXP, exp);
END_GRAMMAR_FRAME(UNQ);

BLN_type      is_UNQ(EXP_type);
UNQ_type  make_UNQ_M(EXP_type);

/*----------------------------------- UQS declarations ---------------------------------*/

BEGIN_GRAMMAR_FRAME(UQS);
  DEFINE_GRAMMAR_SLOT(EXP, exp);
END_GRAMMAR_FRAME(UQS);

BLN_type      is_UQS(EXP_type);
UQS_type  make_UQS_M(EXP_type);

/*----------------------------------- UDF declarations ---------------------------------*/

BLN_type      is_UDF(EXP_type);
UDF_type    make_UDF(NIL_type);

/*----------------------------------- USP declarations ---------------------------------*/

BLN_type      is_USP(EXP_type);
USP_type    make_USP(NIL_type);

/*----------------------------------- VEC declarations ---------------------------------*/

BLN_type      is_VEC(EXP_type);
VEC_type  make_VEC_M(UNS_type);
VEC_type  make_VEC_Z(UNS_type);
UNS_type    size_VEC(VEC_type);

/*----------------------------------- WHI declarations ---------------------------------*/

BEGIN_GRAMMAR_FRAME(WHI);
  DEFINE_GRAMMAR_SLOT(EXP, prd);
  DEFINE_GRAMMAR_SLOT(EXP, bod);
END_GRAMMAR_FRAME(WHI);

BLN_type      is_WHI(EXP_type);
WHI_type  make_WHI_M(EXP_type,
                     EXP_type);
/*--------------------------------------------------------------------------------------*/

NIL_type Grammar_Initialize(NIL_type);
