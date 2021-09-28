/*
 * Copyright © 2007, 2011, 2013 Intel Corporation
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
 * Authors:
 *    Eric Anholt <eric@anholt.net>
 *    Daniel Vetter <daniel.vetter@ffwll.ch>
 *
 */

#ifdef HAVE_LIBGEN_H
#include <libgen.h>
#endif
#include <stdio.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <string.h>
#include <sys/mman.h>
#include <signal.h>
#include <pciaccess.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/syscall.h>
#include <sys/utsname.h>
#include <termios.h>
#include <pthread.h>

#include "drmtest.h"
#include "i915_drm.h"
#include "intel_chipset.h"
#include "intel_io.h"
#include "igt_debugfs.h"
#include "igt_device.h"
#include "igt_gt.h"
#include "igt_kmod.h"
#include "igt_sysfs.h"
#include "version.h"
#include "config.h"
#include "intel_reg.h"
#include "ioctl_wrappers.h"
#include "igt_dummyload.h"

/**
 * SECTION:drmtest
 * @short_description: Base library for drm tests and tools
 * @title: drmtest
 * @include: igt.h
 *
 * This library contains the basic support for writing tests, with the most
 * important part being the helper function to open drm device nodes.
 *
 * But there's also a bit of other assorted stuff here.
 *
 * Note that this library's header pulls in the [i-g-t core](igt-gpu-tools-i-g-t-core.html)
 * and [batchbuffer](igt-gpu-tools-intel-batchbuffer.html) libraries as dependencies.
 */

static int __get_drm_device_name(int fd, char *name, int name_size)
{
	drm_version_t version;

	memset(&version, 0, sizeof(version));
	version.name_len = name_size;
	version.name = name;

	if (!drmIoctl(fd, DRM_IOCTL_VERSION, &version)){
		return 0;
	}

	return -1;
}

static bool __is_device(int fd, const char *expect)
{
	char name[12] = "";

	if (__get_drm_device_name(fd, name, sizeof(name) - 1))
		return false;

	return strcmp(expect, name) == 0;
}

bool is_amdgpu_device(int fd)
{
	return __is_device(fd, "amdgpu");
}

bool is_i915_device(int fd)
{
	return __is_device(fd, "i915");
}

bool is_vc4_device(int fd)
{
	return __is_device(fd, "vc4");
}

static bool has_known_intel_chipset(int fd)
{
	struct drm_i915_getparam gp;
	int devid = 0;

	memset(&gp, 0, sizeof(gp));
	gp.param = I915_PARAM_CHIPSET_ID;
	gp.value = &devid;

	if (ioctl(fd, DRM_IOCTL_I915_GETPARAM, &gp, sizeof(gp)))
		return false;

	if (!intel_gen(devid))
		return false;

	return true;
}

static char _forced_driver[16] = "";

/**
 * __set_forced_driver:
 * @name: name of driver to forcibly use
 *
 * Set the name of a driver to use when calling #drm_open_driver with
 * the #DRIVER_ANY flag.
 */
void __set_forced_driver(const char *name)
{
	if (!name) {
		igt_warn("No driver specified, keep default behaviour\n");
		return;
	}

	strncpy(_forced_driver, name, sizeof(_forced_driver) - 1);
}

static const char *forced_driver(void)
{
	if (_forced_driver[0])
		return _forced_driver;

	return NULL;
}

#define LOCAL_I915_EXEC_VEBOX	(4 << 0)
/**
 * gem_quiescent_gpu:
 * @fd: open i915 drm file descriptor
 *
 * Ensure the gpu is idle by launching a nop execbuf and stalling for it. This
 * is automatically run when opening a drm device node and is also installed as
 * an exit handler to have the best assurance that the test is run in a pristine
 * and controlled environment.
 *
 * This function simply allows tests to make additional calls in-between, if so
 * desired.
 */
void gem_quiescent_gpu(int fd)
{
	igt_terminate_spins();

	igt_drop_caches_set(fd,
			    DROP_ACTIVE | DROP_RETIRE | DROP_IDLE | DROP_FREED);
}

static int modprobe(const char *driver)
{
	return igt_kmod_load(driver, "");
}

static void modprobe_i915(const char *name)
{
	/* When loading i915, we also want to load snd-hda et al */
	igt_i915_driver_load(NULL);
}

static const struct module {
	unsigned int bit;
	const char *module;
	void (*modprobe)(const char *name);
} modules[] = {
	{ DRIVER_AMDGPU, "amdgpu" },
	{ DRIVER_INTEL, "i915", modprobe_i915 },
	{ DRIVER_PANFROST, "panfrost" },
	{ DRIVER_V3D, "v3d" },
	{ DRIVER_VC4, "vc4" },
	{ DRIVER_VGEM, "vgem" },
	{}
};

static int open_device(const char *name, unsigned int chipset)
{
	const char *forced;
	char dev_name[16] = "";
	int chip = DRIVER_ANY;
	int fd;

	fd = open(name, O_RDWR);
	if (fd == -1)
		return -1;

	if (__get_drm_device_name(fd, dev_name, sizeof(dev_name) - 1) == -1)
		goto err;

	forced = forced_driver();
	if (forced && chipset == DRIVER_ANY && strcmp(forced, dev_name))
		goto err;

	for (int start = 0, end = ARRAY_SIZE(modules) - 1; start < end; ){
		int mid = start + (end - start) / 2;
		int ret = strcmp(modules[mid].module, dev_name);
		if (ret < 0) {
			start = mid + 1;
		} else if (ret > 0) {
			end = mid;
		} else {
			chip = modules[mid].bit;
			break;
		}
	}
	if ((chipset & chip) == chip)
		return fd;

err:
	close(fd);
	return -1;
}

static int __search_and_open(const char *base, int offset, unsigned int chipset)
{
	const char *forced;

	forced = forced_driver();
	if (forced)
		igt_info("Force option used: Using driver %s\n", forced);

	for (int i = 0; i < 16; i++) {
		char name[80];
		int fd;

		sprintf(name, "%s%u", base, i + offset);
		fd = open_device(name, chipset);
		if (fd != -1)
			return fd;
	}

	return -1;
}

static int __open_driver(const char *base, int offset, unsigned int chipset)
{
	static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
	int fd;

	fd = __search_and_open(base, offset, chipset);
	if (fd != -1)
		return fd;

	pthread_mutex_lock(&mutex);
	for (const struct module *m = modules; m->module; m++) {
		if (chipset & m->bit) {
			if (m->modprobe)
				m->modprobe(m->module);
			else
				modprobe(m->module);
		}
	}
	pthread_mutex_unlock(&mutex);

	return __search_and_open(base, offset, chipset);
}

/**
 * __drm_open_driver:
 * @chipset: OR'd flags for each chipset to search, eg. #DRIVER_INTEL
 *
 * Open the first DRM device we can find, searching up to 16 device nodes
 *
 * Returns:
 * An open DRM fd or -1 on error
 */
int __drm_open_driver(int chipset)
{
	return __open_driver("/dev/dri/card", 0, chipset);
}

static int __drm_open_driver_render(int chipset)
{
	return __open_driver("/dev/dri/renderD", 128, chipset);
}

static int at_exit_drm_fd = -1;
static int at_exit_drm_render_fd = -1;

static void __cancel_work_at_exit(int fd)
{
	igt_terminate_spins(); /* for older kernels */

	igt_sysfs_set_parameter(fd, "reset", "%x", -1u /* any method */);
	igt_drop_caches_set(fd,
			    /* cancel everything */
			    DROP_RESET_ACTIVE | DROP_RESET_SEQNO |
			    /* cleanup */
			    DROP_ACTIVE | DROP_RETIRE | DROP_IDLE | DROP_FREED);
}

static void cancel_work_at_exit(int sig)
{
	if (at_exit_drm_fd < 0)
		return;

	__cancel_work_at_exit(at_exit_drm_fd);

	close(at_exit_drm_fd);
	at_exit_drm_fd = -1;
}

static void cancel_work_at_exit_render(int sig)
{
	if (at_exit_drm_render_fd < 0)
		return;

	__cancel_work_at_exit(at_exit_drm_render_fd);

	close(at_exit_drm_render_fd);
	at_exit_drm_render_fd = -1;
}

static const char *chipset_to_str(int chipset)
{
	switch (chipset) {
	case DRIVER_INTEL:
		return "intel";
	case DRIVER_V3D:
		return "v3d";
	case DRIVER_VC4:
		return "vc4";
	case DRIVER_VGEM:
		return "vgem";
	case DRIVER_AMDGPU:
		return "amdgpu";
	case DRIVER_PANFROST:
		return "panfrost";
	case DRIVER_ANY:
		return "any";
	default:
		return "other";
	}
}

/**
 * drm_open_driver:
 * @chipset: OR'd flags for each chipset to search, eg. #DRIVER_INTEL
 *
 * Open a drm legacy device node. This function always returns a valid
 * file descriptor.
 *
 * Returns: a drm file descriptor
 */
int drm_open_driver(int chipset)
{
	static int open_count;
	int fd;

	fd = __drm_open_driver(chipset);
	igt_skip_on_f(fd<0, "No known gpu found for chipset flags 0x%u (%s)\n",
		      chipset, chipset_to_str(chipset));

	/* For i915, at least, we ensure that the driver is idle before
	 * starting a test and we install an exit handler to wait until
	 * idle before quitting.
	 */
	if (is_i915_device(fd)) {
		if (__sync_fetch_and_add(&open_count, 1) == 0) {
			gem_quiescent_gpu(fd);

			at_exit_drm_fd = __drm_open_driver(chipset);
			igt_install_exit_handler(cancel_work_at_exit);
		}
	}

	return fd;
}

/**
 * drm_open_driver_master:
 * @chipset: OR'd flags for each chipset to search, eg. #DRIVER_INTEL
 *
 * Open a drm legacy device node and ensure that it is drm master.
 *
 * Returns:
 * The drm file descriptor or -1 on error
 */
int drm_open_driver_master(int chipset)
{
	int fd = drm_open_driver(chipset);

	igt_device_set_master(fd);

	return fd;
}

/**
 * drm_open_driver_render:
 * @chipset: OR'd flags for each chipset to search, eg. #DRIVER_INTEL
 *
 * Open a drm render device node.
 *
 * Returns:
 * The drm file descriptor or -1 on error
 */
int drm_open_driver_render(int chipset)
{
	static int open_count;
	int fd = __drm_open_driver_render(chipset);

	/* no render nodes, fallback to drm_open_driver() */
	if (fd == -1)
		return drm_open_driver(chipset);

	if (__sync_fetch_and_add(&open_count, 1))
		return fd;

	at_exit_drm_render_fd = __drm_open_driver(chipset);
	if(chipset & DRIVER_INTEL){
		gem_quiescent_gpu(fd);
		igt_install_exit_handler(cancel_work_at_exit_render);
	}

	return fd;
}

void igt_require_amdgpu(int fd)
{
	igt_require(is_amdgpu_device(fd));
}

void igt_require_intel(int fd)
{
	igt_require(is_i915_device(fd) && has_known_intel_chipset(fd));
}

void igt_require_vc4(int fd)
{
	igt_require(is_vc4_device(fd));
}
