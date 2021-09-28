/*
 *
 * Copyright 2013 rockchip Electronics Co. LTD
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

/*
 * @file       Rockchip_OMX_Baseport.h
 * @brief
 * @author     csy (csy@rock-chips.com)
 * @version    1.0.0
 * @history
 *    2013.11.26 : Create
 */

#ifndef ROCKCHIP_OMX_BASE_PORT
#define ROCKCHIP_OMX_BASE_PORT


#include "OMX_Component.h"
#include "Rockchip_OMX_Def.h"
#include "Rockchip_OSAL_Queue.h"


#define BUFFER_STATE_ALLOCATED  (1 << 0)
#define BUFFER_STATE_ASSIGNED   (1 << 1)
#define HEADER_STATE_ALLOCATED  (1 << 2)
#define BUFFER_STATE_FREE        0

#define MAX_BUFFER_NUM          40

#define INPUT_PORT_INDEX    0
#define OUTPUT_PORT_INDEX   1
#define ALL_PORT_INDEX     -1
#define ALL_PORT_NUM        2


typedef struct _ROCKCHIP_OMX_BUFFERHEADERTYPE {
    OMX_BUFFERHEADERTYPE *OMXBufferHeader;
    OMX_BOOL              bBufferInOMX;
    OMX_HANDLETYPE        ANBHandle;
    void                 *pYUVBuf[MAX_BUFFER_PLANE];
    int                   buf_fd[MAX_BUFFER_PLANE];
    int                   pRegisterFlag;
    OMX_PTR               pPrivate;
} ROCKCHIP_OMX_BUFFERHEADERTYPE;

typedef struct _ROCKCHIP_OMX_DATABUFFER {
    OMX_HANDLETYPE        bufferMutex;
    OMX_BUFFERHEADERTYPE* bufferHeader;
    OMX_BOOL              dataValid;
    OMX_U32               allocSize;
    OMX_U32               dataLen;
    OMX_U32               usedDataLen;
    OMX_U32               remainDataLen;
    OMX_U32               nFlags;
    OMX_TICKS             timeStamp;
    OMX_PTR               pPrivate;
} ROCKCHIP_OMX_DATABUFFER;

typedef void* CODEC_EXTRA_BUFFERINFO;

typedef struct _ROCKCHIP_OMX_SINGLEPLANE_DATA {
    OMX_PTR dataBuffer;
    int fd;
} ROCKCHIP_OMX_SINGLEPLANE_DATA;

typedef struct _ROCKCHIP_OMX_MULTIPLANE_DATA {
    OMX_U32 validPlaneNum;
    OMX_PTR dataBuffer[MAX_BUFFER_PLANE];
    int fd[MAX_BUFFER_PLANE];
} ROCKCHIP_OMX_MULTIPLANE_DATA;

typedef struct _ROCKCHIP_OMX_DATA {
    union {
        ROCKCHIP_OMX_SINGLEPLANE_DATA singlePlaneBuffer;
        ROCKCHIP_OMX_MULTIPLANE_DATA multiPlaneBuffer;
    } buffer;
    OMX_U32   allocSize;
    OMX_U32   dataLen;
    OMX_U32   usedDataLen;
    OMX_U32   remainDataLen;
    OMX_U32   nFlags;
    OMX_TICKS timeStamp;
    OMX_PTR   pPrivate;
    CODEC_EXTRA_BUFFERINFO extInfo;

    /* For Share Buffer */
    OMX_BUFFERHEADERTYPE* bufferHeader;
} ROCKCHIP_OMX_DATA;

typedef struct _ROCKCHIP_OMX_WAY1_PORT_DATABUFFER {
    ROCKCHIP_OMX_DATABUFFER dataBuffer;
} ROCKCHIP_OMX_PORT_1WAY_DATABUFFER;

typedef struct _ROCKCHIP_OMX_WAY2_PORT_DATABUFFER {
    ROCKCHIP_OMX_DATABUFFER inputDataBuffer;
    ROCKCHIP_OMX_DATABUFFER outputDataBuffer;
} ROCKCHIP_OMX_PORT_2WAY_DATABUFFER;

typedef enum _ROCKCHIP_OMX_PORT_WAY_TYPE {
    WAY1_PORT = 0x00,
    WAY2_PORT
} ROCKCHIP_OMX_PORT_WAY_TYPE;

typedef enum _ROCKCHIP_OMX_EXCEPTION_STATE {
    GENERAL_STATE = 0x00,
    NEED_PORT_FLUSH,
    NEED_PORT_DISABLE,
    INVALID_STATE,
} ROCKCHIP_OMX_EXCEPTION_STATE;

typedef enum _ROCKCHIP_OMX_PLANE {
    ONE_PLANE       = 0x01,
    TWO_PLANE       = 0x02,
    THREE_PLANE     = 0x03,
    /*
        ANB_START_PLANE = 0x10,
        ANB_ONE_PLANE   = 0x11,
        ANB_TWO_PLANE   = 0x12,
        ANB_THREE_PLANE = 0x13,
    */
} ROCKCHIP_OMX_PLANE;

typedef struct _ROCKCHIP_OMX_BASEPORT {
    ROCKCHIP_OMX_BUFFERHEADERTYPE   *extendBufferHeader;
    OMX_U32                       *bufferStateAllocate;
    OMX_PARAM_PORTDEFINITIONTYPE   portDefinition;
    OMX_HANDLETYPE                 bufferSemID;
    ROCKCHIP_QUEUE                 bufferQ;
    ROCKCHIP_QUEUE                 securebufferQ;
    OMX_U32                        assignedBufferNum;
    OMX_STATETYPE                  portState;
    OMX_HANDLETYPE                 loadedResource;
    OMX_HANDLETYPE                 unloadedResource;

    OMX_BOOL                       bIsPortFlushed;
    OMX_BOOL                       bIsPortDisabled;
    OMX_MARKTYPE                   markType;

    OMX_CONFIG_RECTTYPE            cropRectangle;

    /* Tunnel Info */
    OMX_HANDLETYPE                 tunneledComponent;
    OMX_U32                        tunneledPort;
    OMX_U32                        tunnelBufferNum;
    OMX_BUFFERSUPPLIERTYPE         bufferSupplier;
    OMX_U32                        tunnelFlags;

    OMX_BOOL                       bStoreMetaData;

    ROCKCHIP_OMX_BUFFERPROCESS_TYPE  bufferProcessType;
    ROCKCHIP_OMX_PORT_WAY_TYPE       portWayType;
    OMX_HANDLETYPE                 codecSemID;
    ROCKCHIP_QUEUE                   codecBufferQ;

    OMX_HANDLETYPE                 pauseEvent;

    /* Buffer */
    union {
        ROCKCHIP_OMX_PORT_1WAY_DATABUFFER port1WayDataBuffer;
        ROCKCHIP_OMX_PORT_2WAY_DATABUFFER port2WayDataBuffer;
    } way;

    /* Data */
    ROCKCHIP_OMX_DATA                processData;

    /* for flush of Shared buffer scheme */
    OMX_HANDLETYPE                 hAllCodecBufferReturnEvent;
    OMX_HANDLETYPE                 hPortMutex;
    OMX_HANDLETYPE                 secureBufferMutex;
    ROCKCHIP_OMX_EXCEPTION_STATE     exceptionFlag;

    OMX_PARAM_PORTDEFINITIONTYPE   newPortDefinition;
    OMX_CONFIG_RECTTYPE            newCropRectangle;
} ROCKCHIP_OMX_BASEPORT;


#ifdef __cplusplus
extern "C" {
#endif

OMX_ERRORTYPE Rockchip_OMX_PortEnableProcess(OMX_COMPONENTTYPE *pOMXComponent, OMX_S32 nPortIndex);
OMX_ERRORTYPE Rockchip_OMX_PortDisableProcess(OMX_COMPONENTTYPE *pOMXComponent, OMX_S32 nPortIndex);
OMX_ERRORTYPE Rockchip_OMX_BufferFlushProcess(OMX_COMPONENTTYPE *pOMXComponent, OMX_S32 nPortIndex, OMX_BOOL bEvent);
OMX_ERRORTYPE Rockchip_OMX_Port_Constructor(OMX_HANDLETYPE hComponent);
OMX_ERRORTYPE Rockchip_OMX_Port_Destructor(OMX_HANDLETYPE hComponent);
OMX_ERRORTYPE Rockchip_ResetDataBuffer(ROCKCHIP_OMX_DATABUFFER *pDataBuffer);
OMX_ERRORTYPE Rockchip_ResetCodecData(ROCKCHIP_OMX_DATA *pData);
OMX_ERRORTYPE Rockchip_Shared_BufferToData(ROCKCHIP_OMX_DATABUFFER *pUseBuffer, ROCKCHIP_OMX_DATA *pData, ROCKCHIP_OMX_PLANE nPlane);
OMX_ERRORTYPE Rockchip_Shared_DataToBuffer(ROCKCHIP_OMX_DATA *pData, ROCKCHIP_OMX_DATABUFFER *pUseBuffer);
OMX_ERRORTYPE Rkvpu_OMX_InputBufferReturn(OMX_COMPONENTTYPE *pOMXComponent, OMX_BUFFERHEADERTYPE* bufferHeader);
OMX_ERRORTYPE Rockchip_OMX_OutputBufferReturn(OMX_COMPONENTTYPE *pOMXComponent, OMX_BUFFERHEADERTYPE* bufferHeader);
#ifdef __cplusplus
};
#endif


#endif
