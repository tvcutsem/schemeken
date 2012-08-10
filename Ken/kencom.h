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

#ifndef KENCOM_H_INCLUDED
#define KENCOM_H_INCLUDED

/* Common material, e.g., global variables, utility macros. */

#ifdef NDEBUG
#error do not disable assertions
#endif

#include <assert.h>
#include <errno.h>

#define KEN_IMPL
#include "ken.h"
#undef KEN_IMPL

typedef int64_t turn_t;

/* "a" is a fixed-size array for the next group of macros */
#define ELEMENTSIZE(a)  (sizeof (a)[0])
#define NELEMS(a)       ((sizeof(a))/ELEMENTSIZE(a))
#define LAST_BYTE(a)    ((char *)&((a)[NELEMS(a)-1])+(ELEMENTSIZE(a)-1))

/* Checking macros.  Ordinary assertions are for the application logic
   in the ken_handler() function and the functions it calls; raw
   assert() should not be used in implementation of Ken.  Exceptional
   conditions, e.g., resource exhaustion, may result in error codes
   returned from ken_* functions. */

/* Application logic does something wrong, e.g., passed bad arguments
   into a ken_*() function. */
#define APPERR(e) assert(((void)"APPERR", (e)))

/* "Can't happen" condition. */
#define KENASRT(e) assert(((void)"KENASRT", (e)))

/* Non-tolerated failure. */
#define NTF(e) assert(((void)"NTF", (e)))

#define DEALLOCATE_STRUCT(p)    \
do {                            \
  memset((p), 0, sizeof *(p));  \
  free(p);                      \
  (p) = NULL;                   \
} while (0)

#define EXISTS(file) (0 == access(file, F_OK))

/* Sometimes necessary to do one of these write()s in pieces (?).
   Perhaps write failure here could be tolerated better; see
   "writen()" and surrounding rationale in Stevens.  For writes to
   pipes of less than PIPE_BUF bytes, we shouldn't worry. */
#define CKWRITE(fd, buf, count)                         \
do { ssize_t ck_bytes_wr = write((fd), (buf), (count)); \
     RESET_ERRNO;                                       \
     NTF(ck_bytes_wr == (ssize_t)(count));              } while (0)

#define CKWRITEVAR(fd, var) CKWRITE((fd), &(var), sizeof(var))

/* for reading serialized data from files into in-memory variables;
   alignment constraints may preclude simple cast+deref */
#define MEMCPY_TO_VAR(v, p) memcpy(&(v), (p), sizeof(v))

#define MUNMAP(start, length)                   \
do { int ck_munmap = munmap((start), (length)); \
     NTF(0 == ck_munmap);                       } while (0)

#define CLOSE(fd)              \
do { int ck_close = close(fd); \
     NTF(0 == ck_close);       } while (0)

#define FCLOSE(stream)               \
do { int ck_fclose = fclose(stream); \
     NTF(0 == ck_fclose);            } while (0)

extern void i_ken_set_SIGCHLD_handler(void);

extern void i_ken_fsync_file(const char *fn);

extern void i_ken_state_filename(char *buf, size_t s);

#define SFX_ACK "_ackd"
#define SFX_PAT "_patched"
#define SFX_DEL "_delete"

extern void i_ken_eot_filename(turn_t turn, char *buf, size_t s,
                               const char *suffix);

extern turn_t i_ken_turn_from_eot_file_name(const char * f);

extern void i_ken_create_eot_file(turn_t turn, const char *suffix);

extern int  i_ken_eot_file_exists(turn_t turn, const char *suffix);

extern void i_ken_delete_eot_file(turn_t turn);

extern void * i_ken_map_file(const char *fn, off_t *size);

extern void i_ken_mprotect(void *addr, size_t len, int prot);

extern void i_ken_patch_eot_file(turn_t turn);

extern off_t i_ken_file_size(const char *fn);

#endif

