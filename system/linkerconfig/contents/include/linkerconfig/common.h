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

#include <string>
#include <vector>

#include "linkerconfig/context.h"
#include "linkerconfig/namespace.h"
#include "linkerconfig/section.h"

namespace android {
namespace linkerconfig {
namespace contents {

// Adds links from all namespaces in the given section to the namespace for
// /system/${LIB} for standard libraries like Bionic (libc.so, libm.so,
// libdl.so) and applicable libclang_rt.*.
void AddStandardSystemLinks(const Context& ctx, modules::Section* section);

std::vector<std::string> GetSystemStubLibraries();
}  // namespace contents
}  // namespace linkerconfig
}  // namespace android
