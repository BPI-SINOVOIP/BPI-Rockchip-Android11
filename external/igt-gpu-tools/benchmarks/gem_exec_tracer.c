/*
 * Copyright © 2015 Intel Corporation
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
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdarg.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <errno.h>
#include <sys/mman.h>
#include <dlfcn.h>
#include <i915_drm.h>
#include <pthread.h>

#include "intel_aub.h"
#include "intel_chipset.h"

static int (*libc_close)(int fd);
static int (*libc_ioctl)(int fd, unsigned long request, void *argp);

static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

struct trace {
	int fd;
	FILE *file;
	struct trace *next;
} *traces;

#define DRM_MAJOR 226

enum {
	ADD_BO = 0,
	DEL_BO,
	ADD_CTX,
	DEL_CTX,
	EXEC,
	WAIT,
};

static struct trace_verion {
	uint32_t magic;
	uint32_t version;
} version = {
	.magic = 0xdeadbeef,
	.version = 1
};

struct trace_add_bo {
	uint8_t cmd;
	uint32_t handle;
	uint64_t size;
} __attribute__((packed));
struct trace_del_bo {
	uint8_t cmd;
	uint32_t handle;
}__attribute__((packed));

struct trace_add_ctx {
	uint8_t cmd;
	uint32_t handle;
} __attribute__((packed));
struct trace_del_ctx {
	uint8_t cmd;
	uint32_t handle;
}__attribute__((packed));

struct trace_exec {
	uint8_t cmd;
	uint32_t object_count;
	uint64_t flags;
	uint32_t context;
}__attribute__((packed));

struct trace_exec_object {
	uint32_t handle;
	uint32_t relocation_count;
	uint64_t alignment;
	uint64_t offset;
	uint64_t flags;
	uint64_t rsvd1;
	uint64_t rsvd2;
}__attribute__((packed));

struct trace_exec_relocation {
	uint32_t target_handle;
	uint32_t delta;
	uint64_t offset;
	uint64_t presumed_offset;
	uint32_t read_domains;
	uint32_t write_domain;
}__attribute__((packed));

struct trace_wait {
	uint8_t cmd;
	uint32_t handle;
} __attribute__((packed));

static void __attribute__ ((format(__printf__, 2, 3)))
fail_if(int cond, const char *format, ...)
{
	va_list args;

	if (!cond)
		return;

	va_start(args, format);
	vfprintf(stderr, format, args);
	va_end(args);

	abort();
}

#define LOCAL_I915_EXEC_FENCE_IN              (1<<16)
#define LOCAL_I915_EXEC_FENCE_OUT             (1<<17)

static void
trace_exec(struct trace *trace,
	   const struct drm_i915_gem_execbuffer2 *execbuffer2)
{
#define to_ptr(T, x) ((T *)(uintptr_t)(x))
	const struct drm_i915_gem_exec_object2 *exec_objects =
		to_ptr(typeof(*exec_objects), execbuffer2->buffers_ptr);

	fail_if(execbuffer2->flags & (LOCAL_I915_EXEC_FENCE_IN | LOCAL_I915_EXEC_FENCE_OUT),
		"fences not supported yet\n");

	flockfile(trace->file);
	{
		struct trace_exec t = {
			EXEC,
			execbuffer2->buffer_count,
			execbuffer2->flags,
			execbuffer2->rsvd1,
		};
		fwrite(&t, sizeof(t), 1, trace->file);
	}

	for (uint32_t i = 0; i < execbuffer2->buffer_count; i++) {
		const struct drm_i915_gem_exec_object2 *obj = &exec_objects[i];
		const struct drm_i915_gem_relocation_entry *relocs =
			to_ptr(typeof(*relocs), obj->relocs_ptr);
		{
			struct trace_exec_object t = {
				obj->handle,
				obj->relocation_count,
				obj->alignment,
				obj->offset,
				obj->flags,
				obj->rsvd1,
				obj->rsvd2
			};
			fwrite(&t, sizeof(t), 1, trace->file);
		}
		fwrite(relocs, sizeof(*relocs), obj->relocation_count,
		       trace->file);
	}

	fflush(trace->file);
	funlockfile(trace->file);
#undef to_ptr
}

static void
trace_wait(struct trace *trace, uint32_t handle)
{
	struct trace_wait t = { WAIT, handle };
	fwrite(&t, sizeof(t), 1, trace->file);
}

static void
trace_add(struct trace *trace, uint32_t handle, uint64_t size)
{
	struct trace_add_bo t = { ADD_BO, handle, size };
	fwrite(&t, sizeof(t), 1, trace->file);
}

static void
trace_del(struct trace *trace, uint32_t handle)
{
	struct trace_del_bo t = { DEL_BO, handle };
	fwrite(&t, sizeof(t), 1, trace->file);
}

static void
trace_add_context(struct trace *trace, uint32_t handle)
{
	struct trace_add_ctx t = { ADD_CTX, handle };
	fwrite(&t, sizeof(t), 1, trace->file);
}

static void
trace_del_context(struct trace *trace, uint32_t handle)
{
	struct trace_del_ctx t = { DEL_CTX, handle };
	fwrite(&t, sizeof(t), 1, trace->file);
}

int
close(int fd)
{
	struct trace *t, **p;

	pthread_mutex_lock(&mutex);
	for (p = &traces; (t = *p); p = &t->next) {
		if (t->fd == fd) {
			*p = t->next;
			fclose(t->file);
			free(t);
			break;
		}
	}
	pthread_mutex_unlock(&mutex);

	return libc_close(fd);
}

static unsigned long
size_for_fb(const struct drm_mode_fb_cmd *cmd)
{
	unsigned long size;

#ifndef ALIGN
#define ALIGN(x, y) (((x) + (y) - 1) & -(y))
#endif

	size = ALIGN(cmd->width * cmd->bpp, 64);
	size *= cmd->height;
	return ALIGN(size, 4096);
}

static int is_i915(int fd)
{
	drm_version_t v;
	char name[5] = "";

	memset(&v, 0, sizeof(v));
	v.name_len = 4;
	v.name = name;

	if (libc_ioctl(fd, DRM_IOCTL_VERSION, &v))
		return 0;

	return strcmp(name, "i915") == 0;
}

#define LOCAL_IOCTL_I915_GEM_EXECBUFFER2_WR \
    DRM_IOWR(DRM_COMMAND_BASE + DRM_I915_GEM_EXECBUFFER2, struct drm_i915_gem_execbuffer2)

int
ioctl(int fd, unsigned long request, ...)
{
	struct trace *t, **p;
	va_list args;
	void *argp;
	int ret;

	va_start(args, request);
	argp = va_arg(args, void *);
	va_end(args);

	if (_IOC_TYPE(request) != DRM_IOCTL_BASE)
		goto untraced;

	pthread_mutex_lock(&mutex);
	for (p = &traces; (t = *p); p = &t->next) {
		if (fd == t->fd) {
			if (traces != t) {
				*p = t->next;
				t->next = traces;
				traces = t;
			}
			break;
		}
	}
	if (!t) {
		char filename[80];

		if (!is_i915(fd)) {
			pthread_mutex_unlock(&mutex);
			goto untraced;
		}

		t = malloc(sizeof(*t));
		if (!t) {
			pthread_mutex_unlock(&mutex);
			return -ENOMEM;
		}

		sprintf(filename, "/tmp/trace-%d.%d", getpid(), fd);
		t->file = fopen(filename, "w+");
		t->fd = fd;

		if (!fwrite(&version, sizeof(version), 1, t->file)) {
			pthread_mutex_unlock(&mutex);
			fclose(t->file);
			free(t);
			return -ENOMEM;
		}

		t->next = traces;
		traces = t;
	}
	pthread_mutex_unlock(&mutex);

	switch (request) {
	case DRM_IOCTL_I915_GEM_EXECBUFFER2:
	case LOCAL_IOCTL_I915_GEM_EXECBUFFER2_WR:
		trace_exec(t, argp);
		break;

	case DRM_IOCTL_GEM_CLOSE: {
		struct drm_gem_close *close = argp;
		trace_del(t, close->handle);
		break;
	}

	case DRM_IOCTL_I915_GEM_CONTEXT_DESTROY: {
		struct drm_i915_gem_context_destroy *close = argp;
		trace_del_context(t, close->ctx_id);
		break;
	}

	case DRM_IOCTL_I915_GEM_WAIT: {
		struct drm_i915_gem_wait *w = argp;
		trace_wait(t, w->bo_handle);
		break;
	}

	case DRM_IOCTL_I915_GEM_SET_DOMAIN: {
		struct drm_i915_gem_set_domain *w = argp;
		trace_wait(t, w->handle);
		break;
	}
	}

	ret = libc_ioctl(fd, request, argp);
	if (ret)
		return ret;

	switch (request) {
	case DRM_IOCTL_I915_GEM_CREATE: {
		struct drm_i915_gem_create *create = argp;
		trace_add(t, create->handle, create->size);
		break;
	}

	case DRM_IOCTL_I915_GEM_USERPTR: {
		struct drm_i915_gem_userptr *userptr = argp;
		trace_add(t, userptr->handle, userptr->user_size);
		break;
	}

	case DRM_IOCTL_GEM_OPEN: {
		struct drm_gem_open *open = argp;
		trace_add(t, open->handle, open->size);
		break;
	}

	case DRM_IOCTL_PRIME_FD_TO_HANDLE: {
		struct drm_prime_handle *prime = argp;
		off_t size = lseek(prime->fd, 0, SEEK_END);
		fail_if(size == -1, "failed to get prime bo size\n");
		trace_add(t, prime->handle, size);
		break;
	}

	case DRM_IOCTL_MODE_GETFB: {
		struct drm_mode_fb_cmd *cmd = argp;
		trace_add(t, cmd->handle, size_for_fb(cmd));
		break;
	}

	case DRM_IOCTL_I915_GEM_CONTEXT_CREATE: {
		struct drm_i915_gem_context_create *create = argp;
		trace_add_context(t, create->ctx_id);
		break;
	}
	}

	return 0;

untraced:
	return libc_ioctl(fd, request, argp);
}

static void __attribute__ ((constructor))
init(void)
{
	libc_close = dlsym(RTLD_NEXT, "close");
	libc_ioctl = dlsym(RTLD_NEXT, "ioctl");
	fail_if(libc_close == NULL || libc_ioctl == NULL,
		"failed to get libc ioctl or close\n");
}
