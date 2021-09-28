#include "RKISP2FecUnit.h"

#include <dlfcn.h>
#include <stdio.h>
#include <sync/sync.h>

#include <string>
#include <ui/GraphicBuffer.h>

#include "LogHelper.h"

namespace android {
namespace camera2 {
namespace rkisp2 {

#define ALIGN(value, align) ((value + align -1) & ~(align-1))
#if 0
#if defined(__LP64__)
#define RK_XXX_PATH "/vendor/lib64/libdistortion_gl.so"
#else
#define RK_XXX_PATH "/vendor/lib/libdistortion_gl.so"
#endif
#else
#define RK_XXX_PATH "/vendor/lib/libdistortion_gl.so"
#endif
#if 0
RKISP2FecUnit *RKISP2FecUnit::mInstance = nullptr;

RKISP2FecUnit *RKISP2FecUnit::getInstance() {
  if (mInstance == nullptr) {
    mInstance = new RKISP2FecUnit();
  }
  return mInstance;
}
#endif

RKISP2FecUnit::RKISP2FecUnit()
    : createFenceFd(nullptr), dso(nullptr), done_init(0),
      distortionByGpuInit(nullptr), distortionByGpuDeinit(nullptr),
      distortionByGpuProcess(nullptr), distortionSyncFenceFd(nullptr) {
  loadDistortionGlLibray();
}

RKISP2FecUnit::~RKISP2FecUnit() {
  distortionDeinit();
  if (dso)
    dlclose(dso);
#if 0
  if (mInstance != nullptr) {
    delete (mInstance);
    mInstance = nullptr;
  }
#endif
}

void RKISP2FecUnit::loadDistortionGlLibray() {
  property_set("vendor.gl.distorfile", "/vendor/etc/meshXY.bin");
  if (dso == NULL) {
    dso = dlopen(RK_XXX_PATH, RTLD_NOW | RTLD_LOCAL);
  }
  LOGE("rk-debug[%s %d] android_load_sphal_library dso=%p", __FUNCTION__,
      RK_XXX_PATH, __LINE__, dso);
  if (dso == 0) {
    LOGE("rk-debug can't not find %s ! error=%s ",
         dlerror());
  }
  createGLClass = NULL;
  if (createGLClass == NULL)
    createGLClass = (__createGLClass)dlsym(dso, "createGLContext");
  if (createGLClass == NULL) {
    LOGE("rk_debug can't not find CreateGLClass function in "
         "/system/lib64/libbicubic_gl.so !");
    goto err;
  }
  if (distortionByGpuInit == NULL)
    distortionByGpuInit =
        (__distortionByGpuInit)dlsym(dso, "distortionByGpuInit");
  if (distortionByGpuInit == NULL) {
    LOGE("rk_debug can't not find distortionByGpuInit function in "
         "/system/lib64/libbicubic_gl.so !");
    goto err;
  }
  
  if (distortionByGpuProcess == NULL)
    distortionByGpuProcess =
        (__distortionByGpuProcess)dlsym(dso, "distortionByGpuProcess");
  if (distortionByGpuProcess == NULL) {
    LOGE("rk_debug can't not find distortionByGpuProcess function in "
         "/system/lib64/libbicubic_gl.so !");
    goto err;
  }
  if (distortionByGpuDeinit == NULL)
    distortionByGpuDeinit =
        (__distortionByGpuDeinit)dlsym(dso, "distortionByGpuDeinit");
  if (distortionByGpuDeinit == NULL) {
    LOGE("rk_debug can't not find distortionByGpuDeinit function in "
         "/system/lib64/libbicubic_gl.so !");
    goto err;
  }

  if (createFenceFd == NULL)
    createFenceFd = (__createFenceFd)dlsym(dso, "createFencefd");
  if (createFenceFd == NULL) {
    LOGE("rk_debug can't not find createFencefd function in "
         "/system/lib64/libbicubic_gl.so !");
    goto err;
  }
  return;
err:
  dlclose(dso);
}

void RKISP2FecUnit::calculateMeshGridSize(int width, int height, int &meshW, int& meshH) {
  int meshStepW, meshStepH;
  int alignW = ALIGN(width, 32);
  int alignH = ALIGN(height, 32);

  if (width <= 1920) {
    meshStepW = 16;
    meshStepH = 8;
  } else {
    meshStepW = 32;
    meshStepH = 16;
  }

  meshW = (alignW + meshStepW -1) / meshStepW + 1;
  meshH = (alignH + meshStepH -1) / meshStepH + 1;
  LOGE("meshW=%d meshH=%d  alignw=%d alignh=%d", meshW, meshH, alignW, alignH);
}

int RKISP2FecUnit::distortionInit(int width, int height) {
  std::unique_lock<std::mutex> lk(mtx);
  int success = 0;
  if (!done_init && distortionByGpuInit) {
    uint32_t w = (width&0xffff)<<16|3840;
    uint32_t h = (height&0xffff)<<16|2160;
    int meshGridW=0, meshGridH=0;

    done_init = 1;
    calculateMeshGridSize(width, height, meshGridW, meshGridH);
    glClass = createGLClass();
    if (glClass != nullptr) {
      success = distortionByGpuInit(glClass, w, h, meshGridW, meshGridH);
      LOGE("rk-debug: glclass = %p meshGridW=%d meshGridH=%d", glClass, meshGridW, meshGridH);
    } else {
      success = -1;
    }
  }
  return success;
}

int RKISP2FecUnit::distortionDeinit() {
  std::unique_lock<std::mutex> lk(mtx);
  int success = 0;
  if (distortionByGpuDeinit && done_init)
    success = distortionByGpuDeinit(glClass);
  done_init = 0;
  return success;
}

int RKISP2FecUnit::doFecProcess(int inW, int inH, int inFd, int outW, int outH, int outFd, int& fenceFd) {
  if (distortionByGpuProcess) {
    int gpu_fence_fd = -1;
    distortionByGpuProcess(glClass, inFd, inW, inH, outFd, outW, outH);
    #if 0
    if (fenceFd > 0) {
        int retireFenceFd = sync_merge("fec_and_camera", fenceFd, gpu_fence_fd);
        close(gpu_fence_fd);
        close(fenceFd);
        fenceFd = retireFenceFd
    }
    #endif
    gpu_fence_fd = createFenceFd(glClass);
    sync_wait(gpu_fence_fd, 1000);
    close(gpu_fence_fd);
  }
  return 0;
}

} // namespace rkisp2
} // namespace camera2
} // namespace android
