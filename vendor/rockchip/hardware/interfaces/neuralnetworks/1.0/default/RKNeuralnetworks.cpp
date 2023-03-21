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

#include "RKNeuralnetworks.h"
#include "utils.h"
#include <sys/mman.h>

#ifndef IMPL_RKNN
#define IMPL_RKNN 0
#else
#define IMPL_RKNN 1
#endif

#if IMPL_RKNN
#include "prebuilts/librknnrt/include/rknn_api.h"
#endif

#include <android/hidl/memory/1.0/IMemory.h>
#include <hidlmemory/mapping.h>

using ::android::hidl::memory::V1_0::IMemory;
using ::android::hardware::hidl_memory;
using namespace std;

namespace rockchip::hardware::neuralnetworks::implementation {

V1_0::ErrorStatus toErrorStatus(int ret) {
    return (V1_0::ErrorStatus)ret;
}

static rknn_query_cmd to_rknnapi(V1_0::RKNNQueryCmd cmd) {
    switch (cmd) {
        case V1_0::RKNNQueryCmd::RKNN_QUERY_IN_OUT_NUM:
            return RKNN_QUERY_IN_OUT_NUM;
        case V1_0::RKNNQueryCmd::RKNN_QUERY_INPUT_ATTR:
            return RKNN_QUERY_INPUT_ATTR;
        case V1_0::RKNNQueryCmd::RKNN_QUERY_OUTPUT_ATTR:
            return RKNN_QUERY_OUTPUT_ATTR;
        case V1_0::RKNNQueryCmd::RKNN_QUERY_PERF_DETAIL:
            return RKNN_QUERY_PERF_DETAIL;
        case V1_0::RKNNQueryCmd::RKNN_QUERY_PERF_RUN:
            return::RKNN_QUERY_PERF_RUN;
        case V1_0::RKNNQueryCmd::RKNN_QUERY_SDK_VERSION:
            return RKNN_QUERY_SDK_VERSION;
        case V1_0::RKNNQueryCmd::RKNN_QUERY_MEM_SIZE:
            return RKNN_QUERY_MEM_SIZE;
        case V1_0::RKNNQueryCmd::RKNN_QUERY_CUSTOM_STRING:
            return RKNN_QUERY_CUSTOM_STRING;
        default:
            return RKNN_QUERY_CMD_MAX;
    }
}

static rknn_tensor_type to_rknnapi(V1_0::RKNNTensorType type) {
    switch (type) {
        case V1_0::RKNNTensorType::RKNN_TENSOR_FLOAT32:
            return RKNN_TENSOR_FLOAT32;
        case V1_0::RKNNTensorType::RKNN_TENSOR_FLOAT16:
            return RKNN_TENSOR_FLOAT16;
        case V1_0::RKNNTensorType::RKNN_TENSOR_INT8:
            return RKNN_TENSOR_INT8;
        case V1_0::RKNNTensorType::RKNN_TENSOR_UINT8:
            return RKNN_TENSOR_UINT8;
        case V1_0::RKNNTensorType::RKNN_TENSOR_INT16:
            return RKNN_TENSOR_INT16;
        default:
            return RKNN_TENSOR_TYPE_MAX;
    }
}

static rknn_tensor_format to_rknnapi(V1_0::RKNNTensorFormat format) {
    switch (format) {
        case V1_0::RKNNTensorFormat::RKNN_TENSOR_NCHW:
            return RKNN_TENSOR_NCHW;
        case V1_0::RKNNTensorFormat::RKNN_TENSOR_NHWC:
            return RKNN_TENSOR_NHWC;
        default:
            return RKNN_TENSOR_FORMAT_MAX;
    }
}

static int dma_map(int fd, uint32_t length, int prot, int flags, off_t offset, void **ptr) {
    static uint32_t pagesize_mask = 0;
    if (ptr == NULL)
    return -EINVAL;

    if (!pagesize_mask)
        pagesize_mask = sysconf(_SC_PAGESIZE) - 1;

    offset = offset & (~pagesize_mask);

    *ptr = mmap(NULL, length, prot, flags, fd, offset);
    if (*ptr == MAP_FAILED) {
        ALOGE("fail to mmap(fd = %d), error:%s", fd, strerror(errno));
        *ptr = NULL;
        return -errno;
    }
    return 0;
}

// Methods from ::rockchip::hardware::neuralnetworks::V1_0::IRKNeuralnetworks follow.
Return<void> RKNeuralnetworks::rknnInit(const ::rockchip::hardware::neuralnetworks::V1_0::RKNNModel& model, uint32_t size, uint32_t flag, rknnInit_cb _hidl_cb) {
    RECORD_TAG();
    g_debug_pro = property_get_bool("persist.vendor.rknndebug", false);
    sp<IMemory> pMem = mapMemory(model.modelData);
    void *pData = pMem->getPointer();
    
#if IMPL_RKNN
    ret = rknn_init(&ctx, pData, size, flag, nullptr);
#else
    ALOGI("%s: %s", __func__, (char *)pData);
#endif
    _hidl_cb(toErrorStatus(ret), ctx);
    return Void();
}

Return<::rockchip::hardware::neuralnetworks::V1_0::ErrorStatus> RKNeuralnetworks::rknnDestory(uint64_t context) {
    RECORD_TAG();
#if IMPL_RKNN
    //CheckContext();
    
    ret = rknn_destroy(context);
#endif
    return ::rockchip::hardware::neuralnetworks::V1_0::ErrorStatus {toErrorStatus(ret)};
}

Return<::rockchip::hardware::neuralnetworks::V1_0::ErrorStatus> RKNeuralnetworks::rknnQuery(uint64_t context, ::rockchip::hardware::neuralnetworks::V1_0::RKNNQueryCmd cmd, const hidl_memory& info, uint32_t size) {
    RECORD_TAG();
#if IMPL_RKNN
    //CheckContext();
    sp<IMemory> pMem = mapMemory(info);
    void *pData = pMem->getPointer();
    pMem->update();
    // ALOGI("%s:len=%d, cmd=%d, rknnapi=%d", __func__, size, cmd, to_rknnapi(cmd));
    ret = rknn_query(context, to_rknnapi(cmd), pData, size);
    pMem->commit();
#endif
    return ::rockchip::hardware::neuralnetworks::V1_0::ErrorStatus {toErrorStatus(ret)};
}

Return<::rockchip::hardware::neuralnetworks::V1_0::ErrorStatus> RKNeuralnetworks::rknnInputsSet(uint64_t context, const ::rockchip::hardware::neuralnetworks::V1_0::Request& request) {
    RECORD_TAG();
#if IMPL_RKNN
    sp<IMemory> pMem = mapMemory(request.pool);
    pMem->update();
    void *pData = pMem->getPointer();

    rknn_input temp_inputs[request.n_inputs];
    for (int i = 0; i < request.n_inputs; i++) {
        temp_inputs[i].index = request.inputs[i].index;
        temp_inputs[i].buf = request.inputs[i].buf.offset + (char *)pData;
        temp_inputs[i].size = request.inputs[i].buf.length;
        temp_inputs[i].pass_through = request.inputs[i].pass_through;
        temp_inputs[i].type = to_rknnapi(request.inputs[i].type);
        temp_inputs[i].fmt = to_rknnapi(request.inputs[i].fmt);
    }
    ret = rknn_inputs_set(context, request.n_inputs, temp_inputs);
    pMem->commit();
#endif
    return ::rockchip::hardware::neuralnetworks::V1_0::ErrorStatus {toErrorStatus(ret)};
}

// debug only
static int rknn_GetTopN(float* pfProb, float* pfMaxProb, uint32_t* pMaxClass, uint32_t outputCount, uint32_t topNum)
{
  uint32_t i, j;
  uint32_t top_count = outputCount > topNum ? topNum : outputCount;

  for (i = 0; i < topNum; ++i) {
    pfMaxProb[i] = -FLT_MAX;
    pMaxClass[i] = -1;
  }

  for (j = 0; j < top_count; j++) {
    for (i = 0; i < outputCount; i++) {
      if ((i == *(pMaxClass + 0)) || (i == *(pMaxClass + 1)) || (i == *(pMaxClass + 2)) || (i == *(pMaxClass + 3)) ||
          (i == *(pMaxClass + 4))) {
        continue;
      }

      if (pfProb[i] > *(pfMaxProb + j)) {
        *(pfMaxProb + j) = pfProb[i];
        *(pMaxClass + j) = i;
      }
    }
  }

  return 1;
}

// debug only
static int dump_data(char *filename, const char* data, int sz) {

    FILE *fp = NULL;

    fp = fopen(filename, "wb+");
    if (fp != NULL)
    {
        ALOGI("dump %s", filename);
        fwrite(data, sz, 1, fp);
        fclose(fp);
        return 0;
    }

    ALOGD("write %s error:%s\n", filename, strerror(errno));
    return -1;
}

Return<::rockchip::hardware::neuralnetworks::V1_0::ErrorStatus> RKNeuralnetworks::rknnRun(uint64_t context, const ::rockchip::hardware::neuralnetworks::V1_0::RKNNRunExtend& extend) {
    RECORD_TAG();
#if IMPL_RKNN
    rknn_run_extend temp_extend = {
        .frame_id = extend.frame_id,
    };
    ret = rknn_run(context, &temp_extend);

    // std::map<uint64_t, void *>::iterator iter;  
  
    // ALOGI("rknnRun: ========>");

    // for(iter = m_TempTensorMap.begin(); iter != m_TempTensorMap.end(); iter++) {
    //     rknn_tensor_mem * mem =  (rknn_tensor_mem *)iter->second;
    //     ALOGI("rknnRun: mem addr=%p, len=%d", mem->virt_addr, mem->size);

    //     if (mem->size == 4000) {
    //         int topNum = 5;
    //         uint32_t MaxClass[topNum];
    //         float    fMaxProb[topNum];
    //         float*   buffer    = (float*)mem->virt_addr;
    //         uint32_t sz        = 1001;
    //         int      top_count = sz > topNum ? topNum : sz;

    //         rknn_GetTopN(buffer, fMaxProb, MaxClass, sz, topNum);

    //         ALOGI("---- Top%d ----\n", top_count);
    //         for (int j = 0; j < top_count; j++) {
    //         ALOGI("%8.6f - %d\n", fMaxProb[j], MaxClass[j]);
    //         }
    //     } else {
    //         static int cnt = 0;
    //         char filename[1024];
    //         memset(filename, 0x00, sizeof(filename));

    //         sprintf(filename, "/data/zht/%d.RGB", cnt++);

    //         dump_data(filename, ( const char*)mem->virt_addr, mem->size);
    //     }
    // }  
#endif
    return ::rockchip::hardware::neuralnetworks::V1_0::ErrorStatus {toErrorStatus(ret)};
}

Return<::rockchip::hardware::neuralnetworks::V1_0::ErrorStatus> RKNeuralnetworks::rknnOutputsGet(uint64_t context, const ::rockchip::hardware::neuralnetworks::V1_0::Response& response, const ::rockchip::hardware::neuralnetworks::V1_0::RKNNOutputExtend& extend) {
    RECORD_TAG();
#if IMPL_RKNN
    //CheckContext();
    sp<IMemory> pMem = mapMemory(response.pool);
    pMem->update();
    void *pData = pMem->getPointer();

    rknn_output pOutputs[response.n_outputs];
    for (int i = 0; i < response.n_outputs; i++) {
        pOutputs[i].want_float = response.outputs[i].want_float;
        pOutputs[i].is_prealloc = response.outputs[i].is_prealloc;
        pOutputs[i].buf = response.outputs[i].buf.offset + (char *)pData;
        pOutputs[i].size = response.outputs[i].buf.length;
        pOutputs[i].index = i;
    }
    rknn_output_extend temp_extend = {
        .frame_id = extend.frame_id,
    };
    ret = rknn_outputs_get(context, response.n_outputs, pOutputs, &temp_extend);
    // commit to response.
    pMem->commit();
#endif
    return ::rockchip::hardware::neuralnetworks::V1_0::ErrorStatus {toErrorStatus(ret)};
}

Return<::rockchip::hardware::neuralnetworks::V1_0::ErrorStatus> RKNeuralnetworks::rknnOutputsRelease(uint64_t context, const ::rockchip::hardware::neuralnetworks::V1_0::Response& response) {
    RECORD_TAG();

    UNUSED(context);

#if IMPL_RKNN
    //CheckContext();
    sp<IMemory> pMem = mapMemory(response.pool);
    pMem->update();
    void *pData = pMem->getPointer();

    rknn_output pOutputs[response.n_outputs];
    for (int i = 0; i < response.n_outputs; i++) {
        pOutputs[i].want_float = response.outputs[i].want_float;
        pOutputs[i].is_prealloc = response.outputs[i].is_prealloc;
        pOutputs[i].buf = response.outputs[i].buf.offset + (char *)pData;
        pOutputs[i].size = response.outputs[i].buf.length;
        pOutputs[i].index = i;
    }
    ret = rknn_outputs_release(context, response.n_outputs, pOutputs);
    pMem->commit();
#endif
    return ::rockchip::hardware::neuralnetworks::V1_0::ErrorStatus {toErrorStatus(ret)};
}

Return<::rockchip::hardware::neuralnetworks::V1_0::ErrorStatus> RKNeuralnetworks::rknnDestoryMemory(uint64_t context, const ::rockchip::hardware::neuralnetworks::V1_0::RKNNTensorMemory& bridge_mem) {
    rknn_tensor_mem * mem = nullptr;

    std::map<uint64_t, void *>::iterator iter;  
    for(iter = m_TempTensorMap.begin(); iter != m_TempTensorMap.end(); iter++) {
        if (bridge_mem.bridge_uuid == (uint64_t)iter->first) {
            mem =  (rknn_tensor_mem *)iter->second;   
            m_TempTensorMap.erase(iter);
            break;
        }
    }

    // #ifdef __arm__
    //     ALOGE("rknnDestoryMemory: bridge_mem.bridge_uuid:0x%llx", bridge_mem.bridge_uuid);
    // #else
    //     ALOGE("rknnDestoryMemory: bridge_mem.bridge_uuid:0x%lx", bridge_mem.bridge_uuid);
    // #endif

    if (mem != nullptr) {
        if (mem->flags & RKNN_TENSOR_MEMORY_FLAGS_ALLOC_INSIDE) {
            rknn_destroy_mem(context, mem);
            mem = nullptr;
        } else {
            rknn_destroy_mem(context, mem);
            if (mem->fd >= 0) {
                close(mem->fd);
            }

            free(mem);
            mem = nullptr;
        }
    }

    return ::rockchip::hardware::neuralnetworks::V1_0::ErrorStatus {};
}



Return<::rockchip::hardware::neuralnetworks::V1_0::ErrorStatus> RKNeuralnetworks::rknnSetIOMem(uint64_t context, const ::rockchip::hardware::neuralnetworks::V1_0::RKNNTensorMemory& bridge_mem, const ::rockchip::hardware::neuralnetworks::V1_0::RKNNTensorAttr& bridge_attr) {
    RECORD_TAG();

    rknn_tensor_mem * mem = nullptr;
    rknn_tensor_attr attr;

    if (sizeof(rknn_tensor_attr) != sizeof(V1_0::RKNNTensorAttr)) {
        ALOGE("sizeof(rknn_tensor_attr) != sizeof(RKNNTensorAttr)");
        return ::rockchip::hardware::neuralnetworks::V1_0::ErrorStatus {RKNN_ERR_PARAM_INVALID};
    }

    memcpy(&attr, &bridge_attr, sizeof(rknn_tensor_attr));

    if (bridge_mem.bridge_uuid != 0) {
        std::map<uint64_t, void *>::iterator iter;  
        for(iter = m_TempTensorMap.begin(); iter != m_TempTensorMap.end(); iter++) {
            if (bridge_mem.bridge_uuid == (uint64_t)iter->first) {
                mem =  (rknn_tensor_mem *)iter->second;                
                break;
            }
        }
    }

    if ((bridge_mem.bridge_uuid != 0) && (mem == nullptr)) {
#ifdef __arm__
        ALOGE("rknn_set_io_mem: invalid bridge_mem.bridge_uuid:0x%llx", bridge_mem.bridge_uuid);
#else
        ALOGE("rknn_set_io_mem: invalid bridge_mem.bridge_uuid:0x%lx", bridge_mem.bridge_uuid);
#endif
        return ::rockchip::hardware::neuralnetworks::V1_0::ErrorStatus {toErrorStatus(RKNN_ERR_PARAM_INVALID)};
    }

    if (mem == nullptr) {
        mem = (rknn_tensor_mem *)malloc(sizeof(rknn_tensor_mem));
        mem->virt_addr = nullptr;
        mem->phys_addr = bridge_mem.phys_addr;
        mem->offset = bridge_mem.offset;
        mem->size = bridge_mem.size;
        mem->flags = bridge_mem.flags;
        mem->priv_data = (void *)bridge_mem.priv_data;

        const native_handle_t* hnd = bridge_mem.bufferHdl.getNativeHandle();
        mem->fd = dup(hnd->data[0]);

        if (dma_map(mem->fd, mem->size, PROT_READ | PROT_WRITE, MAP_SHARED, 0, &mem->virt_addr) < 0) {
            ALOGE("rknn_create_mem: dma_map failed!");
        }

        m_TempTensorMap.insert(pair<uint64_t, void*>((uint64_t)mem, mem));
    }

    ret = rknn_set_io_mem(context, mem, &attr);

    return ::rockchip::hardware::neuralnetworks::V1_0::ErrorStatus {toErrorStatus(ret)};
}

Return<void> RKNeuralnetworks::rknnCreateMem(uint64_t context, uint32_t size, rknnCreateMem_cb _hidl_cb) {
    RECORD_TAG();

    V1_0::RKNNTensorMemory respose_mem;
    android::hardware::hidl_handle handle;

    memset(&respose_mem, 0x00, sizeof(V1_0::RKNNTensorMemory));

#if IMPL_RKNN

    rknn_tensor_mem* mem = rknn_create_mem(context, size);

    if (mem != nullptr) {
        respose_mem.virt_addr = (uint64_t)mem->virt_addr;
        respose_mem.phys_addr = mem->phys_addr;
        respose_mem.offset = mem->offset;
        respose_mem.size = mem->size;
        respose_mem.flags = mem->flags;
        respose_mem.priv_data = (uint64_t)mem->priv_data;
        respose_mem.bridge_uuid = (uint64_t)mem;

        native_handle_t* nativeHandle = native_handle_create(/*numFd*/ 1, 0);
        if(mem->fd >= 0){
            nativeHandle->data[0] = dup(mem->fd);
            
            handle.setTo(nativeHandle, /*shouldOwn=*/true);
            respose_mem.bufferHdl = handle;
        } else {
            native_handle_delete(nativeHandle);
        }

        m_TempTensorMap.insert(pair<uint64_t, void*>((uint64_t)mem, mem));      
    }

#endif
    _hidl_cb(toErrorStatus(ret), respose_mem);
    return Void();
}

Return<void> RKNeuralnetworks::registerCallback(const sp<::rockchip::hardware::neuralnetworks::V1_0::ILoadModelCallback>& loadCallback, const sp<::rockchip::hardware::neuralnetworks::V1_0::IGetResultCallback>& getCallback) {
    RECORD_TAG();
    UNUSED(ret);
#if IMPL_RKNN
    if (loadCallback != nullptr) {
        ALOGI("Register LoadCallback Successfully!");
        // TODO set callback
        //_load_cb = loadCallback;
    } else {
        ALOGE("Register LoadCallback Failed!");
    }
    if (getCallback != nullptr) {
        ALOGI("Register GetCallback Successfully!");
        // TODO set callback
        //_get_cb = getCallback;
    } else {
        ALOGE("Register GetCallback Failed!");
    }
#endif
    return Void();
}

// Methods from ::android::hidl::base::V1_0::IBase follow.

V1_0::IRKNeuralnetworks* HIDL_FETCH_IRKNeuralnetworks(const char* /* name */) {
    RECORD_TAG();
    UNUSED(ret);
#if IMPL_RKNN
    ALOGI("Linked RKNeuralnetworks and rknn_api.");
#endif
    return new RKNeuralnetworks();
}
//
}  // namespace rockchip::hardware::neuralnetworks::implementation
