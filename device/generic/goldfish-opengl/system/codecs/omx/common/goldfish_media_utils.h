// Copyright 2018 The Android Open Source Project
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

#include <linux/types.h>
#include <stdint.h>

#ifndef GOLDFISH_COMMON_GOLDFISH_DEFS_H
#define GOLDFISH_COMMON_GOLDFISH_DEFS_H

enum class MediaCodecType : __u8 {
    VP8Codec = 0,
    VP9Codec = 1,
    H264Codec = 2,
    Max = 3,
};

enum class MediaOperation : __u8 {
    InitContext = 0,
    DestroyContext = 1,
    DecodeImage = 2,
    GetImage = 3,
    Flush = 4,
    Reset = 5,
    Max = 6,
};

// This class will abstract away the knowledge required to send media codec data
// to the host. The implementation should only need the following information to
// properly send the data:
//   1) Which codec to use (MediaCodecType)
//   2) What operation to perform (MediaOperation)
//
// Example:
//   auto transport = GoldfishMediaTransport::getInstance();
//
class GoldfishMediaTransport {
protected:
    GoldfishMediaTransport() {}

public:
    virtual ~GoldfishMediaTransport() {}

    // Writes a parameter to send to the host. Each parameter will take up
    // 64-bits. |val| is the value of the parameter, and |num| is the parameter
    // number, starting from 0. If |val| is an address, wrap it around
    // offsetOf(), e.g., writeParam(offsetOf((uint64_t)ptr), 2);
    virtual void writeParam(__u64 val, unsigned int num, unsigned int offSetToStartAddr = 0) = 0;
    // Send the operation to perform to the host. At the time of this call, any
    // parameters that the host needs should have already been passed using
    // writeParam().
    virtual bool sendOperation(MediaCodecType codec, MediaOperation op, unsigned int offSetToStartAddr = 0) = 0;
    // Get the address for input. This is usually given the the codec context to
    // write data into for the host to process.
    virtual uint8_t* getInputAddr(unsigned int offSet = 0) const = 0;
    // Get the address for base pointer
    virtual uint8_t* getBaseAddr() const = 0;
    // Get the address for output. This is usually given to the codec context to
    // read data written there by the host.
    virtual uint8_t* getOutputAddr() const = 0;
    // Get the address for return data from the host. The guest codec
    // implementation will have knowledge of how the return data is laid out.
    virtual uint8_t* getReturnAddr(unsigned int offSet = 0) const = 0;
    // Get the offset of an address relative to the starting address of the
    // allocated memory region. Use this for passing pointers from the guest to
    // the host, as the guest address will be translated, thus the offset is the
    // only value of significance.
    virtual __u64 offsetOf(uint64_t addr) const = 0;

    // Get a slot of memory (8 M per slot) for use by a decoder instance.
    // returns -1 for failure; or a slot >=0 on success.
    // as of now, there are only 4 slots for use, each has 8 M, it is up
    // to client on how to use it.
    // 0th slot: [base, base+8M)
    // ...
    // ith slot: [base+8M*i, base+8M*(i+1))
    virtual int getMemorySlot() = 0;

    // Return a slot back to pool. the slot should be valid >=0 and less
    // than the total size of slots. If nobody returns slot timely, the
    // new client could get -1 from getMemorySlot()
    virtual void returnMemorySlot(int slot) = 0;

    static GoldfishMediaTransport* getInstance();
};

__u64 goldfish_create_media_metadata(MediaCodecType codecType,
                                     __u64 metadata);

#endif
