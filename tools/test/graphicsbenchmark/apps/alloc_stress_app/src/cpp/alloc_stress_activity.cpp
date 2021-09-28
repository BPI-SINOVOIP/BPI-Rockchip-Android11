/*
 * Copyright (C) 2010 The Android Open Source Project
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
 *
 */

#include <jni.h>
#include <iostream>
#include <tuple>
#include <algorithm>
#include <numeric>
#include <string>
#include <sstream>
#include <cstring>
#include <fstream>

#include <android/log.h>
#define LOG(...) __android_log_write(ANDROID_LOG_INFO, "ALLOC-STRESS",__VA_ARGS__)

using namespace std;

size_t s = 4 * (1 << 20); // 4 MB
extern "C"
JNIEXPORT void JNICALL
Java_com_android_game_qualification_allocstress_MainActivity_cmain(JNIEnv* , jobject /* this */)
{

    long long allocCount = 0;
    while (1) {
        char *ptr = (char *) malloc(s);
        memset(ptr, (int) allocCount >> 10, s);
        std::stringstream ss;
        ss << "total alloc: " << (allocCount >> 20) << endl;
        LOG(ss.str().c_str());
        allocCount += s;
    }
}
