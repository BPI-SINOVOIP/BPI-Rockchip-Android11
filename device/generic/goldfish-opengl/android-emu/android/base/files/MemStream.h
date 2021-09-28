// Copyright 2015 The Android Open Source Project
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#pragma once

#include "android/base/Compiler.h"
#include "android/base/files/Stream.h"

#include <vector>

namespace android {
namespace base {

// An implementation of the Stream interface on top of a vector.
class MemStream : public Stream {
public:
    using Buffer = std::vector<char>;

    MemStream(int reserveSize = 512);
    MemStream(Buffer&& data);

    MemStream(MemStream&& other) = default;
    MemStream& operator=(MemStream&& other) = default;

    int writtenSize() const;
    int readPos() const;
    int readSize() const;

    // Stream interface implementation.
    ssize_t read(void* buffer, size_t size) override;
    ssize_t write(const void* buffer, size_t size) override;

    // Snapshot support.
    void save(Stream* stream) const;
    void load(Stream* stream);

    const Buffer& buffer() const { return mData; }

private:
    DISALLOW_COPY_AND_ASSIGN(MemStream);

    Buffer mData;
    int mReadPos = 0;
};

}  // namespace base
}  // namespace android
