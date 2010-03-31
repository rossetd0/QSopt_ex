/****************************************************************************/
/*                                                                          */
/*  This file is part of QSopt_ex.                                          */
/*                                                                          */
/*  (c) Copyright 2006 by David Applegate, William Cook, Sanjeeb Dash,      */
/*  and Daniel Espinoza.  Sanjeeb Dash's ownership of copyright in          */
/*  QSopt_ex is derived from his copyright in QSopt.                        */
/*                                                                          */
/*  This code may be used under the terms of the GNU General Public License */
/*  (Version 2.1 or later) as published by the Free Software Foundation.    */
/*                                                                          */
/*  Alternatively, use is granted for research purposes only.               */ 
/*                                                                          */
/*  It is your choice of which of these two licenses you are operating      */
/*  under.                                                                  */
/*                                                                          */
/*  We make no guarantees about the correctness or usefulness of this code. */
/*                                                                          */
/****************************************************************************/

/* RCSINFO $Id: dheaps_i.c,v 1.2 2003/11/05 16:47:22 meven Exp $ */
/****************************************************************************/
/*                                                                          */
/*  This file is part of CONCORDE                                           */
/*                                                                          */
/*  (c) Copyright 1995--1999 by David Applegate, Robert Bixby,              */
/*  Vasek Chvatal, and William Cook                                         */
/*                                                                          */
/*  Permission is granted for academic research use.  For other uses,       */
/*  contact the authors for licensing options.                              */
/*                                                                          */
/*  Use at your own risk.  We make no guarantees about the                  */
/*  correctness or usefulness of this code.                                 */
/*                                                                          */
/****************************************************************************/

/****************************************************************************/
/*                                                                          */
/*                           DHEAP ROUTINES                                 */
/*                                                                          */
/*                                                                          */
/*                              TSP CODE                                    */
/*  Written by:  Applegate, Bixby, Chvatal, and Cook                        */
/*  Date: February 9, 1995                                                  */
/*        March 11, 2002 - Cook (Modifed for QS)                            */
/*  Reference: R.E. Tarjan, Data Structures and Network Algorithms          */
/*                                                                          */
/*    EXPORTED FUNCTIONS:                                                   */
/*                                                                          */
/*  int ILLutil_dheap_init (ILLdheap *h, int k)                             */
/*        -h should point to a ILLdheap struct.                             */
/*        -k the max number of elements in the dheap.                       */
/*                                                                          */
/*  void ILLutil_dheap_free (ILLdheap *h)                                   */
/*    -frees the spaces allocated by ILLutil_dheap_init                     */
/*                                                                          */
/*  int ILLutil_dheap_resize (ILLdheap *h, int newsize)                     */
/*    -REALLOCs h so it can contain newsize elements.                       */
/*    -returns -1 if it can't resize the heap.                              */
/*                                                                          */
/*  void ILLutil_dheap_findmin (ILLdheap *h, int *i)                        */
/*    -sets i to the index of the element with min value h->key[i]          */
/*    -sets i to -1 if no elements in heap.                                 */
/*                                                                          */
/*  int ILLutil_dheap_insert (ILLdheap *h, int i)                           */
/*    -inserts the element with index i (so its key should be loaded        */
/*     beforehand in h->key[i]).                                            */
/*                                                                          */
/*  void ILLutil_dheap_delete (ILLdheap *h, int i)                          */
/*    -deletes the element with index i.                                    */
/*                                                                          */
/*  void ILLutil_dheap_deletemin (ILLdheap *h, int *i)                      */
/*    -sets i to the min element in the heap, and deletes the min element   */
/*    -sets i to -1 if no elements in heap.                                 */
/*                                                                          */
/*  void ILLutil_dheap_changekey (ILLdheap *h, int i, EGlpNum_t* newkey)        */
/*    -changes the key of the element with index i to newkey.               */
/*                                                                          */
/****************************************************************************/

/****************************************************************************/
/*                                                                          */
/*  NOTES:                                                                  */
/*      A k-element heap will malloc 16k bytes of memory. If memory is      */
/*  tight, using integer keys (instead of doubles), brings it down to       */
/*  12k bytes, and if arbitrary deletions are not required, with a little   */
/*  rewriting, the h->loc field can be eliminated, bring the space down     */
/*  to 8k bytes.                                                            */
/*      These routines work with indices into the h->key array, so in       */
/*  some cases, you will need to maintain a separate names array to know    */
/*  what element belongs to index i. For an example, see the k_nearest      */
/*  code in kdnear.c.                                                       */
/*                                                                          */
/****************************************************************************/

#include "econfig.h"
#include "dheaps_i.h"
#include "allocrus.h"
#include "machdefs.h"
#include "except.h"
#include "trace.h"
#ifdef USEDMALLOC
#include "dmalloc.h"
#endif

static int TRACE = 0;

#define HEAP_D 3
#define HEAP_UP(x) (((x)-1)/HEAP_D)
#define HEAP_DOWN(x) (((x)*HEAP_D)+1)


static void dheap_siftup (ILLdheap * h,
													int i,
													int x),
  dheap_siftdown (ILLdheap * h,
									int i,
									int x);

static int dheap_minchild (int x,
													 ILLdheap * h);


int ILLutil_dheap_init (ILLdheap * h,
												int k)
{
	int rval = 0;

	h->entry = (int *) NULL;
	h->loc = (int *) NULL;
	h->key = 0;


	ILL_SAFE_MALLOC (h->entry, k, int);
	ILL_SAFE_MALLOC (h->loc, k, int);
	h->key = EGlpNumAllocArray (k);
	h->size = 0;
	h->total_space = k;

CLEANUP:

	if (rval)
	{
		ILLutil_dheap_free (h);
	}

	ILL_RETURN (rval, "ILLutil_dheap_init");
}

void ILLutil_dheap_free (ILLdheap * h)
{
	ILL_IFFREE (h->entry, int);
	ILL_IFFREE (h->loc, int);
	EGlpNumFreeArray (h->key);
}

int ILLutil_dheap_resize (ILLdheap * h,
													int newsize)
{
	int rval = 0;

	if (newsize < h->size || newsize < h->total_space)
	{
		ILL_CLEANUP;
	}

	h->key = EGrealloc(h->key, sizeof(double)*newsize);
	//rval = ILLutil_reallocrus_count ((void **) &(h->key), newsize,
	//																 sizeof (double));
	//ILL_CLEANUP_IF (rval);
	h->entry = EGrealloc(h->entry, sizeof(int)*newsize);
	//rval = ILLutil_reallocrus_count ((void **) &(h->entry), newsize,
	//																 sizeof (int));
	//ILL_CLEANUP_IF (rval);
	h->loc = EGrealloc(h->loc,sizeof(int)*newsize);
	//rval = ILLutil_reallocrus_count ((void **) &(h->loc), newsize, sizeof (int));
	//ILL_CLEANUP_IF (rval);
	h->total_space = newsize;

CLEANUP:

	ILL_RETURN (rval, "ILLutil_dheap_resize");
}

void ILLutil_dheap_findmin (ILLdheap * h,
														int *i)
{
	if (h->size == 0)
		*i = -1;
	else
		*i = h->entry[0];
}

int ILLutil_dheap_insert (ILLdheap * h,
													int i)
{
	if (h->size >= h->total_space)
	{
		fprintf (stderr, "Error - heap already full\n");
		return 1;
	}
	h->size++;
	dheap_siftup (h, i, h->size - 1);

	return 0;
}

void ILLutil_dheap_delete (ILLdheap * h,
													 int i)
{
	int j;

	h->size--;
	j = h->entry[h->size];
	h->entry[h->size] = -1;

	if (j != i)
	{
		if (EGlpNumIsLeq (h->key[j], h->key[i]))
		{
			dheap_siftup (h, j, h->loc[i]);
		}
		else
		{
			dheap_siftdown (h, j, h->loc[i]);
		}
	}
}

void ILLutil_dheap_deletemin (ILLdheap * h,
															int *i)
{
	int j;

	if (h->size == 0)
		*i = -1;
	else
	{
		j = h->entry[0];
		ILLutil_dheap_delete (h, j);
		*i = j;
	}
}

void ILLutil_dheap_changekey (ILLdheap * h,
															int i,
															EGlpNum_t * newkey)
{
	if (EGlpNumIsLess (*newkey, h->key[i]))
	{
		EGlpNumCopy (h->key[i], *newkey);
		dheap_siftup (h, i, h->loc[i]);
	}
	else if (EGlpNumIsLess (h->key[i], *newkey))
	{
		EGlpNumCopy (h->key[i], *newkey);
		dheap_siftdown (h, i, h->loc[i]);
	}
}

static void dheap_siftup (ILLdheap * h,
													int i,
													int x)
{
	int p;

	p = HEAP_UP (x);
	while (x && EGlpNumIsLess (h->key[i], h->key[h->entry[p]]))
	{
		h->entry[x] = h->entry[p];
		h->loc[h->entry[p]] = x;
		x = p;
		p = HEAP_UP (p);
	}
	h->entry[x] = i;
	h->loc[i] = x;
}

static void dheap_siftdown (ILLdheap * h,
														int i,
														int x)
{
	int c;

	c = dheap_minchild (x, h);

	while (c >= 0 && EGlpNumIsLess (h->key[h->entry[c]], h->key[i]))
	{
		h->entry[x] = h->entry[c];
		h->loc[h->entry[c]] = x;
		x = c;
		c = dheap_minchild (c, h);
	}
	h->entry[x] = i;
	h->loc[i] = x;
}

static int dheap_minchild (int x,
													 ILLdheap * h)
{
	int c = HEAP_DOWN (x);
	int cend;
	EGlpNum_t minval;
	int minloc;

	if (c >= h->size)
		return -1;

	EGlpNumInitVar (minval);
	EGlpNumCopy (minval, h->key[h->entry[c]]);
	minloc = c;
	cend = c + HEAP_D;
	if (h->size < cend)
		cend = h->size;
	for (c++; c < cend; c++)
	{
		if (EGlpNumIsLess (h->key[h->entry[c]], minval))
		{
			EGlpNumCopy (minval, h->key[h->entry[c]]);
			minloc = c;
		}
	}
	EGlpNumClearVar (minval);
	return minloc;
}
