/*
 * Copyright (C) 2018 The Android Open Source Project
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

package com.android.emulator.multidisplay;

import android.content.Context;
import android.app.Service;
import android.content.Intent;
import android.os.Handler;
import android.os.IBinder;
import android.util.Log;
import android.hardware.display.DisplayManager;
import android.hardware.display.VirtualDisplay;
import android.view.Surface;
import android.os.Messenger;


public class MultiDisplayService extends Service {
    private static final String TAG = "MultiDisplayService";
    private static final String DISPLAY_NAME = "Emulator 2D Display";
    private static final String[] UNIQUE_DISPLAY_ID = new String[]{"notUsed", "1234562",
                                                                   "1234563", "1234564",
                                                                   "1234565", "1234566",
                                                                   "1234567", "1234568",
                                                                   "1234569", "1234570",
                                                                   "1234571"};
    private static final int MAX_DISPLAYS = 10;
    private static final int ADD = 1;
    private static final int DEL = 2;
    private static final int mFlags = DisplayManager.VIRTUAL_DISPLAY_FLAG_PUBLIC |
                                      DisplayManager.VIRTUAL_DISPLAY_FLAG_OWN_CONTENT_ONLY |
                                      DisplayManager.VIRTUAL_DISPLAY_FLAG_ROTATES_WITH_CONTENT |
                                      1 << 6 |//DisplayManager.VIRTUAL_DISPLAY_FLAG_SUPPORTS_TOUCH
                                      1 << 9; //DisplayManager.VIRTUAL_DISPLAY_FLAG_SHOULD_SHOW_SYSTEM_DECORATIONS;
    private DisplayManager mDisplayManager;
    private VirtualDisplay mVirtualDisplay[];
    private Surface mSurface[];
    private Messenger mMessenger;
    private ListenerThread mListener;

    private final Handler mHandler = new Handler();

    class MultiDisplay {
        public int width;
        public int height;
        public int dpi;
        public int flag;
        public VirtualDisplay virtualDisplay;
        public Surface surface;
        public boolean enabled;
        MultiDisplay() {
            clear();
        }
        public void clear() {
            width = 0;
            height = 0;
            dpi = 0;
            flag = 0;
            virtualDisplay = null;
            surface = null;
            enabled = false;
        }
        public void set(int w, int h, int d, int f) {
            width = w;
            height = h;
            dpi = d;
            flag = f;
            enabled = true;
        }
        public boolean match(int w, int h, int d, int f) {
            return (w == width && h == height && d == dpi && f == flag);
        }
    }
    private MultiDisplay mMultiDisplay[];

    @Override
    public void onCreate() {
        super.onCreate();

        try {
            System.loadLibrary("emulator_multidisplay_jni");
        } catch (Exception e) {
            Log.e(TAG, "Failed to loadLibrary: " + e);
        }

        mListener = new ListenerThread();
        mListener.start();

        mDisplayManager = (DisplayManager)getSystemService(Context.DISPLAY_SERVICE);
        mMultiDisplay = new MultiDisplay[MAX_DISPLAYS + 1];
        for (int i = 0; i < MAX_DISPLAYS + 1; i++) {
            mMultiDisplay[i] = new MultiDisplay();
        }
    }

    @Override
    public IBinder onBind(Intent intent) {
        if(mMessenger == null)
            mMessenger = new Messenger(mHandler);
        return mMessenger.getBinder();
    }

    @Override
    public int onStartCommand(Intent intent, int flags, int startId) {
        // keep it alive.
        return START_STICKY;
    }

    class ListenerThread extends Thread {
        ListenerThread() {
            super(TAG);
        }

        private void deleteVirtualDisplay(int displayId) {
            if (!mMultiDisplay[displayId].enabled) {
                return;
            }
            if (mMultiDisplay[displayId].virtualDisplay != null) {
                mMultiDisplay[displayId].virtualDisplay.release();
            }
            if (mMultiDisplay[displayId].surface != null) {
                mMultiDisplay[displayId].surface.release();
            }
            mMultiDisplay[displayId].clear();
            nativeReleaseListener(displayId);
        }

        private void createVirtualDisplay(int displayId, int w, int h, int dpi, int flag) {
            mMultiDisplay[displayId].surface = nativeCreateSurface(displayId, w, h);
            mMultiDisplay[displayId].virtualDisplay = mDisplayManager.createVirtualDisplay(
                                              null /* projection */,
                                              DISPLAY_NAME, w, h, dpi,
                                              mMultiDisplay[displayId].surface, flag,
                                              null /* callback */,
                                              null /* handler */,
                                              UNIQUE_DISPLAY_ID[displayId]);
            mMultiDisplay[displayId].set(w, h, dpi, flag);
        }

        private void addVirtualDisplay(int displayId, int w, int h, int dpi, int flag) {
            if (mMultiDisplay[displayId].match(w, h, dpi, flag)) {
                return;
            }
            if (mMultiDisplay[displayId].virtualDisplay == null) {
                createVirtualDisplay(displayId, w, h, dpi, flag);
                return;
            }
            if (mMultiDisplay[displayId].flag != flag) {
                deleteVirtualDisplay(displayId);
                createVirtualDisplay(displayId, w, h, dpi, flag);
                return;
            }
            if (mMultiDisplay[displayId].width != w || mMultiDisplay[displayId].height != h) {
                nativeResizeListener(displayId, w, h);
            }
            // only dpi changes
            mMultiDisplay[displayId].virtualDisplay.resize(w, h, dpi);
            mMultiDisplay[displayId].set(w, h, dpi, flag);
        }

        @Override
        public void run() {
            while(nativeOpen() <= 0) {
                Log.e(TAG, "failed to open multiDisplay pipe, retry");
            }
            while(true) {
                int[] array = {0, 0, 0, 0, 0, 0};
                if (!nativeReadPipe(array)) {
                    continue;
                }
                switch (array[0]) {
                    case ADD: {
                        for (int j = 0; j < 6; j++) {
                            Log.d(TAG, "received " + array[j]);
                        }
                        int i = array[1];
                        int width = array[2];
                        int height = array[3];
                        int dpi = array[4];
                        int flag = (array[5] != 0) ? array[5] : mFlags;
                        if (i < 1 || i > MAX_DISPLAYS || width <=0 || height <=0 || dpi <=0
                            || flag < 0) {
                            Log.e(TAG, "invalid parameters for add/modify display");
                            break;
                        }
                        addVirtualDisplay(i, width, height, dpi, flag);
                        break;
                    }
                    case DEL: {
                        int i = array[1];
                        Log.d(TAG, "DEL " + i);
                        if (i < 1 || i > MAX_DISPLAYS) {
                            Log.e(TAG, "invalid parameters for delete display");
                            break;
                        }
                        deleteVirtualDisplay(i);
                        break;
                    }
                }
            }
        }
    }

    private native int nativeOpen();
    private native Surface nativeCreateSurface(int displayId, int width, int height);
    private native boolean nativeReadPipe(int[] arr);
    private native boolean nativeReleaseListener(int displayId);
    private native boolean nativeResizeListener(int displayId, int with, int height);
}
