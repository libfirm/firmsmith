/*
 * Copyright (C) 1995-2008 University of Karlsruhe.  All right reserved.
 *
 * This file is part of libFirm.
 *
 * This file may be distributed and/or modified under the terms of the
 * GNU General Public License version 2 as published by the Free Software
 * Foundation and appearing in the file LICENSE.GPL included in the
 * packaging of this file.
 *
 * Licensees holding valid libFirm Professional Edition licenses may use
 * this file in accordance with the libFirm Commercial License.
 * Agreement provided with the Software.
 *
 * This file is provided AS IS with NO WARRANTY OF ANY KIND, INCLUDING THE
 * WARRANTY OF DESIGN, MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE.
 */

/**
 * @file
 * @brief       double ended queue of generic pointers.
 * @author      Christian von Roques
 * @date        1999 by getting from fiasco
 * @version     $Id: pdeq.c 24125 2008-11-28 16:00:39Z mallon $
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "fourcc.h"
#include "pdeq.h"
#include "xmalloc.h"
#include "align.h"

/* Pointer Double Ended Queue */
#define PDEQ_MAGIC1	FOURCC('P','D','E','1')
#define PDEQ_MAGIC2	FOURCC('P','D','E','2')

/** Size of pdeq block cache. */
#define TUNE_NSAVED_PDEQS 16

/**
 * Maximal number of data items in a pdeq chunk.
 */
#define NDATA ((int)((PREF_MALLOC_SIZE - offsetof (pdeq, data)) / sizeof (void *)))

#ifdef NDEBUG
# define VRFY(dq) ((void)0)
#else
# define VRFY(dq) assert((dq) && ((dq)->magic == PDEQ_MAGIC1))
#endif

/**
 * A pointer double ended queue.
 * This structure is used as a list chunk either.
 */
struct pdeq {
#ifndef NDEBUG
  unsigned magic;       /**< debug magic */
#endif
  pdeq *l_end, *r_end;  /**< left and right ends of the queue */
  pdeq *l, *r;          /**< left and right neighbor */
  int n;                /**< number of elements in the current chunk */
  int p;                /**< the read/write pointer */
  const void *data[1];  /**< storage for elements */
};


/**
 * cache of unused, pdeq blocks to speed up new_pdeq and del_pdeq.
 * +1 for compilers that can't grok empty arrays
 */
static pdeq *pdeq_block_cache[TUNE_NSAVED_PDEQS+1];

/**
 * Number of pdeqs in pdeq_store.
 */
static unsigned pdeqs_cached;

/**
 * Free a pdeq chunk, put in into the cache if possible.
 *
 * @param p   The pdeq chunk.
 */
static inline void free_pdeq_block (pdeq *p)
{
#ifndef NDEBUG
  p->magic = 0xbadf00d1;
#endif
  if (pdeqs_cached < TUNE_NSAVED_PDEQS) {
    pdeq_block_cache[pdeqs_cached++] = p;
  } else {
    xfree (p);
  }
}

/**
 * Allocate a new pdeq chunk, get it from the cache if possible.
 *
 * @return A new pdeq chunk.
 */
static inline pdeq *alloc_pdeq_block (void)
{
  pdeq *p;
  if (TUNE_NSAVED_PDEQS && pdeqs_cached) {
    p = pdeq_block_cache[--pdeqs_cached];
  } else {
    p = xmalloc(PREF_MALLOC_SIZE);
  }
  return p;
}


#ifndef NDEBUG
/**
 * Verify a double ended list, assert if failure.
 *
 * @param dq   The list to verify.
 */
void _pdeq_vrfy(pdeq *dq)
{
  pdeq *q;

  assert (   dq
	  && (dq->magic == PDEQ_MAGIC1)
	  && (dq->l_end && dq->r_end));
  q = dq->l_end;
  while (q) {
    assert (   ((q == dq) || (q->magic == PDEQ_MAGIC2))
	    && ((q == dq->l_end) ^ (q->l != NULL))
	    && ((q == dq->r_end) ^ (q->r != NULL))
	    && (!q->l || (q == q->l->r))
	    && ((q->n >= 0) && (q->n <= NDATA))
	    && ((q == dq->l_end) || (q == dq->r_end) || (q->n == NDATA))
	    && ((q->p >= 0) && (q->p < NDATA)));
    q = q->r;
  }
}
#endif

/* Creates a new double ended pointer list. */
pdeq *new_pdeq(void)
{
  pdeq *dq;

  dq = alloc_pdeq_block();

#ifndef NDEBUG
  dq->magic = PDEQ_MAGIC1;
#endif
  dq->l_end = dq->r_end = dq;
  dq->l = dq->r = NULL;
  dq->n = dq->p = 0;

  VRFY(dq);
  return dq;
}

/* Creates a new double ended pointer list and puts an initial pointer element in. */
pdeq *new_pdeq1(const void *x)
{
  return pdeq_putr(new_pdeq(), x);
}

/* Delete a double ended pointer list. */
void del_pdeq(pdeq *dq)
{
  pdeq *q, *qq;

  VRFY(dq);

  q = dq->l_end;		/* left end of chain */
  /* pdeq trunk empty, but !pdeq_empty() ==> trunk not in chain */
  if (dq->n == 0 && dq->l_end != dq ) {
    free_pdeq_block(dq);
  }

  /* Free all blocks in the pdeq chain */
  do {
    qq = q->r;
    free_pdeq_block(q);
  } while ((q = qq));

}

/* Checks if a list is empty. */
int pdeq_empty(pdeq *dq)
{
  VRFY(dq);
  return dq->l_end->n == 0;
}

/* Returns the length of a double ended pointer list. */
int pdeq_len(pdeq *dq)
{
  int n;
  pdeq *q;

  VRFY(dq);

  n = 0;
  q = dq->l_end;
  do {
    n += q->n;
    q = q->r;
  }  while (q);

  return n;
}

/* Add a pointer to the right site of a double ended pointer list. */
pdeq *pdeq_putr(pdeq *dq, const void *x)
{
  pdeq *rdq;
  int n;

  VRFY(dq);

  rdq = dq->r_end;
  if (rdq->n >= NDATA) {	/* tailblock full */
    pdeq *ndq;

    ndq = dq;			/* try to reuse trunk, but ... */
    if (dq->n) {		/* ... if trunk used */
      /* allocate and init new block */
      ndq = alloc_pdeq_block();
#ifndef NDEBUG
      ndq->magic = PDEQ_MAGIC2;
#endif
      ndq->l_end = ndq->r_end = NULL;
    }

    ndq->r = NULL;
    ndq->l = rdq; rdq->r = ndq;
    ndq->n = 0; ndq->p = 0;
    dq->r_end = ndq;
    rdq = ndq;
  }

  n = rdq->n++ + rdq->p;
  if (n >= NDATA) n -= NDATA;

  rdq->data[n] = x;

  VRFY(dq);
  return dq;
}

/* Add a pointer to the left site of a double ended pointer list. */
pdeq *pdeq_putl(pdeq *dq, const void *x)
{
  pdeq *ldq;
  int p;

  VRFY(dq);

  ldq = dq->l_end;
  if (ldq->n >= NDATA) {	/* headblock full */
    pdeq *ndq;

    ndq = dq;			/* try to reuse trunk, but ... */
    if (dq->n) {		/* ... if trunk used */
      /* allocate and init new block */
      ndq = alloc_pdeq_block();
#ifndef NDEBUG
      ndq->magic = PDEQ_MAGIC2;
#endif
      ndq->l_end = ndq->r_end = NULL;
    }

    ndq->l = NULL;
    ndq->r = ldq; ldq->l = ndq;
    ndq->n = 0; ndq->p = 0;
    dq->l_end = ndq;
    ldq = ndq;
  }

  ldq->n++;
  p = ldq->p - 1;
  if (p < 0) p += NDATA;
  ldq->p = p;

  ldq->data[p] = x;

  VRFY(dq);
  return dq;
}

/* Retrieve a pointer from the right site of a double ended pointer list. */
void *pdeq_getr(pdeq *dq)
{
  pdeq *rdq;
  const void *x;
  int n;

  VRFY(dq);
  assert(dq->l_end->n);

  rdq = dq->r_end;
  n = rdq->p + --rdq->n;
  if (n >= NDATA) n -= NDATA;
  x = rdq->data[n];

  if (rdq->n == 0) {
    if (rdq->l) {
      dq->r_end = rdq->l;
      rdq->l->r = NULL;
      rdq->l = NULL;
    } else {
      dq->r_end = dq->l_end = dq;
    }
    if (dq != rdq) {
      free_pdeq_block(rdq);
    }
  }

  VRFY(dq);
  return (void *)x;
}

/* Retrieve a pointer from the left site of a double ended pointer list. */
void *pdeq_getl(pdeq *dq)
{
  pdeq *ldq;
  const void *x;
  int p;

  VRFY(dq);
  assert(dq->l_end->n);

  ldq = dq->l_end;
  p = ldq->p;
  x = ldq->data[p];
  if (++p >= NDATA) p = 0;
  ldq->p = p;

  if (--ldq->n == 0) {
    if (ldq->r) {
      dq->l_end = ldq->r;
      ldq->r->l = NULL;
      ldq->r = NULL;
    } else {
      dq->l_end = dq->r_end = dq;
    }
    if (dq != ldq) {
      free_pdeq_block(ldq);
    }
  }

  VRFY(dq);
  return (void *)x;
}

/*
 * Returns non-zero if a double ended pointer list
 * contains a pointer x.
 */
int pdeq_contains(pdeq *dq, const void *x)
{
  pdeq *q;

  VRFY(dq);

  q = dq->l_end;
  do {
    int p, ep;

    p = q->p; ep = p + q->n;

    if (ep > NDATA) {
      do {
	if (q->data[p] == x) return 1;
      } while (++p < NDATA);
      p = 0;
      ep -= NDATA;
    }

    while (p < ep) {
      if (q->data[p++] == x) return 1;
    }

    q = q->r;
  } while (q);

  return 0;
}

/*
 * Search a key in a double ended pointer list, the search
 * is controlled by a compare function.
 * An element is found, if the compare function returns 0.
 * The search is started from the left site of the list.
 */
void *pdeq_search(pdeq *dq, cmp_fun cmp, const void *key)
{
  pdeq *q;
  int p;

  VRFY(dq);

  q = dq->l_end;
  do {
    int ep;

    p = q->p; ep = p + q->n;

    if (ep > NDATA) {
      do {
	if (!cmp (q->data[p], key)) return (void *)q->data[p-1];
      } while (++p < NDATA);
      p = 0;
      ep -= NDATA;
    }

    while (p < ep) {
      if (!cmp (q->data[p++], key)) return (void *)q->data[p-1];
    }

    q = q->r;
  } while (q);

  return NULL;
}

/*
 * Convert the double ended pointer list into a linear array beginning from
 * left, the first element in the linear array will be the left one.
 */
void **pdeq_copyl(pdeq *dq, const void **dst)
{
  pdeq *q;
  const void **d = dst;

  VRFY(dq);

  q = dq->l_end;
  while (q) {
    int p, n;

    p = q->p; n = q->n;

    if (n + p > NDATA) {
      int nn = NDATA - p;
      memcpy((void *) d, &q->data[p], nn * sizeof(void *)); d += nn;
      p = 0; n -= nn;
    }

    memcpy((void *) d, &q->data[p], n * sizeof(void *)); d += n;

    q = q->r;
  }

  return (void **)dst;
}

/*
 * Convert the double ended pointer list into a linear array beginning from
 * right, the first element in the linear array will be the right one.
 */
void **pdeq_copyr(pdeq *dq, const void **dst)
{
  pdeq *q;
  const void **d = dst;

  VRFY(dq);

  q = dq->r_end;
  while (q) {
    int p, i;

    p = q->p; i = q->n + p - 1;
    if (i >= NDATA) {
      i -= NDATA;
      do *d++ = q->data[i]; while (--i >= 0);
      i = NDATA - 1;
    }

    do *d++ = q->data[i]; while (--i >= p);

    q = q->l;
  }

  return (void **)dst;
}
