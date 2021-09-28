// Copyright 2016 The Android Open Source Project
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

#ifndef ANDROID_INCLUDE_HARDWARE_GOLDFISH_DMA_H
#define ANDROID_INCLUDE_HARDWARE_GOLDFISH_DMA_H

#include <inttypes.h>

// userspace interface
struct goldfish_dma_context {
	uint64_t mapped_addr;
	uint32_t size;
	int32_t fd;
};

int goldfish_dma_create_region(uint32_t sz, struct goldfish_dma_context* res);

void* goldfish_dma_map(struct goldfish_dma_context* cxt);
int goldfish_dma_unmap(struct goldfish_dma_context* cxt);

void goldfish_dma_write(struct goldfish_dma_context* cxt,
                        const void* to_write,
                        uint32_t sz);

void goldfish_dma_free(goldfish_dma_context* cxt);
uint64_t goldfish_dma_guest_paddr(const struct goldfish_dma_context* cxt);

#endif
