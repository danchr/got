#ifndef _OPENBSD_COMPAT_UTIL_H_
#define _OPENBSD_COMPAT_UTIL_H_

#ifndef __APPLE__
#include <libutil.h>

/*
 * fmt_scaled(3) specific flags.
 */
#define FMT_SCALED_STRSIZE	7	/* minus sign, 4 digits, suffix, null byte */

#define fmt_scaled(number, result) ({					\
	int64_t num = number;						\
	int r = humanize_number(result,					\
	    FMT_SCALED_STRSIZE - (num < 0 ? 0 : 1),			\
	    num, "", HN_AUTOSCALE, HN_NOSPACE | HN_B);			\
	r == -1 ? -1 : 0;						\
})

#else
#include_next <util.h>

#define FMT_SCALED_STRSIZE      7
int     fmt_scaled(long long number, char *result);
#endif
#endif
