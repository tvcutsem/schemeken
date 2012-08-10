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

#ifndef KENAPP_H_INCLUDED
#define KENAPP_H_INCLUDED

/* Starting point of application-level software, analogue of the
   main() function of conventional C programs.  A "turn" is an
   invocation of the handler function.  The handler function is
   called once with a NULL msg at the birth of a Ken process.  The
   sender is: kenid_NULL for initialization, kenid_stdin for inputs
   from stdin, kenid_alarm for alarms, and Ken ID of sender for
   messages from other Ken processes.  Return value is Unix epoch
   time in microseconds at which alarm should fire and handler should
   be re-invoked.  Note that Ken alarms may fire a bit prematurely
   due to imprecision in the underlying system calls that implement
   alarms, so applications must be aware of this possibility.  Return
   value of zero means re-invoke handler immediately without even
   checking for next input/message.  Return value of -1 means block
   until next input/message arrives.  If return value is time in the
   past, then an input or message is returned immediately if one is
   available, but in any case the handler is re-invoked
   immediately. */
extern int64_t ken_handler(void * msg, int32_t len, kenid_t sender);

#endif

