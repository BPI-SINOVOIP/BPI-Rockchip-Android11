#include "drm.h"
#include "gem.h"
#include "igt.h"
#include "ion.h"

#include <ion/ion.h>

size_t size_for_fb(const struct fb_configuration *config)
{
	return config->width * config->height * config->pixel_size;
}

/**
 * test_make_fb
 *
 * Tests that an ion buffer can be ingested into DRM to the point
 * where it can be used for a framebuffer
 **/

static void make_fb_with_buffer(int drm_fd, int ion_fd,
				const struct fb_configuration *config,
				int ion_buffer_fd)
{
	uint32_t fb_id = 0;

	igt_assert_eq(0, drm_check_prime_caps(drm_fd));
	igt_assert_eq(0, drm_fb_for_ion_buffer(
			drm_fd, &fb_id, ion_buffer_fd, config));
	drm_release_fb(drm_fd, fb_id);
}

static void make_fb_with_fds(int drm_fd, int ion_fd,
			     const struct fb_configuration *config)
{
	int ion_buffer_fd;

	const int heap_id = ion_get_heap_id(ion_fd, ION_HEAP_TYPE_SYSTEM);
	igt_assert(heap_id != -1);

	igt_assert(!ion_alloc_one_fd(
			ion_fd,
			size_for_fb(config),
			heap_id,
			&ion_buffer_fd));

        make_fb_with_buffer(drm_fd, ion_fd, config, ion_buffer_fd);

	close(ion_buffer_fd);
}

static void test_make_fb(const struct fb_configuration *config)
{
	const int drm_fd = drm_open_driver(DRIVER_ANY);
	igt_assert(drm_fd >= 0);

	const int ion_fd = ion_open();
	igt_assert(ion_fd >= 0);

	make_fb_with_fds(drm_fd, ion_fd, config);

	ion_close(ion_fd);
	close(drm_fd);
}

/**
 * test_clone
 *
 * Tests that an ion buffer can be 'cloned' by making a GEM buffer out of
 * it and then reversing the process
 **/

static void clone_with_fds(int drm_fd, int ion_fd,
			   const struct fb_configuration *config)
{
	int ion_buffer_fd;

	const int heap_id = ion_get_heap_id(ion_fd, ION_HEAP_TYPE_SYSTEM);
	igt_assert(heap_id != -1);

	igt_assert(!ion_alloc_one_fd(
			ion_fd,
			size_for_fb(config),
			heap_id,
			&ion_buffer_fd));

	int clone_fd = 0;

	igt_assert(!ion_clone_fd_via_gem(drm_fd, &clone_fd, ion_buffer_fd));

	igt_assert(clone_fd >= 0);
	igt_assert(clone_fd != ion_buffer_fd);

	close(clone_fd);
	close(ion_buffer_fd);
}

static void test_clone(const struct fb_configuration *config)
{
	const int drm_fd = drm_open_driver(DRIVER_ANY);
	igt_assert(drm_fd >= 0);

	const int ion_fd = ion_open();
	igt_assert(ion_fd >= 0);

	clone_with_fds(drm_fd, ion_fd, config);

	ion_close(ion_fd);
	close(drm_fd);
}

/**
 * test_mmap
 *
 * Tests that the GEM version of an ion buffer contains the same data that
 * the original ion buffer did
 **/

static void mmap_with_buffer(int drm_fd, int ion_fd,
			     uint8_t *buffer, size_t size)
{
	int ion_buffer_fd;

	const int heap_id = ion_get_heap_id(ion_fd, ION_HEAP_TYPE_SYSTEM);
	igt_assert(heap_id != -1);

	const struct gem_driver *gem = gem_get_driver(drm_fd);
	igt_assert(gem != NULL);

	igt_assert(!ion_alloc_one_fd(
			ion_fd,
			size,
			heap_id,
			&ion_buffer_fd));

	void *ion_ptr = NULL;

	igt_assert(!ion_mmap(&ion_ptr, ion_buffer_fd, size));

	memcpy(buffer, ion_ptr, size);

	igt_assert(!ion_munmap(ion_ptr, size));

	uint32_t gem_handle = 0;

	igt_assert(!gem_handle_for_ion_buffer(
			drm_fd,
			&gem_handle,
			ion_buffer_fd));

	close(ion_buffer_fd);

	size_t gem_buf_size = 0;

	igt_assert(!gem_size(drm_fd, &gem_buf_size, gem_handle));
	igt_assert_eq(gem_buf_size, size);

	void *gem_ptr = NULL;
	igt_assert(!gem->mmap(
			&gem_ptr,
			drm_fd,
			gem_handle,
			size));

	igt_assert(!memcmp(buffer, gem_ptr, size));

	igt_assert(!gem->munmap(
			drm_fd,
			gem_handle,
			gem_ptr,
			size));

	gem_release_handle(drm_fd, gem_handle);
}

static void mmap_with_fds(int drm_fd, int ion_fd,
			      const struct fb_configuration *config)
{
	uint8_t *buffer = malloc(size_for_fb(config));
	igt_assert(buffer);

	mmap_with_buffer(drm_fd, ion_fd, buffer, size_for_fb(config));

	free(buffer);
}

static void test_mmap(const struct fb_configuration *config)
{
	const int drm_fd = drm_open_driver(DRIVER_ANY);
	igt_assert(drm_fd >= 0);

	const int ion_fd = ion_open();
	igt_assert(ion_fd >= 0);

	mmap_with_fds(drm_fd, ion_fd, config);

	ion_close(ion_fd);
	close(drm_fd);
}

igt_main
{
	const struct fb_configuration config = {
		.width = 1024,
		.height = 1024,
		.pixel_format = DRM_FORMAT_ABGR8888,
		.pixel_size = 4
	};

	igt_subtest("make-fb")
	{
		test_make_fb(&config);
	}

	igt_subtest("clone")
	{
		test_clone(&config);
	}

	igt_subtest("mmap")
	{
		test_mmap(&config);
	}
}
