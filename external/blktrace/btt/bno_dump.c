/*
 * blktrace output analysis: generate a timeline & gather statistics
 *
 * Copyright (C) 2006 Alan D. Brunelle <Alan.Brunelle@hp.com>
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

struct bno_dump {
	FILE *rfp, *wfp, *cfp;
};

static FILE *bno_dump_open(struct d_info *dip, char rwc)
{
	FILE *fp;
	char *oname;

	oname = malloc(strlen(bno_dump_name) + strlen(dip->dip_name) + 32);
	sprintf(oname, "%s_%s_%c.dat", bno_dump_name, dip->dip_name, rwc);
	if ((fp = my_fopen(oname, "w")) == NULL) {
		perror(oname);
		free(oname);
	} else
		add_file(fp, oname);
	return fp;
}

static inline void bno_dump_write(FILE *fp, struct io *iop)
{
	fprintf(fp, "%15.9lf %lld %lld\n", BIT_TIME(iop->t.time),
		(long long)BIT_START(iop), (long long)BIT_END(iop));
}

void *bno_dump_alloc(struct d_info *dip)
{
	struct bno_dump *bdp;

	if (bno_dump_name == NULL) return NULL;

	bdp = malloc(sizeof(*bdp));
	bdp->rfp = bno_dump_open(dip, 'r');
	bdp->wfp = bno_dump_open(dip, 'w');
	bdp->cfp = bno_dump_open(dip, 'c');

	return bdp;
}

void bno_dump_free(void *param)
{
	free(param);
}

void bno_dump_add(void *handle, struct io *iop)
{
	struct bno_dump *bdp = handle;

	if (bdp) {
		FILE *fp = IOP_READ(iop) ? bdp->rfp : bdp->wfp;

		if (fp)
			bno_dump_write(fp, iop);
		if (bdp->cfp)
			bno_dump_write(bdp->cfp, iop);
	}
}
