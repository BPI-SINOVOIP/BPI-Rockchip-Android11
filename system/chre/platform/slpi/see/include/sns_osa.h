/*
 * Copyright (C) 2019 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once
/*=============================================================================
  @file sns_osa.h

  Several Operating System Abstractions to be used by the sns_qsocket client
  library.

  ===========================================================================*/

/*=============================================================================
  Include Files
  ===========================================================================*/
#include <stdio.h>
#include <stdlib.h>

#include "chre/platform/log.h"
#include "err.h"

/*=============================================================================
  Macros
  ===========================================================================*/

/* Log a string message */
#define SNS_LOG_VERBOSE(...)
#define SNS_LOG_DEBUG LOGD
#define SNS_LOG_WARN LOGW
#define SNS_LOG_ERROR LOGE
#define SNS_LOG(prio, ...) SNS_LOG_##prio(__VA_ARGS__)

/* See assert() */
#define SNS_ASSERT(value) \
  if (!(value)) ERR_FATAL(#value, 0, 0, 0)

/* Allocate and free memory */
#define sns_malloc(x) malloc(x)
#define sns_free(x) free(x)

#ifndef ARR_SIZE
#define ARR_SIZE(arr) (sizeof(arr) / sizeof(arr[0]))
#endif

#ifndef UNUSED_VAR
#define UNUSED_VAR(var) ((void)(var));
#endif