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

/* RCS_INFO = "$RCSfile: read_lp_state.c,v $ $Revision: 1.2 $ $Date:
   2003/11/05 16:49:52 $"; */

/****************************************************************************/
/* */
/* Routines to support Reading LP Files                       */
/* */
/****************************************************************************/

/* -) anything after '\' is comment -) variables consist of a-z A-Z
   0-9!"#$%(),;.?@_`'{}|~ don't start with a digit or '.' */

#include "econfig.h"
#include "mpf_iqsutil.h"
#include "mpf_read_lp.h"
#include "mpf_lp.h"
#include "mpf_rawlp.h"
#include "mpf_lpdefs.h"
#include "mpf_format.h"
#ifdef USEDMALLOC
#include "dmalloc.h"
#endif
static int TRACE = 0;

#define mpf_END_LINE(p)  (((*p) == '\\' || (*p) == '\n' || (*p) == '\0') ? 1 : 0)

static const char *mpf_all_keyword[] = {
    "MIN", "MINIMUM", "MINIMIZE",
    "MAX", "MAXIMUM", "MAXIMIZE",
    "SUBJECT", "ST", "PROBLEM", "PROB",
    "BOUNDS", "BOUND", "INTEGER", "END", NULL
};
static int mpf_all_keyword_len[] = {
    3, 7, 8,
    3, 7, 8,
    7, 2, 7, 4,
    6, 5, 7, 3, -1
};

int mpf_ILLread_lp_state_init (mpf_ILLread_lp_state * state,
      mpf_qsline_reader * file,
      const char *mpf_fname,
      int inter)
{
    int rval = 0;
    ILL_FAILtrue (file == NULL, "need a file");
    state->eof = 0;
    state->file_name = mpf_fname;
    state->mpf_interactive = inter;
    state->file = file;
    state->line_num = 0;
    state->p = state->line;
    state->line[0] = '\0';
    state->realline[0] = '\0';
    state->field[0] = '\0';
    state->fieldOnFirstCol = 0;
    mpf_EGlpNumInitVar (state->bound_val);
    mpf_ILLread_lp_state_skip_blanks (state, 1);
CLEANUP:
    ILL_RETURN (rval, "mpf_ILLread_lp_state_init");
}

int mpf_ILLread_lp_state_next_line (mpf_ILLread_lp_state * state)
{
    char *slash;
    if (state->eof) {
	return 1;
    }
    state->line[0] = '\0';
    if (state->mpf_interactive) {
	fprintf (stdout, "> ");
	fflush (stdout);
    }
    while (mpf_ILLline_reader_get (state->realline, ILL_namebufsize - 2, state->file)
	!= (char *) NULL) {
	state->p = state->line;
	state->line_num++;
	strcpy (state->line, state->realline);
	slash = strchr (state->line, '\\');
	if (slash != NULL) {
	    *slash = '\0';
	}
	while (mpf_ILL_ISBLANK (state->p)) {
	    state->p++;
	}
	if (!mpf_END_LINE (state->p)) {
	    ILL_IFTRACE ("NEWLINE %s %d: %s",
		state->file_name, state->line_num, state->line);
	    return 0;
	}
	if (state->mpf_interactive) {
	    fprintf (stdout, "> ");
	    fflush (stdout);
	}
    }
    state->eof = 1;
    state->line_num++;
    state->field[0] = '\0';
    state->line[0] = '\0';
    strcpy (state->realline, "\n");
    state->p = state->line;
    state->fieldOnFirstCol = 0;
    return 1;
}

int mpf_ILLread_lp_state_skip_blanks (mpf_ILLread_lp_state * state,
      int wrapLines)
{
    while (1) {
	while (mpf_ILL_ISBLANK (state->p)) {
	    state->p++;
	}
	if (mpf_END_LINE (state->p)) {
	    if (wrapLines) {
		if (mpf_ILLread_lp_state_next_line (state) != 0) {
		    return 1;
		}
	    } else {
		return 0;	/* done */
	    }
	} else {
	    return 0;		/* foud non blank */
	}
    }
}

static int mpf_next_field (mpf_ILLread_lp_state * state,
      int acrossLines)
{
    (void) mpf_ILLread_lp_state_skip_blanks (state, (char) acrossLines);
    if (state->eof) {
	return 1;
    }
    state->fieldOnFirstCol = (state->line == state->p);
    if (sscanf (state->p, "%s", state->field) != EOF) {
	state->p += strlen (state->field);
	return 0;
    }
    return 1;
}

int mpf_ILLread_lp_state_next_field_on_line (mpf_ILLread_lp_state * state)
{
    return mpf_next_field (state, 0);
}

int mpf_ILLread_lp_state_next_field (mpf_ILLread_lp_state * state)
{
    return mpf_next_field (state, 1);
}

void mpf_ILLread_lp_state_prev_field (mpf_ILLread_lp_state * state)
{
    if (state->p > state->line) {
	state->p--;
    }
    while (mpf_ILL_ISBLANK (state->p) && (state->p > state->line)) {
	state->p--;
    }
    while (!mpf_ILL_ISBLANK (state->p) && (state->p > state->line)) {
	state->p--;
    }
    state->fieldOnFirstCol = (state->line == state->p);
}

int mpf_ILLread_lp_state_next_var (mpf_ILLread_lp_state * state)
{
    char *p;
    int var_len, i;

    if (mpf_ILLread_lp_state_skip_blanks (state, 1)) {
	return 1;
    }
    state->fieldOnFirstCol = (state->line == state->p);
    var_len = 0;
    p = state->p;
    while (1) {
	if (mpf_ILLis_lp_name_char (*p, var_len)) {
	    p++;
	    var_len++;
	} else {
	    break;
	}
    }
    if (var_len == 0) {
	return 1;
    }
    if (state->fieldOnFirstCol) {
	/* see whether we founbd a reserved keyword */
	for (i = 0; mpf_all_keyword[i] != NULL; i++) {
	    if ((var_len == mpf_all_keyword_len[i]) &&
		(strncasecmp (mpf_all_keyword[i], state->p, (size_t) (mpf_all_keyword_len[i])) == 0)) {
		return -1;	/* yes we did */
	    }
	}
    }
    strncpy (state->field, state->p, (size_t) var_len);
    state->field[var_len] = '\0';
    state->p = p;
    return 0;
}

int mpf_ILLread_lp_state_bad_keyword (mpf_ILLread_lp_state * state)
{
    if (!state->fieldOnFirstCol) {
	return mpf_ILLlp_error (state,
	    "Keyword \"%s\" not at beginning of line.\n",
	    state->field);
    }
    return 0;
}

int mpf_ILLtest_lp_state_keyword (mpf_ILLread_lp_state * state,
      const char *kwd[])
{
    int i = 0;
    if (!state->eof && state->fieldOnFirstCol) {
	for (i = 0; kwd[i] != NULL; i++) {
	    if (strcasecmp (state->field, kwd[i]) == 0) {
		return 0;
	    }
	}
    }
    return 1;
}

int mpf_ILLread_lp_state_keyword (mpf_ILLread_lp_state * state,
      const char *kwd[])
{
    if (state->eof || mpf_ILLread_lp_state_bad_keyword (state)) {
	return 1;
    }
    return mpf_ILLtest_lp_state_keyword (state, kwd);
}


int mpf_ILLread_lp_state_colon (mpf_ILLread_lp_state * state)
{
    if ((mpf_ILLread_lp_state_skip_blanks (state, 1) == 0) && (*state->p == ':')) {
	state->p++;
	return 0;
    }
    return 1;
}

int mpf_ILLread_lp_state_has_colon (mpf_ILLread_lp_state * state)
{
    char *pp;
    mpf_ILLread_lp_state_skip_blanks (state, 0);
    for (pp = state->p; *pp != '\n'; pp++) {
	if (*pp == ':') {
	    return 1;
	}
    }
    return 0;
}

int mpf_ILLread_lp_state_next_constraint (mpf_ILLread_lp_state * state)
{
    int rval;
    int ln = state->line_num;

    mpf_ILLread_lp_state_skip_blanks (state, 1);
    if (state->eof) {
	return 1;
    }
    if (ln == state->line_num) {
	return mpf_ILLlp_error (state, "Constraints must start on a new line.\n");
    }
    if (mpf_ILLread_lp_state_next_field (state) == 0) {
	rval = mpf_ILLtest_lp_state_keyword (state, mpf_all_keyword);
	mpf_ILLread_lp_state_prev_field (state);
	return !rval;
    }
    return 0;
}

/* return 0 if there is a sign */
int mpf_ILLread_lp_state_sign (mpf_ILLread_lp_state * state,
      mpf_t * sign)
{
    char found = 0;
    mpf_EGlpNumOne (*sign);
    if (mpf_ILLread_lp_state_skip_blanks (state, 1) == 0) {
	if ((*state->p == '+') || (*state->p == '-')) {
	    if (*state->p != '+')
		mpf_EGlpNumSign (*sign);
	    state->p++;
	    found = 1;
	}
    }
    return 1 - found;
}

int mpf_ILLtest_lp_state_next_is (mpf_ILLread_lp_state * state,
      const char *str)
{
    mpf_ILLread_lp_state_skip_blanks (state, 0);
    if (strncasecmp (state->p, str, strlen (str)) == 0) {
	state->p += strlen (str);
	return 1;
    }
    return 0;
}

int mpf_ILLread_lp_state_value (mpf_ILLread_lp_state * state,
      mpf_t * coef)
{
    int len = 0;

    if (mpf_ILLread_lp_state_skip_blanks (state, 1) != 0) {
	ILL_RESULT (1, "mpf_ILLread_lp_state_value");
    } else {
	state->fieldOnFirstCol = (state->line == state->p);
	len = mpf_ILLget_value (state->p, coef);
	if (len > 0) {
	    state->p += len;
	    ILL_RESULT (0, "mpf_ILLread_lp_state_value");
	}
	ILL_RESULT (1, "mpf_ILLread_lp_state_value");
    }
}

int mpf_ILLread_lp_state_possible_coef (mpf_ILLread_lp_state * state,
      mpf_t * coef,
      mpf_t defValue)
{
    mpf_EGlpNumCopy (*coef, defValue);
    return mpf_ILLread_lp_state_value (state, coef);
}


int mpf_ILLread_lp_state_possible_bound_value (mpf_ILLread_lp_state * state)
{
    mpf_t sign;
    int len = 0;
    char *p = NULL;
    int rval = 0;
    mpf_EGlpNumInitVar (sign);
    (void) mpf_ILLread_lp_state_sign (state, &sign);

    if (!strncasecmp (state->p, "INFINITY", (size_t) 8)) {
	len = 8;
    } else {
	if (!strncasecmp (state->p, "INF", (size_t) 3)) {
	    len = 3;
	}
    }
    if (len > 0) {
	state->p += len;
	p = state->p;
	mpf_ILLread_lp_state_skip_blanks (state, 0);
	if (!mpf_END_LINE (p) && p == state->p) {
	    /* found no blanks so this INF/INFINITY is the prefix of
	       something else */
	    state->p -= len;
	    goto CLEANUP;
	    return 0;		/* no coef found */
	} else {
	    if (mpf_EGlpNumIsLess (sign, mpf_zeroLpNum))
		mpf_EGlpNumCopy (state->bound_val, mpf_ILL_MINDOUBLE);
	    else if (mpf_EGlpNumIsLess (mpf_zeroLpNum, sign))
		mpf_EGlpNumCopy (state->bound_val, mpf_ILL_MAXDOUBLE);
	    else
		mpf_EGlpNumZero (state->bound_val);
	    rval = 1;
	    goto CLEANUP;
	}
    }
    if (mpf_ILLread_lp_state_value (state, &(state->bound_val)) == 0) {
	mpf_EGlpNumMultTo (state->bound_val, sign);
	rval = 1;
	goto CLEANUP;
    }
CLEANUP:
    mpf_EGlpNumClearVar (sign);
    return rval;		/* no coef found */
}

int mpf_ILLtest_lp_state_sense (mpf_ILLread_lp_state * state,
      int all)
{
    char c;

    state->sense_val = ' ';
    if (mpf_ILLread_lp_state_skip_blanks (state, 1) == 0) {
	c = *state->p;
	if (!all) {		/* look for '=' and '<=' */
	    if (c == '=') {
		state->p++;
		state->sense_val = 'E';
	    } else {
		if ((c == '<') && (*(state->p + 1) == '=')) {
		    state->p += 2;
		    state->sense_val = 'L';
		}
	    }
	} else {
	    c = *state->p;
	    if ((c == '<') || (c == '>')) {
		state->sense_val = (c == '<') ? 'L' : 'G';
		state->p++;
		c = *state->p;
		if (*state->p == '=') {
		    state->p++;
		}
	    } else {
		if (c == '=') {
		    state->p++;
		    c = *state->p;
		    if ((c == '<') || (c == '>')) {
			state->sense_val = (c == '<') ? 'L' : 'G';
			state->p++;
		    } else {
			state->sense_val = 'E';
		    }
		}
	    }
	}
    }
    return (state->sense_val != ' ');
}

void mpf_ILLtest_lp_state_bound_sense (mpf_ILLread_lp_state * state)
{
    (void) mpf_ILLtest_lp_state_sense (state, 0);
}

int mpf_ILLread_lp_state_sense (mpf_ILLread_lp_state * state)
{
    if (!mpf_ILLtest_lp_state_sense (state, 1)) {
	if (mpf_END_LINE (state->p)) {
	    return mpf_ILLlp_error (state, "Missing row sense at end of line.\n");
	} else {
	    if (*state->p != '\0') {
		return mpf_ILLlp_error (state, "\"%c\" is not a row sense.\n", *state->p);
	    } else {
		return mpf_ILLlp_error (state, "Missing row sense at end of line.\n");
	    }
	}
    }
    return 0;
}

/* ------------------------------------------------------------------------- */
/* error printing */

static void mpf_ILLread_lp_state_print_at (mpf_ILLread_lp_state * state)
{
    char *p;
    if (state->eof) {
	fprintf (stderr, "end of file");
    } else {
	if (*state->p == '\n') {
	    fprintf (stderr, "end of line");
	} else {
	    p = state->p;
	    while (mpf_ILL_ISBLANK (p)) {
		p++;
	    }
	    fprintf (stderr, "%c", '"');
	    for (; !mpf_ILL_ISBLANK (p) && !mpf_END_LINE (p); p++) {
		fprintf (stderr, "%c", *p);
	    }
	    fprintf (stderr, "\"");
	}
    }
}

static void mpf_lp_err (mpf_ILLread_lp_state * state,
      int isError,
      const char *format,
      va_list args)
{
    int rval = 0;
    int errtype, slen, at;
    mpf_qsformat_error error;
    char error_desc[256];

    ILL_FAILfalse (state != NULL, "state != NULL");
    ILL_FAILfalse (state->file != NULL, "state->file != NULL");
    ILL_FAILfalse (format != NULL, "format != NULL");
    ILL_FAILfalse (format[0] != '\0', "format[0] != '0'");

    mpf_ILLread_lp_state_skip_blanks (state, 0);
    at = state->p - state->line;
    vsprintf (error_desc, format, args);
    slen = strlen (error_desc);
    if ((slen > 0) && error_desc[slen - 1] != '\n') {
	error_desc[slen] = '\n';
	error_desc[slen + 1] = '\0';
    }
    if (state->file->error_collector != NULL) {
	errtype = (isError) ? QS_LP_FORMAT_ERROR : QS_LP_FORMAT_WARN;
	mpf_ILLformat_error_create (&error, errtype, error_desc,
	    state->line_num, state->realline, at);
	mpf_ILLformat_error (state->file->error_collector, &error);
	mpf_ILLformat_error_delete (&error);
    } else {
	if (!state->mpf_interactive) {
	    fprintf (stderr, "%s %d: %s\t", state->file_name, state->line_num,
		state->realline);
	    fprintf (stderr, "%s at ", (isError) ? "LP Error" : "LP Warning");
	    mpf_ILLread_lp_state_print_at (state);
	    fprintf (stderr, ": ");
	} else {
	    fprintf (stderr, "%s : ", (isError) ? "LP Error" : "LP Warning");
	}
	fprintf (stderr, error_desc);
	fflush (stderr);
    }
CLEANUP:;
}

int mpf_ILLlp_error (mpf_ILLread_lp_state * state,
      const char *format,
    ...)
{
    va_list args;
    va_start (args, format);
    mpf_lp_err (state, mpf_TRUE, format, args);
    return 1;
}

void mpf_ILLlp_warn (mpf_ILLread_lp_state * state,
      const char *format,
    ...)
{
    va_list args;
    va_start (args, format);
    if (format != NULL) {
	mpf_lp_err (state, mpf_FALSE, format, args);
    }
}

/* shared with read_mps_state.c */
int mpf_ILLget_value (char *line,
      mpf_t * coef)
{
#ifdef mpq_READ_LP_STATE_H
    mpq_t res;
    int rval = 0;
    mpq_init (res);
    rval = mpq_EGlpNumReadStrXc (res, line);
    if (rval == 0)
	mpq_set_ui (*coef, 1UL, 1UL);
    else
	mpq_set (*coef, res);
    mpq_clear (res);
    return rval;
    /* return mpq_EGlpNumReadStrXc (*coef, line); */
#else
    char field[ILL_namebufsize];
    int rval = 0, i;
    char c, lastC, *p;
    int allowDot, allowExp, allowSign;
    double dtmp;

    p = line;
    c = *p;
    i = 0;
    lastC = ' ';
    allowSign = 1;
    allowExp = 0;
    allowDot = 1;
    while ((('0' <= c) && (c <= '9')) ||
	(allowDot && (c == '.')) ||
	((allowExp == 1) && ((c == 'e') || (c == 'E'))) ||
	((allowSign || lastC == 'e' || lastC == 'E') &&
	    ((c == '+') || (c == '-')))) {
	if (c == '.')
	    allowDot = 0;
	allowSign = 0;

	if ((allowExp == 0) && (c >= '0') && (c <= '9')) {
	    allowExp = 1;
	}
	if ((c == 'e') || (c == 'E')) {
	    allowExp++;
	    allowDot = 0;
	}
	p++;
	lastC = c;
	c = *p;
	i++;
    }
    if ((lastC == '+') || (lastC == '-')) {
	p--;
	i--;
	if (p > line)
	    lastC = *(p - 1);
	else
	    lastC = ' ';
    }
    if ((lastC == 'e') || (lastC == 'E')) {
	p--;
	i--;
    }
    if (i > 0) {
	strncpy (field, line, (size_t) i);
	field[i] = '\0';
	rval = !sscanf (field, "%lf%n", &dtmp, &i);
	mpf_EGlpNumSet (*coef, dtmp);
	ILL_IFTRACE ("%la\n", mpf_EGlpNumToLf (*coef));
	if (rval != 0) {
	    ILL_RESULT (0, "mpf_ILLget_value");
	}
    }
    /* ILL_RESULT (i, "mpf_ILLget_value"); */
    return i;
#endif
}

int mpf_ILLcheck_subject_to (mpf_ILLread_lp_state * state)
{
    int rval;
    char *p;
    if ((rval = mpf_ILLread_lp_state_next_field (state)) == 0) {
	if (strcasecmp (state->field, "ST") == 0) {
	    rval = mpf_ILLread_lp_state_bad_keyword (state);
	} else {
	    if (strcasecmp (state->field, "SUBJECT") == 0) {
		p = state->p;
		while (mpf_ILL_ISBLANK (p)) {
		    p++;
		}
		if (!strncasecmp (p, "TO", (size_t) 2)) {
		    rval = mpf_ILLread_lp_state_bad_keyword (state);
		    if (rval == 0) {
			state->p = p + 2;
		    }
		}
	    } else {
		rval = 1;
	    }
	}
	if (rval != 0) {
	    mpf_ILLread_lp_state_prev_field (state);
	} else {
	    mpf_ILLread_lp_state_skip_blanks (state, 1);
	}
    }
    ILL_RESULT (rval, "check_subject_to");
}
