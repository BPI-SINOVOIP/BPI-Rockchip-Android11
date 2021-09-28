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
 * date:   2019/12/24
 */

#include "RTSubtitleSink.h"
#include "RTGraphicWindowApi.h"
#include "HdmiDefine.h"

#include <ui/DisplayConfig.h>
#include <ui/DisplayState.h>
#include <input/DisplayViewport.h>
#include <utils/Log.h>
#include <cutils/properties.h> // for property_get

#ifdef LOG_TAG
#undef LOG_TAG
#endif
#define LOG_TAG "SubtitleSink"

#ifdef DEBUG_FLAG
#undef DEBUG_FLAG
#endif
#define DEBUG_FLAG 0x0

using namespace ::android;

RTSubteSink::RTSubteSink() {
    mInitialized = false;
}

RTSubteSink::~RTSubteSink() {
    destroy();
}

void RTSubteSink::create(int renderType, int display) {
    if (mInitialized) {
        return;
    }

    mDisplay    = EGL_NO_DISPLAY;
    mEGLSurface = EGL_NO_SURFACE;
    mContext    = EGL_NO_CONTEXT;

    mRenderType = renderType;
    mClient = NULL;
    mSurfaceControl = NULL;
    mSubtitleZOrder = INT_MAX-2;
    mSurfaceShow = false;

    mDisplayMode    = DISPLAY_ALL;
    mDisplayDev     = display;
    mBindThread     = false;
    mHdmiMode       = HDMI_3D_NONE;

    createSubitleSurface();
    if(mSurface != NULL) {
        createEGLSurface();
    }

    if (mEGLSurface != EGL_NO_SURFACE) {
        mInitialized = true;
        show();
    }
}

void RTSubteSink::destroy() {
    if (mInitialized) {
        clean();
        destoryEGLSurface();
    }

    if (mClient != NULL) {
        mClient->dispose();
        mClient = NULL;
    }

    mInitialized = false;
}

void RTSubteSink::createEGLSurface() {
    Mutex::Autolock autoLock(mRenderLock);
    if ((mSurfaceControl == NULL) || (mSurfaceControl->getSurface() == NULL)) {
        return ;
    }

    EGLint defaultConfigAttribs[] = {
        EGL_RED_SIZE, 8,
        EGL_GREEN_SIZE, 8,
        EGL_BLUE_SIZE, 8,
        EGL_ALPHA_SIZE, 8,
        EGL_DEPTH_SIZE, 0,
        EGL_NONE
    };

    EGLint w, h;
    EGLint numConfigs;
    EGLConfig config;

    mDisplay = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (mDisplay == EGL_NO_DISPLAY) {
        ALOGE("eglGetDisplay error: %#x\n", eglGetError());
    }

    EGLint majorVersion;
    EGLint minorVersion;
    eglInitialize(mDisplay, &majorVersion, &minorVersion);
    eglChooseConfig(mDisplay,defaultConfigAttribs , &config, 1, &numConfigs);//attribs
    mEGLSurface = eglCreateWindowSurface(mDisplay, config,mSurface.get(), NULL);
    if (mEGLSurface == EGL_NO_SURFACE) {
        ALOGE("eglCreateWindowSurface error: %#x\n", eglGetError());
        return;
    }

    mContext = eglCreateContext(mDisplay, config, EGL_NO_CONTEXT, NULL);
    if (mContext == EGL_NO_CONTEXT) {
        ALOGE("eglCreateWindowSurface error: %#x\n", eglGetError());
        return;
    }

    eglQuerySurface(mDisplay, mEGLSurface, EGL_WIDTH, &w);
    eglQuerySurface(mDisplay, mEGLSurface, EGL_HEIGHT, &h);

    mRect.width = w;
    mRect.height = h;
    ALOGD("create egl surface(width=%d, height=%d)", w, h);
}

void RTSubteSink::destoryEGLSurface() {
    Mutex::Autolock autoLock(mRenderLock);
    if(mRenderType == RENDER_RGA) {
        return ;
    }
    ALOGD("destoryEGLSurface");
    if (mClient != NULL) {
        mClient->dispose();
    }

    if (mDisplay != EGL_NO_DISPLAY) {
        eglMakeCurrent(mDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    }

    if (mContext != EGL_NO_CONTEXT) {
        eglDestroyContext(mDisplay, mContext);
    }

    if (mEGLSurface != EGL_NO_SURFACE) {
        eglDestroySurface(mDisplay, mEGLSurface);
    }

    if (mSurface != NULL) {
        mSurface.clear();
        mSurface = NULL;
    }

    if (mDisplay != EGL_NO_DISPLAY) {
        eglTerminate(mDisplay);
    }

    mContext = EGL_NO_CONTEXT;
    mEGLSurface = EGL_NO_SURFACE;
    mDisplay = EGL_NO_DISPLAY;
}

bool RTSubteSink::bindSurfaceToThread() {
    Mutex::Autolock autoLock(mRenderLock);
    if (mRenderType == RENDER_RGA) {
        return false;
    }
    if (mEGLSurface != EGL_NO_SURFACE) {
        if (eglMakeCurrent(mDisplay, mEGLSurface, mEGLSurface, mContext) == EGL_FALSE) {
            ALOGD("failed to bind surface to thread");
            return false;
        }
        mBindThread = true;
        ALOGD("succeed to bind surface to thread");
        return true;
    }
    ALOGD("failed to bind surface to thread");
    return false;
}

void RTSubteSink::initScene() {
    Mutex::Autolock autoLock(mRenderLock);
    if ((mRenderType == RENDER_GPU) && (mEGLSurface != EGL_NO_SURFACE)) {
        glDisable(GL_DITHER);
        glEnable(GL_CULL_FACE);
        glViewport(0, 0, mRect.width, mRect.height);
        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();

        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();
        glOrthof(0,mRect.width,0,mRect.height,0,1);

        glEnable(GL_TEXTURE_2D);
        glEnableClientState(GL_VERTEX_ARRAY);
        glEnableClientState(GL_TEXTURE_COORD_ARRAY);

        glClearColor(0.0, 0.0, 0.0, 0.0);
        glClear(GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT);
    }
}

void RTSubteSink::showScene() {
    if (!mInitialized) {
        return;
    }

    if (mRenderType == RENDER_GPU) {
        if (mEGLSurface != EGL_NO_SURFACE) {
            eglSwapBuffers(mDisplay, mEGLSurface);
        }
    }
}

void RTSubteSink::render(RTSubFrame *frame) {
    if (!mInitialized) {
        return;
    }

    if(!mBindThread) {
        bindSurfaceToThread();
    }
    if(mRenderType == RENDER_GPU) {
        gpuRender(frame);
    } else {
        ALOGE("render: not support");
    }
}

void RTSubteSink::gpuRender(RTSubFrame *frame) {
    if (mEGLSurface == EGL_NO_SURFACE) {
        return;
    }

    Mutex::Autolock autoLock(mRenderLock);
    if (frame != NULL && frame->data != NULL) {
        GLuint   texture;
        int x = frame->x;
        int y = frame->y;
        int width = frame->width;
        int height = frame->height;

        initTexture(frame->data,width,height,texture);
        float widthScale = ((float)mRect.width/(float)frame->subWidth);
        float heightScale = ((float)mRect.height/(float)frame->subHeight);

#if 0
        if (mHdmiManager != NULL) {
            mode = mHdmiManager->getCurrentMode();
        }
#endif
        if (mHdmiMode == HDMI_3D_SIDE_BY_SIDE_HALT) {
            widthScale = widthScale/2;
        } else if (mHdmiMode == HDMI_3D_TOP_BOTTOM) {
            heightScale = heightScale/2;
        }

        float realWidth = ((float)width)*widthScale;
        float realHight = ((float)height)*heightScale;
        float realX = ((float)x)*widthScale;
        float realY = ((float)(frame->subHeight - y))*heightScale-realHight;

        if (realY < 0) {
            realY = 20;
            if (realX*2 + realWidth > mRect.width) {
                realX = (mRect.width - realWidth) / 2;
            }

            if (y + height > frame->subHeight) {
                realHight = height * (widthScale >= 2 ? 2 : 1);
            }
            ALOGD("gpuRender origin coordinate: x = %d,y = %d,width = %d,height = %d", x, y, width, height);
        }
        GLfloat texCoords[] = {
            0,    1,
            1.0,  1,
            1.0,  0,
            0,    0
        };

        bool changed = false;
        if (frame->needcrop && (mDisplayMode == DISPLAY_LEFT_HALF)) {
            float texX = 1.0f;
            if ((realX < (mRect.width/2)) && ((realX+realWidth) > (mRect.width/2))) {
                texX = (((float)(mRect.width/2)-realX))/realWidth;
                realX *= 2;
                realWidth *= 2;
                changed = true;
            }
            texCoords[2]  = texX;//0.5;
            texCoords[4]  = texX;//0.5;
        } else if (frame->needcrop && (mDisplayMode == DISPLAY_TOP_HALF)) {
            float texY= 1.0f;
            if ((realY < (mRect.height/2)) && ((realY+realHight) > (mRect.height/2))) {
                texY = 0.5f;
                changed = true;
            }
            texCoords[1]  = texY;
            texCoords[3]  = texY;
        }

        const GLushort indices[] = { 0, 1, 2,  0, 2, 3 };
        int nelem = sizeof(indices)/sizeof(indices[0]);

        // 2D
        if (mHdmiMode == HDMI_3D_NONE) {
            GLfloat vertices[] = {
                realX,           realY,            0,
                realX+realWidth, realY,            0,
                realX+realWidth, realY+realHight,  0,
                realX,           realY+realHight,  0
            };

            if (changed) {
                if (mDisplayMode == DISPLAY_LEFT_HALF) {
                    vertices[3] = (realX+realWidth)/2;
                    vertices[6] = (realX+realWidth)/2;
                } else if (mDisplayMode == DISPLAY_TOP_HALF) {
                    vertices[7] = realY + realHight/2;
                    vertices[10] = realY + realHight/2;
                }
            }

            glVertexPointer(3, GL_FLOAT, 0, vertices);
            glTexCoordPointer(2, GL_FLOAT, 0, texCoords);
            glDrawElements(GL_TRIANGLES, nelem, GL_UNSIGNED_SHORT, indices);
        } else if (mHdmiMode == HDMI_3D_SIDE_BY_SIDE_HALT) {
            float realLeftX = ((float)x)*widthScale;
            float realRightX = ((float)x)*widthScale+mRect.width/2;

            const GLfloat leftVertices[] = {
                realLeftX,           realY,           0,
                realLeftX+realWidth, realY,           0,
                realLeftX+realWidth, realY+realHight, 0,
                realLeftX,           realY+realHight, 0
            };

            const GLfloat RightVertices[] = {
                realRightX,           realY,           0,
                realRightX+realWidth, realY,           0,
                realRightX+realWidth, realY+realHight, 0,
                realRightX,           realY+realHight, 0
            };

            // left
            glVertexPointer(3, GL_FLOAT, 0, leftVertices);
            glTexCoordPointer(2, GL_FLOAT, 0, texCoords);
            glDrawElements(GL_TRIANGLES, nelem, GL_UNSIGNED_SHORT, indices);

            // right
            glVertexPointer(3, GL_FLOAT, 0, RightVertices);
            glTexCoordPointer(2, GL_FLOAT, 0, texCoords);
            glDrawElements(GL_TRIANGLES, nelem, GL_UNSIGNED_SHORT, indices);
        } else if (mHdmiMode == HDMI_3D_TOP_BOTTOM) {
            float realTopY = (float)mRect.height/2 - ((float)y)*heightScale - realHight;
            float realBottomY = (float)mRect.height - ((float)y)*heightScale - realHight;
            const GLfloat topVertices[] = {
                realX,           realTopY,           0,
                realX+realWidth, realTopY,           0,
                realX+realWidth, realTopY+realHight, 0,
                realX,           realTopY+realHight, 0
            };

            const GLfloat bottomVertices[] = {
                realX,           realBottomY,           0,
                realX+realWidth, realBottomY,           0,
                realX+realWidth, realBottomY+realHight, 0,
                realX,           realBottomY+realHight, 0
            };

            // top
            glVertexPointer(3, GL_FLOAT, 0, topVertices);
            glTexCoordPointer(2, GL_FLOAT, 0, texCoords);
            glDrawElements(GL_TRIANGLES, nelem, GL_UNSIGNED_SHORT, indices);

            // bottom
            glVertexPointer(3, GL_FLOAT, 0, bottomVertices);
            glTexCoordPointer(2, GL_FLOAT, 0, texCoords);
            glDrawElements(GL_TRIANGLES, nelem, GL_UNSIGNED_SHORT, indices);
        }

        glDeleteTextures(1, &texture);
    }
}

int RTSubteSink::initTexture(void* data,int width,int height,GLuint& texture) {
    GLint crop[4] = { 0, height, width, -height };

    glEnable(GL_TEXTURE_2D);
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);

    glEnableClientState(GL_VERTEX_ARRAY);
    glEnableClientState(GL_TEXTURE_COORD_ARRAY);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
    glTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_CROP_RECT_OES, crop);

    glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
    glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
   #if 0
    if (PlatformInstance->shouldGlSwap()) {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    }
  #endif
    return NO_ERROR;
}

void RTSubteSink::clean() {
    if (!mInitialized) {
        return;
    }

    if (mRenderType == RENDER_RGA) {
        ANativeWindow_Buffer mSurfaceInfo;
        unsigned char* mSurfaceBuf  = NULL;

        ARect dirtyregion;
        dirtyregion.left = 0;
        dirtyregion.right = 0x3fff;
        dirtyregion.top = 0;
        dirtyregion.bottom = 0x3fff;

        int status = mSurface->lock(&mSurfaceInfo, &dirtyregion);
        if (status != 0) {
            return ;
        }

        mSurfaceBuf = reinterpret_cast<unsigned char*>(mSurfaceInfo.bits);
        if (mSurfaceBuf != NULL) {
            memset(mSurfaceBuf,0, mSurfaceInfo.width* mSurfaceInfo.height*sizeof(int));
        }

        mSurface->unlockAndPost();
    } else if (mRenderType == RENDER_GPU) {
        if (mEGLSurface != EGL_NO_SURFACE) {
            initScene();
            eglSwapBuffers(mDisplay, mEGLSurface);
        }
    }
}

void RTSubteSink::show() {
    if (!mInitialized || mSurfaceShow) {
        return;
    }

    Mutex::Autolock autoLock(mRenderLock);
    if (mSurfaceControl != NULL) {
        GraphicWindowApi::OpenSurfaceTransaction();
        Transaction t;
        GraphicWindowApi::ShowSurface(mSurfaceControl, &t);
        GraphicWindowApi::CloseSurfaceTransaction(&t);
        mSurfaceShow = true;
    }
}

void RTSubteSink::hide() {
    if (!mInitialized || !mSurfaceShow) {
        return;
    }

    Mutex::Autolock autoLock(mRenderLock);
    if (mSurfaceControl != NULL) {
        GraphicWindowApi::OpenSurfaceTransaction();
        Transaction t;
        GraphicWindowApi::HideSurface(mSurfaceControl, &t);
        GraphicWindowApi::CloseSurfaceTransaction(&t);
        mSurfaceShow = false;
    }
}

bool RTSubteSink::isShowing() {
    return mSurfaceShow;
}

void RTSubteSink::createSubitleSurface() {
    if(mClient == NULL) {
        mClient = new SurfaceComposerClient();
        if (mClient != NULL) {
            DisplayConfig config;
            ui::DisplayState state;
            getSurfaceMaxWidthAndHeight(config,state, 0);
            // create the native surface
            mSurfaceControl = mClient->createSurface(String8("SubtitleSurface"),config.resolution.getWidth(),config.resolution.getHeight(),PIXEL_FORMAT_RGBA_8888);
            if (mSurfaceControl != NULL) {
                GraphicWindowApi::OpenSurfaceTransaction();
                Transaction t;
                GraphicWindowApi::SetSurfaceLayer(mSurfaceControl, &t, mSubtitleZOrder);
                GraphicWindowApi::SetSurfacePosition(mSurfaceControl, &t, 0, 0);
                GraphicWindowApi::SetSurfaceSize(mSurfaceControl, &t,config.resolution.getWidth(),config.resolution.getHeight());
                GraphicWindowApi::CloseSurfaceTransaction(&t);
                mSurface = mSurfaceControl->getSurface();

                mRect.x = 0;
                mRect.y = 0;
                mRect.width = config.resolution.getWidth();
                mRect.height = config.resolution.getHeight();
                mRect.rotation = (int)state.orientation;
            } else {
                ALOGE("createSubitleSurface:mSurfaceControl == NULL");
            }
        } else {
            ALOGE("createSubitleSurface:mClient == NULL");
        }
    }
}

int RTSubteSink::setLayerStack(int display) {
    int status = -1;
    Mutex::Autolock autoLock(mRenderLock);
    if (mSurfaceControl != NULL) {
        GraphicWindowApi::OpenSurfaceTransaction();
        Transaction t;
        GraphicWindowApi::SetSurfaceLayerStack(mSurfaceControl, &t, display);
        GraphicWindowApi::CloseSurfaceTransaction(&t);
        status = 0;
    }

    return status;
}

int RTSubteSink::setSubtitleSurfaceZOrder(int order) {
    Mutex::Autolock autoLock(mRenderLock);
    if(mSubtitleZOrder != order) {
        if (mClient != NULL && mSurfaceControl != NULL) {
            GraphicWindowApi::OpenSurfaceTransaction();
            Transaction t;
            GraphicWindowApi::SetSurfaceLayer(mSurfaceControl, &t, order);
            mSubtitleZOrder = order;
            GraphicWindowApi::CloseSurfaceTransaction(&t);
            return 0;
        }
    }

    return -1;
}

int RTSubteSink::setSubtitleSurfacePosition(int x,int y,int width,int height) {
    Mutex::Autolock autoLock(mRenderLock);
    if (mClient != NULL && mSurfaceControl != NULL) {
        DisplayConfig config;
        ui::DisplayState state;
        getSurfaceMaxWidthAndHeight(config,state,0);

        int maxWidth = config.resolution.getWidth();
        int maxHeight = config.resolution.getHeight();
        maxWidth = (maxWidth>=width)?width:maxWidth;
        maxHeight = (maxHeight>=height)?height:maxHeight;

        if((x != mRect.x) || (y != mRect.y) || (maxWidth != mRect.width) || (maxHeight != mRect.height)) {
            GraphicWindowApi::OpenSurfaceTransaction();
            Transaction t;
            GraphicWindowApi::SetSurfacePosition(mSurfaceControl, &t, x, y);
            GraphicWindowApi::SetSurfaceSize(mSurfaceControl, &t, maxWidth, maxHeight);
            GraphicWindowApi::CloseSurfaceTransaction(&t);

            mRect.x = x;
            mRect.y = y;
            mRect.width = maxWidth;
            mRect.height= maxHeight;
        }
        return 0;
    }

    return -1;
}

void RTSubteSink::setSubtitleSurfaceVisibility(int visible) {
    if (visible == 1) {
        show();
    } else {
        hide();
    }
}

void RTSubteSink::displayPgsSubtitle() {

}

SurfaceRect& RTSubteSink::getSurfaceRect() {
    return mRect;
}

int RTSubteSink::getSurfaceZOrder() {
    return mSubtitleZOrder;
}


void RTSubteSink::setSubtitleMode(int mode) {
    mDisplayMode = mode;
}

void RTSubteSink::setHdmiMode(int mode) {
    mHdmiMode = mode;
}

bool RTSubteSink::checkRotation() {
    Mutex::Autolock autoLock(mRenderLock);
    DisplayConfig config;
    ui::DisplayState state;
    getSurfaceMaxWidthAndHeight(config, state, mDisplayDev);
    if((config.resolution.getWidth() != mRect.width) || (config.resolution.getHeight() != mRect.height) || ((int)state.orientation != mRect.rotation)){
        mRect.width = config.resolution.getWidth();
        mRect.height = config.resolution.getHeight();
        mRect.rotation = (int)state.orientation;

        // if rotation is happend, must set new position and size
        GraphicWindowApi::OpenSurfaceTransaction();
        Transaction t;
        GraphicWindowApi::SetSurfacePosition(mSurfaceControl, &t, mRect.x, mRect.y);
        GraphicWindowApi::SetSurfaceSize(mSurfaceControl, &t, mRect.width, mRect.height);
        GraphicWindowApi::CloseSurfaceTransaction(&t);

        return true;
    }

    return false;
}

void RTSubteSink::getSurfaceMaxWidthAndHeight(DisplayConfig& config, ui::DisplayState& state, int displayId)
{
    int width = 1920;
    int height = 1080;
    int orientation = DISPLAY_ORIENTATION_0;
    status_t err;

    // set default value
    config.resolution.set(width,height);
    state.orientation = (android::ui::Rotation)orientation;

    if(0 /*ISurfaceComposer::eDisplayIdMain*/ != displayId && 1 /*ISurfaceComposer::eDisplayIdHdmi*/ != displayId){
        ALOGE("getSurfaceMaxWidthAndHeight: displayId = %d in not suppport, set default widht = 1920,height  = 1080", displayId);
        return ;
    }
    //  sp<IBinder> display(SurfaceComposerClient::getBuiltInDisplay(displayId));

    const auto display = SurfaceComposerClient::getInternalDisplayToken();
    if(display == nullptr){
        ALOGE("error: no display");
        return;
    }

    err = SurfaceComposerClient::getDisplayState(display, &state);
    if(err != NO_ERROR){
        ALOGE("error: unable to get display state");
        return;
    }

    err = SurfaceComposerClient::getActiveDisplayConfig(display, &config);
    if(err != NO_ERROR){
        ALOGE("error: unable to get display config");
        return;
    }

    char value[PROPERTY_VALUE_MAX];
    if((state.orientation == (android::ui::Rotation)DISPLAY_ORIENTATION_90) || (state.orientation == (android::ui::Rotation)DISPLAY_ORIENTATION_270)){
        width = config.resolution.getWidth();
        height = config.resolution.getHeight();
        orientation = (int)state.orientation;
    }else{
        property_get("ro.sf.fakerotation", value, "false");
        if (strcmp(value,"true") == 0) {
            property_get("ro.sf.hwrotation", value, "0");
            if ((strcmp(value,"90") == 0) || (strcmp(value,"270") == 0)) {
                width = config.resolution.getWidth();
                height = config.resolution.getHeight();
                orientation = (strcmp(value,"90") == 0)?DISPLAY_ORIENTATION_90:DISPLAY_ORIENTATION_270;
            } else {
                width = config.resolution.getWidth();
                height = config.resolution.getHeight();
            }
        } else {
            width = config.resolution.getWidth();
            height = config.resolution.getHeight();
        }
    }

    if(0 /*ISurfaceComposer::eDisplayIdMain*/ == displayId){
        property_get("persist.sys.display.policy", value, "");
        if (strcasecmp(value,"auto") == 0) {
            property_get("sys.fb.cursize", value, "1280x720");
            sscanf(value,"%dx%d",&width,&height);
        }
    }

    config.resolution.set(width,height);
    state.orientation = (android::ui::Rotation)orientation;
}

