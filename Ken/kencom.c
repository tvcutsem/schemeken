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


#include <fcntl.h>
#include <inttypes.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "kencom.h"

/* See p. 559 sec 26.3.3 Kerrisk book:  "Explicitly setting the
   disposition of SIGCHLD to SIG_IGN causes any child process that
   subsequently terminates to be immediately removed from the system
   instead of being converted into a zombie."  That might be a
   reasonable alternative (?). */
static void i_ken_SIGCHLD_handler(int signo) {
  int my_errno = errno;
  KENASRT(SIGCHLD == signo);
  /* reset errno here (?) */
  FP1("SIGCHLD exit\n");
  exit(EXIT_FAILURE);  /* find a more informative exit code (?) */
  /* NOTREACHED */
  errno = my_errno;
}

void i_ken_set_SIGCHLD_handler(void) {
  int r;
  struct sigaction sa;
  sa.sa_handler = i_ken_SIGCHLD_handler;
  r = sigemptyset(&sa.sa_mask);           NTF(0 == r);
  sa.sa_flags = SA_NOCLDSTOP;
  r = sigaction(SIGCHLD, &sa, NULL);      NTF(0 == r);
}

/* fdatasync() is adequate for Ken's guarantees and may offer better
   performance than fsync() */
void i_ken_fsync_file(const char *fn) {
  int r, fd;
  KENASRT(NULL != fn);
  fd = open(fn, O_RDONLY);  NTF(0 <= fd);
#ifdef __APPLE__
  r  = fcntl(fd, F_FULLFSYNC); NTF(0 == r);
#else
  r  = fsync(fd);           NTF(0 == r);
#endif
  CLOSE(fd);
}

void i_ken_state_filename(char *buf, size_t s) {
  char idbuf[KEN_ID_BUF_SIZE];
  int r;
  KENASRT(NULL != buf);
  ken_id_to_string(ken_id(), idbuf, sizeof idbuf);
  r = snprintf(buf, s, "ken_%s_state", idbuf);
  KENASRT(r < (int)(s));
}

void i_ken_eot_filename(turn_t turn, char *buf, size_t s,
                        const char *suffix) {
  char idbuf[KEN_ID_BUF_SIZE];
  int r;
  KENASRT(0 <= turn && NULL != buf && NULL != suffix);
  KENASRT(   0 == strcmp(suffix, "")
          || 0 == strcmp(suffix, SFX_ACK)
          || 0 == strcmp(suffix, SFX_PAT)
          || 0 == strcmp(suffix, SFX_DEL));
  ken_id_to_string(ken_id(), idbuf, sizeof idbuf);
  r = snprintf(buf, s, "ken_%s_eot_%019" PRId64 "%s", idbuf, turn, suffix);
  KENASRT(r < (int)(s));
}

turn_t i_ken_turn_from_eot_file_name(const char * f) {
  turn_t t;
  const char *p;
  char *end;
  KENASRT(NULL != f);
  p = strstr(f, "_eot_");
  KENASRT(NULL != p);
  p += strlen("_eot_");
  t = strtoimax(p, &end, 10);
  KENASRT('\0' == *end || '_' == *end);
  KENASRT(0 <= t);
  return t;
}

/* sometimes O_EXCL might be too strict (?) */
void i_ken_create_eot_file(turn_t turn, const char *suffix) {
  char filename[1024];
  int fd;
  i_ken_eot_filename(turn, filename, sizeof filename, suffix);
  fd = open(filename, O_CREAT|O_EXCL|O_RDONLY, S_IRUSR);  NTF(-1 != fd);
  CLOSE(fd);
}

int i_ken_eot_file_exists(turn_t turn, const char *suffix) {
  int exists;
  char filename[1024];
  i_ken_eot_filename(turn, filename, sizeof filename, suffix);
  exists = EXISTS(filename);
  if (0 == exists) {
    if (ENOENT == errno) {   /* happens on Linux */
      errno = 0;
    }
    else {
      FP4("WEIRD EXISTENCE errno == %d '%s' on file '%s'\n",
          errno, strerror(errno), filename);
      NTF(0);
    }
  }
  return exists;
}

void i_ken_delete_eot_file(turn_t turn) {
  char filename[1024];
  const char *suf[] = { SFX_PAT, SFX_ACK, "", SFX_DEL, NULL };
  int fd, r, i;
  /* FP2("start del turn %" PRId64 "\n", turn); */
  /* create marker indicating intent to delete this turn */
  i_ken_eot_filename(turn, filename, sizeof filename, SFX_DEL);
  fd = open(filename, O_CREAT | O_RDONLY, S_IRUSR);  NTF(-1 != fd);
  CLOSE(fd);
  /* possible/advantageous to make next two syncs simultaneous (?) */
  i_ken_fsync_file(filename);
  i_ken_fsync_file(".");
  for (i = 0; NULL != suf[i]; i++) {
    i_ken_eot_filename(turn, filename, sizeof filename, suf[i]);
    if (EXISTS(filename)) {
      /* FP2("try del %s\n", filename); */
      r = unlink(filename);
      NTF(0 == r);
      /* FP2("did del %s\n", filename); */
    }
    else {
      /* FP2("no file %s\n", filename); */
      KENASRT(ENOENT == errno);
      errno = 0;
    }
  }
  /* FP2("done del turn %" PRId64 "\n", turn); */
}

void * i_ken_map_file(const char *fn, off_t *size) {
  void *p;
  int r, fd;
  struct stat st;
  KENASRT(NULL != fn && NULL != size);
  r  = stat(fn, &st);                       NTF(0 == r);
  fd = open(fn, O_RDONLY);                  NTF(0 <= fd);
  p  = mmap(NULL, st.st_size, PROT_READ,
            MAP_PRIVATE, fd, 0);
  RESET_ERRNO;                              NTF(MAP_FAILED != p);
  CLOSE(fd);
  *size = st.st_size;
  return p;
}

/* Note that on some Linux systems the man page incorrectly says that
   the first argument is a "const void *" when in fact there is no
   const.  See p. 1319 of POSIX 2008. */
void i_ken_mprotect(void *addr, size_t len, int prot) {
  int r = mprotect(addr, len, prot);
  RESET_ERRNO;
  NTF(0 == r);
}

void i_ken_patch_eot_file(turn_t turn) {
  char eot_filename[1024], *eot_start, state_file_name[1024],
       idbuf[KEN_ID_BUF_SIZE], *p, *dp;
  off_t size, dirty_idx_off, off;
  int r, fd;
  int32_t ndirty, pageid;
  KENASRT(! i_ken_eot_file_exists(turn, SFX_PAT));
  i_ken_eot_filename(turn, eot_filename, sizeof eot_filename, "");
  eot_start = (char *)i_ken_map_file(eot_filename, &size);
  ken_id_to_string(ken_id(), idbuf, sizeof idbuf);
  r = snprintf(state_file_name, sizeof state_file_name, "ken_%s_state", idbuf);
  KENASRT((int)sizeof state_file_name > r);
  fd = open(state_file_name, O_WRONLY | O_CREAT, S_IRUSR | S_IWUSR);
  NTF(-1 != fd);
  p = eot_start + size - sizeof(uint32_t) - sizeof(off_t);
  MEMCPY_TO_VAR(dirty_idx_off, p);
  p = eot_start + dirty_idx_off;
  MEMCPY_TO_VAR(ndirty, p);
  KENASRT(0 < ndirty);
  p += sizeof ndirty;
  dp = p + ndirty * sizeof pageid;
  while (0 < ndirty--) {
    MEMCPY_TO_VAR(pageid, p);
    KENASRT(0 <= pageid);  /* check against BLOB_PAGES too (?) */
    off = lseek(fd, (int64_t)pageid * (int64_t)PAGESZ, SEEK_SET);
    KENASRT(off == (int64_t)pageid * (int64_t)PAGESZ);
    CKWRITE(fd, dp, PAGESZ);
    p  += sizeof pageid;
    dp += PAGESZ;
  }
  CLOSE(fd);
  MUNMAP(eot_start, size);
  /* possible/advantageous to make next two syncs simultaneous (?) */
  i_ken_fsync_file(state_file_name);
  i_ken_fsync_file(".");
  if (i_ken_eot_file_exists(turn, SFX_ACK))
    i_ken_delete_eot_file(turn);
  else
    i_ken_create_eot_file(turn, SFX_PAT);
}

off_t i_ken_file_size(const char *fn) {
  struct stat st;
  int r;
  KENASRT(NULL != fn);
  r = stat(fn, &st);
  NTF(0 == r);
  return st.st_size;
}

