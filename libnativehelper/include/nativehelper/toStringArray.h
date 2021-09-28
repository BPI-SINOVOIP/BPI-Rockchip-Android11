/*
 * Copyright (C) 2011 The Android Open Source Project
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

#ifndef LIBNATIVEHELPER_INCLUDE_NATIVEHELPER_TOSTRINGARRAY_H_
#define LIBNATIVEHELPER_INCLUDE_NATIVEHELPER_TOSTRINGARRAY_H_

#include "libnativehelper_api.h"

#ifdef __cplusplus

#include <string>
#include <vector>
#include "ScopedLocalRef.h"

template <typename Counter, typename Getter>
jobjectArray toStringArray(JNIEnv* env, Counter* counter, Getter* getter) {
    size_t count = (*counter)();
    jobjectArray result = newStringArray(env, count);
    if (result == NULL) {
        return NULL;
    }
    for (size_t i = 0; i < count; ++i) {
        ScopedLocalRef<jstring> s(env, env->NewStringUTF((*getter)(i)));
        if (env->ExceptionCheck()) {
            return NULL;
        }
        env->SetObjectArrayElement(result, i, s.get());
        if (env->ExceptionCheck()) {
            return NULL;
        }
    }
    return result;
}

struct VectorCounter {
    const std::vector<std::string>& strings;
    explicit VectorCounter(const std::vector<std::string>& strings) : strings(strings) {}
    size_t operator()() {
        return strings.size();
    }
};
struct VectorGetter {
    const std::vector<std::string>& strings;
    explicit VectorGetter(const std::vector<std::string>& strings) : strings(strings) {}
    const char* operator()(size_t i) {
        return strings[i].c_str();
    }
};

inline jobjectArray toStringArray(JNIEnv* env, const std::vector<std::string>& strings) {
    VectorCounter counter(strings);
    VectorGetter getter(strings);
    return toStringArray<VectorCounter, VectorGetter>(env, &counter, &getter);
}

#endif  // __cplusplus

#endif  // LIBNATIVEHELPER_INCLUDE_NATIVEHELPER_TOSTRINGARRAY_H_
