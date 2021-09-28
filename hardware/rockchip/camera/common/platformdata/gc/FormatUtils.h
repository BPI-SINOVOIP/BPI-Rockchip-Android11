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
#ifndef COMMON_PLATFORMDATA_GC_FORMATUTILS_H_
#define COMMON_PLATFORMDATA_GC_FORMATUTILS_H_

#include <string>

NAMESPACE_DECLARATION {

namespace graphconfig {
namespace utils {

#define get_fourcc(a, b, c, d) ((uint32_t)(d) | ((uint32_t)(c) << 8) \
                        | ((uint32_t)(b) << 16) | ((uint32_t)(a) << 24))

int32_t getMBusFormat(int32_t commonPixelFormat);
int32_t getMBusFormat(const std::string& bayerOrder, const int32_t bpp);
int32_t getV4L2Format(const int32_t commonPixelFormat);
int32_t getV4L2Format(const std::string& formatName);
int32_t getBpl(int32_t format, int32_t width);
int32_t getBpp(int32_t format);
int32_t getBppFromCommon(int32_t format);
const std::string pixelCode2String(int32_t code);
int32_t pixelCode2fourcc(int32_t code);

bool isRawFormat(int32_t format);
}  // namespace utils
}  // namespace graphconfig

} NAMESPACE_DECLARATION_END

#endif /* COMMON_PLATFORMDATA_GC_FORMATUTILS_H_ */
