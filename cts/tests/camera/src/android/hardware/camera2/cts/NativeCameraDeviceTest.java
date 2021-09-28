/*
 * Copyright 2016 The Android Open Source Project
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

import android.graphics.SurfaceTexture;
import android.hardware.camera2.cts.testcases.Camera2SurfaceViewTestCase;
import android.util.Log;
import android.util.Size;
import android.view.Surface;

import org.junit.runner.RunWith;
import org.junit.runners.Parameterized;
import org.junit.Test;

import static org.junit.Assert.assertTrue;

/**
 * <p>Basic test for CameraManager class.</p>
 */

@RunWith(Parameterized.class)
public class NativeCameraDeviceTest extends Camera2SurfaceViewTestCase {
    private static final String TAG = "NativeCameraDeviceTest";
    private static final boolean VERBOSE = Log.isLoggable(TAG, Log.VERBOSE);

    /** Load jni on initialization */
    static {
        Log.i("NativeCameraDeviceTest", "before loadlibrary");
        System.loadLibrary("ctscamera2_jni");
        Log.i("NativeCameraDeviceTest", "after loadlibrary");
    }

    @Test
    public void testCameraDeviceOpenAndClose() {
        assertTrue("testCameraDeviceOpenAndClose fail, see log for details",
                testCameraDeviceOpenAndCloseNative(mOverrideCameraId));
    }

    @Test
    public void testCameraDeviceCreateCaptureRequest() {
        assertTrue("testCameraDeviceCreateCaptureRequest fail, see log for details",
                testCameraDeviceCreateCaptureRequestNative(mOverrideCameraId));
    }

    @Test
    public void testCameraDeviceSessionOpenAndClose() {
        // Init preview surface to a guaranteed working size
        updatePreviewSurface(new Size(640, 480));
        assertTrue("testCameraDeviceSessionOpenAndClose fail, see log for details",
                testCameraDeviceSessionOpenAndCloseNative(mPreviewSurface, mOverrideCameraId));
    }

    @Test
    public void testCameraDeviceSimplePreview() {
        // Init preview surface to a guaranteed working size
        updatePreviewSurface(new Size(640, 480));
        assertTrue("testCameraDeviceSimplePreview fail, see log for details",
                testCameraDeviceSimplePreviewNative(mPreviewSurface, mOverrideCameraId));
    }

    @Test
    public void testCameraDevicePreviewWithSessionParameters() {
        // Init preview surface to a guaranteed working size
        updatePreviewSurface(new Size(640, 480));
        assertTrue("testCameraDevicePreviewWithSessionParameters fail, see log for details",
                testCameraDevicePreviewWithSessionParametersNative(mPreviewSurface,
                mOverrideCameraId));
    }

    @Test
    public void testCameraDeviceSharedOutputUpdate() {
        // Init preview surface to a guaranteed working size
        Size previewSize = new Size(640, 480);
        updatePreviewSurface(previewSize);
        SurfaceTexture outputTexture = new SurfaceTexture(/* random texture ID*/ 5);
        outputTexture.setDefaultBufferSize(previewSize.getWidth(), previewSize.getHeight());
        Surface outputSurface = new Surface(outputTexture);
        assertTrue("testCameraDeviceSharedOutputUpdate fail, see log for details",
                testCameraDeviceSharedOutputUpdate(mPreviewSurface, outputSurface,
                mOverrideCameraId));
    }

    @Test
    public void testCameraDeviceLogicalPhysicalStreaming() {
        // Init preview surface to a guaranteed working size
        Size previewSize = new Size(640, 480);
        updatePreviewSurface(previewSize);
        SurfaceTexture outputTexture = new SurfaceTexture(/* random texture ID*/ 5);
        outputTexture.setDefaultBufferSize(previewSize.getWidth(), previewSize.getHeight());
        Surface outputSurface = new Surface(outputTexture);
        assertTrue("testCameraDeviceLogicalPhysicalStreaming fail, see log for details",
                testCameraDeviceLogicalPhysicalStreamingNative(mPreviewSurface, mOverrideCameraId));
    }

    @Test
    public void testCameraDeviceLogicalPhysicalSettings() {
        // Init preview surface to a guaranteed working size
        Size previewSize = new Size(640, 480);
        updatePreviewSurface(previewSize);
        SurfaceTexture outputTexture = new SurfaceTexture(/* random texture ID*/ 5);
        outputTexture.setDefaultBufferSize(previewSize.getWidth(), previewSize.getHeight());
        Surface outputSurface = new Surface(outputTexture);
        assertTrue("testCameraDeviceLogicalPhysicalSettings fail, see log for details",
                testCameraDeviceLogicalPhysicalSettingsNative(mPreviewSurface, mOverrideCameraId));
    }

    @Test
    public void testCameraDeviceCaptureFailure() {
        assertTrue("testCameraDeviceCaptureFailure fail, see log for details",
                testCameraDeviceCaptureFailureNative(mOverrideCameraId));
    }

    private static native boolean testCameraDeviceOpenAndCloseNative(String overrideCameraId);
    private static native boolean testCameraDeviceCreateCaptureRequestNative(
            String overrideCameraId);
    private static native boolean testCameraDeviceSessionOpenAndCloseNative(Surface preview,
            String overrideCameraId);
    private static native boolean testCameraDeviceSimplePreviewNative(Surface preview,
            String overrideCameraId);
    private static native boolean testCameraDevicePreviewWithSessionParametersNative(
            Surface preview, String overrideCameraId);
    private static native boolean testCameraDeviceSharedOutputUpdate(Surface src, Surface dst,
            String overrideCameraId);
    private static native boolean testCameraDeviceLogicalPhysicalStreamingNative(Surface preview,
            String overrideCameraId);
    private static native boolean testCameraDeviceLogicalPhysicalSettingsNative(Surface preview,
            String overrideCameraId);
    private static native boolean testCameraDeviceCaptureFailureNative(String overrideCameraId);

}
