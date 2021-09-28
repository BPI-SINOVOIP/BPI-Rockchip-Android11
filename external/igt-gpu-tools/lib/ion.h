#ifndef ION_GEM_H
#define ION_GEM_H

#include "igt.h"
#include "gem.h"

/**
 * ion_get_heap_id:
 * @ion_fd: open ion device fd
 * @heap_type: ION_HEAP_TYPE_* constant
 * Returns: the index of the first heap with type matching heap_type, or -1 on
 * failure
 **/
int ion_get_heap_id(int ion_fd, uint32_t heap_type);

/**
 * ion_alloc_one_fd
 * @ion_fd: open ion device fd
 * @size: size of the desired ion buffer
 * @heap_id: index of the heap to allocate from
 * @ion_buffer_fd: (out) ion buffer fd
 * Returns: 0 on success; not 0 otherwise
 **/
int ion_alloc_one_fd(int ion_fd, size_t size, int heap_id, int *ion_buffer_fd);

/**
 * ion_mmap
 * @ptr: (out) pointer to the buffer in the user process's memory
 * @ion_buffer_fd: ion buffer fd
 * @size: size of the desired mapping
 * Returns: 0 on success; not 0 otherwise
 **/
int ion_mmap(void **ptr, int ion_buffer_fd, size_t size);

/**
 * ion_munmap
 * @ptr: pointer to the buffer in the user process's memory
 * @size: exact size of the mapping
 * Returns: 0 on success; not 0 otherwise
 **/
int ion_munmap(void *ptr, size_t size);

/**
 * drm_check_prime_caps
 * drm_fd: open DRM device fd
 * Returns: 0 if the device supports Prime import/export; -1 otherwise
 **/
int drm_check_prime_caps(int drm_fd);

/**
 * gem_handle_for_ion_buffer
 * drm_fd: open DRM device fd
 * gem_handle: (out) GEM handle
 * ion_fd: ion buffer fd
 *
 * Imports an ion buffer into GEM
 *
 * Returns: 0 if the ion buffer could be imported; -1 otherwise
 **/
int gem_handle_for_ion_buffer(int drm_fd, uint32_t *gem_handle, int ion_buffer_fd);

/**
 * ion_fd_for_gem_handle
 * drm_fd: open DRM device fd
 * ion_fd: ion buffer fd
 *
 * Exports a GEM buffer into ion
 *
 * Returns: 0 if the buffer could be exported; -1 otherwise
 **/
int ion_fd_for_gem_handle(int drm_fd, int *ion_bufferfd, uint32_t gem_handle);

/**
 * drm_fb_for_ion_buffer
 * drm_fd: open DRM device fd
 * fb_id: (out) id of the DRM KMS fb
 * ion_fd: ion buffer fd
 * fb_config: metadata for the fb
 *
 * Converts an ion buffer into a DRM KMS fb
 *
 * Returns: 0 if the buffer could be exported; -1 otherwise
 **/
int drm_fb_for_ion_buffer(int drm_fd, uint32_t *fb_id, int ion_buffer_fd,
			  const struct fb_configuration *fb_config);

/**
 * drm_release_fb
 * drm_fd: open DRM device fd
 * fb_id: id of the DRM KMS fb
 *
 * Releases the DRM KMS fb
 **/
void drm_release_fb(int drm_fd, uint32_t fb_id);

/**
 * ion_clone_fd_via_gem
 * drm_fd: open DRM device fd
 * cloned_fd: (out) cloned buffer fd
 * ion_fd: ion buffer fd
 *
 * Uses GEM to clone an ion fd by importing and re-exporting it.
 *
 * Returns: 0 if the buffer could be cloned; -1 otherwise
 **/
int ion_clone_fd_via_gem(int drm_fd, int *cloned_fd, int ion_buffer_fd);

#endif
