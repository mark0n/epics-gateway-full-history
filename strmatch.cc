/*-
 * Copyright (c) 1991 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Kenneth Almquist.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/* Modified by Claude Saunders, APS Controls Group, Argonne National Lab */
/* Extracted patmatch function from expand.c source of ksh */

#ifndef lint
static char sccsid[] = "@(#)strmatch.c	1.2 8/19/93";
#endif /* not lint */
/* $Log$
 * Revision 1.1  1996/07/26 02:34:48  jbk
 * Interum step.
 *
 * Revision 1.2  1994/05/11  20:16:31  saunders
 * First fully functional version added to cvs addon environment.
 * */

/*
 * Routines to expand arguments to commands.  We have to deal with
 * backquotes, shell variables, and file metacharacters.
 */

#include <sys/types.h>
#include <errno.h>

static int pmatch(char *, char *);

/*
 * Returns true if the pattern matches the string.
 */

int patmatch(char *pattern, char *string)
{
	if (pattern[0] == '!' && pattern[1] == '!')
		return 1 - pmatch(pattern + 2, string);
	else
		return pmatch(pattern, string);
}


static int pmatch(char* pattern, char* string)
{
	register char *p, *q;
	register char c;

	p = pattern;
	q = string;
	for (;;) {
		switch (c = *p++) {
		case '\0':
			goto breakloop;
		case '?':
			if (*q++ == '\0')
				return 0;
			break;
		case '*':
			c = *p;
			if (c != '?' && c != '*' && c != '[') {
				while (*q != c) {
					if (*q == '\0')
						return 0;
					q++;
				}
			}
			do {
				if (pmatch(p, q))
					return 1;
			} while (*q++ != '\0');
			return 0;
		case '[': {
			char *endp;
			int invert, found;
			char chr;

			endp = p;
			if (*endp == '!')
				endp++;
			for (;;) {
				if (*endp == '\0')
					goto dft;		/* no matching ] */
				if (*++endp == ']')
					break;
			}
			invert = 0;
			if (*p == '!') {
				invert++;
				p++;
			}
			found = 0;
			chr = *q++;
			c = *p++;
			do {
			  if (*p == '-' && p[1] != ']') {
					p++;
					if (chr >= c && chr <= *p)
						found = 1;
					p++;
				} else {
					if (chr == c)
						found = 1;
				}
			} while ((c = *p++) != ']');
			if (found == invert)
				return 0;
			break;
		}
dft:	    default:
			if (*q++ != c)
				return 0;
			break;
		}
	}
breakloop:
	if (*q != '\0')
		return 0;
	return 1;
}

