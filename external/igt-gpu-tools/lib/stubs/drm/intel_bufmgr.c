#include <stdbool.h>
#include <errno.h>

#include "igt_core.h"
#include "intel_bufmgr.h"

#ifdef __GNUC__
#pragma GCC push_options
#pragma GCC diagnostic ignored "-Wsuggest-attribute=noreturn"
#endif

static const char missing_support_str[] = "Not compiled with libdrm_intel support\n";

drm_intel_bufmgr *drm_intel_bufmgr_gem_init(int fd, int batch_size)
{
	igt_require_f(false, missing_support_str);
	return NULL;
}

void drm_intel_bo_unreference(drm_intel_bo *bo)
{
	igt_require_f(false, missing_support_str);
}

drm_intel_bo *drm_intel_bo_alloc(drm_intel_bufmgr *bufmgr, const char *name,
				 unsigned long size, unsigned int alignment)
{
	igt_require_f(false, missing_support_str);
	return NULL;
}

int drm_intel_bo_subdata(drm_intel_bo *bo, unsigned long offset,
			 unsigned long size, const void *data)
{
	igt_require_f(false, missing_support_str);
	return -ENODEV;
}

int drm_intel_gem_bo_context_exec(drm_intel_bo *bo, drm_intel_context *ctx,
				  int used, unsigned int flags)
{
	igt_require_f(false, missing_support_str);
	return -ENODEV;
}

int drm_intel_bo_emit_reloc(drm_intel_bo *bo, uint32_t offset,
				drm_intel_bo *target_bo, uint32_t target_offset,
				uint32_t read_domains, uint32_t write_domain)
{
	igt_require_f(false, missing_support_str);
	return -ENODEV;
}

int drm_intel_bo_emit_reloc_fence(drm_intel_bo *bo, uint32_t offset,
				  drm_intel_bo *target_bo,
				  uint32_t target_offset,
				  uint32_t read_domains, uint32_t write_domain)
{
	igt_require_f(false, missing_support_str);
	return -ENODEV;
}

int drm_intel_bo_get_tiling(drm_intel_bo *bo, uint32_t * tiling_mode,
			    uint32_t * swizzle_mode)
{
	igt_require_f(false, missing_support_str);
	return -ENODEV;
}

int drm_intel_bo_mrb_exec(drm_intel_bo *bo, int used,
			  struct drm_clip_rect *cliprects, int num_cliprects,
			  int DR4, unsigned int flags)
{
	igt_require_f(false, missing_support_str);
	return -ENODEV;
}

void drm_intel_bufmgr_gem_set_aub_annotations(drm_intel_bo *bo,
					      drm_intel_aub_annotation *annotations,
					      unsigned count)
{
	igt_require_f(false, missing_support_str);
}

void drm_intel_bufmgr_gem_enable_reuse(drm_intel_bufmgr *bufmgr)
{
	igt_require_f(false, missing_support_str);
}

int drm_intel_bo_exec(drm_intel_bo *bo, int used,
		      struct drm_clip_rect *cliprects, int num_cliprects, int DR4)
{
	igt_require_f(false, missing_support_str);
	return -ENODEV;
}

void drm_intel_bufmgr_destroy(drm_intel_bufmgr *bufmgr)
{
	igt_require_f(false, missing_support_str);
}

void drm_intel_bo_wait_rendering(drm_intel_bo *bo)
{
	igt_require_f(false, missing_support_str);
}

int drm_intel_bo_get_subdata(drm_intel_bo *bo, unsigned long offset,
			     unsigned long size, void *data)
{
	igt_require_f(false, missing_support_str);
	return -ENODEV;
}

int drm_intel_bo_map(drm_intel_bo *bo, int write_enable)
{
	igt_require_f(false, missing_support_str);
	return -ENODEV;
}

int drm_intel_gem_bo_map_gtt(drm_intel_bo *bo)
{
	igt_require_f(false, missing_support_str);
	return -ENODEV;
}

void drm_intel_bufmgr_gem_enable_fenced_relocs(drm_intel_bufmgr *bufmgr)
{
	igt_require_f(false, missing_support_str);
}

int drm_intel_bo_unmap(drm_intel_bo *bo)
{
	igt_require_f(false, missing_support_str);
	return -ENODEV;
}

int drm_intel_bo_flink(drm_intel_bo *bo, uint32_t * name)
{
	igt_require_f(false, missing_support_str);
	return -ENODEV;
}

drm_intel_bo *drm_intel_bo_gem_create_from_name(drm_intel_bufmgr *bufmgr,
						const char *name,
						unsigned int handle)
{
	igt_require_f(false, missing_support_str);
	return NULL;
}

int drm_intel_bo_gem_export_to_prime(drm_intel_bo *bo, int *prime_fd)
{
	igt_require_f(false, missing_support_str);
	return -ENODEV;
}

drm_intel_bo *drm_intel_bo_gem_create_from_prime(drm_intel_bufmgr *bufmgr,
						 int prime_fd, int size)
{
	igt_require_f(false, missing_support_str);
	return NULL;
}

void drm_intel_bufmgr_gem_set_vma_cache_size(drm_intel_bufmgr *bufmgr,
					     int limit)
{
	igt_require_f(false, missing_support_str);
}

int drm_intel_gem_bo_unmap_gtt(drm_intel_bo *bo)
{
	igt_require_f(false, missing_support_str);
	return -ENODEV;
}

drm_intel_context *drm_intel_gem_context_create(drm_intel_bufmgr *bufmgr)
{
	igt_require_f(false, missing_support_str);
	return NULL;
}

int drm_intel_gem_context_get_id(drm_intel_context *ctx,
                                 uint32_t *ctx_id)
{
	igt_require_f(false, missing_support_str);
	return -ENODEV;
}

void drm_intel_gem_context_destroy(drm_intel_context *ctx)
{
	igt_require_f(false, missing_support_str);
}

drm_intel_bo *drm_intel_bo_alloc_tiled(drm_intel_bufmgr *bufmgr,
				       const char *name,
				       int x, int y, int cpp,
				       uint32_t *tiling_mode,
				       unsigned long *pitch,
				       unsigned long flags)
{
	igt_require_f(false, missing_support_str);
	return NULL;
}

void drm_intel_bufmgr_gem_set_aub_filename(drm_intel_bufmgr *bufmgr,
					  const char *filename)
{
	igt_require_f(false, missing_support_str);
}

void drm_intel_bufmgr_gem_set_aub_dump(drm_intel_bufmgr *bufmgr, int enable)
{
	igt_require_f(false, missing_support_str);
}

void drm_intel_gem_bo_aub_dump_bmp(drm_intel_bo *bo,
				   int x1, int y1, int width, int height,
				   enum aub_dump_bmp_format format,
				   int pitch, int offset)
{
	igt_require_f(false, missing_support_str);
}

void drm_intel_gem_bo_start_gtt_access(drm_intel_bo *bo, int write_enable)
{
	igt_require_f(false, missing_support_str);
}

int drm_intel_bo_set_tiling(drm_intel_bo *bo, uint32_t * tiling_mode,
				uint32_t stride)
{
	igt_require_f(false, missing_support_str);
	return -ENODEV;
}

int drm_intel_bo_disable_reuse(drm_intel_bo *bo)
{
	igt_require_f(false, missing_support_str);
	return -ENODEV;
}

void drm_intel_bo_reference(drm_intel_bo *bo)
{
	igt_require_f(false, missing_support_str);
}

int drm_intel_bufmgr_gem_get_devid(drm_intel_bufmgr *bufmgr)
{
	igt_require_f(false, missing_support_str);
	return -ENODEV;
}

drm_intel_bo *drm_intel_bo_alloc_for_render(drm_intel_bufmgr *bufmgr,
					    const char *name,
					    unsigned long size,
					    unsigned int alignment)
{
	igt_require_f(false, missing_support_str);
	return NULL;
}

int drm_intel_bo_references(drm_intel_bo *bo, drm_intel_bo *target_bo)
{
	igt_require_f(false, missing_support_str);
	return -ENODEV;
}

int drm_intel_gem_bo_wait(drm_intel_bo *bo, int64_t timeout_ns)
{
	igt_require_f(false, missing_support_str);
	return -ENODEV;
}

drm_intel_bo *drm_intel_bo_alloc_userptr(drm_intel_bufmgr *bufmgr,
					 const char *name,
					 void *addr, uint32_t tiling_mode,
					 uint32_t stride, unsigned long size,
					 unsigned long flags)
{
	igt_require_f(false, missing_support_str);
	return NULL;
}

#ifdef __GNUC__
#pragma GCC pop_options
#endif
