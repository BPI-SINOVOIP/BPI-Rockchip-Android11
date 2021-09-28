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
#include "../default/prebuilts/librknnrt/include/rknn_api.h"

#include <stdint.h>
#include <stdlib.h>
#include <fstream>
#include <algorithm>
#include <queue>
#include <sys/time.h>
#include <sys/cdefs.h>

__BEGIN_DECLS

typedef struct ARKNNHAL ARKNNHAL;

// From rknn api
int ARKNN_client_create(ARKNNHAL **hal);
int ARKNN_init(ARKNNHAL *hal, rknn_context* context, void *model, uint32_t size, uint32_t flag);
int ARKNN_destroy(ARKNNHAL *hal, rknn_context context);
int ARKNN_query(ARKNNHAL *hal, rknn_context context, rknn_query_cmd cmd, void* info, uint32_t size);
int ARKNN_inputs_set(ARKNNHAL *hal, rknn_context context, uint32_t n_inputs, rknn_input inputs[]);
int ARKNN_run(ARKNNHAL *hal, rknn_context context, rknn_run_extend* extend);
int ARKNN_outputs_get(ARKNNHAL *hal, rknn_context context, uint32_t n_outputs, rknn_output outputs[], rknn_output_extend* extend);
int ARKNN_outputs_release(ARKNNHAL *hal, rknn_context context, uint32_t n_ouputs, rknn_output outputs[]);
int ARKNN_destory_mem(ARKNNHAL *hal, rknn_context context, rknn_tensor_mem *mem);
rknn_tensor_mem * ARKNN_create_mem(ARKNNHAL *hal, rknn_context context, uint32_t size);
int ARKNN_set_io_mem(ARKNNHAL *hal, rknn_context context, rknn_tensor_mem *mem, rknn_tensor_attr *attr);

__END_DECLS
