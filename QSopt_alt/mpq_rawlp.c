/****************************************************************************/
/*                                                                          */
/* This file is part of QSopt_ex.                                           */
/*                                                                          */
/* (c) Copyright 2006 by David Applegate, William Cook, Sanjeeb Dash,       */
/* and Daniel Espinoza.  Sanjeeb Dash's ownership of copyright in           */
/* QSopt_ex is derived from his copyright in QSopt.                         */
/*                                                                          */
/* This code may be used under the terms of the GNU General Public License  */
/* (Version 2.1 or later) as published by the Free Software Foundation.     */
/*                                                                          */
/* Alternatively, use is granted for research purposes only.                */
/*                                                                          */
/* It is your choice of which of these two licenses you are operating       */
/* under.                                                                   */
/*                                                                          */
/* We make no guarantees about the correctness or usefulness of this code.  */
/*                                                                          */
/****************************************************************************/

/* RCS_INFO = "$RCSfile: mpq_rawlp.c,v $ $Revision: 1.2 $ $Date: 2003/11/05
   16:49:52 $"; */
/****************************************************************************/
/* DataStructure and routines to deal with raw lp information as read       */
/* from mps or lp files.                                                    */
/****************************************************************************/

#include "econfig.h"
#include "config.h"
#include "mpq_sortrus.h"
#include "mpq_iqsutil.h"
#include "mpq_rawlp.h"
#include "allocrus.h"
#ifdef USEDMALLOC
#include "dmalloc.h"
#endif
static int TRACE = 0;

ILL_PTRWORLD_ROUTINES (mpq_colptr, colptralloc, colptr_bulkalloc, colptrfree)
ILL_PTRWORLD_LEAKS_ROUTINE (mpq_colptr, colptr_check_leaks, this, int)
const int mpq_ILL_SOS_TYPE1 = 1;
const int mpq_ILL_SOS_TYPE2 = 2;

static void mpq_ILLprt_EGlpNum (FILE * f, mpq_t * d)
{
    if (mpq_EGlpNumIsLeq (mpq_ILL_MAXDOUBLE, *d)) {
	fprintf (f, "MAX_DOUBLE");
    } else {
	if (mpq_EGlpNumIsLeq (*d, mpq_ILL_MINDOUBLE)) {
	    fprintf (f, "-MAX_DOUBLE");
	} else {
	    fprintf (f, "%f", mpq_EGlpNumToLf (*d));
	}
    }
}

static int mpq_ILLraw_check_bounds (mpq_rawlpdata * lp);

void mpq_ILLinit_rawlpdata (mpq_rawlpdata * lp,
      mpq_qserror_collector * collector)
{
    if (lp) {
	lp->name = 0;
	lp->ncols = 0;
	lp->nrows = 0;
	lp->cols = 0;
	lp->rowsense = 0;
	lp->rhsname = 0;
	lp->rhs = 0;
	lp->rhsind = 0;
	lp->rangesname = 0;
	lp->rangesind = 0;
	lp->ranges = 0;
	lp->boundsname = 0;
	lp->lbind = 0;
	lp->ubind = 0;
	lp->lower = 0;
	lp->upper = 0;
	lp->intmarker = 0;
	lp->colsize = 0;
	lp->sensesize = 0;
	lp->intsize = 0;
	lp->rhssize = 0;
	lp->refrow = NULL;
	lp->is_sos_size = 0;
	lp->is_sos_member = NULL;
	lp->nsos_member = 0;
	lp->sos_weight_size = 0;
	lp->sos_weight = NULL;
	lp->sos_col_size = 0;
	lp->sos_col = NULL;
	lp->nsos = 0;
	lp->sos_setsize = 0;
	lp->sos_set = NULL;
	ILLsymboltab_init (&lp->coltab);
	ILLsymboltab_init (&lp->rowtab);
	lp->objindex = -1;
	lp->objsense = mpq_ILL_MIN;
	lp->refrowind = -1;	/* undefined */
	ILLptrworld_init (&lp->ptrworld);
	lp->error_collector = collector;
    }
}

void mpq_ILLraw_clear_matrix (mpq_rawlpdata * lp)
{
    int i;
    mpq_colptr *next, *curr;
    if ((lp != NULL) && (lp->cols != NULL)) {
	for (i = 0; i < lp->ncols; i++) {
	    {
		curr = lp->cols[i];
		while (curr) {
		    next = curr->next;
		    mpq_EGlpNumClearVar ((curr->coef));
		    colptrfree (&(lp->ptrworld), curr);
		    curr = next;
		}
	    }
	    lp->cols[i] = NULL;
	}
    }
}

void mpq_ILLfree_rawlpdata (mpq_rawlpdata * lp)
{
    int total, onlist;
    mpq_colptr *next, *curr;

    if (lp) {
	ILL_IFFREE (lp->name, char);
	ILLsymboltab_free (&lp->rowtab);
	ILLsymboltab_free (&lp->coltab);
	ILL_IFFREE (lp->rowsense, char);
	mpq_ILLraw_clear_matrix (lp);
	ILL_IFFREE (lp->cols, mpq_colptr *);
	{
	    curr = lp->ranges;
	    while (curr) {
		next = curr->next;
		mpq_EGlpNumClearVar ((curr->coef));
		colptrfree (&(lp->ptrworld), curr);
		curr = next;
	    }
	}
	if (colptr_check_leaks (&lp->ptrworld, &total, &onlist)) {
	    fprintf (stderr, "WARNING: %d outstanding colptrs\n", total - onlist);
	}
	ILLptrworld_delete (&lp->ptrworld);
	ILL_IFFREE (lp->rhsname, char);
	mpq_EGlpNumFreeArray (lp->rhs);
	ILL_IFFREE (lp->rhsind, char);
	ILL_IFFREE (lp->rangesname, char);
	ILL_IFFREE (lp->rangesind, char);
	ILL_IFFREE (lp->boundsname, char);
	ILL_IFFREE (lp->lbind, char);
	ILL_IFFREE (lp->ubind, char);
	mpq_EGlpNumFreeArray (lp->lower);
	mpq_EGlpNumFreeArray (lp->upper);
	ILL_IFFREE (lp->intmarker, char);
	ILL_IFFREE (lp->refrow, char);
	ILL_IFFREE (lp->is_sos_member, int);
	mpq_EGlpNumFreeArray (lp->sos_weight);
	ILL_IFFREE (lp->sos_col, int);
	ILL_IFFREE (lp->sos_set, mpq_sosptr);
	mpq_ILLinit_rawlpdata (lp, NULL);
    }
}

const char *mpq_ILLraw_rowname (mpq_rawlpdata * lp,
      int i)
{
    const char *name = NULL;
    ILL_FAILfalse_no_rval ((i >= 0) && (i < lp->nrows), "index out of range");
    ILL_FAILfalse_no_rval (lp->nrows == lp->rowtab.tablesize,
	"tab and lp must be in synch");
    name = ILLsymboltab_get (&lp->rowtab, i);
CLEANUP:
    return name;
}
const char *mpq_ILLraw_colname (mpq_rawlpdata * lp,
      int i)
{
    const char *name = NULL;
    ILL_FAILfalse_no_rval ((i >= 0) && (i < lp->ncols), "index out of range");
    ILL_FAILfalse_no_rval (lp->ncols == lp->coltab.tablesize,
	"tab and lp must be in synch");
    name = ILLsymboltab_get (&lp->coltab, i);
CLEANUP:
    return name;
}

int mpq_ILLraw_add_col (mpq_rawlpdata * lp,
      const char *name,
      int intmarker)
{
    int rval = 0;
    int pindex, hit;

    rval = ILLsymboltab_register (&lp->coltab, name, -1, &pindex, &hit);
    rval = rval || hit;
    ILL_CLEANUP_IF (rval);
    if (lp->ncols >= lp->colsize) {
	lp->colsize *= 1.3;
	lp->colsize += 1000;
	if (lp->colsize < lp->ncols + 1)
	    lp->colsize = lp->ncols + 1;
	lp->cols = EGrealloc (lp->cols, lp->colsize * sizeof (mpq_colptr *));
	/*
	        rval = rval || ILLutil_reallocrus_scale (&lp->cols,
	              &lp->colsize, lp->ncols + 1, 1.3, sizeof (mpq_colptr *));
	*/
    }
    if (lp->ncols >= lp->intsize) {
	lp->intsize *= 1.3;
	lp->intsize += 1000;
	if (lp->intsize < lp->ncols + 1)
	    lp->intsize = lp->ncols + 1;
	lp->intmarker = EGrealloc (lp->intmarker, lp->intsize * sizeof (char));
	/*
	        rval = rval || ILLutil_reallocrus_scale ((void **) &lp->intmarker,
	                                                                                         &lp->intsize, lp->ncols + 1,
	                                                                                         1.3, sizeof (char));
	*/
    }
    if (lp->ncols >= lp->is_sos_size) {
	lp->is_sos_size *= 1.3;
	lp->is_sos_size += 1000;
	if (lp->is_sos_size < lp->ncols + 1)
	    lp->is_sos_size = lp->ncols + 1;
	lp->is_sos_member = EGrealloc (lp->is_sos_member,
	    sizeof (int) * lp->is_sos_size);
	/*
	        rval = rval || ILLutil_reallocrus_scale ((void **) &lp->is_sos_member,
	                                                                                         &lp->is_sos_size, lp->ncols + 1,
	                                                                                         1.3, sizeof (int));
	*/
    }
    ILL_CLEANUP_IF (rval);
    lp->cols[lp->ncols] = 0;
    lp->is_sos_member[lp->ncols] = -1;
    lp->intmarker[lp->ncols] = intmarker;
    lp->ncols++;
CLEANUP:
    ILL_RETURN (rval, "mpq_ILLraw_add_col");
}

int mpq_ILLraw_init_rhs (mpq_rawlpdata * lp)
{
    int i, rval = 0;

    ILL_FAILfalse (lp->rhsind == NULL, "Should be called exactly once");
    if (lp->nrows > 0) {
	ILL_SAFE_MALLOC (lp->rhsind, lp->nrows, char);
	for (i = 0; i < lp->nrows; i++) {
	    lp->rhsind[i] = (char) 0;
	}
    }
CLEANUP:
    ILL_RETURN (rval, "mpq_ILLraw_init_rhs");
}

int mpq_ILLraw_init_ranges (mpq_rawlpdata * lp)
{
    int i, rval = 0;

    ILL_FAILfalse (lp->rangesind == NULL, "Should be called exactly once");
    if (lp->nrows > 0) {
	ILL_SAFE_MALLOC (lp->rangesind, lp->nrows, char);
	for (i = 0; i < lp->nrows; i++) {
	    lp->rangesind[i] = (char) 0;
	}
    }
CLEANUP:
    ILL_RETURN (rval, "mpq_ILLraw_init_ranges");
}

int mpq_ILLraw_add_col_coef (mpq_rawlpdata * lp,
      int colind,
      int rowind,
      mpq_t coef)
{
    mpq_colptr *cp = mpq_ILLcolptralloc (&lp->ptrworld);
    if (!cp) {
	return 1;
    }
    cp->this = rowind;
    mpq_EGlpNumCopy (cp->coef, coef);
    cp->next = lp->cols[colind];
    lp->cols[colind] = cp;
    return 0;
}


int mpq_ILLraw_add_ranges_coef (mpq_rawlpdata * lp,
      int rowind,
      mpq_t coef)
{
    mpq_colptr *cp = mpq_ILLcolptralloc (&lp->ptrworld);
    if (!cp) {
	return 1;
    }
    cp->this = rowind;
    mpq_EGlpNumCopy (cp->coef, coef);
    cp->next = lp->ranges;
    lp->ranges = cp;
    lp->rangesind[rowind] = (char) 1;
    return 0;
}

int mpq_ILLraw_add_sos (mpq_rawlpdata * lp,
      int tp)
{
    int rval = 0;
    mpq_sosptr *sos, *bef;

    if (lp->nsos >= lp->sos_setsize) {
	lp->sos_setsize *= 1.3;
	lp->sos_setsize += 1000;
	if (lp->sos_setsize < lp->nsos + 1)
	    lp->sos_setsize = lp->nsos + 1;
	lp->sos_set = EGrealloc (lp->sos_set, sizeof (mpq_sosptr *) * lp->sos_setsize);
	/*
	if (ILLutil_reallocrus_scale ((void **) &lp->sos_set,
           &lp->sos_setsize, lp->nsos + 1, 1.3, sizeof (mpq_sosptr *))) {
           ILL_CLEANUP_IF (rval);
        }
	*/
    }
    sos = lp->sos_set + lp->nsos;
    sos->nelem = 0;
    sos->type = tp;
    if (lp->nsos == 0) {
	sos->first = 0;
    } else {
	bef = &(lp->sos_set[lp->nsos - 1]);
	sos->first = bef->first + bef->nelem;
    }
    lp->nsos++;
    /* CLEANUP: */
    ILL_RETURN (rval, "mpq_ILLraw_add_sos");
}

int mpq_ILLraw_is_mem_other_sos (mpq_rawlpdata * lp,
      int colind)
{
    return (lp->is_sos_member[colind] >= 0) &&
    (lp->is_sos_member[colind] != (lp->nsos - 1));
}

int mpq_ILLraw_add_sos_member (mpq_rawlpdata * lp,
      int colind)
{
    int rval = 0;
    ILL_FAILfalse (lp->nsos > 0, "we should have called mpq_ILLraw_add_sos earlier");
    ILL_FAILtrue (mpq_ILLraw_is_mem_other_sos (lp, colind),
	"colind is member of another sos set");

    if (lp->is_sos_member[colind] == -1) {
	if (lp->nsos_member >= lp->sos_weight_size) {
	    lp->sos_weight_size *= 1.3;
	    lp->sos_weight_size += 1000;
	    if (lp->sos_weight_size < lp->nsos_member + 1)
		lp->sos_weight_size = lp->nsos_member + 1;
	    lp->sos_weight = EGrealloc (lp->sos_weight,
		lp->sos_weight_size * sizeof (double));
	    /*
	                if (ILLutil_reallocrus_scale ((void **) &lp->sos_weight,
	                                                                            &lp->sos_weight_size,
	                                                                            lp->nsos_member + 1, 1.3, sizeof (double)))
	                {
	                    ILL_CLEANUP_IF (rval);
	                }
	    */
	}
	if (lp->nsos_member >= lp->sos_col_size) {
	    lp->sos_col_size *= 1.3;
	    lp->sos_col_size += 1000;
	    if (lp->sos_col_size < lp->nsos_member + 1)
		lp->sos_col_size = lp->nsos_member + 1;
	    lp->sos_col = EGrealloc (lp->sos_col, sizeof (int) * lp->sos_col_size);
	    /*
	                if (ILLutil_reallocrus_scale ((void **) &lp->sos_col,
	                                                                            &lp->sos_col_size,
	                                                                            lp->nsos_member + 1, 1.3, sizeof (int)))
	                {
	                    ILL_CLEANUP_IF (rval);
	                }
	    */
	}
	lp->sos_col[lp->nsos_member] = colind;
	lp->sos_set[lp->nsos - 1].nelem++;
	lp->is_sos_member[colind] = lp->nsos - 1;
	lp->nsos_member++;
    }
CLEANUP:
    ILL_RETURN (rval, "mpq_ILLraw_add_sos_member");
}


int mpq_ILLraw_add_row (mpq_rawlpdata * lp,
      const char *name,
      int sense,
      mpq_t rhs)
{
    int pindex, hit, rval = 0;

    rval = ILLsymboltab_register (&lp->rowtab, name, -1, &pindex, &hit);
    rval = rval || hit;
    ILL_CLEANUP_IF (rval);
    if (lp->nrows >= lp->sensesize) {
	lp->sensesize *= 1.3;
	lp->sensesize += 1000;
	if (lp->sensesize < lp->nrows + 1)
	    lp->sensesize = lp->nrows + 1;
	lp->rowsense = EGrealloc (lp->rowsense, sizeof (char) * lp->sensesize);
	/*
	        if (ILLutil_reallocrus_scale ((void **) &lp->rowsense,
	                                                                    &lp->sensesize, lp->nrows + 1,
	                                                                    1.3, sizeof (char)))
	        {
	            ILL_CLEANUP_IF (rval);
	        }
	*/
    }
    if (lp->nrows >= lp->rhssize) {
	if (lp->rhssize + 1000 < (lp->nrows + 1) * 1.3)
	    lp->rhssize = (lp->nrows + 1) * 1.3;
	else
	    lp->rhssize += 1000;
	mpq_EGlpNumReallocArray (&(lp->rhs), lp->rhssize);
    }
    lp->rowsense[lp->nrows] = sense;
    mpq_EGlpNumCopy (lp->rhs[lp->nrows], rhs);
    lp->nrows++;

CLEANUP:
    ILL_RETURN (rval, "mpq_ILLraw_add_row");
}

static int mpq_ILLcheck_rawlpdata (mpq_rawlpdata * lp)
{
    int i, col, rval = 0;
    int si, *perm = NULL;
    const char *c1, *c2;
    mpq_sosptr *set;

    ILL_FAILfalse (lp, "lp must not be NULL");

    /* check    *) that there is at least one variable *) that the weights in
       all SOS sets are distinct *) all sos members are non integer variables
       ) sos set members have distint weights *) objindex is not -1 *)
       INVARIANT: rowname[objindex] != NULL *) INVARIANT: upper/lower arrays
       are filled in *) INVARIANT: if col or rownames != NULL then all their
       elements are not NULL */
    if (lp->ncols < 1) {
	return mpq_ILLdata_error (lp->error_collector, "There are no variables.");
    }
    if (lp->objindex == -1) {
	return mpq_ILLdata_error (lp->error_collector, "There is no objective fct.");
    }
    ILL_FAILfalse (mpq_ILLraw_rowname (lp, lp->objindex) != NULL,
	"must have objective name");
    if (lp->nsos_member > 1) {
	ILL_SAFE_MALLOC (perm, lp->nsos_member, int);
	for (si = 0; si < lp->nsos; si++) {
	    set = lp->sos_set + si;
	    for (i = 0; i < set->nelem; i++) {
		col = lp->sos_col[set->first + i];
		if (lp->intmarker[col]) {
		    rval = mpq_ILLdata_error (lp->error_collector,
			"SOS set member \"%s\" is an %s.\n",
			mpq_ILLraw_colname (lp, col),
			"integer/binary variable");
		}
	    }
	    if (set->nelem > 1) {
		for (i = 0; i < set->nelem; i++) {
		    perm[i] = set->first + i;
		}
		mpq_ILLutil_EGlpNum_perm_quicksort (perm, lp->sos_weight, set->nelem);
		for (i = 1; i < set->nelem; i++) {
		    if (mpq_EGlpNumIsEqqual
			(lp->sos_weight[perm[i - 1]], lp->sos_weight[perm[i]])) {
			c1 = mpq_ILLraw_colname (lp, lp->sos_col[perm[i]]);
			c2 = mpq_ILLraw_colname (lp, lp->sos_col[perm[i - 1]]);
			mpq_ILLdata_error (lp->error_collector,
			    "\"%s\" and \"%s\" both have %s %f.\n", c1, c2,
			    "SOS weight", lp->sos_weight[perm[i]]);
			rval = 1;
		    }
		}
	    }
	}
    }
    for (i = 0; i < lp->ncols; i++) {
	ILL_CHECKnull (mpq_ILLraw_colname (lp, i), "There is a NULL col name");
    }
    for (i = 0; i < lp->nrows; i++) {
	ILL_CHECKnull (mpq_ILLraw_rowname (lp, i), "There is a NULL row name");
    }
    ILL_FAILtrue ((lp->upper == NULL) | (lp->lower == NULL),
	"Upper/Lower arrays must be filled in.");

    rval += mpq_ILLraw_check_bounds (lp);
CLEANUP:
    ILL_IFFREE (perm, int);
    ILL_RESULT (rval, "mpq_ILLcheck_rawlpdata");
}

int mpq_ILLraw_init_bounds (mpq_rawlpdata * lp)
{
    int i, rval = 0;

    ILL_FAILfalse (lp->upper == NULL, "Should be called exactly once");
    ILL_FAILfalse (lp->lower == NULL, "Should be called exactly once");
    ILL_FAILfalse (lp->lbind == NULL, "Should be called exactly once");
    ILL_FAILfalse (lp->ubind == NULL, "Should be called exactly once");
    lp->upper = mpq_EGlpNumAllocArray (lp->ncols);
    lp->lower = mpq_EGlpNumAllocArray (lp->ncols);
    ILL_SAFE_MALLOC (lp->lbind, lp->ncols, char);
    ILL_SAFE_MALLOC (lp->ubind, lp->ncols, char);

    for (i = 0; i < lp->ncols; i++) {
	lp->lbind[i] = (char) 0;
	lp->ubind[i] = (char) 0;
	mpq_EGlpNumZero (lp->lower[i]);
    }
CLEANUP:
    ILL_RETURN (rval, "mpq_ILLraw_init_bounds");
}

const char *mpq_ILLraw_set_lowerBound (mpq_rawlpdata * lp,
      int i,
      mpq_t bnd)
{
    ILL_FAILtrue_no_rval (i >= lp->ncols, "proper colind");
    if (lp->lbind[i]) {
	return "Using previous bound definition.";
    }
    mpq_EGlpNumCopy (lp->lower[i], bnd);
    lp->lbind[i] = (char) 1;
CLEANUP:
    return NULL;
}

const char *mpq_ILLraw_set_upperBound (mpq_rawlpdata * lp,
      int i,
      mpq_t bnd)
{
    ILL_FAILtrue_no_rval (i >= lp->ncols, "proper colind");
    if (lp->ubind[i]) {
	return "Using previous bound definition.";
    }
    mpq_EGlpNumCopy (lp->upper[i], bnd);
    lp->ubind[i] = (char) 1;
    if (mpq_EGlpNumIsEqqual (lp->lower[i], mpq_zeroLpNum) &&
	mpq_EGlpNumIsEqqual (bnd, mpq_zeroLpNum)) {
	return "0.0 upper bound fixes variable.";
    }
CLEANUP:
    return NULL;
}

const char *mpq_ILLraw_set_fixedBound (mpq_rawlpdata * lp,
      int i,
      mpq_t bnd)
{
    ILL_FAILtrue_no_rval (i >= lp->ncols, "proper colind");
    if (lp->ubind[i] || lp->lbind[i]) {
	return "Using previous bound definition.";
    }
    mpq_EGlpNumCopy (lp->lower[i], bnd);
    lp->lbind[i] = (char) 1;
    mpq_EGlpNumCopy (lp->upper[i], bnd);
    lp->ubind[i] = (char) 1;
CLEANUP:
    return NULL;
}

const char *mpq_ILLraw_set_unbound (mpq_rawlpdata * lp,
      int i)
{
    ILL_FAILtrue_no_rval (i >= lp->ncols, "proper colind");
    if (lp->lbind[i] || lp->ubind[i]) {
	return "Using previous bound definition.";
    }
    mpq_EGlpNumCopy (lp->lower[i], mpq_ILL_MINDOUBLE);
    mpq_EGlpNumCopy (lp->upper[i], mpq_ILL_MAXDOUBLE);
    lp->lbind[i] = 1;
    lp->ubind[i] = 1;
CLEANUP:
    return NULL;
}

const char *mpq_ILLraw_set_binaryBound (mpq_rawlpdata * lp,
      int i)
{
    ILL_FAILtrue_no_rval (i >= lp->ncols, "proper colind");
    if (lp->lbind[i] || lp->ubind[i]) {
	return "Using previous bound definition.";
    }
    mpq_EGlpNumZero (lp->lower[i]);
    mpq_EGlpNumOne (lp->upper[i]);
    lp->lbind[i] = 1;
    lp->ubind[i] = 1;
CLEANUP:
    return NULL;
}

int mpq_ILLraw_fill_in_bounds (mpq_rawlpdata * lp)
{
    int rval = 0, i;
    if (lp->lbind == NULL) {
	mpq_ILLraw_init_bounds (lp);
    }
    ILL_FAILtrue (lp->upper == NULL, "must all be there now");
    ILL_FAILtrue (lp->lower == NULL, "must all be there now");
    ILL_FAILtrue (lp->lbind == NULL, "must all be there now");
    ILL_FAILtrue (lp->ubind == NULL, "must all be there now");
    for (i = 0; i < lp->ncols; i++) {
	if (!lp->lbind[i]) {
	    if (lp->ubind[i] && mpq_EGlpNumIsLess (lp->upper[i], mpq_zeroLpNum)) {
		mpq_EGlpNumCopy (lp->lower[i], mpq_ILL_MINDOUBLE);
	    }
	}
	if (!lp->ubind[i]) {
	    /* int vars without bounds are binary                        */
	    /* all, also int vars                                        */
	    /* with explicit lower bound 0.0 are in [0.0,+inf]  */
	    if (((lp->intmarker != NULL) && lp->intmarker[i]) && !lp->lbind[i]) {
		mpq_EGlpNumOne (lp->upper[i]);
	    } else {
		mpq_EGlpNumCopy (lp->upper[i], mpq_ILL_MAXDOUBLE);
	    }
	}
    }

CLEANUP:
    if (rval) {
	mpq_EGlpNumFreeArray (lp->lower);
	mpq_EGlpNumFreeArray (lp->upper);
    }
    ILL_RETURN (rval, "mpq_ILLraw_fill_in_bounds");
}

static int mpq_ILLraw_check_bounds (mpq_rawlpdata * lp)
{
    int rval = 0, i;
    ILL_FAILtrue (lp->upper == NULL, "must all be there now");
    ILL_FAILtrue (lp->lower == NULL, "must all be there now");
    ILL_FAILtrue (lp->lbind == NULL, "must all be there now");
    ILL_FAILtrue (lp->ubind == NULL, "must all be there now");
    for (i = 0; i < lp->ncols; i++) {
	if (mpq_EGlpNumIsLess (lp->upper[i], lp->lower[i])) {
	    rval += mpq_ILLdata_error (lp->error_collector,
		"Lower bound is bigger than %s \"%s\".\n",
		"upper bound for", mpq_ILLraw_colname (lp, i));
	}
    }
    ILL_RESULT (rval, "mpq_ILLraw_check_bounds");
CLEANUP:
    ILL_RETURN (rval, "mpq_ILLraw_check_bounds");
}

int mpq_ILLraw_first_nondefault_bound (mpq_ILLlpdata * lp)
{
    int ri = lp->nstruct, i;
    ILL_FAILtrue_no_rval (lp->lower == NULL || lp->upper == NULL,
	"Should not call mpq_write_bounds when lower or upper are NULL");
    for (ri = 0; ri < lp->nstruct; ri++) {
	i = lp->structmap[ri];
	if (!mpq_ILLraw_default_lower (lp, i) || !mpq_ILLraw_default_upper (lp, i))
	    break;
    }
CLEANUP:
    return ri;
}

int mpq_ILLraw_default_lower (mpq_ILLlpdata * lp,
      int i)
{
    ILL_FAILtrue_no_rval (lp->lower == NULL || lp->upper == NULL,
	"Should not call mpq_write_bounds when lower or upper are NULL");
    ILL_FAILfalse_no_rval (lp->ncols > i, "i is not col index");
    if (mpq_EGlpNumIsEqqual (lp->lower[i], mpq_zeroLpNum) &&
	mpq_EGlpNumIsLeq (mpq_zeroLpNum, lp->upper[i])) {
	return 1;
    }
    if (mpq_EGlpNumIsEqqual (lp->lower[i], mpq_ILL_MINDOUBLE) &&
	mpq_EGlpNumIsLess (lp->upper[i], mpq_zeroLpNum)) {
	return 1;
    }
CLEANUP:
    return 0;
}

int mpq_ILLraw_default_upper (mpq_ILLlpdata * lp,
      int i)
{
    int isInt;

    ILL_FAILtrue_no_rval (lp->lower == NULL || lp->upper == NULL,
	"Should not call mpq_write_bounds when lower or upper are NULL");
    ILL_FAILfalse_no_rval (lp->ncols >= i, "i is not col index");
    isInt = (lp->intmarker != NULL) && lp->intmarker[i];
    if (isInt) {
	if (mpq_EGlpNumIsEqqual (lp->lower[i], mpq_zeroLpNum)) {
	    return (mpq_EGlpNumIsEqqual (lp->upper[i], mpq_oneLpNum));
	}
    }
    if (mpq_EGlpNumIsEqqual (lp->upper[i], mpq_ILL_MAXDOUBLE)) {
	return 1;
    }
CLEANUP:
    return 0;
}

int mpq_ILLraw_fill_in_rownames (mpq_rawlpdata * lp)
{
    int i, rval = 0;
    char uname[ILL_namebufsize];
    ILLsymboltab *rowtab;
    char first = 1;

    rowtab = &lp->rowtab;
    ILL_FAILtrue (lp->nrows != rowtab->tablesize, "must have same #entries");
    for (i = 0; (rval == 0) && i < lp->nrows; i++) {
	if (ILLsymboltab_get (rowtab, i) == NULL) {
	    if (first) {
		mpq_ILLdata_warn (lp->error_collector,
		    "Generating names for unnamed rows.");
		first = 0;
	    }
	    ILLsymboltab_unique_name (rowtab, i, "c", uname);
	    rval = ILLsymboltab_rename (rowtab, i, uname);
	    ILL_CLEANUP_IF (rval);
	}
    }
CLEANUP:
    ILL_RESULT (rval, "mpq_ILLraw_fill_in_rownames");
}

static int mpq_whichColsAreUsed (mpq_rawlpdata * raw,
      mpq_ILLlpdata * lp,
      int *colindex)
{
    int rval = 0;
    int i, objind = raw->objindex;
    mpq_colptr *cp;
    char *colUsed = NULL;

    /* colUsed[i]  variable raw->colnames[i] is used in obj fct and/or
       equation(s) */
    ILL_SAFE_MALLOC (colUsed, raw->ncols, char);
    for (i = 0; i < raw->ncols; i++) {
	colUsed[i] = 0;
    }
    for (i = 0; i < raw->ncols; i++) {
	for (cp = raw->cols[i]; cp; cp = cp->next) {
	    if ((cp->this == objind) || (raw->rowsense[cp->this] != 'N')) {
		colUsed[i] = 1;
		break;
	    }
	}
    }

    /* colindex[i] = -1 for undefined, 0, 1, ... lp->ncol-1 lp->ncols <=
       raw->ncols */
    for (i = 0; i < raw->ncols; i++) {
	if (colUsed[i]) {
	    colindex[i] = lp->ncols++;
	} else {
	    colindex[i] = -1;
	    mpq_ILLdata_warn (raw->error_collector,
		"\"%s\" is used in non objective 'N' rows only.",
		mpq_ILLraw_colname (raw, i));
	}
    }
    if (lp->ncols < 1) {
	rval = mpq_ILLdata_error (raw->error_collector, "There are no variables.");
	ILL_CLEANUP_IF (rval);
    }
CLEANUP:
    ILL_IFFREE (colUsed, char);
    ILL_RESULT (rval, "mpq_whichColsAreUsed");
}

static int mpq_whichRowsAreUsed (mpq_rawlpdata * raw,
      mpq_ILLlpdata * lp,
      int *rowindex)
{
    int i, rval = 0;

    /* only use non 'N' rows */
    for (i = 0; i < raw->nrows; i++) {
	if (raw->rowsense[i] != 'N') {
	    rowindex[i] = lp->nrows++;
	} else {
	    rowindex[i] = -1;
	}
    }
    if (lp->nrows == 0) {
	rval = mpq_ILLdata_error (raw->error_collector, "There are no constraints.");
    }
    ILL_RESULT (rval, "mpq_whichRowsAreUsed");
}


static int mpq_transferObjective (mpq_rawlpdata * raw,
      mpq_ILLlpdata * lp,
      int *colindex)
{
    int rval = 0, i, ci, objind = raw->objindex;
    mpq_colptr *cp;
    int *coefWarn = NULL;

    /* transfer objective fct */
    lp->obj = mpq_EGlpNumAllocArray (lp->ncols);
    ILL_SAFE_MALLOC (coefWarn, lp->ncols, int);
    for (i = 0; i < lp->ncols; i++) {
	mpq_EGlpNumZero (lp->obj[i]);
	coefWarn[i] = 0;
    }
    for (i = 0; i < raw->ncols; i++) {
	for (cp = raw->cols[i]; cp; cp = cp->next) {
	    if (cp->this == objind) {
		ci = colindex[i];
		TESTG ((rval =
			(ci < 0
			    || ci >= lp->ncols)), CLEANUP, "ci %d is out of range [0,%d[",
		    ci, lp->ncols);
		ILL_FAILfalse (ci != -1,
		    "all vars in obj fct should be marked as useful");
		coefWarn[ci]++;
		if (mpq_EGlpNumIsNeqqZero (cp->coef))
		    mpq_EGlpNumAddTo (lp->obj[ci], cp->coef);
		if (coefWarn[ci] == 2) {
		    mpq_ILLdata_warn (raw->error_collector,
			"Multiple coefficients for \"%s\" in %s.",
			mpq_ILLraw_colname (raw, i), "objective function");
		}
	    }
	}
    }
CLEANUP:
    ILL_IFFREE (coefWarn, int);
    ILL_RETURN (rval, "mpq_transferObjective");
}

static int mpq_transferColNamesLowerUpperIntMarker (mpq_rawlpdata * raw,
      mpq_ILLlpdata * lp,
      int *colindex)
{
    int i, ci, ind, pre, rval = 0;
    int hasIntVar;
    ILL_SAFE_MALLOC (lp->colnames, lp->ncols, char *);
    if (raw->upper)
	lp->upper = mpq_EGlpNumAllocArray (lp->ncols);
    if (raw->lower)
	lp->lower = mpq_EGlpNumAllocArray (lp->ncols);
    ILL_SAFE_MALLOC (lp->intmarker, lp->ncols, char);
    hasIntVar = 0;
    for (i = 0; i < raw->ncols; i++) {
	ci = colindex[i];
	if (ci != -1) {
	    ILL_FAILfalse ((ci >= 0) && (ci < lp->ncols), "colindex problem");
	    mpq_ILL_UTIL_STR (lp->colnames[ci], mpq_ILLraw_colname (raw, i));
	    rval = ILLsymboltab_register (&lp->coltab,
		lp->colnames[ci], -1, &ind, &pre);
	    ILL_FAILfalse ((rval == 0) && (ind == ci) && (pre == 0),
		"should have new entry");
	    if (raw->upper) {
		mpq_EGlpNumCopy (lp->upper[ci], raw->upper[i]);
	    }
	    if (raw->lower) {
		mpq_EGlpNumCopy (lp->lower[ci], raw->lower[i]);
	    }
	    lp->intmarker[ci] = raw->intmarker[i];
	    hasIntVar = hasIntVar || lp->intmarker[ci];
	    ILL_IFDOTRACE
	    {
		if (lp->lower) {
		    mpq_ILLprt_EGlpNum (stdout, &(lp->lower[ci]));
		    ILL_IFTRACE (" <= ");
		}
		ILL_IFTRACE ("%s", lp->colnames[ci]);
		if (lp->upper) {
		    ILL_IFTRACE (" <= ");
		    mpq_ILLprt_EGlpNum (stdout, &(lp->upper[ci]));
		}
		if (lp->intmarker[ci]) {
		    ILL_IFTRACE (" INTEGER ");
		}
		ILL_IFTRACE ("\n");
	    }
	}
    }
    if (!hasIntVar) {
	ILL_IFFREE (lp->intmarker, char);
    }
CLEANUP:
    ILL_RETURN (rval, "mpq_transferColNamesLowerUpperIntMarker");
}

static void mpq_safeRegister (ILLsymboltab * tab,
      const char *name,
      int i)
{
    int ind, pre, rval;
    rval = ILLsymboltab_register (tab, name, -1, &ind, &pre);
    ILL_FAILfalse ((rval == 0) && (ind == i) && (pre == 0),
	"Pgming Error: should have new entry");
CLEANUP:
    return;
}

static int mpq_transferSenseRhsRowNames (mpq_rawlpdata * raw,
      mpq_ILLlpdata * lp,
      int *rowindex)
{
    int i, ri, rval = 0;
    int objind = raw->objindex;

    /* transfer sense/rhs/rownames */
    if (lp->nrows > 0) {
	ILL_SAFE_MALLOC (lp->sense, lp->nrows, char);
	lp->rhs = mpq_EGlpNumAllocArray (lp->nrows);
	ILL_SAFE_MALLOC (lp->rownames, lp->nrows, char *);

	ILL_FAILfalse (mpq_ILLraw_rowname (raw, raw->objindex), "NULL objname");
	mpq_safeRegister (&lp->rowtab, mpq_ILLraw_rowname (raw, raw->objindex), 0);

	ri = 0;
	for (i = 0; i < raw->nrows; i++) {
	    ri = rowindex[i];
	    if (i == raw->refrowind) {
		mpq_ILL_UTIL_STR (lp->refrowname, mpq_ILLraw_rowname (raw, i));
		lp->refind = ri;
	    }
	    if (raw->rowsense[i] != 'N') {
		ILL_FAILfalse (mpq_ILLraw_rowname (raw, i) != NULL,
		    "all rownames should be non NULL");
		mpq_ILL_UTIL_STR (lp->rownames[ri], mpq_ILLraw_rowname (raw, i));
		mpq_safeRegister (&lp->rowtab, lp->rownames[ri], ri + 1);
		lp->sense[ri] = raw->rowsense[i];
		mpq_EGlpNumCopy (lp->rhs[ri], raw->rhs[i]);
	    } else if (i == objind) {
		ILL_FAILfalse (lp->objname == NULL, "objname == NULL");
		mpq_ILL_UTIL_STR (lp->objname, mpq_ILLraw_rowname (raw, i));
	    } else {
		/* unused 'N' row */
	    }
	}
	ILL_FAILfalse ((lp->nrows + 1) == lp->rowtab.tablesize,
	    "problem with rowtab structure");
    }
CLEANUP:
    ILL_RETURN (rval, "mpq_transferSenseRhsRowNames");
}

static int mpq_buildMatrix (mpq_rawlpdata * raw,
      mpq_ILLlpdata * lp,
      int *rowindex,
      int *colindex)
{
    int i, ri, ci, k, nempty = 0, rval = 0;
    int *nRowsUsed = 0;
    int *coefSet = 0;
    int *coefWarn = 0;
    mpq_ILLmatrix *A = &lp->A;
    mpq_colptr *cp = NULL;

    /* put subjective fcts into matrix */
    ILL_SAFE_MALLOC (A->matcnt, lp->ncols, int);
    ILL_SAFE_MALLOC (A->matbeg, lp->ncols, int);
    ILL_SAFE_MALLOC (nRowsUsed, lp->nrows, int);

    ILL_SAFE_MALLOC (coefWarn, lp->ncols, int);
    for (i = 0; i < lp->ncols; i++) {
	coefWarn[i] = 0;
    }
    for (i = 0; i < lp->nrows; i++) {
	nRowsUsed[i] = -1;
    }
    for (i = 0; i < raw->ncols; i++) {
	ci = colindex[i];
	if (ci == -1)
	    continue;
	k = 0;
	for (cp = raw->cols[i]; cp; cp = cp->next) {
	    ri = rowindex[cp->this];
	    if (ri >= 0) {
		if (nRowsUsed[ri] != i) {
		    nRowsUsed[ri] = i;
		    k++;
		} else {
		    if (!coefWarn[ci]) {
			mpq_ILLdata_warn (raw->error_collector,
			    "Multiple coefficients for \"%s\" %s.",
			    lp->colnames[i], "in a row");
			coefWarn[ci] = 1;
		    }
		}
	    }
	}
	A->matcnt[ci] = k;
	A->matbeg[ci] = lp->nzcount + nempty;	/* mark empty cols */
	lp->nzcount += k;
	if (k == 0)
	    nempty++;
    }

    A->matrows = lp->nrows;
    A->matcols = lp->ncols;
    A->matcolsize = lp->ncols;
    A->matsize = lp->nzcount + nempty + 1;
    A->matfree = 1;
    ILL_SAFE_MALLOC (A->matind, A->matsize, int);
    A->matval = mpq_EGlpNumAllocArray (A->matsize);
    ILL_SAFE_MALLOC (coefSet, lp->nrows, int);

    for (k = 0; k < lp->nrows; k++) {
	coefSet[k] = -1;
    }

    for (i = 0; i < raw->ncols; i++) {
	ci = colindex[i];
	if (ci == -1)
	    continue;		/* unused variable */
	k = A->matbeg[ci];
	if (A->matcnt[ci] == 0) {
	    A->matind[k] = 1;	/* Used in addcols and addrows */
	} else {
	    for (cp = raw->cols[i]; cp; cp = cp->next) {
		ri = rowindex[cp->this];
		if (ri >= 0) {
		    if (coefSet[ri] == -1) {
			A->matind[k] = ri;
			mpq_EGlpNumCopy (A->matval[k], cp->coef);
			coefSet[ri] = k;
			k++;
		    } else {
			mpq_EGlpNumAddTo (A->matval[coefSet[ri]], cp->coef);
		    }
		}
	    }
	    if (k != A->matbeg[ci] + A->matcnt[ci]) {
		ILL_ERROR (rval, "problem with matrix");
	    }
	    for (k--; k >= A->matbeg[ci]; k--) {
		coefSet[A->matind[k]] = -1;
	    }
	}
    }
    A->matind[lp->nzcount + nempty] = -1;
CLEANUP:
    ILL_IFFREE (nRowsUsed, int);
    ILL_IFFREE (coefWarn, int);
    ILL_IFFREE (coefSet, int);
    ILL_RETURN (rval, "mpq_buildMatrix");
}

static int mpq_transferRanges (mpq_rawlpdata * raw,
      mpq_ILLlpdata * lp,
      int *rowindex)
{
    int i, ri, rval = 0;
    mpq_colptr *cp;

    /*****************************************************/
    /* */
    /* Interpretation of RANGE values in MPS files      */
    /* */
    /* G    rhs           <= row <= rhs + |range|     */
    /* L    rhs - |range| <= row <= rhs               */
    /* E +  rhs           <= row <= rhs + range       */
    /* E -  rhs + range   <= row <= rhs               */
    /* */
    /* - where + and - refer to the sign of range    */
    /* and the letters refer to sense of the row.  */
    /* */
    /* We will store ranged rows as                   */
    /* */
    /* rhs  <= row  <= rhs + range                 */
    /* */
    /*****************************************************/


    lp->rangeval = mpq_EGlpNumAllocArray (lp->nrows);
    for (i = 0; i < lp->nrows; i++) {
	mpq_EGlpNumZero (lp->rangeval[i]);
    }
    for (cp = raw->ranges; cp; cp = cp->next) {
	i = cp->this;
	ri = rowindex[cp->this];
	switch (raw->rowsense[i]) {
	case 'N':
	    mpq_ILLdata_error (raw->error_collector, "No range for N-row.\n");
	    rval = 1;
	    goto CLEANUP;
	case 'G':
	    lp->sense[ri] = 'R';
	    mpq_EGlpNumCopyAbs (lp->rangeval[ri], cp->coef);
	    break;
	case 'L':
	    lp->sense[ri] = 'R';
	    mpq_EGlpNumCopyAbs (lp->rangeval[ri], cp->coef);
	    mpq_EGlpNumSubTo (lp->rhs[ri], lp->rangeval[ri]);
	    break;
	case 'E':
	    lp->sense[ri] = 'R';
	    if (mpq_EGlpNumIsLeq (mpq_zeroLpNum, cp->coef)) {
		mpq_EGlpNumCopy (lp->rangeval[ri], cp->coef);
	    } else {
		mpq_EGlpNumAddTo (lp->rhs[ri], cp->coef);
		mpq_EGlpNumCopyNeg (lp->rangeval[ri], cp->coef);
	    }
	    break;
	}
    }
CLEANUP:
    ILL_RETURN (rval, "mpq_transferRanges");
}

static int mpq_initStructmap (mpq_ILLlpdata * lp)
{
    int i, rval = 0;

    /* all vars are structural */
    ILL_SAFE_MALLOC (lp->structmap, lp->nstruct, int);
    for (i = 0; i < lp->nstruct; i++) {
	lp->structmap[i] = i;
    }

CLEANUP:
    ILL_RETURN (rval, "mpq_initStructmap");
}

static int mpq_buildSosInfo (mpq_rawlpdata * raw,
      mpq_ILLlpdata * lp,
      int *colindex)
{
    int i, ci, set, rval = 0;
    int nSosMem, nSetMem;

    /* build sos info */
    /* see comment in mpq_lpdata.h about mpq_ILLlpdata's sos and is_sos_mem
       fields and section of mpq_ILLprint_rawlpdata that prints SOS sets */

    ILL_SAFE_MALLOC (lp->is_sos_mem, lp->ncols, int);
    nSosMem = 0;
    for (i = 0; i < raw->ncols; i++) {
	ci = colindex[i];
	if (ci != -1) {
	    lp->is_sos_mem[ci] = raw->is_sos_member[i];
	    if (raw->is_sos_member[i] != -1)
		nSosMem++;
	}
    }
    if (nSosMem > 0) {
	lp->sos.matsize = nSosMem;
	lp->sos.matcols = raw->nsos;
	lp->sos.matcolsize = raw->nsos;
	lp->sos.matrows = lp->ncols;
	lp->sos.matfree = 0;
	lp->sos.matval = mpq_EGlpNumAllocArray (nSosMem);
	ILL_SAFE_MALLOC (lp->sos.matind, nSosMem, int);
	ILL_SAFE_MALLOC (lp->sos.matbeg, raw->nsos, int);
	ILL_SAFE_MALLOC (lp->sos.matcnt, raw->nsos, int);
	ILL_SAFE_MALLOC (lp->sos_type, raw->nsos, char);
	nSosMem = 0;
	for (set = 0; set < raw->nsos; set++) {
	    lp->sos_type[set] = raw->sos_set[set].type;
	    lp->sos.matbeg[set] = nSosMem;
	    nSetMem = 0;
	    for (i = raw->sos_set[set].first;
		i < raw->sos_set[set].first + raw->sos_set[set].nelem; i++) {
		ci = colindex[raw->sos_col[i]];
		if (ci != -1) {
		    lp->sos.matind[nSosMem + nSetMem] = ci;
		    mpq_EGlpNumCopy (lp->sos.matval[nSosMem + nSetMem], raw->sos_weight[i]);
		    nSetMem++;
		}
	    }
	    lp->sos.matcnt[set] = nSetMem;
	    nSosMem += nSetMem;
	}
    }
CLEANUP:
    ILL_RETURN (rval, "mpq_buildSosInfo");
}

static int mpq_convert_rawlpdata_to_lpdata (mpq_rawlpdata * raw,
      mpq_ILLlpdata * lp)
/* only raw's non 'N' rows are converted to matrix entries in lp columns that
   are used in non objective 'N' rows only are not converted. That is they
   don't end up in lp's matrix, row/colnames, upper/lower bounds or SOS
   information. */
{
    int rval = 0;
    int *rowindex = 0;
    int *colindex = 0;

    ILL_FAILfalse ((raw && lp), "rawlpdata_to_lpdata called without input");
    if (raw->name == NULL) {
	mpq_ILLdata_warn (raw->error_collector, "Setting problem name to \"unnamed\".");
	mpq_ILL_UTIL_STR (raw->name, "unnamed");
    }
    rval = mpq_ILLcheck_rawlpdata (raw);
    ILL_CLEANUP_IF (rval);

    ILL_FAILfalse (raw->objindex != -1, "mpq_rawlpdata must have objective fct.");
    mpq_ILLlpdata_init (lp);

    ILL_IFFREE (lp->probname, char);
    lp->probname = raw->name;
    raw->name = 0;

    /* MINIMIZE or MAXIMIZE ? */
    lp->objsense = raw->objsense;
    if (lp->objsense != mpq_ILL_MIN && lp->objsense != mpq_ILL_MAX) {
	mpq_ILLdata_error (raw->error_collector, "Bad objsense.\n");
	rval = 1;
	goto CLEANUP;
    }
    ILL_SAFE_MALLOC (colindex, raw->ncols, int);
    ILL_SAFE_MALLOC (rowindex, raw->nrows, int);
    rval = mpq_whichColsAreUsed (raw, lp, colindex) ||
	mpq_whichRowsAreUsed (raw, lp, rowindex);
    ILL_CLEANUP_IF (rval);
    ILL_FAILtrue (lp->ncols == 0 || lp->nrows == 0, "we need rows and cols");

    /* array sizes */
    lp->rowsize = lp->nrows;
    lp->colsize = lp->ncols;
    lp->nstruct = lp->ncols;
    lp->structsize = lp->ncols;
    ILLsymboltab_create (&lp->rowtab, lp->nrows);
    ILLsymboltab_create (&lp->coltab, lp->ncols);

    rval = mpq_transferObjective (raw, lp, colindex);
    rval = rval || mpq_transferColNamesLowerUpperIntMarker (raw, lp, colindex);
    rval = rval || mpq_buildMatrix (raw, lp, rowindex, colindex);
    rval = rval || mpq_buildSosInfo (raw, lp, colindex);
    ILL_CLEANUP_IF (rval);
    ILL_IFDOTRACE
    {
	mpq_ILLmatrix_prt (stdout, &lp->A);
    }

    rval = mpq_transferSenseRhsRowNames (raw, lp, rowindex);
    if ((lp->nrows > 0) && raw->ranges) {
	rval = rval || mpq_transferRanges (raw, lp, rowindex);
    }
    ILL_CLEANUP_IF (rval);

    rval = mpq_initStructmap (lp);
    ILL_CLEANUP_IF (rval);

CLEANUP:

    ILL_IFFREE (rowindex, int);
    ILL_IFFREE (colindex, int);
    mpq_ILLfree_rawlpdata (raw);

    ILL_RESULT (rval, "mpq_convert_rawlpdata_to_lpdata");
}

int mpq_ILLrawlpdata_to_lpdata (mpq_rawlpdata * raw,
      mpq_ILLlpdata * lp)
{
    int rval = 0;

    ILL_IFDOTRACE
    {
	printf ("%s\n", __func__);
	mpq_ILLprint_rawlpdata (raw);
    }
    rval = mpq_convert_rawlpdata_to_lpdata (raw, lp);
    if (rval == 0) {
	rval = mpq_ILLlp_add_logicals (lp);
    }
    ILL_RESULT (rval, "mpq_ILLrawlpdata_to_lpdata");
}

static int mpq_set_field_name (char **field,
      const char *name,
      int *skip)
{
    int rval = 0;
    /* name is bounds/rhs/rangesname field from mpq_rawlpdata */
    *skip = 0;
    if (!*field) {
	mpq_ILL_UTIL_STR (*field, name);
    }
    if (strcmp (*field, name)) {
	/* not first specified RHS/BOUNDS - skip it */
	*skip = 1;
    }
CLEANUP:
    ILL_RETURN (rval, "mpq_set_field_name");
}

int mpq_ILLraw_set_rhs_name (mpq_rawlpdata * lp,
      const char *name,
      int *skip)
{
    return mpq_set_field_name (&lp->rhsname, name, skip);
}

int mpq_ILLraw_set_bounds_name (mpq_rawlpdata * lp,
      const char *name,
      int *skip)
{
    return mpq_set_field_name (&lp->boundsname, name, skip);
}

int mpq_ILLraw_set_ranges_name (mpq_rawlpdata * lp,
      const char *name,
      int *skip)
{
    return mpq_set_field_name (&lp->rangesname, name, skip);
}

void mpq_ILLprint_rawlpdata (mpq_rawlpdata * lp)
{
    int i, cnt, si, m;
    char c;
    mpq_t d;
    mpq_colptr *cp;
    mpq_sosptr *set;
    mpq_EGlpNumInitVar (d);

    if (lp) {
	if (lp->name) {
	    printf ("PROBLEM  %s\n", lp->name);
	    fflush (stdout);
	}
	if (lp->rowsense && lp->rhs) {
	    printf ("Subject To\n");
	    for (i = 0; i < lp->nrows; i++) {
		switch (lp->rowsense[i]) {
		case 'E':
		    c = '=';
		    break;
		case 'L':
		    c = '<';
		    break;
		case 'G':
		    c = '>';
		    break;
		default:
		    c = '?';
		    break;
		}
		printf ("%s: %c %f\n", mpq_ILLraw_rowname (lp, i), c,
		    mpq_EGlpNumToLf (lp->rhs[i]));
	    }
	    printf ("\n");
	    fflush (stdout);
	}
	if (lp->ncols > 0) {
	    printf ("Columns\n");
	    for (i = 0; i < lp->ncols; i++) {
		for (cp = lp->cols[i]; cp; cp = cp->next) {
		    printf ("%s: ", mpq_ILLraw_rowname (lp, cp->this));
		    printf ("%c ", (mpq_EGlpNumIsLess (cp->coef, mpq_zeroLpNum)) ? '-' : '+');
		    mpq_EGlpNumCopyAbs (d, cp->coef);
		    if (mpq_EGlpNumIsNeqq (d, mpq_oneLpNum)) {
			printf (" %f ", mpq_EGlpNumToLf (d));
		    }
		    printf ("%s\n", mpq_ILLraw_colname (lp, i));
		}
		printf ("\n");
		fflush (stdout);
	    }
	}
	if (lp->rangesname) {
	    printf ("RANGES %s\n", lp->rangesname);
	    for (cp = lp->ranges; cp; cp = cp->next) {
		printf ("(%s, %f) ", mpq_ILLraw_rowname (lp, cp->this),
		    mpq_EGlpNumToLf (cp->coef));
	    }
	    printf ("\n");
	    fflush (stdout);
	}
	if (lp->boundsname) {
	    printf ("BOUNDS %s\n", lp->boundsname);
	    fflush (stdout);
	} else {
	    printf ("BOUNDS \n");
	    fflush (stdout);
	}
	if (lp->lower && lp->upper) {
	    for (i = 0; i < lp->ncols; i++) {
		mpq_ILLprt_EGlpNum (stdout, &(lp->lower[i]));
		printf (" <= %s <= ", mpq_ILLraw_colname (lp, i));
		mpq_ILLprt_EGlpNum (stdout, &(lp->upper[i]));
		printf ("\n");
	    }
	}
	if (lp->intmarker) {
	    printf ("Integer\n");
	    cnt = 0;
	    for (i = 0; i < lp->ncols; i++) {
		if (lp->intmarker[i]) {
		    printf ("%s", mpq_ILLraw_colname (lp, i));
		    cnt++;
		    if (cnt == 8) {
			printf ("\n    ");
			cnt = 0;
		    }
		}
	    }
	    printf ("\n");
	    fflush (stdout);
	}
	printf ("SOS-SETS\n");
	for (si = 0; si < lp->nsos; si++) {
	    set = lp->sos_set + si;
	    printf ("SOS-SET %d: %s; nelem=%d; first=%d;\n",
		si, ((set->type == mpq_ILL_SOS_TYPE1) ? "TYPE1" : "TYPE2"),
		set->nelem, set->first);
	    printf ("\t");
	    for (m = set->first; m < set->first + set->nelem; m++) {
		printf (" %s %f; ", mpq_ILLraw_colname (lp, lp->sos_col[m]),
		    mpq_EGlpNumToLf (lp->sos_weight[m]));
	    }
	    printf ("\n");
	}
	printf ("\n");
	fflush (stdout);
    }
    mpq_EGlpNumClearVar (d);
}

static int mpq_ILLmsg (mpq_qserror_collector * collector,
      int isError,
      const char *format,
      va_list args)
{
    const char *pre;
    int slen, errtype;
    mpq_qsformat_error error;
    char error_desc[256];

    vsprintf (error_desc, format, args);
    slen = strlen (error_desc);
    if ((slen > 0) && error_desc[slen - 1] != '\n') {
	error_desc[slen] = '\n';
	error_desc[slen + 1] = '\0';
    }
    if (collector != NULL) {
	errtype = (isError) ? QS_DATA_ERROR : QS_DATA_WARN;
	mpq_ILLformat_error_create (&error, errtype, error_desc, -1, NULL, -1);
	mpq_ILLformat_error (collector, &error);
	mpq_ILLformat_error_delete (&error);
    } else {
	pre = (isError) ? "Data Error" : "Data Warning";
	fprintf (stderr, "%s: %s", pre, error_desc);
    }
    return 1;
}

int mpq_ILLdata_error (mpq_qserror_collector * collector,
      const char *format,
    ...)
{
    va_list args;
    va_start (args, format);
    return mpq_ILLmsg (collector, mpq_TRUE, format, args);
}

void mpq_ILLdata_warn (mpq_qserror_collector * collector,
      const char *format,
    ...)
{
    va_list args;
    va_start (args, format);
    (void) mpq_ILLmsg (collector, mpq_FALSE, format, args);
}

mpq_colptr *mpq_ILLcolptralloc (ILLptrworld * p)
{
    mpq_colptr *sol = colptralloc (p);
    mpq_EGlpNumInitVar ((sol->coef));
    return sol;
}
