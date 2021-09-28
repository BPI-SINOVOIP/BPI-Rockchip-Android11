/*
 * Copyright (C) 2019-2020 Arm Limited.
 * SPDX-License-Identifier: Apache-2.0
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

#include "Accessor.h"
#include "AttributeAccessor.h"
#include "mali_gralloc_buffer.h"
#include "mali_gralloc_formats.h"
#include "mali_fourcc.h"

#include <assert.h>
#include <inttypes.h>
#include <sys/mman.h>

#include <log/log.h>
#include "drmutils.h"


namespace arm {
namespace graphics {
namespace privatebuffer {
namespace V1_0 {
namespace implementation {

namespace pb = arm::graphics::privatebuffer::V1_0;

using android::hardware::hidl_handle;
using android::hardware::hidl_vec;
using android::hardware::hidl_string;
using android::hardware::Return;
using android::hardware::Void;
using android::hardware::graphics::common::V1_2::PixelFormat;

static const private_handle_t *getPrivateHandle(const hidl_handle &buffer_handle)
{
	auto hnd = reinterpret_cast<const private_handle_t *>(buffer_handle.getNativeHandle());
	if (private_handle_t::validate(hnd) < 0)
	{
		MALI_GRALLOC_LOGE("Error getting buffer pixel format info. Invalid handle.");
		return nullptr;
	}
	return hnd;
}

Return<void> Accessor::getAllocation(const hidl_handle &buffer_handle, getAllocation_cb _hidl_cb)
{
	const private_handle_t *hnd = getPrivateHandle(buffer_handle);
	if (hnd == nullptr)
	{
		_hidl_cb(pb::Error::BAD_HANDLE, 0, 0);
		return Void();
	}

	_hidl_cb(pb::Error::NONE, hnd->share_fd, hnd->size);
	return Void();
}

Return<void> Accessor::getAllocatedFormat(const hidl_handle &buffer_handle, getAllocatedFormat_cb _hidl_cb)
{
	const private_handle_t *hnd = getPrivateHandle(buffer_handle);
	if (hnd == nullptr)
	{
		_hidl_cb(pb::Error::BAD_HANDLE, 0, 0);
		return Void();
	}

	uint32_t drm_fourcc = drm_fourcc_from_handle(hnd);
	uint64_t drm_modifier = 0;
	if (drm_fourcc != DRM_FORMAT_INVALID)
	{
		drm_modifier |= drm_modifier_from_handle(hnd);
	}
	else
	{
		MALI_GRALLOC_LOGE("Error getting the allocated format: returning DRM_FORMAT_INVALID for 0x%" PRIx64 ".",
		      hnd->alloc_format);
	}

	_hidl_cb(pb::Error::NONE, drm_fourcc, drm_modifier);
	return Void();
}

Return<void> Accessor::getRequestedDimensions(const hidl_handle &buffer_handle, getRequestedDimensions_cb _hidl_cb)
{
	const private_handle_t *hnd = getPrivateHandle(buffer_handle);
	if (hnd == nullptr)
	{
		_hidl_cb(pb::Error::BAD_HANDLE, 0, 0);
		return Void();
	}

	_hidl_cb(pb::Error::NONE, hnd->width, hnd->height);
	return Void();
}

Return<void> Accessor::getRequestedFormat(const hidl_handle &bufferHandle, getRequestedFormat_cb _hidl_cb)
{
	const private_handle_t *hnd = getPrivateHandle(bufferHandle);
	if (hnd == nullptr)
	{
		_hidl_cb(pb::Error::BAD_HANDLE, static_cast<PixelFormat>(0));
		return Void();
	}

	_hidl_cb(pb::Error::NONE, static_cast<PixelFormat>(hnd->req_format));
	return Void();
}

Return<void> Accessor::getUsage(const hidl_handle &buffer_handle, getUsage_cb _hidl_cb)
{
	const private_handle_t *hnd = getPrivateHandle(buffer_handle);
	if (hnd == nullptr)
	{
		_hidl_cb(pb::Error::BAD_HANDLE, static_cast<pb::BufferUsage>(0));
		return Void();
	}

	pb::BufferUsage usage = static_cast<pb::BufferUsage>(hnd->producer_usage | hnd->consumer_usage);
	_hidl_cb(pb::Error::NONE, usage);
	return Void();
}

Return<void> Accessor::getLayerCount(const hidl_handle &buffer_handle, getLayerCount_cb _hidl_cb)
{
	const private_handle_t *hnd = getPrivateHandle(buffer_handle);
	if (hnd == nullptr)
	{
		_hidl_cb(pb::Error::BAD_HANDLE, 0);
		return Void();
	}

	_hidl_cb(pb::Error::NONE, hnd->layer_count);
	return Void();
}

Return<void> Accessor::getPlaneLayout(const hidl_handle &buffer_handle,
                                      getPlaneLayout_cb _hidl_cb)
{
	const private_handle_t *hnd = getPrivateHandle(buffer_handle);
	if (hnd == nullptr)
	{
		_hidl_cb(pb::Error::BAD_HANDLE, NULL);
		return Void();
	}

	std::vector<pb::PlaneLayout> plane_layout;
	for (int i = 0; i < MAX_PLANES && hnd->plane_info[i].byte_stride > 0; ++i)
	{
		pb::PlaneLayout plane;
		plane.offset = hnd->plane_info[i].offset;
		plane.byteStride = hnd->plane_info[i].byte_stride;
		plane.allocWidth = hnd->plane_info[i].alloc_width;
		plane.allocHeight = hnd->plane_info[i].alloc_height;
		plane_layout.push_back(plane);
	}

	_hidl_cb(pb::Error::NONE, plane_layout);
	return Void();
}

Return<void> Accessor::getAttributeAccessor(const hidl_handle& bufferHandle, getAttributeAccessor_cb _hidl_cb)
{
	const private_handle_t *hnd = getPrivateHandle(bufferHandle);
	if (hnd == nullptr)
	{
		_hidl_cb(Error::BAD_HANDLE, NULL);
		return Void();
	}

	void *attr_base = mmap(NULL, hnd->attr_size, PROT_READ | PROT_WRITE, MAP_SHARED, hnd->share_attr_fd, 0);

	if (attr_base == MAP_FAILED)
	{
		_hidl_cb(Error::ATTRIBUTE_ACCESS_FAILED, NULL);
		return Void();
	}

	_hidl_cb(Error::NONE, new AttributeAccessor(attr_base, hnd->attr_size));
	return Void();
}

IAccessor *HIDL_FETCH_IAccessor(const char *)
{
#ifndef PLATFORM_SDK_VERSION
#error "PLATFORM_SDK_VERSION is not defined"
#endif

#if PLATFORM_SDK_VERSION >= 29
	return new Accessor();
#else
	MALI_GRALLOC_LOGE("IAccessor HIDL interface is only supported on Android 10 and above.");
	return nullptr;
#endif
}

}  // namespace implementation
}  // namespace V1_0
}  // namespace privatebuffer
}  // namespace graphics
}  // namespace arm
