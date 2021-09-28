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
package com.android.wallpaper.util;

import android.os.Bundle;
import android.os.Message;
import android.view.SurfaceControlViewHost;
import android.view.SurfaceView;

/** Util class to generate surface view requests and parse responses */
public class SurfaceViewUtils {

    private static final String KEY_HOST_TOKEN = "host_token";
    private static final String KEY_VIEW_WIDTH = "width";
    private static final String KEY_VIEW_HEIGHT = "height";
    private static final String KEY_DISPLAY_ID = "display_id";
    private static final String KEY_SURFACE_PACKAGE = "surface_package";
    private static final String KEY_CALLBACK = "callback";

    /** Create a surface view request. */
    public static Bundle createSurfaceViewRequest(SurfaceView surfaceView) {
        Bundle bundle = new Bundle();
        bundle.putBinder(KEY_HOST_TOKEN, surfaceView.getHostToken());
        bundle.putInt(KEY_DISPLAY_ID, surfaceView.getDisplay().getDisplayId());
        bundle.putInt(KEY_VIEW_WIDTH, surfaceView.getWidth());
        bundle.putInt(KEY_VIEW_HEIGHT, surfaceView.getHeight());
        return bundle;
    }

    /** Return the surface package. */
    public static SurfaceControlViewHost.SurfacePackage getSurfacePackage(Bundle bundle) {
        return bundle.getParcelable(KEY_SURFACE_PACKAGE);
    }

    /** Return the message callback. */
    public static Message getCallback(Bundle bundle) {
        return bundle.getParcelable(KEY_CALLBACK);
    }
}
