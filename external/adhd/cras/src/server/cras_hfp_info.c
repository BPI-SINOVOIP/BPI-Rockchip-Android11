/* Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <syslog.h>

#include "audio_thread.h"
#include "byte_buffer.h"
#include "cras_hfp_info.h"
#include "cras_hfp_slc.h"
#include "cras_iodev_list.h"
#include "cras_plc.h"
#include "cras_sbc_codec.h"
#include "cras_server_metrics.h"
#include "utlist.h"

/* The max buffer size. Note that the actual used size must set to multiple
 * of SCO packet size, and the packet size does not necessarily be equal to
 * MTU. We should keep this as common multiple of possible packet sizes, for
 * example: 48, 60, 64, 128.
 */
#define MAX_HFP_BUF_SIZE_BYTES 28800

/* rate(8kHz) * sample_size(2 bytes) * channels(1) */
#define HFP_BYTE_RATE 16000

/* Per Bluetooth Core v5.0 and HFP 1.7 specification. */
#define MSBC_H2_HEADER_LEN 2
#define MSBC_FRAME_LEN 57
#define MSBC_FRAME_SIZE 59
#define MSBC_CODE_SIZE 240
#define MSBC_SYNC_WORD 0xAD

/* For one mSBC 1 compressed wideband audio channel the HCI packets will
 * be 3 octets of HCI header + 60 octets of data. */
#define MSBC_PKT_SIZE 60
#define WRITE_BUF_SIZE_BYTES MSBC_PKT_SIZE
#define HCI_SCO_HDR_SIZE_BYTES 3
#define HCI_SCO_PKT_SIZE (MSBC_PKT_SIZE + HCI_SCO_HDR_SIZE_BYTES)

#define H2_HEADER_0 0x01

/* Second octet of H2 header is composed by 4 bits fixed 0x8 and 4 bits
 * sequence number 0000, 0011, 1100, 1111. */
static const uint8_t h2_header_frames_count[] = { 0x08, 0x38, 0xc8, 0xf8 };

/* Structure to hold variables for a HFP connection. Since HFP supports
 * bi-direction audio, two iodevs should share one hfp_info if they
 * represent two directions of the same HFP headset
 * Members:
 *     fd - The file descriptor for SCO socket.
 *     started - If the hfp_info has started to read/write SCO data.
 *     mtu - The max transmit unit reported from BT adapter.
 *     packet_size - The size of SCO packet to read/write preferred by
 *         adapter, could be different than mtu.
 *     capture_buf - The buffer to hold samples read from SCO socket.
 *     playback_buf - The buffer to hold samples about to write to SCO socket.
 *     msbc_read - mSBC codec to decode input audio in wideband speech mode.
 *     msbc_write - mSBC codec to encode output audio in wideband speech mode.
 *     msbc_plc - PLC component to handle the packet loss of input audio in
 *         wideband speech mode.
 *     msbc_num_out_frames - Number of total written mSBC frames.
 *     msbc_num_in_frames - Number of total read mSBC frames.
 *     msbc_num_lost_frames - Number of total lost mSBC frames.
 *     read_cb - Callback to call when SCO socket can read. It returns the
 *         number of PCM bytes read.
 *     write_cb - Callback to call when SCO socket can write.
 *     hci_sco_buf - Buffer to read one HCI SCO packet.
 *     input_format_bytes - The audio format bytes for input device. 0 means
 *         there is no input device for the hfp_info.
 *     output_format_bytes - The audio format bytes for output device. 0 means
 *         there is no output device for the hfp_info.
 */
struct hfp_info {
	int fd;
	int started;
	unsigned int mtu;
	unsigned int packet_size;
	struct byte_buffer *capture_buf;
	struct byte_buffer *playback_buf;
	struct cras_audio_codec *msbc_read;
	struct cras_audio_codec *msbc_write;
	struct cras_msbc_plc *msbc_plc;
	unsigned int msbc_num_out_frames;
	unsigned int msbc_num_in_frames;
	unsigned int msbc_num_lost_frames;
	int (*read_cb)(struct hfp_info *info);
	int (*write_cb)(struct hfp_info *info);
	uint8_t write_buf[WRITE_BUF_SIZE_BYTES];
	uint8_t hci_sco_buf[HCI_SCO_PKT_SIZE];
	size_t input_format_bytes;
	size_t output_format_bytes;
};

int hfp_info_add_iodev(struct hfp_info *info,
		       enum CRAS_STREAM_DIRECTION direction,
		       struct cras_audio_format *format)
{
	if (direction == CRAS_STREAM_OUTPUT) {
		if (info->output_format_bytes)
			goto invalid;
		info->output_format_bytes = cras_get_format_bytes(format);

		buf_reset(info->playback_buf);
	} else if (direction == CRAS_STREAM_INPUT) {
		if (info->input_format_bytes)
			goto invalid;
		info->input_format_bytes = cras_get_format_bytes(format);

		buf_reset(info->capture_buf);
	}

	return 0;

invalid:
	return -EINVAL;
}

int hfp_info_rm_iodev(struct hfp_info *info,
		      enum CRAS_STREAM_DIRECTION direction)
{
	if (direction == CRAS_STREAM_OUTPUT && info->output_format_bytes) {
		memset(info->playback_buf->bytes, 0,
		       info->playback_buf->used_size);
		info->output_format_bytes = 0;
	} else if (direction == CRAS_STREAM_INPUT && info->input_format_bytes) {
		info->input_format_bytes = 0;
	} else {
		return -EINVAL;
	}

	return 0;
}

int hfp_info_has_iodev(struct hfp_info *info)
{
	return info->output_format_bytes || info->input_format_bytes;
}

void hfp_buf_acquire(struct hfp_info *info,
		     enum CRAS_STREAM_DIRECTION direction, uint8_t **buf,
		     unsigned *count)
{
	size_t format_bytes;
	unsigned int buf_avail;

	if (direction == CRAS_STREAM_OUTPUT && info->output_format_bytes) {
		*buf = buf_write_pointer_size(info->playback_buf, &buf_avail);
		format_bytes = info->output_format_bytes;
	} else if (direction == CRAS_STREAM_INPUT && info->input_format_bytes) {
		*buf = buf_read_pointer_size(info->capture_buf, &buf_avail);
		format_bytes = info->input_format_bytes;
	} else {
		*count = 0;
		return;
	}

	if (*count * format_bytes > buf_avail)
		*count = buf_avail / format_bytes;
}

int hfp_buf_size(struct hfp_info *info, enum CRAS_STREAM_DIRECTION direction)
{
	if (direction == CRAS_STREAM_OUTPUT && info->output_format_bytes)
		return info->playback_buf->used_size /
		       info->output_format_bytes;
	else if (direction == CRAS_STREAM_INPUT && info->input_format_bytes)
		return info->capture_buf->used_size / info->input_format_bytes;
	return 0;
}

void hfp_buf_release(struct hfp_info *info,
		     enum CRAS_STREAM_DIRECTION direction,
		     unsigned written_frames)
{
	if (direction == CRAS_STREAM_OUTPUT && info->output_format_bytes)
		buf_increment_write(info->playback_buf,
				    written_frames * info->output_format_bytes);
	else if (direction == CRAS_STREAM_INPUT && info->input_format_bytes)
		buf_increment_read(info->capture_buf,
				   written_frames * info->input_format_bytes);
	else
		written_frames = 0;
}

int hfp_buf_queued(struct hfp_info *info, enum CRAS_STREAM_DIRECTION direction)
{
	if (direction == CRAS_STREAM_OUTPUT && info->output_format_bytes)
		return buf_queued(info->playback_buf) /
		       info->output_format_bytes;
	else if (direction == CRAS_STREAM_INPUT && info->input_format_bytes)
		return buf_queued(info->capture_buf) / info->input_format_bytes;
	else
		return 0;
}

int hfp_fill_output_with_zeros(struct hfp_info *info, unsigned int nframes)
{
	unsigned int buf_avail;
	unsigned int nbytes;
	uint8_t *buf;
	int i;
	int ret = 0;

	if (info->output_format_bytes) {
		nbytes = nframes * info->output_format_bytes;
		/* Loop twice to make sure ring buffer is filled. */
		for (i = 0; i < 2; i++) {
			buf = buf_write_pointer_size(info->playback_buf,
						     &buf_avail);
			if (buf_avail == 0)
				break;
			buf_avail = MIN(nbytes, buf_avail);
			memset(buf, 0, buf_avail);
			buf_increment_write(info->playback_buf, buf_avail);
			nbytes -= buf_avail;
			ret += buf_avail / info->output_format_bytes;
		}
	}
	return ret;
}

void hfp_force_output_level(struct hfp_info *info, unsigned int level)
{
	if (info->output_format_bytes) {
		level *= info->output_format_bytes;
		level = MIN(level, MAX_HFP_BUF_SIZE_BYTES);
		buf_adjust_readable(info->playback_buf, level);
	}
}

int hfp_write_msbc(struct hfp_info *info)
{
	size_t encoded;
	int err;
	int pcm_encoded;
	unsigned int pcm_avail;
	uint8_t *samples;
	uint8_t *wp;

	samples = buf_read_pointer_size(info->playback_buf, &pcm_avail);
	wp = info->write_buf;
	if (pcm_avail >= MSBC_CODE_SIZE) {
		/* Encode more */
		wp[0] = H2_HEADER_0;
		wp[1] = h2_header_frames_count[info->msbc_num_out_frames % 4];
		pcm_encoded = info->msbc_write->encode(
			info->msbc_write, samples, pcm_avail,
			wp + MSBC_H2_HEADER_LEN,
			WRITE_BUF_SIZE_BYTES - MSBC_H2_HEADER_LEN, &encoded);
		if (pcm_encoded < 0) {
			syslog(LOG_ERR, "msbc encoding err: %s",
			       strerror(pcm_encoded));
			return pcm_encoded;
		}
		buf_increment_read(info->playback_buf, pcm_encoded);
		pcm_avail -= pcm_encoded;
	} else {
		memset(wp, 0, WRITE_BUF_SIZE_BYTES);
	}

msbc_send_again:
	err = send(info->fd, info->write_buf, MSBC_PKT_SIZE, 0);
	if (err < 0) {
		if (errno == EINTR)
			goto msbc_send_again;
		return err;
	}
	if (err != MSBC_PKT_SIZE) {
		syslog(LOG_ERR, "Partially write %d bytes for mSBC", err);
		return -1;
	}
	info->msbc_num_out_frames++;

	return err;
}

int hfp_write(struct hfp_info *info)
{
	int err = 0;
	unsigned to_send;
	uint8_t *samples;

	/* Write something */
	samples = buf_read_pointer_size(info->playback_buf, &to_send);
	if (to_send < info->packet_size)
		return 0;
	to_send = info->packet_size;

send_sample:
	err = send(info->fd, samples, to_send, 0);
	if (err < 0) {
		if (errno == EINTR)
			goto send_sample;

		return err;
	}

	if (err != (int)info->packet_size) {
		syslog(LOG_ERR,
		       "Partially write %d bytes for SCO packet size %u", err,
		       info->packet_size);
		return -1;
	}

	buf_increment_read(info->playback_buf, to_send);

	return err;
}

static int h2_header_get_seq(const uint8_t *p)
{
	int i;
	for (i = 0; i < 4; i++) {
		if (*p == h2_header_frames_count[i])
			return i;
	}
	return -1;
}

/*
 * Extract mSBC frame from SCO socket input bytes, given that the mSBC frame
 * could be lost or corrupted.
 * Args:
 *    input - Pointer to input bytes read from SCO socket.
 *    len - Length of input bytes.
 *    seq_out - To be filled by the sequence number of mSBC packet.
 * Returns:
 *    The starting position of mSBC frame if found.
 */
static const uint8_t *extract_msbc_frame(const uint8_t *input, int len,
					 unsigned int *seq_out)
{
	int rp = 0;
	int seq = -1;
	while (len - rp >= MSBC_FRAME_SIZE) {
		if ((input[rp] != H2_HEADER_0) ||
		    (input[rp + 2] != MSBC_SYNC_WORD)) {
			rp++;
			continue;
		}
		seq = h2_header_get_seq(input + rp + 1);
		if (seq < 0) {
			rp++;
			continue;
		}
		// `seq` is guaranteed to be positive now.
		*seq_out = (unsigned int)seq;
		return input + rp;
	}
	return NULL;
}

/*
 * Handle the case when mSBC frame is considered lost.
 * Args:
 *    info - The hfp_info instance holding mSBC codec and PLC objects.
 */
static int handle_packet_loss(struct hfp_info *info)
{
	int decoded;
	unsigned int pcm_avail;
	uint8_t *in_bytes;

	/* It's possible client doesn't consume data causing overrun. In that
	 * case we treat it as one mSBC frame read but dropped. */
	info->msbc_num_in_frames++;
	info->msbc_num_lost_frames++;

	in_bytes = buf_write_pointer_size(info->capture_buf, &pcm_avail);
	if (pcm_avail < MSBC_CODE_SIZE)
		return 0;

	decoded = cras_msbc_plc_handle_bad_frames(info->msbc_plc,
						  info->msbc_read, in_bytes);
	if (decoded < 0)
		return decoded;

	buf_increment_write(info->capture_buf, decoded);

	return decoded;
}

int hfp_read_msbc(struct hfp_info *info)
{
	int err = 0;
	unsigned int pcm_avail = 0;
	int decoded;
	size_t pcm_decoded = 0;
	size_t pcm_read = 0;
	uint8_t *capture_buf;
	const uint8_t *frame_head = NULL;
	unsigned int seq;

recv_msbc_bytes:
	err = recv(info->fd, info->hci_sco_buf, HCI_SCO_PKT_SIZE, 0);
	if (err < 0) {
		syslog(LOG_ERR, "HCI SCO packet read err %s", strerror(errno));
		if (errno == EINTR)
			goto recv_msbc_bytes;
		return err;
	}
	/*
	 * Treat return code 0 (socket shutdown) as error here. BT stack
	 * shall send signal to main thread for device disconnection.
	 */
	if (err != HCI_SCO_PKT_SIZE) {
		syslog(LOG_ERR, "Partially read %d bytes for mSBC packet", err);
		return -1;
	}

	/*
	 * HCI SCO packet status flag:
	 * 0x00 - correctly received data.
	 * 0x01 - possibly invalid data.
	 * 0x10 - No data received.
	 * 0x11 - Data partially lost.
	 */
	err = (info->hci_sco_buf[1] >> 4);
	if (err) {
		syslog(LOG_ERR, "HCI SCO status flag %u", err);
		return handle_packet_loss(info);
	}

	/* There is chance that erroneous data reporting gives us false positive.
	 * If mSBC frame extraction fails, we shall handle it as packet loss.
	 */
	frame_head =
		extract_msbc_frame(info->hci_sco_buf + HCI_SCO_HDR_SIZE_BYTES,
				   MSBC_PKT_SIZE, &seq);
	if (!frame_head) {
		syslog(LOG_ERR, "Failed to extract msbc frame");
		return handle_packet_loss(info);
	}

	/*
	 * Consider packet loss when found discontinuity in sequence number.
	 */
	while (seq != (info->msbc_num_in_frames % 4)) {
		syslog(LOG_ERR, "SCO packet seq unmatch");
		err = handle_packet_loss(info);
		if (err < 0)
			return err;
		pcm_read += err;
	}

	/* Check if there's room for more PCM. */
	capture_buf = buf_write_pointer_size(info->capture_buf, &pcm_avail);
	if (pcm_avail < MSBC_CODE_SIZE)
		return pcm_read;

	decoded = info->msbc_read->decode(info->msbc_read,
					  frame_head + MSBC_H2_HEADER_LEN,
					  MSBC_FRAME_LEN, capture_buf,
					  pcm_avail, &pcm_decoded);
	if (decoded < 0) {
		/*
		 * If mSBC frame cannot be decoded, consider this packet is
		 * corrupted and lost.
		 */
		syslog(LOG_ERR, "mSBC decode failed");
		err = handle_packet_loss(info);
		if (err < 0)
			return err;
		pcm_read += err;
	} else {
		/* Good mSBC frame decoded. */
		buf_increment_write(info->capture_buf, pcm_decoded);
		info->msbc_num_in_frames++;
		cras_msbc_plc_handle_good_frames(info->msbc_plc, capture_buf,
						 capture_buf);
		pcm_read += pcm_decoded;
	}
	return pcm_read;
}

int hfp_read(struct hfp_info *info)
{
	int err = 0;
	unsigned to_read;
	uint8_t *capture_buf;

	capture_buf = buf_write_pointer_size(info->capture_buf, &to_read);

	if (to_read < info->packet_size)
		return 0;
	to_read = info->packet_size;

recv_sample:
	err = recv(info->fd, capture_buf, to_read, 0);
	if (err < 0) {
		syslog(LOG_ERR, "Read error %s", strerror(errno));
		if (errno == EINTR)
			goto recv_sample;

		return err;
	}

	if (err != (int)info->packet_size) {
		/* Allow the SCO packet size be modified from the default MTU
		 * value to the size of SCO data we first read. This is for
		 * some adapters who prefers a different value than MTU for
		 * transmitting SCO packet.
		 */
		if (err && (info->packet_size == info->mtu)) {
			info->packet_size = err;
		} else {
			syslog(LOG_ERR,
			       "Partially read %d bytes for %u size SCO packet",
			       err, info->packet_size);
			return -1;
		}
	}

	buf_increment_write(info->capture_buf, err);

	return err;
}

/* Callback function to handle sample read and write.
 * Note that we poll the SCO socket for read sample, since it reflects
 * there is actual some sample to read while the socket always reports
 * writable even when device buffer is full.
 * The strategy is to synchronize read & write operations:
 * 1. Read one chunk of MTU bytes of data.
 * 2. When input device not attached, ignore the data just read.
 * 3. When output device attached, write one chunk of MTU bytes of data.
 */
static int hfp_info_callback(void *arg)
{
	struct hfp_info *info = (struct hfp_info *)arg;
	int err;

	if (!info->started)
		return 0;

	err = info->read_cb(info);
	if (err < 0) {
		syslog(LOG_ERR, "Read error");
		goto read_write_error;
	}

	/* Ignore the bytes just read if input dev not in present */
	if (!info->input_format_bytes)
		buf_increment_read(info->capture_buf, err);

	/* Without output stream's presence, we shall still send zero packets
	 * to HF. This is required for some HF devices to start sending non-zero
	 * data to AG.
	 */
	if (!info->output_format_bytes)
		buf_increment_write(info->playback_buf,
				    info->msbc_write ? err : info->packet_size);

	err = info->write_cb(info);
	if (err < 0) {
		syslog(LOG_ERR, "Write error");
		goto read_write_error;
	}

	return 0;

read_write_error:
	/*
	 * This callback is executing in audio thread, so it's safe to
	 * unregister itself by audio_thread_rm_callback().
	 */
	audio_thread_rm_callback(info->fd);
	close(info->fd);
	info->fd = 0;
	info->started = 0;

	return 0;
}

struct hfp_info *hfp_info_create(int codec)
{
	struct hfp_info *info;
	info = (struct hfp_info *)calloc(1, sizeof(*info));
	if (!info)
		goto error;

	info->capture_buf = byte_buffer_create(MAX_HFP_BUF_SIZE_BYTES);
	if (!info->capture_buf)
		goto error;

	info->playback_buf = byte_buffer_create(MAX_HFP_BUF_SIZE_BYTES);
	if (!info->playback_buf)
		goto error;

	if (codec == HFP_CODEC_ID_MSBC) {
		info->write_cb = hfp_write_msbc;
		info->read_cb = hfp_read_msbc;
		info->msbc_read = cras_msbc_codec_create();
		info->msbc_write = cras_msbc_codec_create();
		info->msbc_plc = cras_msbc_plc_create();
	} else {
		info->write_cb = hfp_write;
		info->read_cb = hfp_read;
	}

	return info;

error:
	if (info) {
		if (info->capture_buf)
			byte_buffer_destroy(&info->capture_buf);
		if (info->playback_buf)
			byte_buffer_destroy(&info->playback_buf);
		free(info);
	}
	return NULL;
}

int hfp_info_running(struct hfp_info *info)
{
	return info->started;
}

int hfp_info_start(int fd, unsigned int mtu, struct hfp_info *info)
{
	info->fd = fd;
	info->mtu = mtu;

	/* Initialize to MTU, it may change when actually read the socket. */
	info->packet_size = mtu;
	buf_reset(info->playback_buf);
	buf_reset(info->capture_buf);

	audio_thread_add_callback(info->fd, hfp_info_callback, info);

	info->started = 1;
	info->msbc_num_out_frames = 0;
	info->msbc_num_in_frames = 0;
	info->msbc_num_lost_frames = 0;

	return 0;
}

int hfp_info_stop(struct hfp_info *info)
{
	if (!info->started)
		return 0;

	audio_thread_rm_callback_sync(cras_iodev_list_get_audio_thread(),
				      info->fd);

	close(info->fd);
	info->fd = 0;
	info->started = 0;

	if (info->msbc_num_in_frames) {
		cras_server_metrics_hfp_packet_loss(
			(float)info->msbc_num_lost_frames /
			info->msbc_num_in_frames);
	}

	return 0;
}

void hfp_info_destroy(struct hfp_info *info)
{
	if (info->capture_buf)
		byte_buffer_destroy(&info->capture_buf);

	if (info->playback_buf)
		byte_buffer_destroy(&info->playback_buf);

	if (info->msbc_read)
		cras_sbc_codec_destroy(info->msbc_read);
	if (info->msbc_write)
		cras_sbc_codec_destroy(info->msbc_write);
	if (info->msbc_plc)
		cras_msbc_plc_destroy(info->msbc_plc);

	free(info);
}
