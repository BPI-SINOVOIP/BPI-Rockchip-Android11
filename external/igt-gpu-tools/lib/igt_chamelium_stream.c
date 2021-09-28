/*
 * Copyright © 2019 Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 *
 * Authors: Simon Ser <simon.ser@intel.com>
 */

#include "config.h"

#include <arpa/inet.h>
#include <errno.h>
#include <netdb.h>
#include <stdbool.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>

#include "igt_chamelium_stream.h"
#include "igt_core.h"
#include "igt_rc.h"

#define STREAM_PORT 9994
#define STREAM_VERSION_MAJOR 1
#define STREAM_VERSION_MINOR 0

enum stream_error {
	STREAM_ERROR_NONE = 0,
	STREAM_ERROR_COMMAND = 1,
	STREAM_ERROR_ARGUMENT = 2,
	STREAM_ERROR_EXISTS = 3,
	STREAM_ERROR_VIDEO_MEM_OVERFLOW_STOP = 4,
	STREAM_ERROR_VIDEO_MEM_OVERFLOW_DROP = 5,
	STREAM_ERROR_AUDIO_MEM_OVERFLOW_STOP = 6,
	STREAM_ERROR_AUDIO_MEM_OVERFLOW_DROP = 7,
	STREAM_ERROR_NO_MEM = 8,
};

enum stream_message_kind {
	STREAM_MESSAGE_REQUEST = 0,
	STREAM_MESSAGE_RESPONSE = 1,
	STREAM_MESSAGE_DATA = 2,
};

enum stream_message_type {
	STREAM_MESSAGE_RESET = 0,
	STREAM_MESSAGE_GET_VERSION = 1,
	STREAM_MESSAGE_VIDEO_STREAM = 2,
	STREAM_MESSAGE_SHRINK_VIDEO = 3,
	STREAM_MESSAGE_VIDEO_FRAME = 4,
	STREAM_MESSAGE_DUMP_REALTIME_VIDEO = 5,
	STREAM_MESSAGE_STOP_DUMP_VIDEO = 6,
	STREAM_MESSAGE_DUMP_REALTIME_AUDIO = 7,
	STREAM_MESSAGE_STOP_DUMP_AUDIO = 8,
};

struct chamelium_stream {
	char *host;
	unsigned int port;

	int fd;
};

static const char *stream_error_str(enum stream_error err)
{
	switch (err) {
	case STREAM_ERROR_NONE:
		return "no error";
	case STREAM_ERROR_COMMAND:
		return "invalid command";
	case STREAM_ERROR_ARGUMENT:
		return "invalid arguments";
	case STREAM_ERROR_EXISTS:
		return "dump already started";
	case STREAM_ERROR_VIDEO_MEM_OVERFLOW_STOP:
		return "video dump stopped after overflow";
	case STREAM_ERROR_VIDEO_MEM_OVERFLOW_DROP:
		return "video frame dropped after overflow";
	case STREAM_ERROR_AUDIO_MEM_OVERFLOW_STOP:
		return "audio dump stoppred after overflow";
	case STREAM_ERROR_AUDIO_MEM_OVERFLOW_DROP:
		return "audio page dropped after overflow";
	case STREAM_ERROR_NO_MEM:
		return "out of memory";
	}
	return "unknown error";
}

/**
 * The Chamelium URL is specified in the configuration file. We need to extract
 * the host to connect to the stream server.
 */
static char *parse_url_host(const char *url)
{
	static const char prefix[] = "http://";
	char *colon;

	if (strstr(url, prefix) != url)
		return NULL;
	url += strlen(prefix);

	colon = strchr(url, ':');
	if (!colon)
		return NULL;

	return strndup(url, colon - url);
}

static bool chamelium_stream_read_config(struct chamelium_stream *client)
{
	GError *error = NULL;
	gchar *chamelium_url;

	if (!igt_key_file) {
		igt_warn("No configuration file available for chamelium\n");
		return false;
	}

	chamelium_url = g_key_file_get_string(igt_key_file, "Chamelium", "URL",
					      &error);
	if (!chamelium_url) {
		igt_warn("Couldn't read Chamelium URL from config file: %s\n",
			 error->message);
		return false;
	}

	client->host = parse_url_host(chamelium_url);
	if (!client->host) {
		igt_warn("Invalid Chamelium URL in config file: %s\n",
			 chamelium_url);
		return false;
	}
	client->port = STREAM_PORT;

	return true;
}

static bool chamelium_stream_connect(struct chamelium_stream *client)
{
	int ret;
	char port_str[16];
	struct addrinfo hints = {};
	struct addrinfo *results, *ai;
	struct timeval tv = {};

	igt_debug("Connecting to Chamelium stream server: tcp://%s:%u\n",
		  client->host, client->port);

	snprintf(port_str, sizeof(port_str), "%u", client->port);

	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	ret = getaddrinfo(client->host, port_str, &hints, &results);
	if (ret != 0) {
		igt_warn("getaddrinfo failed: %s\n", gai_strerror(ret));
		return false;
	}

	client->fd = -1;
	for (ai = results; ai != NULL; ai = ai->ai_next) {
		client->fd = socket(ai->ai_family, ai->ai_socktype,
				    ai->ai_protocol);
		if (client->fd == -1)
			continue;

		if (connect(client->fd, ai->ai_addr, ai->ai_addrlen) == -1) {
			close(client->fd);
			client->fd = -1;
			continue;
		}

		break;
	}

	freeaddrinfo(results);

	if (client->fd < 0) {
		igt_warn("Failed to connect to Chamelium stream server\n");
		return false;
	}

	/* Set a read and write timeout of 5 seconds. */
	tv.tv_sec = 5;
	setsockopt(client->fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
	setsockopt(client->fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));

	return true;
}

static bool read_whole(int fd, void *buf, size_t buf_len)
{
	ssize_t ret;
	size_t n = 0;
	char *ptr;

	while (n < buf_len) {
		ptr = (char *) buf + n;
		ret = read(fd, ptr, buf_len - n);
		if (ret < 0) {
			igt_warn("read failed: %s\n", strerror(errno));
			return false;
		} else if (ret == 0) {
			igt_warn("short read\n");
			return false;
		}
		n += ret;
	}

	return true;
}

static bool write_whole(int fd, void *buf, size_t buf_len)
{
	ssize_t ret;
	size_t n = 0;
	char *ptr;

	while (n < buf_len) {
		ptr = (char *) buf + n;
		ret = write(fd, ptr, buf_len - n);
		if (ret < 0) {
			igt_warn("write failed: %s\n", strerror(errno));
			return false;
		} else if (ret == 0) {
			igt_warn("short write\n");
			return false;
		}
		n += ret;
	}

	return true;
}

static bool read_and_discard(int fd, size_t len)
{
	char buf[1024];
	size_t n;

	while (len > 0) {
		n = len;
		if (n > sizeof(buf))
			n = sizeof(buf);

		if (!read_whole(fd, buf, n))
			return false;

		len -= n;
	}

	return true;
}

/** Read a message header from the socket.
 *
 * The header is laid out as follows:
 * - u16: message type
 * - u16: error code
 * - u32: message length
 */
static bool chamelium_stream_read_header(struct chamelium_stream *client,
					 enum stream_message_kind *kind,
					 enum stream_message_type *type,
					 enum stream_error *err,
					 size_t *len)
{
	uint16_t _type;
	char buf[8];

	if (!read_whole(client->fd, buf, sizeof(buf)))
		return false;

	_type = ntohs(*(uint16_t *) &buf[0]);
	*type = _type & 0xFF;
	*kind = _type >> 8;
	*err = ntohs(*(uint16_t *) &buf[2]);
	*len = ntohl(*(uint32_t *) &buf[4]);

	return true;
}

static bool chamelium_stream_write_header(struct chamelium_stream *client,
					  enum stream_message_type type,
					  enum stream_error err,
					  size_t len)
{
	char buf[8];
	uint16_t _type;

	_type = type | (STREAM_MESSAGE_REQUEST << 8);

	*(uint16_t *) &buf[0] = htons(_type);
	*(uint16_t *) &buf[2] = htons(err);
	*(uint32_t *) &buf[4] = htonl(len);

	return write_whole(client->fd, buf, sizeof(buf));
}

static bool chamelium_stream_read_response(struct chamelium_stream *client,
					   enum stream_message_type type,
					   void *buf, size_t buf_len)
{
	enum stream_message_kind read_kind;
	enum stream_message_type read_type;
	enum stream_error read_err;
	size_t read_len;

	if (!chamelium_stream_read_header(client, &read_kind, &read_type,
					  &read_err, &read_len))
		return false;

	if (read_kind != STREAM_MESSAGE_RESPONSE) {
		igt_warn("Expected a response, got kind %d\n", read_kind);
		return false;
	}
	if (read_type != type) {
		igt_warn("Expected message type %d, got %d\n",
			 type, read_type);
		return false;
	}
	if (read_err != STREAM_ERROR_NONE) {
		igt_warn("Received error: %s (%d)\n",
			 stream_error_str(read_err), read_err);
		return false;
	}
	if (buf_len != read_len) {
		igt_warn("Received invalid message body size "
			 "(got %zu bytes, want %zu bytes)\n",
			 read_len, buf_len);
		return false;
	}

	return read_whole(client->fd, buf, buf_len);
}

static bool chamelium_stream_write_request(struct chamelium_stream *client,
					   enum stream_message_type type,
					   void *buf, size_t buf_len)
{
	if (!chamelium_stream_write_header(client, type, STREAM_ERROR_NONE,
					   buf_len))
		return false;

	if (buf_len == 0)
		return true;

	return write_whole(client->fd, buf, buf_len);
}

static bool chamelium_stream_call(struct chamelium_stream *client,
				  enum stream_message_type type,
				  void *req_buf, size_t req_len,
				  void *resp_buf, size_t resp_len)
{
	if (!chamelium_stream_write_request(client, type, req_buf, req_len))
		return false;

	return chamelium_stream_read_response(client, type, resp_buf, resp_len);
}

static bool chamelium_stream_check_version(struct chamelium_stream *client)
{
	char resp[2];
	uint8_t major, minor;

	if (!chamelium_stream_call(client, STREAM_MESSAGE_GET_VERSION,
				   NULL, 0, resp, sizeof(resp)))
		return false;

	major = resp[0];
	minor = resp[1];
	if (major != STREAM_VERSION_MAJOR || minor < STREAM_VERSION_MINOR) {
		igt_warn("Version mismatch (want %d.%d, got %d.%d)\n",
			 STREAM_VERSION_MAJOR, STREAM_VERSION_MINOR,
			 major, minor);
		return false;
	}

	return true;
}

/**
 * chamelium_stream_dump_realtime_audio:
 *
 * Starts audio capture. The caller can then call
 * #chamelium_stream_receive_realtime_audio to receive audio pages.
 */
bool chamelium_stream_dump_realtime_audio(struct chamelium_stream *client,
					  enum chamelium_stream_realtime_mode mode)
{
	char req[1];

	igt_debug("Starting real-time audio capture\n");

	req[0] = mode;
	return chamelium_stream_call(client, STREAM_MESSAGE_DUMP_REALTIME_AUDIO,
				     req, sizeof(req), NULL, 0);
}

/**
 * chamelium_stream_receive_realtime_audio:
 * @page_count: if non-NULL, will be set to the dumped page number
 * @buf: must either point to a dynamically allocated memory region or NULL
 * @buf_len: number of elements of *@buf, for zero if @buf is NULL
 *
 * Receives one audio page from the streaming server.
 *
 * In "best effort" mode, some pages can be dropped. This can be detected via
 * the page count.
 *
 * buf_len will be set to the size of the page. The caller is responsible for
 * calling free(3) on *buf.
 */
bool chamelium_stream_receive_realtime_audio(struct chamelium_stream *client,
					     size_t *page_count,
					     int32_t **buf, size_t *buf_len)
{
	enum stream_message_kind kind;
	enum stream_message_type type;
	enum stream_error err;
	size_t body_len;
	char page_count_buf[4];
	int32_t *ptr;

	while (true) {
		if (!chamelium_stream_read_header(client, &kind, &type,
						  &err, &body_len))
			return false;

		if (kind != STREAM_MESSAGE_DATA) {
			igt_warn("Expected a data message, got kind %d\n", kind);
			return false;
		}
		if (type != STREAM_MESSAGE_DUMP_REALTIME_AUDIO) {
			igt_warn("Expected real-time audio dump message, "
				 "got type %d\n", type);
			return false;
		}

		if (err == STREAM_ERROR_NONE)
			break;
		else if (err != STREAM_ERROR_AUDIO_MEM_OVERFLOW_DROP) {
			igt_warn("Received error: %s (%d)\n",
				 stream_error_str(err), err);
			return false;
		}

		igt_debug("Dropped an audio page because of an overflow\n");
		igt_assert(body_len == 0);
	}

	igt_assert(body_len >= sizeof(page_count_buf));

	if (!read_whole(client->fd, page_count_buf, sizeof(page_count_buf)))
		return false;
	if (page_count)
		*page_count = ntohl(*(uint32_t *) &page_count_buf[0]);
	body_len -= sizeof(page_count_buf);

	igt_assert(body_len % sizeof(int32_t) == 0);
	if (*buf_len * sizeof(int32_t) != body_len) {
		ptr = realloc(*buf, body_len);
		if (!ptr) {
			igt_warn("realloc failed: %s\n", strerror(errno));
			return false;
		}
		*buf = ptr;
		*buf_len = body_len / sizeof(int32_t);
	}

	return read_whole(client->fd, *buf, body_len);
}

/**
 * chamelium_stream_stop_realtime_audio:
 *
 * Stops real-time audio capture. This also drops any buffered audio pages.
 * The caller shouldn't call #chamelium_stream_receive_realtime_audio after
 * stopping audio capture.
 */
bool chamelium_stream_stop_realtime_audio(struct chamelium_stream *client)
{
	enum stream_message_kind kind;
	enum stream_message_type type;
	enum stream_error err;
	size_t len;

	igt_debug("Stopping real-time audio capture\n");

	if (!chamelium_stream_write_request(client,
					    STREAM_MESSAGE_STOP_DUMP_AUDIO,
					    NULL, 0))
		return false;

	while (true) {
		if (!chamelium_stream_read_header(client, &kind, &type,
						  &err, &len))
			return false;

		if (kind == STREAM_MESSAGE_RESPONSE)
			break;

		if (!read_and_discard(client->fd, len))
			return false;
	}

	if (type != STREAM_MESSAGE_STOP_DUMP_AUDIO) {
		igt_warn("Unexpected response type %d\n", type);
		return false;
	}
	if (err != STREAM_ERROR_NONE) {
		igt_warn("Received error: %s (%d)\n",
			 stream_error_str(err), err);
		return false;
	}
	if (len != 0) {
		igt_warn("Expected an empty response, got %zu bytes\n", len);
		return false;
	}

	return true;
}

/**
 * chamelium_stream_init:
 *
 * Connects to the Chamelium streaming server.
 */
struct chamelium_stream *chamelium_stream_init(void)
{
	struct chamelium_stream *client;

	client = calloc(1, sizeof(*client));

	if (!chamelium_stream_read_config(client))
		goto error_client;
	if (!chamelium_stream_connect(client))
		goto error_client;
	if (!chamelium_stream_check_version(client))
		goto error_fd;

	return client;

error_fd:
	close(client->fd);
error_client:
	free(client);
	return NULL;
}

void chamelium_stream_deinit(struct chamelium_stream *client)
{
	if (close(client->fd) != 0)
		igt_warn("close failed: %s\n", strerror(errno));
	free(client);
}
