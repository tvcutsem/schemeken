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

#ifndef KENEXT_H_INCLUDED
#define KENEXT_H_INCLUDED

/* Maybe add checksum to message header (?).  Standard IP checksum is
   supposedly rather weak and data corruption in transit can occur. */
typedef struct {
  kenid_t orig,   /* originator: Ken process that created message */
          cons;   /* consumer: Ken process that will process msg */
  seqno_t seqno,  /* ensures pairwise-FIFO delivery; first msg is 1 */
          ack;    /* seqno of last msg from *consumer* processed
                     by *originator* */
} __attribute__((packed)) ken_msg_hdr_t;

typedef struct {
  turn_t turn;
  kenid_t id;
  seqno_t seqno;
} ken_eot_notice_t;

/* main job:  sends messages in end-of-turn files on their way */
extern void i_ken_externalizer(int pipefd, int writepipefd, int commsock);

#endif

