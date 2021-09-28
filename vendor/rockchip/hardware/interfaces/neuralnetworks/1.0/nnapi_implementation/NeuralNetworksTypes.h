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

#ifndef RKNNAPI_NEURALNETWORKSTYPES_H_
#define RKNNAPI_NEURALNETWORKSTYPES_H_

#include <stdint.h>

#include "../default/prebuilts/librknnrt/include/rknn_api.h"

typedef struct ARKNNHAL ARKNNHAL;

typedef int (*ARKNN_client_create_fn)(ARKNNHAL **hal);
typedef int (*ARKNN_init_fn)(ARKNNHAL *hal, rknn_context *context, void *model, uint32_t size, uint32_t flag);
typedef int (*ARKNN_destroy_fn)(ARKNNHAL *hal, rknn_context);
typedef int (*ARKNN_query_fn)(ARKNNHAL *hal, rknn_context context, rknn_query_cmd cmd, void* info, uint32_t size);
typedef int (*ARKNN_inputs_set_fn)(ARKNNHAL *hal, rknn_context context, uint32_t n_inputs, rknn_input inputs[]);
typedef int (*ARKNN_run_fn)(ARKNNHAL *hal, rknn_context context, rknn_run_extend* extend);
typedef int (*ARKNN_outputs_get_fn)(ARKNNHAL *hal, rknn_context, uint32_t n_outputs, rknn_output outputs[], rknn_output_extend* extend);
typedef int (*ARKNN_outputs_release_fn)(ARKNNHAL *hal, rknn_context context, uint32_t n_ouputs, rknn_output outputs[]);
typedef int (*ARKNN_destory_mem_fn)(ARKNNHAL *hal, rknn_context context, rknn_tensor_mem *mem);
typedef rknn_tensor_mem * (*ARKNN_create_mem_fn)(ARKNNHAL *hal, rknn_context context, uint32_t size);
typedef int (*ARKNN_set_io_mem_fn)(ARKNNHAL *hal, rknn_context context, rknn_tensor_mem *mem, rknn_tensor_attr *attr);

typedef int (*ASharedMemory_create_fn)(const char* name, size_t size);

#endif  // RKNNAPI_NEURALNETWORKSTYPES_H_
