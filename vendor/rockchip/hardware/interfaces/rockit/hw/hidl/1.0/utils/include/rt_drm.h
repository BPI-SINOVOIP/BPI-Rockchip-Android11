/*
 * Copyright 2019 Rockchip Electronics Co. LTD
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
 *
 * author: rimon.xu@rock-chips.com
 *   date: 2019/01/16
 * module: drm native api
 */

#ifndef SRC_RT_DRM_H_
#define SRC_RT_DRM_H_


int drm_open();
int drm_close(int fd);
int drm_ioctl(int fd, int req, void* arg);
int drm_get_phys(int fd, uint32_t  handle, uint32_t *phy, uint32_t heaps);
int drm_handle_to_fd(int fd, uint32_t handle, int *map_fd, uint32_t flags);
int drm_fd_to_handle(int fd, int map_fd, uint32_t *handle, uint32_t flags);
int drm_alloc(int  fd, uint32_t len, uint32_t align, uint32_t *handle, uint32_t flags, uint32_t heaps);
int drm_free(int  fd, uint32_t handle);
int drm_get_info_from_name(int fd, uint32_t name, uint32_t *handle, int *size);

void *drm_mmap(void *addr, uint32_t length, int  prot, int  flags, int  fd, loff_t offset);
int drm_munmap(void *addr, int length);

int drm_map(int  fd,
               uint32_t handle,
               uint32_t length,
               int  prot,
               int  flags,
               int  offset,
               void **ptr,
               int *map_fd,
               uint32_t heaps);

#endif  // SRC_RT_DRM_H_