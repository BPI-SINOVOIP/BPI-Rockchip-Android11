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

#define LOG_TAG "SysCall"

#include "SysCall.h"

NAMESPACE_DECLARATION {

SysCall::SysCall()
{
}

SysCall::~SysCall()
{
}

int SysCall::open(const char *pathname, int flags)
{
    return ::open(pathname, flags);
}

int SysCall::close(int fd)
{
    return ::close(fd);
}

int SysCall::ioctl(int fd, int request, void *arg)
{
    return ::ioctl(fd, request, (void *)arg);
}

int SysCall::poll(struct pollfd *pfd, nfds_t nfds, int timeout)
{
    return ::poll(pfd, nfds, timeout);
}

} NAMESPACE_DECLARATION_END
