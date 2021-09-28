#include "gem.h"

#include "gem_msm.h"

struct gem_driver_lookup
{
	const char name[16];
	struct gem_driver *driver;
};

static struct gem_driver_lookup drivers[] = {
	{
		.name = "msm_drm",
		.driver = &gem_msm_driver
	}
};

static inline size_t num_drivers()
{
	return ARRAY_SIZE(drivers);
}

struct gem_driver *gem_get_driver(int drm_fd)
{
	char name[16] = {0};
	drm_version_t version = {};

	version.name_len = sizeof(name);
	version.name = name;

	if (drmIoctl(drm_fd, DRM_IOCTL_VERSION, &version))
	{
		return NULL;
	}

	for (struct gem_driver_lookup *di = drivers;
	     di - drivers < num_drivers();
	     ++di)
	{
		if (!strncmp(name, di->name, sizeof(name)))
		{
			return di->driver;
		}
	}

	return NULL;
}

int gem_size(int drm_fd, size_t *size, uint32_t gem_handle)
{
	struct drm_gem_flink drm_gem_flink_arg = {
		.handle = gem_handle,
		.name = 0
	};

	if (drmIoctl(drm_fd,
		     DRM_IOCTL_GEM_FLINK,
		     &drm_gem_flink_arg))
	{
		return -1;
	}

	struct drm_gem_open drm_gem_open_arg = {
		.name = drm_gem_flink_arg.name,
		.handle = 0,
		.size = 0
	};

	if (drmIoctl(drm_fd,
		     DRM_IOCTL_GEM_OPEN,
		     &drm_gem_open_arg))
	{
		return -1;
	}

	*size = drm_gem_open_arg.size;

	if (drm_gem_open_arg.handle != gem_handle)
	{
		gem_release_handle(drm_fd, drm_gem_open_arg.handle);
	}

	return 0;
}

void gem_release_handle(int drm_fd, uint32_t gem_handle)
{
	struct drm_gem_close drm_gem_close_arg = {
		.handle = gem_handle,
		.pad = 0
	};

	drmIoctl(drm_fd, DRM_IOCTL_GEM_CLOSE, &drm_gem_close_arg);
}

int drm_fb_for_gem_handle(int drm_fd, uint32_t *fb_id, uint32_t gem_handle,
			     const struct fb_configuration *fb_config)
{
	struct drm_mode_fb_cmd2 drm_mode_addfb2_arg = {
		.fb_id = 0,
		.width = fb_config->width,
		.height = fb_config->height,
		.pixel_format = fb_config->pixel_format,
		.flags = DRM_MODE_FB_MODIFIERS,
		.handles = { gem_handle, 0, 0, 0 },
		.pitches = { fb_config->width * fb_config->pixel_size, 0, 0, 0 },
		.offsets = { 0, 0, 0, 0 },
		.modifier = { 0 }
	};

	if (drmIoctl(drm_fd, DRM_IOCTL_MODE_ADDFB2, &drm_mode_addfb2_arg))
	{
		return -1;
	}

	*fb_id = drm_mode_addfb2_arg.fb_id;

	return 0;
}
