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

#ifndef KEN_H_INCLUDED
#define KEN_H_INCLUDED

#include <stddef.h>  /* for size_t */
#include <stdint.h>

#include "kenerr.h"

#define KEN_MPROTECT_SIG SIGSEGV

#ifdef __APPLE__
#include "kenosx.h"
#endif

/* Limits & constants. */
enum {
  /* Max length of body of message, based on Ken header size,
     IPv4 hdr <= 60 bytes, UDP hdr == 8 bytes, 2^16-1 == 65535. */
  KEN_MAX_MSG_BODY = 65439,
  /* Max number of distinct Ken processes with which messages may
     be exchanged. */
  KEN_MAX_NPEERS = 1024,
  /* Max number of outbound messages that may be sent during a turn.
     A broadcast counts as a single message. */
  KEN_MAX_OUTMSGS = 10 * KEN_MAX_NPEERS,
  /* Length of Ken ID, "nnn.nnn.nnn.nnn:nnnnn", plus nul char. */
  KEN_ID_BUF_SIZE = 22,
  /* Size of memory page; must agree with sysconf(_SC_PAGESIZE).
     Hard-wired because dynamic handling appears difficult: what
     should we do if page size changes between crash and recovery? */
  PAGESZ = 4096
};

/* Replacement for conventional argv/argc, which should not be used.
   Set identical to the conventional argv/argc when a Ken process is
   born.  If a Ken process is re-started after a crash with different
   arguments, these do NOT change. */
extern          int ken_argc(void);
extern const char * ken_argv(int index);

/* For Ken persistent heap.  These act like malloc()/free().
   See implementation for notes on performance. */
extern void * ken_malloc(size_t size);
extern void   ken_free(void * p);

/* Deprecated; avoid use if at all possible.  Returns nonzero iff
   the given pointer appears to be one that could have been returned
   by ken_malloc() and that could safely be passed to ken_free(). */
extern int ken_mallocated_pointer(const void * p);

/* Deprecated; avoid use if at all possible.  Set to nonzero iff
   Ken's persistent heap is ready for use.  This flag is intended for
   use in C++ code that overloads things like operator new.  We must
   check to make sure that Ken's heap is ready because constructors
   in C++ may be called in an uncontrollable order before main() is
   called, and therefore before Ken's infrastructure is properly
   initialized. */
extern int ken_heap_ready;

/* Set and get app_data pointer, entry to persistent heap.  Value
   passed to the set function must be either NULL or a valid address
   on persistent heap.  The app_data pointer is initially NULL.
   If the app_data pointer is not NULL and if it is set to NULL,
   then the heap becomes unreachable and the implementation may
   free all of it.  See kenvar.h for an alternative interface to
   persistent heap entry points. */
extern void   ken_set_app_data(void * p);
extern void * ken_get_app_data(void);

/* Applications should not create kenid_t structures with IP address
   "0.0.0.0" because this IP address is reserved for special uses.
   For normal (not special) Ken IDs, the port is stored in network
   byte order. */
typedef struct { uint8_t ip[4]; uint16_t port; } kenid_t;

/* Returns the Ken ID of the calling Ken process; can't fail. */
extern kenid_t ken_id(void);

/* Special Ken IDs. */
extern const kenid_t
kenid_NULL,     /* not a valid Ken ID */
kenid_stdin,    /* sender of inputs from stdin */
kenid_stdout,   /* for ken_send() to stdout */
kenid_alarm;    /* sender of alarms from self */

/* Converts syntactically valid string representation of KenID
   (IP:PORT pair given as "123.456.789.012:98765") to struct
   representation.  Accepts "NULL", "stdin", "stdout", "alarm". */
extern kenid_t ken_id_from_string(const char * id);

/* Returns string representation of given Ken ID, which may be a
   special Ken ID.  Buffer must be sufficiently large to hold largest
   possible Ken ID string, including terminating nul character. */
extern void ken_id_to_string(kenid_t id, char *buf, int32_t len);

/* Returns zero iff given Ken IDs are the same.  Also serves as a
   comparison function.  In future releases of Ken, the type signature
   may change to match that of the qsort() comparison function. */
extern int ken_id_cmp(kenid_t a, kenid_t b);

typedef int64_t seqno_t;

/* Send message to the given Ken ID, which may include kenid_stdout.
   Returns sequence number; first seqno in each direction is 1.
   See notes on ken_bcst() below. */
extern seqno_t ken_send(kenid_t dest, const void * msg, int32_t len);

typedef struct { kenid_t id; seqno_t seqno, reserved; } ken_bcst_t;

/* Broadcast.  Send given message to all destinations in dests array,
   which must not include duplicate destinations and which must be
   sorted ascending according to ken_id_cmp() function.  Message
   length must be greater than zero and less than the max defined
   elsewhere in this header file.  Sequence numbers are returned in
   seqno field of dests array.  The "reserved" field should not be
   inspected by applications.  Ken guarantees that messages to other
   Ken processes are processed by the recipient exactly once, but the
   semantics of inputs and outputs are weaker: Inputs are read *at
   most once* and outputs are emitted *at least once*.  Applications
   may create "drivers" that control the hardware that mediates
   interactions between the outside world and the Ken processes that
   constitute the "inside world"; such drivers might be able to
   provide stronger input/output semantics.  Application programmers
   are allowed to treat the standard error stream as "write only" and
   freely write messages to stderr, disregarding Ken's turn
   discipline.  See "kenerr.h" for macros that make it easier to
   print formatted output to stderr.  Note that information written
   to stderr in violation of Ken's turn discipline must never find
   its way back into a Ken process; stderr is "write only."  See
   notes on the implementation in kenext.c. */
extern void ken_bcst(ken_bcst_t * dests, int32_t ndests,
                     const void * msg, int32_t len);

/* Returns pointer to array of Ken IDs with whom messages have been
   exchanged and stores number of peers in location pointed to by
   npeers.  List of peers and number of peers change over time, so
   don't make a private copy of this info. */
extern const kenid_t * ken_peers(int32_t * npeers);

/* Returns pointer to array of highest message sequence numbers
   acknowledged by recipients and stores in *npeers the number of
   communication partners, which are ordered in the same way as in
   the Ken ID array returned by ken_peers().  To determine if a given
   message has been ack'd---which in Ken means that it has been
   processed to completion by the recipient---compare the highest ack
   seqno to the seqno of the given message.  If the former is greater
   than or equal to the latter, the message has been processed.  Note
   that outputs---messages sent to kenid_stdout---are not ack'd. */
extern const seqno_t * ken_ackd(int32_t * npeers);

/* Return current time in microseconds since Unix epoch. */
extern int64_t ken_current_time(void);

#endif

