/* Copyright 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <sys/cdefs.h>
#include <sys/mman.h>
#ifdef __BIONIC__
#include <cutils/ashmem.h>
#else
#include <sys/shm.h>
#endif
#include <errno.h>
#include <syslog.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "cras_shm.h"

int cras_shm_info_init(const char *stream_name, uint32_t length,
		       struct cras_shm_info *info_out)
{
	struct cras_shm_info info;

	if (!info_out)
		return -EINVAL;

	strncpy(info.name, stream_name, sizeof(info.name) - 1);
	info.name[sizeof(info.name) - 1] = '\0';
	info.length = length;
	info.fd = cras_shm_open_rw(info.name, info.length);
	if (info.fd < 0)
		return info.fd;

	*info_out = info;

	return 0;
}

int cras_shm_info_init_with_fd(int fd, size_t length,
			       struct cras_shm_info *info_out)
{
	struct cras_shm_info info;

	if (!info_out)
		return -EINVAL;

	info.name[0] = '\0';
	info.length = length;
	info.fd = dup(fd);
	if (info.fd < 0)
		return info.fd;

	*info_out = info;

	return 0;
}

/* Move the resources from the cras_shm_info 'from' into the cras_shm_info 'to'.
 * The owner of 'to' will be responsible for cleaning up those resources with
 * cras_shm_info_cleanup.
 */
static int cras_shm_info_move(struct cras_shm_info *from,
			      struct cras_shm_info *to)
{
	if (!from || !to)
		return -EINVAL;

	*to = *from;
	from->fd = -1;
	from->name[0] = '\0';
	return 0;
}

void cras_shm_info_cleanup(struct cras_shm_info *info)
{
	if (!info)
		return;

	if (info->name[0] != '\0')
		cras_shm_close_unlink(info->name, info->fd);
	else
		close(info->fd);

	info->fd = -1;
	info->name[0] = '\0';
}

int cras_audio_shm_create(struct cras_shm_info *header_info,
			  struct cras_shm_info *samples_info, int samples_prot,
			  struct cras_audio_shm **shm_out)
{
	struct cras_audio_shm *shm;
	int ret;

	if (!header_info || !samples_info || !shm_out) {
		ret = -EINVAL;
		goto cleanup_info;
	}

	if (samples_prot != PROT_READ && samples_prot != PROT_WRITE) {
		ret = -EINVAL;
		syslog(LOG_ERR,
		       "cras_shm: samples must be mapped read or write only");
		goto cleanup_info;
	}

	shm = calloc(1, sizeof(*shm));
	if (!shm) {
		ret = -ENOMEM;
		goto cleanup_info;
	}

	/* Move the cras_shm_info params into the new cras_audio_shm object.
	 * The parameters are cleared, and the owner of cras_audio_shm is now
	 * responsible for closing the fds and unlinking any associated shm
	 * files using cras_audio_shm_destroy.
	 *
	 * The source pointers are updated to point to the moved structs so that
	 * they will be properly cleaned up in the error case.
	 */
	ret = cras_shm_info_move(header_info, &shm->header_info);
	if (ret)
		goto free_shm;
	header_info = &shm->header_info;

	ret = cras_shm_info_move(samples_info, &shm->samples_info);
	if (ret)
		goto free_shm;
	samples_info = &shm->samples_info;

	shm->header = mmap(NULL, header_info->length, PROT_READ | PROT_WRITE,
			   MAP_SHARED, header_info->fd, 0);
	if (shm->header == (struct cras_audio_shm_header *)-1) {
		ret = errno;
		syslog(LOG_ERR, "cras_shm: mmap failed to map shm for header.");
		goto free_shm;
	}

	shm->samples = mmap(NULL, samples_info->length, samples_prot,
			    MAP_SHARED, samples_info->fd, 0);
	if (shm->samples == (uint8_t *)-1) {
		ret = errno;
		syslog(LOG_ERR,
		       "cras_shm: mmap failed to map shm for samples.");
		goto unmap_header;
	}

	cras_shm_set_volume_scaler(shm, 1.0);

	*shm_out = shm;
	return 0;

unmap_header:
	munmap(shm->header, shm->header_info.length);
free_shm:
	free(shm);
cleanup_info:
	cras_shm_info_cleanup(samples_info);
	cras_shm_info_cleanup(header_info);
	return ret;
}

void cras_audio_shm_destroy(struct cras_audio_shm *shm)
{
	if (!shm)
		return;

	munmap(shm->samples, shm->samples_info.length);
	cras_shm_info_cleanup(&shm->samples_info);
	munmap(shm->header, shm->header_info.length);
	cras_shm_info_cleanup(&shm->header_info);
	free(shm);
}

/* Set the correct SELinux label for SHM fds. */
static void cras_shm_restorecon(int fd)
{
#ifdef CRAS_SELINUX
	char fd_proc_path[64];

	if (snprintf(fd_proc_path, sizeof(fd_proc_path), "/proc/self/fd/%d",
		     fd) < 0) {
		syslog(LOG_WARNING,
		       "Couldn't construct proc symlink path of fd: %d", fd);
		return;
	}

	/* Get the actual file-path for this fd. */
	char *path = realpath(fd_proc_path, NULL);
	if (path == NULL) {
		syslog(LOG_WARNING, "Couldn't run realpath() for %s: %s",
		       fd_proc_path, strerror(errno));
		return;
	}

	if (cras_selinux_restorecon(path) < 0) {
		syslog(LOG_WARNING, "Restorecon on %s failed: %s", fd_proc_path,
		       strerror(errno));
	}

	free(path);
#endif
}

#ifdef __BIONIC__

int cras_shm_open_rw(const char *name, size_t size)
{
	int fd;

	/* Eliminate the / in the shm_name. */
	if (name[0] == '/')
		name++;
	fd = ashmem_create_region(name, size);
	if (fd < 0) {
		fd = -errno;
		syslog(LOG_ERR, "failed to ashmem_create_region %s: %s\n", name,
		       strerror(-fd));
	}
	return fd;
}

int cras_shm_reopen_ro(const char *name, int fd)
{
	/* After mmaping the ashmem read/write, change it's protection
	   bits to disallow further write access. */
	if (ashmem_set_prot_region(fd, PROT_READ) != 0) {
		fd = -errno;
		syslog(LOG_ERR, "failed to ashmem_set_prot_region %s: %s\n",
		       name, strerror(-fd));
	}
	return fd;
}

void cras_shm_close_unlink(const char *name, int fd)
{
	close(fd);
}

#else

int cras_shm_open_rw(const char *name, size_t size)
{
	int fd;
	int rc;

	fd = shm_open(name, O_CREAT | O_EXCL | O_RDWR, 0600);
	if (fd < 0) {
		fd = -errno;
		syslog(LOG_ERR, "failed to shm_open %s: %s\n", name,
		       strerror(-fd));
		return fd;
	}
	rc = ftruncate(fd, size);
	if (rc) {
		rc = -errno;
		syslog(LOG_ERR, "failed to set size of shm %s: %s\n", name,
		       strerror(-rc));
		return rc;
	}

	cras_shm_restorecon(fd);

	return fd;
}

int cras_shm_reopen_ro(const char *name, int fd)
{
	/* Open a read-only copy to dup and pass to clients. */
	fd = shm_open(name, O_RDONLY, 0);
	if (fd < 0) {
		fd = -errno;
		syslog(LOG_ERR,
		       "Failed to re-open shared memory '%s' read-only: %s",
		       name, strerror(-fd));
	}
	return fd;
}

void cras_shm_close_unlink(const char *name, int fd)
{
	shm_unlink(name);
	close(fd);
}

#endif

void *cras_shm_setup(const char *name, size_t mmap_size, int *rw_fd_out,
		     int *ro_fd_out)
{
	int rw_shm_fd = cras_shm_open_rw(name, mmap_size);
	if (rw_shm_fd < 0)
		return NULL;

	/* mmap shm. */
	void *exp_state = mmap(NULL, mmap_size, PROT_READ | PROT_WRITE,
			       MAP_SHARED, rw_shm_fd, 0);
	if (exp_state == (void *)-1)
		return NULL;

	/* Open a read-only copy to dup and pass to clients. */
	int ro_shm_fd = cras_shm_reopen_ro(name, rw_shm_fd);
	if (ro_shm_fd < 0)
		return NULL;

	*rw_fd_out = rw_shm_fd;
	*ro_fd_out = ro_shm_fd;

	return exp_state;
}
