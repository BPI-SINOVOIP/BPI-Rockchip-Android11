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

#include <rockchip/hardware/neuralnetworks/1.0/IRKNeuralnetworks.h>
#include <log/log.h>
#include <iostream>
#include <android/hidl/allocator/1.0/IAllocator.h>
#include <android/hidl/memory/1.0/IMemory.h>
#include <hidlmemory/mapping.h>

#include <errno.h>

#include <stdint.h>
#include <stdlib.h>
#include <fstream>
#include <algorithm>
#include <queue>
#include <sys/time.h>
#include <sys/cdefs.h>

#include "../default/prebuilts/librknnrt/include/rknn_api.h"
#include "HalInterfaces.h"

__BEGIN_DECLS
using ::android::hidl::allocator::V1_0::IAllocator;
using ::android::hidl::memory::V1_0::IMemory;
using ::android::hardware::hidl_memory;

using namespace std;
using namespace rockchip::hardware::neuralnetworks::V1_0;
using namespace android;

using ::android::hardware::Return;
using ::android::hardware::Void;

namespace rockchip {
namespace nn {

class RockchipNeuralnetworksBuilder {
  public:
    RockchipNeuralnetworksBuilder();
    // From rknn api
    int rknn_init(rknn_context* context, void *model, uint32_t size, uint32_t flag);
    int rknn_destroy(rknn_context context);
    int rknn_query(rknn_context context, rknn_query_cmd cmd, void* info, uint32_t size);
    int rknn_inputs_set(rknn_context context, uint32_t n_inputs, rknn_input inputs[]);
    int rknn_run(rknn_context context, rknn_run_extend* extend);
    int rknn_outputs_get(rknn_context, uint32_t n_outputs, rknn_output outputs[], rknn_output_extend* extend);
    int rknn_outputs_release(rknn_context context, uint32_t n_ouputs, rknn_output outputs[]);
    int rknn_destory_mem(rknn_context ctx, rknn_tensor_mem *mem);
    rknn_tensor_mem* rknn_create_mem(rknn_context ctx, uint32_t size);
    int rknn_set_io_mem(rknn_context ctx, rknn_tensor_mem *mem, rknn_tensor_attr *attr);
    
  private:
    int _get_model_info(rknn_context context);
    sp<hal::V1_0::IRKNeuralnetworks> _kRKNNInterface;
    sp<IAllocator> _kAllocInterface;
    std::map<uint64_t, void*> m_TempTensorMap;
};
}
}
__END_DECLS
