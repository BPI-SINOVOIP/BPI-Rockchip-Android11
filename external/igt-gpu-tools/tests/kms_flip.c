/*
 * Copyright 2012 Intel Corporation
 *   Jesse Barnes <jesse.barnes@intel.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#include "config.h"

#include "igt.h"

#if defined(USE_CAIRO_PIXMAN)
#include <cairo.h>
#endif
#include <errno.h>
#include <fcntl.h>
#include <math.h>
#include <stdint.h>
#include <unistd.h>
#include <sys/poll.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#ifdef HAVE_LINUX_KD_H
#include <linux/kd.h>
#elif HAVE_SYS_KD_H
#include <sys/kd.h>
#endif
#include <time.h>
#include <pthread.h>

#include "igt_stats.h"

#define TEST_DPMS		(1 << 0)

#define TEST_PAN		(1 << 3)
#define TEST_MODESET		(1 << 4)
#define TEST_CHECK_TS		(1 << 5)
#define TEST_EBUSY		(1 << 6)
#define TEST_EINVAL		(1 << 7)
#define TEST_FLIP		(1 << 8)
#define TEST_VBLANK		(1 << 9)
#define TEST_VBLANK_BLOCK	(1 << 10)
#define TEST_VBLANK_ABSOLUTE	(1 << 11)
#define TEST_VBLANK_EXPIRED_SEQ	(1 << 12)
#define TEST_FB_RECREATE	(1 << 13)
#define TEST_RMFB		(1 << 14)
#define TEST_HANG		(1 << 15)
#define TEST_NOEVENT		(1 << 16)

#define TEST_SINGLE_BUFFER	(1 << 18)
#define TEST_DPMS_OFF		(1 << 19)
#define TEST_NO_2X_OUTPUT	(1 << 20)
#define TEST_DPMS_OFF_OTHERS	(1 << 21)
#define TEST_ENOENT		(1 << 22)
#define TEST_FENCE_STRESS	(1 << 23)
#define TEST_VBLANK_RACE	(1 << 24)
#define TEST_SUSPEND		(1 << 26)
#define TEST_BO_TOOBIG		(1 << 28)

#define TEST_NO_VBLANK		(1 << 29)
#define TEST_BASIC		(1 << 30)

#define EVENT_FLIP		(1 << 0)
#define EVENT_VBLANK		(1 << 1)

#define RUN_TEST		1
#define RUN_PAIR		2

#ifndef DRM_CAP_TIMESTAMP_MONOTONIC
#define DRM_CAP_TIMESTAMP_MONOTONIC 6
#endif

drmModeRes *resources;
int drm_fd;
static drm_intel_bufmgr *bufmgr;
struct intel_batchbuffer *batch;
uint32_t devid;
int test_time = 3;
static bool monotonic_timestamp;
static pthread_t vblank_wait_thread;

static drmModeConnector *last_connector;

uint32_t *fb_ptr;

struct type_name {
	int type;
	const char *name;
};

struct event_state {
	const char *name;

	/*
	 * Event data for the last event that has already passed our check.
	 * Updated using the below current_* vars in update_state().
	 */
	struct timeval last_ts;			/* kernel reported timestamp */
	struct timeval last_received_ts;	/* the moment we received it */
	unsigned int last_seq;			/* kernel reported seq. num */

	/*
	 * Event data for for the current event that we just received and
	 * going to check for validity. Set in event_handler().
	 */
	struct timeval current_ts;		/* kernel reported timestamp */
	struct timeval current_received_ts;	/* the moment we received it */
	unsigned int current_seq;		/* kernel reported seq. num */

	int count;				/* # of events of this type */

	/* Step between the current and next 'target' sequence number. */
	int seq_step;
};

static bool vblank_dependence(int flags)
{
	int vblank_flags = TEST_VBLANK | TEST_VBLANK_BLOCK |
			   TEST_VBLANK_ABSOLUTE | TEST_VBLANK_EXPIRED_SEQ |
			   TEST_CHECK_TS | TEST_VBLANK_RACE | TEST_EBUSY;

	if (flags & vblank_flags)
		return true;

	return false;
}

static float timeval_float(const struct timeval *tv)
{
	return tv->tv_sec + tv->tv_usec / 1000000.0f;
}

static void dump_event_state(const struct event_state *es)
{
	igt_debug("name = %s\n"
		  "last_ts = %.06f\n"
		  "last_received_ts = %.06f\n"
		  "last_seq = %u\n"
		  "current_ts = %.06f\n"
		  "current_received_ts = %.06f\n"
		  "current_seq = %u\n"
		  "count = %u\n"
		  "seq_step = %d\n",
		  es->name,
		  timeval_float(&es->last_ts),
		  timeval_float(&es->last_received_ts),
		  es->last_seq,
		  timeval_float(&es->current_ts),
		  timeval_float(&es->current_received_ts),
		  es->current_seq,
		  es->count,
		  es->seq_step);
}

struct test_output {
	int mode_valid;
	drmModeModeInfo kmode[4];
	drmModeEncoder *kencoder[4];
	drmModeConnector *kconnector[4];
	uint32_t _connector[4];
	uint32_t _crtc[4];
	int _pipe[4];
	int count; /* 1:1 mapping between crtc:connector */
	int flags;
	int pipe; /* primary pipe for vblank */
	unsigned int current_fb_id;
	unsigned int fb_width;
	unsigned int fb_height;
	unsigned int fb_ids[3];
	int bpp, depth;
	struct igt_fb fb_info[3];

	struct event_state flip_state;
	struct event_state vblank_state;
	/* Overall step between each round */
	int seq_step;
	unsigned int pending_events;
	int flip_count;

	double vblank_interval;
};


static unsigned long gettime_us(void)
{
	struct timespec ts;

	clock_gettime(CLOCK_MONOTONIC, &ts);

	return ts.tv_sec * 1000000 + ts.tv_nsec / 1000;
}

static void emit_fence_stress(struct test_output *o)
{
	const int num_fences = gem_available_fences(drm_fd);
	struct igt_fb *fb_info = &o->fb_info[o->current_fb_id];
	struct drm_i915_gem_execbuffer2 execbuf;
	struct drm_i915_gem_exec_object2 *exec;
	uint32_t buf[2] = { MI_BATCH_BUFFER_END, 0 };
	drm_intel_bo **bo;
	int i;

	igt_require(bufmgr);

	bo = calloc(sizeof(*bo), num_fences);
	exec = calloc(sizeof(*exec), num_fences+1);
	for (i = 0; i < num_fences - 1; i++) {
		uint32_t tiling = I915_TILING_X;
		unsigned long pitch = 0;
		bo[i] = drm_intel_bo_alloc_tiled(bufmgr,
						 "X tiled bo", 1024, 1024, 4,
						 &tiling, &pitch, 0);
		exec[i].handle = bo[i]->handle;
		exec[i].flags = EXEC_OBJECT_NEEDS_FENCE;
	}
	exec[i].handle = fb_info->gem_handle;
	exec[i].flags = EXEC_OBJECT_NEEDS_FENCE;
	exec[++i].handle = gem_create(drm_fd, 4096);
	gem_write(drm_fd, exec[i].handle, 0, buf, sizeof(buf));

	memset(&execbuf, 0, sizeof(execbuf));
	execbuf.buffers_ptr = (uintptr_t)exec;
	execbuf.buffer_count = i + 1;
	execbuf.batch_len = sizeof(buf);
	if (HAS_BLT_RING(intel_get_drm_devid(drm_fd)))
		execbuf.flags = I915_EXEC_BLT;

	gem_execbuf(drm_fd, &execbuf);

	gem_close(drm_fd, exec[i].handle);
	for (i = 0; i < num_fences - 1; i++)
		drm_intel_bo_unreference(bo[i]);
	free(bo);
	free(exec);
}

static void dpms_off_other_outputs(struct test_output *o)
{
	int i, n;
	drmModeConnector *connector;
	uint32_t connector_id;

	for (i = 0; i < resources->count_connectors; i++) {
		connector_id = resources->connectors[i];

		for (n = 0; n < o->count; n++) {
			if (connector_id == o->kconnector[n]->connector_id)
				goto next;
		}

		connector = drmModeGetConnectorCurrent(drm_fd, connector_id);

		kmstest_set_connector_dpms(drm_fd, connector,  DRM_MODE_DPMS_ON);
		kmstest_set_connector_dpms(drm_fd, connector,  DRM_MODE_DPMS_OFF);

		drmModeFreeConnector(connector);
next:
		;
	}
}

static void set_dpms(struct test_output *o, int mode)
{
	for (int n = 0; n < o->count; n++)
		kmstest_set_connector_dpms(drm_fd, o->kconnector[n], mode);
}

static void set_flag(unsigned int *v, unsigned int flag)
{
	igt_assert(!(*v & flag));
	*v |= flag;
}

static void clear_flag(unsigned int *v, unsigned int flag)
{
	igt_assert(*v & flag);
	*v &= ~flag;
}

static int do_page_flip(struct test_output *o, uint32_t fb_id, bool event)
{
	int n, ret = 0;

	o->flip_count = 0;

	for (n = 0; ret == 0 && n < o->count; n++)
		ret = drmModePageFlip(drm_fd, o->_crtc[n], fb_id,
				      event ? DRM_MODE_PAGE_FLIP_EVENT : 0,
				      event ? (void *)((unsigned long)o | (n==0)) : NULL);

	if (ret == 0 && event)
		set_flag(&o->pending_events, EVENT_FLIP);

	return ret;
}

struct vblank_reply {
	unsigned int sequence;
	struct timeval ts;
};

static int __wait_for_vblank(unsigned int flags, int crtc_idx,
			      int target_seq, unsigned long ret_data,
			      struct vblank_reply *reply)
{
	drmVBlank wait_vbl;
	int ret;
	uint32_t pipe_id_flag;
	bool event = !(flags & TEST_VBLANK_BLOCK);

	memset(&wait_vbl, 0, sizeof(wait_vbl));
	pipe_id_flag = kmstest_get_vbl_flag(crtc_idx);

	wait_vbl.request.type = pipe_id_flag;
	if (flags & TEST_VBLANK_ABSOLUTE)
		wait_vbl.request.type |= DRM_VBLANK_ABSOLUTE;
	else
		wait_vbl.request.type |= DRM_VBLANK_RELATIVE;
	if (event) {
		wait_vbl.request.type |= DRM_VBLANK_EVENT;
		wait_vbl.request.signal = ret_data;
	}
	wait_vbl.request.sequence = target_seq;

	ret = drmWaitVBlank(drm_fd, &wait_vbl);

	if (ret == 0) {
		reply->ts.tv_sec = wait_vbl.reply.tval_sec;
		reply->ts.tv_usec = wait_vbl.reply.tval_usec;
		reply->sequence = wait_vbl.reply.sequence;
	} else
		ret = -errno;

	return ret;
}

static int do_wait_for_vblank(struct test_output *o, int pipe_id,
			      int target_seq, struct vblank_reply *reply)
{
	int ret;
	unsigned flags = o->flags;

	/* Absolute waits only works once we have a frame counter. */
	if (!(o->vblank_state.count > 0))
		flags &= ~TEST_VBLANK_ABSOLUTE;

	ret = __wait_for_vblank(flags, pipe_id, target_seq, (unsigned long)o,
				reply);
	if (ret == 0 && !(o->flags & TEST_VBLANK_BLOCK))
		set_flag(&o->pending_events, EVENT_VBLANK);

	return ret;
}

static bool
analog_tv_connector(const struct test_output *o)
{
	uint32_t connector_type = o->kconnector[0]->connector_type;

	return connector_type == DRM_MODE_CONNECTOR_TV ||
		connector_type == DRM_MODE_CONNECTOR_9PinDIN ||
		connector_type == DRM_MODE_CONNECTOR_SVIDEO ||
		connector_type == DRM_MODE_CONNECTOR_Composite;
}

static void event_handler(struct event_state *es, unsigned int frame,
			  unsigned int sec, unsigned int usec)
{
	struct timeval now;

	if (monotonic_timestamp) {
		struct timespec ts;

		clock_gettime(CLOCK_MONOTONIC, &ts);
		now.tv_sec = ts.tv_sec;
		now.tv_usec = ts.tv_nsec / 1000;
	} else {
		gettimeofday(&now, NULL);
	}
	es->current_received_ts = now;

	es->current_ts.tv_sec = sec;
	es->current_ts.tv_usec = usec;
	es->current_seq = frame;
}

static void page_flip_handler(int fd, unsigned int frame, unsigned int sec,
			      unsigned int usec, void *data)
{
	int primary = (unsigned long)data & 1;
	struct test_output *o = (void *)((unsigned long)data & ~ 1);

	if (++o->flip_count == o->count)
		clear_flag(&o->pending_events, EVENT_FLIP);
	if (primary)
		event_handler(&o->flip_state, frame, sec, usec);
}

static double mode_frame_time(const struct test_output *o)
{
	return 1000.0 * o->kmode[0].htotal * o->kmode[0].vtotal / o->kmode[0].clock;
}

static double actual_frame_time(const struct test_output *o)
{
	igt_assert(o->flags & TEST_CHECK_TS);
	return o->vblank_interval;
}

static void *vblank_wait_thread_func(void *data)
{
	struct test_output *o = data;
	struct vblank_reply reply;
	int i;

	for (i = 0; i < 32; i++) {
		unsigned long start = gettime_us();
		__wait_for_vblank(TEST_VBLANK_BLOCK, o->pipe, 20, (unsigned long)o, &reply);
		if (gettime_us() - start > 2 * mode_frame_time(o))
			return (void*)1;
	}

	return 0;
}

static void spawn_vblank_wait_thread(struct test_output *o)
{
	igt_assert(pthread_create(&vblank_wait_thread, NULL,
				  vblank_wait_thread_func, o) == 0);
}

static void join_vblank_wait_thread(void)
{
	igt_assert(pthread_join(vblank_wait_thread, NULL) == 0);
}

static void fixup_premature_vblank_ts(struct test_output *o,
				      struct event_state *es)
{
	/*
	 * In case a power off event preempts the completion of a
	 * wait-for-vblank event the kernel will return a wf-vblank event with
	 * a zeroed-out timestamp. In order that check_state() doesn't
	 * complain replace this ts with a valid ts. As we can't calculate the
	 * exact timestamp, just use the time we received the event.
	 */
	struct timeval tv;

	if (!(o->flags & (TEST_DPMS | TEST_MODESET)))
		return;

	if (o->vblank_state.current_ts.tv_sec != 0 ||
	    o->vblank_state.current_ts.tv_usec != 0)
		return;

	tv.tv_sec = 0;
	tv.tv_usec = 1;
	timersub(&es->current_received_ts, &tv, &es->current_ts);
}

static void vblank_handler(int fd, unsigned int frame, unsigned int sec,
			      unsigned int usec, void *data)
{
	struct test_output *o = data;

	clear_flag(&o->pending_events, EVENT_VBLANK);
	event_handler(&o->vblank_state, frame, sec, usec);
	fixup_premature_vblank_ts(o, &o->vblank_state);
}

static void check_state(const struct test_output *o, const struct event_state *es)
{
	struct timeval diff;

	dump_event_state(es);

	timersub(&es->current_ts, &es->current_received_ts, &diff);
	if (!analog_tv_connector(o)) {
		igt_assert_f(diff.tv_sec < 0 || (diff.tv_sec == 0 && diff.tv_usec <= 2000),
			     "%s ts delayed for too long: %.06f\n",
			     es->name, timeval_float(&diff));
	}

	if (es->count == 0)
		return;

	timersub(&es->current_ts, &es->last_received_ts, &diff);
	igt_assert_f(timercmp(&es->last_received_ts, &es->current_ts, <),
		     "%s ts before the %s was issued!\n"
		     "timerdiff %.06f\n",
		     es->name, es->name, timeval_float(&diff));

	/* check only valid if no modeset happens in between, that increments by
	 * (1 << 23) on each step. This bounding matches the one in
	 * DRM_IOCTL_WAIT_VBLANK. */
	if (!(o->flags & (TEST_DPMS | TEST_MODESET | TEST_NO_VBLANK)))
		igt_assert_f(es->current_seq - (es->last_seq + o->seq_step) <= 1UL << 23,
			     "unexpected %s seq %u, should be >= %u\n",
			     es->name, es->current_seq, es->last_seq + o->seq_step);

	if (o->flags & TEST_CHECK_TS) {
		double elapsed, expected;

		timersub(&es->current_ts, &es->last_ts, &diff);
		elapsed = 1e6*diff.tv_sec + diff.tv_usec;
		expected = (es->current_seq - es->last_seq) * actual_frame_time(o);

		igt_debug("%s ts/seq: last %.06f/%u, current %.06f/%u: elapsed=%.1fus expected=%.1fus +- %.1fus, error %.1f%%\n",
			  es->name, timeval_float(&es->last_ts), es->last_seq,
			  timeval_float(&es->current_ts), es->current_seq,
			  elapsed, expected, expected * 0.005,
			  fabs((elapsed - expected) / expected) * 100);

		igt_assert_f(fabs((elapsed - expected) / expected) <= 0.005,
			     "inconsistent %s ts/seq: last %.06f/%u, current %.06f/%u: elapsed=%.1fus expected=%.1fus\n",
			     es->name, timeval_float(&es->last_ts), es->last_seq,
			     timeval_float(&es->current_ts), es->current_seq,
			     elapsed, expected);

		igt_assert_f(es->current_seq == es->last_seq + o->seq_step,
			     "unexpected %s seq %u, expected %u\n",
			     es->name, es->current_seq,
			     es->last_seq + o->seq_step);
	}
}

static void check_state_correlation(struct test_output *o,
				    struct event_state *es1,
				    struct event_state *es2)
{
	struct timeval tv_diff;
	double ftime;
	double usec_diff;
	int seq_diff;

	if (es1->count == 0 || es2->count == 0)
		return;

	timersub(&es2->current_ts, &es1->current_ts, &tv_diff);
	usec_diff = tv_diff.tv_sec * USEC_PER_SEC + tv_diff.tv_usec;

	seq_diff = es2->current_seq - es1->current_seq;
	ftime = mode_frame_time(o);
	usec_diff -= seq_diff * ftime;

	igt_assert_f(fabs(usec_diff) / ftime <= 0.005,
		     "timestamp mismatch between %s and %s (diff %.6f sec)\n",
		     es1->name, es2->name, usec_diff / USEC_PER_SEC);
}

static void check_all_state(struct test_output *o,
			    unsigned int completed_events)
{
	bool flip, vblank;

	flip = completed_events & EVENT_FLIP;
	vblank = completed_events & EVENT_VBLANK;

	if (flip)
		check_state(o, &o->flip_state);
	if (vblank)
		check_state(o, &o->vblank_state);

	/* FIXME: Correlation check is broken. */
	if (flip && vblank && 0)
		check_state_correlation(o, &o->flip_state, &o->vblank_state);
}

static void recreate_fb(struct test_output *o)
{
	drmModeFBPtr r;
	struct igt_fb *fb_info = &o->fb_info[o->current_fb_id];
	uint32_t new_fb_id;

	/* Call rmfb/getfb/addfb to ensure those don't introduce stalls */
	r = drmModeGetFB(drm_fd, fb_info->fb_id);
	igt_assert(r);

	do_or_die(drmModeAddFB(drm_fd, o->fb_width, o->fb_height, o->depth,
			       o->bpp, fb_info->strides[0],
			       r->handle, &new_fb_id));

	gem_close(drm_fd, r->handle);
	drmFree(r);
	do_or_die(drmModeRmFB(drm_fd, fb_info->fb_id));

	o->fb_ids[o->current_fb_id] = new_fb_id;
	o->fb_info[o->current_fb_id].fb_id = new_fb_id;
}

static igt_hang_t hang_gpu(int fd)
{
#if defined(USE_INTEL)
	return igt_hang_ring(fd, I915_EXEC_DEFAULT);
#else
	igt_hang_t ret = {};
	return ret;
#endif
}

static void unhang_gpu(int fd, igt_hang_t hang)
{
#if defined(USE_INTEL)
	igt_post_hang_ring(fd, hang);
#endif
}

static bool is_wedged(int fd)
{
	if (drmIoctl(fd, DRM_IOCTL_I915_GEM_THROTTLE, 0) == 0)
		return false;

	return errno == EIO;
}

static int set_mode(struct test_output *o, uint32_t fb, int x, int y)
{
	int n, ret;

	for (n = o->count - 1; n >= 0; n--) {
		uint32_t buffer_id = fb, x_crtc = x, y_crtc = y;
		uint32_t *conn = &o->_connector[n];
		int count = 1;
		drmModeModeInfoPtr mode = &o->kmode[n];

		if (fb == 0) {
			buffer_id = x_crtc = y_crtc = count = 0;
			conn = NULL; mode = NULL;
		}

		ret = drmModeSetCrtc(drm_fd, o->_crtc[n],
				     buffer_id, x_crtc, y_crtc,
				     conn, count, mode);
		if (ret)
			return ret;
	}

	return 0;
}

/* Return mask of completed events. */
static unsigned int run_test_step(struct test_output *o)
{
	unsigned int new_fb_id;
	/* for funny reasons page_flip returns -EBUSY on disabled crtcs ... */
	int expected_einval = o->flags & TEST_MODESET ? -EBUSY : -EINVAL;
	unsigned int completed_events = 0;
	bool do_flip;
	bool do_vblank;
	struct vblank_reply vbl_reply;
	unsigned int target_seq;
	igt_hang_t hang;

	target_seq = o->vblank_state.seq_step;
	/* Absolute waits only works once we have a frame counter. */
	if (o->flags & TEST_VBLANK_ABSOLUTE && o->vblank_state.count > 0)
		target_seq += o->vblank_state.last_seq;

	/*
	 * It's possible that we don't have a pending flip here, in case both
	 * wf-vblank and flip were scheduled and the wf-vblank event was
	 * delivered earlier. The same applies to vblank events w.r.t flip.
	 */
	do_flip = (o->flags & TEST_FLIP) && !(o->pending_events & EVENT_FLIP);
	do_vblank = (o->flags & TEST_VBLANK) &&
		    !(o->pending_events & EVENT_VBLANK);

	if (o->flags & TEST_DPMS_OFF_OTHERS)
		dpms_off_other_outputs(o);

	if (!(o->flags & TEST_SINGLE_BUFFER))
		o->current_fb_id = !o->current_fb_id;

	if (o->flags & TEST_FB_RECREATE)
		recreate_fb(o);
	new_fb_id = o->fb_ids[o->current_fb_id];

	if ((o->flags & TEST_VBLANK_EXPIRED_SEQ) &&
	    !(o->pending_events & EVENT_VBLANK) && o->flip_state.count > 0) {
		struct vblank_reply reply;
		unsigned int exp_seq;
		unsigned long start, end;

		exp_seq = o->flip_state.current_seq;
		start = gettime_us();
		do_or_die(__wait_for_vblank(TEST_VBLANK_ABSOLUTE |
					    TEST_VBLANK_BLOCK, o->pipe, exp_seq,
					    0, &reply));
		end = gettime_us();
		igt_debug("Vblank took %luus\n", end - start);
		igt_assert(end - start < 500);
		igt_assert_eq(reply.sequence, exp_seq);
		igt_assert(timercmp(&reply.ts, &o->flip_state.last_ts, ==));
	}

	if (o->flags & TEST_ENOENT) {
		/* hope that fb 0xfffffff0 does not exist */
		igt_assert_eq(do_page_flip(o, 0xfffffff0, false), -ENOENT);
		igt_assert_eq(set_mode(o, 0xfffffff0, 0, 0), -ENOENT);
	}

	if (do_flip && (o->flags & TEST_EINVAL) && o->flip_state.count > 0)
		igt_assert_eq(do_page_flip(o, new_fb_id, false), expected_einval);

	if (do_vblank && (o->flags & TEST_EINVAL) && o->vblank_state.count > 0)
		igt_assert_eq(do_wait_for_vblank(o, o->pipe, target_seq, &vbl_reply), -EINVAL);

	if (o->flags & TEST_VBLANK_RACE) {
		spawn_vblank_wait_thread(o);

		if (o->flags & TEST_MODESET)
			igt_assert_f(set_mode(o, 0 /* no fb */, 0, 0) == 0,
				     "failed to disable output: %s\n",
				     strerror(errno));
	}

	if (o->flags & TEST_DPMS_OFF)
		set_dpms(o, DRM_MODE_DPMS_OFF);

	if (o->flags & TEST_MODESET)
		igt_assert(set_mode(o, o->fb_ids[o->current_fb_id], 0, 0) == 0);

	if (o->flags & TEST_DPMS)
		set_dpms(o, DRM_MODE_DPMS_ON);

	if (o->flags & TEST_VBLANK_RACE) {
		struct vblank_reply reply;
		unsigned long start, end;

		/* modeset/DPMS is done, vblank wait should work normally now */
		start = gettime_us();
		igt_assert(__wait_for_vblank(TEST_VBLANK_BLOCK, o->pipe, 2, 0, &reply) == 0);
		end = gettime_us();
		/*
		 * we waited for two vblanks, so verify that
		 * we were blocked for ~1-2 frames.
		 */
		igt_assert_f(end - start > 0.9 * mode_frame_time(o) &&
			     end - start < 2.1 * mode_frame_time(o),
			     "wait for two vblanks took %lu usec (frame time %f usec)\n",
			     end - start, mode_frame_time(o));
		join_vblank_wait_thread();
	}

	igt_print_activity();

	memset(&hang, 0, sizeof(hang));
	if (do_flip && (o->flags & TEST_HANG))
		hang = hang_gpu(drm_fd);

	/* try to make sure we can issue two flips during the same frame */
	if (do_flip && (o->flags & TEST_EBUSY)) {
		struct vblank_reply reply;
		igt_assert(__wait_for_vblank(TEST_VBLANK_BLOCK, o->pipe, 1, 0, &reply) == 0);
	}

	if (do_flip)
		do_or_die(do_page_flip(o, new_fb_id, !(o->flags & TEST_NOEVENT)));

	if (o->flags & TEST_FENCE_STRESS)
		emit_fence_stress(o);

	if (do_vblank) {
		do_or_die(do_wait_for_vblank(o, o->pipe, target_seq,
					     &vbl_reply));
		if (o->flags & TEST_VBLANK_BLOCK) {
			event_handler(&o->vblank_state, vbl_reply.sequence,
				      vbl_reply.ts.tv_sec,
				      vbl_reply.ts.tv_usec);
			completed_events = EVENT_VBLANK;
		}
	}

	if (do_flip && (o->flags & TEST_EBUSY))
		igt_assert_eq(do_page_flip(o, new_fb_id, false), -EBUSY);

	if (do_flip && (o->flags & TEST_RMFB))
		recreate_fb(o);

	/* pan before the flip completes */
	if (o->flags & TEST_PAN) {
		int count = do_flip ?
			o->flip_state.count : o->vblank_state.count;
		int width = o->fb_width - o->kmode[0].hdisplay;
		int x_ofs = count * 10 % (2 * width);
		if (x_ofs >= width)
			x_ofs = 2 * width - x_ofs;

		/* Make sure DSPSURF changes value */
		if (o->flags & TEST_HANG)
			o->current_fb_id = !o->current_fb_id;

		igt_assert_f(set_mode(o, o->fb_ids[o->current_fb_id], x_ofs, 0) == 0,
			     "failed to pan (%dx%d@%dHz)+%d: %s\n",
			     o->kmode[0].hdisplay, o->kmode[0].vdisplay, o->kmode[0].vrefresh,
			     x_ofs, strerror(errno));
	}

	if (o->flags & TEST_DPMS)
		set_dpms(o, DRM_MODE_DPMS_OFF);

	if (o->flags & TEST_MODESET && !(o->flags & TEST_RMFB) && !(o->flags & TEST_VBLANK_RACE))
		igt_assert_f(set_mode(o, 0 /* no fb */, 0, 0) == 0,
			     "failed to disable output: %s\n",
			     strerror(errno));

	if (o->flags & TEST_SUSPEND)
		igt_system_suspend_autoresume(SUSPEND_STATE_MEM,
					      SUSPEND_TEST_NONE);

	if (do_vblank && (o->flags & TEST_EINVAL) && o->vblank_state.count > 0)
		igt_assert(do_wait_for_vblank(o, o->pipe, target_seq, &vbl_reply)
			   == -EINVAL);

	if (do_flip && (o->flags & TEST_EINVAL))
		igt_assert(do_page_flip(o, new_fb_id, false) == expected_einval);

	unhang_gpu(drm_fd, hang);

	return completed_events;
}

static void update_state(struct event_state *es)
{
	es->last_received_ts = es->current_received_ts;
	es->last_ts = es->current_ts;
	es->last_seq = es->current_seq;
	es->count++;
}

static void update_all_state(struct test_output *o,
			     unsigned int completed_events)
{
	if (completed_events & EVENT_FLIP)
		update_state(&o->flip_state);

	if (completed_events & EVENT_VBLANK)
		update_state(&o->vblank_state);
}

static void connector_find_preferred_mode(uint32_t connector_id, int crtc_idx,
					  struct test_output *o)
{
	struct kmstest_connector_config config;

	if (!kmstest_get_connector_config(drm_fd, connector_id, 1 << crtc_idx,
					  &config)) {
		o->mode_valid = 0;
		return;
	}

	o->pipe = config.pipe;
	o->kconnector[0] = config.connector;
	o->kencoder[0] = config.encoder;
	o->_crtc[0] = config.crtc->crtc_id;
	o->_pipe[0] = config.pipe;
	o->kmode[0] = config.default_mode;
	o->mode_valid = 1;

	o->fb_width = o->kmode[0].hdisplay;
	o->fb_height = o->kmode[0].vdisplay;

	drmModeFreeCrtc(config.crtc);
}

static bool mode_compatible(const drmModeModeInfo *a, const drmModeModeInfo *b)
{
	int d_refresh;

	if (a->hdisplay != b->hdisplay)
		return false;

	if (a->vdisplay != b->vdisplay)
		return false;

	d_refresh = a->vrefresh - b->vrefresh;
	if (d_refresh < -1 || d_refresh > 1)
		return false;

	return true;
}

static void connector_find_compatible_mode(int crtc_idx0, int crtc_idx1,
					   struct test_output *o)
{
	struct kmstest_connector_config config[2];
	drmModeModeInfo *mode[2];
	int n, m;

	if (!kmstest_get_connector_config(drm_fd, o->_connector[0],
					  1 << crtc_idx0, &config[0]))
		return;

	if (!kmstest_get_connector_config(drm_fd, o->_connector[1],
					  1 << crtc_idx1, &config[1])) {
		kmstest_free_connector_config(&config[0]);
		return;
	}

	mode[0] = &config[0].default_mode;
	mode[1] = &config[1].default_mode;
	if (!mode_compatible(mode[0], mode[1])) {
		for (n = 0; n < config[0].connector->count_modes; n++) {
			mode[0] = &config[0].connector->modes[n];
			for (m = 0; m < config[1].connector->count_modes; m++) {
				mode[1] = &config[1].connector->modes[m];
				if (mode_compatible(mode[0], mode[1]))
					goto found;
			}
		}

		/* hope for the best! */
		mode[1] = mode[0] = &config[0].default_mode;
	}

found:
	o->pipe = config[0].pipe;
	o->fb_width = mode[0]->hdisplay;
	o->fb_height = mode[0]->vdisplay;
	o->mode_valid = 1;

	o->kconnector[0] = config[0].connector;
	o->kencoder[0] = config[0].encoder;
	o->_crtc[0] = config[0].crtc->crtc_id;
	o->_pipe[0] = config[0].pipe;
	o->kmode[0] = *mode[0];

	o->kconnector[1] = config[1].connector;
	o->kencoder[1] = config[1].encoder;
	o->_crtc[1] = config[1].crtc->crtc_id;
	o->_pipe[1] = config[1].pipe;
	o->kmode[1] = *mode[1];

	drmModeFreeCrtc(config[0].crtc);
	drmModeFreeCrtc(config[1].crtc);
}

static void paint_flip_mode(struct igt_fb *fb, bool odd_frame)
{
	/* TODO (b/145293089) resolve Cairo/Pixman dependencies */
#if defined(USE_CAIRO_PIXMAN)
	cairo_t *cr = igt_get_cairo_ctx(drm_fd, fb);
	int width = fb->width;
	int height = fb->height;

	igt_paint_test_pattern(cr, width, height);

	if (odd_frame)
		cairo_rectangle(cr, width/4, height/2, width/4, height/8);
	else
		cairo_rectangle(cr, width/2, height/2, width/4, height/8);

	cairo_set_source_rgb(cr, 1, 1, 1);
	cairo_fill(cr);

	igt_put_cairo_ctx(drm_fd, fb, cr);
#endif
}

static bool fb_is_bound(struct test_output *o, int fb)
{
	int n;

	for (n = 0; n < o->count; n++) {
		struct drm_mode_crtc mode = {
			.crtc_id = o->_crtc[n]
		};

		if (drmIoctl(drm_fd, DRM_IOCTL_MODE_GETCRTC, &mode))
			return false;

		if (!mode.mode_valid || mode.fb_id != fb)
			return false;
	}

	return true;
}

static void check_final_state(const struct test_output *o,
			      const struct event_state *es,
			      unsigned int elapsed)
{
	igt_assert_f(es->count > 0,
		     "no %s event received\n", es->name);

	/* Verify we drop no frames, but only if it's not a TV encoder, since
	 * those use some funny fake timings behind userspace's back. */
	if (o->flags & TEST_CHECK_TS) {
		int count = es->count * o->seq_step;
		unsigned int min = actual_frame_time(o) * (count - 1);
		unsigned int max = actual_frame_time(o) * (count + 1);

		igt_debug("expected %d, counted %d, encoder type %d\n",
			  (int)(elapsed / actual_frame_time(o)), count,
			  o->kencoder[0]->encoder_type);
		igt_assert_f(elapsed >= min && elapsed <= max,
			     "dropped frames, expected %d, counted %d, encoder type %d\n",
			     (int)(elapsed / actual_frame_time(o)), count,
			     o->kencoder[0]->encoder_type);
	}
}

/*
 * Wait until at least one pending event completes. Return mask of completed
 * events.
 */
static unsigned int wait_for_events(struct test_output *o)
{
	drmEventContext evctx;
	struct timeval timeout = { .tv_sec = 3, .tv_usec = 0 };
	fd_set fds;
	unsigned int event_mask;
	int ret;

	event_mask = o->pending_events;
	igt_assert(event_mask);

	memset(&evctx, 0, sizeof evctx);
	evctx.version = 2;
	evctx.vblank_handler = vblank_handler;
	evctx.page_flip_handler = page_flip_handler;

	FD_ZERO(&fds);
	FD_SET(drm_fd, &fds);
	do {
		do {
			ret = select(drm_fd + 1, &fds, NULL, NULL, &timeout);
		} while (ret < 0 && errno == EINTR);

		igt_assert_f(ret >= 0,
			     "select error (errno %i)\n", errno);
		igt_assert_f(ret > 0,
			     "select timed out or error (ret %d)\n", ret);
		igt_assert_f(!FD_ISSET(0, &fds),
			     "no fds active, breaking\n");

		do_or_die(drmHandleEvent(drm_fd, &evctx));
	} while (o->pending_events);

	event_mask ^= o->pending_events;
	igt_assert(event_mask);

	return event_mask;
}

/* Returned the elapsed time in us */
static unsigned event_loop(struct test_output *o, unsigned duration_ms)
{
	unsigned long start, end;
	int count = 0;

	start = gettime_us();

	while (1) {
		unsigned int completed_events;

		completed_events = run_test_step(o);
		if (o->pending_events)
			completed_events |= wait_for_events(o);
		check_all_state(o, completed_events);
		update_all_state(o, completed_events);

		if (count && (gettime_us() - start) / 1000 >= duration_ms)
			break;

		count++;
	}

	end = gettime_us();

	/* Flush any remaining events */
	if (o->pending_events)
		wait_for_events(o);

	return end - start;
}

static void free_test_output(struct test_output *o)
{
	int i;

	for (i = 0; i < o->count; i++) {
		drmModeFreeEncoder(o->kencoder[i]);
		drmModeFreeConnector(o->kconnector[i]);
	}
}

static void calibrate_ts(struct test_output *o, int crtc_idx)
{
#define CALIBRATE_TS_STEPS 16
	drmVBlank wait;
	igt_stats_t stats;
	uint32_t last_seq;
	uint64_t last_timestamp;
	double expected;
	double mean;
	double stddev;
	int n;

	memset(&wait, 0, sizeof(wait));
	wait.request.type = kmstest_get_vbl_flag(crtc_idx);
	wait.request.type |= DRM_VBLANK_RELATIVE | DRM_VBLANK_NEXTONMISS;
	do_or_die(drmWaitVBlank(drm_fd, &wait));

	last_seq = wait.reply.sequence;
	last_timestamp = wait.reply.tval_sec;
	last_timestamp *= 1000000;
	last_timestamp += wait.reply.tval_usec;

	memset(&wait, 0, sizeof(wait));
	wait.request.type = kmstest_get_vbl_flag(crtc_idx);
	wait.request.type |= DRM_VBLANK_ABSOLUTE | DRM_VBLANK_EVENT;
	wait.request.sequence = last_seq;
	for (n = 0; n < CALIBRATE_TS_STEPS; n++) {
		drmVBlank check = {};

		++wait.request.sequence;
		do_or_die(drmWaitVBlank(drm_fd, &wait));

		/* Double check that haven't already missed the vblank */
		check.request.type = kmstest_get_vbl_flag(crtc_idx);
		check.request.type |= DRM_VBLANK_RELATIVE;
		do_or_die(drmWaitVBlank(drm_fd, &check));

		igt_assert(!igt_vblank_after(check.reply.sequence, wait.request.sequence));
	}

	igt_stats_init_with_size(&stats, CALIBRATE_TS_STEPS);
	for (n = 0; n < CALIBRATE_TS_STEPS; n++) {
		struct drm_event_vblank ev;
		uint64_t now;
		int poll_ret;

		while (1) {
			/*
			 * In case of the interruptible tests, this poll may
			 * be interrupted with -EINTR, handle this by restarting
			 * until we poll timeout or success.
			 */
			poll_ret = poll(&(struct pollfd){drm_fd, POLLIN}, 1, -1);

			if (poll_ret == 1)
				break;

			igt_assert_neq(poll_ret, 0);
			igt_assert_eq(errno, EINTR);
		}
		igt_assert(read(drm_fd, &ev, sizeof(ev)) == sizeof(ev));
		igt_assert_eq(ev.sequence, last_seq + 1);

		now = ev.tv_sec;
		now *= 1000000;
		now += ev.tv_usec;

		igt_stats_push(&stats, now - last_timestamp);

		last_timestamp = now;
		last_seq = ev.sequence;
	}

	expected = mode_frame_time(o);

	mean = igt_stats_get_mean(&stats);
	stddev = igt_stats_get_std_deviation(&stats);

	igt_info("Expected frametime: %.0fus; measured %.1fus +- %.3fus accuracy %.2f%%\n",
		 expected, mean, stddev, 100 * 3 * stddev / mean);
	/* 99.7% samples within 0.5% of the mean */
	igt_assert(3 * stddev / mean < 0.005);
	/* 84% samples within 0.5% of the expected value.
	 * See comments in check_timings() in kms_setmode.c
	 */
	if (fabs(mean - expected) > 2*stddev) {
		igt_info("vblank interval differs from modeline! expected %.1fus, measured %1.fus +- %.3fus, difference %.1fus (%.1f sigma)\n",
			 expected, mean, stddev,
			 fabs(mean - expected), fabs(mean - expected) / stddev);
	}

	o->vblank_interval = mean;
}

static void run_test_on_crtc_set(struct test_output *o, int *crtc_idxs,
				 int crtc_count, int duration_ms)
{
	char test_name[128];
	unsigned elapsed;
	unsigned bo_size = 0;
	uint64_t tiling;
	int i;
	bool vblank = true;

	switch (crtc_count) {
	case RUN_TEST:
		connector_find_preferred_mode(o->_connector[0], crtc_idxs[0], o);
		if (!o->mode_valid)
			return;
		snprintf(test_name, sizeof(test_name),
			 "%s on pipe %s, connector %s-%d",
			 igt_subtest_name(),
			 kmstest_pipe_name(o->_pipe[0]),
			 kmstest_connector_type_str(o->kconnector[0]->connector_type),
			 o->kconnector[0]->connector_type_id);
		break;
	case RUN_PAIR:
		connector_find_compatible_mode(crtc_idxs[0], crtc_idxs[1], o);
		if (!o->mode_valid)
			return;
		snprintf(test_name, sizeof(test_name),
			 "%s on pipe %s:%s, connector %s-%d:%s-%d",
			 igt_subtest_name(),
			 kmstest_pipe_name(o->_pipe[0]),
			 kmstest_pipe_name(o->_pipe[1]),
			 kmstest_connector_type_str(o->kconnector[0]->connector_type),
			 o->kconnector[0]->connector_type_id,
			 kmstest_connector_type_str(o->kconnector[1]->connector_type),
			 o->kconnector[1]->connector_type_id);
		break;
	default:
		igt_assert(0);
	}

	igt_assert_eq(o->count, crtc_count);

	last_connector = o->kconnector[0];

	igt_info("Beginning %s\n", test_name);

	if (o->flags & TEST_PAN)
		o->fb_width *= 2;

	tiling = LOCAL_DRM_FORMAT_MOD_NONE;
	if (o->flags & TEST_FENCE_STRESS)
		tiling = LOCAL_I915_FORMAT_MOD_X_TILED;

	/* 256 MB is usually the maximum mappable aperture,
	 * (make it 4x times that to ensure failure) */
	if (o->flags & TEST_BO_TOOBIG) {
		bo_size = 4*gem_mappable_aperture_size();
		igt_require(bo_size < gem_global_aperture_size(drm_fd));
	}

	o->fb_ids[0] = igt_create_fb(drm_fd, o->fb_width, o->fb_height,
					 igt_bpp_depth_to_drm_format(o->bpp, o->depth),
					 tiling, &o->fb_info[0]);
	o->fb_ids[1] = igt_create_fb_with_bo_size(drm_fd, o->fb_width, o->fb_height,
						  igt_bpp_depth_to_drm_format(o->bpp, o->depth),
						  tiling, IGT_COLOR_YCBCR_BT709,
						  IGT_COLOR_YCBCR_LIMITED_RANGE,
						  &o->fb_info[1], bo_size, 0);

	igt_assert(o->fb_ids[0]);
	igt_assert(o->fb_ids[1]);

	paint_flip_mode(&o->fb_info[0], false);
	if (!(o->flags & TEST_BO_TOOBIG))
		paint_flip_mode(&o->fb_info[1], true);
	if (o->fb_ids[2])
		paint_flip_mode(&o->fb_info[2], true);

	for (i = 0; i < o->count; i++)
		kmstest_dump_mode(&o->kmode[i]);

	kmstest_unset_all_crtcs(drm_fd, resources);

	if (set_mode(o, o->fb_ids[0], 0, 0)) {
		/* We may fail to apply the mode if there are hidden
		 * constraints, such as bandwidth on the third pipe.
		 */
		igt_assert_f(crtc_count > 1 || crtc_idxs[0] < 2,
			     "set_mode may only fail on the 3rd pipe or in multiple crtc tests\n");
		igt_info("\n%s: SKIPPED\n\n", test_name);
		goto out;
	}
	igt_assert(fb_is_bound(o, o->fb_ids[0]));

	vblank = kms_has_vblank(drm_fd);
	if (!vblank) {
		if (vblank_dependence(o->flags))
			igt_require_f(vblank, "There is no VBlank\n");
		else
			o->flags |= TEST_NO_VBLANK;
	}

	/* quiescent the hw a bit so ensure we don't miss a single frame */
	if (o->flags & TEST_CHECK_TS)
		calibrate_ts(o, crtc_idxs[0]);

	if (o->flags & TEST_BO_TOOBIG) {
		int err = do_page_flip(o, o->fb_ids[1], true);
		igt_assert(err == 0 || err == -E2BIG);
		if (err)
			goto out;
	} else {
		igt_assert_eq(do_page_flip(o, o->fb_ids[1], true), 0);
	}
	wait_for_events(o);

	o->current_fb_id = 1;

	if (o->flags & TEST_FLIP)
		o->flip_state.seq_step = 1;
	else
		o->flip_state.seq_step = 0;
	if (o->flags & TEST_VBLANK)
		o->vblank_state.seq_step = 10;
	else
		o->vblank_state.seq_step = 0;

	/* We run the vblank and flip actions in parallel by default. */
	o->seq_step = max(o->vblank_state.seq_step, o->flip_state.seq_step);

	elapsed = event_loop(o, duration_ms);

	if (o->flags & TEST_FLIP && !(o->flags & TEST_NOEVENT))
		check_final_state(o, &o->flip_state, elapsed);
	if (o->flags & TEST_VBLANK)
		check_final_state(o, &o->vblank_state, elapsed);

	igt_info("\n%s: PASSED\n\n", test_name);

out:
	igt_remove_fb(drm_fd, &o->fb_info[2]);
	igt_remove_fb(drm_fd, &o->fb_info[1]);
	igt_remove_fb(drm_fd, &o->fb_info[0]);

	last_connector = NULL;

	free_test_output(o);
}

static int run_test(int duration, int flags)
{
	struct test_output o;
	int i, n, modes = 0;

	igt_require((flags & TEST_HANG) == 0 || !is_wedged(drm_fd));

	resources = drmModeGetResources(drm_fd);
	igt_require(resources);

	/* Count output configurations to scale test runtime. */
	for (i = 0; i < resources->count_connectors; i++) {
		for (n = 0; n < resources->count_crtcs; n++) {
			memset(&o, 0, sizeof(o));
			o.count = 1;
			o._connector[0] = resources->connectors[i];
			o.flags = flags;
			o.flip_state.name = "flip";
			o.vblank_state.name = "vblank";
			o.bpp = 32;
			o.depth = 24;

			connector_find_preferred_mode(o._connector[0], n, &o);
			if (o.mode_valid)
				modes++;

			free_test_output(&o);
		}
	}

	igt_require(modes);
	duration = duration * 1000 / modes;
	duration = max(500, duration);

	/* Find any connected displays */
	for (i = 0; i < resources->count_connectors; i++) {
		for (n = 0; n < resources->count_crtcs; n++) {
			int crtc_idx;

			memset(&o, 0, sizeof(o));
			o.count = 1;
			o._connector[0] = resources->connectors[i];
			o.flags = flags;
			o.flip_state.name = "flip";
			o.vblank_state.name = "vblank";
			o.bpp = 32;
			o.depth = 24;

			crtc_idx = n;
			run_test_on_crtc_set(&o, &crtc_idx, RUN_TEST, duration);
		}
	}

	drmModeFreeResources(resources);
	return 1;
}

static int run_pair(int duration, int flags)
{
	struct test_output o;
	int i, j, m, n, modes = 0;

	igt_require((flags & TEST_HANG) == 0 || !is_wedged(drm_fd));

	resources = drmModeGetResources(drm_fd);
	igt_require(resources);

	/* Find a pair of connected displays */
	for (i = 0; i < resources->count_connectors; i++) {
		for (n = 0; n < resources->count_crtcs; n++) {
			for (j = i + 1; j < resources->count_connectors; j++) {
				for (m = n + 1; m < resources->count_crtcs; m++) {
					memset(&o, 0, sizeof(o));
					o.count = 2;
					o._connector[0] = resources->connectors[i];
					o._connector[1] = resources->connectors[j];
					o.flags = flags;
					o.flip_state.name = "flip";
					o.vblank_state.name = "vblank";
					o.bpp = 32;
					o.depth = 24;

					connector_find_compatible_mode(n, m, &o);
					if (o.mode_valid)
						modes++;

					free_test_output(&o);
				}
			}
		}
	}

	/* If we have fewer than 2 connected outputs then we won't have any
	 * configuration at all. So skip in that case. */
	igt_require_f(modes, "At least two displays required\n");
	duration = duration * 1000 / modes;
	duration = max(duration, 500);

	/* Find a pair of connected displays */
	for (i = 0; i < resources->count_connectors; i++) {
		for (n = 0; n < resources->count_crtcs; n++) {
			for (j = i + 1; j < resources->count_connectors; j++) {
				for (m = n + 1; m < resources->count_crtcs; m++) {
					int crtc_idxs[2];

					memset(&o, 0, sizeof(o));
					o.count = 2;
					o._connector[0] = resources->connectors[i];
					o._connector[1] = resources->connectors[j];
					o.flags = flags;
					o.flip_state.name = "flip";
					o.vblank_state.name = "vblank";
					o.bpp = 32;
					o.depth = 24;

					crtc_idxs[0] = n;
					crtc_idxs[1] = m;

					run_test_on_crtc_set(&o, crtc_idxs,
							     RUN_PAIR,
							     duration);
				}
			}
		}
	}

	drmModeFreeResources(resources);
	return 1;
}

static void get_timestamp_format(void)
{
	uint64_t cap_mono;
	int ret;

	ret = drmGetCap(drm_fd, DRM_CAP_TIMESTAMP_MONOTONIC, &cap_mono);
	igt_assert(ret == 0 || errno == EINVAL);
	monotonic_timestamp = ret == 0 && cap_mono == 1;
	igt_info("Using %s timestamps\n",
		 monotonic_timestamp ? "monotonic" : "real");
}

static void kms_flip_exit_handler(int sig)
{
	igt_fixture {
		if (last_connector)
			kmstest_set_connector_dpms(drm_fd, last_connector, DRM_MODE_DPMS_ON);
	}
}

static void test_nonblocking_read(int in)
{
	char buffer[1024];
	int fd = dup(in);
	int ret;

	ret = -1;
	if (fd != -1)
		ret = fcntl(fd, F_GETFL);
	if (ret != -1) {
		ret |= O_NONBLOCK;
		ret = fcntl(fd, F_SETFL, ret);
	}
	igt_require(ret != -1);

	igt_set_timeout(5, "Nonblocking DRM fd reading");
	ret = read(fd, buffer, sizeof(buffer));
	igt_reset_timeout();

	igt_assert_eq(ret, -1);
	igt_assert_eq(errno, EAGAIN);

	close(fd);
}

igt_main
{
	struct {
		int duration;
		int flags;
		const char *name;
	} tests[] = {
		{ 30, TEST_VBLANK | TEST_CHECK_TS, "wf_vblank-ts-check" },
		{ 30, TEST_VBLANK | TEST_VBLANK_BLOCK | TEST_CHECK_TS,
					"blocking-wf_vblank" },
		{ 30,  TEST_VBLANK | TEST_VBLANK_ABSOLUTE,
					"absolute-wf_vblank" },
		{ 30,  TEST_VBLANK | TEST_VBLANK_BLOCK | TEST_VBLANK_ABSOLUTE,
					"blocking-absolute-wf_vblank" },
		{ 10, TEST_FLIP | TEST_BASIC, "plain-flip" },
		{ 1, TEST_FLIP | TEST_EBUSY, "busy-flip" },
		{ 30, TEST_FLIP | TEST_FENCE_STRESS , "flip-vs-fences" },
		{ 30, TEST_FLIP | TEST_CHECK_TS, "plain-flip-ts-check" },
		{ 30, TEST_FLIP | TEST_CHECK_TS | TEST_FB_RECREATE,
			"plain-flip-fb-recreate" },
		{ 30, TEST_FLIP | TEST_RMFB | TEST_MODESET , "flip-vs-rmfb" },
		{ 20, TEST_FLIP | TEST_DPMS | TEST_EINVAL | TEST_BASIC, "flip-vs-dpms" },
		{ 30,  TEST_FLIP | TEST_PAN, "flip-vs-panning" },
		{ 20, TEST_FLIP | TEST_MODESET | TEST_EINVAL | TEST_BASIC, "flip-vs-modeset" },
		{ 30,  TEST_FLIP | TEST_VBLANK_EXPIRED_SEQ,
					"flip-vs-expired-vblank" },

		{ 30, TEST_FLIP | TEST_VBLANK | TEST_VBLANK_ABSOLUTE |
		      TEST_CHECK_TS, "flip-vs-absolute-wf_vblank" },
		{ 10, TEST_FLIP | TEST_VBLANK | TEST_CHECK_TS | TEST_BASIC,
					"flip-vs-wf_vblank" },
		{ 30, TEST_FLIP | TEST_VBLANK | TEST_VBLANK_BLOCK |
			TEST_CHECK_TS, "flip-vs-blocking-wf-vblank" },
		{ 30, TEST_FLIP | TEST_MODESET | TEST_HANG | TEST_NOEVENT, "flip-vs-modeset-vs-hang" },
		{ 30, TEST_FLIP | TEST_PAN | TEST_HANG, "flip-vs-panning-vs-hang" },

		{ 1, TEST_DPMS_OFF | TEST_MODESET | TEST_FLIP,
					"flip-vs-dpms-off-vs-modeset" },
		{ 1, TEST_DPMS_OFF | TEST_MODESET | TEST_FLIP | TEST_SINGLE_BUFFER,
					"single-buffer-flip-vs-dpms-off-vs-modeset" },
		{ 30, TEST_FLIP | TEST_NO_2X_OUTPUT | TEST_DPMS_OFF_OTHERS , "dpms-off-confusion" },
		{ 0, TEST_ENOENT | TEST_NOEVENT, "nonexisting-fb" },
		{ 10, TEST_DPMS_OFF | TEST_DPMS | TEST_VBLANK_RACE, "dpms-vs-vblank-race" },
		{ 10, TEST_MODESET | TEST_VBLANK_RACE, "modeset-vs-vblank-race" },
		{ 0, TEST_BO_TOOBIG | TEST_NO_2X_OUTPUT, "bo-too-big" },
		{ 10, TEST_FLIP | TEST_SUSPEND, "flip-vs-suspend" },
	};
	int i;

	igt_fixture {
		drm_fd = drm_open_driver_master(DRIVER_ANY);

		igt_enable_connectors(drm_fd);

		kmstest_set_vt_graphics_mode();
		igt_install_exit_handler(kms_flip_exit_handler);
		get_timestamp_format();

#if defined(USE_INTEL)
		if (is_i915_device(drm_fd)) {
			bufmgr = drm_intel_bufmgr_gem_init(drm_fd, 4096);
			if (bufmgr) {
				devid = intel_get_drm_devid(drm_fd);
				batch = intel_batchbuffer_alloc(bufmgr, devid);
			}
		}
#endif
	}

	igt_subtest("nonblocking-read")
		test_nonblocking_read(drm_fd);

	for (i = 0; i < sizeof(tests) / sizeof (tests[0]); i++) {
		igt_subtest_f("%s%s",
			      tests[i].flags & TEST_BASIC ? "basic-" : "",
			      tests[i].name)
			run_test(tests[i].duration, tests[i].flags);

		if (tests[i].flags & TEST_NO_2X_OUTPUT)
			continue;

		igt_subtest_f( "2x-%s", tests[i].name)
			run_pair(tests[i].duration, tests[i].flags);
	}

	igt_fork_signal_helper();
	for (i = 0; i < sizeof(tests) / sizeof (tests[0]); i++) {
		/* relative blocking vblank waits that get constantly interrupt
		 * take forver. So don't do them. */
		if ((tests[i].flags & TEST_VBLANK_BLOCK) &&
		    !(tests[i].flags & TEST_VBLANK_ABSOLUTE))
			continue;

		/*
		 * -EBUSY needs to complete in a single vblank, skip them for
		 * interruptible tests
		 */
		if (tests[i].flags & TEST_EBUSY)
			continue;

		igt_subtest_f( "%s-interruptible", tests[i].name)
			run_test(tests[i].duration, tests[i].flags);

		if (tests[i].flags & TEST_NO_2X_OUTPUT)
			continue;

		igt_subtest_f( "2x-%s-interruptible", tests[i].name)
			run_pair(tests[i].duration, tests[i].flags);
	}
	igt_stop_signal_helper();
}
