// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once

#include <zircon/types.h>

__BEGIN_CDECLS

#define ARM_SMC_SERVICE_CALL_NUM_TRUSTED_OS_BASE 0x32
#define ARM_SMC_SERVICE_CALL_NUM_TRUSTED_OS_LENGTH 0xE
#define ARM_SMC_SERVICE_CALL_NUM_MAX 0x3F
#define ARM_SMC_SERVICE_CALL_NUM_MASK 0x3F
#define ARM_SMC_SERVICE_CALL_NUM_SHIFT 24
#define ARM_SMC_GET_SERVICE_CALL_NUM_FROM_FUNC_ID(func_id) \
    (((func_id) >> ARM_SMC_SERVICE_CALL_NUM_SHIFT) & ARM_SMC_SERVICE_CALL_NUM_MASK)

typedef struct zx_smc_parameters {
    uint32_t func_id;
    uint64_t arg1;
    uint64_t arg2;
    uint64_t arg3;
    uint64_t arg4;
    uint64_t arg5;
    uint64_t arg6;
    uint16_t client_id;
    uint16_t secure_os_id;
} zx_smc_parameters_t;

typedef struct zx_smc_result {
    uint64_t arg0;
    uint64_t arg1;
    uint64_t arg2;
    uint64_t arg3;
} zx_smc_result_t;

__END_CDECLS
