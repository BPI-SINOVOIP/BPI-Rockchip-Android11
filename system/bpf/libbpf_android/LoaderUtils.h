/*
 * Copyright (C) 2018 The Android Open Source Project
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

#include <stdio.h>
#include <iostream>
#include <string>
#include <vector>

#include <android-base/strings.h>

static std::string pathToFilename(const std::string& path, bool noext = false) {
    std::vector<std::string> spath = android::base::Split(path, "/");
    std::string ret = spath.back();

    if (noext) {
        size_t lastindex = ret.find_last_of('.');
        return ret.substr(0, lastindex);
    }
    return ret;
}

static void deslash(std::string& s) {
    std::replace(s.begin(), s.end(), '/', '_');
}
