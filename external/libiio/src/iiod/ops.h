/*
 * libiio - Library for interfacing industrial I/O (IIO) devices
 *
 * Copyright (C) 2014 Analog Devices, Inc.
 * Author: Paul Cercueil <paul.cercueil@analog.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * */

#ifndef __OPS_H__
#define __OPS_H__

#include "../iio-private.h"
#include "queue.h"

#include <endian.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>

#if WITH_AIO
#include <libaio.h>
#endif

#ifndef __bswap_constant_16
#define __bswap_constant_16(x) \
	((unsigned short int) ((((x) >> 8) & 0xff) | (((x) & 0xff) << 8)))
#endif

#ifndef __bswap_constant_32
#define __bswap_constant_32(x) \
	((((x) & 0xff000000) >> 24) | (((x) & 0x00ff0000) >>  8) | \
	 (((x) & 0x0000ff00) <<  8) | (((x) & 0x000000ff) << 24))
#endif

struct thread_pool;
extern struct thread_pool *main_thread_pool;

struct parser_pdata {
	struct iio_context *ctx;
	bool stop, verbose;
	int fd_in, fd_out;

	SLIST_HEAD(ParserDataThdHead, ThdEntry) thdlist_head;

	/* Used as temporaries placements by the lexer */
	struct iio_device *dev;
	struct iio_channel *chn;
	bool channel_is_output;
	bool fd_in_is_socket, fd_out_is_socket;
#if WITH_AIO
	io_context_t aio_ctx;
	int aio_eventfd;
	pthread_mutex_t aio_mutex;
#endif
	struct thread_pool *pool;

	ssize_t (*writefd)(struct parser_pdata *pdata, const void *buf, size_t len);
	ssize_t (*readfd)(struct parser_pdata *pdata, void *buf, size_t len);
};

extern bool server_demux; /* Defined in iiod.c */

void interpreter(struct iio_context *ctx, int fd_in, int fd_out, bool verbose,
	bool is_socket, bool use_aio, struct thread_pool *pool);

int start_usb_daemon(struct iio_context *ctx, const char *ffs,
		bool debug, bool use_aio, unsigned int nb_pipes,
		struct thread_pool *pool);

int open_dev(struct parser_pdata *pdata, struct iio_device *dev,
		size_t samples_count, const char *mask, bool cyclic);
int close_dev(struct parser_pdata *pdata, struct iio_device *dev);

ssize_t rw_dev(struct parser_pdata *pdata, struct iio_device *dev,
		unsigned int nb, bool is_write);

ssize_t read_dev_attr(struct parser_pdata *pdata, struct iio_device *dev,
		const char *attr, enum iio_attr_type type);
ssize_t write_dev_attr(struct parser_pdata *pdata, struct iio_device *dev,
		const char *attr, size_t len, enum iio_attr_type type);

ssize_t read_chn_attr(struct parser_pdata *pdata, struct iio_channel *chn,
		const char *attr);
ssize_t write_chn_attr(struct parser_pdata *pdata, struct iio_channel *chn,
		const char *attr, size_t len);

ssize_t get_trigger(struct parser_pdata *pdata, struct iio_device *dev);
ssize_t set_trigger(struct parser_pdata *pdata,
		struct iio_device *dev, const char *trig);

int set_timeout(struct parser_pdata *pdata, unsigned int timeout);
int set_buffers_count(struct parser_pdata *pdata,
		struct iio_device *dev, long value);

ssize_t read_line(struct parser_pdata *pdata, char *buf, size_t len);
ssize_t write_all(struct parser_pdata *pdata, const void *src, size_t len);

static __inline__ void output(struct parser_pdata *pdata, const char *text)
{
	if (write_all(pdata, text, strlen(text)) <= 0)
		pdata->stop = true;
}

static __inline__ int poll_nointr(struct pollfd *pfd, unsigned int num_pfd)
{
	int ret;

	do {
		ret = poll(pfd, num_pfd, -1);
	} while (ret == -1 && errno == EINTR);

	return ret;
}

#endif /* __OPS_H__ */
