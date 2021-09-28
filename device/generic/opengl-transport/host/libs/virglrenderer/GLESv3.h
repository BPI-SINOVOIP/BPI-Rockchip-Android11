/*
 * Copyright (C) 2018 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

#include "gles3_dec.h"

#include <map>

struct GlSync;

struct GLESv3 : public gles3_decoder_context_t {
    GLESv3();

    glDrawBuffers_server_proc_t glDrawBuffersEXT;

    glDrawArraysInstanced_server_proc_t glDrawArraysInstancedEXT;
    glDrawElementsInstanced_server_proc_t glDrawElementsInstancedEXT;

    glVertexAttribDivisor_server_proc_t glVertexAttribDivisorEXT;

    std::map<uint64_t, GlSync*> sync_map;

  private:
    uint64_t sync_nextId = 1U;
    friend struct GlSync;
};
