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

package android.graphics.cts;

import android.app.Activity;
import android.content.Context;
import android.content.pm.ActivityInfo;
import android.content.res.AssetManager;
import android.content.res.Configuration;
import android.os.Bundle;
import android.util.Log;
import android.view.Surface;
import android.view.SurfaceView;
import android.view.WindowManager;

/**
 * Activity for VulkanPreTransformTest.
 */
public class VulkanPreTransformCtsActivity extends Activity {
    static {
        System.loadLibrary("ctsgraphics_jni");
    }

    private static final String TAG = VulkanPreTransformCtsActivity.class.getSimpleName();

    private static boolean sOrientationRequested = false;

    protected Surface mSurface;

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        Log.d(TAG, "onCreate!");
        setActivityOrientation();
        setContentView(R.layout.vulkan_pretransform_layout);
        SurfaceView surfaceView = (SurfaceView) findViewById(R.id.surfaceview);
        mSurface = surfaceView.getHolder().getSurface();
    }

    private void setActivityOrientation() {
        if (sOrientationRequested) {
            // it might be called again because changing the orientation kicks off onCreate again!.
            return;
        }

        if (getRotation() == Surface.ROTATION_0) {
            if (getResources().getConfiguration().orientation
                    == Configuration.ORIENTATION_LANDSCAPE) {
                setRequestedOrientation(ActivityInfo.SCREEN_ORIENTATION_PORTRAIT);
            } else {
                setRequestedOrientation(ActivityInfo.SCREEN_ORIENTATION_LANDSCAPE);
            }
        }

        sOrientationRequested = true;
    }

    public int getRotation() {
        return ((WindowManager) getSystemService(Context.WINDOW_SERVICE))
                .getDefaultDisplay()
                .getRotation();
    }

    public void testVulkanPreTransform(boolean setPreTransform) {
        nCreateNativeTest(getAssets(), mSurface, setPreTransform);
        sOrientationRequested = false;
    }

    private static native void nCreateNativeTest(
            AssetManager manager, Surface surface, boolean setPreTransform);
}
