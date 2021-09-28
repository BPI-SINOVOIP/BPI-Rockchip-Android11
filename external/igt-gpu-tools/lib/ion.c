#include <ion/ion.h>
#include <linux/ion_4.12.h>

#include "ion.h"

int ion_get_heap_id(int ion_fd, uint32_t heap_type)
{
	int heap_count = 0;
	int ret = -1;

	if (ion_query_heap_cnt(ion_fd, &heap_count))
	{
		return -1;
	}

	struct ion_heap_data *heap_data =
			malloc(heap_count * sizeof(struct ion_heap_data));

	if (ion_query_get_heaps(ion_fd, heap_count, heap_data))
	{
		free(heap_data);
		return -1;
	}

	for (struct ion_heap_data *hi = heap_data;
	     (hi - heap_data) < heap_count;
	     ++hi)
	{
		if (hi->type == heap_type)
		{
			ret = hi->heap_id;
			break;
		}
	}

	free(heap_data);

	return ret;
}

static const int kBitsInAnInt = (sizeof(int) * 8);

int ion_alloc_one_fd(int ion_fd, size_t size, int heap_id, int *ion_buffer_fd)
{
	if (heap_id < 0 || heap_id >= kBitsInAnInt)
	{
		return -1;
	}

	const int align = 0;
	const int heap_mask = 1 << heap_id;
	const int flags = 0;

	return ion_alloc_fd(ion_fd, size, align, heap_mask, flags, ion_buffer_fd);
}

int ion_mmap(void **ptr, int ion_buffer_fd, size_t size)
{
	void *const k_addr = 0;
	const int k_prot = PROT_READ | PROT_WRITE;
	const int k_flags = MAP_SHARED;

	void *ret = mmap(k_addr, size, k_prot, k_flags, ion_buffer_fd, 0);

	if (ret == MAP_FAILED)
	{
		return -1;
	}

	*ptr = ret;
	return 0;
}

int ion_munmap(void *ptr, size_t size)
{
	if (munmap(ptr, size))
	{
		return -1;
	}

	return 0;
}

int drm_check_prime_caps(int drm_fd)
{
	struct drm_get_cap drm_get_cap_arg = {
		.capability = DRM_CAP_PRIME,
		.value = 0
	};

	if (drmIoctl(drm_fd, DRM_IOCTL_GET_CAP, &drm_get_cap_arg))
	{
		return -1;
	}

	if (!(drm_get_cap_arg.value & DRM_PRIME_CAP_IMPORT) ||
	    !(drm_get_cap_arg.value & DRM_PRIME_CAP_EXPORT))
	{
		return -1;
	}

	return 0;
}

int gem_handle_for_ion_buffer(int drm_fd, uint32_t *gem_handle, int ion_buffer_fd)
{
	struct drm_prime_handle drm_prime_fd_to_handle_arg = {
		.handle = 0,
		.flags = 0,
		.fd = ion_buffer_fd
	};

	if (drmIoctl(drm_fd,
		     DRM_IOCTL_PRIME_FD_TO_HANDLE,
		     &drm_prime_fd_to_handle_arg))
	{
		return -1;
	}

	*gem_handle = drm_prime_fd_to_handle_arg.handle;

	return 0;
}

int ion_fd_for_gem_handle(int drm_fd, int *ion_fd, uint32_t gem_handle)
{
	struct drm_prime_handle drm_prime_handle_to_fd_arg = {
		.handle = gem_handle,
		.flags = 0,
		.fd = 0
	};

	if (drmIoctl(drm_fd,
		     DRM_IOCTL_PRIME_HANDLE_TO_FD,
		     &drm_prime_handle_to_fd_arg))
	{
		return -1;
	}

	*ion_fd = drm_prime_handle_to_fd_arg.fd;

	return 0;
}

int drm_fb_for_ion_buffer(int drm_fd, uint32_t *fb_id, int ion_buffer_fd,
			  const struct fb_configuration *fb_config)
{
	uint32_t gem_handle = 0;

	if (gem_handle_for_ion_buffer(drm_fd, &gem_handle, ion_buffer_fd))
	{
		return -1;
	}

	int ret = drm_fb_for_gem_handle(drm_fd, fb_id, gem_handle, fb_config);

	gem_release_handle(drm_fd, gem_handle);
	return ret;
}

void drm_release_fb(int drm_fd, uint32_t fb_id)
{
	(void)drmIoctl(drm_fd, DRM_IOCTL_MODE_RMFB, &fb_id);
}

int ion_clone_fd_via_gem(int drm_fd, int *cloned_fd, int ion_buffer_fd)
{
	uint32_t gem_handle = 0;
	if (gem_handle_for_ion_buffer(drm_fd, &gem_handle, ion_buffer_fd))
	{
		return -1;
	}

	int ret = ion_fd_for_gem_handle(drm_fd, cloned_fd, gem_handle);
	gem_release_handle(drm_fd, gem_handle);

	return ret;
}


