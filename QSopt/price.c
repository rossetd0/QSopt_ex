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

/* RCS_INFO = "$RCSfile: price.c,v $ $Revision: 1.2 $ $Date: 2003/11/05 16:49:52 $"; */
static int TRACE = 0;
#include "econfig.h"
#include "stddefs.h"
#include "qsopt.h"
#include "lpdefs.h"
#include "fct.h"
#include "price.h"
#include "basis.h"
#include "iqsutil.h"
#include "dstruct.h"
#ifdef USEDMALLOC
#include "dmalloc.h"
#endif

#define  MULTIP 1
#define  PRICE_DEBUG 0

static void update_d_scaleinf (price_info * const p,
															 heap * const h,
															 int const j,
															 EGlpNum_t inf,
															 int const prule),
  update_p_scaleinf (price_info * const p,
										 heap * const h,
										 int const i,
										 EGlpNum_t inf,
										 int const prule);

static void compute_dualI_inf (lpinfo * const lp,
															 int const j,
															 EGlpNum_t * const inf),
  compute_dualII_inf (lpinfo * const lp,
											int const j,
											EGlpNum_t * const inf),
  compute_primalI_inf (lpinfo * const lp,
											 int const i,
											 EGlpNum_t * const inf),
  compute_primalII_inf (lpinfo * const lp,
												int const i,
												EGlpNum_t * const inf);

void ILLprice_free_heap (price_info * const pinf)
{
	ILLheap_free (&(pinf->h));
}

int ILLprice_build_heap (price_info * const pinf,
												 int const nkeys,
												 EGlpNum_t * keylist)
{
	ILLheap_init (&(pinf->h));
	EGlpNumSet (pinf->htrigger,
							1.0 +
							(double) nkeys / (PARAM_HEAP_RATIO * ILLutil_our_log2 (nkeys)));
	return ILLheap_build (&(pinf->h), nkeys, keylist);
}

int ILLprice_test_for_heap (lpinfo * const lp,
														price_info * const pinf,
														int const nkeys,
														EGlpNum_t * keylist,
														int const algo,
														int const upd)
{
	heap *const h = &(pinf->h);
	int rval = 0;
	EGlpNum_t ravg;
	if (upd != 0)
	{
		EGlpNumInitVar (ravg);
		if (algo == PRIMAL_SIMPLEX)
			EGlpNumCopy (ravg, lp->cnts->za_ravg);
		else
			EGlpNumCopy (ravg, lp->cnts->y_ravg);
		if (EGlpNumIsLeq (ravg, pinf->htrigger))
			pinf->hineff--;
		else
		{
			EGlpNumDivUiTo (ravg, 2U);
			if (EGlpNumIsLess (pinf->htrigger, ravg))
				pinf->hineff++;
		}
		EGlpNumClearVar (ravg);
	}
	if (h->hexist == 0 && pinf->hineff <= 0)
	{
		rval = ILLprice_build_heap (pinf, nkeys, keylist);
		ILL_CLEANUP_IF (rval);
	}
	else if (h->hexist != 0 && pinf->hineff >= PARAM_HEAP_UTRIGGER)
	{
		ILLprice_free_heap (pinf);
		/*
		 * printf ("freeing heap ..\n");
		 * printf ("iter = %d, ravg = %.2f, trigger = %.2f\n",
		 * lp->cnts->tot_iter, ravg, pinf->htrigger);
		 */
	}

CLEANUP:
	if (rval)
		ILLprice_free_heap (pinf);
	return rval;
}

void ILLprice_init_pricing_info (price_info * const pinf)
{
	pinf->p_strategy = -1;
	pinf->d_strategy = -1;
	pinf->pI_price = -1;
	pinf->pII_price = -1;
	pinf->dI_price = -1;
	pinf->dII_price = -1;
	pinf->cur_price = -1;
	pinf->p_scaleinf = 0;
	pinf->d_scaleinf = 0;
	pinf->pdinfo.norms = 0;
	pinf->pdinfo.refframe = 0;
	pinf->psinfo.norms = 0;
	pinf->ddinfo.norms = 0;
	pinf->ddinfo.refframe = 0;
	pinf->dsinfo.norms = 0;
	pinf->dmpinfo.gstart = pinf->pmpinfo.gstart = 0;
	pinf->dmpinfo.gshift = pinf->pmpinfo.gshift = 0;
	pinf->dmpinfo.gsize = pinf->pmpinfo.gsize = 0;
	pinf->dmpinfo.bucket = pinf->pmpinfo.bucket = 0;
	pinf->dmpinfo.perm = pinf->pmpinfo.perm = 0;
	pinf->dmpinfo.infeas = pinf->pmpinfo.infeas = 0;
	ILLheap_init (&(pinf->h));
	EGlpNumZero (pinf->htrigger);
	pinf->hineff = 0;
}

void ILLprice_free_pricing_info (price_info * const pinf)
{
	EGlpNumFreeArray (pinf->p_scaleinf);
	EGlpNumFreeArray (pinf->d_scaleinf);
	EGlpNumFreeArray (pinf->pdinfo.norms);
	ILL_IFFREE (pinf->pdinfo.refframe, int);
	EGlpNumFreeArray (pinf->psinfo.norms);
	EGlpNumFreeArray (pinf->ddinfo.norms);
	ILL_IFFREE (pinf->ddinfo.refframe, int);
	EGlpNumFreeArray (pinf->dsinfo.norms);
	ILLprice_free_mpartial_info (&(pinf->pmpinfo));
	ILLprice_free_mpartial_info (&(pinf->dmpinfo));
	ILLprice_free_heap (pinf);
}

int ILLprice_build_pricing_info (lpinfo * const lp,
																 price_info * const pinf,
																 int const phase)
{
	int rval = 0;
	int p_price = -1;
	int d_price = -1;

	switch (phase)
	{
	case PRIMAL_PHASEI:
		p_price = pinf->pI_price;
		break;
	case PRIMAL_PHASEII:
		p_price = pinf->pII_price;
		break;
	case DUAL_PHASEI:
		d_price = pinf->dI_price;
		break;
	case DUAL_PHASEII:
		d_price = pinf->dII_price;
		break;
	}

	if (p_price != -1)
	{
		pinf->cur_price = p_price;

		if (p_price == QS_PRICE_PDANTZIG || p_price == QS_PRICE_PDEVEX ||
				p_price == QS_PRICE_PSTEEP)
		{
			pinf->p_strategy = COMPLETE_PRICING;
			EGlpNumFreeArray (pinf->d_scaleinf);
			pinf->d_scaleinf = EGlpNumAllocArray (lp->nnbasic);
		}
		else if (p_price == QS_PRICE_PMULTPARTIAL)
			pinf->p_strategy = MULTI_PART_PRICING;

		switch (p_price)
		{
		case QS_PRICE_PDEVEX:
			if (pinf->pdinfo.norms)
				return rval;
			rval = ILLprice_build_pdevex_norms (lp, &(pinf->pdinfo), 0);
			ILL_CLEANUP_IF (rval);
			break;
		case QS_PRICE_PSTEEP:
			if (pinf->psinfo.norms)
				return rval;
			rval = ILLprice_build_psteep_norms (lp, &(pinf->psinfo));
			ILL_CLEANUP_IF (rval);
			break;
		case QS_PRICE_PMULTPARTIAL:
			rval = ILLprice_build_mpartial_info (lp, pinf, COL_PRICING);
			ILL_CLEANUP_IF (rval);
			break;
		}
	}
	else if (d_price != -1)
	{
		pinf->cur_price = d_price;

		if (d_price == QS_PRICE_DDANTZIG || d_price == QS_PRICE_DSTEEP ||
				d_price == QS_PRICE_DDEVEX)
		{
			pinf->d_strategy = COMPLETE_PRICING;
			EGlpNumFreeArray (pinf->p_scaleinf);
			pinf->p_scaleinf = EGlpNumAllocArray (lp->nrows);
		}
		else if (d_price == QS_PRICE_DMULTPARTIAL)
			pinf->d_strategy = MULTI_PART_PRICING;

		switch (d_price)
		{
		case QS_PRICE_DSTEEP:
			if (pinf->dsinfo.norms)
				return rval;
			rval = ILLprice_build_dsteep_norms (lp, &(pinf->dsinfo));
			ILL_CLEANUP_IF (rval);
			break;
		case QS_PRICE_DMULTPARTIAL:
			rval = ILLprice_build_mpartial_info (lp, pinf, ROW_PRICING);
			ILL_CLEANUP_IF (rval);
			break;
		case QS_PRICE_DDEVEX:
			if (pinf->ddinfo.norms)
				return rval;
			rval = ILLprice_build_ddevex_norms (lp, &(pinf->ddinfo), 0);
			ILL_CLEANUP_IF (rval);
			break;
		}
	}

CLEANUP:
	if (rval)
		ILLprice_free_pricing_info (pinf);
	ILL_RETURN (rval, "ILLprice_build_pricing_info");
}

int ILLprice_update_pricing_info (lpinfo * const lp,
																	price_info * const pinf,
																	int const phase,
																	svector * const wz,
																	int const eindex,
																	int const lindex,
																	EGlpNum_t y)
{
	int rval = 0;
	int p_price = -1;
	int d_price = -1;

	switch (phase)
	{
	case PRIMAL_PHASEI:
		p_price = pinf->pI_price;
		break;
	case PRIMAL_PHASEII:
		p_price = pinf->pII_price;
		break;
	case DUAL_PHASEI:
		d_price = pinf->dI_price;
		break;
	case DUAL_PHASEII:
		d_price = pinf->dII_price;
		break;
	}

	if (p_price != -1)
	{
		if (p_price == QS_PRICE_PDEVEX)
		{
			rval = ILLprice_update_pdevex_norms (lp, &(pinf->pdinfo), eindex, y);
			ILL_CLEANUP_IF (rval);
		}
		else if (p_price == QS_PRICE_PSTEEP)
			ILLprice_update_psteep_norms (lp, &(pinf->psinfo), wz, eindex, y);
	}
	else if (d_price != -1)
	{
		if (d_price == QS_PRICE_DSTEEP)
			ILLprice_update_dsteep_norms (lp, &(pinf->dsinfo), wz, lindex, y);
		else if (d_price == QS_PRICE_DDEVEX)
		{
			rval = ILLprice_update_ddevex_norms (lp, &(pinf->ddinfo), lindex, y);
			ILL_CLEANUP_IF (rval);
		}
	}
CLEANUP:
	ILL_RETURN (rval, "ILLprice_update_pricing_info");
}

int ILLprice_get_price (price_info * const p,
												int const phase)
{
	int pri = -1;

	switch (phase)
	{
	case PRIMAL_PHASEI:
		return p->pI_price;
	case PRIMAL_PHASEII:
		return p->pII_price;
	case DUAL_PHASEI:
		return p->dI_price;
	case DUAL_PHASEII:
		return p->dII_price;
	}
	return pri;
}

void ILLprice_free_mpartial_info (mpart_info * p)
{
	ILL_IFFREE (p->gstart, int);
	ILL_IFFREE (p->gshift, int);
	ILL_IFFREE (p->gsize, int);
	ILL_IFFREE (p->bucket, int);
	EGlpNumFreeArray (p->infeas);
	ILL_IFFREE (p->perm, int);
}

int ILLprice_build_mpartial_info (lpinfo * const lp,
																	price_info * const pinf,
																	int const pricetype)
{
	mpart_info *const p =
		(pricetype == COL_PRICING) ? &(pinf->pmpinfo) : &(pinf->dmpinfo);
	int const nelems = (pricetype == COL_PRICING) ? lp->nnbasic : lp->nrows;
	int const extra = (nelems % 50) ? nelems - 50 * (nelems / 50) : 0;
	int i = 0;
	int rval = 0;
	p->k = 50;
	p->cgroup = 0;
	p->ngroups = nelems / p->k;
	if (extra != 0)
		p->ngroups++;

	ILL_SAFE_MALLOC (p->gstart, p->ngroups, int);
	ILL_SAFE_MALLOC (p->gshift, p->ngroups, int);
	ILL_SAFE_MALLOC (p->gsize, p->ngroups, int);
	ILL_SAFE_MALLOC (p->bucket, 2 * p->k, int);
	p->infeas = EGlpNumAllocArray (2 * p->k);
	ILL_SAFE_MALLOC (p->perm, 2 * p->k, int);

	p->bsize = 0;

	if (extra != 0)
	{
		p->gstart[0] = 0;
		p->gshift[0] = 1;
		p->gsize[0] = extra;
		for (i = 1; i < p->ngroups; i++)
		{
			p->gstart[i] = extra + i - 1;
			p->gshift[i] = p->ngroups - 1;
			p->gsize[i] = p->k;
		}
	}
	else
	{
		for (i = 0; i < p->ngroups; i++)
		{
			p->gstart[i] = i;
			p->gshift[i] = p->ngroups;
			p->gsize[i] = p->k;
		}
	}

CLEANUP:
	if (rval)
		ILLprice_free_mpartial_info (p);
	ILL_RETURN (rval, "ILLprice_build_mpartial_info");
}

void ILLprice_init_mpartial_price (lpinfo * const lp,
																	 price_info * const pinf,
																	 int const phase,
																	 int const pricetype)
{
	mpart_info *const p =
		(pricetype == COL_PRICING) ? &(pinf->pmpinfo) : &(pinf->dmpinfo);
	int i;
	p->bsize = 0;
	i = p->cgroup;
	do
	{
		ILLprice_mpartial_group (lp, p, phase, i, pricetype);
		i = (i + 1) % p->ngroups;
	} while (i != p->cgroup && p->bsize <= p->k);
	p->cgroup = i;
}

void ILLprice_update_mpartial_price (lpinfo * const lp,
																		 price_info * const pinf,
																		 int const phase,
																		 int const pricetype)
{
	mpart_info *const p =
		(pricetype == COL_PRICING) ? &(pinf->pmpinfo) : &(pinf->dmpinfo);
	int i = 0;
	int csize = 0;
	EGlpNum_t infeas;
	price_res pr;
	EGlpNumInitVar (pr.dinfeas);
	EGlpNumInitVar (pr.pinfeas);
	EGlpNumInitVar (infeas);

#ifdef MULTIP
	i = 0;
	while (i < p->bsize)
	{
		if (pricetype == COL_PRICING)
		{
			ILLprice_column (lp, p->bucket[i], phase, &pr);
			EGlpNumCopy (infeas, pr.dinfeas);
		}
		else
		{
			ILLprice_row (lp, p->bucket[i], phase, &pr);
			EGlpNumCopy (infeas, pr.pinfeas);
		}
		if (EGlpNumIsEqqual (infeas, zeroLpNum))
		{
			p->bucket[i] = p->bucket[p->bsize - 1];
			p->bsize--;
		}
		else
		{
			EGlpNumCopy (p->infeas[i], infeas);
			i++;
		}
	}
	if (p->bsize > 0)
	{
		for (i = 0; i < p->bsize; i++)
			p->perm[i] = i;
		EGutilPermSort ((size_t)(p->bsize), p->perm,(const EGlpNum_t*const) p->infeas);

		csize = MIN (p->bsize, p->k);
		for (i = csize - 1; i >= 0; i--)
			lp->iwork[p->bucket[p->perm[i]]] = 1;

		for (i = 0, csize = 0; i < p->bsize; i++)
			if (lp->iwork[p->bucket[i]] == 1)
			{
				EGlpNumCopy (p->infeas[csize], p->infeas[i]);
				p->bucket[csize] = p->bucket[i];
				csize++;
			}
		p->bsize = csize;
	}
#else
	p->bsize = 0;
#endif

	i = p->cgroup;
	do
	{
		ILLprice_mpartial_group (lp, p, phase, i, pricetype);
		i = (i + 1) % p->ngroups;
	} while (i != p->cgroup && p->bsize <= p->k);
	p->cgroup = i;

#ifdef MULTIP
	for (i = 0; i < csize; i++)
		lp->iwork[p->bucket[i]] = 0;
#endif
	EGlpNumClearVar (infeas);
	EGlpNumClearVar (pr.pinfeas);
	EGlpNumClearVar (pr.dinfeas);
}

void ILLprice_delete_onempart_price (lpinfo * const lp,
																		 price_info * const pinf,
																		 int const indx,
																		 int const pricetype)
{
	mpart_info *const p =
		(pricetype == COL_PRICING) ? &(pinf->pmpinfo) : &(pinf->dmpinfo);
	int i = 0;

	for (i = 0; i < p->bsize; i++)
		if (p->bucket[i] == indx)
		{
			p->bucket[i] = p->bucket[p->bsize - 1];
			EGlpNumCopy (p->infeas[i], p->infeas[p->bsize - 1]);
			p->bsize--;
			break;
		}
}

void ILLprice_mpartial_group (lpinfo * const lp,
															mpart_info * const p,
															int const phase,
															int const g,
															int const pricetype)
{
	int i,
	  ix;
	int gstart = p->gstart[g];
	int gsize = p->gsize[g];
	int gshift = p->gshift[g];
	EGlpNum_t infeas;
	price_res pr;
	EGlpNumInitVar (pr.dinfeas);
	EGlpNumInitVar (pr.pinfeas);
	EGlpNumInitVar (infeas);

	for (i = 0, ix = gstart; i < gsize; i++, ix += gshift)
	{
#ifdef MULTIP
		if (lp->iwork[ix])
			continue;
#endif
		if (pricetype == COL_PRICING)
		{
			ILLprice_column (lp, ix, phase, &pr);
			EGlpNumCopy (infeas, pr.dinfeas);
		}
		else
		{
			ILLprice_row (lp, ix, phase, &pr);
			EGlpNumCopy (infeas, pr.pinfeas);
		}
		if (EGlpNumIsNeqqZero (infeas))
		{
			EGlpNumCopy (p->infeas[p->bsize], infeas);
			p->bucket[p->bsize] = ix;
			p->bsize++;
		}
	}
	EGlpNumClearVar (infeas);
	EGlpNumClearVar (pr.dinfeas);
	EGlpNumClearVar (pr.pinfeas);
}

void ILLprice_column (lpinfo * const lp,
											int const ix,
											int const phase,
											price_res * const pr)
{
	int const col = lp->nbaz[ix];
	int const mcnt = lp->matcnt[col];
	int const mbeg = lp->matbeg[col];
	int i;
	EGlpNum_t sum;
	EGlpNumZero (pr->dinfeas);
	if (lp->vtype[col] == VARTIFICIAL || lp->vtype[col] == VFIXED)
		return;
	EGlpNumInitVar (sum);
	EGlpNumZero (sum);

	if (phase == PRIMAL_PHASEII)
	{
		for (i = 0; i < mcnt; i++)
			EGlpNumAddInnProdTo (sum, lp->piz[lp->matind[mbeg + i]],
													 lp->matval[mbeg + i]);
		EGlpNumCopyDiff (lp->dz[ix], lp->cz[col], sum);
		compute_dualII_inf (lp, ix, &(pr->dinfeas));
	}
	else
	{
		for (i = 0; i < mcnt; i++)
			EGlpNumAddInnProdTo (sum, lp->pIpiz[lp->matind[mbeg + i]],
													 lp->matval[mbeg + i]);
		EGlpNumCopyNeg (lp->pIdz[ix], sum);
		compute_dualI_inf (lp, ix, &(pr->dinfeas));
	}
	EGlpNumClearVar (sum);
}

void ILLprice_row (lpinfo * const lp,
									 int const ix,
									 int const phase,
									 price_res * const pr)
{
	if (phase == DUAL_PHASEII)
		compute_primalII_inf (lp, ix, &(pr->pinfeas));
	else
		compute_primalI_inf (lp, ix, &(pr->pinfeas));
}

int ILLprice_build_pdevex_norms (lpinfo * const lp,
																 p_devex_info * const pdinfo,
																 int const reinit)
{
	int j;
	int rval = 0;

	if (reinit == 0)
	{
		pdinfo->ninit = 0;
		pdinfo->norms = EGlpNumAllocArray (lp->nnbasic);
		ILL_SAFE_MALLOC (pdinfo->refframe, lp->ncols, int);
	}

	if (reinit != 0)
		pdinfo->ninit++;

	for (j = 0; j < lp->ncols; j++)
	{
		if (lp->vstat[j] == STAT_BASIC)
			pdinfo->refframe[j] = 0;
		else
		{
			EGlpNumOne (pdinfo->norms[lp->vindex[j]]);
			pdinfo->refframe[j] = 1;
		}
	}

CLEANUP:
	if (rval)
	{
		EGlpNumFreeArray (pdinfo->norms);
		ILL_IFFREE (pdinfo->refframe, int);
	}
	ILL_RETURN (rval, "ILLprice_build_pdevex_norms");
}

int ILLprice_update_pdevex_norms (lpinfo * const lp,
																	p_devex_info * const pdinfo,
																	int const eindex,
																	EGlpNum_t yl)
{
	int i,
	  j;
	EGlpNum_t normj;
	EGlpNum_t zAj;
	EGlpNumInitVar (normj);
	EGlpNumInitVar (zAj);
	EGlpNumZero (normj);

	for (i = 0; i < lp->yjz.nzcnt; i++)
		if (pdinfo->refframe[lp->baz[lp->yjz.indx[i]]])
			EGlpNumAddInnProdTo (normj, lp->yjz.coef[i], lp->yjz.coef[i]);

	if (pdinfo->refframe[lp->nbaz[eindex]])
		EGlpNumAddTo (normj, oneLpNum);

	EGlpNumCopyFrac (zAj, normj, pdinfo->norms[eindex]);
	if (EGlpNumIsGreaDbl (zAj, 0x1p10) || EGlpNumIsLessDbl (zAj, 0x1p-10))
	{
		EGlpNumClearVar (zAj);
		EGlpNumClearVar (normj);
		return ILLprice_build_pdevex_norms (lp, pdinfo, 1);
	}

	for (i = 0; i < lp->zA.nzcnt; i++)
	{
		j = lp->zA.indx[i];
		EGlpNumCopyFrac (zAj, lp->zA.coef[i], yl);
		EGlpNumMultTo (zAj, zAj);
		EGlpNumMultTo (zAj, normj);
		if (EGlpNumIsLess (pdinfo->norms[j], zAj))
			EGlpNumCopy (pdinfo->norms[j], zAj);
	}
	EGlpNumDivTo (normj, yl);
	EGlpNumDivTo (normj, yl);
	if (EGlpNumIsLess (normj, oneLpNum))
		EGlpNumCopy (pdinfo->norms[eindex], oneLpNum);
	else
		EGlpNumCopy (pdinfo->norms[eindex], normj);
	EGlpNumClearVar (zAj);
	EGlpNumClearVar (normj);
	return 0;
}

int ILLprice_build_psteep_norms (lpinfo * const lp,
																 p_steep_info * const psinfo)
{
	int j;
	int rval = 0;
	svector yz;

	ILLsvector_init (&yz);
	rval = ILLsvector_alloc (&yz, lp->nrows);
	ILL_CLEANUP_IF (rval);
	psinfo->norms = EGlpNumAllocArray (lp->nnbasic);

	for (j = 0; j < lp->nnbasic; j++)
	{
		rval = ILLstring_report (NULL, &lp->O->reporter);
		ILL_CLEANUP_IF (rval);
		ILLfct_compute_yz (lp, &yz, 0, lp->nbaz[j]);
		EGlpNumInnProd (psinfo->norms[j], yz.coef, yz.coef, (size_t)yz.nzcnt);
		EGlpNumAddTo (psinfo->norms[j], oneLpNum);
	}

CLEANUP:
	ILLsvector_free (&yz);
	if (rval)
		EGlpNumFreeArray (psinfo->norms);
	ILL_RETURN (rval, "ILLprice_build_psteep_norms");
}

void ILLprice_update_psteep_norms (lpinfo * const lp,
																	 p_steep_info * const psinfo,
																	 svector * const wz,
																	 int const eindex,
																	 EGlpNum_t yl)
{
	int i,
	  j,
	  k;
	int mcnt,
	  mbeg;
	EGlpNum_t normj;
	EGlpNum_t zAj,
	  wAj;
	EGlpNum_t *v = 0;
	EGlpNumInitVar (normj);
	EGlpNumInitVar (zAj);
	EGlpNumInitVar (wAj);
	EGlpNumInnProd (normj, lp->yjz.coef, lp->yjz.coef, (size_t)(lp->yjz.nzcnt));
	EGlpNumAddTo (normj, oneLpNum);

#if 0
	Bico - remove warnings for dist
		if (fabs ((normj - psinfo->norms[eindex]) / normj) > 1000.0 /* 0.01 */ )
		{
			printf ("warning: incorrect norm values\n");
			printf ("anorm = %.6f, pnorm = %.6f\n", normj, psinfo->norms[eindex]);
			fflush (stdout);
		}
#endif

	ILLfct_load_workvector (lp, wz);
	v = lp->work.coef;

	for (k = 0; k < lp->zA.nzcnt; k++)
	{
		j = lp->zA.indx[k];
		EGlpNumZero (wAj);
		mcnt = lp->matcnt[lp->nbaz[j]];
		mbeg = lp->matbeg[lp->nbaz[j]];
		for (i = 0; i < mcnt; i++)
			EGlpNumAddInnProdTo (wAj, lp->matval[mbeg + i], v[lp->matind[mbeg + i]]);
		EGlpNumCopy (zAj, lp->zA.coef[k]);
		//psinfo->norms[j] += (zAj * ((zAj * normj / yl) - (2.0 * wAj))) / yl;
		EGlpNumMultUiTo (wAj, 2U);
		EGlpNumMultTo (zAj, normj);
		EGlpNumDivTo (zAj, yl);
		EGlpNumSubTo (zAj, wAj);
		EGlpNumMultTo (zAj, lp->zA.coef[k]);
		EGlpNumDivTo (zAj, yl);
		EGlpNumAddTo (psinfo->norms[j], zAj);
		if (EGlpNumIsLess (psinfo->norms[j], oneLpNum))
			EGlpNumOne (psinfo->norms[j]);
	}

	EGlpNumCopyFrac (psinfo->norms[eindex], normj, yl);
	EGlpNumDivTo (psinfo->norms[eindex], yl);
	if (EGlpNumIsLess (psinfo->norms[eindex], oneLpNum))
		EGlpNumOne (psinfo->norms[eindex]);

	ILLfct_zero_workvector (lp);
	EGlpNumClearVar (wAj);
	EGlpNumClearVar (zAj);
	EGlpNumClearVar (normj);
}

int ILLprice_build_ddevex_norms (lpinfo * const lp,
																 d_devex_info * const ddinfo,
																 int const reinit)
{
	int i;
	int rval = 0;

	if (reinit == 0)
	{
		ddinfo->ninit = 0;
		ddinfo->norms = EGlpNumAllocArray (lp->nrows);
		ILL_SAFE_MALLOC (ddinfo->refframe, lp->ncols, int);
	}
	if (reinit != 0)
		ddinfo->ninit++;

	for (i = 0; i < lp->ncols; i++)
		ddinfo->refframe[i] = (lp->vstat[i] == STAT_BASIC) ? 1 : 0;

	for (i = 0; i < lp->nrows; i++)
		EGlpNumOne (ddinfo->norms[i]);

CLEANUP:
	if (rval)
	{
		EGlpNumFreeArray (ddinfo->norms);
		ILL_IFFREE (ddinfo->refframe, int);
	}
	ILL_RETURN (rval, "ILLprice_build_ddevex_norms");
}

int ILLprice_update_ddevex_norms (lpinfo * const lp,
																	d_devex_info * const ddinfo,
																	int const lindex,
																	EGlpNum_t yl)
{
	int i,
	  r;
	EGlpNum_t normi;
	EGlpNum_t yr;
	EGlpNumInitVar (normi);
	EGlpNumInitVar (yr);
	EGlpNumZero (normi);

	for (i = 0; i < lp->zA.nzcnt; i++)
		if (ddinfo->refframe[lp->nbaz[lp->zA.indx[i]]])
			EGlpNumAddInnProdTo (normi, lp->zA.coef[i], lp->zA.coef[i]);

	if (ddinfo->refframe[lp->baz[lindex]])
		EGlpNumAddTo (normi, oneLpNum);

	EGlpNumCopyFrac (yr, normi, ddinfo->norms[lindex]);
	if (EGlpNumIsGreaDbl (yr, 0x1p10) || EGlpNumIsLessDbl (yr, 0x1p-10))
	{
		EGlpNumClearVar (normi);
		EGlpNumClearVar (yr);
		return ILLprice_build_ddevex_norms (lp, ddinfo, 1);
	}
	EGlpNumDivTo (normi, yl);
	EGlpNumDivTo (normi, yl);

	for (i = 0; i < lp->yjz.nzcnt; i++)
	{
		r = lp->yjz.indx[i];
		EGlpNumCopy (yr, lp->yjz.coef[i]);
		EGlpNumMultTo (yr, yr);
		EGlpNumMultTo (yr, normi);
		if (EGlpNumIsLess (ddinfo->norms[r], yr))
			EGlpNumCopy (ddinfo->norms[r], yr);
	}
	EGlpNumCopy (ddinfo->norms[lindex], normi);
	if (EGlpNumIsLess (ddinfo->norms[lindex], oneLpNum))
		EGlpNumOne (ddinfo->norms[lindex]);
	EGlpNumClearVar (normi);
	EGlpNumClearVar (yr);
	return 0;
}

int ILLprice_build_dsteep_norms (lpinfo * const lp,
																 d_steep_info * const dsinfo)
{
	int i;
	int rval = 0;
	svector z;

	ILLsvector_init (&z);
	rval = ILLsvector_alloc (&z, lp->nrows);
	ILL_CLEANUP_IF (rval);
	dsinfo->norms = EGlpNumAllocArray (lp->nrows);

	for (i = 0; i < lp->nrows; i++)
	{
		rval = ILLstring_report (NULL, &lp->O->reporter);
		ILL_CLEANUP_IF (rval);

		ILLfct_compute_zz (lp, &z, i);
		EGlpNumInnProd (dsinfo->norms[i], z.coef, z.coef, (size_t)z.nzcnt);
		if (EGlpNumIsLess (dsinfo->norms[i], PARAM_MIN_DNORM))
			EGlpNumCopy (dsinfo->norms[i], PARAM_MIN_DNORM);
	}

CLEANUP:
	ILLsvector_free (&z);
	if (rval)
		EGlpNumFreeArray (dsinfo->norms);
	ILL_RETURN (rval, "ILLprice_build_dsteep_norms");
}

int ILLprice_get_dsteep_norms (lpinfo * const lp,
															 int const count,
															 int *const rowind,
															 EGlpNum_t * const norms)
{
	int i;
	int rval = 0;
	svector z;

	ILLsvector_init (&z);
	rval = ILLsvector_alloc (&z, lp->nrows);
	ILL_CLEANUP_IF (rval);

	for (i = 0; i < count; i++)
	{
		ILLfct_compute_zz (lp, &z, rowind[i]);
		EGlpNumInnProd (norms[i], z.coef, z.coef, (size_t)z.nzcnt);
	}

CLEANUP:
	ILLsvector_free (&z);
	ILL_RETURN (rval, "ILLprice_get_dsteep_norms");
}

void ILLprice_update_dsteep_norms (lpinfo * const lp,
																	 d_steep_info * const dsinfo,
																	 svector * const wz,
																	 int const lindex,
																	 EGlpNum_t yl)
{
	int i,
	  k;
	EGlpNum_t yij;
	EGlpNum_t norml;
	EGlpNum_t *v = 0;
	EGlpNumInitVar (norml);
	EGlpNumInitVar (yij);
	EGlpNumZero (norml);
	EGlpNumInnProd (norml, lp->zz.coef, lp->zz.coef, (size_t)(lp->zz.nzcnt));

#if 0
	Bico - remove warnings for dist
		if (fabs ((norml - dsinfo->norms[lindex]) / norml) > 1000.0 /*0.01 */ )
		{
			printf ("warning: incorrect dnorm values\n");
			printf ("anorm = %.6f, pnorm = %.6f\n", norml, dsinfo->norms[lindex]);
			fflush (stdout);
		}
#endif

	ILLfct_load_workvector (lp, wz);
	v = lp->work.coef;

	for (k = 0; k < lp->yjz.nzcnt; k++)
	{
		i = lp->yjz.indx[k];
		EGlpNumCopy (yij, lp->yjz.coef[k]);
		EGlpNumMultTo (yij, norml);
		EGlpNumDivTo (yij, yl);
		EGlpNumSubTo (yij, v[i]);
		EGlpNumSubTo (yij, v[i]);
		EGlpNumMultTo (yij, lp->yjz.coef[k]);
		EGlpNumDivTo (yij, yl);
		EGlpNumAddTo (dsinfo->norms[i], yij);
		if (EGlpNumIsLess (dsinfo->norms[i], PARAM_MIN_DNORM))
			EGlpNumCopy (dsinfo->norms[i], PARAM_MIN_DNORM);
	}
	EGlpNumCopyFrac (dsinfo->norms[lindex], norml, yl);
	EGlpNumDivTo (dsinfo->norms[lindex], yl);
	if (EGlpNumIsLess (dsinfo->norms[lindex], PARAM_MIN_DNORM))
		EGlpNumCopy (dsinfo->norms[lindex], PARAM_MIN_DNORM);

	ILLfct_zero_workvector (lp);
	EGlpNumClearVar (norml);
	EGlpNumClearVar (yij);
}

static void update_d_scaleinf (price_info * const p,
															 heap * const h,
															 int const j,
															 EGlpNum_t inf,
															 int const prule)
{
	if (EGlpNumIsEqqual (inf, zeroLpNum))
	{
		EGlpNumZero (p->d_scaleinf[j]);
		if (h->hexist != 0 && h->loc[j] != -1)
			ILLheap_delete (h, j);
	}
	else
	{
		if (prule == QS_PRICE_PDANTZIG)
			EGlpNumCopy (p->d_scaleinf[j], inf);
		else if (prule == QS_PRICE_PDEVEX)
			EGlpNumCopySqrOver (p->d_scaleinf[j], inf, p->pdinfo.norms[j]);
		else if (prule == QS_PRICE_PSTEEP)
			EGlpNumCopySqrOver (p->d_scaleinf[j], inf, p->psinfo.norms[j]);

		if (h->hexist != 0)
		{
			if (h->loc[j] == -1)
				ILLheap_insert (h, j);
			else
				ILLheap_modify (h, j);
		}
	}
}

static void compute_dualI_inf (lpinfo * const lp,
															 const int j,
															 EGlpNum_t * const inf)
{
	int const col = lp->nbaz[j];
	int const vt = lp->vtype[col];
	int const vs = lp->vstat[col];
	EGlpNumZero (*inf);
	if (vt != VARTIFICIAL && vt != VFIXED)
	{
		if ((vs == STAT_LOWER || vs == STAT_ZERO) &&
				EGlpNumIsSumLess (lp->pIdz[j], lp->tol->id_tol, zeroLpNum))
			EGlpNumCopyAbs (*inf, lp->pIdz[j]);
		else if ((vs == STAT_UPPER || vs == STAT_ZERO) &&
						 EGlpNumIsLess (lp->tol->id_tol, lp->pIdz[j]))
			EGlpNumCopy (*inf, lp->pIdz[j]);
	}
}

static void compute_dualII_inf (lpinfo * const lp,
																int const j,
																EGlpNum_t * const inf)
{
	int const col = lp->nbaz[j];
	int const vt = lp->vtype[col];
	int const vs = lp->vstat[col];
	EGlpNumZero (*inf);
	if (vt != VARTIFICIAL && vt != VFIXED)
	{
		if ((vs == STAT_LOWER || vs == STAT_ZERO) &&
				EGlpNumIsSumLess (lp->dz[j], lp->tol->dfeas_tol, zeroLpNum))
			EGlpNumCopyAbs (*inf, lp->dz[j]);
		else if ((vs == STAT_UPPER || vs == STAT_ZERO) &&
						 EGlpNumIsLess (lp->tol->dfeas_tol, lp->dz[j]))
			EGlpNumCopy (*inf, lp->dz[j]);
	}
}

void ILLprice_compute_dual_inf (lpinfo * const lp,
																price_info * const p,
																int *const ix,
																int const icnt,
																int const phase)
{
	int const price = (phase == PRIMAL_PHASEI) ? p->pI_price : p->pII_price;
	heap *const h = &(p->h);
	int i;
	EGlpNum_t inf;
	EGlpNumInitVar (inf);
	EGlpNumZero (inf);

	if (phase == PRIMAL_PHASEI)
	{
		if (ix == NULL)
			for (i = 0; i < lp->nnbasic; i++)
			{
				compute_dualI_inf (lp, i, &(inf));
				update_d_scaleinf (p, h, i, inf, price);
			}
		else
			for (i = 0; i < icnt; i++)
			{
				compute_dualI_inf (lp, ix[i], &(inf));
				update_d_scaleinf (p, h, ix[i], inf, price);
			}
	}
	else if (phase == PRIMAL_PHASEII)
	{
		if (ix == NULL)
			for (i = 0; i < lp->nnbasic; i++)
			{
				compute_dualII_inf (lp, i, &inf);
				update_d_scaleinf (p, h, i, inf, price);
			}
		else
			for (i = 0; i < icnt; i++)
			{
				compute_dualII_inf (lp, ix[i], &inf);
				update_d_scaleinf (p, h, ix[i], inf, price);
			}
	}
	EGlpNumClearVar (inf);
}

void ILLprice_primal (lpinfo * const lp,
											price_info * const pinf,
											price_res * const pr,
											int const phase)
{
	heap *const h = &(pinf->h);
	int j,
	  vs;
	EGlpNum_t d;
	EGlpNumInitVar (d);
	EGlpNumZero (d);
	pr->eindex = -1;

#if USEHEAP > 0
	ILLprice_test_for_heap (lp, pinf, lp->nnbasic, pinf->d_scaleinf,
													PRIMAL_SIMPLEX, 1);
#endif

	if (pinf->p_strategy == COMPLETE_PRICING)
	{
		if (h->hexist)
		{
			pr->eindex = ILLheap_findmin (h);
			if (pr->eindex != -1)
				ILLheap_delete (h, pr->eindex);
		}
		else
		{
			for (j = 0; j < lp->nnbasic; j++)
			{
				if (EGlpNumIsLess (d, pinf->d_scaleinf[j]))
				{
					EGlpNumCopy (d, pinf->d_scaleinf[j]);
					pr->eindex = j;
				}
			}
		}
	}
	else if (pinf->p_strategy == MULTI_PART_PRICING)
	{
		for (j = 0; j < pinf->pmpinfo.bsize; j++)
		{
			if (EGlpNumIsLess (d, pinf->pmpinfo.infeas[j]))
			{
				EGlpNumCopy (d, pinf->pmpinfo.infeas[j]);
				pr->eindex = pinf->pmpinfo.bucket[j];
			}
		}
	}

	if (pr->eindex < 0)
		pr->price_stat = PRICE_OPTIMAL;
	else
	{
		if (phase == PRIMAL_PHASEI)
			EGlpNumCopy (d, lp->pIdz[pr->eindex]);
		else
			EGlpNumCopy (d, lp->dz[pr->eindex]);
		vs = lp->vstat[lp->nbaz[pr->eindex]];

		pr->price_stat = PRICE_NONOPTIMAL;
		if (vs == STAT_UPPER || (vs == STAT_ZERO &&
														 EGlpNumIsLess (lp->tol->dfeas_tol, d)))
			pr->dir = VDECREASE;
		else
			pr->dir = VINCREASE;
	}
	EGlpNumClearVar (d);
}

static void update_p_scaleinf (price_info * const p,
															 heap * const h,
															 int const i,
															 EGlpNum_t inf,
															 int const prule)
{
	if (EGlpNumIsEqqual (inf, zeroLpNum))
	{
		EGlpNumZero (p->p_scaleinf[i]);
		if (h->hexist != 0 && h->loc[i] != -1)
			ILLheap_delete (h, i);
	}
	else
	{
		if (prule == QS_PRICE_DDANTZIG)
			EGlpNumCopy (p->p_scaleinf[i], inf);
		else if (prule == QS_PRICE_DSTEEP)
			EGlpNumCopySqrOver (p->p_scaleinf[i], inf, p->dsinfo.norms[i]);
		else if (prule == QS_PRICE_DDEVEX)
			EGlpNumCopySqrOver (p->p_scaleinf[i], inf, p->ddinfo.norms[i]);

		if (h->hexist != 0)
		{
			if (h->loc[i] == -1)
				ILLheap_insert (h, i);
			else
				ILLheap_modify (h, i);
		}
	}
}

static void compute_primalI_inf (lpinfo * const lp,
																 int const i,
																 EGlpNum_t * const inf)
{
	int const col = lp->baz[i];
	EGlpNumZero (*inf);
	if (EGlpNumIsLess (lp->tol->ip_tol, lp->xbz[i]) &&
			EGlpNumIsNeqq (lp->uz[col], INFTY))
		EGlpNumCopy (*inf, lp->xbz[i]);
	else if (EGlpNumIsNeqq (lp->lz[col], NINFTY) &&
					 EGlpNumIsSumLess (lp->xbz[i], lp->tol->ip_tol, zeroLpNum))
		EGlpNumCopyAbs (*inf, lp->xbz[i]);
}

static void compute_primalII_inf (lpinfo * const lp,
																	int const i,
																	EGlpNum_t * const inf)
{
	int const col = lp->baz[i];
	EGlpNumZero (*inf);
	if (EGlpNumIsNeqq (lp->uz[col], INFTY) &&
			EGlpNumIsSumLess (lp->uz[col], lp->tol->pfeas_tol, lp->xbz[i]))
		EGlpNumCopyDiff (*inf, lp->xbz[i], lp->uz[col]);
	else if (EGlpNumIsNeqq (lp->lz[col], NINFTY) &&
					 EGlpNumIsSumLess (lp->xbz[i], lp->tol->pfeas_tol, lp->lz[col]))
		EGlpNumCopyDiff (*inf, lp->lz[col], lp->xbz[i]);
}

void ILLprice_compute_primal_inf (lpinfo * const lp,
																	price_info * const p,
																	int *const ix,
																	int const icnt,
																	int const phase)
{
	heap *const h = &(p->h);
	int const price = (phase == DUAL_PHASEI) ? p->dI_price : p->dII_price;
	int i;
	EGlpNum_t inf;
	EGlpNumInitVar (inf);
	EGlpNumZero (inf);

	if (phase == DUAL_PHASEI)
	{
		if (ix == NULL)
			for (i = 0; i < lp->nrows; i++)
			{
				compute_primalI_inf (lp, i, &inf);
				update_p_scaleinf (p, h, i, inf, price);
			}
		else
			for (i = 0; i < icnt; i++)
			{
				compute_primalI_inf (lp, ix[i], &inf);
				update_p_scaleinf (p, h, ix[i], inf, price);
			}
	}
	else if (phase == DUAL_PHASEII)
	{
		if (ix == NULL)
			for (i = 0; i < lp->nrows; i++)
			{
				compute_primalII_inf (lp, i, &inf);
				update_p_scaleinf (p, h, i, inf, price);
			}
		else
			for (i = 0; i < icnt; i++)
			{
				compute_primalII_inf (lp, ix[i], &inf);
				update_p_scaleinf (p, h, ix[i], inf, price);
			}
	}
	EGlpNumClearVar (inf);
}

void ILLprice_dual (lpinfo * const lp,
										price_info * const pinf,
										int const phase,
										price_res * const pr)
{
	heap *const h = &(pinf->h);
	int i;
	EGlpNum_t d;
	EGlpNumInitVar (d);
	EGlpNumZero (d);
	pr->lindex = -1;

#if USEHEAP > 0
	ILLprice_test_for_heap (lp, pinf, lp->nrows, pinf->p_scaleinf, DUAL_SIMPLEX,
													1);
#endif

	if (pinf->d_strategy == COMPLETE_PRICING)
	{
		if (h->hexist)
		{
			pr->lindex = ILLheap_findmin (h);
			if (pr->lindex != -1)
				ILLheap_delete (h, pr->lindex);
		}
		else
		{
			for (i = 0; i < lp->nrows; i++)
			{
				if (EGlpNumIsLess (d, pinf->p_scaleinf[i]))
				{
					EGlpNumCopy (d, pinf->p_scaleinf[i]);
					pr->lindex = i;
				}
			}
		}
	}
	else if (pinf->d_strategy == MULTI_PART_PRICING)
	{
		for (i = 0; i < pinf->dmpinfo.bsize; i++)
		{
			if (EGlpNumIsLess (d, pinf->dmpinfo.infeas[i]))
			{
				EGlpNumCopy (d, pinf->dmpinfo.infeas[i]);
				pr->lindex = pinf->dmpinfo.bucket[i];
			}
		}
	}

	if (pr->lindex < 0)
		pr->price_stat = PRICE_OPTIMAL;
	else
	{
		pr->price_stat = NONOPTIMAL;

		if (EGlpNumIsNeqq (lp->uz[lp->baz[pr->lindex]], INFTY))
		{
			if (phase == DUAL_PHASEI)
				EGlpNumZero (d);
			else
				EGlpNumCopy (d, lp->uz[lp->baz[pr->lindex]]);
			if (EGlpNumIsSumLess (lp->tol->pfeas_tol, d, lp->xbz[pr->lindex]))
				pr->lvstat = STAT_UPPER;
			else
				pr->lvstat = STAT_LOWER;
		}
		else
			pr->lvstat = STAT_LOWER;
	}
	EGlpNumClearVar (d);
}

int ILLprice_get_rownorms (lpinfo * const lp,
													 price_info * const pinf,
													 EGlpNum_t * const rnorms)
{
	int rval = 0;
	int i;

	if (pinf->dsinfo.norms == NULL)
	{
		rval = ILLprice_build_dsteep_norms (lp, &(pinf->dsinfo));
		ILL_CLEANUP_IF (rval);
	}
	for (i = 0; i < lp->nrows; i++)
		EGlpNumCopy (rnorms[i], pinf->dsinfo.norms[i]);

CLEANUP:
	if (rval)
		EGlpNumFreeArray (pinf->dsinfo.norms);
	return rval;
}

int ILLprice_get_colnorms (lpinfo * const lp,
													 price_info * const pinf,
													 EGlpNum_t * const cnorms)
{
	int rval = 0;
	int i,
	  j;

	if (pinf->psinfo.norms == NULL)
	{
		rval = ILLprice_build_psteep_norms (lp, &(pinf->psinfo));
		ILL_CLEANUP_IF (rval);
	}
	for (i = 0; i < lp->nrows; i++)
		EGlpNumZero (cnorms[lp->baz[i]]);
	for (j = 0; j < lp->nnbasic; j++)
		EGlpNumCopy (cnorms[lp->nbaz[j]], pinf->psinfo.norms[j]);

CLEANUP:
	if (rval)
		EGlpNumFreeArray (pinf->psinfo.norms);
	return rval;
}

int ILLprice_get_newnorms (lpinfo * const lp,
													 int const nelems,
													 EGlpNum_t * const norms,
													 int *const matcnt,
													 int *const matbeg,
													 int *const matind,
													 EGlpNum_t * const matval,
													 int const option)
{
	int i,
	  j;
	int rval = 0;
	svector a;
	svector y;

	ILLsvector_init (&y);
	rval = ILLsvector_alloc (&y, lp->nrows);
	ILL_CLEANUP_IF (rval);

	for (j = 0; j < nelems; j++)
	{
		a.nzcnt = matcnt[j];
		a.indx = &(matind[matbeg[j]]);
		a.coef = &(matval[matbeg[j]]);

		if (option == COLUMN_SOLVE)
			ILLbasis_column_solve (lp, &a, &y);
		else
			ILLbasis_row_solve (lp, &a, &y);

		EGlpNumOne (norms[j]);
		for (i = 0; i < y.nzcnt; i++)
			EGlpNumAddInnProdTo (norms[j], y.coef[i], y.coef[i]);
	}

CLEANUP:
	ILLsvector_free (&y);
	ILL_RETURN (rval, "ILLprice_get_newnorms");
}

int ILLprice_get_new_rownorms (lpinfo * const lp,
															 int const newrows,
															 EGlpNum_t * const rnorms,
															 int *const rmatcnt,
															 int *const rmatbeg,
															 int *const rmatind,
															 EGlpNum_t * const rmatval)
{
	return ILLprice_get_newnorms (lp, newrows, rnorms, rmatcnt, rmatbeg, rmatind,
																rmatval, ROW_SOLVE);
}

int ILLprice_get_new_colnorms (lpinfo * const lp,
															 int const newrows,
															 EGlpNum_t * const rnorms,
															 int *const matcnt,
															 int *const matbeg,
															 int *const matind,
															 EGlpNum_t * const matval)
{
	return ILLprice_get_newnorms (lp, newrows, rnorms, matcnt, matbeg, matind,
																matval, COLUMN_SOLVE);
}

int ILLprice_load_rownorms (lpinfo * const lp,
														EGlpNum_t * const rnorms,
														price_info * const pinf)
{
	int i;
	int rval = 0;

	EGlpNumFreeArray (pinf->dsinfo.norms);
	pinf->dsinfo.norms = EGlpNumAllocArray (lp->nrows);
	for (i = 0; i < lp->nrows; i++)
	{
		EGlpNumCopy (pinf->dsinfo.norms[i], rnorms[i]);
		if (EGlpNumIsLess (pinf->dsinfo.norms[i], PARAM_MIN_DNORM))
			EGlpNumCopy (pinf->dsinfo.norms[i], PARAM_MIN_DNORM);
	}

	ILL_RETURN (rval, "ILLprice_load_rownorms");
}

int ILLprice_load_colnorms (lpinfo * const lp,
														EGlpNum_t * const cnorms,
														price_info * const pinf)
{
	int j;
	int rval = 0;

	EGlpNumFreeArray (pinf->psinfo.norms);
	pinf->psinfo.norms = EGlpNumAllocArray (lp->nnbasic);
	for (j = 0; j < lp->nnbasic; j++)
	{
		EGlpNumCopy (pinf->psinfo.norms[j], cnorms[lp->nbaz[j]]);
		if (EGlpNumIsLess (pinf->psinfo.norms[j], oneLpNum))
			EGlpNumOne (pinf->psinfo.norms[j]);
	}

	ILL_RETURN (rval, "ILLprice_load_colnorms");
}

#if PRICE_DEBUG > 0
void test_dsteep_norms (lpinfo * lp,
												price_info * p)
{
	int i,
	  errn = 0;
	EGlpNum_t err,
	  diff;
	EGlpNum_t *pn;
	EGlpNumInitVar (err);
	EGlpNumInitVar (diff);
	pn = EGlpNumAllocArray (lp->nrows);
	EGlpNumZero (err);

	ILLprice_get_dsteep_norms (lp, lp->yjz.nzcnt, lp->yjz.indx, pn);
	for (i = 0; i < lp->yjz.nzcnt; i++)
	{
		EGlpNumCopyDiff (diff, pn[i], p->dsinfo.norms[lp->yjz.indx[i]]);
		if (EGlpNumIsLess (diff, zeroLpNum))
			EGlpNumSign (diff);
		if (EGlpNumIsLess (PFEAS_TOLER, diff))
		{
			errn++;
			EGlpNumAddTo (err, diff);
			EGlpNumCopy (p->dsinfo.norms[lp->yjz.indx[i]], pn[i]);
		}
	}
	if (errn)
		printf ("%d: dnorm errn = %d, err = %.6f\n", lp->cnts->tot_iter, errn,
						EGlpNumToLf (err));
	EGlpNumFreeArray (pn);
	EGlpNumClearVar (diff);
	EGlpNumClearVar (err);
}
#endif
