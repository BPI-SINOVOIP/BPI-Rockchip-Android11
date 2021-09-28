/*
 * Copyright © 2016 Intel Corporation
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
#include <sys/sysmacros.h>
#include <sys/mount.h>
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <i915_drm.h>
#include <dirent.h>
#include <unistd.h>
#include <fcntl.h>

#include "igt_core.h"
#include "igt_sysfs.h"
#include "igt_device.h"

/**
 * SECTION:igt_sysfs
 * @short_description: Support code for sysfs features
 * @title: sysfs
 * @include: igt.h
 *
 * This library provides helpers to access sysfs features. Right now it only
 * provides basic support for like igt_sysfs_open().
 */

static int readN(int fd, char *buf, int len)
{
	int ret, total = 0;
	do {
		ret = read(fd, buf + total, len - total);
		if (ret < 0)
			ret = -errno;
		if (ret == -EINTR || ret == -EAGAIN)
			continue;
		if (ret <= 0)
			break;
		total += ret;
	} while (total != len);
	return total ?: ret;
}

static int writeN(int fd, const char *buf, int len)
{
	int ret, total = 0;
	do {
		ret = write(fd, buf + total, len - total);
		if (ret < 0)
			ret = -errno;
		if (ret == -EINTR || ret == -EAGAIN)
			continue;
		if (ret <= 0)
			break;
		total += ret;
	} while (total != len);
	return total ?: ret;
}

/**
 * igt_sysfs_path:
 * @device: fd of the device
 * @path: buffer to fill with the sysfs path to the device
 * @pathlen: length of @path buffer
 *
 * This finds the sysfs directory corresponding to @device.
 *
 * Returns:
 * The directory path, or NULL on failure.
 */
char *igt_sysfs_path(int device, char *path, int pathlen)
{
	struct stat st;

	if (device < 0)
		return NULL;

	if (fstat(device, &st) || !S_ISCHR(st.st_mode))
		return NULL;

	snprintf(path, pathlen, "/sys/dev/char/%d:%d",
		 major(st.st_rdev), minor(st.st_rdev));

	if (access(path, F_OK))
		return NULL;

	return path;
}

/**
 * igt_sysfs_open:
 * @device: fd of the device
 *
 * This opens the sysfs directory corresponding to device for use
 * with igt_sysfs_set() and igt_sysfs_get().
 *
 * Returns:
 * The directory fd, or -1 on failure.
 */
int igt_sysfs_open(int device)
{
	char path[80];

	if (!igt_sysfs_path(device, path, sizeof(path)))
		return -1;

	return open(path, O_RDONLY);
}

/**
 * igt_sysfs_set_parameters:
 * @device: fd of the device
 * @parameter: the name of the parameter to set
 * @fmt: printf-esque format string
 *
 * Returns true on success
 */
bool igt_sysfs_set_parameter(int device,
			     const char *parameter,
			     const char *fmt, ...)
{
	va_list ap;
	int dir;
	int ret;

	dir = igt_sysfs_open_parameters(device);
	if (dir < 0)
		return false;

	va_start(ap, fmt);
	ret = igt_sysfs_vprintf(dir, parameter, fmt, ap);
	va_end(ap);

	close(dir);

	return ret > 0;
}

/**
 * igt_sysfs_open_parameters:
 * @device: fd of the device
 *
 * This opens the module parameters directory (under sysfs) corresponding
 * to the device for use with igt_sysfs_set() and igt_sysfs_get().
 *
 * Returns:
 * The directory fd, or -1 on failure.
 */
int igt_sysfs_open_parameters(int device)
{
	int dir, params = -1;

	dir = igt_sysfs_open(device);
	if (dir >= 0) {
		params = openat(dir,
				"device/driver/module/parameters",
				O_RDONLY);
		close(dir);
	}

	if (params < 0) { /* builtin? */
		drm_version_t version;
		char name[32] = "";
		char path[PATH_MAX];

		memset(&version, 0, sizeof(version));
		version.name_len = sizeof(name);
		version.name = name;
		ioctl(device, DRM_IOCTL_VERSION, &version);

		sprintf(path, "/sys/module/%s/parameters", name);
		params = open(path, O_RDONLY);
	}

	return params;
}

/**
 * igt_sysfs_write:
 * @dir: directory for the device from igt_sysfs_open()
 * @attr: name of the sysfs node to open
 * @data: the block to write from
 * @len: the length to write
 *
 * This writes @len bytes from @data to the sysfs file.
 *
 * Returns:
 * The number of bytes written, or -errno on error.
 */
int igt_sysfs_write(int dir, const char *attr, const void *data, int len)
{
	int fd;

	fd = openat(dir, attr, O_WRONLY);
	if (fd < 0)
		return -errno;

	len = writeN(fd, data, len);
	close(fd);

	return len;
}

/**
 * igt_sysfs_read:
 * @dir: directory for the device from igt_sysfs_open()
 * @attr: name of the sysfs node to open
 * @data: the block to read into
 * @len: the maximum length to read
 *
 * This reads @len bytes from the sysfs file to @data
 *
 * Returns:
 * The length read, -errno on failure.
 */
int igt_sysfs_read(int dir, const char *attr, void *data, int len)
{
	int fd;

	fd = openat(dir, attr, O_RDONLY);
	if (fd < 0)
		return -errno;

	len = readN(fd, data, len);
	close(fd);

	return len;
}

/**
 * igt_sysfs_set:
 * @dir: directory for the device from igt_sysfs_open()
 * @attr: name of the sysfs node to open
 * @value: the string to write
 *
 * This writes the value to the sysfs file.
 *
 * Returns:
 * True on success, false on failure.
 */
bool igt_sysfs_set(int dir, const char *attr, const char *value)
{
	int len = strlen(value);
	return igt_sysfs_write(dir, attr, value, len) == len;
}

/**
 * igt_sysfs_get:
 * @dir: directory for the device from igt_sysfs_open()
 * @attr: name of the sysfs node to open
 *
 * This reads the value from the sysfs file.
 *
 * Returns:
 * A nul-terminated string, must be freed by caller after use, or NULL
 * on failure.
 */
char *igt_sysfs_get(int dir, const char *attr)
{
	char *buf;
	int len, offset, rem;
	int ret, fd;

	fd = openat(dir, attr, O_RDONLY);
	if (fd < 0)
		return NULL;

	offset = 0;
	len = 64;
	rem = len - offset - 1;
	buf = malloc(len);
	if (!buf)
		goto out;

	while ((ret = readN(fd, buf + offset, rem)) == rem) {
		char *newbuf;

		newbuf = realloc(buf, 2*len);
		if (!newbuf)
			break;

		buf = newbuf;
		len *= 2;
		offset += ret;
		rem = len - offset - 1;
	}

	if (ret > 0)
		offset += ret;
	buf[offset] = '\0';
	while (offset > 0 && buf[offset-1] == '\n')
		buf[--offset] = '\0';

out:
	close(fd);
	return buf;
}

/**
 * igt_sysfs_scanf:
 * @dir: directory for the device from igt_sysfs_open()
 * @attr: name of the sysfs node to open
 * @fmt: scanf format string
 * @...: Additional paramaters to store the scaned input values
 *
 * scanf() wrapper for sysfs.
 * 
 * Returns:
 * Number of values successfully scanned (which can be 0), EOF on errors or
 * premature end of file.
 */
int igt_sysfs_scanf(int dir, const char *attr, const char *fmt, ...)
{
	FILE *file;
	int fd;
	int ret = -1;

	fd = openat(dir, attr, O_RDONLY);
	if (fd < 0)
		return -1;

	file = fdopen(fd, "r");
	if (file) {
		va_list ap;

		va_start(ap, fmt);
		ret = vfscanf(file, fmt, ap);
		va_end(ap);

		fclose(file);
	} else {
		close(fd);
	}

	return ret;
}

int igt_sysfs_vprintf(int dir, const char *attr, const char *fmt, va_list ap)
{
	char stack[128], *buf = stack;
	va_list tmp;
	int ret, fd;

	fd = openat(dir, attr, O_WRONLY);
	if (fd < 0)
		return -errno;

	va_copy(tmp, ap);
	ret = vsnprintf(buf, sizeof(stack), fmt, tmp);
	va_end(tmp);
	if (ret < 0)
		return -EINVAL;

	if (ret > sizeof(stack)) {
		unsigned int len = ret + 1;

		buf = malloc(len);
		if (!buf)
			return -ENOMEM;

		ret = vsnprintf(buf, ret, fmt, ap);
		if (ret > len) {
			free(buf);
			return -EINVAL;
		}
	}

	ret = writeN(fd, buf, ret);

	close(fd);
	if (buf != stack)
		free(buf);

	return ret;
}

/**
 * igt_sysfs_printf:
 * @dir: directory for the device from igt_sysfs_open()
 * @attr: name of the sysfs node to open
 * @fmt: printf format string
 * @...: Additional paramaters to store the scaned input values
 *
 * printf() wrapper for sysfs.
 *
 * Returns:
 * Number of characters written, negative value on error.
 */
int igt_sysfs_printf(int dir, const char *attr, const char *fmt, ...)
{
	va_list ap;
	int ret;

	va_start(ap, fmt);
	ret = igt_sysfs_vprintf(dir, attr, fmt, ap);
	va_end(ap);

	return ret;
}

/**
 * igt_sysfs_get_u32:
 * @dir: directory for the device from igt_sysfs_open()
 * @attr: name of the sysfs node to open
 *
 * Convenience wrapper to read a unsigned 32bit integer from a sysfs file.
 *
 * Returns:
 * The value read.
 */
uint32_t igt_sysfs_get_u32(int dir, const char *attr)
{
	uint32_t result;

	if (igt_sysfs_scanf(dir, attr, "%u", &result) != 1)
		return 0;

	return result;
}

/**
 * igt_sysfs_set_u32:
 * @dir: directory for the device from igt_sysfs_open()
 * @attr: name of the sysfs node to open
 * @value: value to set
 *
 * Convenience wrapper to write a unsigned 32bit integer to a sysfs file.
 *
 * Returns:
 * True if successfully written
 */
bool igt_sysfs_set_u32(int dir, const char *attr, uint32_t value)
{
	return igt_sysfs_printf(dir, attr, "%u", value) > 0;
}

/**
 * igt_sysfs_get_boolean:
 * @dir: directory for the device from igt_sysfs_open()
 * @attr: name of the sysfs node to open
 *
 * Convenience wrapper to read a boolean sysfs file.
 * 
 * Returns:
 * The value read.
 */
bool igt_sysfs_get_boolean(int dir, const char *attr)
{
	int result;

	if (igt_sysfs_scanf(dir, attr, "%d", &result) != 1)
		return false;

	return result;
}

/**
 * igt_sysfs_set_boolean:
 * @dir: directory for the device from igt_sysfs_open()
 * @attr: name of the sysfs node to open
 * @value: value to set
 *
 * Convenience wrapper to write a boolean sysfs file.
 * 
 * Returns:
 * The value read.
 */
bool igt_sysfs_set_boolean(int dir, const char *attr, bool value)
{
	return igt_sysfs_printf(dir, attr, "%d", value) == 1;
}

static void bind_con(const char *name, bool enable)
{
	const char *path = "/sys/class/vtconsole";
	DIR *dir;
	struct dirent *de;

	dir = opendir(path);
	if (!dir)
		return;

	while ((de = readdir(dir))) {
		char buf[PATH_MAX];
		int fd, len;

		if (strncmp(de->d_name, "vtcon", 5))
			continue;

		sprintf(buf, "%s/%s/name", path, de->d_name);
		fd = open(buf, O_RDONLY);
		if (fd < 0)
			continue;

		buf[sizeof(buf) - 1] = '\0';
		len = read(fd, buf, sizeof(buf) - 1);
		close(fd);
		if (len >= 0)
			buf[len] = '\0';

		if (!strstr(buf, name))
			continue;

		sprintf(buf, "%s/%s/bind", path, de->d_name);
		fd = open(buf, O_WRONLY);
		if (fd != -1) {
			igt_ignore_warn(write(fd, enable ? "1\n" : "0\n", 2));
			close(fd);
		}
		break;
	}
	closedir(dir);
}

/**
 * bind_fbcon:
 * @enable: boolean value
 *
 * This functions enables/disables the text console running on top of the
 * framebuffer device.
 */
void bind_fbcon(bool enable)
{
	/*
	 * The vtcon bind interface seems somewhat broken. Possibly
	 * depending on the order the console drivers have been
	 * registered you either have to unbind the old driver,
	 * or bind the new driver. Let's do both.
	 */
	bind_con("dummy device", !enable);
	bind_con("frame buffer device", enable);
}

/**
 * kick_snd_hda_intel:
 *
 * This functions unbinds the snd_hda_intel driver so the module cand be
 * unloaded.
 *
 */
void kick_snd_hda_intel(void)
{
	DIR *dir;
	struct dirent *snd_hda;
	int fd; size_t len;

	const char *dpath = "/sys/bus/pci/drivers/snd_hda_intel";
	const char *path = "/sys/bus/pci/drivers/snd_hda_intel/unbind";
	const char *devid = "0000:";

	fd = open(path, O_WRONLY);
	if (fd < 0) {
		return;
	}

	dir = opendir(dpath);
	if (!dir)
		goto out;

	len = strlen(devid);
	while ((snd_hda = readdir(dir))) {
		struct stat st;
		char fpath[PATH_MAX];

		if (*snd_hda->d_name == '.')
			continue;

		snprintf(fpath, sizeof(fpath), "%s/%s", dpath, snd_hda->d_name);
		if (lstat(fpath, &st))
			continue;

		if (!S_ISLNK(st.st_mode))
			continue;

		if (!strncmp(devid, snd_hda->d_name, len)) {
			igt_ignore_warn(write(fd, snd_hda->d_name,
					strlen(snd_hda->d_name)));
		}
	}

	closedir(dir);
out:
	close(fd);
}

static int fbcon_cursor_blink_fd = -1;
static char fbcon_cursor_blink_prev_value[2];

static void fbcon_cursor_blink_restore(int sig)
{
	write(fbcon_cursor_blink_fd, fbcon_cursor_blink_prev_value,
	      strlen(fbcon_cursor_blink_prev_value) + 1);
	close(fbcon_cursor_blink_fd);
}

/**
 * fbcon_blink_enable:
 * @enable: if true enables the fbcon cursor blinking otherwise disables it
 *
 * Enables or disables the cursor blinking in fbcon, it also restores the
 * previous blinking state when exiting test.
 *
 */
void fbcon_blink_enable(bool enable)
{
	const char *cur_blink_path = "/sys/class/graphics/fbcon/cursor_blink";
	int fd, r;
	char buffer[2];

	fd = open(cur_blink_path, O_RDWR);
	igt_require(fd >= 0);

	/* Restore original value on exit */
	if (fbcon_cursor_blink_fd == -1) {
		r = read(fd, fbcon_cursor_blink_prev_value,
			 sizeof(fbcon_cursor_blink_prev_value));
		if (r > 0) {
			fbcon_cursor_blink_fd = dup(fd);
			igt_assert(fbcon_cursor_blink_fd >= 0);
			igt_install_exit_handler(fbcon_cursor_blink_restore);
		}
	}

	r = snprintf(buffer, sizeof(buffer), enable ? "1" : "0");
	write(fd, buffer, r + 1);
	close(fd);
}
