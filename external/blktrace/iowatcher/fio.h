/*
 * Copyright (C) 2013 Fusion-io
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
#ifndef __FIO__
#define __FIO__

int read_fio_event(struct trace *trace, int *time, u64 *bw, int *dir);
int add_fio_gld(unsigned int time, u64 bw, struct graph_line_data *gld);
int next_fio_line(struct trace *trace);
struct trace *open_fio_trace(char *path);
char *first_fio(struct trace *trace);

#endif
