#ifndef GEM_H
#define GEM_H

#include "igt.h"

struct gem_driver
{
	/**
	 * mmap:
	 * @ptr: (out) pointer to the buffer in the user process's memory
	 * @drm_fd: open DRM device fd
	 * @gem_handle: GEM handle
	 * @size: exact size of the GEM buffer
	 *
	 * Maps the buffer backing the GEM handle for reading and writing.
	 *
	 * Returns: 0 on success, -1 otherwise
	 **/
	int (*mmap)(void **ptr, int drm_fd, uint32_t gem_handle, size_t size);

	/**
	 * munmap:
	 * @drm_fd: open DRM device fd
	 * @gem_handle: GEM handle
	 * @ptr: pointer (see ptr argument to mmap)
	 * @size: exact size of the mapped area
	 *
	 * Unmaps a region previously mapped with mmap.
	 *
	 * Returns: 0 on success: -1 otherwise
	 **/
	int (*munmap)(int drm_fd, uint32_t gem_handle, void *ptr, size_t size);
};

/**
 * gem_get_driver:
 * @drm_fd: open DRM device fd
 *
 * Gets the driver-specific GEM APIs for a particular device.
 *
 * Returns: a struct with function pointers on success; NULL otherwise
 **/
struct gem_driver *gem_get_driver(int drm_fd);

/**
 * gem_size:
 * @drm_fd: open DRM device fd
 * @size: (out) size of the buffer
 * @gem_handle: GEM handle
 * Returns: size of the buffer backing the GEM handle.
 **/
int gem_size(int drm_fd, size_t *size, uint32_t gem_handle);

/**
 * gem_release_handle
 * @drm_fd: open DRM device fd
 * @gem_handle: GEM handle
 *
 * Releases a GEM handle.
 **/
void gem_release_handle(int drm_fd, uint32_t gem_handle);

struct fb_configuration
{
	uint32_t width;
	uint32_t height;
	uint32_t pixel_format;
	uint32_t pixel_size;
};

/**
 * drm_fb_for_gem_handle
 * @drm_fd: open DRM device fd
 * @fb_id: (out) id of the DRM KMS fb
 * @gem_handle: GEM handle
 * @fb_config: metadata for the fb
 *
 * Converts a GEM buffer into a DRM KMS fb
 *
 * Returns: 0 if the buffer could be converted; -1 otherwise
 **/
int drm_fb_for_gem_handle(int drm_fd, uint32_t *fb_id, uint32_t gem_handle,
			  const struct fb_configuration *fb_config);

#endif
