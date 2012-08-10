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

/* Patcher function for patcher process:  Pastes dirty pages from EOT
   file into state file. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>  /* for off_t; maybe put in header that needs it (?) */

#include "kencom.h"
#include "kenpat.h"

void i_ken_patcher(int patchpipe) {
  turn_t highest_turn = -1;
  int go_msg;
  ssize_t nbytes;

  /* don't do anything until parent gives go-ahead message */
  nbytes = read(patchpipe, &go_msg, sizeof go_msg);
  NTF(sizeof go_msg == nbytes);
  KENASRT(0xABCDEF == go_msg);

  FP1("patcher starting\n");

  for (;;) {
    turn_t turn;
    nbytes = read(patchpipe, &turn, sizeof turn);
    if (0 > nbytes) {
      RESET_ERRNO;
      NTF(0);
    }
    if (0 == nbytes) {
      FP1("read pipe returns zero, exit from patcher\n");
      exit(EXIT_FAILURE);  /* find a more informative exit code (?) */
    }
    KENASRT(sizeof turn == nbytes);
    KENASRT(0 <= turn);
    if (-1 == highest_turn || 1 + highest_turn == turn) {  /* patch this turn */
      highest_turn = turn;
      i_ken_patch_eot_file(turn);
    }
    else { /* turn has been ack'd; delete EOT */
      KENASRT(turn <= highest_turn);
      if (i_ken_eot_file_exists(turn, "")) {
        KENASRT(i_ken_eot_file_exists(turn, SFX_ACK));
        KENASRT(i_ken_eot_file_exists(turn, SFX_PAT));
        i_ken_delete_eot_file(turn);
      }
      else {
        KENASRT(! i_ken_eot_file_exists(turn, SFX_PAT));
        KENASRT(! i_ken_eot_file_exists(turn, SFX_ACK));
      }
    }
  }

  /* NOTREACHED */
  KENASRT(0);
}

