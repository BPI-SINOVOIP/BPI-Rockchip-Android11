/*
 * Copyright © 2013 Intel Corporation
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
 */

#include <inttypes.h>
#include <sys/stat.h>
#include <sys/mount.h>
#include <sys/sysmacros.h>
#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <i915_drm.h>
#include <poll.h>

#include "drmtest.h"
#include "igt_aux.h"
#include "igt_kms.h"
#include "igt_debugfs.h"
#include "igt_sysfs.h"

/**
 * SECTION:igt_debugfs
 * @short_description: Support code for debugfs features
 * @title: debugfs
 * @include: igt.h
 *
 * This library provides helpers to access debugfs features. On top of some
 * basic functions to access debugfs files with e.g. igt_debugfs_open() it also
 * provides higher-level wrappers for some debugfs features.
 *
 * # Pipe CRC Support
 *
 * This library wraps up the kernel's support for capturing pipe CRCs into a
 * neat and tidy package. For the detailed usage see all the functions which
 * work on #igt_pipe_crc_t. This is supported on all platforms and outputs.
 *
 * Actually using pipe CRCs to write modeset tests is a bit tricky though, so
 * there is no way to directly check a CRC: Both the details of the plane
 * blending, color correction and other hardware and how exactly the CRC is
 * computed at each tap point vary by hardware generation and are not disclosed.
 *
 * The only way to use #igt_crc_t CRCs therefore is to compare CRCs among each
 * another either for equality or difference. Otherwise CRCs must be treated as
 * completely opaque values. Note that not even CRCs from different pipes or tap
 * points on the same platform can be compared. Hence only use
 * igt_assert_crc_equal() to inspect CRC values captured by the same
 * #igt_pipe_crc_t object.
 *
 * # Other debugfs interface wrappers
 *
 * This covers the miscellaneous debugfs interface wrappers:
 *
 * - drm/i915 supports interfaces to evict certain classes of gem buffer
 *   objects, see igt_drop_caches_set().
 *
 * - drm/i915 supports an interface to disable prefaulting, useful to test
 *   slow paths in ioctls. See igt_disable_prefault().
 */

/*
 * General debugfs helpers
 */

static bool is_mountpoint(const char *path)
{
	char buf[strlen(path) + 4];
	struct stat st;
	dev_t dev;

	igt_assert_lt(snprintf(buf, sizeof(buf), "%s/.", path), sizeof(buf));
	if (stat(buf, &st))
		return false;

	if (!S_ISDIR(st.st_mode))
		return false;

	dev = st.st_dev;

	igt_assert_lt(snprintf(buf, sizeof(buf), "%s/..", path), sizeof(buf));
	if (stat(buf, &st))
		return false;

	if (!S_ISDIR(st.st_mode))
		return false;

	return dev != st.st_dev;
}

static const char *__igt_debugfs_mount(void)
{
	if (is_mountpoint("/sys/kernel/debug"))
		return "/sys/kernel/debug";

	if (is_mountpoint("/debug"))
		return "/debug";

	if (mount("debug", "/sys/kernel/debug", "debugfs", 0, 0))
		return NULL;

	return "/sys/kernel/debug";
}

/**
 * igt_debugfs_mount:
 *
 * This attempts to locate where debugfs is mounted on the filesystem,
 * and if not found, will then try to mount debugfs at /sys/kernel/debug.
 *
 * Returns:
 * The path to the debugfs mount point (e.g. /sys/kernel/debug)
 */
const char *igt_debugfs_mount(void)
{
	static const char *path;

	if (!path)
		path = __igt_debugfs_mount();

	return path;
}

/**
 * igt_debugfs_path:
 * @device: fd of the device
 * @path: buffer to store path
 * @pathlen: len of @path buffer.
 *
 * This finds the debugfs directory corresponding to @device.
 *
 * Returns:
 * The directory path, or NULL on failure.
 */
char *igt_debugfs_path(int device, char *path, int pathlen)
{
	struct stat st;
	const char *debugfs_root;
	int idx;

	debugfs_root = igt_debugfs_mount();
	igt_assert(debugfs_root);

	memset(&st, 0, sizeof(st));
	if (device != -1) { /* if no fd, we presume we want dri/0 */
		if (fstat(device, &st)) {
			igt_debug("Couldn't stat FD for DRM device: %m\n");
			return NULL;
		}

		if (!S_ISCHR(st.st_mode)) {
			igt_debug("FD for DRM device not a char device!\n");
			return NULL;
		}
	}

	idx = minor(st.st_rdev);
	snprintf(path, pathlen, "%s/dri/%d/name", debugfs_root, idx);
	if (stat(path, &st))
		return NULL;

	if (idx >= 64) {
		int file, name_len, cmp_len;
		char name[100], cmp[100];

		file = open(path, O_RDONLY);
		if (file < 0)
			return NULL;

		name_len = read(file, name, sizeof(name));
		close(file);

		for (idx = 0; idx < 16; idx++) {
			snprintf(path, pathlen, "%s/dri/%d/name",
				 debugfs_root, idx);
			file = open(path, O_RDONLY);
			if (file < 0)
				return NULL;

			cmp_len = read(file, cmp, sizeof(cmp));
			close(file);

			if (cmp_len == name_len && !memcmp(cmp, name, name_len))
				break;
		}

		if (idx == 16)
			return NULL;
	}

	snprintf(path, pathlen, "%s/dri/%d", debugfs_root, idx);
	return path;
}

/**
 * igt_debugfs_dir:
 * @device: fd of the device
 *
 * This opens the debugfs directory corresponding to device for use
 * with igt_sysfs_get() and related functions.
 *
 * Returns:
 * The directory fd, or -1 on failure.
 */
int igt_debugfs_dir(int device)
{
	char path[200];

	if (!igt_debugfs_path(device, path, sizeof(path)))
		return -1;

	igt_debug("Opening debugfs directory '%s'\n", path);
	return open(path, O_RDONLY);
}

/**
 * igt_debugfs_connector_dir:
 * @device: fd of the device
 * @conn_name: conenctor name
 * @mode: mode bits as used by open()
 *
 * This opens the debugfs directory corresponding to connector on the device
 * for use with igt_sysfs_get() and related functions.
 *
 * Returns:
 * The directory fd, or -1 on failure.
 */
int igt_debugfs_connector_dir(int device, char *conn_name, int mode)
{
	int dir, ret;

	dir = igt_debugfs_dir(device);
	if (dir < 0)
		return dir;

	ret = openat(dir, conn_name, mode);

	close(dir);

	return ret;
}

/**
 * igt_debugfs_open:
 * @filename: name of the debugfs node to open
 * @mode: mode bits as used by open()
 *
 * This opens a debugfs file as a Unix file descriptor. The filename should be
 * relative to the drm device's root, i.e. without "drm/$minor".
 *
 * Returns:
 * The Unix file descriptor for the debugfs file or -1 if that didn't work out.
 */
int igt_debugfs_open(int device, const char *filename, int mode)
{
	int dir, ret;

	dir = igt_debugfs_dir(device);
	if (dir < 0)
		return dir;

	ret = openat(dir, filename, mode);

	close(dir);

	return ret;
}

/**
 * igt_debugfs_simple_read:
 * @filename: file name
 * @buf: buffer where the contents will be stored, allocated by the caller
 * @size: size of the buffer
 *
 * This function is similar to __igt_debugfs_read, the difference is that it
 * expects the debugfs directory to be open and it's descriptor passed as the
 * first argument.
 *
 * Returns:
 * -errorno on failure or bytes read on success
 */
int igt_debugfs_simple_read(int dir, const char *filename, char *buf, int size)
{
	int len;

	len = igt_sysfs_read(dir, filename, buf, size - 1);
	if (len < 0)
		buf[0] = '\0';
	else
		buf[len] = '\0';

	return len;
}

/**
 * __igt_debugfs_read:
 * @filename: file name
 * @buf: buffer where the contents will be stored, allocated by the caller
 * @size: size of the buffer
 *
 * This function opens the debugfs file, reads it, stores the content in the
 * provided buffer, then closes the file. Users should make sure that the buffer
 * provided is big enough to fit the whole file, plus one byte.
 */
void __igt_debugfs_read(int fd, const char *filename, char *buf, int size)
{
	int dir = igt_debugfs_dir(fd);

	igt_debugfs_simple_read(dir, filename, buf, size);
	close(dir);
}

/**
 * igt_debugfs_search:
 * @filename: file name
 * @substring: string to search for in @filename
 *
 * Searches each line in @filename for the substring specified in @substring.
 *
 * Returns: True if the @substring is found to occur in @filename
 */
bool igt_debugfs_search(int device, const char *filename, const char *substring)
{
	FILE *file;
	size_t n = 0;
	char *line = NULL;
	bool matched = false;
	int fd;

	fd = igt_debugfs_open(device, filename, O_RDONLY);
	file = fdopen(fd, "r");
	igt_assert(file);

	while (getline(&line, &n, file) >= 0) {
		matched = strstr(line, substring) != NULL;
		if (matched)
			break;
	}

	free(line);
	fclose(file);
	close(fd);

	return matched;
}

/*
 * Pipe CRC
 */

static bool igt_find_crc_mismatch(const igt_crc_t *a, const igt_crc_t *b,
				  int *index)
{
	int nwords = min(a->n_words, b->n_words);
	int i;

	for (i = 0; i < nwords; i++) {
		if (a->crc[i] != b->crc[i]) {
			if (index)
				*index = i;

			return true;
		}
	}

	if (a->n_words != b->n_words) {
		if (index)
			*index = i;
		return true;
	}

	return false;
}

/**
 * igt_assert_crc_equal:
 * @a: first pipe CRC value
 * @b: second pipe CRC value
 *
 * Compares two CRC values and fails the testcase if they don't match with
 * igt_fail(). Note that due to CRC collisions CRC based testcase can only
 * assert that CRCs match, never that they are different. Otherwise there might
 * be random testcase failures when different screen contents end up with the
 * same CRC by chance.
 *
 * Passing --skip-crc-compare on the command line will force this function
 * to always pass, which can be useful in interactive debugging where you
 * might know the test will fail, but still want the test to keep going as if
 * it had succeeded so that you can see the on-screen behavior.
 *
 */
void igt_assert_crc_equal(const igt_crc_t *a, const igt_crc_t *b)
{
	int index;
	bool mismatch;

	mismatch = igt_find_crc_mismatch(a, b, &index);
	if (mismatch)
		igt_debug("CRC mismatch%s at index %d: 0x%x != 0x%x\n",
			  igt_skip_crc_compare ? " (ignored)" : "",
			  index, a->crc[index], b->crc[index]);

	igt_assert(!mismatch || igt_skip_crc_compare);
}

/**
 * igt_check_crc_equal:
 * @a: first pipe CRC value
 * @b: second pipe CRC value
 *
 * Compares two CRC values and return whether they match.
 *
 * Returns: A boolean indicating whether the CRC values match
 */
bool igt_check_crc_equal(const igt_crc_t *a, const igt_crc_t *b)
{
	int index;
	bool mismatch;

	mismatch = igt_find_crc_mismatch(a, b, &index);
	if (mismatch)
		igt_debug("CRC mismatch at index %d: 0x%x != 0x%x\n", index,
			  a->crc[index], b->crc[index]);

	return !mismatch;
}

/**
 * igt_crc_to_string_extended:
 * @crc: pipe CRC value to print
 * @delimiter: The delimiter to use between crc words
 * @crc_size: the number of bytes to print per crc word (between 1 and 4)
 *
 * This function allocates a string and formats @crc into it, depending on
 * @delimiter and @crc_size.
 * The caller is responsible for freeing the string.
 *
 * This should only ever be used for diagnostic debug output.
 */
char *igt_crc_to_string_extended(igt_crc_t *crc, char delimiter, int crc_size)
{
	int i;
	int len = 0;
	int field_width = 2 * crc_size; /* Two chars per byte. */
	char *buf = malloc((field_width+1) * crc->n_words);

	if (!buf)
		return NULL;

	for (i = 0; i < crc->n_words - 1; i++)
		len += sprintf(buf + len, "%0*x%c", field_width,
			       crc->crc[i], delimiter);

	sprintf(buf + len, "%0*x", field_width, crc->crc[i]);

	return buf;
}

/**
 * igt_crc_to_string:
 * @crc: pipe CRC value to print
 *
 * This function allocates a string and formats @crc into it.
 * The caller is responsible for freeing the string.
 *
 * This should only ever be used for diagnostic debug output.
 */
char *igt_crc_to_string(igt_crc_t *crc)
{
	return igt_crc_to_string_extended(crc, ' ', 4);
}

#define MAX_CRC_ENTRIES 10
#define MAX_LINE_LEN (10 + 11 * MAX_CRC_ENTRIES + 1)

struct _igt_pipe_crc {
	int fd;
	int dir;
	int ctl_fd;
	int crc_fd;
	int flags;

	enum pipe pipe;
	char *source;
};

/**
 * igt_require_pipe_crc:
 *
 * Convenience helper to check whether pipe CRC capturing is supported by the
 * kernel. Uses igt_skip to automatically skip the test/subtest if this isn't
 * the case.
 */
void igt_require_pipe_crc(int fd)
{
	int dir;
	struct stat stat;

	dir = igt_debugfs_dir(fd);
	igt_require_f(dir >= 0, "Could not open debugfs directory\n");
	igt_require_f(fstatat(dir, "crtc-0/crc/control", &stat, 0) == 0,
		      "CRCs not supported on this platform\n");

	close(dir);
}

static void igt_hpd_storm_exit_handler(int sig)
{
	int fd = drm_open_driver(DRIVER_INTEL);

	/* Here we assume that only one i915 device will be ever present */
	igt_hpd_storm_reset(fd);

	close(fd);
}

/**
 * igt_hpd_storm_set_threshold:
 * @threshold: How many hotplugs per second required to trigger an HPD storm,
 * or 0 to disable storm detection.
 *
 * Convienence helper to configure the HPD storm detection threshold for i915
 * through debugfs. Useful for hotplugging tests where HPD storm detection
 * might get in the way and slow things down.
 *
 * If the system does not support HPD storm detection, this function does
 * nothing.
 *
 * See: https://01.org/linuxgraphics/gfx-docs/drm/gpu/i915.html#hotplug
 */
void igt_hpd_storm_set_threshold(int drm_fd, unsigned int threshold)
{
	int fd = igt_debugfs_open(drm_fd, "i915_hpd_storm_ctl", O_WRONLY);
	char buf[16];

	if (fd < 0)
		return;

	igt_debug("Setting HPD storm threshold to %d\n", threshold);
	snprintf(buf, sizeof(buf), "%d", threshold);
	igt_assert_eq(write(fd, buf, strlen(buf)), strlen(buf));

	close(fd);
	igt_install_exit_handler(igt_hpd_storm_exit_handler);
}

/**
 * igt_hpd_storm_reset:
 *
 * Convienence helper to reset HPD storm detection to it's default settings.
 * If hotplug detection was disabled on any ports due to an HPD storm, it will
 * be immediately re-enabled. Always called on exit if the HPD storm detection
 * threshold was modified during any tests.
 *
 * If the system does not support HPD storm detection, this function does
 * nothing.
 *
 * See: https://01.org/linuxgraphics/gfx-docs/drm/gpu/i915.html#hotplug
 */
void igt_hpd_storm_reset(int drm_fd)
{
	int fd = igt_debugfs_open(drm_fd, "i915_hpd_storm_ctl", O_WRONLY);
	const char *buf = "reset";

	if (fd < 0)
		return;

	igt_debug("Resetting HPD storm threshold\n");
	igt_assert_eq(write(fd, buf, strlen(buf)), strlen(buf));

	close(fd);
}

/**
 * igt_hpd_storm_detected:
 *
 * Checks whether or not i915 has detected an HPD interrupt storm on any of the
 * system's ports.
 *
 * This function always returns false on systems that do not support HPD storm
 * detection.
 *
 * See: https://01.org/linuxgraphics/gfx-docs/drm/gpu/i915.html#hotplug
 *
 * Returns: Whether or not an HPD storm has been detected.
 */
bool igt_hpd_storm_detected(int drm_fd)
{
	int fd = igt_debugfs_open(drm_fd, "i915_hpd_storm_ctl", O_RDONLY);
	char *start_loc;
	char buf[32] = {0}, detected_str[4];
	bool ret;

	if (fd < 0)
		return false;

	igt_assert_lt(0, read(fd, buf, sizeof(buf) - 1));
	igt_assert(start_loc = strstr(buf, "Detected: "));
	igt_assert_eq(sscanf(start_loc, "Detected: %s\n", detected_str), 1);

	if (strcmp(detected_str, "yes") == 0)
		ret = true;
	else if (strcmp(detected_str, "no") == 0)
		ret = false;
	else
		igt_fail_on_f(true, "Unknown hpd storm detection status '%s'\n",
			      detected_str);

	close(fd);
	return ret;
}

/**
 * igt_require_hpd_storm_ctl:
 *
 * Skips the current test if the system does not have HPD storm detection.
 *
 * See: https://01.org/linuxgraphics/gfx-docs/drm/gpu/i915.html#hotplug
 */
void igt_require_hpd_storm_ctl(int drm_fd)
{
	int fd = igt_debugfs_open(drm_fd, "i915_hpd_storm_ctl", O_RDONLY);

	igt_require_f(fd > 0, "No i915_hpd_storm_ctl found in debugfs\n");
	close(fd);
}

static igt_pipe_crc_t *
pipe_crc_new(int fd, enum pipe pipe, const char *source, int flags)
{
	igt_pipe_crc_t *pipe_crc;
	char buf[128];
	int debugfs;

	igt_assert(source);

	debugfs = igt_debugfs_dir(fd);
	igt_assert(debugfs != -1);

	pipe_crc = calloc(1, sizeof(struct _igt_pipe_crc));

	sprintf(buf, "crtc-%d/crc/control", pipe);
	pipe_crc->ctl_fd = openat(debugfs, buf, O_WRONLY);
	igt_assert(pipe_crc->ctl_fd != -1);

	pipe_crc->crc_fd = -1;
	pipe_crc->fd = fd;
	pipe_crc->dir = debugfs;
	pipe_crc->pipe = pipe;
	pipe_crc->source = strdup(source);
	igt_assert(pipe_crc->source);
	pipe_crc->flags = flags;

	return pipe_crc;
}

/**
 * igt_pipe_crc_new:
 * @pipe: display pipe to use as source
 * @source: CRC tap point to use as source
 *
 * This sets up a new pipe CRC capture object for the given @pipe and @source
 * in blocking mode.
 *
 * Returns: A pipe CRC object for the given @pipe and @source. The library
 * assumes that the source is always available since recent kernels support at
 * least INTEL_PIPE_CRC_SOURCE_AUTO everywhere.
 */
igt_pipe_crc_t *
igt_pipe_crc_new(int fd, enum pipe pipe, const char *source)
{
	return pipe_crc_new(fd, pipe, source, O_RDONLY);
}

/**
 * igt_pipe_crc_new_nonblock:
 * @pipe: display pipe to use as source
 * @source: CRC tap point to use as source
 *
 * This sets up a new pipe CRC capture object for the given @pipe and @source
 * in nonblocking mode.
 *
 * Returns: A pipe CRC object for the given @pipe and @source. The library
 * assumes that the source is always available since recent kernels support at
 * least INTEL_PIPE_CRC_SOURCE_AUTO everywhere.
 */
igt_pipe_crc_t *
igt_pipe_crc_new_nonblock(int fd, enum pipe pipe, const char *source)
{
	return pipe_crc_new(fd, pipe, source, O_RDONLY | O_NONBLOCK);
}

/**
 * igt_pipe_crc_free:
 * @pipe_crc: pipe CRC object
 *
 * Frees all resources associated with @pipe_crc.
 */
void igt_pipe_crc_free(igt_pipe_crc_t *pipe_crc)
{
	if (!pipe_crc)
		return;

	close(pipe_crc->ctl_fd);
	close(pipe_crc->crc_fd);
	close(pipe_crc->dir);
	free(pipe_crc->source);
	free(pipe_crc);
}

static bool pipe_crc_init_from_string(igt_pipe_crc_t *pipe_crc, igt_crc_t *crc,
				      const char *line)
{
	int i;
	const char *buf;

	if (strncmp(line, "XXXXXXXXXX", 10) == 0)
		crc->has_valid_frame = false;
	else {
		crc->has_valid_frame = true;
		crc->frame = strtoul(line, NULL, 16);
	}

	buf = line + 10;
	for (i = 0; *buf != '\n'; i++, buf += 11)
		crc->crc[i] = strtoul(buf, NULL, 16);

	crc->n_words = i;

	return true;
}

static int read_crc(igt_pipe_crc_t *pipe_crc, igt_crc_t *out)
{
	ssize_t bytes_read;
	char buf[MAX_LINE_LEN + 1];

	igt_set_timeout(5, "CRC reading");
	bytes_read = read(pipe_crc->crc_fd, &buf, MAX_LINE_LEN);
	igt_reset_timeout();

	if (bytes_read < 0)
		bytes_read = -errno;
	else
		buf[bytes_read] = '\0';

	if (bytes_read > 0 && !pipe_crc_init_from_string(pipe_crc, out, buf))
		return -EINVAL;

	return bytes_read;
}

static void read_one_crc(igt_pipe_crc_t *pipe_crc, igt_crc_t *out)
{
	int ret;

	fcntl(pipe_crc->crc_fd, F_SETFL, pipe_crc->flags & ~O_NONBLOCK);

	do {
		ret = read_crc(pipe_crc, out);
	} while (ret == -EINTR);

	fcntl(pipe_crc->crc_fd, F_SETFL, pipe_crc->flags);
}

/**
 * igt_pipe_crc_start:
 * @pipe_crc: pipe CRC object
 *
 * Starts the CRC capture process on @pipe_crc.
 */
void igt_pipe_crc_start(igt_pipe_crc_t *pipe_crc)
{
	const char *src = pipe_crc->source;
	struct pollfd pfd;
	char buf[32];

	/* Stop first just to make sure we don't have lingering state left. */
	igt_pipe_crc_stop(pipe_crc);

	igt_reset_fifo_underrun_reporting(pipe_crc->fd);

	igt_assert_eq(write(pipe_crc->ctl_fd, src, strlen(src)), strlen(src));

	sprintf(buf, "crtc-%d/crc/data", pipe_crc->pipe);

	igt_set_timeout(10, "Opening crc fd, and poll for first CRC.");
	pipe_crc->crc_fd = openat(pipe_crc->dir, buf, pipe_crc->flags);
	igt_assert(pipe_crc->crc_fd != -1);

	pfd.fd = pipe_crc->crc_fd;
	pfd.events = POLLIN;
	poll(&pfd, 1, -1);

	igt_reset_timeout();

	errno = 0;
}

/**
 * igt_pipe_crc_stop:
 * @pipe_crc: pipe CRC object
 *
 * Stops the CRC capture process on @pipe_crc.
 */
void igt_pipe_crc_stop(igt_pipe_crc_t *pipe_crc)
{
	close(pipe_crc->crc_fd);
	pipe_crc->crc_fd = -1;
}

/**
 * igt_pipe_crc_get_crcs:
 * @pipe_crc: pipe CRC object
 * @n_crcs: number of CRCs to capture
 * @out_crcs: buffer pointer for the captured CRC values
 *
 * Read up to @n_crcs from @pipe_crc. This function does not block, and will
 * return early if not enough CRCs can be captured, if @pipe_crc has been
 * opened using igt_pipe_crc_new_nonblock(). It will block until @n_crcs are
 * retrieved if @pipe_crc has been opened using igt_pipe_crc_new(). @out_crcs is
 * alloced by this function and must be released with free() by the caller.
 *
 * Callers must start and stop the capturing themselves by calling
 * igt_pipe_crc_start() and igt_pipe_crc_stop(). For one-shot CRC collecting
 * look at igt_pipe_crc_collect_crc().
 *
 * Returns:
 * The number of CRCs captured. Should be equal to @n_crcs in blocking mode, but
 * can be less (even zero) in non-blocking mode.
 */
int
igt_pipe_crc_get_crcs(igt_pipe_crc_t *pipe_crc, int n_crcs,
		      igt_crc_t **out_crcs)
{
	igt_crc_t *crcs;
	int n = 0;

	crcs = calloc(n_crcs, sizeof(igt_crc_t));

	do {
		igt_crc_t *crc = &crcs[n];
		int ret;

		ret = read_crc(pipe_crc, crc);
		if (ret == -EAGAIN)
			break;

		if (ret < 0)
			continue;

		n++;
	} while (n < n_crcs);

	*out_crcs = crcs;
	return n;
}

static void crc_sanity_checks(igt_pipe_crc_t *pipe_crc, igt_crc_t *crc)
{
	int i;
	bool all_zero = true;

	/* Any CRC value can be considered valid on amdgpu hardware. */
	if (is_amdgpu_device(pipe_crc->fd))
		return;

	for (i = 0; i < crc->n_words; i++) {
		igt_warn_on_f(crc->crc[i] == 0xffffffff,
			      "Suspicious CRC: it looks like the CRC "
			      "read back was from a register in a powered "
			      "down well\n");
		if (crc->crc[i])
			all_zero = false;
	}

	igt_warn_on_f(all_zero, "Suspicious CRC: All values are 0.\n");
}

/**
 * igt_pipe_crc_drain:
 * @pipe_crc: pipe CRC object
 *
 * Discards all currently queued CRC values from @pipe_crc. This function does
 * not block, and is useful to flush @pipe_crc. Afterwards you can get a fresh
 * CRC with igt_pipe_crc_get_single().
 */
void igt_pipe_crc_drain(igt_pipe_crc_t *pipe_crc)
{
	int ret;
	igt_crc_t crc;

	fcntl(pipe_crc->crc_fd, F_SETFL, pipe_crc->flags | O_NONBLOCK);

	do {
		ret = read_crc(pipe_crc, &crc);
	} while (ret > 0 || ret == -EINVAL);

	fcntl(pipe_crc->crc_fd, F_SETFL, pipe_crc->flags);
}

/**
 * igt_pipe_crc_get_single:
 * @pipe_crc: pipe CRC object
 * @crc: buffer pointer for the captured CRC value
 *
 * Read a single @crc from @pipe_crc. This function blocks even
 * when nonblocking CRC is requested.
 *
 * Callers must start and stop the capturing themselves by calling
 * igt_pipe_crc_start() and igt_pipe_crc_stop(). For one-shot CRC collecting
 * look at igt_pipe_crc_collect_crc().
 *
 * If capturing has been going on for a while and a fresh crc is required,
 * you should use igt_pipe_crc_get_current() instead.
 */
void igt_pipe_crc_get_single(igt_pipe_crc_t *pipe_crc, igt_crc_t *crc)
{
	read_one_crc(pipe_crc, crc);

	crc_sanity_checks(pipe_crc, crc);
}

/**
 * igt_pipe_crc_get_current:
 * @drm_fd: Pointer to drm fd for vblank counter
 * @pipe_crc: pipe CRC object
 * @crc: buffer pointer for the captured CRC value
 *
 * Same as igt_pipe_crc_get_single(), but will wait until a new CRC can be captured.
 * This is useful for retrieving the current CRC in a more race free way than
 * igt_pipe_crc_drain() + igt_pipe_crc_get_single().
 */
void
igt_pipe_crc_get_current(int drm_fd, igt_pipe_crc_t *pipe_crc, igt_crc_t *crc)
{
	unsigned vblank = kmstest_get_vblank(drm_fd, pipe_crc->pipe, 0);

	do {
		read_one_crc(pipe_crc, crc);

		/* Only works with valid frame counter */
		if (!crc->has_valid_frame) {
			igt_pipe_crc_drain(pipe_crc);
			igt_pipe_crc_get_single(pipe_crc, crc);
			return;
		}
	} while (igt_vblank_before_eq(crc->frame, vblank));

	crc_sanity_checks(pipe_crc, crc);
}

/**
 * igt_pipe_crc_collect_crc:
 * @pipe_crc: pipe CRC object
 * @out_crc: buffer for the captured CRC values
 *
 * Read a single CRC from @pipe_crc. This function blocks until the CRC is
 * retrieved, irrespective of whether @pipe_crc has been opened with
 * igt_pipe_crc_new() or igt_pipe_crc_new_nonblock().  @out_crc must be
 * allocated by the caller.
 *
 * This function takes care of the pipe_crc book-keeping, it will start/stop
 * the collection of the CRC.
 *
 * This function also calls the interactive debug with the "crc" domain, so you
 * can make use of this feature to actually see the screen that is being CRC'd.
 *
 * For continuous CRC collection look at igt_pipe_crc_start(),
 * igt_pipe_crc_get_crcs() and igt_pipe_crc_stop().
 */
void igt_pipe_crc_collect_crc(igt_pipe_crc_t *pipe_crc, igt_crc_t *out_crc)
{
	igt_debug_wait_for_keypress("crc");

	igt_pipe_crc_start(pipe_crc);
	igt_pipe_crc_get_single(pipe_crc, out_crc);
	igt_pipe_crc_stop(pipe_crc);
}

/**
 * igt_reset_fifo_underrun_reporting:
 * @drm_fd: drm device file descriptor
 *
 * Resets fifo underrun reporting, if supported by the device. Useful since fifo
 * underrun reporting tends to be one-shot, so good to reset it before the
 * actual functional test again in case there's been a separate issue happening
 * while preparing the test setup.
 */
void igt_reset_fifo_underrun_reporting(int drm_fd)
{
	int fd = igt_debugfs_open(drm_fd, "i915_fifo_underrun_reset", O_WRONLY);
	if (fd >= 0) {
		igt_assert_eq(write(fd, "y", 1), 1);

		close(fd);
	}
}

/*
 * Drop caches
 */

/**
 * igt_drop_caches_has:
 * @val: bitmask for DROP_* values
 *
 * This queries the debugfs to see if it supports the full set of desired
 * operations.
 */
bool igt_drop_caches_has(int drm_fd, uint64_t val)
{
	uint64_t mask;
	int dir;

	mask = 0;
	dir = igt_debugfs_dir(drm_fd);
	igt_sysfs_scanf(dir, "i915_gem_drop_caches", "0x%" PRIx64, &mask);
	close(dir);

	return (val & mask) == val;
}

/**
 * igt_drop_caches_set:
 * @val: bitmask for DROP_* values
 *
 * This calls the debugfs interface the drm/i915 GEM driver exposes to drop or
 * evict certain classes of gem buffer objects.
 */
void igt_drop_caches_set(int drm_fd, uint64_t val)
{
	int dir;

	dir = igt_debugfs_dir(drm_fd);
	igt_assert(igt_sysfs_printf(dir, "i915_gem_drop_caches",
				    "0x%" PRIx64, val) > 0);
	close(dir);
}

/*
 * Prefault control
 */

#define PREFAULT_DEBUGFS "/sys/module/i915/parameters/prefault_disable"
static void igt_prefault_control(bool enable)
{
	const char *name = PREFAULT_DEBUGFS;
	int fd;
	char buf[2] = {'Y', 'N'};
	int index;

	fd = open(name, O_RDWR);
	igt_require(fd >= 0);

	if (enable)
		index = 1;
	else
		index = 0;

	igt_require(write(fd, &buf[index], 1) == 1);

	close(fd);
}

static void enable_prefault_at_exit(int sig)
{
	igt_enable_prefault();
}

/**
 * igt_disable_prefault:
 *
 * Disable prefaulting in certain gem ioctls through the debugfs interface. As
 * usual this installs an exit handler to clean up and re-enable prefaulting
 * even when the test exited abnormally.
 *
 * igt_enable_prefault() will enable normale operation again.
 */
void igt_disable_prefault(void)
{
	igt_prefault_control(false);

	igt_install_exit_handler(enable_prefault_at_exit);
}

/**
 * igt_enable_prefault:
 *
 * Enable prefault (again) through the debugfs interface.
 */
void igt_enable_prefault(void)
{
	igt_prefault_control(true);
}

static int get_object_count(int fd)
{
	int dir, ret, scanned;

	igt_drop_caches_set(fd,
			    DROP_RETIRE | DROP_ACTIVE | DROP_IDLE | DROP_FREED);

	dir = igt_debugfs_dir(fd);
	scanned = igt_sysfs_scanf(dir, "i915_gem_objects",
				  "%i objects", &ret);
	igt_assert_eq(scanned, 1);
	close(dir);

	return ret;
}

/**
 * igt_get_stable_obj_count:
 * @driver: fd to drm/i915 GEM driver
 *
 * This puts the driver into a stable (quiescent) state and then returns the
 * current number of gem buffer objects as reported in the i915_gem_objects
 * debugFS interface.
 */
int igt_get_stable_obj_count(int driver)
{
	int obj_count;
	gem_quiescent_gpu(driver);
	obj_count = get_object_count(driver);
	/* The test relies on the system being in the same state before and
	 * after the test so any difference in the object count is a result of
	 * leaks during the test. */
	return obj_count;
}

void __igt_debugfs_dump(int device, const char *filename, int level)
{
	char *contents;
	int dir;

	dir = igt_debugfs_dir(device);
	contents = igt_sysfs_get(dir, filename);
	close(dir);

	igt_log(IGT_LOG_DOMAIN, level, "%s:\n%s\n", filename, contents);
	free(contents);
}
