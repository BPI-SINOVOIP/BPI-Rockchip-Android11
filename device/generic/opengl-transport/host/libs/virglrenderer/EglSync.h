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

#include <cstdint>
#include <map>

#include <EGL/egl.h>

struct EglSync {
    static std::map<uint64_t, EglSync*> map;
    static uint64_t nextId;

    EglSync(EGLSyncKHR sync_) : sync(sync_), id(nextId++) {
        map.emplace(id, this);
    }

    ~EglSync() {
        map.erase(id);
    }

    EGLSyncKHR sync;
    uint64_t id;
};
