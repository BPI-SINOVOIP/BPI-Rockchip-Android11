/*
 * Copyright 2019 The Android Open Source Project
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

package android.hardware.camera2.cts;

import android.content.Context;
import android.hardware.cts.helpers.CameraParameterizedTestCase;
import android.hardware.camera2.cts.CameraTestUtils;
import android.hardware.camera2.CameraManager;
import android.util.Log;

import java.util.Arrays;

import org.junit.Ignore;

import static junit.framework.Assert.assertNotNull;
import static junit.framework.Assert.assertTrue;

public class Camera2ParameterizedTestCase extends CameraParameterizedTestCase {
    private static final String TAG = "Camera2ParameterizedTestCase";
    protected CameraManager mCameraManager;
    // The list of camera ids we're testing. If we're testing system cameras
    // (mAdoptShellPerm == true), we have only system camera ids in the array and not normal camera
    // ids.
    protected String[] mCameraIdsUnderTest;

    @Override
    public void setUp() throws Exception {
        super.setUp();
        /**
         * Workaround for mockito and JB-MR2 incompatibility
         *
         * Avoid java.lang.IllegalArgumentException: dexcache == null
         * https://code.google.com/p/dexmaker/issues/detail?id=2
         */
        System.setProperty("dexmaker.dexcache", mContext.getCacheDir().toString());
        mCameraManager = (CameraManager) mContext.getSystemService(Context.CAMERA_SERVICE);
        assertNotNull("Unable to get CameraManager", mCameraManager);
        mCameraIdsUnderTest = deriveCameraIdsUnderTest();
        assertNotNull("Unable to get camera ids", mCameraIdsUnderTest);
    }

    @Override
    public void tearDown() throws Exception {
        String[] cameraIdsPostTest = deriveCameraIdsUnderTest();
        assertNotNull("Camera ids shouldn't be null", cameraIdsPostTest);
        Log.i(TAG, "Camera ids in setup:" + Arrays.toString(mCameraIdsUnderTest));
        Log.i(TAG, "Camera ids in tearDown:" + Arrays.toString(cameraIdsPostTest));
        assertTrue(
                "Number of cameras changed from " + mCameraIdsUnderTest.length + " to " +
                cameraIdsPostTest.length,
                mCameraIdsUnderTest.length == cameraIdsPostTest.length);
        super.tearDown();
    }

    private String[] deriveCameraIdsUnderTest() throws Exception {
        String[] idsUnderTest =
                CameraTestUtils.getCameraIdListForTesting(mCameraManager, mAdoptShellPerm);
        assertNotNull("Camera ids shouldn't be null", idsUnderTest);
        if (mOverrideCameraId != null) {
            if (Arrays.asList(idsUnderTest).contains(mOverrideCameraId)) {
                idsUnderTest = new String[]{mOverrideCameraId};
            } else {
                idsUnderTest = new String[]{};
            }
        }

        return idsUnderTest;
    }
}
