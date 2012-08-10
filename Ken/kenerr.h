#if 0

    Copyright (c) 2011-2012, Hewlett-Packard Development Co., L.P.

    All rights reserved.

    Redistribution and use in source and binary forms, with or without
    modification, are permitted provided that the following conditions
    are met:

    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.

    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the
      distribution.

    * Neither the name of the Hewlett-Packard Company nor the names of its
      contributors may be used to endorse or promote products derived
      from this software without specific prior written permission.

    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
    "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
    LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
    A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
    HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
    INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
    BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
    OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
    AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
    LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY
    WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
    POSSIBILITY OF SUCH DAMAGE.

#endif

#ifndef KENERR_H_INCLUDED
#define KENERR_H_INCLUDED

/* Error reporting macros available to application code and to the
   Ken implementation. */

#include <assert.h>
#include <errno.h>
#include <limits.h>  /* for PIPE_BUF */
#include <stdio.h>   /* for snprintf() */
#include <unistd.h>  /* for write() */

/* FPn() macros facilitate formatted write() to stderr without resort
   to variadic macros.  We use write() to avoid anomalies due to
   stdio buffering.  Ken's stderr must pass through a pipe to avoid
   interleaving of messages from the three Unix processes that
   constitute a Ken process. */

#define GP getpid()
#define STR2(s) #s
#define STR(s) STR2(s)
#define COORDS "%d:" __FILE__ ":" STR(__LINE__) ": "

#ifndef KEN_IMPL
#define KENERR_NTF      assert
#define KENERR_KENASRT  assert
#else
#define KENERR_NTF      NTF
#define KENERR_KENASRT  KENASRT
#endif

#define FPSTART do { ssize_t br; char pb[PIPE_BUF]; \
                     int pbck = snprintf(pb, sizeof pb,

#define FPCHEX  KENERR_NTF(0<pbck); KENERR_KENASRT((int)sizeof pb > pbck); \
                br = write(STDERR_FILENO, pb, pbck);                       \
                KENERR_NTF(br==(ssize_t)pbck); } while (0)

#define FP1(   s)                                       \
FPSTART COORDS s, GP);                                  FPCHEX
#define FP2(   s,     a)                                \
FPSTART COORDS s, GP,(a));                              FPCHEX
#define FP3(   s,     a,  b)                            \
FPSTART COORDS s, GP,(a),(b));                          FPCHEX
#define FP4(   s,     a,  b,  c)                        \
FPSTART COORDS s, GP,(a),(b),(c));                      FPCHEX
#define FP5(   s,     a,  b,  c,  d)                    \
FPSTART COORDS s, GP,(a),(b),(c),(d));                  FPCHEX
#define FP6(   s,     a,  b,  c,  d,  e)                \
FPSTART COORDS s, GP,(a),(b),(c),(d),(e));              FPCHEX
#define FP7(   s,     a,  b,  c,  d,  e,  f)            \
FPSTART COORDS s, GP,(a),(b),(c),(d),(e),(f));          FPCHEX
#define FP8(   s,     a,  b,  c,  d,  e,  f,  g)        \
FPSTART COORDS s, GP,(a),(b),(c),(d),(e),(f),(g));      FPCHEX
#define FP9(   s,     a,  b,  c,  d,  e,  f,  g,  h)    \
FPSTART COORDS s, GP,(a),(b),(c),(d),(e),(f),(g),(h));  FPCHEX

#define RESET_ERRNO                                                 \
do {                                                                \
  if (0 != errno) {                                                 \
    FP3("resetting errno == %d: \"%s\"\n", errno, strerror(errno)); \
    errno = 0;                                                      \
  }                                                                 \
} while (0)

#endif

