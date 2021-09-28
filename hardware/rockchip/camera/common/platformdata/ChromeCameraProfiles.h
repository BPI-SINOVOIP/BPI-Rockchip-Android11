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

#ifndef _CHROME_CAMERA_PROFILE_H_
#define _CHROME_CAMERA_PROFILE_H_

#include "CameraProfiles.h"

NAMESPACE_DECLARATION {
/**
 * \class ChromeCameraProfiles
 *
 * This class is used to parse the Chrome specific part of the
 * camera configuration file.
 */
class ChromeCameraProfiles : public CameraProfiles {
public:
    explicit ChromeCameraProfiles(CameraHWInfo *cameraHWInfo);
    virtual ~ChromeCameraProfiles();

    virtual status_t init();

private:  /* Methods */
    void getXmlConfigName(void);
    void handleAndroidStaticMetadata(const char *name, const char **atts);

};
} NAMESPACE_DECLARATION_END
#endif // _CHROME_CAMERA_PROFILE_H_
