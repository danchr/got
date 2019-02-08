/*	$OpenBSD: diff3.c,v 1.41 2016/10/18 21:06:52 millert Exp $	*/

/*
 * Copyright (C) Caldera International Inc.  2001-2002.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code and documentation must retain the above
 *    copyright notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed or owned by Caldera
 *	International, Inc.
 * 4. Neither the name of Caldera International, Inc. nor the names of other
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 *
 * USE OF THE SOFTWARE PROVIDED FOR UNDER THIS LICENSE BY CALDERA
 * INTERNATIONAL, INC. AND CONTRIBUTORS ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL CALDERA INTERNATIONAL, INC. BE LIABLE FOR ANY DIRECT,
 * INDIRECT INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */
/*-
 * Copyright (c) 1991, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
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
 *
 *	@(#)diff3.c	8.1 (Berkeley) 6/6/93
 */

#include <sys/stat.h>
#include <sys/queue.h>

#include <ctype.h>
#include <limits.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "got_error.h"
#include "got_opentemp.h"
#include "got_object.h"

#include "buf.h"
#include "rcsutil.h"
#include "got_lib_diff.h"
#include "worklist.h"

#ifndef nitems
#define nitems(_a) (sizeof(_a) / sizeof((_a)[0]))
#endif

/* diff3 - 3-way differential file comparison */

/* diff3 [-ex3EX] d13 d23 f1 f2 f3 [m1 m3]
 *
 * d13 = diff report on f1 vs f3
 * d23 = diff report on f2 vs f3
 * f1, f2, f3 the 3 files
 * if changes in f1 overlap with changes in f3, m1 and m3 are used
 * to mark the overlaps; otherwise, the file names f1 and f3 are used
 * (only for options E and X).
 */

/*
 * "from" is first in range of changed lines; "to" is last+1
 * from=to=line after point of insertion for added lines.
 */
struct range {
	int from;
	int to;
};

struct diff {
	struct range old;
	struct range new;
};

struct diff3_state {
	size_t szchanges;

	struct diff *d13;
	struct diff *d23;

	/*
	 * "de" is used to gather editing scripts.  These are later spewed out
	 * in reverse order.  Its first element must be all zero, the "new"
	 * component of "de" contains line positions or byte positions
	 * depending on when you look (!?).  Array overlap indicates which
	 * sections in "de" correspond to lines that are different in all
	 * three files.
	 */
	struct diff *de;
	char *overlap;
	int overlapcnt;
	FILE *fp[3];
	int cline[3];		/* # of the last-read line in each file (0-2) */

	/*
	 * the latest known correspondence between line numbers of the 3 files
	 * is stored in last[1-3];
	 */
	int last[4];
	int eflag;
	int oflag;
	int debug;
	char f1mark[PATH_MAX], f3mark[PATH_MAX]; /* markers for -E and -X */

	char *buf;
	size_t bufsize;
};

static BUF *diffbuf;

static int duplicate(struct range *, struct range *, struct diff3_state *);
static int edit(struct diff *, int, int, struct diff3_state *);
static char *getchange(FILE *, struct diff3_state *);
static char *get_line(FILE *, size_t *, struct diff3_state *);
static int number(char **);
static const struct got_error *readin(size_t *, char *, struct diff **,
    struct diff3_state *);
static int ed_patch_lines(struct rcs_lines *, struct rcs_lines *);
static int skip(int, int, char *, struct diff3_state *);
static int edscript(int, struct diff3_state *);
static int merge(size_t, size_t, struct diff3_state *);
static void change(int, struct range *, int, struct diff3_state *);
static void keep(int, struct range *, struct diff3_state *);
static void prange(struct range *);
static void repos(int, struct diff3_state *);
static void separate(const char *);
static const struct got_error *increase(struct diff3_state *);
static const struct got_error *diff3_internal(char *, char *, char *,
    char *, char *, const char *, const char *, struct diff3_state *);

int diff3_conflicts = 0;

static const struct got_error *
diff_output(const char *fmt, ...)
{
	va_list vap;
	int i;
	char *str;
	size_t newsize;

	va_start(vap, fmt);
	i = vasprintf(&str, fmt, vap);
	va_end(vap);
	if (i == -1)
		return got_error_from_errno();
	if (diffbuf != NULL)
		buf_append(&newsize, diffbuf, str, strlen(str));
	else
		printf("%s", str);
	free(str);
	return NULL;
}

static const struct got_error*
diffreg(BUF **d, const char *path1, const char *path2)
{
	const struct got_error *err = NULL;
	FILE *f1 = NULL, *f2 = NULL, *outfile = NULL;
	char *outpath = NULL;
	struct got_diff_state ds;
	struct got_diff_args args;
	int res;

	*d = NULL;

	f1 = fopen(path1, "r");
	if (f1 == NULL) {
		err = got_error_from_errno();
		goto done;
	}
	f2 = fopen(path2, "r");
	if (f1 == NULL) {
		err = got_error_from_errno();
		goto done;
	}

	err = got_opentemp_named(&outpath, &outfile, "/tmp/got-diff");
	if (err)
		goto done;

	memset(&ds, 0, sizeof(ds));
	/* XXX should stat buffers be passed in args instead of ds? */
	if (stat(path1, &ds.stb1) == -1) {
		err = got_error_from_errno();
		goto done;
	}
	if (stat(path2, &ds.stb2) == -1) {
		err = got_error_from_errno();
		goto done;
	}

	memset(&args, 0, sizeof(args));
	args.diff_format = D_NORMAL;
	args.label[0] = "";
	args.label[1] = "";
	args.diff_context = 0;

	err = got_diffreg(&res, f1, f2, D_FORCEASCII, &args, &ds,
	    outfile, NULL);
	if (err)
		goto done;

	*d = buf_load(outpath);
	if (*d == NULL)
		err = got_error_from_errno();
done:
	free(outpath);
	if (outfile)
		fclose(outfile);
	if (f1)
		fclose(f1);
	if (f2)
		fclose(f2);
	return err;
}

/*
 * For merge(1).
 */
const struct got_error *
got_merge_diff3(int outfd, const char *p1, const char *p2, const char *p3)
{
	const struct got_error *err = NULL;
	char *dp13, *dp23, *path1, *path2, *path3;
	BUF *b1, *b2, *b3, *d1, *d2, *diffb;
	u_char *data, *patch;
	size_t dlen, plen;
	struct wklhead temp_files;
	struct diff3_state *d3s;
	int i;

	d3s = calloc(1, sizeof(*d3s));
	if (d3s == NULL)
		return got_error_from_errno();
	d3s->eflag = 3; /* default -E for compatibility with former RCS */
	d3s->oflag = 1; /* default -E for compatibility with former RCS */

	b1 = b2 = b3 = d1 = d2 = diffb = NULL;
	dp13 = dp23 = path1 = path2 = path3 = NULL;
	data = patch = NULL;

	if ((b1 = buf_load(p1)) == NULL)
		goto out;
	if ((b2 = buf_load(p2)) == NULL)
		goto out;
	if ((b3 = buf_load(p3)) == NULL)
		goto out;

	diffb = buf_alloc(128);

	if (asprintf(&path1, "/tmp/got-diff1.XXXXXXXX") == -1) {
		err = got_error_from_errno();
		goto out;
	}
	if (asprintf(&path2, "/tmp/got-diff2.XXXXXXXX") == -1) {
		err = got_error_from_errno();
		goto out;
	}
	if (asprintf(&path3, "/tmp/got-diff3.XXXXXXXX") == -1) {
		err = got_error_from_errno();
		goto out;
	}

	err = buf_write_stmp(b1, path1, &temp_files);
	if (err)
		goto out;
	err = buf_write_stmp(b2, path2, &temp_files);
	if (err)
		goto out;
	err = buf_write_stmp(b3, path3, &temp_files);
	if (err)
		goto out;

	buf_free(b2);
	b2 = NULL;

	err = diffreg(&d1, path1, path3);
	if (err) {
		buf_free(diffb);
		diffb = NULL;
		goto out;

	}
	err = diffreg(&d2, path2, path3);
	if (err) {
		buf_free(diffb);
		diffb = NULL;
		goto out;
	}

	if (asprintf(&dp13, "/tmp/got-d13.XXXXXXXXXX") == -1) {
		err = got_error_from_errno();
		goto out;
	}
	err = buf_write_stmp(d1, dp13, &temp_files);
	if (err)
		goto out;

	buf_free(d1);
	d1 = NULL;

	if (asprintf(&dp23, "/tmp/got-d23.XXXXXXXXXX") == -1) {
		err = got_error_from_errno();
		goto out;
	}
	err = buf_write_stmp(d2, dp23, &temp_files);
	if (err)
		goto out;

	buf_free(d2);
	d2 = NULL;

	diffbuf = diffb;
	err = diff3_internal(dp13, dp23, path1, path2, path3, p1, p3,
	    d3s);
	if (err) {
		buf_free(diffb);
		diffb = NULL;
		goto out;
	}

	plen = buf_len(diffb);
	patch = buf_release(diffb);
	dlen = buf_len(b1);
	data = buf_release(b1);

	diffb = rcs_patchfile(data, dlen, patch, plen, ed_patch_lines);
out:
	buf_free(b2);
	buf_free(b3);
	buf_free(d1);
	buf_free(d2);

	(void)unlink(path1);
	(void)unlink(path2);
	(void)unlink(path3);
	(void)unlink(dp13);
	(void)unlink(dp23);

	free(path1);
	free(path2);
	free(path3);
	free(dp13);
	free(dp23);
	free(data);
	free(patch);

	worklist_clean(&temp_files, worklist_unlink);

	for (i = 0; i < nitems(d3s->fp); i++) {
		if (d3s->fp[i])
			fclose(d3s->fp[i]);
	}
	free(d3s);
	if (err == NULL && diffb) {
		if (buf_write_fd(diffb, outfd) < 0)
			err = got_error_from_errno();
	}
	buf_free(diffb);
	return err;
}

static const struct got_error *
diff3_internal(char *dp13, char *dp23, char *path1, char *path2, char *path3,
    const char *fmark, const char *rmark, struct diff3_state *d3s)
{
	const struct got_error *err = NULL;
	ssize_t m, n;
	int i;

	i = snprintf(d3s->f1mark, sizeof(d3s->f1mark), "<<<<<<< %s", fmark);
	if (i < 0 || i >= (int)sizeof(d3s->f1mark))
		return got_error(GOT_ERR_NO_SPACE);

	i = snprintf(d3s->f3mark, sizeof(d3s->f3mark), ">>>>>>> %s", rmark);
	if (i < 0 || i >= (int)sizeof(d3s->f3mark))
		return got_error(GOT_ERR_NO_SPACE);

	err = increase(d3s);
	if (err)
		return err;

	err = readin(&m, dp13, &d3s->d13, d3s);
	if (err)
		return err;
	err = readin(&n, dp23, &d3s->d23, d3s);
	if (err)
		return err;

	if ((d3s->fp[0] = fopen(path1, "r")) == NULL)
		return got_error_from_errno();
	if ((d3s->fp[1] = fopen(path2, "r")) == NULL)
		return got_error_from_errno();
	if ((d3s->fp[2] = fopen(path3, "r")) == NULL)
		return got_error_from_errno();

	if (merge(m, n, d3s) < 0)
		return got_error_from_errno();
	return NULL;
}

static int
ed_patch_lines(struct rcs_lines *dlines, struct rcs_lines *plines)
{
	char op, *ep;
	struct rcs_line *sort, *lp, *dlp, *ndlp, *insert_after;
	int start, end, i, lineno;
	u_char tmp;

	dlp = TAILQ_FIRST(&(dlines->l_lines));
	lp = TAILQ_FIRST(&(plines->l_lines));

	end = 0;
	for (lp = TAILQ_NEXT(lp, l_list); lp != NULL;
	    lp = TAILQ_NEXT(lp, l_list)) {
		/* Skip blank lines */
		if (lp->l_len < 2)
			continue;

		/* NUL-terminate line buffer for strtol() safety. */
		tmp = lp->l_line[lp->l_len - 1];
		lp->l_line[lp->l_len - 1] = '\0';

		/* len - 1 is NUL terminator so we use len - 2 for 'op' */
		op = lp->l_line[lp->l_len - 2];
		start = (int)strtol(lp->l_line, &ep, 10);

		/* Restore the last byte of the buffer */
		lp->l_line[lp->l_len - 1] = tmp;

		if (op == 'a') {
			if (start > dlines->l_nblines ||
			    start < 0 || *ep != 'a')
				return -1;
		} else if (op == 'c') {
			if (start > dlines->l_nblines ||
			    start < 0 || (*ep != ',' && *ep != 'c'))
				return -1;

			if (*ep == ',') {
				ep++;
				end = (int)strtol(ep, &ep, 10);
				if (end < 0 || *ep != 'c')
					return -1;
			} else {
				end = start;
			}
		}


		for (;;) {
			if (dlp == NULL)
				break;
			if (dlp->l_lineno == start)
				break;
			if (dlp->l_lineno > start) {
				dlp = TAILQ_PREV(dlp, tqh, l_list);
			} else if (dlp->l_lineno < start) {
				ndlp = TAILQ_NEXT(dlp, l_list);
				if (ndlp->l_lineno > start)
					break;
				dlp = ndlp;
			}
		}

		if (dlp == NULL)
			return -1;


		if (op == 'c') {
			insert_after = TAILQ_PREV(dlp, tqh, l_list);
			for (i = 0; i <= (end - start); i++) {
				ndlp = TAILQ_NEXT(dlp, l_list);
				TAILQ_REMOVE(&(dlines->l_lines), dlp, l_list);
				dlp = ndlp;
			}
			dlp = insert_after;
		}

		if (op == 'a' || op == 'c') {
			for (;;) {
				ndlp = lp;
				lp = TAILQ_NEXT(lp, l_list);
				if (lp == NULL)
					return -1;

				if (!memcmp(lp->l_line, ".", 1))
					break;

				TAILQ_REMOVE(&(plines->l_lines), lp, l_list);
				TAILQ_INSERT_AFTER(&(dlines->l_lines), dlp,
				    lp, l_list);
				dlp = lp;

				lp->l_lineno = start;
				lp = ndlp;
			}
		}

		/*
		 * always resort lines as the markers might be put at the
		 * same line as we first started editing.
		 */
		lineno = 0;
		TAILQ_FOREACH(sort, &(dlines->l_lines), l_list)
			sort->l_lineno = lineno++;
		dlines->l_nblines = lineno - 1;
	}

	return (0);
}

/*
 * Pick up the line numbers of all changes from one change file.
 * (This puts the numbers in a vector, which is not strictly necessary,
 * since the vector is processed in one sequential pass.
 * The vector could be optimized out of existence)
 */
static const struct got_error *
readin(size_t *n, char *name, struct diff **dd, struct diff3_state *d3s)
{
	const struct got_error *err = NULL;
	int a, b, c, d;
	char kind, *p;
	size_t i;

	d3s->fp[0] = fopen(name, "r");
	if (d3s->fp[0] == NULL)
		return got_error_from_errno();
	for (i = 0; (p = getchange(d3s->fp[0], d3s)); i++) {
		if (i >= d3s->szchanges - 1) {
			err = increase(d3s);
			if (err)
				return err;
		}
		a = b = number(&p);
		if (*p == ',') {
			p++;
			b = number(&p);
		}
		kind = *p++;
		c = d = number(&p);
		if (*p==',') {
			p++;
			d = number(&p);
		}
		if (kind == 'a')
			a++;
		if (kind == 'd')
			c++;
		b++;
		d++;
		(*dd)[i].old.from = a;
		(*dd)[i].old.to = b;
		(*dd)[i].new.from = c;
		(*dd)[i].new.to = d;
	}

	if (i) {
		(*dd)[i].old.from = (*dd)[i-1].old.to;
		(*dd)[i].new.from = (*dd)[i-1].new.to;
	}

	(void)fclose(d3s->fp[0]);

	return NULL;
}

static int
number(char **lc)
{
	int nn;

	nn = 0;
	while (isdigit((unsigned char)(**lc)))
		nn = nn*10 + *(*lc)++ - '0';

	return (nn);
}

static char *
getchange(FILE *b, struct diff3_state *d3s)
{
	char *line;

	while ((line = get_line(b, NULL, d3s))) {
		if (isdigit((unsigned char)line[0]))
			return (line);
	}

	return (NULL);
}

static char *
get_line(FILE *b, size_t *n, struct diff3_state *d3s)
{
	char *cp;
	size_t len;
	char *new;

	if ((cp = fgetln(b, &len)) == NULL)
		return (NULL);

	if (cp[len - 1] != '\n')
		len++;
	if (len + 1 > d3s->bufsize) {
		do {
			d3s->bufsize += 1024;
		} while (len + 1 > d3s->bufsize);
		new = reallocarray(d3s->buf, 1, d3s->bufsize);
		if (new == NULL)
			return NULL;
		d3s->buf = new;
	}
	memcpy(d3s->buf, cp, len - 1);
	d3s->buf[len - 1] = '\n';
	d3s->buf[len] = '\0';
	if (n != NULL)
		*n = len;

	return (d3s->buf);
}

static int
merge(size_t m1, size_t m2, struct diff3_state *d3s)
{
	struct diff *d1, *d2, *d3;
	int dpl, j, t1, t2;

	d1 = d3s->d13;
	d2 = d3s->d23;
	j = 0;
	for (;;) {
		t1 = (d1 < d3s->d13 + m1);
		t2 = (d2 < d3s->d23 + m2);
		if (!t1 && !t2)
			break;

		if (d3s->debug) {
			printf("%d,%d=%d,%d %d,%d=%d,%d\n",
			d1->old.from, d1->old.to,
			d1->new.from, d1->new.to,
			d2->old.from, d2->old.to,
			d2->new.from, d2->new.to);
		}

		/* first file is different from others */
		if (!t2 || (t1 && d1->new.to < d2->new.from)) {
			/* stuff peculiar to 1st file */
			if (d3s->eflag == 0) {
				separate("1");
				change(1, &d1->old, 0, d3s);
				keep(2, &d1->new, d3s);
				change(3, &d1->new, 0, d3s);
			}
			d1++;
			continue;
		}

		/* second file is different from others */
		if (!t1 || (t2 && d2->new.to < d1->new.from)) {
			if (d3s->eflag == 0) {
				separate("2");
				keep(1, &d2->new, d3s);
				change(2, &d2->old, 0, d3s);
				change(3, &d2->new, 0, d3s);
			}
			d2++;
			continue;
		}

		/*
		 * Merge overlapping changes in first file
		 * this happens after extension (see below).
		 */
		if (d1 + 1 < d3s->d13 + m1 && d1->new.to >= d1[1].new.from) {
			d1[1].old.from = d1->old.from;
			d1[1].new.from = d1->new.from;
			d1++;
			continue;
		}

		/* merge overlapping changes in second */
		if (d2 + 1 < d3s->d23 + m2 && d2->new.to >= d2[1].new.from) {
			d2[1].old.from = d2->old.from;
			d2[1].new.from = d2->new.from;
			d2++;
			continue;
		}
		/* stuff peculiar to third file or different in all */
		if (d1->new.from == d2->new.from && d1->new.to == d2->new.to) {
			dpl = duplicate(&d1->old, &d2->old, d3s);
			if (dpl == -1)
				return (-1);

			/*
			 * dpl = 0 means all files differ
			 * dpl = 1 means files 1 and 2 identical
			 */
			if (d3s->eflag == 0) {
				separate(dpl ? "3" : "");
				change(1, &d1->old, dpl, d3s);
				change(2, &d2->old, 0, d3s);
				d3 = d1->old.to > d1->old.from ? d1 : d2;
				change(3, &d3->new, 0, d3s);
			} else
				j = edit(d1, dpl, j, d3s);
			d1++;
			d2++;
			continue;
		}

		/*
		 * Overlapping changes from file 1 and 2; extend changes
		 * appropriately to make them coincide.
		 */
		if (d1->new.from < d2->new.from) {
			d2->old.from -= d2->new.from-d1->new.from;
			d2->new.from = d1->new.from;
		} else if (d2->new.from < d1->new.from) {
			d1->old.from -= d1->new.from-d2->new.from;
			d1->new.from = d2->new.from;
		}
		if (d1->new.to > d2->new.to) {
			d2->old.to += d1->new.to - d2->new.to;
			d2->new.to = d1->new.to;
		} else if (d2->new.to > d1->new.to) {
			d1->old.to += d2->new.to - d1->new.to;
			d1->new.to = d2->new.to;
		}
	}

	return (edscript(j, d3s));
}

static void
separate(const char *s)
{
	diff_output("====%s\n", s);
}

/*
 * The range of lines rold.from thru rold.to in file i is to be changed.
 * It is to be printed only if it does not duplicate something to be
 * printed later.
 */
static void
change(int i, struct range *rold, int fdup, struct diff3_state *d3s)
{
	diff_output("%d:", i);
	d3s->last[i] = rold->to;
	prange(rold);
	if (fdup || d3s->debug)
		return;
	i--;
	(void)skip(i, rold->from, NULL, d3s);
	(void)skip(i, rold->to, "  ", d3s);
}

/*
 * print the range of line numbers, rold.from thru rold.to, as n1,n2 or n1
 */
static void
prange(struct range *rold)
{
	if (rold->to <= rold->from)
		diff_output("%da\n", rold->from - 1);
	else {
		diff_output("%d", rold->from);
		if (rold->to > rold->from+1)
			diff_output(",%d", rold->to - 1);
		diff_output("c\n");
	}
}

/*
 * No difference was reported by diff between file 1 (or 2) and file 3,
 * and an artificial dummy difference (trange) must be ginned up to
 * correspond to the change reported in the other file.
 */
static void
keep(int i, struct range *rnew, struct diff3_state *d3s)
{
	int delta;
	struct range trange;

	delta = d3s->last[3] - d3s->last[i];
	trange.from = rnew->from - delta;
	trange.to = rnew->to - delta;
	change(i, &trange, 1, d3s);
}

/*
 * skip to just before line number from in file "i".  If "pr" is non-NULL,
 * print all skipped stuff with string pr as a prefix.
 */
static int
skip(int i, int from, char *pr, struct diff3_state *d3s)
{
	size_t j, n;
	char *line;

	for (n = 0; d3s->cline[i] < from - 1; n += j) {
		if ((line = get_line(d3s->fp[i], &j, d3s)) == NULL)
			return (-1);
		if (pr != NULL)
			diff_output("%s%s", pr, line);
		d3s->cline[i]++;
	}
	return ((int) n);
}

/*
 * Return 1 or 0 according as the old range (in file 1) contains exactly
 * the same data as the new range (in file 2).
 */
static int
duplicate(struct range *r1, struct range *r2, struct diff3_state *d3s)
{
	int c,d;
	int nchar;
	int nline;

	if (r1->to-r1->from != r2->to-r2->from)
		return (0);
	(void)skip(0, r1->from, NULL, d3s);
	(void)skip(1, r2->from, NULL, d3s);
	nchar = 0;
	for (nline=0; nline < r1->to - r1->from; nline++) {
		do {
			c = getc(d3s->fp[0]);
			d = getc(d3s->fp[1]);
			if (c == -1 || d== -1)
				return (-1);
			nchar++;
			if (c != d) {
				repos(nchar, d3s);
				return (0);
			}
		} while (c != '\n');
	}
	repos(nchar, d3s);
	return (1);
}

static void
repos(int nchar, struct diff3_state *d3s)
{
	int i;

	for (i = 0; i < 2; i++)
		(void)fseek(d3s->fp[i], (long)-nchar, SEEK_CUR);
}

/*
 * collect an editing script for later regurgitation
 */
static int
edit(struct diff *diff, int fdup, int j, struct diff3_state *d3s)
{
	if (((fdup + 1) & d3s->eflag) == 0)
		return (j);
	j++;
	d3s->overlap[j] = !fdup;
	if (!fdup)
		d3s->overlapcnt++;
	d3s->de[j].old.from = diff->old.from;
	d3s->de[j].old.to = diff->old.to;
	d3s->de[j].new.from =
	    d3s->de[j-1].new.to + skip(2, diff->new.from, NULL, d3s);
	d3s->de[j].new.to =
	    d3s->de[j].new.from + skip(2, diff->new.to, NULL, d3s);
	return (j);
}

/* regurgitate */
static int
edscript(int n, struct diff3_state *d3s)
{
	int j, k;
	char block[BUFSIZ+1];

	for (; n > 0; n--) {
		if (!d3s->oflag || !d3s->overlap[n])
			prange(&d3s->de[n].old);
		else
			diff_output("%da\n=======\n", d3s->de[n].old.to -1);
		(void)fseek(d3s->fp[2], (long)d3s->de[n].new.from, SEEK_SET);
		k = d3s->de[n].new.to - d3s->de[n].new.from;
		for (; k > 0; k-= j) {
			j = k > BUFSIZ ? BUFSIZ : k;
			if (fread(block, 1, j, d3s->fp[2]) != (size_t)j)
				return (-1);
			block[j] = '\0';
			diff_output("%s", block);
		}

		if (!d3s->oflag || !d3s->overlap[n])
			diff_output(".\n");
		else {
			diff_output("%s\n.\n", d3s->f3mark);
			diff_output("%da\n%s\n.\n", d3s->de[n].old.from - 1,
			    d3s->f1mark);
		}
	}

	return (d3s->overlapcnt);
}

static const struct got_error *
increase(struct diff3_state *d3s)
{
	size_t newsz, incr;
	struct diff *d;
	char *s;

	/* are the memset(3) calls needed? */
	newsz = d3s->szchanges == 0 ? 64 : 2 * d3s->szchanges;
	incr = newsz - d3s->szchanges;

	d = reallocarray(d3s->d13, newsz, sizeof(*d3s->d13));
	if (d == NULL)
		return got_error_from_errno();
	d3s->d13 = d;
	memset(d3s->d13 + d3s->szchanges, 0, incr * sizeof(*d3s->d13));

	d = reallocarray(d3s->d23, newsz, sizeof(*d3s->d23));
	if (d == NULL)
		return got_error_from_errno();
	d3s->d23 = d;
	memset(d3s->d23 + d3s->szchanges, 0, incr * sizeof(*d3s->d23));

	d = reallocarray(d3s->de, newsz, sizeof(*d3s->de));
	if (d == NULL)
		return got_error_from_errno();
	d3s->de = d;
	memset(d3s->de + d3s->szchanges, 0, incr * sizeof(*d3s->de));

	s = reallocarray(d3s->overlap, newsz, sizeof(*d3s->overlap));
	if (s == NULL)
		return got_error_from_errno();
	d3s->overlap = s;
	memset(d3s->overlap + d3s->szchanges, 0, incr * sizeof(*d3s->overlap));
	d3s->szchanges = newsz;

	return NULL;
}