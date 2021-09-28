/*
 * Copyright (C) 2017 Intel Corporation
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
#ifndef _CAMERA3_HAL_IERRORCALLBACK_H_
#define _CAMERA3_HAL_IERRORCALLBACK_H_

NAMESPACE_DECLARATION {

class IErrorCallback {
public:
    virtual ~IErrorCallback(){};

    virtual status_t deviceError(void) = 0;
};

} NAMESPACE_DECLARATION_END

#endif /* _CAMERA3_HAL_IERRORCALLBACK_H_ */
