/*
 * Copyright (C) 2012-2017 Intel Corporation
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

#include <memory>
#include "ExifCreater.h"
#include "3ATypes.h"
#include "CameraMetadata.h"
#include "EXIFMetaData.h"


#ifndef _EXIFMAKER_H_
#define _EXIFMAKER_H_

NAMESPACE_DECLARATION {
/**
 * \class EXIFMaker
 *
 */
class EXIFMaker {

public:
    EXIFMaker();
    ~EXIFMaker();

    void initialize(int width, int height);
    bool isInitialized() { return initialized; }
    void initializeLocation(ExifMetaData& metadata);
    void setMakerNote(const ia_binary_data &aaaMkNoteData);
    uint32_t getMakerNoteDataSize() const;
    void setDriverData(const ExifMetaData::MakernoteType &ispData);
    void setSensorAeConfig(const SensorAeConfig &aeConfig);
    void pictureTaken(ExifMetaData& exifmetadata);
    void enableFlash(bool enable, int8_t aeMode, int8_t flashMode);
    void setThumbnail(unsigned char *data, size_t size, int width, int height);
    bool isThumbnailSet() const;
    size_t makeExif(unsigned char **data);
    size_t makeExifInPlace(unsigned char *bufferStartAddr,
                           unsigned char *dqtAddress,
                           size_t jpegSize,
                           bool usePadding);
    void setMaker(const char *data);
    void setModel(const char *data);
    void setSoftware(const char *data);

    void getExifAttrbutes(exif_attribute_t &exifAttrs);

private: // member variables
    ExifCreater encoder;
    exif_attribute_t exifAttributes;
    size_t exifSize;
    bool initialized;

// prevent copy constructor and assignment operator
private:
    EXIFMaker(const EXIFMaker& other);
    EXIFMaker& operator=(const EXIFMaker& other);

private: // Methods
    void copyAttribute(uint8_t *dst,
                       size_t dstSize,
                       const char *src,
                       size_t srcLength);
    void clear();
};
} NAMESPACE_DECLARATION_END
#endif // _EXIFMAKER_H_
