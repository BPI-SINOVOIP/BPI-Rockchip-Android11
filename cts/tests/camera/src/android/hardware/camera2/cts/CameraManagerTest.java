/*
 * Copyright 2013 The Android Open Source Project
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

import static junit.framework.Assert.*;

import static org.mockito.Mockito.*;

import android.app.Instrumentation;
import android.app.NotificationManager;
import android.app.UiAutomation;
import android.content.pm.PackageManager;
import android.hardware.camera2.CameraAccessException;
import android.hardware.camera2.CameraCharacteristics;
import android.hardware.camera2.CameraDevice;
import android.hardware.camera2.CameraDevice.StateCallback;
import android.hardware.camera2.CameraManager;
import android.hardware.camera2.cts.Camera2ParameterizedTestCase;
import android.hardware.camera2.cts.CameraTestUtils.HandlerExecutor;
import android.hardware.camera2.cts.CameraTestUtils.MockStateCallback;
import android.hardware.camera2.cts.helpers.CameraErrorCollector;
import android.os.Build;
import android.os.Handler;
import android.os.HandlerThread;
import android.os.ParcelFileDescriptor;
import android.util.Log;
import android.util.Pair;

import androidx.test.InstrumentationRegistry;

import com.android.compatibility.common.util.PropertyUtil;
import com.android.ex.camera2.blocking.BlockingStateCallback;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.Parameterized;
import org.mockito.ArgumentCaptor;
import org.mockito.InOrder;

import java.io.FileInputStream;
import java.io.IOException;
import java.io.InputStream;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.HashMap;
import java.util.HashSet;
import java.util.List;
import java.util.Map;
import java.util.Set;
import java.util.concurrent.Executor;
import java.util.concurrent.LinkedBlockingQueue;

/**
 * <p>Basic test for CameraManager class.</p>
 */

@RunWith(Parameterized.class)
public class CameraManagerTest extends Camera2ParameterizedTestCase {
    private static final String TAG = "CameraManagerTest";
    private static final boolean VERBOSE = Log.isLoggable(TAG, Log.VERBOSE);
    private static final int NUM_CAMERA_REOPENS = 10;
    private static final int AVAILABILITY_TIMEOUT_MS = 10;

    private PackageManager mPackageManager;
    private NoopCameraListener mListener;
    private HandlerThread mHandlerThread;
    private Handler mHandler;
    private BlockingStateCallback mCameraListener;
    private CameraErrorCollector mCollector;
    private Set<Set<String>> mConcurrentCameraIdCombinations;

    /** Load validation jni on initialization. */
    static {
        System.loadLibrary("ctscamera2_jni");
    }

    @Override
    public void setUp() throws Exception {
        super.setUp();
        mPackageManager = mContext.getPackageManager();
        assertNotNull("Can't get package manager", mPackageManager);
        mListener = new NoopCameraListener();

        /**
         * Workaround for mockito and JB-MR2 incompatibility
         *
         * Avoid java.lang.IllegalArgumentException: dexcache == null
         * https://code.google.com/p/dexmaker/issues/detail?id=2
         */
        System.setProperty("dexmaker.dexcache", mContext.getCacheDir().toString());

        mCameraListener = spy(new BlockingStateCallback());

        mHandlerThread = new HandlerThread(TAG);
        mHandlerThread.start();
        mHandler = new Handler(mHandlerThread.getLooper());
        mCollector = new CameraErrorCollector();
        mConcurrentCameraIdCombinations =
                CameraTestUtils.getConcurrentCameraIds(mCameraManager, mAdoptShellPerm);
    }

    @Override
    public void tearDown() throws Exception {
        mHandlerThread.quitSafely();
        mHandler = null;

        try {
            mCollector.verify();
        } catch (Throwable e) {
            // When new Exception(e) is used, exception info will be printed twice.
            throw new Exception(e.getMessage());
        } finally {
            super.tearDown();
        }
    }

    /**
     * Verifies that the reason is in the range of public-only codes.
     */
    private static int checkCameraAccessExceptionReason(CameraAccessException e) {
        int reason = e.getReason();

        switch (reason) {
            case CameraAccessException.CAMERA_DISABLED:
            case CameraAccessException.CAMERA_DISCONNECTED:
            case CameraAccessException.CAMERA_ERROR:
            case CameraAccessException.CAMERA_IN_USE:
            case CameraAccessException.MAX_CAMERAS_IN_USE:
                return reason;
        }

        fail("Invalid CameraAccessException code: " + reason);

        return -1; // unreachable
    }

    @Test
    public void testCameraManagerGetDeviceIdList() throws Exception {

        String[] ids = mCameraIdsUnderTest;
        if (VERBOSE) Log.v(TAG, "CameraManager ids: " + Arrays.toString(ids));

        /**
         * Test: that if there is at least one reported id, then the system must have
         * the FEATURE_CAMERA_ANY feature.
         */
        assertTrue("System camera feature and camera id list don't match",
                ids.length == 0 ||
                mPackageManager.hasSystemFeature(PackageManager.FEATURE_CAMERA_ANY));

        /**
         * Test: that if the device has front or rear facing cameras, then there
         * must be matched system features.
         */
        boolean externalCameraConnected = false;
        Map<String, Integer> lensFacingMap = new HashMap<String, Integer>();
        for (int i = 0; i < ids.length; i++) {
            CameraCharacteristics props = mCameraManager.getCameraCharacteristics(ids[i]);
            assertNotNull("Can't get camera characteristics for camera " + ids[i], props);
            Integer lensFacing = props.get(CameraCharacteristics.LENS_FACING);
            lensFacingMap.put(ids[i], lensFacing);
            assertNotNull("Can't get lens facing info", lensFacing);
            if (lensFacing == CameraCharacteristics.LENS_FACING_FRONT) {
                assertTrue("System doesn't have front camera feature",
                        mPackageManager.hasSystemFeature(PackageManager.FEATURE_CAMERA_FRONT));
            } else if (lensFacing == CameraCharacteristics.LENS_FACING_BACK) {
                assertTrue("System doesn't have back camera feature",
                        mPackageManager.hasSystemFeature(PackageManager.FEATURE_CAMERA));
            } else if (lensFacing == CameraCharacteristics.LENS_FACING_EXTERNAL) {
                externalCameraConnected = true;
                assertTrue("System doesn't have external camera feature",
                        mPackageManager.hasSystemFeature(PackageManager.FEATURE_CAMERA_EXTERNAL));
            } else {
                fail("Unknown camera lens facing " + lensFacing.toString());
            }
        }

        // Test an external camera is connected if FEATURE_CAMERA_EXTERNAL is advertised
        if (!mAdoptShellPerm &&
                mPackageManager.hasSystemFeature(PackageManager.FEATURE_CAMERA_EXTERNAL)) {
            assertTrue("External camera is not connected on device with FEATURE_CAMERA_EXTERNAL",
                    externalCameraConnected);
        }

        /**
         * Test: that if there is one camera device, then the system must have some
         * specific features.
         */
        assertTrue("Missing system feature: FEATURE_CAMERA_ANY",
               ids.length == 0
            || mPackageManager.hasSystemFeature(PackageManager.FEATURE_CAMERA_ANY));
        assertTrue("Missing system feature: FEATURE_CAMERA, FEATURE_CAMERA_FRONT or FEATURE_CAMERA_EXTERNAL",
               ids.length == 0
            || mPackageManager.hasSystemFeature(PackageManager.FEATURE_CAMERA)
            || mPackageManager.hasSystemFeature(PackageManager.FEATURE_CAMERA_FRONT)
            || mPackageManager.hasSystemFeature(PackageManager.FEATURE_CAMERA_EXTERNAL));

        boolean frontBackAdvertised =
                mPackageManager.hasSystemFeature(PackageManager.FEATURE_CAMERA_CONCURRENT);

        boolean frontBackCombinationFound = false;
        // Go through all combinations and see that at least one combination has a front + back
        // camera.
        for (Set<String> cameraIdCombination : mConcurrentCameraIdCombinations) {
            boolean frontFacingFound = false, backFacingFound = false;
            for (String cameraId : cameraIdCombination) {
                Integer lensFacing = lensFacingMap.get(cameraId);
                if (lensFacing == CameraCharacteristics.LENS_FACING_FRONT) {
                    frontFacingFound = true;
                } else if (lensFacing == CameraCharacteristics.LENS_FACING_BACK) {
                    backFacingFound = true;
                }
                if (frontFacingFound && backFacingFound) {
                    frontBackCombinationFound = true;
                    break;
                }
            }
            if (frontBackCombinationFound) {
                break;
            }
        }

        if(mCameraIdsUnderTest.length > 0) {
            assertTrue("System camera feature FEATURE_CAMERA_CONCURRENT = " + frontBackAdvertised +
                    " and device actually having a front back combination which can operate " +
                    "concurrently = " + frontBackCombinationFound +  " do not match",
                    frontBackAdvertised == frontBackCombinationFound);
        }
    }

    // Test: that properties can be queried from each device, without exceptions.
    @Test
    public void testCameraManagerGetCameraCharacteristics() throws Exception {
        String[] ids = mCameraIdsUnderTest;
        for (int i = 0; i < ids.length; i++) {
            CameraCharacteristics props = mCameraManager.getCameraCharacteristics(ids[i]);
            assertNotNull(
                    String.format("Can't get camera characteristics from: ID %s", ids[i]), props);
        }
    }

    // Test: that properties queried between the Java SDK and the C++ NDK are equivalent.
    @Test
    public void testCameraCharacteristicsNdkFromSdk() throws Exception {
        String[] ids = mCameraIdsUnderTest;
        for (int i = 0; i < ids.length; i++) {
            CameraCharacteristics props = mCameraManager.getCameraCharacteristics(ids[i]);
            Integer lensFacing = props.get(CameraCharacteristics.LENS_FACING);
            assertNotNull("Can't get lens facing info", lensFacing);

            assertTrue(validateACameraMetadataFromCameraMetadataCriticalTagsNative(
                props, lensFacing.intValue()));
        }
    }

    // Returns true if `props` has lens facing `lensFacing` when queried from the NDK via
    // ACameraMetadata_fromCameraMetadata().
    private static native boolean validateACameraMetadataFromCameraMetadataCriticalTagsNative(
        CameraCharacteristics props, int lensFacing);

    // Test: that an exception is thrown if an invalid device id is passed down.
    @Test
    public void testCameraManagerInvalidDevice() throws Exception {
        String[] ids = mCameraIdsUnderTest;
        // Create an invalid id by concatenating all the valid ids together.
        StringBuilder invalidId = new StringBuilder();
        invalidId.append("INVALID");
        for (int i = 0; i < ids.length; i++) {
            invalidId.append(ids[i]);
        }

        try {
            mCameraManager.getCameraCharacteristics(
                invalidId.toString());
            fail(String.format("Accepted invalid camera ID: %s", invalidId.toString()));
        } catch (IllegalArgumentException e) {
            // This is the exception that should be thrown in this case.
        }
    }

    // Test: that each camera device can be opened one at a time, several times.
    @Test
    public void testCameraManagerOpenCamerasSerially() throws Exception {
        testCameraManagerOpenCamerasSerially(/*useExecutor*/ false);
        testCameraManagerOpenCamerasSerially(/*useExecutor*/ true);
    }

    private void testCameraManagerOpenCamerasSerially(boolean useExecutor) throws Exception {
        final Executor executor = useExecutor ? new HandlerExecutor(mHandler) : null;
        String[] ids = mCameraIdsUnderTest;
        for (int i = 0; i < ids.length; i++) {
            for (int j = 0; j < NUM_CAMERA_REOPENS; j++) {
                CameraDevice camera = null;
                try {
                    MockStateCallback mockListener = MockStateCallback.mock();
                    mCameraListener = new BlockingStateCallback(mockListener);

                    if (useExecutor) {
                        mCameraManager.openCamera(ids[i], executor, mCameraListener);
                    } else {
                        mCameraManager.openCamera(ids[i], mCameraListener, mHandler);
                    }

                    // Block until unConfigured
                    mCameraListener.waitForState(BlockingStateCallback.STATE_OPENED,
                            CameraTestUtils.CAMERA_IDLE_TIMEOUT_MS);

                    // Ensure state transitions are in right order:
                    // -- 1) Opened
                    // Ensure no other state transitions have occurred:
                    camera = verifyCameraStateOpened(ids[i], mockListener);
                } finally {
                    if (camera != null) {
                        camera.close();
                    }
                }
            }
        }
    }

    /**
     * Test: one or more camera devices can be open at the same time, or the right error state
     * is set if this can't be done.
     */
    @Test
    public void testCameraManagerOpenAllCameras() throws Exception {
        testCameraManagerOpenAllCameras(/*useExecutor*/ false);
        testCameraManagerOpenAllCameras(/*useExecutor*/ true);
    }

    private void testCameraManagerOpenAllCameras(boolean useExecutor) throws Exception {
        String[] ids = mCameraIdsUnderTest;
        assertNotNull("Camera ids shouldn't be null", ids);

        // Skip test if the device doesn't have multiple cameras.
        if (ids.length <= 1) {
            return;
        }

        final Executor executor = useExecutor ? new HandlerExecutor(mHandler) : null;
        List<CameraDevice> cameraList = new ArrayList<CameraDevice>();
        List<MockStateCallback> listenerList = new ArrayList<MockStateCallback>();
        List<BlockingStateCallback> blockingListenerList = new ArrayList<BlockingStateCallback>();
        try {
            for (int i = 0; i < ids.length; i++) {
                // Ignore state changes from other cameras
                MockStateCallback mockListener = MockStateCallback.mock();
                mCameraListener = new BlockingStateCallback(mockListener);

                /**
                 * Track whether or not we got a synchronous error from openCamera.
                 *
                 * A synchronous error must also be accompanied by an asynchronous
                 * StateCallback#onError callback.
                 */
                boolean expectingError = false;

                String cameraId = ids[i];
                try {
                    if (useExecutor) {
                        mCameraManager.openCamera(cameraId, executor, mCameraListener);
                    } else {
                        mCameraManager.openCamera(cameraId, mCameraListener, mHandler);
                    }
                } catch (CameraAccessException e) {
                    int reason = checkCameraAccessExceptionReason(e);
                    if (reason == CameraAccessException.CAMERA_DISCONNECTED ||
                            reason == CameraAccessException.CAMERA_DISABLED) {
                        // TODO: We should handle a Disabled camera by passing here and elsewhere
                        fail("Camera must not be disconnected or disabled for this test" + ids[i]);
                    } else {
                        expectingError = true;
                    }
                }

                List<Integer> expectedStates = new ArrayList<Integer>();
                expectedStates.add(BlockingStateCallback.STATE_OPENED);
                expectedStates.add(BlockingStateCallback.STATE_ERROR);
                int state = mCameraListener.waitForAnyOfStates(
                        expectedStates, CameraTestUtils.CAMERA_IDLE_TIMEOUT_MS);

                // It's possible that we got an asynchronous error transition only. This is ok.
                if (expectingError) {
                    assertEquals("Throwing a CAMERA_ERROR exception must be accompanied with a " +
                            "StateCallback#onError callback",
                            BlockingStateCallback.STATE_ERROR, state);
                }

                /**
                 * Two situations are considered passing:
                 * 1) The camera opened successfully.
                 *     => No error must be set.
                 * 2) The camera did not open because there were too many other cameras opened.
                 *     => Only MAX_CAMERAS_IN_USE error must be set.
                 *
                 * Any other situation is considered a failure.
                 *
                 * For simplicity we treat disconnecting asynchronously as a failure, so
                 * camera devices should not be physically unplugged during this test.
                 */

                CameraDevice camera;
                if (state == BlockingStateCallback.STATE_ERROR) {
                    // Camera did not open because too many other cameras were opened
                    // => onError called exactly once with a non-null camera
                    assertTrue("At least one camera must be opened successfully",
                            cameraList.size() > 0);

                    ArgumentCaptor<CameraDevice> argument =
                            ArgumentCaptor.forClass(CameraDevice.class);

                    verify(mockListener)
                            .onError(
                                    argument.capture(),
                                    eq(CameraDevice.StateCallback.ERROR_MAX_CAMERAS_IN_USE)
                            );
                    verifyNoMoreInteractions(mockListener);

                    camera = argument.getValue();
                    assertNotNull("Expected a non-null camera for the error transition for ID: "
                            + ids[i], camera);
                } else if (state == BlockingStateCallback.STATE_OPENED) {
                    // Camera opened successfully.
                    // => onOpened called exactly once
                    camera = verifyCameraStateOpened(cameraId,
                            mockListener);
                } else {
                    fail("Unexpected state " + state);
                    camera = null; // unreachable. but need this for java compiler
                }

                // Keep track of cameras so we can close it later
                cameraList.add(camera);
                listenerList.add(mockListener);
                blockingListenerList.add(mCameraListener);
            }
        } finally {
            for (int i = 0; i < cameraList.size(); i++) {
                // With conflicting devices, opening of one camera could result in the other camera
                // being disconnected. To handle such case, reset the mock before close.
                reset(listenerList.get(i));
                cameraList.get(i).close();
            }
            for (BlockingStateCallback blockingListener : blockingListenerList) {
                blockingListener.waitForState(
                        BlockingStateCallback.STATE_CLOSED,
                        CameraTestUtils.CAMERA_IDLE_TIMEOUT_MS);
            }
        }

        /*
         * Ensure that no state transitions have bled through from one camera to another
         * after closing the cameras.
         */
        int i = 0;
        for (MockStateCallback listener : listenerList) {
            CameraDevice camera = cameraList.get(i);

            verify(listener).onClosed(eq(camera));
            verifyNoMoreInteractions(listener);
            i++;
            // Only a #close can happen on the camera since we were done with it.
            // Also nothing else should've happened between the close and the open.
        }
    }

    /**
     * Verifies the camera in this listener was opened and then unconfigured exactly once.
     *
     * <p>This assumes that no other action to the camera has been done (e.g.
     * it hasn't been configured, or closed, or disconnected). Verification is
     * performed immediately without any timeouts.</p>
     *
     * <p>This checks that the state has previously changed first for opened and then unconfigured.
     * Any other state transitions will fail. A test failure is thrown if verification fails.</p>
     *
     * @param cameraId Camera identifier
     * @param listener Listener which was passed to {@link CameraManager#openCamera}
     *
     * @return The camera device (non-{@code null}).
     */
    private static CameraDevice verifyCameraStateOpened(String cameraId,
            MockStateCallback listener) {
        ArgumentCaptor<CameraDevice> argument =
                ArgumentCaptor.forClass(CameraDevice.class);
        InOrder inOrder = inOrder(listener);

        /**
         * State transitions (in that order):
         *  1) onOpened
         *
         * No other transitions must occur for successful #openCamera
         */
        inOrder.verify(listener)
                .onOpened(argument.capture());

        CameraDevice camera = argument.getValue();
        assertNotNull(
                String.format("Failed to open camera device ID: %s", cameraId),
                camera);

        // Do not use inOrder here since that would skip anything called before onOpened
        verifyNoMoreInteractions(listener);

        return camera;
    }

    /**
     * Test: that opening the same device multiple times and make sure the right
     * error state is set.
     */
    @Test
    public void testCameraManagerOpenCameraTwice() throws Exception {
        testCameraManagerOpenCameraTwice(/*useExecutor*/ false);
        testCameraManagerOpenCameraTwice(/*useExecutor*/ true);
    }

    private void testCameraManagerOpenCameraTwice(boolean useExecutor) throws Exception {
        String[] ids = mCameraIdsUnderTest;
        final Executor executor = useExecutor ? new HandlerExecutor(mHandler) : null;

        // Test across every camera device.
        for (int i = 0; i < ids.length; ++i) {
            CameraDevice successCamera = null;
            mCollector.setCameraId(ids[i]);

            try {
                MockStateCallback mockSuccessListener = MockStateCallback.mock();
                MockStateCallback mockFailListener = MockStateCallback.mock();

                BlockingStateCallback successListener =
                        new BlockingStateCallback(mockSuccessListener);
                BlockingStateCallback failListener =
                        new BlockingStateCallback(mockFailListener);

                if (useExecutor) {
                    mCameraManager.openCamera(ids[i], executor, successListener);
                    mCameraManager.openCamera(ids[i], executor, failListener);
                } else {
                    mCameraManager.openCamera(ids[i], successListener, mHandler);
                    mCameraManager.openCamera(ids[i], failListener, mHandler);
                }

                successListener.waitForState(BlockingStateCallback.STATE_OPENED,
                        CameraTestUtils.CAMERA_IDLE_TIMEOUT_MS);
                ArgumentCaptor<CameraDevice> argument =
                        ArgumentCaptor.forClass(CameraDevice.class);
                verify(mockSuccessListener, atLeastOnce()).onOpened(argument.capture());
                verify(mockSuccessListener, atLeastOnce()).onDisconnected(argument.capture());

                failListener.waitForState(BlockingStateCallback.STATE_OPENED,
                        CameraTestUtils.CAMERA_IDLE_TIMEOUT_MS);
                verify(mockFailListener, atLeastOnce()).onOpened(argument.capture());

                successCamera = verifyCameraStateOpened(
                        ids[i], mockFailListener);

                verifyNoMoreInteractions(mockFailListener);
            } finally {
                if (successCamera != null) {
                    successCamera.close();
                }
            }
        }
    }

    private class NoopCameraListener extends CameraManager.AvailabilityCallback {
        @Override
        public void onCameraAvailable(String cameraId) {
            // No-op
        }

        @Override
        public void onCameraUnavailable(String cameraId) {
            // No-op
        }
    }

    /**
     * Test: that the APIs to register and unregister a listener run successfully;
     * doesn't test that the listener actually gets invoked at the right time.
     * Registering a listener multiple times should have no effect, and unregistering
     * a listener that isn't registered should have no effect.
     */
    @Test
    public void testCameraManagerListener() throws Exception {
        mCameraManager.unregisterAvailabilityCallback(mListener);
        // Test Handler API
        mCameraManager.registerAvailabilityCallback(mListener, mHandler);
        mCameraManager.registerAvailabilityCallback(mListener, mHandler);
        mCameraManager.unregisterAvailabilityCallback(mListener);
        mCameraManager.unregisterAvailabilityCallback(mListener);
        // Test Executor API
        Executor executor = new HandlerExecutor(mHandler);
        mCameraManager.registerAvailabilityCallback(executor, mListener);
        mCameraManager.registerAvailabilityCallback(executor, mListener);
        mCameraManager.unregisterAvailabilityCallback(mListener);
        mCameraManager.unregisterAvailabilityCallback(mListener);
    }

    /**
     * Test that the availability callbacks fire when expected
     */
    @Test
    public void testCameraManagerListenerCallbacks() throws Exception {
        if (mOverrideCameraId != null) {
            // Testing is done for individual camera. Skip.
            return;
        }
        testCameraManagerListenerCallbacks(/*useExecutor*/ false);
        testCameraManagerListenerCallbacks(/*useExecutor*/ true);
    }

    private <T> void verifyAvailabilityCbsReceived(HashSet<T> expectedCameras,
            LinkedBlockingQueue<T> queue, LinkedBlockingQueue<T> otherQueue,
            boolean available) throws Exception {
        while (expectedCameras.size() > 0) {
            T id = queue.poll(AVAILABILITY_TIMEOUT_MS,
                    java.util.concurrent.TimeUnit.MILLISECONDS);
            assertTrue("Did not receive initial " + (available ? "available" : "unavailable")
                    + " notices for some cameras", id != null);
            expectedCameras.remove(id);
        }
        // Verify no unavailable/available cameras were reported
        assertTrue("Some camera devices are initially " + (available ? "unavailable" : "available"),
                otherQueue.size() == 0);
    }

    private void testCameraManagerListenerCallbacks(boolean useExecutor) throws Exception {

        final LinkedBlockingQueue<String> availableEventQueue = new LinkedBlockingQueue<>();
        final LinkedBlockingQueue<String> unavailableEventQueue = new LinkedBlockingQueue<>();
        final Executor executor = useExecutor ? new HandlerExecutor(mHandler) : null;

        final LinkedBlockingQueue<Pair<String, String>> availablePhysicalCamEventQueue =
                new LinkedBlockingQueue<>();
        final LinkedBlockingQueue<Pair<String, String>> unavailablePhysicalCamEventQueue =
                new LinkedBlockingQueue<>();

        CameraManager.AvailabilityCallback ac = new CameraManager.AvailabilityCallback() {
            @Override
            public void onCameraAvailable(String cameraId) {
                try {
                    // When we're testing system cameras, we don't list non system cameras in the
                    // camera id list as mentioned in Camera2ParameterizedTest.java
                    if (mAdoptShellPerm &&
                            !CameraTestUtils.isSystemCamera(mCameraManager, cameraId)) {
                        return;
                    }
                } catch (CameraAccessException e) {
                    fail("CameraAccessException thrown when attempting to access camera" +
                         "characteristics" + cameraId);
                }
                availableEventQueue.offer(cameraId);
            }

            @Override
            public void onCameraUnavailable(String cameraId) {
                unavailableEventQueue.offer(cameraId);
            }

            @Override
            public void onPhysicalCameraAvailable(String cameraId, String physicalCameraId) {
                availablePhysicalCamEventQueue.offer(new Pair<>(cameraId, physicalCameraId));
            }

            @Override
            public void onPhysicalCameraUnavailable(String cameraId, String physicalCameraId) {
                unavailablePhysicalCamEventQueue.offer(new Pair<>(cameraId, physicalCameraId));
            }
        };

        if (useExecutor) {
            mCameraManager.registerAvailabilityCallback(executor, ac);
        } else {
            mCameraManager.registerAvailabilityCallback(ac, mHandler);
        }
        String[] cameras = mCameraIdsUnderTest;

        if (cameras.length == 0) {
            Log.i(TAG, "No cameras present, skipping test");
            return;
        }

        // Verify we received available for all cameras' initial state in a reasonable amount of time
        HashSet<String> expectedAvailableCameras = new HashSet<String>(Arrays.asList(cameras));
        verifyAvailabilityCbsReceived(expectedAvailableCameras, availableEventQueue,
                unavailableEventQueue, true /*available*/);

        // Verify transitions for individual cameras
        for (String id : cameras) {
            MockStateCallback mockListener = MockStateCallback.mock();
            mCameraListener = new BlockingStateCallback(mockListener);

            // Clear logical camera callback queue in case the initial state of certain physical
            // cameras are unavailable.
            unavailablePhysicalCamEventQueue.clear();

            if (useExecutor) {
                mCameraManager.openCamera(id, executor, mCameraListener);
            } else {
                mCameraManager.openCamera(id, mCameraListener, mHandler);
            }

            // Block until opened
            mCameraListener.waitForState(BlockingStateCallback.STATE_OPENED,
                    CameraTestUtils.CAMERA_IDLE_TIMEOUT_MS);
            // Then verify only open happened, and get the camera handle
            CameraDevice camera = verifyCameraStateOpened(id, mockListener);

            // Verify that we see the expected 'unavailable' event.
            String candidateId = unavailableEventQueue.poll(AVAILABILITY_TIMEOUT_MS,
                    java.util.concurrent.TimeUnit.MILLISECONDS);
            assertTrue(String.format("Received unavailability notice for wrong ID " +
                            "(expected %s, got %s)", id, candidateId),
                    id.equals(candidateId));
            assertTrue("Availability events received unexpectedly",
                    availableEventQueue.size() == 0);

            // Verify that we see the expected 'unavailable' events if this camera is a physical
            // camera of another logical multi-camera
            HashSet<Pair<String, String>> relatedLogicalCameras = new HashSet<>();
            for (String multiCamId : cameras) {
                CameraCharacteristics props = mCameraManager.getCameraCharacteristics(multiCamId);
                Set<String> physicalIds = props.getPhysicalCameraIds();
                if (physicalIds.contains(id)) {
                    relatedLogicalCameras.add(new Pair<String, String>(multiCamId, id));
                }
            }

            HashSet<Pair<String, String>> expectedLogicalCameras =
                    new HashSet<>(relatedLogicalCameras);
            verifyAvailabilityCbsReceived(expectedLogicalCameras,
                    unavailablePhysicalCamEventQueue, availablePhysicalCamEventQueue,
                    false /*available*/);

            // Verify that we see the expected 'available' event after closing the camera

            camera.close();

            mCameraListener.waitForState(BlockingStateCallback.STATE_CLOSED,
                    CameraTestUtils.CAMERA_CLOSE_TIMEOUT_MS);

            candidateId = availableEventQueue.poll(AVAILABILITY_TIMEOUT_MS,
                    java.util.concurrent.TimeUnit.MILLISECONDS);
            assertTrue(String.format("Received availability notice for wrong ID " +
                            "(expected %s, got %s)", id, candidateId),
                    id.equals(candidateId));
            assertTrue("Unavailability events received unexpectedly",
                    unavailableEventQueue.size() == 0);

            expectedLogicalCameras = new HashSet<Pair<String, String>>(relatedLogicalCameras);
            verifyAvailabilityCbsReceived(expectedLogicalCameras,
                    availablePhysicalCamEventQueue, unavailablePhysicalCamEventQueue,
                    true /*available*/);
        }

        // Verify that we can unregister the listener and see no more events
        assertTrue("Availability events received unexpectedly",
                availableEventQueue.size() == 0);
        assertTrue("Unavailability events received unexpectedly",
                    unavailableEventQueue.size() == 0);

        mCameraManager.unregisterAvailabilityCallback(ac);

        {
            // Open an arbitrary camera and make sure we don't hear about it

            MockStateCallback mockListener = MockStateCallback.mock();
            mCameraListener = new BlockingStateCallback(mockListener);

            if (useExecutor) {
                mCameraManager.openCamera(cameras[0], executor, mCameraListener);
            } else {
                mCameraManager.openCamera(cameras[0], mCameraListener, mHandler);
            }

            // Block until opened
            mCameraListener.waitForState(BlockingStateCallback.STATE_OPENED,
                    CameraTestUtils.CAMERA_IDLE_TIMEOUT_MS);
            // Then verify only open happened, and close the camera
            CameraDevice camera = verifyCameraStateOpened(cameras[0], mockListener);

            camera.close();

            mCameraListener.waitForState(BlockingStateCallback.STATE_CLOSED,
                    CameraTestUtils.CAMERA_CLOSE_TIMEOUT_MS);

            // No unavailability or availability callback should have occured
            String candidateId = unavailableEventQueue.poll(AVAILABILITY_TIMEOUT_MS,
                    java.util.concurrent.TimeUnit.MILLISECONDS);
            assertTrue(String.format("Received unavailability notice for ID %s unexpectedly ",
                            candidateId),
                    candidateId == null);

            candidateId = availableEventQueue.poll(AVAILABILITY_TIMEOUT_MS,
                    java.util.concurrent.TimeUnit.MILLISECONDS);
            assertTrue(String.format("Received availability notice for ID %s unexpectedly ",
                            candidateId),
                    candidateId == null);

            Pair<String, String> candidatePhysicalIds = unavailablePhysicalCamEventQueue.poll(
                    AVAILABILITY_TIMEOUT_MS, java.util.concurrent.TimeUnit.MILLISECONDS);
            assertTrue("Received unavailability physical camera notice unexpectedly ",
                    candidatePhysicalIds == null);

            candidatePhysicalIds = availablePhysicalCamEventQueue.poll(
                    AVAILABILITY_TIMEOUT_MS, java.util.concurrent.TimeUnit.MILLISECONDS);
            assertTrue("Received availability notice for physical camera unexpectedly ",
                    candidatePhysicalIds == null);
        }

    } // testCameraManagerListenerCallbacks

    // Verify no LEGACY-level devices appear on devices first launched in the Q release or newer
    @Test
    public void testNoLegacyOnQ() throws Exception {
        if(PropertyUtil.getFirstApiLevel() < Build.VERSION_CODES.Q){
            // LEGACY still allowed for devices upgrading to Q
            return;
        }
        String[] ids = mCameraIdsUnderTest;
        for (int i = 0; i < ids.length; i++) {
            CameraCharacteristics props = mCameraManager.getCameraCharacteristics(ids[i]);
            assertNotNull(
                    String.format("Can't get camera characteristics from: ID %s", ids[i]), props);
            Integer hardwareLevel = props.get(CameraCharacteristics.INFO_SUPPORTED_HARDWARE_LEVEL);
            assertNotNull(
                    String.format("Can't get hardware level from: ID %s", ids[i]), hardwareLevel);
            assertTrue(String.format(
                            "Camera device %s cannot be LEGACY level for devices launching on Q",
                            ids[i]),
                    hardwareLevel != CameraCharacteristics.INFO_SUPPORTED_HARDWARE_LEVEL_LEGACY);
        }
    }

    @Test
    public void testCameraManagerWithDnD() throws Exception {
        String[] cameras = mCameraIdsUnderTest;
        if (cameras.length == 0) {
            Log.i(TAG, "No cameras present, skipping test");
            return;
        }
        // Allow the test package to adjust notification policy
        toggleNotificationPolicyAccess(mContext.getPackageName(),
                InstrumentationRegistry.getInstrumentation(), true);

        // Enable DnD filtering

        NotificationManager nm = mContext.getSystemService(NotificationManager.class);
        try {
            nm.setInterruptionFilter(NotificationManager.INTERRUPTION_FILTER_NONE);

            // Try to use the camera API

            for (String cameraId : cameras) {
                try {
                    CameraCharacteristics c = mCameraManager.getCameraCharacteristics(cameraId);
                    assertTrue("Unable to get camera characteristics when DnD is enabled",
                            c != null);
                } catch (RuntimeException e) {
                    fail("RuntimeException thrown when attempting to access camera " +
                            "characteristics with DnD enabled. " +
                            "https://android-review.googlesource.com/c/platform/frameworks/base/+" +
                            "/747089/ may be missing.");
                }
            }
        } finally {
            // Restore notifications to normal

            nm.setInterruptionFilter(NotificationManager.INTERRUPTION_FILTER_ALL);
        }
    }

    private void toggleNotificationPolicyAccess(String packageName,
            Instrumentation instrumentation, boolean on) throws IOException {

        String command = " cmd notification " + (on ? "allow_dnd " : "disallow_dnd ") + packageName;

        runCommand(command, instrumentation);

        NotificationManager nm = mContext.getSystemService(NotificationManager.class);
        assertEquals("Notification Policy Access Grant is " +
                nm.isNotificationPolicyAccessGranted() + " not " + on, on,
                nm.isNotificationPolicyAccessGranted());
    }

    private void runCommand(String command, Instrumentation instrumentation) throws IOException {
        UiAutomation uiAutomation = instrumentation.getUiAutomation();
        // Execute command
        ParcelFileDescriptor fd = mUiAutomation.executeShellCommand(command);
        assertNotNull("Failed to execute shell command: " + command, fd);
        // Wait for the command to finish by reading until EOF
        try (InputStream in = new FileInputStream(fd.getFileDescriptor())) {
            byte[] buffer = new byte[4096];
            while (in.read(buffer) > 0) {}
        } catch (IOException e) {
            throw new IOException("Could not read stdout of command: " + command, e);
        }
    }

}
