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

/* Process that commits turn, asynchronously externalizes messages.

   Notes on current implementation: As of early June 2012, the
   implmementation sends messages over UDP, re-transmitting with
   exponential backoff until ack is received.  Two additional
   transport schemes have been developed by the Ken community but
   have not yet been merged into the main stand-alone Ken
   distribution; see the USENIX ATC 2012 paper by Yoo et al. for a
   description of these alternative transport schemes, which have
   been used with MaceKen.  The current implementation sometimes
   benefits from enlarging the UDP socket send and receive buffers,
   to prevent needless datagram loss at heavily loaded receivers.
   Flow control may be implemented by applications that require it
   using the ken_ackd() interface.  The use of large UDP datagrams on
   the wide-area Internet (versus within data centers) is considered
   inadvisable by some; see advisory RFC 5405.  Some network
   providers block UDP traffic; anecdotally this appears to be true
   for several universities, which block incoming UDP packets.  Ken
   can't help you if your data transmission network doesn't transmit
   data. */

#include <fcntl.h>
#include <glob.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <arpa/inet.h>

#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "kencrc.h"
#include "kencom.h"
#include "kenext.h"
#include "kenpat.h"

/* Initial value of retransmission interval, in microseconds.
   Override at compile time to tune this parameter. */
#ifndef KEN_INITIAL_RETRANSMIT_INTERVAL
#define KEN_INITIAL_RETRANSMIT_INTERVAL 250000
#endif

static int e_commsock = -1;

static void i_ken_sendto(kenid_t recip, void * buf, size_t len) {
  ssize_t bytes_written;
  struct sockaddr_in to;
  char addr[17];
  int r;
  KENASRT(NULL != buf && 0 < len && 1+UINT16_MAX >= len);
  (void)memset(&to,   0, sizeof to);
  (void)memset(&addr, 0, sizeof addr);
  to.sin_family = AF_INET;
  to.sin_port   = recip.port;
  r = snprintf(addr, sizeof addr, "%d.%d.%d.%d",
               recip.ip[0], recip.ip[1], recip.ip[2], recip.ip[3]);
  KENASRT((int)sizeof addr > r);
  r = inet_pton(AF_INET, addr, &to.sin_addr);
  KENASRT(0 < r);
  bytes_written = sendto(e_commsock, buf, len, 0,
                         (const struct sockaddr *)&to, sizeof to);
  RESET_ERRNO;
  NTF(bytes_written == (ssize_t)len);
}

static void i_ken_send_ack(kenid_t recip, seqno_t ack) {
  ken_msg_hdr_t hdr;
  KENASRT(   0 != ken_id_cmp(recip, kenid_NULL)
          && 0 != ken_id_cmp(recip, kenid_stdin)
          && 0 != ken_id_cmp(recip, kenid_stdout)
          && 0 != ken_id_cmp(recip, kenid_alarm));
  hdr.orig  = ken_id();
  hdr.cons  = recip;
  hdr.seqno = 0;
  hdr.ack   = ack;
  i_ken_sendto(recip, &hdr, sizeof hdr);
}

typedef struct msg {
  off_t offset;  /* of start of message in EOT file */
  int32_t len;   /* of message */
  kenid_t recip;
  seqno_t seqno, ack;
  struct msg *next, *prev;
} msgrec_t;

typedef struct s2m {
  seqno_t sn;
  msgrec_t * mr;
  struct s2m *next;
} s2m_t;

/* map id/seqno pairs to messages */
static int32_t e_npeers = 0;
static struct {
  kenid_t id;
  s2m_t * msglist;  /* add to front, should be sorted descending on sn */
} e_msgidx[KEN_MAX_NPEERS];

static struct turn {
  turn_t turn;
  int64_t duetime, incr;
  msgrec_t msglist_head;
  struct turn *next, *prev;
} e_turnlist_head;

static void i_ken_insert_turn_record(struct turn *tp) {
  struct turn *p = &e_turnlist_head;
  /* set p such that *tp is inserted after *p on doubly-linked list */
  while (p->next != &e_turnlist_head && p->next->duetime < tp->duetime)
    p = p->next;
  tp->next      = p->next;
  tp->prev      = p;
  p->next->prev = tp;
  p->next       = tp;
}

static int e_patchpipefds[2];
#define PATCHER_RD_PIPE (e_patchpipefds[0])
#define PATCHER_WR_PIPE (e_patchpipefds[1])

static void i_ken_send_messages(struct turn * t) {
  int32_t nmsgs = 0;
  off_t size;
  char *p;
  char filename[1024];
  msgrec_t *mr;
  KENASRT(NULL != t && ken_current_time() >= t->duetime);
  KENASRT(! i_ken_eot_file_exists(t->turn, SFX_ACK));

  i_ken_eot_filename(t->turn, filename, sizeof filename, "");
  p = (char *)i_ken_map_file(filename, &size);

  /* Walk down message list twice; first time send messages, second
     time write outputs if any are present.  Rationale:  writes to
     stdout may block if consumer of stdout is slow, so we send
     messages over the network before doing any potentially slow
     stdout writes.  Whatever is eating a Ken's stdout had better be
     a fast eater, lest it stall Ken. */

  /* first pass */
  for (mr = t->msglist_head.next; mr != &t->msglist_head; mr = mr->next)
    if (0 != ken_id_cmp(kenid_stdout, mr->recip)) {
      char msgbuf[65536];
      ken_msg_hdr_t * hdr = (ken_msg_hdr_t *)msgbuf;
      KENASRT(   0 != ken_id_cmp(kenid_NULL,  mr->recip)
              && 0 != ken_id_cmp(kenid_alarm, mr->recip)
              && 0 != ken_id_cmp(kenid_stdin, mr->recip));
      KENASRT(sizeof *hdr + mr->len <= sizeof msgbuf);
      hdr->orig  = ken_id();
      hdr->cons  = mr->recip;
      hdr->seqno = mr->seqno;
      hdr->ack   = mr->ack;
      memcpy(1 + hdr, p + mr->offset, mr->len);
      /*
      FP5("DBG: sendto tincr %" PRId64
          " %" PRId32 " msgbytes seqno %" PRId64 " ack %" PRId64 "\n",
          t->incr, mr->len, hdr->seqno, hdr->ack);
      */
      i_ken_sendto(mr->recip, msgbuf, sizeof *hdr + mr->len);
      ++nmsgs;
    }

  /* second pass */
  mr = t->msglist_head.next;
  while (mr != &t->msglist_head) {
    if (0 == ken_id_cmp(kenid_stdout, mr->recip)) {
      msgrec_t *del = mr;
      /* think about possible non-atomic write below; maybe use
         "writen()" as in Stevens (?) */
      CKWRITE(STDOUT_FILENO, p + mr->offset, (size_t)mr->len);
      mr = mr->next;
      del->prev->next = del->next;
      del->next->prev = del->prev;
      DEALLOCATE_STRUCT(del);
    }
    else
      mr = mr->next;
  }

  /* third pass is just for debugging; remove (?) */
  for (mr = t->msglist_head.next; mr != &t->msglist_head; mr = mr->next)
    KENASRT(0 != ken_id_cmp(kenid_stdout, mr->recip));

  MUNMAP(p, size);
  p = NULL;

  /* splice *t out of turn record list; below we either delete it or
     re-insert it in its proper place */
  t->next->prev = t->prev;
  t->prev->next = t->next;

  if (0 == nmsgs) {  /* all msgs have been ACK'd, turn is done */
    KENASRT(   t->msglist_head.next == &(t->msglist_head)
            && t->msglist_head.prev == &(t->msglist_head) );
    if (i_ken_eot_file_exists(t->turn, SFX_PAT)) {
      i_ken_delete_eot_file(t->turn);
    }
    else {
      i_ken_create_eot_file(t->turn, SFX_ACK);
      CKWRITEVAR(PATCHER_WR_PIPE, t->turn);
    }
    DEALLOCATE_STRUCT(t);
  }
  else {
    KENASRT(   t->msglist_head.next != &(t->msglist_head)
            && t->msglist_head.prev != &(t->msglist_head) );
    t->duetime += t->incr;
    if ((int64_t)60 * 60 * 1000000 <= t->incr)  /* magic # : one hour */
      FP4("turn %" PRId64 " %d non-ACK msgs, incr %" PRId64 " sec\n",
          t->turn, nmsgs, t->incr / 1000000);
    else
      t->incr *= 2;
    i_ken_insert_turn_record(t);
  }
}

/* Below is sub-optimal (?)  Probably should send most recent ACK
   number, not the one current at the time ken_send() called. */
static void i_ken_process_eot_file(turn_t turn, const char * fn) {
  struct turn * tr = (struct turn *)malloc(sizeof *tr);
  off_t size, outmsg_idx_off;
  char *msgidx, *cksm, * const start = (char *)i_ken_map_file(fn, &size);
  int32_t noutmsgs;
  uint32_t cs;
  NTF(NULL != tr);
  tr->turn    = turn;
  tr->duetime = ken_current_time();
  tr->incr    = KEN_INITIAL_RETRANSMIT_INTERVAL;
  tr->msglist_head.offset =  0;
  tr->msglist_head.len    = -1;
  tr->msglist_head.recip  = kenid_NULL;
  tr->msglist_head.seqno  = -1;
  tr->msglist_head.ack    = -1;
  tr->msglist_head.next =
  tr->msglist_head.prev = &(tr->msglist_head);
  tr->next =
  tr->prev = NULL;
  cksm = start + size - sizeof cs;
  MEMCPY_TO_VAR(cs, cksm);
  NTF(i_ken_cksum((unsigned char *)start, size - sizeof cs) == cs);
  msgidx = cksm - 2 * sizeof outmsg_idx_off;
  MEMCPY_TO_VAR(outmsg_idx_off, msgidx);
  msgidx = start + outmsg_idx_off;
  MEMCPY_TO_VAR(noutmsgs, msgidx);
  KENASRT(0 <= noutmsgs);
  msgidx += sizeof noutmsgs;
  while (0 < noutmsgs--) {  /* for each message; each may be broadcast */
    off_t outmsg_off;
    char *msg, *body, *destptr;
    int32_t ndests, len;
    ken_bcst_t dest;
    MEMCPY_TO_VAR(outmsg_off, msgidx);
    msg = start + outmsg_off;
    MEMCPY_TO_VAR(ndests, msg);
    KENASRT(0 < ndests);
    MEMCPY_TO_VAR(len, msg + sizeof ndests + ndests * sizeof dest);
    KENASRT(0 < len);
    body = msg + sizeof ndests + ndests * sizeof dest + sizeof len;
    destptr = msg + sizeof ndests;
    while (0 < ndests--) {  /* for each destination of the message */
      int32_t pidx;
      msgrec_t * mr = (msgrec_t *)malloc(sizeof *mr);
      NTF(NULL != mr);
      MEMCPY_TO_VAR(dest, destptr);
      KENASRT(0 <  dest.seqno);
      KENASRT(0 <= dest.reserved);
      /* add msg record to turn record's list of msg records */
      mr->offset = (off_t)(body - start);
      mr->len    = len;
      mr->recip  = dest.id;
      mr->seqno  = dest.seqno;
      mr->ack    = dest.reserved;
      /* insert at tail of doubly-linked list; function that sends
         messages will traverse list from head to tail, so we insert
         at tail to send messages in FIFO order */
      mr->next              = &(tr->msglist_head);
      mr->prev              = tr->msglist_head.prev;
      mr->prev->next        = mr;
      tr->msglist_head.prev = mr;
      if (0 != ken_id_cmp(kenid_stdout, mr->recip)) {
        /* add to (peer,seqno)->msg mapping */
        s2m_t * s2mp  = (s2m_t *)malloc(sizeof *s2mp);
        for (pidx = 0; pidx < e_npeers; pidx++)
          if (0 == ken_id_cmp(dest.id, e_msgidx[pidx].id))
            break;
        if (pidx == e_npeers) {
          KENASRT(KEN_MAX_NPEERS > e_npeers);
          e_msgidx[e_npeers++].id = dest.id;
        }
        KENASRT(NULL == e_msgidx[pidx].msglist
                || dest.seqno > e_msgidx[pidx].msglist->sn);
        s2mp->sn   = dest.seqno;
        s2mp->mr   = mr;
        s2mp->next = e_msgidx[pidx].msglist;
        e_msgidx[pidx].msglist = s2mp;
      }
      destptr += sizeof dest;
    }
    msgidx += sizeof outmsg_off;
  }
  i_ken_insert_turn_record(tr);
  MUNMAP((void *)start, size);
  i_ken_send_messages(tr);
}

/* Remove this check function after debugging (?). */
static void i_ken_check_invariants(void) {
  struct turn *tp;
  int32_t pidx, nmsgs_a = 0, nmsgs_b = 0;
  /* FP1("DBG: start check_invariants\n"); */
  /* turn list should be sorted ascending on duetime */
  for (tp = e_turnlist_head.next; tp != &e_turnlist_head; tp = tp->next) {
    msgrec_t *mp;
    /* FP2("DBG:     turn %" PRId64 "\n", tp->turn); */
    /* make sure turn list is proper doubly-linked list */
    KENASRT(tp == tp->next->prev && tp == tp->prev->next);
    if (&e_turnlist_head != tp->prev)
      KENASRT(tp->prev->duetime <= tp->duetime);
    for (mp = tp->msglist_head.next; mp != &tp->msglist_head; mp = mp->next) {
      msgrec_t *mp2;
      /*
      FP3("DBG:         msg seqno %" PRId64 " ack %" PRId64 "\n",
          mp->seqno, mp->ack);
      */
      KENASRT(mp == mp->next->prev && mp == mp->prev->next);
      /* msg records are sorted ascending on seqno as we move head to tail */
      /* this is sub-optimal (?); just check consecutive entries */
      for (mp2 = mp->next; mp2 != &tp->msglist_head; mp2 = mp2->next)
        if (0 == ken_id_cmp(mp->recip, mp2->recip))
          KENASRT(mp->seqno < mp2->seqno);
      nmsgs_a++;
    }
  }
  /* s2m index is consistent with remaining msg recs */
  for (pidx = 0; pidx < e_npeers; pidx++) {
    s2m_t *p, *q = NULL;
    for (p = e_msgidx[pidx].msglist; NULL != p; p = p->next) {
      if (NULL != q)
        KENASRT(q->sn > p->sn);  /* rows of s2m idx are descending on seqno */
      q = p;
      KENASRT(0 == ken_id_cmp(p->mr->recip, e_msgidx[pidx].id));
      KENASRT(p->sn == p->mr->seqno);
      nmsgs_b++;
    }
  }
  KENASRT(nmsgs_a == nmsgs_b);
  /* FP1("DBG: end check_invariants\n"); */
}

void i_ken_externalizer(int pipefd, int gopipefd, int commsock) {
  int r, cpid, go_msg;
  ssize_t nbytes;

  KENASRT(0 < pipefd && 0 < commsock);

  /* don't do anything until parent gives go-ahead message */
  nbytes = read(pipefd, &go_msg, sizeof go_msg);
  NTF(sizeof go_msg == nbytes);
  KENASRT(0xABCDEF == go_msg);

  FP1("externalizer starting\n");

  i_ken_set_SIGCHLD_handler();  /* unnecessary (?) already done in ken.c */

  /* set up pipe to patcher process and fork */
  r = pipe(e_patchpipefds);   NTF(0 == r);
  cpid = fork();              NTF(-1 != cpid);

  if (0 == cpid) { /* child is patcher process, reads pipe from externalizer */
    CLOSE(PATCHER_WR_PIPE);
    FCLOSE(stdout);
    i_ken_patcher(PATCHER_RD_PIPE); /* never returns */
    /* NOTREACHED */
    KENASRT(0);
  }
  else {  /* parent writes pipe to patcher, reads pipe from handler process */
    CLOSE(PATCHER_RD_PIPE);

    /* initialize */
    e_commsock = commsock;
    e_turnlist_head.msglist_head.recip = kenid_NULL;
    e_turnlist_head.next =
    e_turnlist_head.prev = &(e_turnlist_head);

    /* recover */
    {
      char idbuf[KEN_ID_BUF_SIZE], pattern[1024];
      glob_t glb;
      int i;
      ken_id_to_string(ken_id(), idbuf, sizeof idbuf);
      r = snprintf(pattern, sizeof pattern, "ken_%s_eot_*", idbuf);
      KENASRT((int)sizeof pattern > r);
      r = glob(pattern, 0, NULL, &glb);
      NTF(0 == r || GLOB_NOMATCH == r);
      for (i = 0; i < (int)glb.gl_pathc; i++) {
        char *f = glb.gl_pathv[i];
        KENASRT(NULL != f);
        if (! EXISTS(f)) {
          FP2("FILE MISSING: %s\n", f);
          KENASRT(NULL != strstr(f, SFX_PAT));
          FP2("no worries, %s was a " SFX_PAT " file\n", f);
        }
        else {
          /* handler process recovery deletes _ackd files before we get here */
          KENASRT(NULL == strstr(f, SFX_ACK));
          if (NULL == strstr(f, SFX_PAT))
            i_ken_process_eot_file(i_ken_turn_from_eot_file_name(f), f);
        }
      }
      KENASRT(NULL == glb.gl_pathv || NULL == glb.gl_pathv[i]);
      globfree(&glb);
    }
    /* notify parent that externalizer has finished recovery */
    go_msg = 0x987654;
    CKWRITEVAR(gopipefd, go_msg);
    CLOSE(gopipefd);
    /* notify child/patcher that externalizer has finished recovery */
    go_msg = 0xABCDEF;
    CKWRITEVAR(PATCHER_WR_PIPE, go_msg);

    i_ken_check_invariants();

    /* main loop */
    for (;;) {
      ken_eot_notice_t eot_not;

      /* Re-transmit unacknowledged outbound messages that are due,
         as long as incoming pipe from parent is empty.  Pipe from
         parent takes priority over messages due for re-transmission
         because pipe can contain ACKs that reduce need to send
         messages, or EOT notices that require urgent service. */
      do {
        struct turn *tp = e_turnlist_head.next;
        struct timeval tv, *tvp = NULL;
        fd_set readfds;
        if (&e_turnlist_head != tp) {
          int64_t delay = tp->duetime - ken_current_time();
          if (0 > delay)
            delay = 0;
          tv.tv_sec  = delay / 1000000;
          tv.tv_usec = delay % 1000000;
          tvp = &tv;
        }
        FD_ZERO(&readfds);
        FD_SET(pipefd, &readfds);
        r = select(1+pipefd, &readfds, NULL, NULL, tvp);
        NTF(-1 < r);
        if (0 == r) {
          int64_t now = ken_current_time();
          if (now >= tp->duetime)
            i_ken_send_messages(tp);
          else
            FP2("early wake %" PRId64 " usec too soon\n", tp->duetime - now);
        }
        else
          KENASRT(FD_ISSET(pipefd, &readfds));
      } while (0 == r);

      nbytes = read(pipefd, &eot_not, sizeof eot_not);
      if (0 > nbytes) {
        RESET_ERRNO;
        NTF(0);
      }
      if (0 == nbytes) {
        FP1("read pipe returns zero, exit from externalizer\n");
        exit(EXIT_FAILURE);  /* find a more informative exit code (?) */
      }
      KENASRT(sizeof eot_not == nbytes);

      if (-1 == eot_not.turn) {  /* got ack: delete msg recs with seqno <= ack */
        int32_t pidx, ndel = 0;
        s2m_t **ap, *del;
        /* FP2("DBG: got ack seqno %" PRId64 "\n", eot_not.seqno); */
        for (pidx = 0; pidx < e_npeers; pidx++)
          if (0 == ken_id_cmp(eot_not.id, e_msgidx[pidx].id))
            break;
        /* ack from an unknown peer *can* occur: the bad scenario
           involves a re-transmitted message and an ack for the
           original transmission crossing on the wire, then the
           message is garbage collected (EOT file deleted), then a
           crash makes the externalizer forget the peer, then the
           re-ack arrives */
        if (pidx >= e_npeers) {
          char idbuf[KEN_ID_BUF_SIZE];
          KENASRT(pidx == e_npeers);
          ken_id_to_string(eot_not.id, idbuf, sizeof idbuf);
          FP3("ACK from stranger %s seqno %" PRId64 "\n", idbuf, eot_not.seqno);
        }
        else {
          ap = &e_msgidx[pidx].msglist;
          while (NULL != *ap) {
            del = eot_not.seqno >= (*ap)->sn ? *ap : NULL;
            if (NULL != del) {
              msgrec_t *delmr = del->mr;
              *ap = (*ap)->next;  /* splice out list item to be deleted */
              KENASRT(0 == ken_id_cmp(e_msgidx[pidx].id, delmr->recip));
              KENASRT(delmr->seqno == del->sn && del->sn <= eot_not.seqno);
              DEALLOCATE_STRUCT(del);
              /* if *delmr is last msg rec for turn, delete the turn */
              if (delmr->prev == delmr->next) {
                int found = 0;
                struct turn *ch, *tr = (struct turn *)
                  ((char *)delmr->prev - offsetof(struct turn, msglist_head));
                /* verify that *tr is on turn list; maybe remove
                   after debugging (?) */
                for (ch = e_turnlist_head.next; ch != &e_turnlist_head;
                     ch = ch->next)
                  if (tr == ch)
                    found = 1;
                KENASRT(1 == found);
                KENASRT(   tr->msglist_head.next == delmr
                           && tr->msglist_head.prev == delmr);
                tr->next->prev = tr->prev;
                tr->prev->next = tr->next;
                if (i_ken_eot_file_exists(tr->turn, SFX_PAT)) {
                  i_ken_delete_eot_file(tr->turn);
                }
                else {
                  i_ken_create_eot_file(tr->turn, SFX_ACK);
                  CKWRITEVAR(PATCHER_WR_PIPE, tr->turn);
                }
                DEALLOCATE_STRUCT(tr);
              }
              delmr->next->prev = delmr->prev;
              delmr->prev->next = delmr->next;
              DEALLOCATE_STRUCT(delmr);
              ndel++;
            }
            else
              ap = &(*ap)->next;  /* advance down list */
          }
          if (0 == ndel) {  /* ack that deletes no msg recs must be a dup */
            char idbuf[KEN_ID_BUF_SIZE];
            ken_id_to_string(eot_not.id, idbuf, sizeof idbuf);
            FP3("duplicate ACK from %s seqno %" PRId64 "\n", idbuf, eot_not.seqno);
          }
        }
      }
      else if (-2 == eot_not.turn) {   /* re-ack */
        char idbuf[KEN_ID_BUF_SIZE];
        i_ken_send_ack(eot_not.id, eot_not.seqno);
        /* remove next line (?) --- seems useless! */
        ken_id_to_string(eot_not.id, idbuf, sizeof idbuf);
      }
      else { /* ordinary end of turn */
        char eot_filename[1024];
        KENASRT(0 <= eot_not.turn);
        i_ken_eot_filename(eot_not.turn, eot_filename, sizeof eot_filename, "");
        i_ken_fsync_file(eot_filename);
        i_ken_fsync_file(".");
        if (   0 != ken_id_cmp(eot_not.id, kenid_NULL)
            && 0 != ken_id_cmp(eot_not.id, kenid_stdin)
            && 0 != ken_id_cmp(eot_not.id, kenid_alarm))
          i_ken_send_ack(eot_not.id, eot_not.seqno);
        i_ken_process_eot_file(eot_not.turn, eot_filename);
        CKWRITEVAR(PATCHER_WR_PIPE, eot_not.turn);
      }

      i_ken_check_invariants();
    }
  }

  /* NOTREACHED */
  KENASRT(0);
}

