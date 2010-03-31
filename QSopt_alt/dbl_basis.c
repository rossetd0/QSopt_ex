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

/* RCS_INFO = "$RCSfile: dbl_basis.c,v $ $Revision: 1.2 $ $Date: 2003/11/05
   16:49:52 $"; */
static int TRACE = 0;

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "config.h"
#include "dbl_sortrus.h"
#include "dbl_iqsutil.h"
#include "dbl_lpdefs.h"
#include "dbl_qstruct.h"
#include "dbl_qsopt.h"
#include "dbl_basis.h"
#include "dbl_fct.h"
#include "dbl_lp.h"
#include "dbl_lib.h"
#ifdef USEDMALLOC
#include "dmalloc.h"
#endif

#define dbl_DJZERO_TOLER dbl_PFEAS_TOLER
#define dbl_BASIS_STATS 0
#define dbl_BASIS_DEBUG 0

void dbl_ILLbasis_init_vardata (dbl_var_data * vd)
{
    memset (vd, 0, sizeof (dbl_var_data));
    dbl_EGlpNumInitVar (vd->cmax);
}

void dbl_ILLbasis_clear_vardata (dbl_var_data * vd)
{
    dbl_EGlpNumClearVar (vd->cmax);
    memset (vd, 0, sizeof (dbl_var_data));
}

static void dbl_get_var_info (dbl_lpinfo * lp,
      dbl_var_data * v);

static int dbl_init_slack_basis (dbl_lpinfo * lp,
      int *vstat,
      int *irow,
      int *rrow,
      int *unitcol,
      int *icol,
      int *rcol),
  dbl_get_initial_basis1 (dbl_lpinfo * lp,
      int *vstat),
  dbl_get_initial_basis2 (dbl_lpinfo * lp,
      int *vstat),
  dbl_set_basis_indices (dbl_lpinfo * lp,
      int *vstat),
  dbl_choose_basis (int algorithm,
      double pinf1,
      double dinf1,
      double pinf2,
      double dinf2);

void dbl_ILLbasis_init_basisinfo (dbl_lpinfo * lp)
{
    lp->baz = 0;
    lp->nbaz = 0;
    lp->vstat = 0;
    lp->vindex = 0;
    lp->f = 0;
}

void dbl_ILLbasis_free_basisinfo (dbl_lpinfo * lp)
{
    ILL_IFFREE (lp->baz, int);
    ILL_IFFREE (lp->nbaz, int);
    ILL_IFFREE (lp->vstat, int);
    ILL_IFFREE (lp->vindex, int);
    if (lp->f) {
	dbl_ILLfactor_free_factor_work (lp->f);
	dbl_EGlpNumClearVar (lp->f->fzero_tol);
	dbl_EGlpNumClearVar (lp->f->szero_tol);
	dbl_EGlpNumClearVar (lp->f->partial_tol);
	dbl_EGlpNumClearVar (lp->f->maxelem_orig);
	dbl_EGlpNumClearVar (lp->f->maxelem_factor);
	dbl_EGlpNumClearVar (lp->f->maxelem_cur);
	dbl_EGlpNumClearVar (lp->f->partial_cur);
	ILL_IFFREE (lp->f, dbl_factor_work);
    }
}

int dbl_ILLbasis_build_basisinfo (dbl_lpinfo * lp)
{
    int rval = 0;

    ILL_SAFE_MALLOC (lp->baz, lp->nrows, int);
    ILL_SAFE_MALLOC (lp->nbaz, lp->nnbasic, int);
    ILL_SAFE_MALLOC (lp->vstat, lp->ncols, int);
    ILL_SAFE_MALLOC (lp->vindex, lp->ncols, int);

    lp->fbasisid = -1;

CLEANUP:
    if (rval)
	dbl_ILLbasis_free_basisinfo (lp);
    ILL_RETURN (rval, "dbl_ILLbasis_build_basisinfo");
}

int dbl_ILLbasis_load (dbl_lpinfo * lp,
      dbl_ILLlp_basis * B)
{
    int rval = 0;
    char *cstat = B->cstat;
    char *rstat = B->rstat;
    int *structmap = lp->O->structmap;
    int *rowmap = lp->O->rowmap;
    double *rng = lp->O->rangeval;
    int i, j, ncols = lp->ncols, nrows = lp->nrows, nstruct = lp->O->nstruct;
    int basic = 0, nonbasic = 0;

    dbl_ILLbasis_free_basisinfo (lp);
    dbl_ILLbasis_init_basisinfo (lp);
    rval = dbl_ILLbasis_build_basisinfo (lp);
    ILL_CLEANUP_IF (rval);

    for (i = 0; i < nstruct; i++) {
	j = structmap[i];
	if (cstat[i] == QS_COL_BSTAT_BASIC) {
	    lp->vstat[j] = STAT_BASIC;
	    lp->baz[basic] = j;
	    lp->vindex[j] = basic;
	    basic++;
	} else {
	    lp->nbaz[nonbasic] = j;
	    lp->vindex[j] = nonbasic;
	    nonbasic++;
	    switch (cstat[i]) {
	    case QS_COL_BSTAT_LOWER:
		lp->vstat[j] = STAT_LOWER;
		break;
	    case QS_COL_BSTAT_UPPER:
		lp->vstat[j] = STAT_UPPER;
		break;
	    case QS_COL_BSTAT_FREE:
		lp->vstat[j] = STAT_ZERO;
		break;
	    default:
		fprintf (stderr, "unknown col basis stat 1: %c\n", cstat[i]);
		rval = 1;
		goto CLEANUP;
	    }
	}
    }

    for (i = 0; i < nrows; i++) {
	j = rowmap[i];
	if (rng && (dbl_EGlpNumIsNeqqZero (rng[i]))) {
	    if (rstat[i] == QS_ROW_BSTAT_BASIC) {
		lp->vstat[j] = STAT_BASIC;
		lp->baz[basic] = j;
		lp->vindex[j] = basic;
		basic++;
	    } else {
		lp->nbaz[nonbasic] = j;
		lp->vindex[j] = nonbasic;
		nonbasic++;
		switch (rstat[i]) {
		case QS_ROW_BSTAT_LOWER:
		    lp->vstat[j] = STAT_LOWER;
		    break;
		case QS_ROW_BSTAT_UPPER:
		    lp->vstat[j] = STAT_UPPER;
		    break;
		default:
		    fprintf (stderr, "unknown range basis stat 2\n");
		    rval = 1;
		    goto CLEANUP;
		}
	    }
	} else {
	    switch (rstat[i]) {
	    case QS_ROW_BSTAT_BASIC:
		lp->vstat[j] = STAT_BASIC;
		lp->baz[basic] = j;
		lp->vindex[j] = basic;
		basic++;
		break;
	    case QS_ROW_BSTAT_LOWER:
		lp->vstat[j] = STAT_LOWER;
		lp->nbaz[nonbasic] = j;
		lp->vindex[j] = nonbasic;
		nonbasic++;
		break;
	    default:
		fprintf (stderr, "unknown row basis stat 3\n");
		rval = 1;
		goto CLEANUP;
	    }
	}
    }

    if (basic + nonbasic != ncols) {
	fprintf (stderr, "error in counts in ILLopt_load_basis\n");
	rval = 1;
	goto CLEANUP;
    }
    if (lp->fbasisid != 0)
	lp->basisid = 0;
    else
	lp->basisid = 1;

CLEANUP:

    ILL_RETURN (rval, "dbl_ILLbasis_load");
}

int dbl_ILLbasis_tableau_row (dbl_lpinfo * lp,
      int row,
      double *brow,
      double *trow,
      double *rhs,
      int strict)
{
    int rval = 0;
    int i;
    int singular = 0;
    int indx;
    double coef;
    double sum;
    dbl_svector z, zA;
    dbl_EGlpNumInitVar (coef);
    dbl_EGlpNumInitVar (sum);
    dbl_EGlpNumZero (sum);

    dbl_ILLsvector_init (&z);
    dbl_ILLsvector_init (&zA);

    if (lp->basisid == -1) {
	fprintf (stderr, "dbl_ILLbasis_tableau_row: no basis\n");
	rval = E_GENERAL_ERROR;
	ILL_CLEANUP;
    }
    if (lp->fbasisid != lp->basisid) {	/* Needs to be changed */
	rval = dbl_ILLbasis_factor (lp, &singular);
	ILL_CLEANUP_IF (rval);
	if (singular) {
	    rval = E_BASIS_SINGULAR;
	    ILL_CLEANUP;
	}
    }
    if (brow == NULL) {
	fprintf (stderr, "No array for basis inverse row\n");
	rval = E_GENERAL_ERROR;
	ILL_CLEANUP;
    }
    rval = dbl_ILLsvector_alloc (&z, lp->nrows);
    ILL_CLEANUP_IF (rval);
    dbl_ILLfct_compute_zz (lp, &z, row);

    for (i = 0; i < lp->nrows; i++)
	dbl_EGlpNumZero (brow[i]);
    for (i = 0; i < z.nzcnt; i++) {
	indx = z.indx[i];
	dbl_EGlpNumCopy (coef, z.coef[i]);
	dbl_EGlpNumCopy (brow[indx], coef);
	dbl_EGlpNumAddInnProdTo (sum, coef, lp->bz[indx]);
    }

    if (rhs != NULL)
	dbl_EGlpNumCopy (*rhs, sum);
    if (trow != NULL) {
	if (!strict) {
	    rval = dbl_ILLsvector_alloc (&zA, lp->ncols);
	    if (rval)
		ILL_CLEANUP;
	    ILL_IFTRACE ("%s:\n", __func__);
	    rval = dbl_ILLfct_compute_zA (lp, &z, &zA);
	    ILL_CLEANUP_IF (rval);

	    for (i = 0; i < lp->ncols; i++)
		dbl_EGlpNumZero (trow[i]);
	    for (i = 0; i < zA.nzcnt; i++)
		dbl_EGlpNumCopy (trow[lp->nbaz[zA.indx[i]]], zA.coef[i]);
	    dbl_EGlpNumOne (trow[lp->baz[row]]);
	} else {
	    dbl_ILLfct_compute_vA (lp, &z, trow);
	}
    }
#if dbl_BASIS_DEBUG > 0
    if (rhs != NULL && trow != NULL) {
	double *tr = NULL;
	dbl_EGlpNumZero (sum);
	if (strict)
	    tr = trow;
	else {
	    tr = dbl_EGlpNumAllocArray (lp->ncols);
	    dbl_ILLfct_compute_vA (lp, &z, tr);
	}
	for (i = 0; i < lp->nrows; i++)
	    if (dbl_EGlpNumIsLess (dbl_zeroLpNum, tr[lp->baz[i]]))
		dbl_EGlpNumAddTo (sum, tr[lp->baz[i]]);
	    else
		dbl_EGlpNumSubTo (sum, tr[lp->baz[i]]);
	dbl_EGlpNumCopy (coef, dbl_oneLpNum);
	dbl_EGlpNumSubTo (coef, sum);
	if (dbl_EGlpNumIsLess (coef, dbl_zeroLpNum))
	    dbl_EGlpNumSign (coef);
	if (dbl_EGlpNumIsLess (dbl_PIVZ_TOLER, coef))
	    fprintf (stderr, "tableau: bas computed = %.12f\n", dbl_EGlpNumToLf (sum));
	if (!strict)
	    dbl_EGlpNumFreeArray (tr);
#if dbl_BASIS_DEBUG > 1
	dbl_EGlpNumZero (sum);
	for (i = 0; i < lp->ncols; i++) {
	    if (lp->vstat[i] == STAT_BASIC)
		dbl_EGlpNumAddInnProdTo (sum, lp->xbz[lp->vindex[i]], trow[i]);
	    else if (lp->vstat[i] == STAT_UPPER)
		dbl_EGlpNumAddInnProdTo (sum, lp->uz[i], trow[i]);
	    else if (lp->vstat[i] == STAT_LOWER)
		dbl_EGlpNumAddInnProdTo (sum, lp->lz[i], trow[i]);
	}
	dbl_EGlpNumSet (coef, 1e-10);
	if (dbl_EGlpNumIsNeq (sum, *rhs, coef))
	    fprintf (stderr, "tableau rhs = %.9f, computed = %.9f\n",
		dbl_EGlpNumToLf (*rhs), dbl_EGlpNumToLf (sum));
#endif
    }
#endif

CLEANUP:
    dbl_ILLsvector_free (&z);
    dbl_ILLsvector_free (&zA);
    dbl_EGlpNumClearVar (coef);
    dbl_EGlpNumClearVar (sum);
    return rval;
}

static void dbl_get_var_info (dbl_lpinfo * lp,
      dbl_var_data * v)
{
    int i = 0;

    v->nartif = 0;
    v->nslacks = 0;
    v->nfree = 0;
    v->nbndone = 0;
    v->nbounded = 0;
    v->nfixed = 0;
    dbl_EGlpNumCopy (v->cmax, dbl_NINFTY);

    for (i = 0; i < lp->ncols; i++) {
	switch (lp->vtype[i]) {
	case VARTIFICIAL:
	    v->nartif++;
	    break;
	case VFREE:
	    v->nfree++;
	    break;
	case VLOWER:
	case VUPPER:
	    if (lp->vclass[i] == CLASS_LOGICAL)
		v->nslacks++;
	    else
		v->nbndone++;
	    break;

	case VFIXED:
	    v->nfixed++;
	case VBOUNDED:
	    if (lp->vclass[i] == CLASS_LOGICAL)
		v->nslacks++;
	    else
		v->nbounded++;
	    break;
	}
	dbl_EGlpNumSetToMaxAbs (v->cmax, lp->cz[i]);
    }

#if dbl_BASIS_STATS > 0
    printf ("cols = %d, acols = %d, total  = %d, nrows = %d, nlog = %d\n",
	lp->ncols, lp->ncols - lp->nrows,
	v->nartif + v->nfree + v->nslacks + v->nbndone + v->nbounded,
	lp->nrows, v->nartif + v->nslacks);
#endif
}

static int dbl_init_slack_basis (dbl_lpinfo * lp,
      int *vstat,
      int *irow,
      int *rrow,
      int *unitcol,
      int *icol,
      int *rcol)
{
    int j, r, vt;
    int nslacks = 0;

    for (j = 0; j < lp->ncols; j++) {
	r = lp->matind[lp->matbeg[j]];
	vt = lp->vtype[j];

	if ((vt == VUPPER || vt == VLOWER || vt == VBOUNDED || vt == VFIXED) &&
	    lp->vclass[j] == CLASS_LOGICAL) {

	    vstat[j] = STAT_BASIC;
	    irow[r] = 1;
	    rrow[r] = 1;
	    unitcol[r] = j;
	    if (icol != NULL) {
		icol[j] = 1;
		rcol[j] = 1;
	    }
	    nslacks++;
	} else if (vt == VARTIFICIAL) {
	    unitcol[r] = j;
	    vstat[j] = STAT_UPPER;
	} else if (vt == VFREE)
	    vstat[j] = STAT_ZERO;
	else if (vt == VFIXED || vt == VUPPER)
	    vstat[j] = STAT_UPPER;
	else if (vt == VLOWER)
	    vstat[j] = STAT_LOWER;
	else if (vt == VBOUNDED) {
	    if (fabs (dbl_EGlpNumToLf (lp->lz[j])) < fabs (dbl_EGlpNumToLf (lp->uz[j])))
		vstat[j] = STAT_LOWER;
	    else
		vstat[j] = STAT_UPPER;
	}
    }
    return nslacks;
}

static int dbl_primal_col_select (dbl_lpinfo * lp,
      int *vstat,
      int *irow,
      int *rrow,
      int *unitcol,
      double *v,
      int *perm,
      int *porder,
      int nbelem,
      int pcols)
{
    int i, j, k, tr, r = 0;
    int mcnt, mbeg;
    int *matbeg = lp->matbeg;
    int *matcnt = lp->matcnt;
    int *matind = lp->matind;
    double *matval = lp->matval;
    double alpha, val, maxelem;
    dbl_EGlpNumInitVar (alpha);
    dbl_EGlpNumInitVar (val);
    dbl_EGlpNumInitVar (maxelem);

    for (k = 0; k < pcols; k++) {
	j = porder[perm[k]];
	mcnt = matcnt[j];
	mbeg = matbeg[j];

	dbl_EGlpNumCopy (alpha, dbl_NINFTY);
	dbl_EGlpNumCopy (maxelem, dbl_NINFTY);

	for (i = 0; i < mcnt; i++) {
	    dbl_EGlpNumCopyAbs (val, matval[mbeg + i]);
	    if (dbl_EGlpNumIsLess (maxelem, val))
		dbl_EGlpNumCopy (maxelem, val);
	    if (rrow[matind[mbeg + i]] == 0 && dbl_EGlpNumIsLess (alpha, val)) {
		dbl_EGlpNumCopy (alpha, val);
		r = matind[mbeg + i];
	    }
	}
	dbl_EGlpNumCopy (val, maxelem);
	dbl_EGlpNumMultTo (val, dbl_PARAM_IBASIS_RPIVOT);
	if (dbl_EGlpNumIsLess (val, alpha)) {
	    vstat[j] = STAT_BASIC;
	    nbelem++;
	    irow[r] = 1;
	    dbl_EGlpNumCopy (v[r], alpha);
	    for (i = 0; i < mcnt; i++)
		if (dbl_EGlpNumIsNeqqZero (matval[mbeg + i]))
		    rrow[matind[mbeg + i]]++;
	} else {
	    dbl_EGlpNumCopy (alpha, dbl_NINFTY);
	    for (i = 0; i < mcnt; i++) {
		tr = matind[mbeg + i];
		dbl_EGlpNumCopyAbs (val, matval[mbeg + i]);
		dbl_EGlpNumDivTo (val, dbl_PARAM_IBASIS_RTRIANG);
		if (dbl_EGlpNumIsNeqq (v[tr], dbl_INFTY) && dbl_EGlpNumIsLess (v[tr], val)) {
		    dbl_EGlpNumZero (alpha);
		    break;
		}
		dbl_EGlpNumCopyAbs (val, matval[mbeg + i]);
		if (irow[tr] == 0 && dbl_EGlpNumIsLess (alpha, val)) {
		    dbl_EGlpNumCopy (alpha, val);
		    r = tr;
		}
	    }
	    if (dbl_EGlpNumIsNeqqZero (alpha) && dbl_EGlpNumIsNeqq (alpha, dbl_NINFTY)) {
		vstat[j] = STAT_BASIC;
		nbelem++;
		irow[r] = 1;
		dbl_EGlpNumCopy (v[r], alpha);
		for (i = 0; i < mcnt; i++)
		    if (dbl_EGlpNumIsNeqqZero (matval[mbeg + i]))
			rrow[matind[mbeg + i]]++;
	    }
	}
    }
#if dbl_BASIS_STATS > 0
    printf ("nartifs = %d\n", lp->nrows - nbelem);
#endif

    if (nbelem < lp->nrows) {
	for (i = 0; i < lp->nrows; i++) {
	    if (irow[i] == 0) {
		if (unitcol[i] != -1) {
		    vstat[unitcol[i]] = STAT_BASIC;
		    nbelem++;
		} else {
		    fprintf (stderr, "Error: Not enough artificials\n");
		    return -1;
		}
	    }
	}
    }
    dbl_EGlpNumClearVar (alpha);
    dbl_EGlpNumClearVar (val);
    dbl_EGlpNumClearVar (maxelem);
    return nbelem;
}

/* This is an implementation of the initial basis procedure in: "Implementing
   the simplex method: the initial basis", by Bob Bixby. Goals: choose
   initial variables to go into basis which satisfy: 1) vars are slacks, 2)
   vars have freedom to move 3) initial submatrix is nonsingular, 4) low
   objective function contribution. */
static int dbl_get_initial_basis1 (dbl_lpinfo * lp,
      int *vstat)
{
    int rval = 0;
    int i, j, tot1 = 0, tot2 = 0;
    int nbelem = 0, nslacks = 0;
    int tfree = 0, tbndone = 0;
    int tbounded = 0;
    int *irow = NULL, *rrow = NULL;
    int *perm = NULL, *porder = NULL;
    int *unitcol = NULL;
    double cmax;
    double *v = NULL;
    double *qpenalty = NULL;
    dbl_var_data vd;
    dbl_ILLbasis_init_vardata (&vd);
    dbl_EGlpNumInitVar (cmax);

    dbl_get_var_info (lp, &vd);
    if (dbl_EGlpNumIsEqqual (vd.cmax, dbl_zeroLpNum))
	dbl_EGlpNumOne (cmax);
    else {
	dbl_EGlpNumCopy (cmax, vd.cmax);
	dbl_EGlpNumMultUiTo (cmax, 1000);
    }

    ILL_SAFE_MALLOC (irow, lp->nrows, int);
    ILL_SAFE_MALLOC (rrow, lp->nrows, int);
    v = dbl_EGlpNumAllocArray (lp->nrows);
    ILL_SAFE_MALLOC (unitcol, lp->nrows, int);

    for (i = 0; i < lp->nrows; i++) {
	unitcol[i] = -1;
	dbl_EGlpNumCopy (v[i], dbl_INFTY);
	irow[i] = 0;
	rrow[i] = 0;
    }

    nslacks = dbl_init_slack_basis (lp, vstat, irow, rrow, unitcol, NULL, NULL);
    if (nslacks != vd.nslacks) {
	printf ("complain: incorrect basis info(slacks)\n");
	rval = E_SIMPLEX_ERROR;
	ILL_CLEANUP;
    }
    if (nslacks == lp->nrows)
	ILL_CLEANUP;
    nbelem = nslacks;
    if (nbelem < lp->nrows) {
	for (i = 0; i < lp->nrows; i++) {
	    if (irow[i] == 0) {
		if (unitcol[i] != -1) {
		    vstat[unitcol[i]] = STAT_BASIC;
		    nbelem++;
		} else {
		    fprintf (stderr, "Error: Not enough artificials\n");
		    return -1;
		}
	    }
	}
    }
    ILL_CLEANUP;

    tot1 = vd.nfree + vd.nbndone;
    tot2 = vd.nfree + vd.nbndone + vd.nbounded;
    ILL_SAFE_MALLOC (perm, tot2, int);
    ILL_SAFE_MALLOC (porder, tot2, int);
    qpenalty = dbl_EGlpNumAllocArray (tot2);

    for (j = 0; j < lp->ncols; j++) {
	if (vstat[j] == STAT_BASIC)
	    continue;

	switch (lp->vtype[j]) {
	case VFREE:
	    porder[tfree] = j;
	    perm[tfree] = tfree;
	    dbl_EGlpNumCopyFrac (qpenalty[tfree], lp->cz[j], cmax);
	    tfree++;
	    break;

	case VLOWER:
	case VUPPER:
	    porder[vd.nfree + tbndone] = j;
	    perm[vd.nfree + tbndone] = tbndone;
	    dbl_EGlpNumCopyFrac (qpenalty[vd.nfree + tbndone], lp->cz[j], cmax);
	    if (lp->vtype[j] == VLOWER)
		dbl_EGlpNumAddTo (qpenalty[vd.nfree + tbndone], lp->lz[j]);
	    else
		dbl_EGlpNumSubTo (qpenalty[vd.nfree + tbndone], lp->uz[j]);
	    tbndone++;
	    break;

	case VFIXED:
	case VBOUNDED:
	    porder[tot1 + tbounded] = j;
	    perm[tot1 + tbounded] = tbounded;
	    dbl_EGlpNumCopyFrac (qpenalty[tot1 + tbndone], lp->cz[j], cmax);
	    dbl_EGlpNumAddTo (qpenalty[tot1 + tbndone], lp->lz[j]);
	    dbl_EGlpNumSubTo (qpenalty[tot1 + tbndone], lp->uz[j]);
	    tbounded++;
	    break;
	}
    }
    if (tfree != vd.nfree || tbndone != vd.nbndone || tbounded != vd.nbounded) {
	printf ("complain: incorrect basis info \n");
	rval = E_SIMPLEX_ERROR;
	ILL_CLEANUP;
    }
    dbl_ILLutil_EGlpNum_perm_quicksort (perm, qpenalty, vd.nfree);
    dbl_ILLutil_EGlpNum_perm_quicksort (perm + vd.nfree, qpenalty + vd.nfree,
	vd.nbndone);
    dbl_ILLutil_EGlpNum_perm_quicksort (perm + tot1, qpenalty + tot1, vd.nbounded);

    for (i = 0; i < vd.nbndone; i++)
	perm[vd.nfree + i] += vd.nfree;
    for (i = 0; i < vd.nbounded; i++)
	perm[tot1 + i] += tot1;

    nbelem =
	dbl_primal_col_select (lp, vstat, irow, rrow, unitcol, v, perm, porder, nbelem,
	tot2);
    if (nbelem != lp->nrows) {
	printf ("complain: incorrect final basis size\n");
	rval = E_SIMPLEX_ERROR;
	ILL_CLEANUP;
    }
CLEANUP:
    dbl_EGlpNumClearVar (cmax);
    if (rval)
	dbl_ILLbasis_free_basisinfo (lp);
    ILL_IFFREE (irow, int);
    ILL_IFFREE (rrow, int);
    dbl_EGlpNumFreeArray (v);
    ILL_IFFREE (perm, int);
    ILL_IFFREE (porder, int);
    ILL_IFFREE (unitcol, int);
    dbl_EGlpNumFreeArray (qpenalty);
    dbl_ILLbasis_clear_vardata (&vd);
    ILL_RETURN (rval, "dbl_ILLbasis_get_initial");
}

static int dbl_get_initial_basis2 (dbl_lpinfo * lp,
      int *vstat)
{
    int rval = 0;
    int i, j, k, tot1, tot2;
    int rbeg, rcnt, mcnt;
    int nbelem = 0, nslacks = 0;
    int tfree = 0, tbndone = 0;
    int tbounded = 0;
    int *irow = NULL, *rrow = NULL;
    int *perm = NULL, *porder = NULL;
    int *unitcol = NULL;
    double *v = NULL;
    double *qpenalty = NULL;
    int col = 0, s_i = 0, selc = 0;
    int *icol = NULL, *rcol = NULL;
    int *plen = NULL;
    double *dj = NULL;
    dbl_var_data vd;
    double seldj;
    double selv;
    double c_dj;
    double cmax;
    dbl_EGlpNumInitVar (seldj);
    dbl_EGlpNumInitVar (selv);
    dbl_EGlpNumInitVar (c_dj);
    dbl_EGlpNumInitVar (cmax);
    dbl_EGlpNumZero (c_dj);
    dbl_EGlpNumZero (selv);
    dbl_EGlpNumZero (seldj);
    dbl_ILLbasis_init_vardata (&vd);

    dbl_get_var_info (lp, &vd);

    ILL_SAFE_MALLOC (irow, lp->nrows, int);
    ILL_SAFE_MALLOC (rrow, lp->nrows, int);
    v = dbl_EGlpNumAllocArray (lp->nrows);
    ILL_SAFE_MALLOC (unitcol, lp->nrows, int);
    ILL_SAFE_MALLOC (icol, lp->ncols, int);
    ILL_SAFE_MALLOC (rcol, lp->ncols, int);
    dj = dbl_EGlpNumAllocArray (lp->ncols);

    for (i = 0; i < lp->nrows; i++) {
	unitcol[i] = -1;
	dbl_EGlpNumCopy (v[i], dbl_INFTY);
	irow[i] = 0;
	rrow[i] = 0;
    }
    /* assign all d_j */
    for (i = 0; i < lp->ncols; i++) {
	icol[i] = 0;
	rcol[i] = 0;
	dbl_EGlpNumCopy (dj[i], lp->cz[i]);
    }

    nslacks = dbl_init_slack_basis (lp, vstat, irow, rrow, unitcol, icol, rcol);
    if (nslacks != vd.nslacks) {
	printf ("complain: incorrect basis info\n");
	rval = E_SIMPLEX_ERROR;
	ILL_CLEANUP;
    }
    if (nslacks == lp->nrows)
	ILL_CLEANUP;
    nbelem = nslacks;

    /* allocate maximum required space for perm etc. */
    ILL_SAFE_MALLOC (perm, lp->ncols, int);
    ILL_SAFE_MALLOC (porder, lp->ncols, int);
    ILL_SAFE_MALLOC (plen, lp->nrows, int);
    qpenalty = dbl_EGlpNumAllocArray (lp->ncols);

    /* find all unit rows and record lengths */
    for (i = 0; i < lp->nrows; i++) {
	if (irow[i] != 1) {
	    rbeg = lp->rowbeg[i];
	    rcnt = lp->rowcnt[i];
	    for (j = 0; j < rcnt; j++) {
		dbl_EGlpNumCopyAbs (cmax, lp->rowval[rbeg + j]);
		if (dbl_EGlpNumIsNeqq (cmax, dbl_oneLpNum))
		    break;
	    }
	    if (j == rcnt) {
		perm[s_i] = s_i;
		porder[s_i] = i;
		plen[s_i] = rcnt;
		s_i++;
	    }
	}
    }

    /* sort all unit rows */
    dbl_ILLutil_int_perm_quicksort (perm, plen, s_i);

    /* now go through the unit rows */
    for (k = 0; k < s_i; k++) {
	i = porder[perm[k]];
	rbeg = lp->rowbeg[i];
	rcnt = lp->rowcnt[i];
	selc = -1;
	dbl_EGlpNumCopy (seldj, dbl_INFTY);
	dbl_EGlpNumZero (selv);

	/* for every row s_i, compute min {d_j : d_j <0 , j is u or l or fr} */
	for (j = 0; j < rcnt; j++) {
	    col = lp->rowind[rbeg + j];
	    if (rcol[col] == 1)
		break;
	    if (dbl_EGlpNumIsLess (dj[col], dbl_zeroLpNum)) {
		if (dbl_EGlpNumIsLess (dj[col], seldj)) {
		    selc = col;
		    dbl_EGlpNumCopy (seldj, dj[col]);
		    dbl_EGlpNumCopy (selv, lp->rowval[rbeg + j]);
		}
	    }
	}
	/* select pivot element and update all d_j's */
	if (selc != -1) {
	    nbelem++;
	    irow[i] = 1;
	    rrow[i] = 1;
	    icol[selc] = 1;
	    dbl_EGlpNumCopyFrac (c_dj, dj[selc], selv);
	    vstat[selc] = STAT_BASIC;
	    for (j = 0; j < rcnt; j++) {
		col = lp->rowind[rbeg + j];
		dbl_EGlpNumSubInnProdTo (dj[col], lp->rowval[rbeg + j], c_dj);
		rcol[col] = 1;
	    }
	}
    }
#if dbl_BASIS_STATS > 0
    printf ("unit rows = %d\n", s_i);
    printf ("nslacks %d, unit rows selected = %d\n", nslacks, nbelem - nslacks);
#endif
    /* now go through remaining cols with dj = 0 */
    tot1 = vd.nfree + vd.nbndone;

    if (dbl_EGlpNumIsEqqual (vd.cmax, dbl_zeroLpNum))
	dbl_EGlpNumOne (cmax);
    else {
	dbl_EGlpNumCopy (cmax, vd.cmax);
	dbl_EGlpNumMultUiTo (cmax, 1000);
    }
    for (j = 0; j < lp->ncols; j++) {
	if (vstat[j] == STAT_BASIC)
	    continue;
	if (icol[j] == 1 || dbl_EGlpNumIsNeqZero (dj[j], dbl_BD_TOLER))
	    continue;
	mcnt = lp->matcnt[j];

	dbl_EGlpNumSet (c_dj, (double) mcnt);
	switch (lp->vtype[j]) {
	case VFREE:
	    porder[tfree] = j;
	    perm[tfree] = tfree;
	    dbl_EGlpNumCopyFrac (qpenalty[tfree], lp->cz[j], cmax);
	    dbl_EGlpNumAddTo (qpenalty[tfree], c_dj);
	    tfree++;
	    break;

	case VLOWER:
	case VUPPER:
	    porder[vd.nfree + tbndone] = j;
	    perm[vd.nfree + tbndone] = tbndone;
	    dbl_EGlpNumCopyFrac (qpenalty[vd.nfree + tbndone], lp->cz[j], cmax);
	    dbl_EGlpNumAddTo (qpenalty[vd.nfree + tbndone], c_dj);
	    if (lp->vtype[j] == VLOWER)
		dbl_EGlpNumAddTo (qpenalty[vd.nfree + tbndone], lp->lz[j]);
	    else
		dbl_EGlpNumSubTo (qpenalty[vd.nfree + tbndone], lp->uz[j]);
	    tbndone++;
	    break;

	case VFIXED:
	case VBOUNDED:
	    porder[tot1 + tbounded] = j;
	    perm[tot1 + tbounded] = tbounded;
	    dbl_EGlpNumCopyFrac (qpenalty[tot1 + tbounded], lp->cz[j], cmax);
	    dbl_EGlpNumAddTo (qpenalty[tot1 + tbounded], lp->lz[j]);
	    dbl_EGlpNumSubTo (qpenalty[tot1 + tbounded], lp->uz[j]);
	    dbl_EGlpNumAddTo (qpenalty[tot1 + tbounded], c_dj);
	    tbounded++;
	    break;
	}
    }
#if dbl_BASIS_STATS > 0
    printf ("bfree %d, bone %d, bbnd %d\n", tfree, tbndone, tbounded);
#endif

    dbl_ILLutil_EGlpNum_perm_quicksort (perm, qpenalty, tfree);
    dbl_ILLutil_EGlpNum_perm_quicksort (perm + vd.nfree, qpenalty + vd.nfree,
	tbndone);
    dbl_ILLutil_EGlpNum_perm_quicksort (perm + tot1, qpenalty + tot1, tbounded);

    tot2 = tfree + tbndone;
    for (i = 0; i < tbndone; i++) {
	perm[tfree + i] = perm[vd.nfree + i] + tfree;
	porder[tfree + i] = porder[vd.nfree + i];
    }
    for (i = 0; i < tbounded; i++) {
	perm[tot2 + i] = perm[tot1 + i] + tot2;
	porder[tot2 + i] = porder[tot1 + i];
    }
    tot2 += tbounded;

    nbelem =
	dbl_primal_col_select (lp, vstat, irow, rrow, unitcol, v, perm, porder, nbelem,
	tot2);
    if (nbelem != lp->nrows) {
	printf ("complain: incorrect final basis size\n");
	rval = E_SIMPLEX_ERROR;
	ILL_CLEANUP;
    }
CLEANUP:
    if (rval)
	dbl_ILLbasis_free_basisinfo (lp);

    ILL_IFFREE (irow, int);
    ILL_IFFREE (rrow, int);
    dbl_EGlpNumFreeArray (v);
    ILL_IFFREE (unitcol, int);
    ILL_IFFREE (icol, int);
    ILL_IFFREE (rcol, int);
    dbl_EGlpNumFreeArray (dj);
    ILL_IFFREE (perm, int);
    ILL_IFFREE (porder, int);
    ILL_IFFREE (plen, int);
    dbl_EGlpNumFreeArray (qpenalty);
    dbl_EGlpNumClearVar (seldj);
    dbl_EGlpNumClearVar (selv);
    dbl_EGlpNumClearVar (c_dj);
    dbl_EGlpNumClearVar (cmax);
    dbl_ILLbasis_clear_vardata (&vd);
    ILL_RETURN (rval, "dbl_ILLbasis_get_initial");
}

static int dbl_set_basis_indices (dbl_lpinfo * lp,
      int *vstat)
{
    int i, b = 0, nb = 0;
    int vs;

    for (i = 0; i < lp->ncols; i++) {
	vs = vstat[i];
	lp->vstat[i] = vs;

	if (vs == STAT_BASIC) {
	    lp->baz[b] = i;
	    lp->vindex[i] = b;
	    b++;
	} else if (vs == STAT_UPPER || vs == STAT_LOWER || vs == STAT_ZERO) {
	    lp->nbaz[nb] = i;
	    lp->vindex[i] = nb;
	    nb++;
	} else {
	    fprintf (stderr, "Error in basis creation\n");
	    return E_SIMPLEX_ERROR;
	}
    }
    if (b != lp->nrows) {
	fprintf (stderr, "Error 2 in basis creation\n");
	return E_SIMPLEX_ERROR;
    } else if (nb != lp->nnbasic) {
	fprintf (stderr, "Error 3 in basis creation\n");
	return E_SIMPLEX_ERROR;
    }
    return 0;
}

int dbl_ILLbasis_get_initial (dbl_lpinfo * lp,
      int algorithm)
{
    int rval = 0;
    int *vstat = NULL;

    dbl_ILLbasis_free_basisinfo (lp);
    dbl_ILLbasis_init_basisinfo (lp);
    rval = dbl_ILLbasis_build_basisinfo (lp);
    ILL_CLEANUP_IF (rval);

    ILL_SAFE_MALLOC (vstat, lp->ncols, int);

    if (algorithm == PRIMAL_SIMPLEX)
	rval = dbl_get_initial_basis1 (lp, vstat);
    else
	rval = dbl_get_initial_basis2 (lp, vstat);

    if (rval == E_SIMPLEX_ERROR) {
	FILE *f = fopen ("bad.lp", "w");
	int tval = dbl_ILLwrite_lp_file (lp->O, f, NULL);
	if (tval) {
	    fprintf (stderr, "Error writing bad lp\n");
	}
	if (f != NULL)
	    fclose (f);
    }
    ILL_CLEANUP_IF (rval);

    rval = dbl_set_basis_indices (lp, vstat);
    lp->basisid = 0;

CLEANUP:
    ILL_IFFREE (vstat, int);
    ILL_RETURN (rval, "dbl_ILLbasis_get_initial");
}

static int dbl_choose_basis (int algorithm,
      double pinf1,
      double dinf1,
      double pinf2,
      double dinf2)
{
    /* We changed the constant definitions outside here, the actual numbers
       are asigned in dbl_lpdata.c. the values are as follows: dbl_CB_EPS =
       0.001; dbl_CB_PRI_RLIMIT = 0.25; dbl_CB_INF_RATIO = 10.0; */
    int choice = 1;
    double rp, rd;
    if (algorithm == PRIMAL_SIMPLEX) {
	dbl_EGlpNumInitVar (rp);
	dbl_EGlpNumInitVar (rd);
	dbl_EGlpNumCopyDiff (rp, pinf1, pinf2);
	dbl_EGlpNumCopyDiff (rd, dinf1, dinf2);
	if (dbl_EGlpNumIsLeq (rp, dbl_CB_EPS) && dbl_EGlpNumIsLeq (rd, dbl_CB_EPS))
	    choice = 1;
	else {
	    dbl_EGlpNumSign (rp);
	    dbl_EGlpNumSign (rd);
	    if (dbl_EGlpNumIsLeq (rp, dbl_CB_EPS) && dbl_EGlpNumIsLeq (rd, dbl_CB_EPS))
		choice = 2;
	    else if (dbl_EGlpNumIsLess (pinf1, pinf2) && dbl_EGlpNumIsLess (dinf2, dinf1)) {
		choice = 1;
		dbl_EGlpNumCopyFrac (rp, pinf1, pinf2);
		dbl_EGlpNumCopyFrac (rd, dinf2, dinf1);
		dbl_EGlpNumMultTo (rd, dbl_CB_INF_RATIO);
		if (dbl_EGlpNumIsLess (dbl_CB_PRI_RLIMIT, rp) && (dbl_EGlpNumIsLess (rd, rp)))
		    choice = 2;
	    } else if (dbl_EGlpNumIsLess (pinf2, pinf1) && dbl_EGlpNumIsLess (dinf1, dinf2)) {
		choice = 2;
		dbl_EGlpNumCopyFrac (rp, pinf2, pinf1);
		dbl_EGlpNumCopyFrac (rd, dinf1, dinf2);
		dbl_EGlpNumMultTo (rd, dbl_CB_INF_RATIO);
		if (dbl_EGlpNumIsLess (dbl_CB_PRI_RLIMIT, rp) && dbl_EGlpNumIsLess (rd, rp))
		    choice = 1;
	    } else
		choice = 1;
	}
	dbl_EGlpNumClearVar (rp);
	dbl_EGlpNumClearVar (rd);
    }
    ILL_IFTRACE ("%s:%d\n", __func__, choice);
    return choice;
}

int dbl_ILLbasis_get_cinitial (dbl_lpinfo * lp,
      int algorithm)
{
    int rval = 0;
    int *vstat1 = NULL;
    int *vstat2 = NULL;
    int singular;
    int choice = 0;
#if dbl_BASIS_STATS > 0
    int i, nz1 = 0, nz2 = 0;
#endif
    double pinf1, pinf2, dinf1, dinf2;
    dbl_feas_info fi;
    dbl_EGlpNumInitVar (pinf1);
    dbl_EGlpNumInitVar (pinf2);
    dbl_EGlpNumInitVar (dinf1);
    dbl_EGlpNumInitVar (dinf2);
    dbl_EGlpNumInitVar (fi.totinfeas);

    dbl_ILLbasis_free_basisinfo (lp);
    dbl_ILLbasis_init_basisinfo (lp);
    rval = dbl_ILLbasis_build_basisinfo (lp);
    ILL_CLEANUP_IF (rval);

    ILL_SAFE_MALLOC (vstat1, lp->ncols, int);
    ILL_SAFE_MALLOC (vstat2, lp->ncols, int);

    if (algorithm != PRIMAL_SIMPLEX) {
	rval = dbl_get_initial_basis2 (lp, vstat2);
	ILL_CLEANUP_IF (rval);
	rval = dbl_set_basis_indices (lp, vstat2);
	lp->basisid = 0;
	ILL_CLEANUP;
    }
    rval = dbl_get_initial_basis1 (lp, vstat1);
    ILL_CLEANUP_IF (rval);
    rval = dbl_get_initial_basis2 (lp, vstat2);
    ILL_CLEANUP_IF (rval);
    lp->basisid = 0;

    /* handle first basis */
    rval = dbl_set_basis_indices (lp, vstat1);
    ILL_CLEANUP_IF (rval);
#if dbl_BASIS_STATS > 0
    for (i = 0; i < lp->nrows; i++)
	nz1 += lp->matcnt[lp->baz[i]];
#endif
    rval = dbl_ILLbasis_factor (lp, &singular);
    ILL_CLEANUP_IF (rval);

    dbl_ILLfct_compute_piz (lp);
    dbl_ILLfct_compute_dz (lp);
    dbl_ILLfct_dual_adjust (lp, dbl_zeroLpNum);
    dbl_ILLfct_compute_xbz (lp);

    dbl_ILLfct_check_pfeasible (lp, &fi, lp->tol->pfeas_tol);
    dbl_ILLfct_check_dfeasible (lp, &fi, lp->tol->dfeas_tol);
    dbl_EGlpNumCopy (pinf1, lp->pinfeas);
    dbl_EGlpNumCopy (dinf1, lp->dinfeas);
    /*
     * dbl_ILLfct_compute_pobj (lp);  obj1p = lp->objval;
     * dbl_ILLfct_compute_dobj (lp);  obj1d = lp->objval;
     */

    /* handle second basis */
    rval = dbl_set_basis_indices (lp, vstat2);
    ILL_CLEANUP_IF (rval);
#if dbl_BASIS_STATS > 0
    for (i = 0; i < lp->nrows; i++)
	nz2 += lp->matcnt[lp->baz[i]];
#endif
    rval = dbl_ILLbasis_factor (lp, &singular);
    ILL_CLEANUP_IF (rval);

    dbl_ILLfct_compute_piz (lp);
    dbl_ILLfct_compute_dz (lp);
    dbl_ILLfct_dual_adjust (lp, dbl_zeroLpNum);
    dbl_ILLfct_compute_xbz (lp);

    dbl_ILLfct_check_pfeasible (lp, &fi, lp->tol->pfeas_tol);
    dbl_ILLfct_check_dfeasible (lp, &fi, lp->tol->dfeas_tol);
    dbl_EGlpNumCopy (pinf2, lp->pinfeas);
    dbl_EGlpNumCopy (dinf2, lp->dinfeas);

#if dbl_BASIS_STATS > 0
    printf ("b1: nz %d pinf %.2f dinf %.2f\n", nz1, dbl_EGlpNumToLf (pinf1),
	dbl_EGlpNumToLf (dinf1));
    printf ("b2: nz %d pinf %.2f dinf %.2f\n", nz2, dbl_EGlpNumToLf (pinf2),
	dbl_EGlpNumToLf (dinf2));
#endif
    choice = dbl_choose_basis (algorithm, pinf1, dinf1, pinf2, dinf2);
    if (choice == 1) {
	lp->fbasisid = -1;
	rval = dbl_set_basis_indices (lp, vstat1);
	ILL_CLEANUP_IF (rval);
    }
CLEANUP:
    if (rval == E_SIMPLEX_ERROR) {
	FILE *fil = fopen ("bad.lp", "w");
	int tval = dbl_ILLwrite_lp_file (lp->O, fil, NULL);
	if (tval) {
	    fprintf (stderr, "Error writing bad lp\n");
	}
	if (fil != NULL)
	    fclose (fil);
    }
    ILL_IFFREE (vstat1, int);
    ILL_IFFREE (vstat2, int);
    dbl_EGlpNumClearVar (pinf1);
    dbl_EGlpNumClearVar (pinf2);
    dbl_EGlpNumClearVar (dinf1);
    dbl_EGlpNumClearVar (dinf2);
    dbl_EGlpNumClearVar (fi.totinfeas);
    ILL_RETURN (rval, "dbl_ILLbasis_get_initial");
}

int dbl_ILLbasis_factor (dbl_lpinfo * lp,
      int *singular)
{
    int rval = 0;
    int i;
    int eindex;
    int lindex;
    int ltype;
    int lvstat;
    int nsing = 0;
    int *singr = 0;
    int *singc = 0;

    *singular = 0;
    do {
	if (lp->f) {
	    dbl_ILLfactor_free_factor_work (lp->f);
	} else {
	    ILL_SAFE_MALLOC (lp->f, 1, dbl_factor_work);
	    dbl_EGlpNumInitVar (lp->f->fzero_tol);
	    dbl_EGlpNumInitVar (lp->f->szero_tol);
	    dbl_EGlpNumInitVar (lp->f->partial_tol);
	    dbl_EGlpNumInitVar (lp->f->maxelem_orig);
	    dbl_EGlpNumInitVar (lp->f->maxelem_factor);
	    dbl_EGlpNumInitVar (lp->f->maxelem_cur);
	    dbl_EGlpNumInitVar (lp->f->partial_cur);
	    dbl_ILLfactor_init_factor_work (lp->f);
	}
	rval = dbl_ILLfactor_create_factor_work (lp->f, lp->nrows);
	ILL_CLEANUP_IF (rval);

	rval = dbl_ILLfactor (lp->f, lp->baz, lp->matbeg, lp->matcnt,
	    lp->matind, lp->matval, &nsing, &singr, &singc);
	ILL_CLEANUP_IF (rval);

	if (nsing != 0) {
	    *singular = 1;
	    for (i = 0; i < nsing; i++) {
		eindex = lp->vindex[lp->O->rowmap[singr[i]]];
		lindex = singc[i];
		ltype = lp->vtype[lp->baz[lindex]];

		if (ltype == VBOUNDED || ltype == VLOWER || ltype == VARTIFICIAL)
		    lvstat = STAT_LOWER;
		else if (ltype == VUPPER)
		    lvstat = STAT_UPPER;
		else
		    lvstat = STAT_ZERO;

		dbl_ILLfct_update_basis_info (lp, eindex, lindex, lvstat);
		lp->basisid++;
	    }
	    ILL_IFFREE (singr, int);
	    ILL_IFFREE (singc, int);
	}
    } while (nsing != 0);

    lp->fbasisid = lp->basisid;

CLEANUP:
    ILL_IFFREE (singr, int);
    ILL_IFFREE (singc, int);
    ILL_RETURN (rval, "dbl_ILLbasis_factor");
}

int dbl_ILLbasis_refactor (dbl_lpinfo * lp)
{
    int sing = 0;
    int rval = 0;

    rval = dbl_ILLbasis_factor (lp, &sing);
    if (sing) {
	fprintf (stderr, "Singular basis in dbl_ILLbasis_refactor()\n");
	rval = -1;
    }
    ILL_RETURN (rval, "dbl_ILLbasis_refactor");
}

void dbl_ILLbasis_column_solve (dbl_lpinfo * lp,
      dbl_svector * rhs,
      dbl_svector * soln)
{
    dbl_ILLfactor_ftran (lp->f, rhs, soln);
}

void dbl_ILLbasis_column_solve_update (dbl_lpinfo * lp,
      dbl_svector * rhs,
      dbl_svector * upd,
      dbl_svector * soln)
{
    dbl_ILLfactor_ftran_update (lp->f, rhs, upd, soln);
}

void dbl_ILLbasis_row_solve (dbl_lpinfo * lp,
      dbl_svector * rhs,
      dbl_svector * soln)
{
    dbl_ILLfactor_btran (lp->f, rhs, soln);
}

int dbl_ILLbasis_update (dbl_lpinfo * lp,
      dbl_svector * y,
      int lindex,
      int *refactor,
      int *singular)
{
#if 0				/* To always refactor, change 0 to 1 */
    *refactor = 1;
    return dbl_ILLbasis_factor (lp, singular);
#else

    int rval = 0;

    *refactor = 0;
    rval = dbl_ILLfactor_update (lp->f, y, lindex, refactor);
    if (rval == E_FACTOR_BLOWUP || rval == E_UPDATE_SINGULAR_ROW
	|| rval == E_UPDATE_SINGULAR_COL) {
	/* Bico - comment out for dist fprintf(stderr, "Warning: numerically
	   bad basis in dbl_ILLfactor_update\n"); */
	*refactor = 1;
	rval = 0;
    }
    if (rval == E_UPDATE_NOSPACE) {
	*refactor = 1;
	rval = 0;
    }
    if (*refactor)
	rval = dbl_ILLbasis_factor (lp, singular);

    if (rval) {
	FILE *eout = 0;
	int tval;

	printf ("write bad lp to factor.lp\n");
	fflush (stdout);
	eout = fopen ("factor.lp", "w");
	if (!eout) {
	    fprintf (stderr, "could not open file to write bad factor lp\n");
	} else {
	    tval = dbl_ILLwrite_lp_file (lp->O, eout, NULL);
	    if (tval) {
		fprintf (stderr, "error while writing bad factor lp\n");
	    }
	    fclose (eout);
	}

	printf ("write bad basis to factor.bas\n");
	fflush (stdout);
	tval = dbl_ILLlib_writebasis (lp, 0, "factor.bas");
	if (tval) {
	    fprintf (stderr, "error while writing factor basis\n");
	}
    }
    ILL_RETURN (rval, "dbl_ILLbasis_update");
#endif
}
