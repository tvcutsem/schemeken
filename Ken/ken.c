#if 0

    Copyright (c) 2011-2013, Hewlett-Packard Development Co., L.P.

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

#include <errno.h>
#include <fcntl.h>
#include <glob.h>
#include <inttypes.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <arpa/inet.h>

#if !defined _POSIX_SYNCHRONIZED_IO   || \
    !defined _POSIX_FSYNC             || \
    !defined _POSIX_MAPPED_FILES      || \
    !defined _POSIX_MEMORY_PROTECTION
#error
#endif

#include "kencrc.h"
#include "kencom.h"
#include "kenapp.h"
#include "kenext.h"

/* for simplicity the size of this next struct is the basic unit
   of allocation in persistent heap; its size defines alignment */
typedef struct ken_mem_chunk_hdr {
  size_t nbytes;                    /* size of this chunk incl hdr */
  struct ken_mem_chunk_hdr * next;  /* for free list */
} ken_mem_chunk_hdr_t;

/* smallest number of S-sized things that add up to at least L */
#define NPIECES(L,S) (((L)/(S))+(0==((L)%(S))?0:1))

#define VALID_CHUNKSIZE(s) \
(   (2 <= NPIECES(s, sizeof(ken_mem_chunk_hdr_t))) \
 && (0 == ((s) % sizeof(ken_mem_chunk_hdr_t))))

#define VALID_ALLOC_ALIGNMENT(p) \
( 0 == ((const char *)(p) - (char *)0) % sizeof(ken_mem_chunk_hdr_t) )

/* Main data structure:  Ken "state blob".  contains persistent
   heap plus metadata.  Maybe arrange contents so that frequently-
   modified stuff is packed onto same pages; align as necessary (?). */
typedef struct {
  /* fields that are set at birth and remain unmodified thereafter */
  kenid_t self;
  void * mapaddr;    /* address where state blob must be mmap()'d */
  int argc;
  char * argv[64],
         args[4096];
  /* fields that change during/between turns */
  turn_t turn;       /* last successfully completed turn */
  int32_t npeers;
  kenid_t peers[KEN_MAX_NPEERS];  /* communication partners */
  seqno_t  /* sequence nuber of last message... */
    seq_f[KEN_MAX_NPEERS],  /* ... that peer sent and that I consumed */
    seq_t[KEN_MAX_NPEERS],  /* ... that I sent to peer */
    seq_a[KEN_MAX_NPEERS];  /* ... that I sent to peer and that peer ack'd */
  void * appdata;
  int64_t alarmtime;
  /* int32_t alignpad[...];   page-align heap (?) try "127" */
  ken_mem_chunk_hdr_t
    * freelist,
    heap[((size_t)KEN_HEAP_SIZE_MB * 1024*1024)/sizeof(ken_mem_chunk_hdr_t)];
} ken_state_blob_t;

#define BLOB_BYTES  sizeof(ken_state_blob_t)
#define BLOB_PAGES  NPIECES(BLOB_BYTES, PAGESZ)

enum { INVALID_FILEDES  = -999 };

/* Module-global variables.  Those accessed within SIGSEGV handler
   must be declared volatile.  Maybe reconsider use of volatile
   throughout (?). */
static int e_eot_filedes = INVALID_FILEDES;
static int32_t e_noutmsgs;
static off_t e_outmsgoffsets[KEN_MAX_OUTMSGS];
static kenid_t e_my_id;
static ken_state_blob_t * volatile e_state_blob = NULL;
static volatile int32_t e_ndirty;
static int32_t * volatile e_dirtypages;

int ken_heap_ready = 0;

#define CCP(p) ((const char *)(p))

#define PAGE_ALIGNED(p) (0 == (CCP(p) - CCP(0)) % PAGESZ)

#define HEAP_FIRST_BYTE ((char *)&(e_state_blob->heap[0]))
#define HEAP_LAST_BYTE  LAST_BYTE(e_state_blob->heap)

#define BLOB_VALID_PTR(p)           \
(   ( CCP(e_state_blob) <= CCP(p) ) \
 && ( HEAP_LAST_BYTE    >= CCP(p) ) )

#define BLOB_HEAP_PTR(p)          \
(   ( HEAP_FIRST_BYTE <= CCP(p) ) \
 && ( HEAP_LAST_BYTE  >= CCP(p) ) )

#define BLOB_PAGE(addr) \
((int32_t)(((CCP(addr))-CCP(e_state_blob))/PAGESZ))

/* Allocate/initialize scratch bookkeeping space.  For state blobs on
   the order of 1 TB, the number of distinct memory pages in the blob
   is in the hundreds of millions, requiring a 4-byte page ID each and
   therefore an array of dirty pages approaching 1 GB.  So we
   dynamically allocate a fresh dirty page array for each turn, the
   goal being to reduce memory demands if many Ken processes are
   running on a single machine (that has a lot of swap). */
static void i_ken_map_dirtypage_array(void) {
  static void * lastmap = NULL;
  const size_t dirty_page_array_size =
    (BLOB_PAGES * sizeof *e_dirtypages) + (2 * PAGESZ);
  if (NULL != lastmap)
    MUNMAP(lastmap, dirty_page_array_size);
  lastmap = mmap(NULL, dirty_page_array_size, PROT_NONE,
                 MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  NTF(MAP_FAILED != lastmap);
  e_dirtypages = (int32_t *)lastmap;
  while (! PAGE_ALIGNED(e_dirtypages))
    e_dirtypages++;
  KENASRT(PAGE_ALIGNED(e_dirtypages));  /* for mprotect() */
  e_ndirty = 0;
}

int ken_argc(void) { return e_state_blob->argc; }

const char * ken_argv(int idx) {
  APPERR(0 <= idx && ken_argc() >= idx);
  return e_state_blob->argv[idx];
}

#define SET_CURRENT_EOT_OFFSET(var)              \
do { (var) = lseek(e_eot_filedes, 0, SEEK_CUR);  \
     NTF(0 <= (var));                            } while (0)

/* Notes to user: The current implementation of the Ken persistent
   heap is straightforward and is provided as a convenience (compared
   with a simpler page allocator or "here's a pointer to a whole
   bunch of memory you can use" interface).  For better performance,
   an application might use the current ken_malloc() as a page
   allocator and implement an application-specific memory allocator
   atop it.  If memory allocator performance is likely to be
   important to an application, study the current implementation to
   determine if it is likely to be adequate.  Keep in mind that Ken
   checkpoints contain all heap pages dirtied during a turn.
   Therefore, all else being equal, you should prefer a memory
   allocator that dirties as few memory pages as possible. */
void * ken_malloc(size_t size) {
  ken_mem_chunk_hdr_t *r, **p = &(e_state_blob->freelist);
  int32_t nunits = 1 + NPIECES(size, sizeof *r);
  size_t needed = nunits * sizeof *r;
  if (0 == size)
    return NULL;
  while (NULL != *p) {
    KENASRT(BLOB_HEAP_PTR(*p));
    KENASRT(VALID_ALLOC_ALIGNMENT(*p));
    KENASRT(VALID_CHUNKSIZE((*p)->nbytes));
    if (needed <= (*p)->nbytes) {  /* chunk is big enough */
      size_t extra = (*p)->nbytes - needed;
      KENASRT(0 == extra || sizeof *r == extra || VALID_CHUNKSIZE(extra));
      r = *p;
      if (2 > extra / sizeof *r)   /* too small to split, return whole */
        *p = r->next;
      else {                       /* split chunk */
        *p += nunits;
        (*p)->nbytes = extra;
        (*p)->next   = r->next;
        r->nbytes = nunits * sizeof *r;
      }
      r->next = NULL;
      KENASRT(BLOB_HEAP_PTR(r));
      KENASRT(VALID_ALLOC_ALIGNMENT(r));
      KENASRT(BLOB_HEAP_PTR(*p));
      KENASRT(VALID_ALLOC_ALIGNMENT(*p));
      return 1 + r;
    }
    p = &(*p)->next;
  }
  return NULL;
}

/* Growing STL strings/vectors can lead to free list cluttered with
   unusably small chunks, which can impair performance and may
   interact poorly with strict overcommit accounting.  "Binary
   buddies" allocator might help (?).  Another alternative would be to
   expose a "tidy up free list" function to users, or automatically
   tidy up free list upon recovery from crash.  The latter would be
   consistent with the "crash to solve problems" philosophy.  It would
   also encourage practices that stress the recovery code.  Other
   thoughts for possible improvements: Add stricter checks for heap
   corruption, e.g., assertions involving sum of allocated/freed
   chunks; simplify bookkeeping by doing all accounting in units of
   chunks rather than bytes. */
void ken_free(void * p) {
  ken_mem_chunk_hdr_t *t = (ken_mem_chunk_hdr_t *)p;
  if (NULL == p)
    return;
  APPERR(BLOB_HEAP_PTR(p));
  APPERR(VALID_ALLOC_ALIGNMENT(p));
  t -= 1;
  APPERR(VALID_CHUNKSIZE(t->nbytes));
  APPERR(NULL == t->next);
  t->next = e_state_blob->freelist;
  e_state_blob->freelist = t;
  return;
}

int ken_mallocated_pointer(const void * p) {
  const ken_mem_chunk_hdr_t *t;
  if (! BLOB_HEAP_PTR(p))
    return 0;
  if (! VALID_ALLOC_ALIGNMENT(p))
    return 0;
  t = (const ken_mem_chunk_hdr_t *)p;
  t -= 1;
  if (VALID_CHUNKSIZE(t->nbytes) && NULL == t->next)
    return 1;
  else
    return 0;
}

void   ken_set_app_data(void * p) {    APPERR(0 != BLOB_HEAP_PTR(p));
                                       e_state_blob->appdata = p; }
void * ken_get_app_data(void) { return e_state_blob->appdata; }

kenid_t ken_id(void) { return e_my_id; }

const kenid_t
  kenid_NULL   = { { 0, 0, 0, 0 }, 0 },
  kenid_stdin  = { { 0, 0, 0, 0 }, 1 },
  kenid_stdout = { { 0, 0, 0, 0 }, 2 },
  kenid_alarm  = { { 0, 0, 0, 0 }, 3 };

kenid_t ken_id_from_string(const char * id) {
  int ip[4], port, r;
  kenid_t kid;
  APPERR(KEN_ID_BUF_SIZE > strlen(id) && 8 < strlen(id));
  if (0 == strcmp(id, "NULL"))   return kenid_NULL;
  if (0 == strcmp(id, "stdin"))  return kenid_stdin;
  if (0 == strcmp(id, "stdout")) return kenid_stdout;
  if (0 == strcmp(id, "alarm"))  return kenid_alarm;
  r = sscanf(id, "%d.%d.%d.%d:%d", &ip[0], &ip[1], &ip[2], &ip[3], &port);
  APPERR(5 == r);
  APPERR(   (   0 <= ip[0] &&        256 >  ip[0])
         && (   0 <= ip[1] &&        256 >  ip[1])
         && (   0 <= ip[2] &&        256 >  ip[2])
         && (   0 <= ip[3] &&        256 >  ip[3])
         && (1024 <= port  && UINT16_MAX >= port));
  kid.ip[0] = ip[0];
  kid.ip[1] = ip[1];
  kid.ip[2] = ip[2];
  kid.ip[3] = ip[3];
  kid.port  = htons(port);
  APPERR( ! (   0 == ken_id_cmp(kid, kenid_NULL)
             || 0 == ken_id_cmp(kid, kenid_stdin)
             || 0 == ken_id_cmp(kid, kenid_stdout)
             || 0 == ken_id_cmp(kid, kenid_alarm) ) );
  return kid;
}

void ken_id_to_string(kenid_t id, char *buf, int32_t len) {
  int r;
  APPERR(KEN_ID_BUF_SIZE <= len);
  if (0 == ken_id_cmp(id, kenid_NULL))   { sprintf(buf, "NULL");   return; }
  if (0 == ken_id_cmp(id, kenid_stdin))  { sprintf(buf, "stdin");  return; }
  if (0 == ken_id_cmp(id, kenid_stdout)) { sprintf(buf, "stdout"); return; }
  if (0 == ken_id_cmp(id, kenid_alarm))  { sprintf(buf, "alarm");  return; }
  APPERR(1024 <= ntohs(id.port));
  r = snprintf(buf, len, "%d.%d.%d.%d:%d",
               id.ip[0], id.ip[1], id.ip[2], id.ip[3], ntohs(id.port));
  KENASRT(r < len);
}

/* maybe make this same type signature as qsort() comparison ftn (?) */
int ken_id_cmp(kenid_t a, kenid_t b) {
  return memcmp(&a, &b, sizeof a);
}

seqno_t ken_send(kenid_t dest, const void * msg, int32_t len) {
  ken_bcst_t d;
  d.id = dest;
  ken_bcst(&d, 1, msg, len);
  return d.seqno;
}

/* return index of given peer ID, adding peer if necessary */
static int32_t i_ken_peer_index(kenid_t id) {
  int32_t pidx;
  for (pidx = 0; pidx < e_state_blob->npeers; pidx++)
    if (0 == ken_id_cmp(id, e_state_blob->peers[pidx]))
      break;
  KENASRT(pidx <= e_state_blob->npeers);
  if (e_state_blob->npeers == pidx) {
    APPERR(KEN_MAX_NPEERS > e_state_blob->npeers);
    pidx = e_state_blob->npeers++;
    e_state_blob->peers[pidx] = id;
  }
  KENASRT(0 == ken_id_cmp(e_state_blob->peers[pidx], id));
  return pidx;
}

/* Maybe permit zero-length messages (?). */
void ken_bcst(ken_bcst_t * dests, int32_t ndests,
              const void * msg, int32_t len) {
  int32_t i;
  off_t current_eot_offset;
  APPERR(NULL != dests && 0 < ndests && KEN_MAX_NPEERS >= ndests &&
         NULL != msg && 0 < len && KEN_MAX_MSG_BODY >= len);
  APPERR((int32_t)NELEMS(e_outmsgoffsets) > e_noutmsgs);
  for (i = 0; i < ndests; i++) {
    int32_t pidx;
    kenid_t dest = dests[i].id;
    APPERR(   0 != ken_id_cmp(dest, kenid_NULL)
           && 0 != ken_id_cmp(dest, kenid_stdin)
           && 0 != ken_id_cmp(dest, kenid_alarm));
    if (0 < i)
      APPERR(0 > ken_id_cmp(dests[i-1].id, dest));
    pidx = i_ken_peer_index(dest);
    dests[i].seqno    = ++e_state_blob->seq_t[pidx];
    dests[i].reserved =   e_state_blob->seq_f[pidx];
  }
  SET_CURRENT_EOT_OFFSET(current_eot_offset);
  e_outmsgoffsets[e_noutmsgs++] = current_eot_offset;
  CKWRITEVAR(e_eot_filedes, ndests);  /* writev() instead (?) */
  CKWRITE   (e_eot_filedes, dests, ndests * sizeof *dests);
  CKWRITEVAR(e_eot_filedes, len);
  CKWRITE   (e_eot_filedes, msg, (size_t)len);
}

const kenid_t * ken_peers(int32_t * npeers) {
  APPERR(NULL != npeers);
  *npeers = e_state_blob->npeers;
  return &(e_state_blob->peers[0]);
}

const seqno_t * ken_ackd(int32_t * npeers) {
  APPERR(NULL != npeers);
  *npeers = e_state_blob->npeers;
  return &(e_state_blob->seq_a[0]);
}

int64_t ken_current_time(void) {
  struct timeval tv;
  int r = gettimeofday(&tv, NULL);
  NTF(0 == r);
  return (int64_t)tv.tv_sec * 1000000 + (int64_t)tv.tv_usec;
}

/* For extra error check, maybe selectively narrow protection to heap
   only in handler below, versus whole blob, via global flag (?).
   We'd need to allow blob\heap access for, e.g., ken_set_app_data().
   Flag should be volatile sig_atomic_t. */

/* Handler called when read-only page is written.  We must declare
   external variables accessed by the handler "volatile"; see
   Sec. 21.1.3 p. 428 of Kerrisk, _The Linux Programming Interface_.
   Note that the current handler is not thread safe, so if you insist
   on writing multi-threaded code you need to worry about races on
   the shared data accessed by the function below.  Pthread spin
   locks in glibc 2.17 for x86_64 Linux appear to be a safe and
   reasonable way of protecting the shared data accessed by the
   function below; as of April 2013 this approach has survived some
   testing by the MaceKen team without apparent error. */
static void i_ken_SIGSEGV_sigaction(int signo, siginfo_t * info, void * uc) {
  int my_errno;
  void * p;
  int32_t pagenum;  /* beware overflow! */
  /* POSIX 2008 p. 1919 requires that signal handlers on return set
     errno as it was on entry, else value of errno is unspecified. */
  my_errno = errno;
  KENASRT(KEN_MPROTECT_SIG == signo && NULL != uc);
  APPERR(info->si_code == SEGV_ACCERR);
  APPERR(BLOB_VALID_PTR(info->si_addr));
  pagenum = BLOB_PAGE(info->si_addr);
  KENASRT(0 <= pagenum  && (int32_t)BLOB_PAGES > pagenum);
  KENASRT(0 <= e_ndirty && (int32_t)BLOB_PAGES > e_ndirty);
  /* Unprotect next page of array if necessary to avoid SIGSEGV.
     Note that mprotect() is not async-signal-safe and its use in
     signal handlers is not condoned by POSIX.  Alternative designs
     that circumvent use of mprotect() in handler include writing
     pageID to pipe that is read by helper thread that calls
     mprotect() then writes confirmation back over pipe to the
     handler; read() and write() are async-signal-safe. */
  p = &(e_dirtypages[e_ndirty]);
  if (PAGE_ALIGNED(p))
    i_ken_mprotect(p, PAGESZ, PROT_READ | PROT_WRITE);
  e_dirtypages[e_ndirty++] = pagenum;
  p = (char *)e_state_blob + (int64_t)PAGESZ * (int64_t)pagenum;
  i_ken_mprotect(p, PAGESZ, PROT_READ | PROT_WRITE);
  errno = my_errno;
  /* See POSIX 2008 Sec. 2.4.3 p. 489 and C99 Sec. 7.14.1 p. 247. */
}

/* Clean up as necessary, reconstruct & mmap() state blob if present
   in durable storage.  Return address of state blob or NULL if
   recovery was not necessary.  For each end-of-turn (EOT) file, we
   have the following possibilities for the presence of marker files:

   EOT file   _patched   _ackd     possible?   recovery action?
   --------   --------   -------   ---------   ----------------

   exists     exists     exists       yes      delete all three

   exists     exists     doesn't      yes      nothing

   exists     doesn't    exists       yes      patch, delete _ackd then EOT

   exists     doesn't    doesn't      yes      patch, create _patched

   doesn't    exists     exists       no       KENASRT(0)

   doesn't    exists     doesn't      no       KENASRT(0)

   doesn't    doesn't    exists       no       KENASRT(0)

   doesn't    doesn't    doesn't      yes      glob won't reveal it
*/
static void * i_ken_recover(void) {
  char idbuf[KEN_ID_BUF_SIZE], glob_pattern[1024], statefile[1024], *corrupt = NULL;
  glob_t glb;
  int i, r, patch_applied;

  i_ken_state_filename(statefile, sizeof statefile);
  if (EXISTS(statefile))
    i_ken_fsync_file(statefile);

  /* Get list of all end-of-turn files. */
  ken_id_to_string(ken_id(), idbuf, sizeof idbuf);
  r = snprintf(glob_pattern, sizeof glob_pattern, "ken_%s_eot_*", idbuf);
  KENASRT((int)sizeof glob_pattern > r);
  r = glob(glob_pattern, 0, NULL, &glb);
  NTF(0 == r || GLOB_NOMATCH == r);

  /* Sanity-check existence of EOT files and marker files.  Along the
     way fsync() all files to avert confusion due to crashes during
     recovery. */
  for (i = 0; i < (int)glb.gl_pathc; i++) {
    int eot, ack, pat;  /* booleans, really */
    char *f = glb.gl_pathv[i];
    turn_t turn;
    KENASRT(NULL != f && EXISTS(f));
    turn = i_ken_turn_from_eot_file_name(f);
    KENASRT(0 <= turn);
    eot = i_ken_eot_file_exists(turn, "");
    ack = i_ken_eot_file_exists(turn, SFX_ACK);
    pat = i_ken_eot_file_exists(turn, SFX_PAT);
    /* maintain assertion below in sync with comment above function */
    KENASRT( ! ( ! eot && ( ack || pat ) ) );
    /* crash can result in file with no permissions */
    r = chmod(f, S_IRUSR | S_IWUSR);  NTF(0 == r);
    i_ken_fsync_file(f);
  }
  KENASRT((0 == glb.gl_pathc && NULL == glb.gl_pathv) || NULL == glb.gl_pathv[i]);
  i_ken_fsync_file(".");

  /* Delete all EOTs marked for deletion then globfree & re-glob. */
  for (i = 0; i < (int)glb.gl_pathc; i++) {
    turn_t turn = i_ken_turn_from_eot_file_name(glb.gl_pathv[i]);
    if (i_ken_eot_file_exists(turn, SFX_DEL))
      i_ken_delete_eot_file(turn);
  }
  globfree(&glb);
  memset(&glb, 0, sizeof glb);
  r = glob(glob_pattern, 0, NULL, &glb);
  NTF(0 == r || GLOB_NOMATCH == r);

  /* Verify checksums on EOT files and emptiness of marker files.
     Tolerate at most one corrupt EOT file, which happens when a
     crash occurs while writing it. */
  for (i = 0; i < (int)glb.gl_pathc; i++) {
    char *f = glb.gl_pathv[i];
    if (NULL == strstr(f, SFX_PAT) && NULL == strstr(f, SFX_ACK)) {
      /* f is an EOT file, not a marker file */
      if (0 == i_ken_file_size(f)) {
        FP2("zero-byte EOT file \"%s\"\n", f);
        NTF(NULL == corrupt);
        corrupt = f;
      }
      else {
        off_t size;
        uint32_t cksm, cksm2;
        unsigned char * p = (unsigned char *)i_ken_map_file(f, &size);
        MEMCPY_TO_VAR(cksm, p + size - sizeof cksm);
        cksm2 = i_ken_cksum(p, size - sizeof cksm);
        if (cksm != cksm2) {
          FP4("bad EOT file \"%s\" cksums: %u != %u\n", f, cksm, cksm2);
          NTF(NULL == corrupt);
          corrupt = f;
        }
        MUNMAP(p, size);
      }
    }
    else
      KENASRT(0 == i_ken_file_size(f));
  }

  /* If we did something with a corrupt EOT file, e.g., externalize
     its messages or patch its dirty pages into master state file,
     we're hosed.  Also, if there's a corrupt EOT file, it had better
     be last the last EOT in sequence.  Following these checks we
     delete the corrupt file. */
  if (NULL != corrupt) {
    turn_t last_turn = -1,
           corrupt_turn = i_ken_turn_from_eot_file_name(corrupt);
    NTF(! i_ken_eot_file_exists(corrupt_turn, SFX_ACK));
    NTF(! i_ken_eot_file_exists(corrupt_turn, SFX_PAT));
    for (i = 0; i < (int)glb.gl_pathc; i++) {
      char *f = glb.gl_pathv[i];
      if (NULL == strstr(f, SFX_PAT) && NULL == strstr(f, SFX_ACK)) {
        turn_t turn = i_ken_turn_from_eot_file_name(f);
        if (last_turn < turn)
          last_turn = turn;
      }
    }
    NTF(corrupt_turn == last_turn);
    FP2("deleting corrupt EOT file \"%s\"\n", corrupt);
    i_ken_delete_eot_file(corrupt_turn);
  }

#if 0
  /* remove this after verifying that it is no longer needed (?) */

  /* Confusion may result if we try to delete a _patched file and
     then the corresponding EOT file but a crash occurs between the
     two deletions.  We address this possibility below.  (Nothing
     very bad happens if an _ackd file is similarly lost so we don't
     fuss over the latter possibility.) */
  {
    turn_t last_patched = -1;
    for (i = 0; i < (int)glb.gl_pathc; i++) {
      char *f = glb.gl_pathv[i];
      turn_t turn = i_ken_turn_from_eot_file_name(f);
      if (last_patched < turn && i_ken_eot_file_exists(turn, SFX_PAT))
        last_patched = turn;
    }
    for (i = 0; i < (int)glb.gl_pathc; i++) {
      char *f = glb.gl_pathv[i];
      turn_t turn = i_ken_turn_from_eot_file_name(f);
      if (turn < last_patched && ! i_ken_eot_file_exists(turn, SFX_PAT)) {
        FP3("last patched == %" PRId64 " but turn %" PRId64
            " has no _patched; creating _patched file\n",
            last_patched, turn);
        i_ken_create_eot_file(turn, SFX_PAT);
      }
    }
  }
#endif

  /* At this point we shouldn't see gaps in sequence of un-patched
     EOT files. */
  {
    turn_t prev_unpatched = -1;
    for (i = 0; i < (int)glb.gl_pathc; i++) {
      char *f = glb.gl_pathv[i];
      turn_t turn = i_ken_turn_from_eot_file_name(f);
      if (NULL == strstr(f, SFX_PAT) && NULL == strstr(f, SFX_ACK))
        if (! i_ken_eot_file_exists(turn, SFX_PAT)) {
          NTF(-1 == prev_unpatched || 1 + prev_unpatched == turn);
          prev_unpatched = turn;
        }
    }
  }

  /* If both _ackd and _patched exist, delete EOT. */
  for (i = 0; i < (int)glb.gl_pathc; i++) {
    char *f = glb.gl_pathv[i];
    turn_t turn = i_ken_turn_from_eot_file_name(f);
    if (   i_ken_eot_file_exists(turn, SFX_ACK)
        && i_ken_eot_file_exists(turn, SFX_PAT)) {
      KENASRT(i_ken_eot_file_exists(turn, ""));
      i_ken_delete_eot_file(turn);
    }
  }

  /* patch un-patched EOT files into state file; need to check for
     existence of EOT file because we might have deleted it above */
  patch_applied = 0;
  for (i = 0; i < (int)glb.gl_pathc; i++) {
    char *f = glb.gl_pathv[i];
    turn_t turn = i_ken_turn_from_eot_file_name(f);
    if (NULL == strstr(f, SFX_PAT) && NULL == strstr(f, SFX_ACK))
      if (! i_ken_eot_file_exists(turn, SFX_PAT) &&
            i_ken_eot_file_exists(turn, "")) {
        FP2("patching EOT file turn %" PRId64 "\n", turn);
        i_ken_patch_eot_file(turn);
        patch_applied = 1;
      }
  }

  globfree(&glb);
  memset(&glb, 0, sizeof glb);

  /* Map state file into address space at correct location.  Sanity
     checks on state file are done by the caller.  Must make state
     file as large as the state blob before mapping lest references
     beyond end of file generate SIGBUS; see p. 1030 of Kerrisk.  Use
     truncate() rather than ftruncate() because former but not latter
     is guaranteed to work by SUSv3; see Kerrisk p. 103. */
  if (EXISTS(statefile)) {
    if (0 < i_ken_file_size(statefile)) {
      int fd;
      off_t size;
      ken_state_blob_t * tmp = (ken_state_blob_t *)
                               i_ken_map_file(statefile, &size);
      void * final, * addr = tmp->mapaddr;
      KENASRT(sizeof(size_t) == sizeof(off_t));
      KENASRT((size_t)size <= BLOB_BYTES);
      NTF(NULL != addr && PAGE_ALIGNED(addr));
      MUNMAP(tmp, size);
      if ((size_t)size < BLOB_BYTES) {
        FP1("begin truncate (hang here might be due to poor support"
            " for sparse files)\n");
        r = truncate(statefile, BLOB_BYTES);   NTF(0 == r);
        FP1("end truncate\n");
      }
      fd = open(statefile, O_RDONLY);   NTF(0 <= fd);
      final = mmap(addr, BLOB_BYTES, PROT_READ, MAP_PRIVATE, fd, 0);
      NTF(addr == final);
      CLOSE(fd);
      return final;
    }
    else {
      FP2("deleting zero-length state file \"%s\"\n", statefile);
      r = unlink(statefile);    NTF(0 == r);
      return NULL;
    }
  }
  else {
    KENASRT(0 == patch_applied);  /* patching creates a state file */
    return NULL;
  }
}

/* comparison function for sorting array of dirty page IDs */
static int int32cmp(const void *a, const void *b) {
  int32_t A = *(const int32_t *)a, B = *(const int32_t *)b;
  KENASRT(   0 <= A && (int32_t)BLOB_PAGES > A
          && 0 <= B && (int32_t)BLOB_PAGES > B);
  return A - B;
}

/* Read into buffer the next message or input that begins a new
   Ken turn.  Deal with out-of-order messages and incoming acks. */
static void i_ken_next_input(int commsock, char **bufpp, int32_t * msglen,
                             kenid_t * sender, seqno_t * seqno, int wr_pipe,
                             int64_t alarmtime) {
  static char inbuf[65536];
  KENASRT(NULL != bufpp && NULL != msglen && NULL != sender
          && NULL != seqno && -2 < alarmtime && 0 != alarmtime);

#define NEXT_INPUT_RETURN(bp, ml, sd, sn)                          \
do { *bufpp = (bp), *msglen = (ml); *sender = (sd); *seqno = (sn); \
     return; } while (0)

  for (;;) {
    const int maxfd_1 = 1 + (STDIN_FILENO > commsock ? STDIN_FILENO : commsock);
    static int stdin_open = 1;
    fd_set readset;
    int r;
    ssize_t bytes;
    struct timeval tv, *tvp;
    int64_t delay = alarmtime - ken_current_time();
    FD_ZERO(&readset);
    if (0 != stdin_open)
      FD_SET(STDIN_FILENO, &readset);
    FD_SET(commsock, &readset);
    if (-1 == alarmtime)                          /* don't set alarm */
      tvp = NULL;
    else {
      tvp = &tv;
      if (0 > delay)                              /* alarm expired */
        tv.tv_sec = tv.tv_usec = 0;
      else {                                      /* alarm in future */
        tv.tv_sec  = delay / 1000000;
        tv.tv_usec = delay % 1000000;
      }
    }
    /* note that select() sometimes returns early,
       i.e., before the timer fully expires */
    r = select(maxfd_1, &readset, NULL, NULL, tvp);
    NTF(0 <= r);
    if (0 == r)                                   /* timeout */
      NEXT_INPUT_RETURN(NULL, 0, kenid_alarm, 0);
    if (FD_ISSET(STDIN_FILENO, &readset)) {       /* stdin ready */
      bytes = read(STDIN_FILENO, inbuf, sizeof inbuf);
      KENASRT(0 <= bytes);
      if (0 == bytes) {
        FP1("stdin closed\n");
        stdin_open = 0;
      }
      else
        NEXT_INPUT_RETURN(&inbuf[0], (int32_t)bytes, kenid_stdin, 0);
    }
    else {                                        /* socket ready */
      ken_msg_hdr_t * hdr;
      struct sockaddr_in addr;
      socklen_t addrlen = sizeof addr;
      char buf1[1024], buf2[1024];
      const char *rv;
      int32_t pidx;
      ken_eot_notice_t eot_not;
      KENASRT(FD_ISSET(commsock, &readset));
      bytes = recvfrom(commsock, inbuf, sizeof inbuf, 0,
                       (struct sockaddr *)&addr, &addrlen);
      rv = inet_ntop(AF_INET, &addr.sin_addr, buf1, sizeof buf1);
      KENASRT(NULL != rv);
      r = snprintf(buf2, sizeof buf2, "%s:%d", buf1, ntohs(addr.sin_port));
      KENASRT(r < (int)sizeof buf2);
      if ( (                   sizeof *hdr > (size_t)bytes) ||
           (KEN_MAX_MSG_BODY + sizeof *hdr < (size_t)bytes) )
        FP3("DROP %ld bytes from %s\n", bytes, buf2);
      else {
        hdr = (ken_msg_hdr_t *)inbuf;
        KENASRT(0 == ken_id_cmp(ken_id_from_string(buf2), hdr->orig));   /* NAT causes problems here; relax check (?) */
        KENASRT(0 == ken_id_cmp(hdr->cons, e_state_blob->self));
        KENASRT(0 <= hdr->seqno);
        KENASRT(0 <= hdr->ack);
        /*
        FP5("DBG: got %lu bytes %lu msgbytes seqno %" PRId64 " ack %" PRId64 "\n",
            bytes, bytes - sizeof *hdr, hdr->seqno, hdr->ack);
        */
        pidx = i_ken_peer_index(hdr->orig);
        if (hdr->ack > e_state_blob->seq_a[pidx]) { /* every msg has ack */
          eot_not.turn  = -1;
          eot_not.id    = hdr->orig;
          eot_not.seqno = hdr->ack;
          CKWRITEVAR(wr_pipe, eot_not);
          e_state_blob->seq_a[pidx] = hdr->ack;
        }
        if (sizeof *hdr < (size_t)bytes) {          /* has msg, not just ack */
          seqno_t expected = 1 + e_state_blob->seq_f[pidx];
          KENASRT(0 < hdr->seqno);
          if (hdr->seqno < expected) {              /* duplicate msg; re-ack */
            char idbuf[KEN_ID_BUF_SIZE];
            ken_id_to_string(hdr->orig, idbuf, sizeof idbuf);
            FP4("duplicate msg from %s seqno %" PRId64 " expecting %" PRId64 "\n",
                idbuf, hdr->seqno, expected);
            eot_not.turn  = -2;
            eot_not.id    = hdr->orig;
            eot_not.seqno = e_state_blob->seq_f[pidx];
            CKWRITEVAR(wr_pipe, eot_not);
          }
          else if (hdr->seqno == expected) {        /* process message */
            /* It might seem that we're jumping the gun on this next
               line---updating seq_f before we've fully digested the
               message---but this next line will not be committed
               unless the turn completes successfully. */
            /* Maybe drop incoming messages randomly (?).  The intent
               would be to ensure that the retransmission apparatus is
               exercised. */
            KENASRT(1 + e_state_blob->seq_f[pidx] == expected);
            e_state_blob->seq_f[pidx]++;
            NEXT_INPUT_RETURN((char *)(1 + hdr), (int32_t)(bytes - sizeof *hdr),
                              hdr->orig, expected);
          }
          else {                                    /* out-of-order msg */
            KENASRT(hdr->seqno > expected);
            FP4("O-O-O from %s seqno %" PRId64 " > %" PRId64 "\n",
                buf2, hdr->seqno, expected);
          }
        }
      }
    }
  }
}

#define RD_PIPE (pipefds[0])
#define WR_PIPE (pipefds[1])

#define GO_RD_PIPE (gopipefds[0])
#define GO_WR_PIPE (gopipefds[1])

int main(int argc, char *argv[]) {
  int r, commsock, pipefds[2], gopipefds[2];
  long rl;
  pid_t cpid;
  struct sockaddr_in sa;
  char idbuf[KEN_ID_BUF_SIZE];
  struct sigaction SIGSEGV_act;

  /* check assumptions; unfortunately "sizeof" can't be used in #if */
  KENASRT(KEN_MAX_MSG_BODY ==  /* max IP minus IP, UDP, Ken hdrs */
          UINT16_MAX - 60 - 8 - sizeof(ken_msg_hdr_t));
  KENASRT(sizeof(ken_mem_chunk_hdr_t) == 16);

  /* do we want state blob to be exact multiple of page size (?) */
  /* KENASRT(0 == BLOB_BYTES % PAGESZ); */

  /* check arguments */
  APPERR(1 < argc);
  e_my_id = ken_id_from_string(argv[1]);
  APPERR(   0 != ken_id_cmp(e_my_id, kenid_NULL)
         && 0 != ken_id_cmp(e_my_id, kenid_stdin)
         && 0 != ken_id_cmp(e_my_id, kenid_stdout)
         && 0 != ken_id_cmp(e_my_id, kenid_alarm));

  /* check runtime constants */
  rl = sysconf(_SC_PAGESIZE);           NTF(PAGESZ == rl);
#ifndef __APPLE__
	/* OSX lies about supporting SC_SYNCHRONIZED_IO. */
	rl = sysconf(_SC_SYNCHRONIZED_IO);    NTF(     0 <  rl);
#endif
  rl = sysconf(_SC_FSYNC);              NTF(     0 <  rl);
  rl = sysconf(_SC_MAPPED_FILES);       NTF(     0 <  rl);
  rl = sysconf(_SC_MEMORY_PROTECTION);  NTF(     0 <  rl);

  FP1("handler process starting\n");

  /* Note to user:  Here is roughly where you would use setsockopt()
     to set send & receive buffer sizes; see SO_RCVBUF and SO_SNDBUF
     in "man 7 socket".  This requires use of /sbin/sysctl to raise
     ceiling.  The Linux defaults are quite small (roughly 130 KB on
     systems on which Ken has been tested); check using a command
     like this:  "/sbin/sysctl -a | grep mem_max" and you should see
     lines for "net.core.rmem_max" and "net.core.wmem_max".  It would
     be prudent to use getsockopt() to check whether the setsockopt()
     succeeded.  Note that the set operation has tricky semantics
     involving doubling by the kernel; see "man 7 socket". */

  /* set up communication socket */
  (void)memset(&sa, 0, sizeof sa);
  commsock = socket(AF_INET, SOCK_DGRAM, 0);    NTF(-1 != commsock);
  strncpy(idbuf, argv[1], sizeof idbuf);        NTF('\0' == idbuf[sizeof idbuf - 1]);
  *(strchr(idbuf, ':')) = '\0';
  r = inet_pton(AF_INET, idbuf, &sa.sin_addr);  NTF(0 < r);
  sa.sin_family = AF_INET;
  sa.sin_port = e_my_id.port;
  r = bind(commsock, (struct sockaddr *)&sa,
           sizeof sa);                          NTF(0 == r);

  i_ken_set_SIGCHLD_handler();

  /* Note on Linux pipes:  According to pipe(7) manpage, since Linux
     2.6.11 pipe capacity is 65,536 bytes and PIPE_BUF (atomicity
     unit) is 4096 bytes.  The end-of-turn notice that the handler
     process sends to the externalizer is a few bytes, and thousands
     of such messages can be written to the pipe before the handler
     process blocks.  The pipe fills if the externalizer or patcher
     fall behind in their jobs. */

  /* set up pipe for handler->externalizer communication, fork */
  r = pipe(pipefds);    NTF(0 == r);
  r = pipe(gopipefds);  NTF(0 == r);
  cpid = fork();        NTF(-1 != cpid);

  if (0 == cpid) {
    /* child reads pipe, writes stdout & socket */
    FCLOSE(stdin);
    CLOSE(WR_PIPE);
    CLOSE(GO_RD_PIPE);
    i_ken_externalizer(RD_PIPE, GO_WR_PIPE, commsock); /* never returns */
    /* NOTREACHED */
    KENASRT(0);
  }
  else {
    /* parent writes pipe, reads stdin & socket */
    FCLOSE(stdout);
    CLOSE(RD_PIPE);
    CLOSE(GO_WR_PIPE);

    /* set up SIGSEGV handler, which is dirty page tracker */
    SIGSEGV_act.sa_sigaction = i_ken_SIGSEGV_sigaction;
    r = sigemptyset(&SIGSEGV_act.sa_mask);           NTF(0 == r);
    SIGSEGV_act.sa_flags = SA_SIGINFO | SA_RESTART;
    r = sigaction(KEN_MPROTECT_SIG, &SIGSEGV_act, NULL);      NTF(0 == r);

    i_ken_map_dirtypage_array();

    e_state_blob = (ken_state_blob_t *)i_ken_recover();

    if (NULL == e_state_blob) {
      /* Map state blob into middle of big hole in address space.
         On some systems, simply asking for a big empty void in your
         address space doesn't work due to unnecessary gratuitous
         limits on the size that can be requested for a private
         anonymous mapping.  One system appears to have a 4 TB limit,
         for example.  You'd need to tweak the hole size below if your
         system has such limits.  On Linux everything works as
         expected. */
      char * tmp;
      int i;
      size_t remain;
      const size_t holesize = BLOB_BYTES * 15;  /* magic # */
      void * mapaddr, * mmr = mmap(NULL, holesize, PROT_NONE,
                                   MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
      NTF(MAP_FAILED != mmr);
      tmp = (char *)((ken_state_blob_t *)mmr + 7); /* magic # */
      while (! PAGE_ALIGNED(tmp))
        tmp++;
      KENASRT(PAGE_ALIGNED(tmp));
      mapaddr = tmp;
      MUNMAP(mmr, holesize);
      e_state_blob = (ken_state_blob_t *)
                     mmap(mapaddr, BLOB_BYTES, PROT_READ,
                          MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
      NTF(e_state_blob == mapaddr);

      /* initialize state blob */
      e_state_blob->self = e_my_id;
      e_state_blob->mapaddr = e_state_blob;
      APPERR((int)NELEMS(e_state_blob->argv) > argc);
      e_state_blob->argc = argc;
      tmp = &(e_state_blob->args[0]);
      remain = sizeof e_state_blob->args;
      for (i = 0; i < argc; i++) {
        size_t ssz = 1 + strlen(argv[i]);
        APPERR(ssz <= remain);
        e_state_blob->argv[i] = strncpy(tmp, argv[i], remain);
        KENASRT(strlen(argv[i]) == strlen(e_state_blob->argv[i]));
        tmp += ssz;
        remain -= ssz;
      }
      e_state_blob->argv[argc] = NULL;
      e_state_blob->turn       = -1;
      e_state_blob->npeers     =  0;
      e_state_blob->appdata    = NULL;
      e_state_blob->alarmtime  = -1;
      e_state_blob->heap[0].nbytes = sizeof e_state_blob->heap;
      e_state_blob->heap[0].next   = NULL;
      e_state_blob->freelist       = &(e_state_blob->heap[0]);
      KENASRT(VALID_ALLOC_ALIGNMENT(e_state_blob->freelist));
      KENASRT(VALID_CHUNKSIZE(e_state_blob->freelist->nbytes));
    }
    else {
      APPERR(0 == ken_id_cmp(e_state_blob->self, e_my_id));
      KENASRT(e_state_blob->mapaddr == e_state_blob);
      KENASRT(0 <= e_state_blob->turn);
      FP2("recovering from turn %" PRId64 "\n", e_state_blob->turn);
    }

    ken_heap_ready = 1;

    /* notify child that parent has recovered, wait for child to finish recovery */
    {
      ssize_t nbytes;
      int go_msg = 0xABCDEF;
      CKWRITEVAR(WR_PIPE, go_msg);
      nbytes = read(GO_RD_PIPE, &go_msg, sizeof go_msg);
      NTF(sizeof go_msg == nbytes);
      KENASRT(0x987654 == go_msg);
      CLOSE(GO_RD_PIPE);
    }

    FP1("handler process main loop starting\n");

    for (;;) {
      char eot_filename[1024], *bufptr;
      unsigned char * ck;
      int32_t msglen;
      uint32_t cksm;
      int i;
      off_t outmsg_idx_off, dirty_idx_off, cksum_off, fsize;
      seqno_t seqno;
      kenid_t sender;
      struct stat statbuf;

      NTF(INT64_MAX > e_state_blob->turn);
      e_state_blob->turn++;

      /* Ken performance might improve if checkpoints were appended to
         a single file.  Unfortunately there's no portable way (as of
         June 2012) to sparsify a dense file, so reclaiming storage
         blocks from a single checkpoint file is a hassle and possibly
         a performance pessimization in the append-to-one-file
         approach (?). */

      /* open end-of-turn file */
      i_ken_eot_filename(e_state_blob->turn, eot_filename, sizeof eot_filename, "");
      e_eot_filedes = open(eot_filename, O_WRONLY|O_CREAT|O_EXCL, S_IRUSR|S_IWUSR);
      NTF(0 <= e_eot_filedes);

      /* get next message & pass to application handler; first time
         thru the handler gets a null message for initialization */
      bufptr = NULL;
      seqno  = 0;
      msglen = 0;
      if (0 == e_state_blob->turn)
        sender = kenid_NULL;
      else if (0 == e_state_blob->alarmtime)
        sender = kenid_alarm;
      else
        i_ken_next_input(commsock, &bufptr, &msglen, &sender, &seqno,
                         WR_PIPE, e_state_blob->alarmtime);
      /* could prepend sender & seqno to EOT file here; now they're known */
      e_state_blob->alarmtime = ken_handler(bufptr, msglen, sender);
      /* This would be a good place to check for heap corruption,
         e.g., by checking canaries in allocated memory chunks.  More
         general invariant checks could be performed here too, or for
         improved performance they could be done immediately before
         fsync() is called on the EOT file (?). */
      /* Maybe try to dispatch "asynchronous sync" of EOT file here,
         if relevant interfaces permit (?).  In the worst case we
         could always spawn a thread to do the sync & terminate. */
      APPERR(-2 < e_state_blob->alarmtime);

      /* BEGIN PROHIBITION ON STATE BLOB MODIFICATION */
      /* automate enforcement of this rule (?) */

      /* emit outbound message index */
      SET_CURRENT_EOT_OFFSET(outmsg_idx_off);
      CKWRITEVAR(e_eot_filedes, e_noutmsgs);
      CKWRITE(e_eot_filedes, e_outmsgoffsets,
              e_noutmsgs * sizeof e_outmsgoffsets[0]);
      e_noutmsgs = 0;

      /* check dirty pages for redundancy; remove eventually (?) */
      qsort(e_dirtypages, e_ndirty, sizeof e_dirtypages[0], int32cmp);
      for (i = 1; i < e_ndirty; i++)
        KENASRT(e_dirtypages[i-1] < e_dirtypages[i]);

#if 0
      /* debugging:  print IDs of dirty pages */
      if (10 > e_ndirty) {
        char dpbuf[1024];
        int cons = 0;
        for (i = 0; i < e_ndirty; i++) {
          cons += snprintf(&dpbuf[cons], sizeof dpbuf - cons, " %" PRId32,
                           e_dirtypages[i]);
          KENASRT(cons < (int)sizeof dpbuf);
        }
        FP4("turn %" PRId64 " has %" PRId32 " dirty pages: %s\n",
            e_state_blob->turn, e_ndirty, dpbuf);
      }
      else
        FP3("turn %" PRId64 " has %" PRId32 " dirty pages\n",
            e_state_blob->turn, e_ndirty);
#endif

      /* emit dirty page index & data */
      SET_CURRENT_EOT_OFFSET(dirty_idx_off);
      {
        /* is this consistent with rationale & use of volatile (?) */
        int32_t nonvolatile_temp = e_ndirty;
        CKWRITEVAR(e_eot_filedes, nonvolatile_temp);
      }
      CKWRITE(e_eot_filedes, e_dirtypages, e_ndirty * sizeof e_dirtypages[0]);
      for (i = 0; i < e_ndirty; i++) { /* writev() instead (?) */
        const char * p = (char *)e_state_blob
                       + (int64_t)PAGESZ * (int64_t)e_dirtypages[i];
        KENASRT(BLOB_VALID_PTR(p));
        CKWRITE(e_eot_filedes, p, PAGESZ);
      }

      i_ken_map_dirtypage_array();

      /* emit offsets of indices */
      CKWRITEVAR(e_eot_filedes, outmsg_idx_off);
      CKWRITEVAR(e_eot_filedes,  dirty_idx_off);

      SET_CURRENT_EOT_OFFSET(cksum_off);
      CLOSE(e_eot_filedes);
      r = stat(eot_filename, &statbuf);               NTF(0 == r);
      KENASRT(statbuf.st_size == cksum_off);

      /* compute & append checksum */
      ck = (unsigned char *)i_ken_map_file(eot_filename, &fsize);
      cksm = i_ken_cksum(ck, fsize);
      MUNMAP(ck, fsize);
      e_eot_filedes =
        open(eot_filename, O_APPEND | O_WRONLY);      NTF(0 <= e_eot_filedes);
      CKWRITEVAR(e_eot_filedes, cksm);
      CLOSE(e_eot_filedes);
      e_eot_filedes = INVALID_FILEDES;
      r = stat(eot_filename, &statbuf);               NTF(0 == r);
      KENASRT((off_t)(sizeof cksm) + cksum_off
              == statbuf.st_size);

      /*
      FP3("EOT %" PRId64 " file size %" PRIu64 "\n",
          e_state_blob->turn, statbuf.st_size);
      */

      { /* tell externalizer that turn ended */
        ken_eot_notice_t eot_not;
        eot_not.turn  = e_state_blob->turn;
        eot_not.id    = sender;
        eot_not.seqno = seqno;
        CKWRITEVAR(WR_PIPE, eot_not);
      }

      i_ken_mprotect(e_state_blob, sizeof *e_state_blob, PROT_READ);

      /* END PROHIBITION ON STATE BLOB MODIFICATION */
    }
  }

  /* NOTREACHED */
  KENASRT(0);
}

