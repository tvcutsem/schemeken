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

/* Alternative Ken transport implementing Go-Back-N, contributed by
   Sunghwan Yoo of Purdue University / HP Labs.
*/

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

#define KEN_FLOW_CONTROL

#define KEN_WINDOW_SIZE 5            // at most KEN_WINDOW_SIZE number of messages will be sent at a time.


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

typedef struct msg {
  off_t offset;  /* of start of message in EOT file */
  int32_t len;   /* of message */
  kenid_t recip;
  seqno_t seqno, ack;
#ifdef KEN_FLOW_CONTROL
  turn_t turn;
  int64_t starttime;
  int64_t duetime;
#endif
  struct msg *next, *prev;
} msgrec_t;

typedef struct s2m {
  seqno_t sn;
  msgrec_t * mr;
  struct s2m *next;
#ifdef KEN_FLOW_CONTROL
  struct s2m *prev;
#endif
} s2m_t;

/* map id/seqno pairs to messages */
static int32_t e_npeers = 0;
static struct {
  kenid_t id;
  s2m_t * msglist;  /* add to front, should be sorted descending on sn */
#ifdef KEN_FLOW_CONTROL
  // NOTE : KEN_FLOW_CONTROL uses typical GBN protocol, with exponential back-off.
  int64_t min_duetime;      // it should be 0 if msglist == NULL
  seqno_t last_acked_seqno; // start of window
  seqno_t last_sent_seqno;  // end of window
  seqno_t ack;              // piggybacked ack to send
#endif
} e_msgidx[KEN_MAX_NPEERS];

static struct turn {
  turn_t turn;
  int64_t duetime, incr;
  msgrec_t msglist_head;
  struct turn *next, *prev;
} e_turnlist_head;


/* function declaration */

#ifdef KEN_FLOW_CONTROL
int get_pidx(kenid_t recip);
int get_min_duetime_pidx();
static void print_queue(int pidx);
int get_queue_len(int pidx);
static int i_ken_send_new_messages(int pidx);
static int i_ken_resend_messages(int pidx);
static int32_t i_ken_delete_message(int32_t pidx, seqno_t seq, s2m_t **ap);
static void i_ken_queue_messages(struct turn * t);
#else
static void i_ken_send_messages(struct turn * t);
#endif

static void i_ken_insert_turn_record(struct turn *tp);
//static void i_ken_sendto(kenid_t recip, void * buf, size_t len);
static void i_ken_send_ack(kenid_t recip, seqno_t ack);
static void i_ken_process_eot_file(turn_t turn, const char * fn);
static void i_ken_check_invariants(void);
void i_ken_externalizer(int pipefd, int gopipefd, int commsock);



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




#ifdef KEN_FLOW_CONTROL

int get_pidx(kenid_t recip)
{
  int pidx = 0;
  for (pidx = 0; pidx < e_npeers; pidx++)
    if (0 == ken_id_cmp(recip, e_msgidx[pidx].id))
      return pidx;
  return -1;
}

int get_min_duetime_pidx()
{
  int pidx = 0;
  int min_pidx = -1;
  int64_t min_time = 0;
  for (pidx = 0; pidx < e_npeers; pidx++) {
    if( e_msgidx[pidx].min_duetime != 0 ) {
      if( min_time == 0 ) {
        min_time = e_msgidx[pidx].min_duetime;
        min_pidx = pidx;
      } else {
        if( e_msgidx[pidx].min_duetime < min_time ) {
          min_time = e_msgidx[pidx].min_duetime;
          min_pidx = pidx;
        }
      }
    }
  }
  return min_pidx;
}

static void print_queue(int pidx)
{
  KENASRT(pidx >= 0 && pidx < e_npeers);

  int count = 20;

  char idbuf[KEN_ID_BUF_SIZE];
  char buf[65535];
  char temp[65535];
  buf[0] = '\0';

  ken_id_to_string(e_msgidx[pidx].id, idbuf, sizeof idbuf);

  s2m_t *p = NULL;
  s2m_t *header = e_msgidx[pidx].msglist;

  if( header == NULL ) return;

  seqno_t start_sn = header->sn;

  for (p = e_msgidx[pidx].msglist; ; p = p->next) {
    sprintf(temp, " (%d) <- %d -> (%d)", (int) p->prev->sn, (int) p->sn, (int) p->next->sn);
    strcat(buf, temp);
    if( p->next->sn == start_sn ) {
      break;
    }
    count--;
    if(count<=0) break;
  }
  buf[65534] = '\0';
  FP4("STATUS queue[%s] =%s, min_duetime = %ld\n", idbuf, buf, (long int) e_msgidx[pidx].min_duetime );
}


int get_queue_len(int pidx)
{
  int32_t nmsgs = 0;
  KENASRT(pidx >= 0 && pidx < e_npeers);

  s2m_t *p = NULL;
  s2m_t *header = e_msgidx[pidx].msglist;

  if( header == NULL ) return 0;

  seqno_t start_sn = header->sn;

  for (p = e_msgidx[pidx].msglist; ; p = p->next) {
    nmsgs++;
    if( p->next->sn == start_sn ) {
      break;
    }
  }

  return nmsgs;
}

static int i_ken_send_new_messages(int pidx) {
  /* send any remaining */
  int nmsgs_sent = 0;
  int nmsgs_queued = 0;
  s2m_t *p, *q = NULL;
  s2m_t *header = e_msgidx[pidx].msglist;

  KENASRT(pidx >= 0 && pidx < e_npeers);

  int window_size = e_msgidx[pidx].last_sent_seqno - e_msgidx[pidx].last_acked_seqno;
  //KENASRT(window_size >= 0 && window_size <= KEN_WINDOW_SIZE);
  KENASRT(window_size >= 0);


  if( header == NULL ) {
    return nmsgs_queued;
  } else {
    /* after reborn, this information would be different. */
    //char idbuf[KEN_ID_BUF_SIZE];
    //ken_id_to_string(e_msgidx[pidx].id, idbuf, sizeof idbuf);

    if( e_msgidx[pidx].last_acked_seqno == 0 && e_msgidx[pidx].last_sent_seqno == 0 ) {
      e_msgidx[pidx].last_acked_seqno = header->prev->sn - 1;
      e_msgidx[pidx].last_sent_seqno = header->prev->sn - 1;

      //FP6("RECOVERY-QUEUE e_msgidx[%s].last_acked_seqno = %d, last_sent_seqno = %d, ack = %d, min_duetime = %ld\n", idbuf,
      //  (int) e_msgidx[pidx].last_acked_seqno, (int) e_msgidx[pidx].last_sent_seqno, (int) e_msgidx[pidx].ack, (long int) e_msgidx[pidx].min_duetime);
    } else {
      //FP6("STATUS-QUEUE   e_msgidx[%s].last_acked_seqno = %d, last_sent_seqno = %d, ack = %d, min_duetime = %ld\n", idbuf,
      //  (int) e_msgidx[pidx].last_acked_seqno, (int) e_msgidx[pidx].last_sent_seqno, (int) e_msgidx[pidx].ack, (long int) e_msgidx[pidx].min_duetime);

    }

    // NOTE : This cannot be true if we have "gaps" after recovery.
    //KENASRT(header->prev->sn == e_msgidx[pidx].last_acked_seqno + 1);
    //if( header->prev->sn != e_msgidx[pidx].last_acked_seqno + 1 ) {
      //print_queue(pidx);
    //}
    //KENASRT(header->prev->sn == e_msgidx[pidx].last_acked_seqno + 1);
    /* NOTE : there may be gaps after recovery. If sender queues up a new message
     * while the sender doesn't get recent ack to update last_acked_seqno,
     * header->prev->sn == e_msgidx[pidx].last_acked_seqno + 1 may not be true.
     */
    KENASRT(header->prev->sn >= e_msgidx[pidx].last_acked_seqno + 1);
    KENASRT(header->sn >= e_msgidx[pidx].last_sent_seqno);
  }

  //print_queue(pidx);

  int r = 0;



  for( p = e_msgidx[pidx].msglist->prev; ; p = p->prev ) {
    //if( e_msgidx[pidx].last_sent_seqno < p->sn && p->sn <= e_msgidx[pidx].last_acked_seqno + e_msgidx[pidx].window_size) {
    if( e_msgidx[pidx].last_sent_seqno < p->sn
    ) {
      //KENASRT(p->sn == e_msgidx[pidx].last_sent_seqno + 1);
      // There may be gaps, so we don't strictly check the orders.
      //KENASRT(p->sn > e_msgidx[pidx].last_sent_seqno + 1);
      //KENASRT(p->sn - e_msgidx[pidx].last_acked_seqno <= KEN_WINDOW_SIZE);
      msgrec_t *mr = p->mr;

      KENASRT(p->sn == mr->seqno);

      if( e_msgidx[pidx].ack < mr->ack )
        e_msgidx[pidx].ack = mr->ack;

      char msgbuf[65536];
      char src[1024];
      char dest[1024];
      char *turn_p;
      char filename[1024];
      off_t size;
      KENASRT(! i_ken_eot_file_exists(mr->turn, SFX_ACK));

      i_ken_eot_filename(mr->turn, filename, sizeof filename, "");
      turn_p = (char*) i_ken_map_file(filename, &size);

      ken_id_to_string(ken_id(), src, 1024);
      ken_id_to_string(mr->recip, dest, 1024);
      ken_msg_hdr_t * hdr = (ken_msg_hdr_t *)msgbuf;
      KENASRT(   0 != ken_id_cmp(kenid_NULL,  mr->recip)
              && 0 != ken_id_cmp(kenid_alarm, mr->recip)
              && 0 != ken_id_cmp(kenid_stdin, mr->recip)
              && 0 != ken_id_cmp(kenid_stdout, mr->recip));
      KENASRT(sizeof *hdr + mr->len <= sizeof msgbuf);
      hdr->orig  = ken_id();
      hdr->cons  = mr->recip;
      hdr->seqno = mr->seqno;
      hdr->ack   = e_msgidx[pidx].ack;
      memcpy(1 + hdr, turn_p + mr->offset, mr->len);

      //FP7("SEND-MSG dest %s resend_time %" PRId64
      //    " %" PRId32 " msgbytes seqno %" PRId64 " ack %" PRId64 " nmsgs %d\n",
      //    dest, (mr->duetime - mr->starttime), mr->len, hdr->seqno, hdr->ack, nmsgs_sent);
      i_ken_sendto(mr->recip, msgbuf, sizeof *hdr + mr->len);

      /* update */
      e_msgidx[pidx].last_sent_seqno = p->sn;
      mr->duetime = ken_current_time() + KEN_INITIAL_RETRANSMIT_INTERVAL;
      if( nmsgs_sent == 0 ) {
        e_msgidx[pidx].min_duetime = mr->duetime;
      }

      ++nmsgs_sent;
    }

    ++nmsgs_queued;

    if( p == header )
      break;

    if( q != NULL ) {
      KENASRT( p->sn > q->sn);
    }

    q = p;
  }

  //print_queue(pidx);

  return nmsgs_queued;
}

static int i_ken_resend_messages(int pidx)
{
  int nmsgs = 0;
  s2m_t *p, *q = NULL;
  s2m_t *header = e_msgidx[pidx].msglist;

  KENASRT(pidx >= 0 && pidx < e_npeers);

  int window_size = e_msgidx[pidx].last_sent_seqno - e_msgidx[pidx].last_acked_seqno;
  //KENASRT(window_size >= 0 && window_size <= KEN_WINDOW_SIZE);
  KENASRT(window_size >= 0);

  //char idbuf[KEN_ID_BUF_SIZE];
  //ken_id_to_string(e_msgidx[pidx].id, idbuf, sizeof idbuf);

  if( header == NULL ) {
    /* min_duetime should be updated */
    //FP3("RESEND-NULL to %s, pidx = %d\n", idbuf, pidx);
    e_msgidx[pidx].min_duetime = 0;
    return nmsgs;
  } else {
    KENASRT(header->prev->sn == e_msgidx[pidx].last_acked_seqno + 1);
    KENASRT(header->sn >= e_msgidx[pidx].last_sent_seqno);
  }

  int r;

  // A     S     W

  for( p = e_msgidx[pidx].msglist->prev; ; p = p->prev ) {
    if( e_msgidx[pidx].last_acked_seqno < p->sn
    ) {
      msgrec_t *mr = p->mr;

      KENASRT(p->sn == mr->seqno);

      if( e_msgidx[pidx].ack < mr->ack )
        e_msgidx[pidx].ack = mr->ack;

      char msgbuf[65536];
      char src[1024];
      char dest[1024];
      char *turn_p;
      char filename[1024];
      off_t size;
      KENASRT(! i_ken_eot_file_exists(mr->turn, SFX_ACK));

      i_ken_eot_filename(mr->turn, filename, sizeof filename, "");
      turn_p = (char*) i_ken_map_file(filename, &size);

      ken_id_to_string(ken_id(), src, 1024);
      ken_id_to_string(mr->recip, dest, 1024);
      ken_msg_hdr_t * hdr = (ken_msg_hdr_t *)msgbuf;
      KENASRT(   0 != ken_id_cmp(kenid_NULL,  mr->recip)
              && 0 != ken_id_cmp(kenid_alarm, mr->recip)
              && 0 != ken_id_cmp(kenid_stdin, mr->recip)
              && 0 != ken_id_cmp(kenid_stdout, mr->recip));
      KENASRT(sizeof *hdr + mr->len <= sizeof msgbuf);
      hdr->orig  = ken_id();
      hdr->cons  = mr->recip;
      hdr->seqno = mr->seqno;
      hdr->ack   = e_msgidx[pidx].ack;

      memcpy(1 + hdr, turn_p + mr->offset, mr->len);
      //FP7("RESEND-MSG dest %s resend_time %" PRId64
      //    " %" PRId32 " msgbytes seqno %" PRId64 " ack %" PRId64 " nmsgs %d\n",
      //    dest, (mr->duetime - mr->starttime), mr->len, hdr->seqno, hdr->ack, nmsgs);

      i_ken_sendto(mr->recip, msgbuf, sizeof *hdr + mr->len);


      /* update min_duetime */
      mr->duetime = ken_current_time() + KEN_INITIAL_RETRANSMIT_INTERVAL;
      if( nmsgs == 0 ) {
        e_msgidx[pidx].min_duetime = mr->duetime;
      }
      ++nmsgs;

    }

    if( p == header )
      break;

    if( q != NULL ) {
      KENASRT( p->sn > q->sn);
    }

    q = p;
  }

   //KENASRT(nmsgs <= e_msgidx[pidx].window_size);

  return nmsgs;
}
#endif

static int32_t i_ken_delete_message(int32_t pidx, seqno_t seq, s2m_t **ap)
{
  int32_t ndel = 0;
  s2m_t *del;

#ifdef KEN_FLOW_CONTROL
  if( *ap == NULL ) {
    return ndel;
  }

  //char idbuf[KEN_ID_BUF_SIZE];
  //ken_id_to_string(e_msgidx[pidx].id, idbuf, sizeof idbuf);

  KENASRT(e_msgidx[pidx].msglist != NULL);
  KENASRT(e_msgidx[pidx].msglist->prev != NULL);
  KENASRT(e_msgidx[pidx].msglist->next != NULL);


  KENASRT((*ap)->next != NULL);
  KENASRT((*ap)->prev != NULL);

  /* This is inefficient. Traversing from the tail will make it possible that always delete something. (?) */

  while (1) {

    seqno_t start_sn = e_msgidx[pidx].msglist->prev->sn; /* This sn will be the last element to visit. */
    seqno_t last_sn = e_msgidx[pidx].msglist->sn;        /* This sn will be the first element to visit. */

    del = (seq >= (*ap)->sn) ? *ap : NULL;

    if (NULL != del) {
      msgrec_t *delmr = del->mr;
      seqno_t current_sn = (*ap)->sn;
      /* splice out list item to be deleted */
      KENASRT(0 == ken_id_cmp(e_msgidx[pidx].id, delmr->recip));
      KENASRT(delmr->seqno == del->sn && del->sn <= seq);
      KENASRT(del->sn == (*ap)->sn);

      //FP6("DELETE  seq %d from list of %s. start_sn = %d, last_sn = %d, del_until = %d\n", (int)delmr->seqno, idbuf, (int)start_sn, (int)last_sn, (int)seq);

      /* update pointers */
      (*ap)->next->prev = (*ap)->prev;
      (*ap)->prev->next = (*ap)->next;
      // you don't need to advance list.

      /* if you are deleting last_sn, advance it */
      if( del->sn == last_sn ) {
        *ap = (*ap)->next;
      }

//      if( *ap == NULL ) {
//        ap = &(*ap)->next;  /* advance down list if we are about to remove the head */
//      }

      //print_queue(pidx);

      // acked message should only be "sent" messages.
      //KENASRT( del->sn <= e_msgidx[pidx].last_sent_seqno );

      DEALLOCATE_STRUCT(del);
      /* if *delmr is last msg rec for turn, delete the turn */
      if (delmr->prev == delmr->next) {
        int found = 0;
        struct turn *ch, *tr = (struct turn *)
          ((char *)delmr->prev - offsetof(struct turn, msglist_head));
        /* verify that *tr is on turn list; maybe remove */
        for (ch = e_turnlist_head.next; ch != &e_turnlist_head; ch = ch->next)
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
      /* if we have null data structure, remove all! */

      DEALLOCATE_STRUCT(delmr);
      ndel++;


      /* update last_acked_seqno */
      if( current_sn > e_msgidx[pidx].last_acked_seqno ) {
//        FP3("UPDATE  e_msgidx[%s].last_acked_seqno = %d\n", idbuf, (int)current_sn);
        e_msgidx[pidx].last_acked_seqno = current_sn;
      }

      /* if we deleted the only remaining packet, reset msglist */
      if( last_sn == start_sn ) {
        KENASRT(e_msgidx[pidx].msglist != NULL);
        KENASRT(e_msgidx[pidx].msglist->prev == NULL);
        KENASRT(e_msgidx[pidx].msglist->next == NULL);
        e_msgidx[pidx].msglist = NULL;
        //print_queue(pidx);
        break;
      } else {
        //print_queue(pidx);
      }

//      FP4("ap->sn = %d, current_sn = %d, start_sn = %d\n", (int)(*ap)->sn, (int)current_sn, (int)start_sn);

      if( current_sn == start_sn ) {
        // not always.
        break;
      }

    }
    else {
      /* if visited the last element, exit loop */
      if( (*ap)->sn == start_sn ) {
//        FP3("VISITED  seq %d, start_sn = %d.\n", (int)(*ap)->sn, (int)start_sn);
        break;
      } else {
//        FP3("VISITING seq %d, start_sn = %d.\n", (int)(*ap)->sn, (int)start_sn);
      }

      ap = &(*ap)->next;  /* advance down list */
    }
  }
  /* e_msgidx[].last_acked_seqno should be updated */
  if( e_msgidx[pidx].last_acked_seqno < seq ) {
    if( e_msgidx[pidx].msglist != NULL ) {
      KENASRT(e_msgidx[pidx].msglist->prev->sn > seq);
    }
    e_msgidx[pidx].last_acked_seqno = seq;
  }
  if( e_msgidx[pidx].last_sent_seqno < seq ) {
    e_msgidx[pidx].last_sent_seqno = seq;
  }

  //FP5("UPDATE  e_msgidx[%s].queue_len = %d, last_acked_seqno = %d, last_sent_seqno = %d\n", idbuf, (int)get_queue_len(pidx),
  //    (int)e_msgidx[pidx].last_acked_seqno, (int)e_msgidx[pidx].last_sent_seqno);

#else
  while (NULL != *ap) {
    del = (seq >= (*ap)->sn) ? *ap : NULL;
    if (NULL != del) {
      msgrec_t *delmr = del->mr;
      *ap = (*ap)->next;  /* splice out list item to be deleted */
      KENASRT(0 == ken_id_cmp(e_msgidx[pidx].id, delmr->recip));
      KENASRT(delmr->seqno == del->sn && del->sn <= seq);
      DEALLOCATE_STRUCT(del);
      /* if *delmr is last msg rec for turn, delete the turn */
      if (delmr->prev == delmr->next) {
        int found = 0;
        struct turn *ch, *tr = (struct turn *)
          ((char *)delmr->prev - offsetof(struct turn, msglist_head));
        /* verify that *tr is on turn list; maybe remove */
        for (ch = e_turnlist_head.next; ch != &e_turnlist_head; ch = ch->next)
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
#endif
  return ndel;
}

#ifdef KEN_FLOW_CONTROL
static void i_ken_queue_messages(struct turn * t) {
  int32_t nmsgs = 0;
  off_t size;
  char *p;
  char filename[1024];
  msgrec_t *mr;

//  KENASRT(NULL != t && ken_current_time() >= t->duetime);
  KENASRT(! i_ken_eot_file_exists(t->turn, SFX_ACK));

  i_ken_eot_filename(t->turn, filename, sizeof filename, "");
  p = (char*) i_ken_map_file(filename, &size);

  //FP2("PROCESS-QUEUE-MSG turn = %d\n", (int)t->turn);

  /* Walk down message list twice; first time send messages, second
     time write outputs if any are present.  Rationale:  writes to
     stdout may block if consumer of stdout is slow, so we send
     messages over the network before doing any potentially slow
     stdout writes.  Whatever is eating a Ken's stdout had better be
     a fast eater, lest it stall Ken. */

  /* first pass : process new messages per list */
  /* we will use e_msgidx[] rather than using turn. */
  for (mr = t->msglist_head.next; mr != &t->msglist_head; mr = mr->next) {
    if (0 != ken_id_cmp(kenid_stdout, mr->recip)) {
      int pidx = get_pidx(mr->recip);
      KENASRT(pidx != -1);
      nmsgs += i_ken_send_new_messages(pidx); // it returns remaining messages in the queue.
    }
  }

  //FP2("PROCESS-QUEUE-MSG remaining msgs = %d\n", (int)nmsgs);

  /* second pass : process STDOUT */
  mr = t->msglist_head.next;
  while (mr != &t->msglist_head) {
    if (0 == ken_id_cmp(kenid_stdout, mr->recip)) {
      msgrec_t *del = mr;
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
//    KENASRT(   t->msglist_head.next != &(t->msglist_head)
//            && t->msglist_head.prev != &(t->msglist_head) );
//    t->duetime += t->incr;
//    if ((int64_t)60 * 60 * 1000000 <= t->incr)  /* magic # : one hour */
//      FP4("turn %" PRId64 " %d non-ACK msgs, incr %" PRId64 " sec\n",
//          t->turn, nmsgs, t->incr / 1000000);
//    else {
//      t->incr *= 2;
//    }
//#ifdef KEN_MAX_RETRANSMIT_INTERVAL
//    if( t->incr > KEN_MAX_RETRANSMIT_INTERVAL ) {
//      t->incr = KEN_MAX_RETRANSMIT_INTERVAL;
//    }
//#endif

    i_ken_insert_turn_record(t);
  }
}

#endif



#ifndef KEN_FLOW_CONTROL
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
#endif

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

  //char idbuf_from[KEN_ID_BUF_SIZE];
  //char idbuf_to[KEN_ID_BUF_SIZE];

  //ken_id_to_string(hdr.orig, idbuf_from, sizeof idbuf_from);
  //ken_id_to_string(recip, idbuf_to, sizeof idbuf_to);

#ifdef KEN_FLOW_CONTROL
  int pidx;
  for (pidx = 0; pidx < e_npeers; pidx++)
    if (0 == ken_id_cmp(recip, e_msgidx[pidx].id))
      break;

  /* create pidx if not exist */
  if (pidx == e_npeers) {
    KENASRT(KEN_MAX_NPEERS > e_npeers);
    e_msgidx[e_npeers].id = recip;
    //e_msgidx[e_npeers].window_size = KEN_START_WINDOW_SIZE;
    e_msgidx[e_npeers].min_duetime = 0;
    e_msgidx[e_npeers].last_acked_seqno = 0; // first seqno is 1.
    e_msgidx[e_npeers].last_sent_seqno = 0; // first seqno is 1.
    e_msgidx[e_npeers].ack = 0; // so to immediately ack.

    e_npeers++;

    //char idbuf[KEN_ID_BUF_SIZE];
    //ken_id_to_string(recip, idbuf, sizeof idbuf);
    //FP6("CREATE e_msgidx[%s].last_acked_seqno = %d, .last_sent_seqno = %d, .ack = %d, pidx = %d\n", idbuf, (int) e_msgidx[pidx].last_acked_seqno, (int) e_msgidx[pidx].last_sent_seqno, (int) e_msgidx[pidx].ack, pidx );

  } else {
    /* update rtt, if necessary */
  }

  /* update ack with the recent one */
  if( e_msgidx[pidx].ack < ack ) {
    e_msgidx[pidx].ack = ack;
  }

  // ack will always be sent immediately since we don't know
  // when to piggyback ack with other message.
  if( e_msgidx[pidx].last_acked_seqno == e_msgidx[pidx].last_sent_seqno ) {
    //FP4("SEND-IMMEDIATE-ACK ack %" PRId64 " from=%s to=%s\n", hdr.ack, idbuf_from, idbuf_to);
    i_ken_sendto(recip, &hdr, sizeof hdr);
  } else {
    //FP4("SEND-RE-ACK ack %" PRId64 " from=%s to=%s\n", hdr.ack, idbuf_from, idbuf_to);
    i_ken_sendto(recip, &hdr, sizeof hdr);
  }

#else // no KEN_FLOW_CONTROL
  //FP4("SEND-ACK ack %" PRId64 " from=%s to=%s\n", hdr.ack, idbuf_from, idbuf_to);

  i_ken_sendto(recip, &hdr, sizeof hdr);
#endif
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
#ifdef KEN_FLOW_CONTROL
      /* starttime and duetime for each message should be set up properly */
      mr->turn              = turn;
      mr->starttime         = ken_current_time();
      mr->duetime           = ken_current_time() + KEN_INITIAL_RETRANSMIT_INTERVAL;
#endif

      if (0 != ken_id_cmp(kenid_stdout, mr->recip)) {
        /* add to (peer,seqno)->msg mapping */
        s2m_t * s2mp  = (s2m_t *)malloc(sizeof *s2mp);
        for (pidx = 0; pidx < e_npeers; pidx++)
          if (0 == ken_id_cmp(dest.id, e_msgidx[pidx].id))
            break;
        if (pidx == e_npeers) {
          KENASRT(KEN_MAX_NPEERS > e_npeers);
          // NOTE: e_npeers will be increased at the end.
          e_msgidx[e_npeers].id = dest.id;
#ifdef KEN_FLOW_CONTROL
          /* initialize variables */
          //e_msgidx[e_npeers].window_size = KEN_START_WINDOW_SIZE;
          e_msgidx[e_npeers].min_duetime = mr->duetime;
          // if dest.reserved is not 0, it would have been recovered.
          if( mr->seqno > 1 ) {
            //char idbuf[KEN_ID_BUF_SIZE];
            //ken_id_to_string(e_msgidx[pidx].id, idbuf, sizeof idbuf);

            //FP5("RECOVERY e_msgidx[%s].last_acked_seqno = %d, last_sent_seqno = %d, ack = %d\n", idbuf, (int) mr->seqno-1, (int) mr->seqno-1, (int) dest.reserved);
          }

          e_msgidx[e_npeers].last_acked_seqno = mr->seqno-1; // first seqno is 1. this will be updated only we have received "ack".
          e_msgidx[e_npeers].last_sent_seqno = mr->seqno-1; // first seqno is 1.
          e_msgidx[e_npeers].ack = dest.reserved;
#endif
          e_npeers++;
        } else {
#ifdef KEN_FLOW_CONTROL
          /* update min duetime */
          if( NULL == e_msgidx[pidx].msglist ) {
            KENASRT(e_msgidx[pidx].min_duetime == 0);
            e_msgidx[pidx].min_duetime = mr->duetime;
          } else if ( mr->duetime < e_msgidx[pidx].min_duetime ) {
            KENASRT(NULL != e_msgidx[pidx].msglist);
            e_msgidx[pidx].min_duetime = mr->duetime;
          }
#endif
        }
        KENASRT(NULL == e_msgidx[pidx].msglist
                || dest.seqno > e_msgidx[pidx].msglist->sn);
        s2mp->sn   = dest.seqno;
        s2mp->mr   = mr;
#ifdef KEN_FLOW_CONTROL
        //char idbuf[KEN_ID_BUF_SIZE];
        //ken_id_to_string(e_msgidx[pidx].id, idbuf, sizeof idbuf);

        if( NULL == e_msgidx[pidx].msglist ) {
          s2mp->next = s2mp;
          s2mp->prev = s2mp;
          e_msgidx[pidx].msglist = s2mp;

          //FP6("ENQUEUE  seq %d, ack = %d, len = %d into queue(%s), queue.len = %d (NEW)\n", (int)dest.seqno, (int)dest.reserved, (int)mr->len, idbuf, (int)get_queue_len(pidx));
          //print_queue(pidx);

        } else {
          KENASRT(s2mp != e_msgidx[pidx].msglist);

          s2mp->next = e_msgidx[pidx].msglist;
          s2mp->prev = e_msgidx[pidx].msglist->prev;

          e_msgidx[pidx].msglist->prev->next = s2mp;
          e_msgidx[pidx].msglist->prev = s2mp;

          e_msgidx[pidx].msglist = s2mp;

          KENASRT(e_msgidx[pidx].msglist != e_msgidx[pidx].msglist->next);
          KENASRT(e_msgidx[pidx].msglist != e_msgidx[pidx].msglist->prev);

          //FP6("ENQUEUE  seq %d, ack = %d, len = %d into queue(%s), queue.len = %d (APPEND)\n", (int)dest.seqno, (int)dest.reserved, (int)mr->len, idbuf, (int)get_queue_len(pidx));
          //print_queue(pidx);
        }
#else
        s2mp->next = e_msgidx[pidx].msglist;
        e_msgidx[pidx].msglist = s2mp;
#endif
      }
      destptr += sizeof dest;
    }
    msgidx += sizeof outmsg_off;
  }
  i_ken_insert_turn_record(tr);
  MUNMAP((void *)start, size);
#ifdef KEN_FLOW_CONTROL
  i_ken_queue_messages(tr);
#else
  i_ken_send_messages(tr);
#endif
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
#ifdef KEN_FLOW_CONTROL
  /* For the ken_flow_control, it is possible that this can be inconsistent with turns record. */
  /* For the better control, we should modify turn record as well.. */

  for (pidx = 0; pidx < e_npeers; pidx++) {
    s2m_t *p, *q = NULL;
    s2m_t *header = e_msgidx[pidx].msglist;

    if( header == NULL ) continue;

    seqno_t start_sn = header->sn;

    for (p = e_msgidx[pidx].msglist; ; p = p->next) {
      KENASRT(0 == ken_id_cmp(p->mr->recip, e_msgidx[pidx].id));
      KENASRT(p->sn == p->mr->seqno);
      KENASRT(p->sn <= start_sn);
      nmsgs_b++;
      if( p->next->sn == start_sn ) {
        break;
      }
      if (NULL != q)
        KENASRT(q->sn > p->sn);  /* rows of s2m idx are descending on seqno */
      q = p;
    }
  }

#else
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
#endif
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

#ifdef KEN_FLOW_CONTROL
      /* Re-transmit unacked outbound messages that are due,
       * as long as incoming pipe from parent is empty.  Pipe from
       * parent takes priority over messages due for re-transmission
       * because pipe can contain ACKs that reduce need to send
       * messages, or EOT notices that require urgent services. */

      /* You actually have two separate messages queues - mr for each turn list
       * and send list. */

      do {
        int pidx = get_min_duetime_pidx();

        int64_t min_duetime = 0;
        if( pidx != -1 ) {
          min_duetime = e_msgidx[pidx].min_duetime;
        }

        struct timeval tv, *tvp = NULL;
        fd_set readfds;
        if (min_duetime > 0) {
          int64_t delay = min_duetime - ken_current_time();
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
        if (0 == r) { /* timeout */
          int64_t now = ken_current_time();
          //FP4("now = %" PRId64 " usec delay = %" PRId64 " usec min_duetime = %" PRId64 "\n", now, min_duetime-now, min_duetime);
          if (now >= min_duetime)
            i_ken_resend_messages(pidx);  // this will resend everything within the pidx.
          else
            FP2("early wake %" PRId64 " usec too soon\n", min_duetime - now);
        }
        else /* socket ready - probably broken socket */
          KENASRT(FD_ISSET(pipefd, &readfds));
      } while (0 == r);



#else /* UDP, NO-FLOW-CONTROL */

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
#endif

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
        s2m_t **ap; // NOTE: *del is not used and deletion will be processed in i_ken_delete_message.
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
          /* process ack */
          ap = &e_msgidx[pidx].msglist;
          ndel = i_ken_delete_message(pidx, eot_not.seqno, ap);

          if (0 == ndel) {  /* ack that deletes no msg recs must be a dup */
            char idbuf[KEN_ID_BUF_SIZE];
            ken_id_to_string(eot_not.id, idbuf, sizeof idbuf);
            FP3("duplicate ACK from %s seqno %" PRId64 "\n", idbuf, eot_not.seqno);
          } else {

#ifdef KEN_FLOW_CONTROL
            /* If something has been ack'ed, update duetime and last_ack list.
             * This has nothing to do with TCP since TCP does not hold window size. It just
             * sends everything. */

            if( e_msgidx[pidx].msglist != NULL ) {
              // eot_not.seqno may not always be equal to .last_acked_seqno.
              KENASRT(e_msgidx[pidx].last_acked_seqno >= eot_not.seqno);
              KENASRT(e_msgidx[pidx].msglist->prev != NULL );
              KENASRT(e_msgidx[pidx].msglist->prev->mr != NULL );
              e_msgidx[pidx].min_duetime = e_msgidx[pidx].msglist->prev->mr->duetime;
            } else {
              e_msgidx[pidx].min_duetime = 0;
            }

            i_ken_send_new_messages(pidx); /* if there is something to be sent, send it */
#endif
          }

        } /* end of process ack */
      } /* end of got ack */
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
