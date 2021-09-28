/*
 * Copyright (C) 2020 The Android Open Source Project
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

#ifndef ROCKIT_DIRECT_RTLIBDEFINE_H
#define ROCKIT_DIRECT_RTLIBDEFINE_H

#include <stdint.h>

#define ROCKIT_PLAYER_LIB_NAME          "/system/lib/librockit.so"

#define CREATE_PLAYER_FUNC_NAME         "createRockitPlayer"
#define DESTROY_PLAYER_FUNC_NAME        "destroyRockitPlayer"

#define CREATE_METADATA_FUNC_NAME       "createRockitMetaData"
#define DESTROY_METADATA_FUNC_NAME      "destroyRockitMetaData"

#define CREATE_METARETRIEVER_FUNC_NAME  "createRTMetadataRetriever"
#define DESTROY_METARETRIEVER_FUNC_NAME "destroyRTMetadataRetriever"

// rockit player
typedef void * createRockitPlayerFunc();
typedef void   destroyRockitPlayerFunc(void **player);

// rockit meta
typedef void * createRockitMetaDataFunc();
typedef void   destroyRockitMetaDataFunc(void **meta);

// rockit meta data retriever
typedef void * createMetaDataRetrieverFunc();
typedef void   destroyMetaDataRetrieverFunc(void **retriever);

/**************************************************************
 * NOTE:
 * all define below must keep sync with codes of rockit,
 * or will lead to problems
 **************************************************************/


enum RTTrackType {
    RTTRACK_TYPE_UNKNOWN = -1,  // < Usually treated as AVMEDIA_TYPE_DATA
    RTTRACK_TYPE_VIDEO,
    RTTRACK_TYPE_AUDIO,
    RTTRACK_TYPE_DATA,          // < Opaque data information usually continuous
    RTTRACK_TYPE_SUBTITLE,
    RTTRACK_TYPE_ATTACHMENT,    // < Opaque data information usually sparse

    RTTRACK_TYPE_MEDIA,         // this is not a really type of tracks
                                // it means video,audio,subtitle

    RTTRACK_TYPE_MAX
};

typedef enum _ResVideoIdx {
    RES_VIDEO_ROTATION = 0,
} ResVideoIdx;

typedef enum _ResAudioIdx {
    RES_AUDIO_BITRATE = 0,
    RES_AUDIO_BIT_PER_SAMPLE = 1,
} ResAudioIdx;

typedef struct _RockitTrackInfo {
    int32_t  mCodecType;
    int32_t  mCodecID;
    uint32_t mCodecOriginID;
    int32_t  mIdx;

    /* video track features */
    int32_t  mWidth;
    int32_t  mHeight;
    float    mFrameRate;

    /* audio track features*/
    int64_t  mChannelLayout;
    int32_t  mChannels;
    int32_t  mSampleRate;

    /* subtitle track features*/

    /* language */
    char     lang[16];
    char     mine[16];

    bool     mProbeDisabled;
    /* use reserved first when extend this structure */
    int8_t   mReserved[64];
} RockitTrackInfor;

#define RT_VIDEO_FMT_MASK                   0x000f0000
#define RT_VIDEO_FMT_YUV                    0x00000000
#define RT_VIDEO_FMT_RGB                    0x00010000

typedef enum _RTVideoFormat {
    RT_FMT_YUV420SP        = RT_VIDEO_FMT_YUV,         /* YYYY... UV...            */
    RT_FMT_YUV420SP_10BIT,
    RT_FMT_YUV422SP,                                   /* YYYY... UVUV...          */
    RT_FMT_YUV422SP_10BIT,                             ///< Not part of ABI
    RT_FMT_YUV420P,                                    /* YYYY... UUUU... VVVV     */
    RT_FMT_YUV420SP_VU,                                /* YYYY... VUVUVU...        */
    RT_FMT_YUV422P,                                    /* YYYY... UUUU... VVVV     */
    RT_FMT_YUV422SP_VU,                                /* YYYY... VUVUVU...        */
    RT_FMT_YUV422_YUYV,                                /* YUYVYUYV...              */
    RT_FMT_YUV422_UYVY,                                /* UYVYUYVY...              */
    RT_FMT_YUV400SP,                                   /* YYYY...                  */
    RT_FMT_YUV440SP,                                   /* YYYY... UVUV...          */
    RT_FMT_YUV411SP,                                   /* YYYY... UV...            */
    RT_FMT_YUV444SP,                                   /* YYYY... UVUVUVUV...      */
    RT_FMT_YUV_BUTT,
    RT_FMT_RGB565          = RT_VIDEO_FMT_RGB,         /* 16-bit RGB               */
    RT_FMT_BGR565,                                     /* 16-bit RGB               */
    RT_FMT_RGB555,                                     /* 15-bit RGB               */
    RT_FMT_BGR555,                                     /* 15-bit RGB               */
    RT_FMT_RGB444,                                     /* 12-bit RGB               */
    RT_FMT_BGR444,                                     /* 12-bit RGB               */
    RT_FMT_RGB888,                                     /* 24-bit RGB               */
    RT_FMT_BGR888,                                     /* 24-bit RGB               */
    RT_FMT_RGB101010,                                  /* 30-bit RGB               */
    RT_FMT_BGR101010,                                  /* 30-bit RGB               */
    RT_FMT_ARGB8888,                                   /* 32-bit RGB               */
    RT_FMT_ABGR8888,                                   /* 32-bit RGB               */
    RT_FMT_RGB_BUTT,
    RT_FMT_BUTT            = RT_FMT_RGB_BUTT,
} RTVideoFormat;

#endif // ROCKIT_DIRECT_RTLIBDEFINE_H
