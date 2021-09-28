/*
 * Copyright (C) 2012 Fusion-io
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public
 *  License v2 as published by the Free Software Foundation.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */
#ifndef __MPSTAT__
#define __MPSTAT__

int read_mpstat(struct trace *trace, char *trace_name);
char *next_mpstat(struct trace *trace);
char *first_mpstat(struct trace *trace);
int read_mpstat_event(struct trace *trace, double *user,
		      double *sys, double *iowait, double *irq,
		      double *soft);
int next_mpstat_line(struct trace *trace);
int add_mpstat_gld(int time, double sys, struct graph_line_data *gld);
#endif
