/*
 * Copyright 2018 Rockchip Electronics Co. LTD
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
 * author: xhr@rock-chips.com
 * date: 2019/12/24
 */

#ifndef ROCKIT_DIRECT_RTSUBTITLESINK_H_
#define ROCKIT_DIRECT_RTSUBTITLESINK_H_

#include "RTSubtitleSinkInterface.h"

#include <EGL/egl.h>           // NOLINT
#include <GLES/gl.h>           // NOLINT
#include <GLES/glext.h>        // NOLINT

#include <binder/IPCThreadState.h>
#include <gui/IGraphicBufferProducer.h>
#include <gui/Surface.h>
#include <gui/SurfaceComposerClient.h>
#include <gui/ISurfaceComposer.h>

using namespace android;

enum SUBTITLE_DISPLAY_FORMAT {
    DISPLAY_ALL = 0,
    DISPLAY_LEFT_HALF = 1,
    DISPLAY_TOP_HALF = 2
};

class SurfaceRect
{
public:
    int x;
    int y;
    int width;
    int height;
    int rotation;

    SurfaceRect() {
        x = y = 0;
        width = 1280;
        height = 720;
        rotation = 0;  //  DISPLAY_ORIENTATION_0;
    }
};

class RTSubteSink : public RTSubtitleSinkInterface {
 public:
                        RTSubteSink();
    virtual            ~RTSubteSink();

    virtual void        create(int renderType, int display);
    virtual void        destroy();
    virtual void        initScene();
    virtual void        showScene();
    virtual void        render(RTSubFrame *frame);
    virtual void        clean();
    virtual void        show();
    virtual void        hide();
    virtual bool        isShowing();

 private:
    void                createSubitleSurface();
    void                createEGLSurface();
    void                destoryEGLSurface();
    bool                bindSurfaceToThread();

    int                 initTexture(void* data,int width,int height,GLuint& texture);
    void                gpuRender(RTSubFrame *frame);
    int                 setLayerStack(int display);
    int                 setSubtitleSurfaceZOrder(int order);
    int                 setSubtitleSurfacePosition(int x,int y,int width,int height);
    void                setSubtitleSurfaceVisibility(int visible);
    void                displayPgsSubtitle();
    int                 getSurfaceZOrder();
    SurfaceRect&        getSurfaceRect();
    void                setSubtitleMode(int mode);
    void                setHdmiMode(int mode);
    bool                checkRotation();
    void                getSurfaceMaxWidthAndHeight(DisplayConfig& config,ui::DisplayState& state,int displayId);

private:
    SurfaceRect                  mRect;
    sp<SurfaceComposerClient>    mClient;
    sp<SurfaceControl>           mSurfaceControl;
    sp<Surface>                  mSurface;

    int                          mRenderType;

    EGLDisplay                   mDisplay;
    EGLContext                   mContext;
    EGLSurface                   mEGLSurface;

    int                          mSubtitleZOrder;
    int                          mDisplayDev;
    int                          mDisplayMode;
    int                          mHdmiMode;
    bool                         mSurfaceShow;
    bool                         mBindThread;
    bool                         mInitialized;
    Mutex                        mRenderLock;
};

#endif  // ROCKIT_DIRECT_RTSUBTITLESINK_H_

