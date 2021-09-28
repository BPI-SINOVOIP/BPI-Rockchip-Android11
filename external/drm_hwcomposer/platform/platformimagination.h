#ifndef PLATFORMIMAGINATION_H
#define PLATFORMIMAGINATION_H

#include "drmdevice.h"
#include "platform.h"
#include "platformdrmgeneric.h"

#include <stdatomic.h>

#include <hardware/gralloc.h>

namespace android {

class ImaginationImporter : public DrmGenericImporter {
 public:
  using DrmGenericImporter::DrmGenericImporter;

  int ConvertBoInfo(buffer_handle_t handle, hwc_drm_bo_t *bo) override;
};
}  // namespace android

#endif  // PLATFORMIMAGINATION_H
