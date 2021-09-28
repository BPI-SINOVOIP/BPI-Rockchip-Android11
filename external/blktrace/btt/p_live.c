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

struct p_live {
	struct rb_node rb_node;
	struct list_head head;
	__u64 dt, ct;
};

struct get_info {
	struct p_live_info *plip;
	__u64 t_start, t_end;
	__u64 tot_live;

	FILE *ofp;
	double last_0;
	int base_y;
};

static struct rb_root p_live_root;
static LIST_HEAD(all_p_lives);

static FILE *do_open(struct d_info *dip)
{
	FILE *ofp;

	if (do_p_live) {
		char *bn = dip ? dip->dip_name : "sys";
		char *nm = malloc(strlen(bn) + 16);

		sprintf(nm, "%s_live.dat", bn);
		ofp = my_fopen(nm, "w");
		if (ofp)
			add_file(ofp, nm);
		else
			free(nm);
	}
	else
		ofp = NULL;

	return ofp;
}

static inline int inside(struct p_live *plp, __u64 dt, __u64 ct)
{
	if (plp->dt <= dt && dt <= plp->ct)
		return 1;
	if (plp->dt <= ct && ct <= plp->ct)
		return 1;
	if (dt < plp->dt && plp->ct < ct)
		return 1;
	return 0;
}

static void __p_live_add(struct rb_root *root, __u64 dt, __u64 ct)
{
	struct p_live *plp;
	struct rb_node *parent = NULL;
	struct rb_node **p = &root->rb_node;

	while (*p) {
		parent = *p;
		plp = rb_entry(parent, struct p_live, rb_node);

		if (inside(plp, dt, ct)) {
			list_del(&plp->head);
			rb_erase(&plp->rb_node, root);
			__p_live_add(root, min(plp->dt, dt), max(plp->ct, ct));
			free(plp);
			return;
		}

		if (ct < plp->dt)
			p = &(*p)->rb_left;
		else
			p = &(*p)->rb_right;
	}

	plp = malloc(sizeof(*plp));
	plp->dt = dt;
	plp->ct = ct;

	rb_link_node(&plp->rb_node, parent, p);
	rb_insert_color(&plp->rb_node, root);
	list_add_tail(&plp->head, &all_p_lives);
}

void *p_live_alloc(void)
{
	size_t sz = sizeof(struct rb_root);
	return memset(malloc(sz), 0, sz);
}

void p_live_free(void *p)
{
	free(p);
}

void p_live_add(struct d_info *dip, __u64 dt, __u64 ct)
{
	__p_live_add(dip->p_live_handle, dt, ct);
	__p_live_add(&p_live_root, dt, ct);
}

static void p_live_visit(struct rb_node *n, struct get_info *gip)
{
	struct p_live *plp = rb_entry(n, struct p_live, rb_node);

	if (n->rb_left)
		p_live_visit(n->rb_left, gip);

	if (gip->ofp) {
		float y0 = gip->base_y;
		float y1 = gip->base_y + 0.9;

		fprintf(gip->ofp, "%.9lf %.1f\n", gip->last_0, y0);
		fprintf(gip->ofp, "%.9lf %.1f\n", BIT_TIME(plp->dt), y0);
		fprintf(gip->ofp, "%.9lf %.1f\n", BIT_TIME(plp->dt), y1);
		fprintf(gip->ofp, "%.9lf %.1f\n", BIT_TIME(plp->ct), y1);
		fprintf(gip->ofp, "%.9lf %.1f\n", BIT_TIME(plp->ct), y0);
		gip->last_0 = BIT_TIME(plp->ct);
	}

	gip->plip->nlives++;
	gip->tot_live += (plp->ct - plp->dt);

	if (gip->t_start < 0.0 || plp->dt < gip->t_start)
		gip->t_start = plp->dt;
	if (gip->t_end < 0.0 || plp->ct > gip->t_end)
		gip->t_end = plp->ct;

	if (n->rb_right)
		p_live_visit(n->rb_right, gip);
}

struct p_live_info *p_live_get(struct d_info *dip, int base_y)
{
	FILE *ofp = do_open(dip);
	static struct p_live_info pli;
	struct p_live_info *plip = &pli;
	struct get_info gi = {
		.plip = plip,
		.t_start = 0.0,
		.t_end = 0.0,
		.ofp = ofp,
		.base_y = base_y,
	};
	struct rb_root *root = (dip) ? dip->p_live_handle : &p_live_root;

	memset(plip, 0, sizeof(*plip));
	if (root->rb_node)
		p_live_visit(root->rb_node, &gi);

	if (plip->nlives == 0) {
		plip->avg_live = plip->avg_lull = plip->p_live = 0.0;
	}
	else if (plip->nlives == 1) {
		plip->avg_lull = 0.0;
		plip->p_live = 100.0;
	}
	else {
		double t_time = BIT_TIME(gi.t_end - gi.t_start);
		double tot_live = BIT_TIME(gi.tot_live);

		plip->p_live = 100.0 * (tot_live / t_time);
		plip->avg_live = tot_live / plip->nlives;
		plip->avg_lull = (t_time - tot_live) / (plip->nlives - 1);
	}

	return plip;
}

void p_live_exit(void)
{
	struct list_head *p, *q;

	list_for_each_safe(p, q, &all_p_lives) {
		struct p_live *plp = list_entry(p, struct p_live, head);

		list_del(&plp->head);
		free(plp);
	}
}
