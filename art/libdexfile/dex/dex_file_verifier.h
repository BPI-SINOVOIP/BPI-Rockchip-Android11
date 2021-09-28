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

#ifndef ART_LIBDEXFILE_DEX_DEX_FILE_VERIFIER_H_
#define ART_LIBDEXFILE_DEX_DEX_FILE_VERIFIER_H_

#include <string>

#include <inttypes.h>

namespace art {

class DexFile;

namespace dex {

bool Verify(const DexFile* dex_file,
            const uint8_t* begin,
            size_t size,
            const char* location,
            bool verify_checksum,
            std::string* error_msg);

}  // namespace dex
}  // namespace art

#endif  // ART_LIBDEXFILE_DEX_DEX_FILE_VERIFIER_H_
