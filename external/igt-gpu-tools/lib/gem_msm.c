#include "gem_msm.h"
#include "drm-uapi/msm_drm.h"

#include "drm.h"

static int gem_msm_mmap(void **ptr, int drm_fd, uint32_t gem_handle, size_t size)
{
	struct drm_msm_gem_cpu_prep gem_prep = {
		.handle = gem_handle,
		.op = MSM_PREP_READ,
		.timeout = { .tv_sec = 1, .tv_nsec = 0 }
	};

	if (drmIoctl(drm_fd, DRM_IOCTL_MSM_GEM_CPU_PREP, &gem_prep))
	{
		return -1;
	}

	struct drm_msm_gem_info gem_info = {
		.handle = gem_handle,
		.flags = 0,
		.offset = 0
	};

	if (drmIoctl(drm_fd, DRM_IOCTL_MSM_GEM_INFO, &gem_info))
	{
		return -1;
	}

	void *const k_addr = 0;
	const int k_prot = PROT_READ | PROT_WRITE;
	const int k_flags = MAP_SHARED;

	void *ret = mmap(k_addr, size, k_prot, k_flags, drm_fd, gem_info.offset);

	if (ret == MAP_FAILED)
	{
		return -1;
	}

	*ptr = ret;

	return 0;
}

static int gem_msm_munmap(int drm_fd, uint32_t gem_handle, void *ptr, size_t size)
{
	if (munmap(ptr, size))
	{
		return -1;
	}

	struct drm_msm_gem_cpu_fini gem_fini = {
		.handle = gem_handle
	};

	if (drmIoctl(drm_fd, DRM_IOCTL_MSM_GEM_CPU_FINI, &gem_fini))
	{
		return -1;
	}

	return 0;
}

struct gem_driver gem_msm_driver = {
	.mmap = gem_msm_mmap,
	.munmap = gem_msm_munmap
};
