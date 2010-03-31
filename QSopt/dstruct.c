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

/* RCS_INFO = "$RCSfile: dstruct.c,v $ $Revision: 1.2 $ $Date: 2003/11/05 16:49:52 $"; */
static int TRACE = 0;

#include "config.h"
#include "iqsutil.h"
#include "dstruct.h"
#include "qsopt.h"
#include "lpdefs.h"
#ifdef USEDMALLOC
#include "dmalloc.h"
#endif

/****************************************************************************/
/*                                                                          */
/*                            svector                                       */
/*                                                                          */
/*  Written by:  Applegate, Cook, Dash                                      */
/*  Date:                                                                   */
/*                                                                          */
/*    EXPORTED FUNCTIONS:                                                   */
/*                                                                          */
/****************************************************************************/

void ILLsvector_init (svector * s)
{
	s->nzcnt = 0;
	s->indx = 0;
	s->coef = 0;
}

void ILLsvector_free (svector * s)
{
	ILL_IFFREE (s->indx, int);
	EGlpNumFreeArray (s->coef);
	s->nzcnt = 0;
}

int ILLsvector_alloc (svector * s,
											int nzcnt)
{
	int rval = 0;

	s->nzcnt = nzcnt;
	if (nzcnt == 0)
	{
		s->indx = 0;
		s->coef = 0;
	}
	else
	{
		ILL_SAFE_MALLOC (s->indx, nzcnt, int);
		s->coef = EGlpNumAllocArray (nzcnt);
	}
	return 0;
CLEANUP:
	ILL_IFFREE (s->indx, int);
	EGlpNumFreeArray (s->coef);
	ILL_RETURN (rval, "ILLsvector_alloc");
}

int ILLsvector_copy (const svector * s_in,
										 svector * s_out)
{
	int i;
	int nzcnt = s_in->nzcnt;
	int rval = 0;

	rval = ILLsvector_alloc (s_out, nzcnt);
	ILL_CLEANUP_IF (rval);
	for (i = 0; i < nzcnt; i++)
	{
		s_out->indx[i] = s_in->indx[i];
		EGlpNumCopy (s_out->coef[i], s_in->coef[i]);
	}

CLEANUP:
	ILL_RETURN (rval, "ILLsvector_copy");
}

/****************************************************************************/
/*                                                                          */
/*                            heap                                          */
/*                                                                          */
/*  Written by:  Applegate, Cook, Dash                                      */
/*  Date:                                                                   */
/*                                                                          */
/*    EXPORTED FUNCTIONS:                                                   */
/*                                                                          */
/****************************************************************************/

#define DEBUG_HEAP 0

#define HEAP_D 3
#define HEAP_UP(x) (((x)-1)/HEAP_D)
#define HEAP_DOWN(x) (((x)*HEAP_D)+1)

static int siftup (heap * h,
									 int hloc,
									 int ix),
  siftdown (heap * h,
						int hloc,
						int ix),
  maxchild (heap * h,
						int hloc);

static int siftup (heap * h,
									 int hloc,
									 int ix)
{
	int i = hloc;
	int p = HEAP_UP (i);
	EGlpNum_t val;
	EGlpNumInitVar (val);
	EGlpNumCopy (val, h->key[ix]);

	while (i > 0 && EGlpNumIsLess (h->key[h->entry[p]], val))
	{
		h->entry[i] = h->entry[p];
		h->loc[h->entry[i]] = i;
		i = p;
		p = HEAP_UP (p);
	}
	h->entry[i] = ix;
	h->loc[ix] = i;
	ILL_IFTRACE2 ("%s:%la:%d:%d:%d\n", __func__, EGlpNumToLf (val), hloc, ix, i);
	EGlpNumClearVar (val);
	return i;
}

static int siftdown (heap * h,
										 int hloc,
										 int ix)
{
	int i = hloc;
	int c = maxchild (h, i);
	EGlpNum_t val;
	EGlpNumInitVar (val);
	EGlpNumCopy (val, h->key[ix]);
	ILL_IFTRACE2 ("%s:%d:%d:%d:%la", __func__, hloc, ix, c, EGlpNumToLf (val));

	while (c != -1 && EGlpNumIsLess (val, h->key[h->entry[c]]))
	{
		h->entry[i] = h->entry[c];
		h->loc[h->entry[i]] = i;
		i = c;
		c = maxchild (h, c);
	}
	h->entry[i] = ix;
	h->loc[ix] = i;
	EGlpNumClearVar (val);
	ILL_IFTRACE2 ("%s:%d:%d\n", __func__, ix, i);
	return i;
}

//extern EGlpNum_t ILL_MINDOUBLE;
static int maxchild (heap * h,
										 int hloc)
{
	int i;
	int mc = -1;
	int hmin = HEAP_D * hloc + 1;
	int hmax = HEAP_D * hloc + HEAP_D;
	EGlpNum_t val;
	EGlpNumInitVar (val);
	EGlpNumCopy (val, ILL_MINDOUBLE);
	ILL_IFTRACE2 (" %s:%d", __func__, hloc);

	for (i = hmin; i <= hmax && i < h->size; i++)
		if (EGlpNumIsLess (val, h->key[h->entry[i]]))
		{
			EGlpNumCopy (val, h->key[h->entry[i]]);
			mc = i;
			ILL_IFTRACE2 (":%d:%la", mc, EGlpNumToLf (val));
		}
	EGlpNumClearVar (val);
	ILL_IFTRACE2 ("\n");
	return mc;
}

#if DEBUG_HEAP > 0

static void printheap (heap * h)
{
	int i;

	printf ("entry (%d): ", h->size);
	for (i = 0; i < h->size; i++)
		printf ("%d ", h->entry[i]);
	printf ("\n loc: ");
	for (i = 0; i < h->maxsize; i++)
		printf ("%d ", h->loc[i]);
	printf ("\n key: ");
	for (i = 0; i < h->maxsize; i++)
		printf ("%la ", EGlpNumToLf (h->key[i]));
	printf ("\n key(sorted): ");
	for (i = 0; i < h->size; i++)
		printf ("%la ", EGlpNumToLf (h->key[h->entry[i]]));
	printf ("\n");
}

static void heapcheck (heap * h)
{
	int i,
	  tcnt = 0;

	for (i = 0; i < h->maxsize; i++)
	{
		if (h->loc[i] < -1)
			printf ("error in heap\n");
		else if (h->loc[i] > -1)
			tcnt++;
	}
	if (tcnt != h->size)
		printf ("error 3 in heap\n");

	for (i = 0; i < h->size; i++)
	{
		if (h->loc[h->entry[i]] != i)
			printf ("error 1 in heap\n");
		if (EGlpNumIsEqqual (h->key[h->entry[i]], zeroLpNum))
			printf ("error 2 in heap\n");
		if (EGlpNumIsLess (h->key[h->entry[HEAP_UP (i)]], h->key[h->entry[i]]))
			printf ("error 4 in heap\n");
	}
}

#endif

void ILLheap_insert (heap * const h,
										 int const ix)
{
	int i = h->size;
	ILL_IFTRACE ("%s:%d:%la\n", __func__, ix, EGlpNumToLf (h->key[ix]));

	i = siftup (h, i, ix);
	h->size++;

#if DEBUG_HEAP > 0
	heapcheck (h);
#endif
#if DEBUG_HEAP > 1
	printheap (h);
#endif
}

void ILLheap_modify (heap * const h,
										 int const ix)
{
	int i = h->loc[ix];
	int pi = i;
	ILL_IFTRACE ("%s:%d\n", __func__, ix);

	if (h->loc[ix] == -1)
		return;
	i = siftup (h, i, ix);
	if (pi == i)
		i = siftdown (h, i, ix);

#if DEBUG_HEAP > 0
	heapcheck (h);
#endif
#if DEBUG_HEAP > 1
	printheap (h);
#endif
}

void ILLheap_delete (heap * const h,
										 int const ix)
{
	int i = h->loc[ix];
	int pi = i;
	int nix = h->entry[h->size - 1];
	ILL_IFTRACE ("%s:%d:%d:%d\n", __func__, ix, nix, pi);

	h->loc[ix] = -1;
	h->size--;
	if (nix == ix)
	{
#if DEBUG_HEAP > 0
		heapcheck (h);
#endif
#if DEBUG_HEAP > 1
		printheap (h);
#endif
		return;
	}

	h->entry[i] = nix;
	h->loc[nix] = i;

	i = siftup (h, i, nix);
	ILL_IFTRACE ("%s:%d:%d:%d:%d\n", __func__, ix, nix, pi, i);
	if (pi == i)
		siftdown (h, i, nix);

#if DEBUG_HEAP > 0
	heapcheck (h);
#endif
#if DEBUG_HEAP > 1
	printheap (h);
#endif
}

int ILLheap_findmin (heap * const h)
{
	if (h->hexist == 0 || h->size <= 0)
		return -1;
	return h->entry[0];
}

void ILLheap_init (heap * const h)
{
	h->entry = NULL;
	h->loc = NULL;
	h->key = NULL;
	h->hexist = 0;
}

int ILLheap_build (heap * const h,
									 int const nelems,
									 EGlpNum_t * key)
{
	int rval = 0;
	int i,
	  n = 0;
	ILL_IFTRACE ("%s:%d\n", __func__, nelems);

	h->hexist = 1;
	h->size = 0;
	h->maxsize = nelems;
	h->key = key;
	ILL_SAFE_MALLOC (h->entry, nelems, int);
	ILL_SAFE_MALLOC (h->loc, nelems, int);

	for (i = 0; i < nelems; i++)
	{
		if (EGlpNumIsLess (zeroLpNum, key[i]))
		{
			h->entry[n] = i;
			h->loc[i] = n;
			n++;
		}
		else
			h->loc[i] = -1;
	}
	h->size = n;
	for (i = n - 1; i >= 0; i--)
	{
		ILL_IFTRACE2 ("insert %la\n", EGlpNumToLf (h->key[h->entry[i]]));
		siftdown (h, i, h->entry[i]);
	}

#if DEBUG_HEAP > 0
	heapcheck (h);
#endif
#if DEBUG_HEAP > 1
	printheap (h);
#endif

CLEANUP:
	if (rval)
		ILLheap_free (h);
	ILL_RETURN (rval, "ILLheap_init");
}

void ILLheap_free (heap * const h)
{
	if (h->hexist)
	{
		ILL_IFFREE (h->entry, int);
		ILL_IFFREE (h->loc, int);
		h->hexist = 0;
		h->maxsize = 0;
		h->size = 0;
	}
}


/****************************************************************************/
/*                                                                          */
/*                          matrix                                          */
/*                                                                          */
/*  Written by:  Applegate, Cook, Dash                                      */
/*  Date:                                                                   */
/*                                                                          */
/*    EXPORTED FUNCTIONS:                                                   */
/*                                                                          */
/****************************************************************************/

void ILLmatrix_init (ILLmatrix * A)
{
	if (A)
	{
		A->matval = 0;
		A->matcnt = 0;
		A->matbeg = 0;
		A->matind = 0;
		A->matcols = 0;
		A->matcolsize = 0;
		A->matrows = 0;
		A->matsize = 0;
		A->matfree = 0;
	}
}

void ILLmatrix_free (ILLmatrix * A)
{
	if (A)
	{
		EGlpNumFreeArray (A->matval);
		ILL_IFFREE (A->matcnt, int);
		ILL_IFFREE (A->matbeg, int);
		ILL_IFFREE (A->matind, int);
		ILLmatrix_init (A);
	}
}

void ILLmatrix_prt (FILE * fd,
										ILLmatrix * A)
{
	int j,
	  k;
	if (A == NULL)
	{
		fprintf (fd, "Matrix %p: empty\n", (void *) A);
	}
	else
	{
		fprintf (fd, "Matrix %p: nrows = %d ncols = %d\n",
						 (void *) A, A->matrows, A->matcols);
		for (j = 0; j < A->matcols; j++)
		{
			fprintf (fd, "col %d: ", j);
			for (k = A->matbeg[j]; k < A->matbeg[j] + A->matcnt[j]; k++)
			{
				fprintf (fd, "row %d=%.3f ", A->matind[k], EGlpNumToLf (A->matval[k]));
			}
			fprintf (fd, "\n");
		}
	}
}
