/*
 * Copyright (C) 2020 The Android Open Source Project
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

#ifndef ANDROID_FRAMEWORKS_ML_NN_RUNTIME_TEST_TEST_UTILS_H
#define ANDROID_FRAMEWORKS_ML_NN_RUNTIME_TEST_TEST_UTILS_H

#include <android-base/mapped_file.h>
#include <android-base/unique_fd.h>
#include <android/sharedmem.h>

#include <memory>
#include <utility>

#include "TestHarness.h"
#include "TestNeuralNetworksWrapper.h"

namespace android::nn {

// Convenience class to manage the file, mapping, and memory object.
class TestAshmem {
   public:
    TestAshmem(::android::base::unique_fd fd, std::unique_ptr<::android::base::MappedFile> mapped,
               test_wrapper::Memory memory)
        : mFd(std::move(fd)), mMapped(std::move(mapped)), mMemory(std::move(memory)) {}

    // Factory function for TestAshmem; prefer this over the raw constructor
    static std::unique_ptr<TestAshmem> createFrom(const test_helper::TestBuffer& buffer) {
        return createFrom(buffer.get<void>(), buffer.size());
    }

    // Factory function for TestAshmem; prefer this over the raw constructor
    static std::unique_ptr<TestAshmem> createFrom(const void* data, uint32_t length) {
        // Create ashmem-based fd.
        int fd = ASharedMemory_create(nullptr, length);
        if (fd <= 0) return nullptr;
        ::android::base::unique_fd managedFd(fd);

        // Map and populate ashmem.
        auto mappedFile =
                ::android::base::MappedFile::FromFd(fd, 0, length, PROT_READ | PROT_WRITE);
        if (!mappedFile) return nullptr;
        memcpy(mappedFile->data(), data, length);

        // Create NNAPI memory object.
        test_wrapper::Memory memory(length, PROT_READ | PROT_WRITE, fd, 0);
        if (!memory.isValid()) return nullptr;

        // Store everything in managing class.
        return std::make_unique<TestAshmem>(std::move(managedFd), std::move(mappedFile),
                                            std::move(memory));
    }

    size_t size() { return mMapped->size(); }
    test_wrapper::Memory* get() { return &mMemory; }

    template <typename Type>
    Type* dataAs() {
        return static_cast<Type*>(static_cast<void*>(mMapped->data()));
    }

   private:
    ::android::base::unique_fd mFd;
    std::unique_ptr<::android::base::MappedFile> mMapped;
    test_wrapper::Memory mMemory;
};

}  // namespace android::nn

#endif  // ANDROID_FRAMEWORKS_ML_NN_RUNTIME_TEST_TEST_UTILS_H
