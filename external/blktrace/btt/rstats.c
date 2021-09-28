/*
 * blktrace output analysis: generate a timeline & gather statistics
 *
 * (C) Copyright 2009 Hewlett-Packard Development Company, L.P.
 *	Alan D. Brunelle (alan.brunelle@hp.com)
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */
#include "globals.h"

struct files {
	FILE *fp;
	char *nm;
};

struct rstat {
	struct list_head head;
	struct files files[2];
	unsigned long long ios, nblks;
	long long base_sec;
};

static struct rstat *sys_info;
static LIST_HEAD(rstats);

static int do_open(struct files *fip, char *bn, char *pn)
{
	fip->nm = malloc(strlen(bn) + 16);
	sprintf(fip->nm, "%s_%s.dat", bn, pn);

	fip->fp = my_fopen(fip->nm, "w");
	if (fip->fp) {
		add_file(fip->fp, fip->nm);
		return 0;
	}

	free(fip->nm);
	return -1;
}

static int init_rsip(struct rstat *rsip, struct d_info *dip)
{
	char *nm = dip ? dip->dip_name : "sys";

	rsip->base_sec = -1;
	rsip->ios = rsip->nblks = 0;
	if (do_open(&rsip->files[0], nm, "iops_fp") ||
			    do_open(&rsip->files[1], nm, "mbps_fp"))
		return -1;

	list_add_tail(&rsip->head, &rstats);
	return 0;
}

static void rstat_emit(struct rstat *rsip, double cur)
{
	double mbps;

	/*
	 * I/Os per second is easy: just the ios
	 */
	fprintf(rsip->files[0].fp, "%lld %llu\n", rsip->base_sec, rsip->ios);

	/*
	 * MB/s we convert blocks to mb...
	 */
	mbps = ((double)rsip->nblks * 512.0) / (1024.0 * 1024.0);
	fprintf(rsip->files[1].fp, "%lld %lf\n", rsip->base_sec, mbps);

	rsip->base_sec = (unsigned long long)cur;
	rsip->ios = rsip->nblks = 0;
}

static void __add(struct rstat *rsip, double cur, unsigned long long nblks)
{
	if (rsip->base_sec < 0)
		rsip->base_sec = (long long)cur;
	else if (((long long)cur - rsip->base_sec) >= 1)
		rstat_emit(rsip, cur);

	rsip->ios++;
	rsip->nblks += nblks;
}

void *rstat_alloc(struct d_info *dip)
{
	struct rstat *rsip = malloc(sizeof(*rsip));

	if (!init_rsip(rsip, dip))
		return rsip;

	free(rsip);
	return NULL;
}

void rstat_free(void *ptr)
{
	struct rstat *rsip = ptr;

	rstat_emit(rsip, last_t_seen);
	list_del(&rsip->head);
	free(rsip);
}

void rstat_add(void *ptr, double cur, unsigned long long nblks)
{
	if (ptr != NULL)
		__add((struct rstat *)ptr, cur, nblks);
	__add(sys_info, cur, nblks);
}

int rstat_init(void)
{
	sys_info = rstat_alloc(NULL);
	return sys_info != NULL;
}

void rstat_exit(void)
{
	struct list_head *p, *q;

	list_for_each_safe(p, q, &rstats) {
		struct rstat *rsip = list_entry(p, struct rstat, head);
		rstat_free(rsip);
	}
}
