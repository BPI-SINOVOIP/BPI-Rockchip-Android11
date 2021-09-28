/*
 * Copyright (C) 2019 The Android Open Source Project
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

package android.hardware.camera2.cts.testcases;

import static android.hardware.camera2.cts.CameraTestUtils.*;
import static com.android.ex.camera2.blocking.BlockingStateCallback.*;

import android.content.Context;
import android.graphics.Rect;

import android.hardware.camera2.cts.CameraTestUtils;
import android.hardware.camera2.CameraCaptureSession;
import android.hardware.camera2.CameraCaptureSession.CaptureCallback;
import android.hardware.camera2.CameraCharacteristics;
import android.hardware.camera2.CameraDevice;
import android.hardware.camera2.CameraManager;
import android.hardware.camera2.CaptureRequest;
import android.hardware.camera2.params.OutputConfiguration;
import android.hardware.camera2.params.SessionConfiguration;
import android.util.Size;
import android.hardware.camera2.cts.helpers.CameraErrorCollector;
import android.hardware.camera2.cts.helpers.StaticMetadata;
import android.hardware.camera2.cts.helpers.StaticMetadata.CheckLevel;
import android.media.Image;
import android.media.Image.Plane;
import android.media.ImageReader;
import android.os.Bundle;
import android.os.Handler;
import android.os.HandlerThread;
import android.util.Log;
import android.view.Surface;
import android.view.WindowManager;

import androidx.test.InstrumentationRegistry;

import com.android.ex.camera2.blocking.BlockingSessionCallback;
import com.android.ex.camera2.blocking.BlockingStateCallback;

import org.junit.rules.ExternalResource;

import java.io.File;
import java.nio.ByteBuffer;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.HashMap;
import java.util.List;

public class Camera2AndroidTestRule extends ExternalResource {
    private static final String TAG = "Camera2AndroidBasicTestCase";
    private static final boolean VERBOSE = Log.isLoggable(TAG, Log.VERBOSE);

    // Default capture size: VGA size is required by CDD.
    protected static final Size DEFAULT_CAPTURE_SIZE = new Size(640, 480);
    protected static final int CAPTURE_WAIT_TIMEOUT_MS = 5000;

    private CameraManager mCameraManager;
    private CameraDevice mCamera;
    private CameraCaptureSession mCameraSession;
    private BlockingSessionCallback mCameraSessionListener;
    private BlockingStateCallback mCameraListener;
    private String[] mCameraIdsUnderTest;
    // include both standalone camera IDs and "hidden" physical camera IDs
    private String[] mAllCameraIds;
    private HashMap<String, StaticMetadata> mAllStaticInfo;
    private ImageReader mReader;
    private Surface mReaderSurface;
    private Handler mHandler;
    private HandlerThread mHandlerThread;
    private StaticMetadata mStaticInfo;
    private CameraErrorCollector mCollector;
    private List<Size> mOrderedPreviewSizes; // In descending order.
    private List<Size> mOrderedVideoSizes; // In descending order.
    private List<Size> mOrderedStillSizes; // In descending order.
    private String mDebugFileNameBase;

    private WindowManager mWindowManager;
    private Context mContext;

    private static final String CAMERA_ID_INSTR_ARG_KEY = "camera-id";
    private static final String CAMERA_PERF_MEASURE = "perf-measure";
    private static final Bundle mBundle = InstrumentationRegistry.getArguments();
    private static final String mOverrideCameraId = mBundle.getString(CAMERA_ID_INSTR_ARG_KEY);
    private static final String mPerfMeasure = mBundle.getString(CAMERA_PERF_MEASURE);

    public Camera2AndroidTestRule(Context context) {
        mContext = context;
    }

    public Context getContext() {
        return mContext;
    }

    public String[] getCameraIdsUnderTest() {
        return mCameraIdsUnderTest;
    }

    public StaticMetadata getStaticInfo() {
        return mStaticInfo;
    }

    public CameraManager getCameraManager() {
        return mCameraManager;
    }

    public void setStaticInfo(StaticMetadata staticInfo) {
        mStaticInfo = staticInfo;
    }

    public CameraCaptureSession getCameraSession() {
        return mCameraSession;
    }

    public CameraDevice getCamera() {
        return mCamera;
    }

    public void setCamera(CameraDevice camera) {
        mCamera = camera;
    }

    public void setCameraSession(CameraCaptureSession session) {
        mCameraSession = session;
    }

    public BlockingStateCallback getCameraListener() {
        return mCameraListener;
    }

    public BlockingSessionCallback getCameraSessionListener() {
        return mCameraSessionListener;
    }

    public Handler getHandler() {
        return mHandler;
    }

    public void setCameraSessionListener(BlockingSessionCallback listener) {
        mCameraSessionListener = listener;
    }

    public ImageReader getReader() {
        return mReader;
    }

    public HashMap<String, StaticMetadata> getAllStaticInfo() {
        return mAllStaticInfo;
    }

    public List<Size> getOrderedPreviewSizes() {
        return mOrderedPreviewSizes;
    }

    public List<Size> getOrderedStillSizes() {
        return mOrderedStillSizes;
    }

    public Surface getReaderSurface() {
        return mReaderSurface;
    }

    public void setOrderedPreviewSizes(List<Size> sizes) {
        mOrderedPreviewSizes = sizes;
    }

    public WindowManager getWindowManager() {
        return mWindowManager;
    }

    public CameraErrorCollector getCollector() {
        return mCollector;
    }

    public boolean isPerfMeasure() {
        return mPerfMeasure != null && mPerfMeasure.equals("on");
    }

    private String[] deriveCameraIdsUnderTest() throws Exception {
        String[] idsUnderTest = mCameraManager.getCameraIdList();
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

    /**
     * Set up the camera2 test case required environments, including CameraManager,
     * HandlerThread, Camera IDs, and CameraStateCallback etc.
     */
    @Override
    public void before() throws Exception {
        Log.v(TAG, "Set up...");
        mCameraManager = (CameraManager) mContext.getSystemService(Context.CAMERA_SERVICE);
        assertNotNull("Can't connect to camera manager!", mCameraManager);
        mWindowManager = (WindowManager) mContext.getSystemService(Context.WINDOW_SERVICE);
        /**
         * Workaround for mockito and JB-MR2 incompatibility
         *
         * Avoid java.lang.IllegalArgumentException: dexcache == null
         * https://code.google.com/p/dexmaker/issues/detail?id=2
         */
        System.setProperty("dexmaker.dexcache", getContext().getCacheDir().toString());

        mCameraIdsUnderTest = deriveCameraIdsUnderTest();
        mHandlerThread = new HandlerThread(TAG);
        mHandlerThread.start();
        mHandler = new Handler(mHandlerThread.getLooper());
        mCameraListener = new BlockingStateCallback();
        mCollector = new CameraErrorCollector();

        File filesDir = mContext.getPackageManager().isInstantApp()
                ? mContext.getFilesDir()
                : mContext.getExternalFilesDir(null);

        mDebugFileNameBase = filesDir.getPath();

        mAllStaticInfo = new HashMap<String, StaticMetadata>();
        List<String> hiddenPhysicalIds = new ArrayList<>();
        for (String cameraId : mCameraIdsUnderTest) {
            CameraCharacteristics props = mCameraManager.getCameraCharacteristics(cameraId);
            StaticMetadata staticMetadata = new StaticMetadata(props,
                    CheckLevel.ASSERT, /*collector*/null);
            mAllStaticInfo.put(cameraId, staticMetadata);

            for (String physicalId : props.getPhysicalCameraIds()) {
                if (!Arrays.asList(mCameraIdsUnderTest).contains(physicalId) &&
                        !hiddenPhysicalIds.contains(physicalId)) {
                    hiddenPhysicalIds.add(physicalId);
                    props = mCameraManager.getCameraCharacteristics(physicalId);
                    staticMetadata = new StaticMetadata(
                            mCameraManager.getCameraCharacteristics(physicalId),
                            CheckLevel.ASSERT, /*collector*/null);
                    mAllStaticInfo.put(physicalId, staticMetadata);
                }
            }
        }
        mAllCameraIds = new String[mCameraIdsUnderTest.length + hiddenPhysicalIds.size()];
        System.arraycopy(mCameraIdsUnderTest, 0, mAllCameraIds, 0, mCameraIdsUnderTest.length);
        for (int i = 0; i < hiddenPhysicalIds.size(); i++) {
            mAllCameraIds[mCameraIdsUnderTest.length + i] = hiddenPhysicalIds.get(i);
        }
    }

    @Override
    public void after() {
        Log.v(TAG, "Tear down...");
        if (mCameraManager != null) {
            try {
                String[] cameraIdsPostTest = deriveCameraIdsUnderTest();
                Log.i(TAG, "Camera ids in setup:" + Arrays.toString(mCameraIdsUnderTest));
                Log.i(TAG, "Camera ids in tearDown:" + Arrays.toString(cameraIdsPostTest));
                assertTrue(
                        "Number of cameras changed from " + mCameraIdsUnderTest.length + " to " +
                                cameraIdsPostTest.length,
                        mCameraIdsUnderTest.length == cameraIdsPostTest.length);
                mHandlerThread.quitSafely();
                mHandler = null;
                closeDefaultImageReader();
                mCollector.verify();
            } catch (Throwable e) {
                // When new Exception(e) is used, exception info will be printed twice.
                throw new RuntimeException(e.getMessage());
            }
        }
    }

    /**
     * Start capture with given {@link #CaptureRequest}.
     *
     * @param request The {@link #CaptureRequest} to be captured.
     * @param repeating If the capture is single capture or repeating.
     * @param listener The {@link #CaptureCallback} camera device used to notify callbacks.
     * @param handler The handler camera device used to post callbacks.
     */
    public void startCapture(CaptureRequest request, boolean repeating,
            CaptureCallback listener, Handler handler) throws Exception {
        if (VERBOSE) Log.v(TAG, "Starting capture from device");

        if (repeating) {
            mCameraSession.setRepeatingRequest(request, listener, handler);
        } else {
            mCameraSession.capture(request, listener, handler);
        }
    }

    /**
     * Stop the current active capture.
     *
     * @param fast When it is true, {@link CameraDevice#flush} is called, the stop capture
     * could be faster.
     */
    public void stopCapture(boolean fast) throws Exception {
        if (VERBOSE) Log.v(TAG, "Stopping capture");

        if (fast) {
            /**
             * Flush is useful for canceling long exposure single capture, it also could help
             * to make the streaming capture stop sooner.
             */
            mCameraSession.abortCaptures();
            mCameraSessionListener.getStateWaiter().
                    waitForState(BlockingSessionCallback.SESSION_READY, CAMERA_IDLE_TIMEOUT_MS);
        } else {
            mCameraSession.close();
            mCameraSessionListener.getStateWaiter().
                    waitForState(BlockingSessionCallback.SESSION_CLOSED, CAMERA_IDLE_TIMEOUT_MS);
        }
    }

    /**
     * Open a {@link #CameraDevice camera device} and get the StaticMetadata for a given camera id.
     * The default mCameraListener is used to wait for states.
     *
     * @param cameraId The id of the camera device to be opened.
     */
    public void openDevice(String cameraId) throws Exception {
        openDevice(cameraId, mCameraListener);
    }

    /**
     * Open a {@link #CameraDevice} and get the StaticMetadata for a given camera id and listener.
     *
     * @param cameraId The id of the camera device to be opened.
     * @param listener The {@link #BlockingStateCallback} used to wait for states.
     */
    public void openDevice(String cameraId, BlockingStateCallback listener) throws Exception {
        mCamera = CameraTestUtils.openCamera(
                mCameraManager, cameraId, listener, mHandler);
        mCollector.setCameraId(cameraId);
        mStaticInfo = mAllStaticInfo.get(cameraId);
        if (mStaticInfo.isColorOutputSupported()) {
            mOrderedPreviewSizes = getSupportedPreviewSizes(
                    cameraId, mCameraManager,
                    getPreviewSizeBound(mWindowManager, PREVIEW_SIZE_BOUND));
            mOrderedVideoSizes = getSupportedVideoSizes(cameraId, mCameraManager, PREVIEW_SIZE_BOUND);
            mOrderedStillSizes = getSupportedStillSizes(cameraId, mCameraManager, null);
        }

        if (VERBOSE) {
            Log.v(TAG, "Camera " + cameraId + " is opened");
        }
    }

    /**
     * Create a {@link #CameraCaptureSession} using the currently open camera.
     *
     * @param outputSurfaces The set of output surfaces to configure for this session
     */
    public void createSession(List<Surface> outputSurfaces) throws Exception {
        mCameraSessionListener = new BlockingSessionCallback();
        mCameraSession = CameraTestUtils.configureCameraSession(mCamera, outputSurfaces,
                mCameraSessionListener, mHandler);
    }

    /**
     * Create a {@link #CameraCaptureSession} using the currently open camera with
     * OutputConfigurations.
     *
     * @param outputSurfaces The set of output surfaces to configure for this session
     */
    public void createSessionByConfigs(List<OutputConfiguration> outputConfigs) throws Exception {
        mCameraSessionListener = new BlockingSessionCallback();
        mCameraSession = CameraTestUtils.configureCameraSessionWithConfig(mCamera, outputConfigs,
                mCameraSessionListener, mHandler);
    }

    /**
     * Close a {@link #CameraDevice camera device} and clear the associated StaticInfo field for a
     * given camera id. The default mCameraListener is used to wait for states.
     * <p>
     * This function must be used along with the {@link #openDevice} for the
     * same camera id.
     * </p>
     *
     * @param cameraId The id of the {@link #CameraDevice camera device} to be closed.
     */
    public void closeDevice(String cameraId) {
        closeDevice(cameraId, mCameraListener);
    }

    /**
     * Close a {@link #CameraDevice camera device} and clear the associated StaticInfo field for a
     * given camera id and listener.
     * <p>
     * This function must be used along with the {@link #openDevice} for the
     * same camera id.
     * </p>
     *
     * @param cameraId The id of the camera device to be closed.
     * @param listener The BlockingStateCallback used to wait for states.
     */
    public void closeDevice(String cameraId, BlockingStateCallback listener) {
        if (mCamera != null) {
            if (!cameraId.equals(mCamera.getId())) {
                throw new IllegalStateException("Try to close a device that is not opened yet");
            }
            mCamera.close();
            listener.waitForState(STATE_CLOSED, CAMERA_CLOSE_TIMEOUT_MS);
            mCamera = null;
            mCameraSession = null;
            mCameraSessionListener = null;
            mStaticInfo = null;
            mOrderedPreviewSizes = null;
            mOrderedVideoSizes = null;
            mOrderedStillSizes = null;

            if (VERBOSE) {
                Log.v(TAG, "Camera " + cameraId + " is closed");
            }
        }
    }

    /**
     * Create an {@link ImageReader} object and get the surface.
     * <p>
     * This function creates {@link ImageReader} object and surface, then assign
     * to the default {@link mReader} and {@link mReaderSurface}. It closes the
     * current default active {@link ImageReader} if it exists.
     * </p>
     *
     * @param size The size of this ImageReader to be created.
     * @param format The format of this ImageReader to be created
     * @param maxNumImages The max number of images that can be acquired
     *            simultaneously.
     * @param listener The listener used by this ImageReader to notify
     *            callbacks.
     */
    public void createDefaultImageReader(Size size, int format, int maxNumImages,
            ImageReader.OnImageAvailableListener listener) throws Exception {
        closeDefaultImageReader();

        mReader = createImageReader(size, format, maxNumImages, listener);
        mReaderSurface = mReader.getSurface();
        if (VERBOSE) Log.v(TAG, "Created ImageReader size " + size.toString());
    }

    /**
     * Create an {@link ImageReader} object and get the surface.
     * <p>
     * This function creates {@link ImageReader} object and surface, then assign
     * to the default {@link mReader} and {@link mReaderSurface}. It closes the
     * current default active {@link ImageReader} if it exists.
     * </p>
     *
     * @param size The size of this ImageReader to be created.
     * @param format The format of this ImageReader to be created
     * @param maxNumImages The max number of images that can be acquired
     *            simultaneously.
     * @param usage The usage flag of the ImageReader
     * @param listener The listener used by this ImageReader to notify
     *            callbacks.
     */
    public void createDefaultImageReader(Size size, int format, int maxNumImages, long usage,
            ImageReader.OnImageAvailableListener listener) throws Exception {
        closeDefaultImageReader();

        mReader = createImageReader(size, format, maxNumImages, usage, listener);
        mReaderSurface = mReader.getSurface();
        if (VERBOSE) Log.v(TAG, "Created ImageReader size " + size.toString());
    }

    /**
     * Create an {@link ImageReader} object.
     *
     * <p>This function creates image reader object for given format, maxImages, and size.</p>
     *
     * @param size The size of this ImageReader to be created.
     * @param format The format of this ImageReader to be created
     * @param maxNumImages The max number of images that can be acquired simultaneously.
     * @param listener The listener used by this ImageReader to notify callbacks.
     */

    public ImageReader createImageReader(Size size, int format, int maxNumImages,
            ImageReader.OnImageAvailableListener listener) throws Exception {

        ImageReader reader = null;
        reader = ImageReader.newInstance(size.getWidth(), size.getHeight(),
                format, maxNumImages);

        reader.setOnImageAvailableListener(listener, mHandler);
        if (VERBOSE) Log.v(TAG, "Created ImageReader size " + size.toString());
        return reader;
    }

    /**
     * Create an {@link ImageReader} object.
     *
     * <p>This function creates image reader object for given format, maxImages, usage and size.</p>
     *
     * @param size The size of this ImageReader to be created.
     * @param format The format of this ImageReader to be created
     * @param maxNumImages The max number of images that can be acquired simultaneously.
     * @param usage The usage flag of the ImageReader
     * @param listener The listener used by this ImageReader to notify callbacks.
     */

    public ImageReader createImageReader(Size size, int format, int maxNumImages, long usage,
            ImageReader.OnImageAvailableListener listener) throws Exception {
        ImageReader reader = null;
        reader = ImageReader.newInstance(size.getWidth(), size.getHeight(),
                format, maxNumImages, usage);

        reader.setOnImageAvailableListener(listener, mHandler);
        if (VERBOSE) Log.v(TAG, "Created ImageReader size " + size.toString());
        return reader;
    }

    /**
     * Close the pending images then close current default {@link ImageReader} object.
     */
    public void closeDefaultImageReader() {
        closeImageReader(mReader);
        mReader = null;
        mReaderSurface = null;
    }

    /**
     * Close an image reader instance.
     *
     * @param reader
     */
    public void closeImageReader(ImageReader reader) {
        if (reader != null) {
            try {
                // Close all possible pending images first.
                Image image = reader.acquireLatestImage();
                if (image != null) {
                    image.close();
                }
            } finally {
                reader.close();
                reader = null;
            }
        }
    }

    public void checkImageReaderSessionConfiguration(String msg) throws Exception {
        List<OutputConfiguration> outputConfigs = new ArrayList<OutputConfiguration>();
        outputConfigs.add(new OutputConfiguration(mReaderSurface));

        checkSessionConfigurationSupported(mCamera, mHandler, outputConfigs, /*inputConfig*/ null,
                SessionConfiguration.SESSION_REGULAR, /*expectedResult*/ true, msg);
    }

    public CaptureRequest prepareCaptureRequest() throws Exception {
        return prepareCaptureRequest(CameraDevice.TEMPLATE_PREVIEW);
    }

    public CaptureRequest prepareCaptureRequest(int template) throws Exception {
        List<Surface> outputSurfaces = new ArrayList<Surface>();
        Surface surface = mReader.getSurface();
        assertNotNull("Fail to get surface from ImageReader", surface);
        outputSurfaces.add(surface);
        return prepareCaptureRequestForSurfaces(outputSurfaces, template)
                .build();
    }

    public CaptureRequest.Builder prepareCaptureRequestForSurfaces(List<Surface> surfaces,
            int template)
            throws Exception {
        createSession(surfaces);

        CaptureRequest.Builder captureBuilder =
                mCamera.createCaptureRequest(template);
        assertNotNull("Fail to get captureRequest", captureBuilder);
        for (Surface surface : surfaces) {
            captureBuilder.addTarget(surface);
        }

        return captureBuilder;
    }

    public CaptureRequest.Builder prepareCaptureRequestForConfigs(
            List<OutputConfiguration> outputConfigs, int template) throws Exception {
        createSessionByConfigs(outputConfigs);

        CaptureRequest.Builder captureBuilder =
                mCamera.createCaptureRequest(template);
        assertNotNull("Fail to get captureRequest", captureBuilder);
        for (OutputConfiguration config : outputConfigs) {
            for (Surface s : config.getSurfaces()) {
                captureBuilder.addTarget(s);
            }
        }

        return captureBuilder;
    }

    /**
     * Test the invalid Image access: accessing a closed image must result in
     * {@link IllegalStateException}.
     *
     * @param closedImage The closed image.
     * @param closedBuffer The ByteBuffer from a closed Image. buffer invalid
     *            access will be skipped if it is null.
     */
    public void imageInvalidAccessTestAfterClose(Image closedImage,
            Plane closedPlane, ByteBuffer closedBuffer) {
        if (closedImage == null) {
            throw new IllegalArgumentException(" closedImage must be non-null");
        }
        if (closedBuffer != null && !closedBuffer.isDirect()) {
            throw new IllegalArgumentException("The input ByteBuffer should be direct ByteBuffer");
        }

        if (closedPlane != null) {
            // Plane#getBuffer test
            try {
                closedPlane.getBuffer(); // An ISE should be thrown here.
                fail("Image should throw IllegalStateException when calling getBuffer"
                        + " after the image is closed");
            } catch (IllegalStateException e) {
                // Expected.
            }

            // Plane#getPixelStride test
            try {
                closedPlane.getPixelStride(); // An ISE should be thrown here.
                fail("Image should throw IllegalStateException when calling getPixelStride"
                        + " after the image is closed");
            } catch (IllegalStateException e) {
                // Expected.
            }

            // Plane#getRowStride test
            try {
                closedPlane.getRowStride(); // An ISE should be thrown here.
                fail("Image should throw IllegalStateException when calling getRowStride"
                        + " after the image is closed");
            } catch (IllegalStateException e) {
                // Expected.
            }
        }

        // ByteBuffer access test
        if (closedBuffer != null) {
            try {
                closedBuffer.get(); // An ISE should be thrown here.
                fail("Image should throw IllegalStateException when accessing a byte buffer"
                        + " after the image is closed");
            } catch (IllegalStateException e) {
                // Expected.
            }
        }

        // Image#getFormat test
        try {
            closedImage.getFormat();
            fail("Image should throw IllegalStateException when calling getFormat"
                    + " after the image is closed");
        } catch (IllegalStateException e) {
            // Expected.
        }

        // Image#getWidth test
        try {
            closedImage.getWidth();
            fail("Image should throw IllegalStateException when calling getWidth"
                    + " after the image is closed");
        } catch (IllegalStateException e) {
            // Expected.
        }

        // Image#getHeight test
        try {
            closedImage.getHeight();
            fail("Image should throw IllegalStateException when calling getHeight"
                    + " after the image is closed");
        } catch (IllegalStateException e) {
            // Expected.
        }

        // Image#getTimestamp test
        try {
            closedImage.getTimestamp();
            fail("Image should throw IllegalStateException when calling getTimestamp"
                    + " after the image is closed");
        } catch (IllegalStateException e) {
            // Expected.
        }

        // Image#getTimestamp test
        try {
            closedImage.getTimestamp();
            fail("Image should throw IllegalStateException when calling getTimestamp"
                    + " after the image is closed");
        } catch (IllegalStateException e) {
            // Expected.
        }

        // Image#getCropRect test
        try {
            closedImage.getCropRect();
            fail("Image should throw IllegalStateException when calling getCropRect"
                    + " after the image is closed");
        } catch (IllegalStateException e) {
            // Expected.
        }

        // Image#setCropRect test
        try {
            Rect rect = new Rect();
            closedImage.setCropRect(rect);
            fail("Image should throw IllegalStateException when calling setCropRect"
                    + " after the image is closed");
        } catch (IllegalStateException e) {
            // Expected.
        }

        // Image#getPlanes test
        try {
            closedImage.getPlanes();
            fail("Image should throw IllegalStateException when calling getPlanes"
                    + " after the image is closed");
        } catch (IllegalStateException e) {
            // Expected.
        }
    }
}
