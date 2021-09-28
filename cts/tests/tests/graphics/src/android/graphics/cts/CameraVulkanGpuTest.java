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

import android.content.Context;
import android.content.pm.PackageManager;
import android.content.res.AssetManager;
import android.hardware.camera2.CameraManager;
import android.hardware.camera2.CameraCharacteristics;
import android.test.suitebuilder.annotation.SmallTest;

import androidx.test.InstrumentationRegistry;
import androidx.test.runner.AndroidJUnit4;

import com.android.compatibility.common.util.PropertyUtil;

import java.util.ArrayList;

import org.junit.Test;
import org.junit.runner.RunWith;

@SmallTest
@RunWith(AndroidJUnit4.class)
public class CameraVulkanGpuTest {

    static {
        System.loadLibrary("ctsgraphics_jni");
    }

    @Test
    public void testCameraImportAndRendering() throws Exception {
        PackageManager pm = InstrumentationRegistry.getContext().getPackageManager();
        if (!pm.hasSystemFeature(PackageManager.FEATURE_CAMERA)) {
            // Test requires a camera.
            return;
        }

        if(PropertyUtil.getFirstApiLevel() < 28){
            // HAL3 is not required for P upgrade devices.
            return;
        }

        CameraManager camMgr =
                (CameraManager) InstrumentationRegistry.getContext().
                        getSystemService(Context.CAMERA_SERVICE);
        try {
            String[] cameraIds = camMgr.getCameraIdList();
            ArrayList<String> mToBeTestedCameraIds = new ArrayList<String>();
            for (String id : cameraIds) {
                CameraCharacteristics characteristics = camMgr.getCameraCharacteristics(id);
                int hwLevel =
                        characteristics.get(CameraCharacteristics.INFO_SUPPORTED_HARDWARE_LEVEL);
                if (hwLevel != CameraCharacteristics.INFO_SUPPORTED_HARDWARE_LEVEL_LEGACY) {
                    mToBeTestedCameraIds.add(id);
                }
            }

            // skip the test if all camera devices are legacy
            if (mToBeTestedCameraIds.size() == 0) return;
        } catch (IllegalArgumentException e) {
            // This is the exception that should be thrown in this case.
        }

        loadCameraAndVerifyFrameImport(InstrumentationRegistry.getContext().getAssets());
    }

    private static native void loadCameraAndVerifyFrameImport(AssetManager assetManager);
}
