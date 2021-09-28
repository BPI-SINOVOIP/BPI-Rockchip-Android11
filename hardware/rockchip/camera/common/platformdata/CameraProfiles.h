/*
 * Copyright (C) 2013-2017 Intel Corporation
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

#ifndef _CAMERA3_HAL_PROFILE_H_
#define _CAMERA3_HAL_PROFILE_H_

#include <string>
#include <vector>
#include <map>

#include "PlatformData.h"
#include "IPSLConfParser.h"
#include "ItemPool.h"

NAMESPACE_DECLARATION {
class CameraCapInfo;
/**
 * \class CameraProfiles
 *
 * This is the base class for parsing the camera configuration file.
 * The configuration file is xml format.
 * This class will use the expat lib to do the xml parser.
 */
class CameraProfiles {
public:
    virtual ~CameraProfiles();

    virtual status_t init();

    camera_metadata_t *constructDefaultMetadata(int cameraId, int requestTemplate);
    CameraHwType getCameraHwforId(int cameraId);
    const CameraCapInfo *getCameraCapInfo(int cameraId);
    const CameraCapInfo *getCameraCapInfoForXmlCameraId(int xmlCameraId);
    int getXmlCameraId(int cameraId) const;

public: /* types */
    std::map<int, camera_metadata_t *> mStaticMeta;

    // one example: key: 0, value:"ov13858"
    std::map<int, std::string> mCameraIdToSensorName;

protected: /* private types */
    enum DataField {
        FIELD_INVALID = 0,
        FIELD_SUPPORTED_HARDWARE,
        FIELD_ANDROID_STATIC_METADATA,
        FIELD_COMMON
    } mCurrentDataField;

    struct CameraInfo {
        IPSLConfParser* parser;
        std::string hwType;
        int xmlCameraId;
    };

    struct MetaValueRefTable {
        const metadata_value_t* table;
        int tableSize;
    };

protected: /* Constants*/
    static const int BUFFERSIZE = 4*1024;  // For xml file
    static const int METADATASIZE= 4096;
    static const int mMaxConfigNameLength = 64;

protected: /* Methods */
    CameraProfiles(CameraHWInfo *cameraHWInfo);

    static void startElement(void *userData, const char *name, const char **atts);
    static void endElement(void *userData, const char *name);

    // virtual
    virtual void handleAndroidStaticMetadata(const char *name, const char **atts)
    { UNUSED2(name, atts); }
    virtual void handleLinuxStaticMetadata(const char *name, const char **atts)
    { UNUSED2(name, atts); }

    void createConfParser();
    void destroyConfParser();

    status_t addCamera(int cameraId);
    void getDataFromXmlFile(void);
    void checkField(CameraProfiles *profiles, const char *name, const char **atts);
    bool isSensorPresent(std::vector<SensorDriverDescriptor> &detectedSensors,
                         const char* profileName, int cameraId, const char *moduleId) const;
    bool validateStaticMetadata(const char *name, const char **atts);
    const metadata_tag_t* findTagInfo(const char *name, const metadata_tag_t *tagsTable, unsigned int size);
    void handleSupportedHardware(const char *name, const char **atts);
    void handleCommon(const char *name, const char **atts);

    void dumpSupportedHWSection(int cameraId);
    void dumpStaticMetadataSection(int cameraId);
    void dumpCommonSection();
    void dump(void);

    // Helpers
    int convertEnum(void *dest, const char *src, int type,
                    const metadata_value_t *table, int tableLen,
                    void **newDest);
    int parseGenericTypes(const char * src,
                          const metadata_tag_t* tagInfo,
                          int metadataCacheSize,
                          int64_t* metadataCache);
    int parseEnum(const char * src,
                  const metadata_tag_t* tagInfo,
                  int metadataCacheSize,
                  int64_t* metadataCache);
    int parseEnumAndNumbers(const char * src,
                            const metadata_tag_t* tagInfo,
                            int metadataCacheSize,
                            int64_t* metadataCache);
    int parseStreamConfig(const char * src,
            const metadata_tag_t* tagInfo,
            std::vector<MetaValueRefTable> refTables,
            int metadataCacheSize,
            int64_t* metadataCache);
    int parseStreamConfigDuration(const char * src,
            const metadata_tag_t* tagInfo,
            std::vector<MetaValueRefTable> refTables,
            int metadataCacheSize,
            int64_t* metadataCache);
    int parseData(const char * src,
                  const metadata_tag_t* tagInfo,
                  int metadataCacheSize,
                  int64_t* metadataCache);
    int parseAvailableInputOutputFormatsMap(const char * src,
            const metadata_tag_t* tagInfo,
            std::vector<MetaValueRefTable> refTables,
            int metadataCacheSize,
            int64_t* metadataCache);
    int parseSizes(const char * src,
                   const metadata_tag_t* tagInfo,
                   int metadataCacheSize,
                   int64_t* metadataCache);
    int parseRectangle(const char * src,
                       const metadata_tag_t* tagInfo,
                       int metadataCacheSize,
                       int64_t* metadataCache);
    int parseBlackLevelPattern(const char * src,
                               const metadata_tag_t* tagInfo,
                               int metadataCacheSize,
                               int64_t* metadataCache);
    int parseImageFormats(const char * src,
                          const metadata_tag_t* tagInfo,
                          int metadataCacheSize,
                          int64_t* metadataCache);
    int parseAvailableKeys(const char * src,
                           const metadata_tag_t* tagInfo,
                           int metadataCacheSize,
                           int64_t* metadataCache);

    const char *skipWhiteSpace(const char *src);

protected:  /* Members */
    std::string mXmlConfigName;
    int64_t * mMetadataCache;  // for metadata construct
    unsigned mSensorIndex;
    unsigned mXmlSensorIndex;
    unsigned mItemsCount;
    bool mProfileEnd[MAX_CAMERAS];
    CameraHWInfo * mCameraCommon; /* ChromeCameraProfiles has the ownership */
    ItemPool<CameraInfo> mCameraInfoPool;
    // To store the supported HW type for each camera id
    std::map<int, CameraInfo*> mCameraIdToCameraInfo; /* mCameraIdToCameraInfo doesn't has
                                                * the ownership of CameraInfo */
    std::vector<int> mCharacteristicsKeys[MAX_CAMERAS];

    std::vector<SensorDriverDescriptor> mSensorNames;
    bool mUseEntry;
};

} NAMESPACE_DECLARATION_END
#endif // _CAMERA3_HAL_PROFILE_H_
