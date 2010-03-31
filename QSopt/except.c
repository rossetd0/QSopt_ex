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

/* RCSINFO $Id: exception.c,v 1.2 2003/11/05 16:47:22 meven Exp $ */
#include "except.h"
#include <stdio.h>
#include <string.h>
#ifdef USEDMALLOC
#include "dmalloc.h"
#endif


void ILL_report (const char *msg,
								 const char *fct,
								 const char *file,
								 unsigned int line,
								 int with_src_info)
{
	if (msg != NULL)
	{
		fprintf (stderr, "FAILURE: %s", msg);
		if (msg[strlen (msg) - 1] != '\n')
		{
			fprintf (stderr, "\n");
		}
		if (with_src_info == 1)
		{
			fprintf (stderr, "\t");
			if (fct != NULL)
			{
				fprintf (stderr, "in function %s ", fct);
			}
			fprintf (stderr, "in file %s line %d", file, line);
		}
		fprintf (stderr, ".\n");
	}
}
