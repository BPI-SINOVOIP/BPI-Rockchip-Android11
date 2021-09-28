/*
 * Copyright (C) 2016-2017 Intel Corporation.
 * Copyright (c) 2017, Fuzhou Rockchip Electronics Co., Ltd
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

#ifndef _COMMON_SYSCALL_H_
#define _COMMON_SYSCALL_H_

#include <fcntl.h>
#include <poll.h>
#include <unistd.h>
#include <sys/ioctl.h>

NAMESPACE_DECLARATION {
class SysCall
{
public:
    SysCall();
    ~SysCall();

    static int open(const char *pathname, int flags);
    static int close(int fd);
    static int ioctl(int fd, int request, void *arg);
    static int poll(struct pollfd *pfd, nfds_t nfds, int timeout);
};
} NAMESPACE_DECLARATION_END

#endif // _COMMON_SYSCALL_H_
