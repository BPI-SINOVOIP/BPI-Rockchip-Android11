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

#include <cutils/properties.h>
#include <sys/mman.h>
#include "RockchipNeuralnetworksBuilder.h"

using ::android::hidl::allocator::V1_0::IAllocator;
using ::android::hidl::memory::V1_0::IMemory;
using ::android::hardware::hidl_memory;

using namespace std;
using namespace rockchip::hardware::neuralnetworks::V1_0;
using namespace android;

using ::android::hardware::Return;
using ::android::hardware::Void;
static bool g_debug_pro = 0;

#define CHECK() \
    if (g_debug_pro) ALOGE("%s", __func__)

#define allocateAsh(size, ...) \
    if (!size) { ALOGE("%s: allocateAsh size can't be 0!!!", __func__); \
        return -1; \
    } \
    _kAllocInterface->allocate(size, __VA_ARGS__)

class _RKNNContext {
public:
    uint64_t context = 0;
    rknn_input_output_num io_num;
    rknn_tensor_attr *input_tensor_attrs = nullptr;
    rknn_tensor_attr *output_tensor_attrs = nullptr;
};

static void printRKNNTensor(rknn_tensor_attr *attr) {
    ALOGD("index=%d name=%s n_dims=%d dims=[%d %d %d %d] n_elems=%d size=%d fmt=%d type=%d qnt_type=%d fl=%d zp=%d scale=%f\n",
            attr->index, attr->name, attr->n_dims, attr->dims[3], attr->dims[2], attr->dims[1], attr->dims[0],
            attr->n_elems, attr->size, 0, attr->type, attr->qnt_type, attr->fl, attr->zp, attr->scale);
}

static RKNNQueryCmd to_rknn_hal(rknn_query_cmd cmd) {
    switch (cmd) {
        case RKNN_QUERY_IN_OUT_NUM:
            return RKNNQueryCmd::RKNN_QUERY_IN_OUT_NUM;
        case RKNN_QUERY_INPUT_ATTR:
            return RKNNQueryCmd::RKNN_QUERY_INPUT_ATTR;
        case RKNN_QUERY_OUTPUT_ATTR:
            return RKNNQueryCmd::RKNN_QUERY_OUTPUT_ATTR;
        case RKNN_QUERY_PERF_DETAIL:
            return RKNNQueryCmd::RKNN_QUERY_PERF_DETAIL;
        case RKNN_QUERY_PERF_RUN:
            return RKNNQueryCmd::RKNN_QUERY_PERF_RUN;
        case RKNN_QUERY_SDK_VERSION:
            return RKNNQueryCmd::RKNN_QUERY_SDK_VERSION;
        case RKNN_QUERY_MEM_SIZE:
            return RKNNQueryCmd::RKNN_QUERY_MEM_SIZE;
        case RKNN_QUERY_CUSTOM_STRING:
            return RKNNQueryCmd::RKNN_QUERY_CUSTOM_STRING;
        default:
            return RKNNQueryCmd::RKNN_QUERY_CMD_MAX;
    }
}

static RKNNTensorType to_rknn_hal(rknn_tensor_type type) {
    switch (type)
    {
    case RKNN_TENSOR_FLOAT32:
        return RKNNTensorType::RKNN_TENSOR_FLOAT32;
    case RKNN_TENSOR_FLOAT16:
        return RKNNTensorType::RKNN_TENSOR_FLOAT16;
    case RKNN_TENSOR_INT8:
        return RKNNTensorType::RKNN_TENSOR_INT8;
    case RKNN_TENSOR_UINT8:
        return RKNNTensorType::RKNN_TENSOR_UINT8;
    case RKNN_TENSOR_INT16:
        return RKNNTensorType::RKNN_TENSOR_INT16;
    default:
        return RKNNTensorType::RKNNTensorType_MAX;
    }
}

static RKNNTensorFormat to_rknn_hal(rknn_tensor_format fmt) {
    switch (fmt) {
        case RKNN_TENSOR_NCHW:
            return RKNNTensorFormat::RKNN_TENSOR_NCHW;
        case RKNN_TENSOR_NHWC:
            return RKNNTensorFormat::RKNN_TENSOR_NHWC;
        default:
            return RKNNTensorFormat::RKNNTensorFormat_MAX;
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

namespace rockchip {
namespace nn {

RockchipNeuralnetworksBuilder::RockchipNeuralnetworksBuilder() {
    _kRKNNInterface = IRKNeuralnetworks::getService();
    if (_kRKNNInterface == NULL) {
        ALOGE("Failed to getService: IRKNeuralnetworks[vendor.rknn-1-0]!");
    }
    _kAllocInterface = IAllocator::getService("ashmem");
    if (_kAllocInterface == NULL) {
        ALOGE("Failed to getService: IAllocator[android.ashmem-1-0]!");
    }

    // ALOGD("Successfully initial.");
}

int RockchipNeuralnetworksBuilder::rknn_init(rknn_context* context, void *pData, uint32_t size, uint32_t flag) {
    CHECK();
    int ret_code = 0;
    g_debug_pro = property_get_bool("persist.vendor.rknndebug", false);
    allocateAsh(size, [&](bool success, const hidl_memory& mem) {
        if (!success) {
            ALOGE("Allocate memory failed!");
        } else {
            const struct RKNNModel model = {
                //.name = "mobilenet_v1",
                //.path = "/data/mobilenet_v1-tf.rknn",
                .modelData = mem,
            };
            sp<IMemory> memory = mapMemory(mem);
            memory->update();
            memcpy(memory->getPointer(), pData, size);
            memory->commit();

            Return<void> ret =  _kRKNNInterface->rknnInit(model, size, flag, [&](ErrorStatus status, RKNNContext rContext) {
                if ((int)status == RKNN_SUCC) {
                    _RKNNContext *_rknn_context = new _RKNNContext();
                    *context = (rknn_context)_rknn_context;
                    _rknn_context->context = rContext;
                    ret_code = _get_model_info((rknn_context)_rknn_context);
                    // ALOGD("Client init Successfully! ctx=0x%x", rContext);
                } else {
                    ALOGE("Client init failed!");
                }
            });
            if (!ret.isOk()) {
                ALOGE("rknn_init Failed!");
                ret_code = -1;
            }
        }
    });
    return ret_code;
}

int RockchipNeuralnetworksBuilder::rknn_destroy(rknn_context context) {
    CHECK();
    _RKNNContext *_rknn_context = (_RKNNContext *)context;
    if (_rknn_context != nullptr) {
        // ALOGD("rknnDestory ctx=0x%x", _rknn_context->context);
        _kRKNNInterface->rknnDestory(_rknn_context->context);
        _rknn_context->context = 0;
        if (_rknn_context->input_tensor_attrs != nullptr) {
            free(_rknn_context->input_tensor_attrs);
            _rknn_context->input_tensor_attrs = nullptr;
        }
        if (_rknn_context->output_tensor_attrs != nullptr) {
            free(_rknn_context->output_tensor_attrs);
            _rknn_context->output_tensor_attrs = nullptr;
        }
        delete(_rknn_context);
    }
    return 0;
}

int RockchipNeuralnetworksBuilder::rknn_query(rknn_context context, rknn_query_cmd cmd, void* info, uint32_t size) {
    CHECK();
    _RKNNContext *_rknn_context = (_RKNNContext *)context;
    allocateAsh(size, [&](bool success, const hidl_memory& mem) {
        if (!success) {
            ALOGE("Allocate memory failed!");
        } else {
            sp<IMemory> pMem = mapMemory(mem);
            pMem->update();
            memcpy(pMem->getPointer(), info, size);
            pMem->commit();
            
            _kRKNNInterface->rknnQuery(_rknn_context->context, to_rknn_hal(cmd), mem, size);
            memcpy(info, pMem->getPointer(), size);
        }
    });
    return 0;
}

int RockchipNeuralnetworksBuilder::rknn_inputs_set(rknn_context context, uint32_t n_inputs, rknn_input inputs[]) {
    CHECK();
    _RKNNContext *_rknn_context = (_RKNNContext *)context;
    int poolSize = 0;
    for (int i = 0; i < n_inputs; i++) {
        poolSize += inputs[i].size;
    }
    allocateAsh(poolSize, [&](bool success, const hidl_memory& mem) {
        if (!success) {

        } else {
            vector<RKNNInput> input_array;
            sp<IMemory> memory = mapMemory(mem);
            memory->update();
            for (uint32_t i = 0; i < n_inputs; i++) {
                // map memory
                uint32_t cur_offset = 0;
                for (uint32_t j = 0; j < i; j++) {
                    cur_offset += inputs[j].size;
                }

                memcpy(memory->getPointer(), inputs[i].buf, inputs[i].size);
                const struct DataLocation buf = {
                    .poolIndex = static_cast<uint32_t>(i),
                    .offset = cur_offset,
                    .length = static_cast<uint32_t>(inputs[i].size),
                };
                const struct RKNNInput input0 = {
                    .index = inputs[i].index,
                    .buf = buf,
                    .pass_through = false,
                    .type = to_rknn_hal(inputs[i].type),
                    .fmt = to_rknn_hal(inputs[i].fmt),
                };
                input_array.push_back(input0);
            }
            memory->commit();
            const struct Request request = {
                .n_inputs = static_cast<uint32_t>(input_array.size()),
                .inputs = input_array,
                .pool = mem,
            };
            Return<ErrorStatus> ret = _kRKNNInterface->rknnInputsSet(_rknn_context->context, request);
            if (!ret.isOk()) {
                ALOGE("rknnInputsSet error!");
            }
        }
    });
    return 0;
}

int RockchipNeuralnetworksBuilder::rknn_run(rknn_context context, rknn_run_extend* extend) {
    CHECK();
    _RKNNContext *_rknn_context = (_RKNNContext *)context;
    const struct RKNNRunExtend rExt = {
        .frame_id = extend?extend->frame_id:0,
    };
    Return<ErrorStatus> ret = _kRKNNInterface->rknnRun(_rknn_context->context, rExt);
    if (ret.isOk()) {
        return 0;
    } else {
        ALOGE("run error!");
        return -1;
    }
}

int RockchipNeuralnetworksBuilder::rknn_outputs_get(rknn_context context, uint32_t n_outputs, rknn_output outputs[], rknn_output_extend* extend) {
    CHECK();
    _RKNNContext *_rknn_context = (_RKNNContext *)context;
    int poolSize = 0;
    for (int i = 0; i < n_outputs; i++) {
        int out_size;
        if (outputs[i].is_prealloc == 1) {
            out_size = outputs[i].size;
        } else {
            out_size = _rknn_context->output_tensor_attrs[i].size;
            if (outputs[i].want_float == 1) {
                out_size = _rknn_context->output_tensor_attrs[i].n_elems * sizeof(float);
            }
            outputs[i].size = out_size;
        }
        poolSize += out_size;
    }

    allocateAsh(poolSize, [&](bool success, const hidl_memory& mem) {
        if (!success) {
        } else {
            sp<IMemory> memory = mapMemory(mem);
            vector<RKNNOutput> output_array;
            // map memory
            for (uint32_t i = 0; i < n_outputs; i++) {
                uint32_t cur_offset = 0;
                for (uint32_t j = 0; j < i; j++) {
                    cur_offset += outputs[j].size;
                }
                const struct DataLocation result = {
                    .poolIndex = i,
                    .offset = cur_offset,
                    .length = outputs[i].size,
                };
                const struct RKNNOutput output0 = {
                    .want_float = outputs[i].want_float,
                    .is_prealloc = true,
                    .buf = result,
                };
                output_array.push_back(output0);
            }
            const struct RKNNOutputExtend gExt = {
                .frame_id = extend? extend->frame_id:0,
            };
            const struct Response response = {
                .n_outputs = static_cast<uint32_t>(output_array.size()),
                .outputs = output_array,
                .pool = mem,
            };
            Return<ErrorStatus> ret = _kRKNNInterface->rknnOutputsGet(_rknn_context->context, response, gExt);
            if (!ret.isOk()) {
                ALOGE("rknnInputsSet error!");
            }
            void *resultPool = memory->getPointer();
            for (uint32_t i = 0; i < n_outputs; i++) {
                uint32_t cur_offset = 0;
                if (outputs[i].is_prealloc != 1) {
                    outputs[i].buf = malloc(outputs[i].size);
                }
                for (uint32_t j = 0; j < i; j++) {
                    cur_offset += outputs[j].size;
                }
                memcpy(outputs[i].buf, (char *)resultPool + cur_offset, outputs[i].size);
            }
        }
    });
    return 0;
}

int RockchipNeuralnetworksBuilder::rknn_outputs_release(rknn_context context, uint32_t n_outputs, rknn_output outputs[]) {
    CHECK();
    for (int i = 0; i < n_outputs; i++) {
        if (outputs[i].is_prealloc != 1) {
            free(outputs[i].buf);
        }
    }
    return 0;
}

int RockchipNeuralnetworksBuilder::rknn_destory_mem(rknn_context context, rknn_tensor_mem *mem) {
    CHECK();
    _RKNNContext *_rknn_context = (_RKNNContext *)context;
    RKNNTensorMemory bridge_mem;

    bridge_mem.virt_addr = (uint64_t)0;
    bridge_mem.phys_addr = mem->phys_addr;
    bridge_mem.offset = mem->offset;
    bridge_mem.size = mem->size;
    bridge_mem.flags = mem->flags;
    bridge_mem.priv_data = (uint64_t)mem->priv_data;
    bridge_mem.bridge_uuid = 0;

    std::map<uint64_t, void *>::iterator iter;  
    for(iter = m_TempTensorMap.begin(); iter != m_TempTensorMap.end(); iter++) {
        if (mem == (rknn_tensor_mem *)iter->second) {
            bridge_mem.bridge_uuid = iter->first;
            // #ifdef __arm__
            // printf("rknn_destory_mem:uuid=0x%llx, mem=%p\n", bridge_mem.bridge_uuid, mem);
            // #else
            // printf("rknn_destory_mem:uuid=0x%lx, mem=%p\n", bridge_mem.bridge_uuid, mem);
            // #endif
            break;
        }
    }

    if (bridge_mem.bridge_uuid != 0) {      
        Return<ErrorStatus> ret = _kRKNNInterface->rknnDestoryMemory(_rknn_context->context, bridge_mem);
        if (ret.isOk()) {
            return 0;
        } else {
            ALOGE("rknn_destory_mem error!");
            return -1;
        }
    }

    if (mem->fd >= 0) {
        close(mem->fd);
    }
    
    return 0;
}

rknn_tensor_mem* RockchipNeuralnetworksBuilder::rknn_create_mem(rknn_context context, uint32_t size) {
    CHECK();
    _RKNNContext *_rknn_context = (_RKNNContext *)context;
    rknn_tensor_mem* mem= nullptr;

    Return<void> ret = _kRKNNInterface->rknnCreateMem(_rknn_context->context, size, [&](ErrorStatus status, RKNNTensorMemory respose_mem) {
        if ((int)status == RKNN_SUCC) {
            mem = (rknn_tensor_mem *)malloc(sizeof(rknn_tensor_mem));
            mem->virt_addr = nullptr;
            mem->phys_addr = respose_mem.phys_addr;
            mem->offset = respose_mem.offset;
            mem->size = respose_mem.size;
            mem->flags = respose_mem.flags;
            mem->priv_data = (void *)respose_mem.priv_data;

            const native_handle_t* hnd = respose_mem.bufferHdl.getNativeHandle();
            mem->fd = dup(hnd->data[0]);

            if (dma_map(mem->fd, mem->size, PROT_READ | PROT_WRITE, MAP_SHARED, 0, &mem->virt_addr) < 0) {
                ALOGE("rknn_create_mem: dma_map failed!");
                free(mem);
                mem = nullptr;
            }

            m_TempTensorMap.insert(pair<uint64_t, void*>(respose_mem.bridge_uuid, mem));

            // #ifdef __arm__
            // printf("rknn_create_mem:uuid=0x%llx, mem=%p\n", respose_mem.bridge_uuid, mem);
            // #else
            // printf("rknn_create_mem:uuid=0x%lx, mem=%p\n", respose_mem.bridge_uuid, mem);
            // #endif

        } else {
            ALOGE("rknn_create_mem failed!");
        }
    });

    if (ret.isOk()) {
        return mem;
    } else {
        ALOGE("rknn_create_mem failed!");
    }

    return mem;
}

int RockchipNeuralnetworksBuilder::rknn_set_io_mem(rknn_context context, rknn_tensor_mem *mem, rknn_tensor_attr *attr) {
    CHECK();
    _RKNNContext *_rknn_context = (_RKNNContext *)context;
    android::hardware::hidl_handle handle;

    if (sizeof(rknn_tensor_attr) != sizeof(RKNNTensorAttr)) {
        ALOGE("sizeof(rknn_tensor_attr) != sizeof(::rockchip::hardware::neuralnetworks::V1_0::RKNNTensorAttr): %ld vs %ld\n",
                sizeof(rknn_tensor_attr), sizeof(::rockchip::hardware::neuralnetworks::V1_0::RKNNTensorAttr));
        return -1;
    }

    RKNNTensorMemory bridge_mem;
    RKNNTensorAttr   bridge_attr;

    memcpy(&bridge_attr, attr, sizeof(RKNNTensorAttr));

    bridge_mem.virt_addr = (uint64_t)0;
    bridge_mem.phys_addr = mem->phys_addr;
    bridge_mem.offset = mem->offset;
    bridge_mem.size = mem->size;
    bridge_mem.flags = mem->flags;
    bridge_mem.priv_data = (uint64_t)mem->priv_data;
    bridge_mem.bridge_uuid = 0;

    std::map<uint64_t, void *>::iterator iter;  
    for(iter = m_TempTensorMap.begin(); iter != m_TempTensorMap.end(); iter++) {
        if (mem == (rknn_tensor_mem *)iter->second) {
            bridge_mem.bridge_uuid = iter->first;
            // #ifdef __arm__
            // printf("rknn_set_io_mem:uuid=0x%llx, mem=%p\n", bridge_mem.bridge_uuid, mem);
            // #else
            // printf("rknn_set_io_mem:uuid=0x%lx, mem=%p\n", bridge_mem.bridge_uuid, mem);
            // #endif
            break;
        }
    }

    if (bridge_mem.bridge_uuid == 0) {
        native_handle_t* nativeHandle = native_handle_create(/*numFd*/ 1, 0);
        if(mem->fd >= 0){
            nativeHandle->data[0] = dup(mem->fd);            
            handle.setTo(nativeHandle, /*shouldOwn=*/true);
            bridge_mem.bufferHdl = handle;
        } else {
            native_handle_delete(nativeHandle);
        }
    }

    Return<ErrorStatus> ret = _kRKNNInterface->rknnSetIOMem(_rknn_context->context, bridge_mem, bridge_attr);
    if (ret.isOk()) {
        return 0;
    } else {
        ALOGE("rknn_set_io_mem error:%s", ret.description().c_str());
        return -1;
    }
    return 0;
}

int RockchipNeuralnetworksBuilder::_get_model_info(rknn_context context) {
    int ret;
    _RKNNContext *_rknn_context = (_RKNNContext *)context;
    ret = rknn_query(context, RKNN_QUERY_IN_OUT_NUM, &(_rknn_context->io_num), sizeof(rknn_input_output_num));
    if (ret != RKNN_SUCC) {
        ALOGE("query RKNN_QUERY_IN_OUT_NUM fail!");
        return -1;
    }

    _rknn_context->input_tensor_attrs = (rknn_tensor_attr *)malloc(_rknn_context->io_num.n_input*sizeof(rknn_tensor_attr));
    _rknn_context->output_tensor_attrs = (rknn_tensor_attr *)malloc(_rknn_context->io_num.n_output*sizeof(rknn_tensor_attr));

    memset(_rknn_context->input_tensor_attrs, 0, _rknn_context->io_num.n_input*sizeof(rknn_tensor_attr));
    for (int i = 0; i < _rknn_context->io_num.n_input; i++) {
        _rknn_context->input_tensor_attrs[i].index = i;
        ret = rknn_query(context, RKNN_QUERY_INPUT_ATTR, &(_rknn_context->input_tensor_attrs[i]), sizeof(rknn_tensor_attr));
        if (ret != RKNN_SUCC) {
            ALOGE("rknn_query fail! ret=%d\n", ret);
            return -1;
        }
        // printRKNNTensor(&(_rknn_context->output_tensor_attrs[i]));
    }

    memset(_rknn_context->output_tensor_attrs, 0, _rknn_context->io_num.n_output*sizeof(rknn_tensor_attr));
    for (int i = 0; i < _rknn_context->io_num.n_output; i++) {
        _rknn_context->output_tensor_attrs[i].index = i;
        ret = rknn_query(context, RKNN_QUERY_OUTPUT_ATTR, &(_rknn_context->output_tensor_attrs[i]), sizeof(rknn_tensor_attr));
        if (ret != RKNN_SUCC) {
            ALOGE("rknn_query fail! ret=%d\n", ret);
            return -1;
        }
        // printRKNNTensor(&(_rknn_context->output_tensor_attrs[i]));
    }
    return 0;
}


}
}
