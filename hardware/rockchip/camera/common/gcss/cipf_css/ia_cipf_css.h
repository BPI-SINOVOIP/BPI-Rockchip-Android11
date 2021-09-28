/*
 * Copyright (C) 2013-2017 Intel Corporation
 * Copyright (c) 2017, Fuzhou Rockchip Electronics Co., Ltd
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

#ifndef _IA_CIPF_CSS_H_
#define _IA_CIPF_CSS_H_

#include <ia_cipf/ia_cipf_types.h>

/* Note:
 * PSYS Library is considered to query UIDs from binary releases
 * or from other external definition (markup language?)
 *
 * Here we would define only the ones statically integrated
 */

#define psys_2600_pg_uid(id) ia_fourcc(((id & 0xFF00) >> 8),id,'G','0')

#endif
