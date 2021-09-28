/*
 * Copyright 2014 The Android Open Source Project
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

package android.hardware.cts.helpers;

import android.content.Context;
import android.hardware.Camera;
import android.hardware.camera2.CameraCharacteristics;
import android.hardware.camera2.CameraManager;
import android.os.Bundle;

import androidx.test.InstrumentationRegistry;

import java.util.Comparator;
import java.util.stream.IntStream;

/**
 * Utility class containing helper functions for the Camera CTS tests.
 */
public class CameraUtils {

    private static final String CAMERA_ID_INSTR_ARG_KEY = "camera-id";
    private static final Bundle mBundle = InstrumentationRegistry.getArguments();
    public static final String mOverrideCameraId = mBundle.getString(CAMERA_ID_INSTR_ARG_KEY);

    /**
     * Returns {@code true} if this device only supports {@code LEGACY} mode operation in the
     * Camera2 API for the given camera ID.
     *
     * @param context {@link Context} to access the {@link CameraManager} in.
     * @param cameraId the ID of the camera device to check.
     * @return {@code true} if this device only supports {@code LEGACY} mode.
     */
    public static boolean isLegacyHAL(Context context, int cameraId) throws Exception {
        CameraManager manager = (CameraManager) context.getSystemService(Context.CAMERA_SERVICE);
        String cameraIdStr = manager.getCameraIdListNoLazy()[cameraId];
        return isLegacyHAL(manager, cameraIdStr);
    }

    /**
     * Returns {@code true} if this device only supports {@code LEGACY} mode operation in the
     * Camera2 API for the given camera ID.
     *
     * @param manager The {@link CameraManager} used to retrieve camera characteristics.
     * @param cameraId the ID of the camera device to check.
     * @return {@code true} if this device only supports {@code LEGACY} mode.
     */
    public static boolean isLegacyHAL(CameraManager manager, String cameraIdStr) throws Exception {
        CameraCharacteristics characteristics =
                manager.getCameraCharacteristics(cameraIdStr);

        return characteristics.get(CameraCharacteristics.INFO_SUPPORTED_HARDWARE_LEVEL) ==
                CameraCharacteristics.INFO_SUPPORTED_HARDWARE_LEVEL_LEGACY;
    }

    /**
     * Returns {@code true} if this device only supports {@code EXTERNAL} mode operation in the
     * Camera2 API for the given camera ID.
     *
     * @param context {@link Context} to access the {@link CameraManager} in.
     * @param cameraId the ID of the camera device to check.
     * @return {@code true} if this device only supports {@code LEGACY} mode.
     */
    public static boolean isExternal(Context context, int cameraId) throws Exception {
        CameraManager manager = (CameraManager) context.getSystemService(Context.CAMERA_SERVICE);
        String cameraIdStr = manager.getCameraIdListNoLazy()[cameraId];
        CameraCharacteristics characteristics =
                manager.getCameraCharacteristics(cameraIdStr);

        return characteristics.get(CameraCharacteristics.INFO_SUPPORTED_HARDWARE_LEVEL) ==
                CameraCharacteristics.INFO_SUPPORTED_HARDWARE_LEVEL_EXTERNAL;
    }

    /**
     * Shared size comparison method used by size comparators.
     *
     * <p>Compares the number of pixels it covers.If two the areas of two sizes are same, compare
     * the widths.</p>
     */
     public static int compareSizes(int widthA, int heightA, int widthB, int heightB) {
        long left = widthA * (long) heightA;
        long right = widthB * (long) heightB;
        if (left == right) {
            left = widthA;
            right = widthB;
        }
        return (left < right) ? -1 : (left > right ? 1 : 0);
    }

    /**
     * Size comparator that compares the number of pixels it covers.
     *
     * <p>If two the areas of two sizes are same, compare the widths.</p>
     */
    public static class LegacySizeComparator implements Comparator<Camera.Size> {
        @Override
        public int compare(Camera.Size lhs, Camera.Size rhs) {
            return compareSizes(lhs.width, lhs.height, rhs.width, rhs.height);
        }
    }

    public static int[] deriveCameraIdsUnderTest() throws Exception {
        int numberOfCameras = Camera.getNumberOfCameras();
        int[] cameraIds;
        if (mOverrideCameraId == null) {
            cameraIds = IntStream.range(0, numberOfCameras).toArray();
        } else {
            int overrideCameraId = Integer.parseInt(mOverrideCameraId);
            if (overrideCameraId >= 0 && overrideCameraId < numberOfCameras) {
                cameraIds = new int[]{overrideCameraId};
            } else {
                cameraIds = new int[]{};
            }
        }
        return cameraIds;
    }
}
