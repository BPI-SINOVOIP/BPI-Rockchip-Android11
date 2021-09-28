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

#include "AttributeAccessor.h"
#include "gralloc_buffer_priv.h"

#include <assert.h>
#include <inttypes.h>

#include <sys/mman.h>

#include <log/log.h>

namespace arm {
namespace graphics {
namespace privatebuffer {
namespace V1_0 {
namespace implementation {

using android::hardware::hidl_handle;
using android::hardware::hidl_vec;
using android::hardware::hidl_string;
using android::hardware::Return;
using android::hardware::Void;

AttributeAccessor::AttributeAccessor(void* attr_base, size_t attr_size)
    : attr_base(attr_base)
    , attr_size(attr_size)
{
}

AttributeAccessor::~AttributeAccessor()
{
	munmap(attr_base, attr_size);
}

Return<void> AttributeAccessor::getCropRectangle(getCropRectangle_cb _hidl_cb)
{
	attr_region *attribs = reinterpret_cast<attr_region *>(attr_base);
	CropRectangle region;
	region.top = attribs->crop_top;
	region.left = attribs->crop_left;
	region.width = attribs->crop_width;
	region.height = attribs->crop_height;
	if (attribs->crop_top < 0 && attribs->crop_left < 0 && attribs->crop_width < 0 && attribs->crop_height < 0)
	{
		_hidl_cb(Error::ATTRIBUTE_NOT_SET, region);
	}
	else
	{
		_hidl_cb(Error::NONE, region);
	}
	return Void();
}

Return<Error> AttributeAccessor::setCropRectangle(const CropRectangle &region)
{
	attr_region *attribs = reinterpret_cast<attr_region *>(attr_base);
	attribs->crop_top = region.top;
	attribs->crop_left = region.left;
	attribs->crop_width = region.width;
	attribs->crop_height = region.height;
	return Error::NONE;
}

Return<void> AttributeAccessor::getDataspace(getDataspace_cb _hidl_cb)
{
	attr_region *attribs = reinterpret_cast<attr_region *>(attr_base);
	Dataspace dataspace = static_cast<Dataspace>(attribs->dataspace);
	if (attribs->dataspace == -1)
	{
		_hidl_cb(Error::ATTRIBUTE_NOT_SET, dataspace);
	}
	else
	{
		_hidl_cb(Error::NONE, dataspace);
	}
	return Void();
}

Return<Error> AttributeAccessor::setDataspace(Dataspace dataspace)
{
	attr_region *attribs = reinterpret_cast<attr_region *>(attr_base);
	attribs->dataspace = static_cast<android_dataspace_t>(dataspace);
	return Error::NONE;
}

}  // namespace implementation
}  // namespace V1_0
}  // namespace privatebuffer
}  // namespace graphics
}  // namespace arm
