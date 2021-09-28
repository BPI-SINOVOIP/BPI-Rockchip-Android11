// Copyright (c) 2021 by Rockchip Electronics Co., Ltd. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <dlfcn.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>

#include <cstdlib>
#include <android/log.h>

#include <sys/system_properties.h>

#include "NeuralNetworksTypes.h"
#include "../default/prebuilts/librknnrt/include/rknn_api.h"

#define TAG "RKNN_API"
#define NNAPI_LOG(...) __android_log_print(ANDROID_LOG_ERROR,TAG ,__VA_ARGS__)
// #define NNAPI_LOG(...)
/* To prevent unused parameter warnings */
#define UNUSED(x) (void)(x)

struct NnApi {
  bool nnapi_exists;
  int32_t android_sdk_version;

  int (*ARKNN_client_create)(ARKNNHAL **hal);
  int (*ARKNN_init)(ARKNNHAL *hal, rknn_context *context, void *model, uint32_t size, uint32_t flag);
  int (*ARKNN_destroy)(ARKNNHAL *hal, rknn_context);
  int (*ARKNN_query)(ARKNNHAL *hal, rknn_context context, rknn_query_cmd cmd, void* info, uint32_t size);
  int (*ARKNN_inputs_set)(ARKNNHAL *hal, rknn_context context, uint32_t n_inputs, rknn_input inputs[]);
  int (*ARKNN_run)(ARKNNHAL *hal, rknn_context context, rknn_run_extend* extend);
  int (*ARKNN_outputs_get)(ARKNNHAL *hal, rknn_context, uint32_t n_outputs, rknn_output outputs[], rknn_output_extend* extend);
  int (*ARKNN_outputs_release)(ARKNNHAL *hal, rknn_context context, uint32_t n_ouputs, rknn_output outputs[]);
  int (*ARKNN_destory_mem)(ARKNNHAL *hal, rknn_context context, rknn_tensor_mem *mem);
  rknn_tensor_mem * (*ARKNN_create_mem)(ARKNNHAL *hal, rknn_context context, uint32_t size);
  int (*ARKNN_set_io_mem)(ARKNNHAL *hal, rknn_context context, rknn_tensor_mem *mem, rknn_tensor_attr *attr);

  int (*ASharedMemory_create)(const char* name, size_t size);
};

static int32_t GetAndroidSdkVersion() {
  const char* sdkProp = "ro.build.version.sdk";
  char sdkVersion[PROP_VALUE_MAX];
  int length = __system_property_get(sdkProp, sdkVersion);
  if (length != 0) {
    int32_t result = 0;
    for (int i = 0; i < length; ++i) {
      int digit = sdkVersion[i] - '0';
      if (digit < 0 || digit > 9) {
        // Non-numeric SDK version, assume it's higher than expected;
        return 0xffff;
      }
      result = result * 10 + digit;
    }
    // TODO(levp): remove once SDK gets updated to 29th level
    // Upgrade SDK version for pre-release Q to be able to test functionality
    // available from SDK level 29.
    if (result == 28) {
      char versionCodename[PROP_VALUE_MAX];
      const char* versionCodenameProp = "ro.build.version.codename";
      length = __system_property_get(versionCodenameProp, versionCodename);
      if (length != 0) {
        if (versionCodename[0] == 'Q') {
          return 29;
        }
      }
    }
    return result;
  }
  return 0;
}

static void* LoadFunction(void* handle, const char* name, bool optional) {
  if (handle == nullptr) {
    return nullptr;
  }
  void* fn = dlsym(handle, name);
  if (fn == nullptr && !optional) {
    NNAPI_LOG("nnapi error: unable to open function %s", name);
  }
  return fn;
}

#define LOAD_FUNCTION(handle, name)         \
  nnapi.name = reinterpret_cast<name##_fn>( \
      LoadFunction(handle, #name, /*optional*/ false));

#define LOAD_FUNCTION_OPTIONAL(handle, name) \
  nnapi.name = reinterpret_cast<name##_fn>(  \
      LoadFunction(handle, #name, /*optional*/ true));

#define LOAD_FUNCTION_RENAME(handle, name, symbol) \
  nnapi.name = reinterpret_cast<name##_fn>(        \
      LoadFunction(handle, symbol, /*optional*/ false));

static const NnApi LoadNnApi() {
  NnApi nnapi = {};
  nnapi.android_sdk_version = 0;

  nnapi.android_sdk_version = GetAndroidSdkVersion();
  if (nnapi.android_sdk_version < 27) {
    NNAPI_LOG("nnapi error: requires android sdk version to be at least %d",
              27);
    nnapi.nnapi_exists = false;
    return nnapi;
  }

  void* librknnhal_bridge = nullptr;
  // TODO(b/123243014): change RTLD_LOCAL? Assumes there can be multiple
  // instances of nn api RT
  librknnhal_bridge = dlopen("librknnhal_bridge.rockchip.so", RTLD_LAZY | RTLD_LOCAL);
  if (librknnhal_bridge == nullptr) {
    NNAPI_LOG("nnapi error: unable to open library %s", "librknnhal_bridge.rockchip.so");
  }

  nnapi.nnapi_exists = librknnhal_bridge != nullptr;

  // API 27 (NN 1.0) methods.
  LOAD_FUNCTION(librknnhal_bridge, ARKNN_client_create);
  LOAD_FUNCTION(librknnhal_bridge, ARKNN_init);
  LOAD_FUNCTION(librknnhal_bridge, ARKNN_destroy);
  LOAD_FUNCTION(librknnhal_bridge, ARKNN_query);
  LOAD_FUNCTION(librknnhal_bridge, ARKNN_inputs_set);
  LOAD_FUNCTION(librknnhal_bridge, ARKNN_run);
  LOAD_FUNCTION(librknnhal_bridge, ARKNN_outputs_get);
  LOAD_FUNCTION(librknnhal_bridge, ARKNN_outputs_release);
  LOAD_FUNCTION(librknnhal_bridge, ARKNN_destory_mem);
  LOAD_FUNCTION(librknnhal_bridge, ARKNN_create_mem);
  LOAD_FUNCTION(librknnhal_bridge, ARKNN_set_io_mem);

  /* Not sure this func exist.
   * LOAD_FUNCTION_OPTIONAL(
      librknnhal_bridge,
      ANeuralNetworksModel_setOperandSymmPerChannelQuantParams);*/

  // ASharedMemory_create has different implementations in Android depending on
  // the partition. Generally it can be loaded from libandroid.so but in vendor
  // partition (e.g. if a HAL wants to use NNAPI) it is only accessible through
  // libcutils.
  void* libandroid = nullptr;
  libandroid = dlopen("libandroid.so", RTLD_LAZY | RTLD_LOCAL);
  if (libandroid != nullptr) {
    LOAD_FUNCTION(libandroid, ASharedMemory_create);
  } else {
    void* cutils_handle = dlopen("libcutils.so", RTLD_LAZY | RTLD_LOCAL);
    if (cutils_handle != nullptr) {
      LOAD_FUNCTION_RENAME(cutils_handle, ASharedMemory_create,
                           "ashmem_create_region");
    } else {
      NNAPI_LOG("nnapi error: unable to open neither libraries %s and %s",
                "libandroid.so", "libcutils.so");
    }
  }
  return nnapi;
}

static const NnApi* NnApiImplementation() {
  static const NnApi nnapi = LoadNnApi();
  return &nnapi;
}

typedef struct {
  ARKNNHAL *hal;
  rknn_context rknn_ctx;
} _rknn_context;

int rknn_init(rknn_context* context, void* model, uint32_t size, uint32_t flag, rknn_init_extend* extend) {
  int ret;
  const NnApi *_nnapi = NnApiImplementation();
  UNUSED(extend);

  ARKNNHAL *_hal;
  _nnapi->ARKNN_client_create(&_hal);
  if (!_hal) {
      NNAPI_LOG("Failed to create RKNN HAL Client!");
      return RKNN_ERR_DEVICE_UNAVAILABLE;
  }

  rknn_context _rknn_ctx;
  ret = _nnapi->ARKNN_init(_hal, &_rknn_ctx, model, size, flag);

  if (ret == RKNN_SUCC) {
    _rknn_context *_ctx = (_rknn_context *)malloc(sizeof(_rknn_context));
    _ctx->hal = _hal;
    _ctx->rknn_ctx = _rknn_ctx;
    *context = (rknn_context)_ctx;
  }

  return ret;
}

int rknn_destroy(rknn_context context) {
  _rknn_context *_ctx = (_rknn_context *)context;
  if (_ctx == NULL || _ctx->hal == NULL || _ctx->rknn_ctx == 0) {
    return RKNN_ERR_CTX_INVALID;
  }

  const NnApi *_nnapi = NnApiImplementation();

  int ret =  _nnapi->ARKNN_destroy(_ctx->hal, _ctx->rknn_ctx);
  free(_ctx);
  return ret;
}

int rknn_query(rknn_context context, rknn_query_cmd cmd, void* info, uint32_t size) {
  _rknn_context *_ctx = (_rknn_context *)context;
  if (_ctx == NULL || _ctx->hal == NULL || _ctx->rknn_ctx == 0) {
    return RKNN_ERR_CTX_INVALID;
  }

  const NnApi *_nnapi = NnApiImplementation();

  return _nnapi->ARKNN_query(_ctx->hal, _ctx->rknn_ctx, cmd, info, size);
}

int rknn_inputs_set(rknn_context context, uint32_t n_inputs, rknn_input inputs[]) {
  _rknn_context *_ctx = (_rknn_context *)context;
  if (_ctx == NULL || _ctx->hal == NULL || _ctx->rknn_ctx == 0) {
    return RKNN_ERR_CTX_INVALID;
  }

  const NnApi *_nnapi = NnApiImplementation();

  return _nnapi->ARKNN_inputs_set(_ctx->hal, _ctx->rknn_ctx, n_inputs, inputs);
}

int rknn_run(rknn_context context, rknn_run_extend* extend) {
  _rknn_context *_ctx = (_rknn_context *)context;
  if (_ctx == NULL || _ctx->hal == NULL || _ctx->rknn_ctx == 0) {
    return RKNN_ERR_CTX_INVALID;
  }

  const NnApi *_nnapi = NnApiImplementation();

  return _nnapi->ARKNN_run(_ctx->hal, _ctx->rknn_ctx, extend);
}

int rknn_outputs_get(rknn_context context, uint32_t n_outputs, rknn_output outputs[], rknn_output_extend* extend) {
  _rknn_context *_ctx = (_rknn_context *)context;
  if (_ctx == NULL || _ctx->hal == NULL || _ctx->rknn_ctx == 0) {
    return RKNN_ERR_CTX_INVALID;
  }

  const NnApi *_nnapi = NnApiImplementation();

  return _nnapi->ARKNN_outputs_get(_ctx->hal, _ctx->rknn_ctx, n_outputs, outputs, extend);
}

int rknn_outputs_release(rknn_context context, uint32_t n_ouputs, rknn_output outputs[]) {
  _rknn_context *_ctx = (_rknn_context *)context;
  if (_ctx == NULL || _ctx->hal == NULL || _ctx->rknn_ctx == 0) {
    return RKNN_ERR_CTX_INVALID;
  }

  const NnApi *_nnapi = NnApiImplementation();

  return _nnapi->ARKNN_outputs_release(_ctx->hal, _ctx->rknn_ctx, n_ouputs, outputs);
}

int rknn_wait(rknn_context context, rknn_run_extend* extend) {
  UNUSED(context);
  UNUSED(extend);

  NNAPI_LOG("No Implement rknn_wait on Android HIDL RKNN API!");
  return RKNN_ERR_FAIL;
}

rknn_tensor_mem* rknn_create_mem_from_phys(rknn_context ctx, uint64_t phys_addr, void *virt_addr, uint32_t size) {
  UNUSED(ctx);
  UNUSED(phys_addr);
  UNUSED(virt_addr);
  UNUSED(size);

  NNAPI_LOG("No Implement rknn_create_mem_from_phys on Android HIDL RKNN API!");
  return nullptr;
}

rknn_tensor_mem* rknn_create_mem_from_fd(rknn_context ctx, int32_t fd, void *virt_addr, uint32_t size, int32_t offset) {
  if (ctx == 0) {
    return NULL;
  }

  rknn_tensor_mem* mem = (rknn_tensor_mem*)malloc(sizeof(rknn_tensor_mem));
  memset(mem, 0, sizeof(rknn_tensor_mem));
  mem->virt_addr = virt_addr;
  mem->phys_addr = -1;
  mem->fd        = fd;
  mem->offset    = offset;
  mem->size      = size;

  return mem;
}

rknn_tensor_mem* rknn_create_mem_from_mb_blk(rknn_context ctx, void *mb_blk, int32_t offset) {
  UNUSED(ctx);
  UNUSED(mb_blk);
  UNUSED(offset);

  NNAPI_LOG("No Implement rknn_create_mem_from_mb_blk on Android HIDL RKNN API!");
  return nullptr;
}

rknn_tensor_mem* rknn_create_mem(rknn_context context, uint32_t size) {
  _rknn_context *_ctx = (_rknn_context *)context;
  rknn_tensor_mem *mem = nullptr;

  if (_ctx == NULL || _ctx->hal == NULL || _ctx->rknn_ctx == 0) {
    return nullptr;
  }

  const NnApi *_nnapi = NnApiImplementation();

  mem = _nnapi->ARKNN_create_mem(_ctx->hal, _ctx->rknn_ctx, size);
  return mem;
}

int rknn_destory_mem(rknn_context context, rknn_tensor_mem *mem) {
  _rknn_context *_ctx = (_rknn_context *)context;
  if (_ctx == NULL || _ctx->hal == NULL || _ctx->rknn_ctx == 0 || (mem == NULL)) {
    return RKNN_ERR_CTX_INVALID;
  }

  const NnApi *_nnapi = NnApiImplementation();

  _nnapi->ARKNN_destory_mem(_ctx->hal, _ctx->rknn_ctx, mem);

  free(mem);

  return 0;
}

int rknn_set_weight_mem(rknn_context ctx, rknn_tensor_mem *mem) {
  UNUSED(ctx);
  UNUSED(mem);
  NNAPI_LOG("No Implement rknn_set_weight_mem on Android HIDL RKNN API!");
  return RKNN_ERR_FAIL;
}

int rknn_set_internal_mem(rknn_context ctx, rknn_tensor_mem *mem) {
  UNUSED(ctx);
  UNUSED(mem);
  NNAPI_LOG("No Implement rknn_set_internal_mem on Android HIDL RKNN API!");
  return RKNN_ERR_FAIL;
}

int rknn_set_io_mem(rknn_context context, rknn_tensor_mem *mem, rknn_tensor_attr *attr) {
  _rknn_context *_ctx = (_rknn_context *)context;
  if (_ctx == NULL || _ctx->hal == NULL || _ctx->rknn_ctx == 0 || (mem == NULL) || (attr == NULL)) {
    return RKNN_ERR_CTX_INVALID;
  }

  if (mem->fd < 0) {
    NNAPI_LOG("rknn_set_io_mem not support rknn_tensor_mem::fd < 0 on Android HIDL RKNN API!");
    return RKNN_ERR_PARAM_INVALID;
  }

  const NnApi *_nnapi = NnApiImplementation();

  int ret = _nnapi->ARKNN_set_io_mem(_ctx->hal, _ctx->rknn_ctx, mem, attr);

  return ret;
}
