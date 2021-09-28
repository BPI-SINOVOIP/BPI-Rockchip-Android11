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

#include <stdint.h>

// TODO: use netdutils::Slice for (msg, len).
void res_pquery(const uint8_t* msg, int len);

// Thread-unsafe functions returning pointers to static buffers :-(
// TODO: switch all res_debug to std::string
const char* p_type(int type);
const char* p_section(int section, int opcode);
const char* p_class(int cl);
const char* p_rcode(int rcode);

int resolv_set_log_severity(uint32_t logSeverity);
