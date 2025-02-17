/*
 * Copyright (c) 1990, 1991, 1992, 1993, 1994, 1995, 1996, 1997
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that: (1) source code distributions
 * retain the above copyright notice and this paragraph in its entirety, (2)
 * distributions including binary code include the above copyright notice and
 * this paragraph in its entirety in the documentation or other materials
 * provided with the distribution, and (3) all advertising materials mentioning
 * features or use of this software display the following acknowledgement:
 * ``This product includes software developed by the University of California,
 * Lawrence Berkeley Laboratory and its contributors.'' Neither the name of
 * the University nor the names of its contributors may be used to endorse
 * or promote products derived from this software without specific prior
 * written permission.
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "ntp.h"

#include "extract.h"

#define	JAN_1970	INT64_T_CONSTANT(2208988800)	/* 1970 - 1900 in seconds */

void
p_ntp_time(netdissect_options *ndo,
	   const struct l_fixedpt *lfp)
{
	uint32_t i;
	uint32_t uf;
	uint32_t f;
	double ff;

	i = GET_BE_U_4(lfp->int_part);
	uf = GET_BE_U_4(lfp->fraction);
	ff = uf;
	if (ff < 0.0)		/* some compilers are buggy */
		ff += FMAXINT;
	ff = ff / FMAXINT;			/* shift radix point by 32 bits */
	f = (uint32_t)(ff * 1000000000.0);	/* treat fraction as parts per billion */
	ND_PRINT(C_RESET, "%u.%09u", i, f);

#ifdef HAVE_STRFTIME
	/*
	 * print the UTC time in human-readable format.
	 */
	if (i) {
	    int64_t seconds_64bit = (int64_t)i - JAN_1970;
	    time_t seconds;
	    struct tm *tm;
	    char time_buf[128];

	    seconds = (time_t)seconds_64bit;
	    if (seconds != seconds_64bit) {
		/*
		 * It doesn't fit into a time_t, so we can't hand it
		 * to gmtime.
		 */
		ND_PRINT(C_RESET, " (unrepresentable)");
	    } else {
		tm = gmtime(&seconds);
		if (tm == NULL) {
		    /*
		     * gmtime() can't handle it.
		     * (Yes, that might happen with some version of
		     * Microsoft's C library.)
		     */
		    ND_PRINT(C_RESET, " (unrepresentable)");
		} else {
		    /* use ISO 8601 (RFC3339) format */
		    strftime(time_buf, sizeof (time_buf), "%Y-%m-%dT%H:%M:%SZ", tm);
		    ND_PRINT(C_RESET, " (%s)", time_buf);
		}
	    }
	}
#endif
}
