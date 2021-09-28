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
 *
 */
#ifndef __IOWATCH_TRACERS
#define __IOWATCH_TRACERS
int run_program(int argc, char **argv, int wait, pid_t *pid, char *stdoutpath);
int wait_program(pid_t pid, const char *pname, int signal);
int stop_blktrace(void);
int start_blktrace(char **devices, int num_devices, char *trace_name, char *dest);
int start_mpstat(char *trace_name);
int wait_for_tracers(int sig);


#endif
