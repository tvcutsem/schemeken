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

#include <string.h>
#include "kencom.h"
#include "kencrc.h"
#include "kenvar.h"

struct kenvar {
  char * name;
  void * val;
  struct kenvar * next;
};

enum { BUCKETS = 503 };  /* prime number; see Hanson _CII_ p. 126 */

static uint32_t i_ken_hash(const char * name) {
  return i_ken_cksum((const unsigned char *)name, strlen(name)) % BUCKETS;
}

static struct kenvar * i_ken_find(const char * name, uint32_t h) {
  struct kenvar *p, **T = (struct kenvar **)ken_get_app_data();
  if (NULL == T)
    return NULL;  /* APPERR instead (?) */
  for (p = T[h]; NULL != p; p = p->next)
    if (0 == strcmp(name, p->name))
      break;
  return p;
}

/* maybe add a check that val points into Ken persistent heap (?) */
void kenvar_set(const char * name, void * val) {
  struct kenvar *p, **T;
  uint32_t h;
  APPERR(NULL != name && NULL != val);
  T = (struct kenvar **)ken_get_app_data();
  if (NULL == T) {
    T = (struct kenvar **)ken_malloc(BUCKETS * sizeof *T);
    NTF(NULL != T);
    for (h = 0; BUCKETS > h; h++)  /* ken_malloc() doesn't clear */
      T[h] = NULL;                 /* ... so we must initialize */
    ken_set_app_data(T);
  }
  h = i_ken_hash(name);
  p = i_ken_find(name, h);
  if (NULL == p) {
    p = (struct kenvar *)ken_malloc(sizeof *p);
    NTF(NULL != p);
    p->name = (char *)ken_malloc(1 + strlen(name));
    strcpy(p->name, name);
    p->next = T[h];
    T[h] = p;
  }
  p->val = val;
}

void * kenvar_get(const char * name) {
  struct kenvar *p;
  APPERR(NULL != name);
  p = i_ken_find(name, i_ken_hash(name));
  return (NULL == p ? NULL : p->val);
}

#ifdef KENVAR_UNIT_TEST

#include "kenapp.h"

enum { N = 100000 };  /* If you're using this many global variables,
                         you might reconsider your design.  :) */

int64_t ken_handler(void * msg, int32_t len, kenid_t sender) {
  int i, r, tot, max, min, v[N], w[N], *V;
  char k[23];
  struct kenvar *p, **T;
  assert(NULL == msg && 0 == len);
  assert(   0 == ken_id_cmp(sender, kenid_NULL)
         || 0 == ken_id_cmp(sender, kenid_alarm));
  for (i = 0; N > i; i++)
    v[i] = w[i] = i;
  tot = 0;
  max = -1;
  min = 10 * N;
  for (i = 0; N > i; i += 2) {
    r = snprintf(k, sizeof k, "k%06d", i);  assert((int)sizeof k > r);
    /* Do not imitate next line, which stores a pointer to stack-
       allocated data!  This is fine for the present testing
       purposes but in general it's a Very Bad Idea. */
    kenvar_set(k, &v[i]);
  }
  for (i = 0; N > i; i++) {
    r = snprintf(k, sizeof k, "k%06d", i);  assert((int)sizeof k > r);
    V = (int *)kenvar_get(k);
    if (0 == i % 2) {
      assert(NULL != V);
      assert(i == *V && i == V - &(v[0]));
    }
    else {
      assert(NULL == V);
    }
  }
  T = (struct kenvar **)ken_get_app_data();
  for (i = 0; i < BUCKETS; i++) {
    int num = 0;
    for (p = T[i]; NULL != p; p = p->next)
      num++;
    tot += num;
    if (num < min) min = num;
    if (num > max) max = num;
  }
  FP4("A kenvar unit test succeeded %d %d %d\n", tot, min, max);
  /* Do it all again to test the case where we put a different value
     for keys already present. */
  /* begin cut-n-paste; maybe eliminate (?) */
  tot = 0;
  max = -1;
  min = 10 * N;
  for (i = 0; N > i; i += 2) {
    r = snprintf(k, sizeof k, "k%06d", i);  assert((int)sizeof k > r);
    kenvar_set(k, &w[i]);
  }
  for (i = 0; N > i; i++) {
    r = snprintf(k, sizeof k, "k%06d", i);  assert((int)sizeof k > r);
    V = (int *)kenvar_get(k);
    if (0 == i % 2) {
      assert(NULL != V);
      assert(i == *V && i == V - &(w[0]));
    }
    else {
      assert(NULL == V);
    }
  }
  T = (struct kenvar **)ken_get_app_data();
  for (i = 0; BUCKETS > i; i++) {
    int num = 0;
    for (p = T[i]; NULL != p; p = p->next)
      num++;
    tot += num;
    if (num < min) min = num;
    if (num > max) max = num;
  }
  FP4("B kenvar unit test succeeded %d %d %d\n", tot, min, max);
  /* end cut-n-paste */
  return ken_current_time() + 1000 * 1000;
}

#endif

