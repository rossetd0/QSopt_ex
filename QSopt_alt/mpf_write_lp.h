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

/*  RCS_INFO = "$RCSfile: wr_lp.h,v $ $Revision: 1.2 $ $Date: 2003/11/05 16:57:39 $"; */
#ifndef mpf_WRITE_LP_STATE_H
#define mpf_WRITE_LP_STATE_H

/****************************************************************************/
/*                                                                          */
/*               Routines to support writing of LP files                    */
/*                                                                          */
/****************************************************************************/

/* 
 * -) anything after '\' is comment 
 * -) variables consist of a-z A-Z 0-9!"#$%(),;.?@_`'{}|~ 
 *    don't start with a digit or '.'
 */
#include "mpf_iqsutil.h"

typedef struct mpf_ILLwrite_lp_state
{
	char buf[ILL_namebufsize];
	char *p;
	int startlen;
	int total;
}
mpf_ILLwrite_lp_state;

extern void mpf_ILLwrite_lp_state_init (mpf_ILLwrite_lp_state * line,
																		const char *str);
extern void mpf_ILLwrite_lp_state_append (mpf_ILLwrite_lp_state * line,
																			const char *str);
extern void mpf_ILLwrite_lp_state_append_coef (mpf_ILLwrite_lp_state * line,
																					 mpf_t v,
																					 int cnt);
			/* append number sign ('+', '-') iff cnt > 0 or v < 0.0  
			 * append number iff v != 1.0, v != -1.0  
			 */
extern void mpf_ILLwrite_lp_state_append_number (mpf_ILLwrite_lp_state * line,
																						 mpf_t v);
extern void mpf_ILLwrite_lp_state_save_start (mpf_ILLwrite_lp_state * line);
extern void mpf_ILLwrite_lp_state_start (mpf_ILLwrite_lp_state * line);

#endif
