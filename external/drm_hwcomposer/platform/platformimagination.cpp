#define LOG_TAG "hwc-platform-imagination"

#include "platformimagination.h"
#include <log/log.h>
#include <xf86drm.h>

#include "img_gralloc1_public.h"

namespace android {

Importer *Importer::CreateInstance(DrmDevice *drm) {
  ImaginationImporter *importer = new ImaginationImporter(drm);
  if (!importer)
    return NULL;

  int ret = importer->Init();
  if (ret) {
    ALOGE("Failed to initialize the Imagination importer %d", ret);
    delete importer;
    return NULL;
  }
  return importer;
}

int ImaginationImporter::ConvertBoInfo(buffer_handle_t handle,
                                       hwc_drm_bo_t *bo) {
  IMG_native_handle_t *hnd = (IMG_native_handle_t *)handle;
  if (!hnd)
    return -EINVAL;

  /* Extra bits are responsible for buffer compression and memory layout */
  if (hnd->iFormat & ~0x10f) {
    ALOGV("Special buffer formats are not supported");
    return -EINVAL;
  }

  bo->width = hnd->iWidth;
  bo->height = hnd->iHeight;
  bo->usage = hnd->usage;
  bo->prime_fds[0] = hnd->fd[0];
  bo->pitches[0] = ALIGN(hnd->iWidth, HW_ALIGN) * hnd->uiBpp >> 3;

  switch (hnd->iFormat) {
#ifdef HAL_PIXEL_FORMAT_BGRX_8888
    case HAL_PIXEL_FORMAT_BGRX_8888:
      bo->format = DRM_FORMAT_XRGB8888;
      break;
#endif
    default:
      bo->format = ConvertHalFormatToDrm(hnd->iFormat & 0xf);
      if (bo->format == DRM_FORMAT_INVALID) {
        ALOGV("Cannot convert hal format to drm format %u", hnd->iFormat);
        return -EINVAL;
      }
  }

  return 0;
}

std::unique_ptr<Planner> Planner::CreateInstance(DrmDevice *) {
  std::unique_ptr<Planner> planner(new Planner);
  planner->AddStage<PlanStageGreedy>();
  return planner;
}
}  // namespace android
