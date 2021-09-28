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
package com.android.cts.verifier.camera.bokeh;

import com.android.cts.verifier.PassFailButtons;
import com.android.cts.verifier.R;

import com.android.ex.camera2.blocking.BlockingCameraManager;
import com.android.ex.camera2.blocking.BlockingCameraManager.BlockingOpenException;
import com.android.ex.camera2.blocking.BlockingStateCallback;
import com.android.ex.camera2.blocking.BlockingSessionCallback;

import android.app.AlertDialog;
import android.content.res.Configuration;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.graphics.Color;
import android.graphics.ColorFilter;
import android.graphics.ColorMatrixColorFilter;
import android.graphics.ImageFormat;
import android.graphics.Matrix;
import android.graphics.RectF;
import android.graphics.SurfaceTexture;
import android.hardware.camera2.CameraAccessException;
import android.hardware.camera2.CameraCaptureSession;
import android.hardware.camera2.CameraCharacteristics;
import android.hardware.camera2.CameraCharacteristics.Key;
import android.hardware.camera2.CameraDevice;
import android.hardware.camera2.CameraManager;
import android.hardware.camera2.CameraMetadata;
import android.hardware.camera2.CaptureRequest;
import android.hardware.camera2.CaptureResult;
import android.hardware.camera2.params.Capability;
import android.hardware.camera2.params.StreamConfigurationMap;
import android.hardware.camera2.TotalCaptureResult;
import android.media.Image;
import android.media.ImageReader;
import android.os.Bundle;
import android.os.Handler;
import android.os.HandlerThread;
import android.util.Log;
import android.util.Size;
import android.util.SparseArray;
import android.view.Menu;
import android.view.MenuItem;
import android.view.View;
import android.view.Surface;
import android.view.TextureView;
import android.widget.AdapterView;
import android.widget.ArrayAdapter;
import android.widget.Button;
import android.widget.ImageButton;
import android.widget.ImageView;
import android.widget.Spinner;
import android.widget.TextView;
import android.widget.Toast;
import android.content.Context;

import java.lang.Math;
import java.nio.ByteBuffer;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.HashMap;
import java.util.Comparator;
import java.util.List;
import java.util.Optional;
import java.util.Set;
import java.util.TreeSet;

/**
 * Tests for manual verification of bokeh modes supported by the camera device.
 */
public class CameraBokehActivity extends PassFailButtons.Activity
        implements TextureView.SurfaceTextureListener,
                   ImageReader.OnImageAvailableListener {

    private static final String TAG = "CameraBokehActivity";
    private static final boolean VERBOSE = Log.isLoggable(TAG, Log.VERBOSE);
    private static final int SESSION_READY_TIMEOUT_MS = 5000;
    private static final Size FULLHD = new Size(1920, 1080);
    private static final ColorMatrixColorFilter sJFIF_YUVToRGB_Filter =
            new ColorMatrixColorFilter(new float[] {
                        1f,        0f,    1.402f, 0f, -179.456f,
                        1f, -0.34414f, -0.71414f, 0f,   135.46f,
                        1f,    1.772f,        0f, 0f, -226.816f,
                        0f,        0f,        0f, 1f,        0f
                    });

    private TextureView mPreviewView;
    private SurfaceTexture mPreviewTexture;
    private Surface mPreviewSurface;
    private int mPreviewTexWidth, mPreviewTexHeight;

    private ImageView mImageView;
    private ColorFilter mCurrentColorFilter;

    private Spinner mCameraSpinner;
    private TextView mTestLabel;
    private TextView mPreviewLabel;
    private TextView mImageLabel;

    private CameraManager mCameraManager;
    private String[] mCameraIdList;
    private HandlerThread mCameraThread;
    private Handler mCameraHandler;
    private BlockingCameraManager mBlockingCameraManager;
    private CameraCharacteristics mCameraCharacteristics;
    private BlockingStateCallback mCameraListener;

    private BlockingSessionCallback mSessionListener;
    private CaptureRequest.Builder mPreviewRequestBuilder;
    private CaptureRequest mPreviewRequest;
    private CaptureRequest.Builder mStillCaptureRequestBuilder;
    private CaptureRequest mStillCaptureRequest;

    private HashMap<String, ArrayList<Capability>> mTestCases = new HashMap<>();
    private int mCurrentCameraIndex = -1;
    private String mCameraId;
    private CameraCaptureSession mCaptureSession;
    private CameraDevice mCameraDevice;

    SizeComparator mSizeComparator = new SizeComparator();

    private Size mPreviewSize;
    private Size mJpegSize;
    private ImageReader mJpegImageReader;
    private ImageReader mYuvImageReader;

    private SparseArray<String> mModeNames;

    private CameraCombination mNextCombination;
    private Size mMaxBokehStreamingSize;

    private Button mNextButton;

    private final TreeSet<CameraCombination> mTestedCombinations = new TreeSet<>(COMPARATOR);
    private final TreeSet<CameraCombination> mUntestedCombinations = new TreeSet<>(COMPARATOR);
    private final TreeSet<String> mUntestedCameras = new TreeSet<>();

    // Menu to show the test progress
    private static final int MENU_ID_PROGRESS = Menu.FIRST + 1;

    private class CameraCombination {
        private final int mCameraIndex;
        private final int mMode;
        private final Size mPreviewSize;
        private final boolean mIsStillCapture;
        private final String mCameraId;
        private final String mModeName;

        private CameraCombination(int cameraIndex, int mode,
                int streamingWidth, int streamingHeight,
                String cameraId, String modeName,
                boolean isStillCapture) {
            this.mCameraIndex = cameraIndex;
            this.mMode = mode;
            this.mPreviewSize = new Size(streamingWidth, streamingHeight);
            this.mCameraId = cameraId;
            this.mModeName = modeName;
            this.mIsStillCapture = isStillCapture;
        }

        @Override
        public String toString() {
            return String.format("Camera %s, mode %s, intent %s",
                    mCameraId, mModeName, mIsStillCapture ? "PREVIEW" : "STILL_CAPTURE");
        }
    }

    private static final Comparator<CameraCombination> COMPARATOR =
        Comparator.<CameraCombination, Integer>comparing(c -> c.mCameraIndex)
            .thenComparing(c -> c.mMode)
            .thenComparing(c -> c.mIsStillCapture);

    private CameraCaptureSession.CaptureCallback mCaptureCallback =
            new CameraCaptureSession.CaptureCallback() {
        @Override
        public void onCaptureProgressed(CameraCaptureSession session,
                                        CaptureRequest request,
                                        CaptureResult partialResult) {
            // Don't need to do anything here.
        }

        @Override
        public void onCaptureCompleted(CameraCaptureSession session,
                                       CaptureRequest request,
                                       TotalCaptureResult result) {
            // Don't need to do anything here.
        }
    };

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        setContentView(R.layout.cb_main);

        setPassFailButtonClickListeners();

        mPreviewView = (TextureView) findViewById(R.id.preview_view);
        mImageView = (ImageView) findViewById(R.id.image_view);

        mPreviewView.setSurfaceTextureListener(this);

        mCameraManager = (CameraManager) getSystemService(Context.CAMERA_SERVICE);
        try {
            mCameraIdList = mCameraManager.getCameraIdList();
            for (String id : mCameraIdList) {
                CameraCharacteristics characteristics =
                        mCameraManager.getCameraCharacteristics(id);
                Key<Capability[]> key =
                        CameraCharacteristics.CONTROL_AVAILABLE_EXTENDED_SCENE_MODE_CAPABILITIES;
                Capability[] extendedSceneModeCaps = characteristics.get(key);

                if (extendedSceneModeCaps == null) {
                    continue;
                }

                ArrayList<Capability> nonOffModes = new ArrayList<>();
                for (Capability cap : extendedSceneModeCaps) {
                    int mode = cap.getMode();
                    if (mode == CameraMetadata.CONTROL_EXTENDED_SCENE_MODE_BOKEH_STILL_CAPTURE ||
                            mode == CameraMetadata.CONTROL_EXTENDED_SCENE_MODE_BOKEH_CONTINUOUS) {
                        nonOffModes.add(cap);
                    }
                }

                if (nonOffModes.size() > 0) {
                    mUntestedCameras.add("All combinations for Camera " + id);
                    mTestCases.put(id, nonOffModes);
                }

            }
        } catch (CameraAccessException e) {
            e.printStackTrace();
        }

        // If no supported bokeh modes, mark the test as pass
        if (mTestCases.size() == 0) {
            setInfoResources(R.string.camera_bokeh_test, R.string.camera_bokeh_no_support, -1);
            setPassButtonEnabled(true);
        } else {
            setInfoResources(R.string.camera_bokeh_test, R.string.camera_bokeh_test_info, -1);
            // disable "Pass" button until all combinations are tested
            setPassButtonEnabled(false);
        }

        Set<String> cameraIdSet = mTestCases.keySet();
        String[] cameraNames = new String[cameraIdSet.size()];
        int i = 0;
        for (String id : cameraIdSet) {
            cameraNames[i++] = "Camera " + id;
        }
        mCameraSpinner = (Spinner) findViewById(R.id.cameras_selection);
        mCameraSpinner.setAdapter(
            new ArrayAdapter<String>(
                this, R.layout.camera_list_item, cameraNames));
        mCameraSpinner.setOnItemSelectedListener(mCameraSpinnerListener);

        mTestLabel = (TextView) findViewById(R.id.test_label);
        mPreviewLabel = (TextView) findViewById(R.id.preview_label);
        mImageLabel = (TextView) findViewById(R.id.image_label);

        // Must be kept in sync with camera bokeh mode manually
        mModeNames = new SparseArray(2);
        mModeNames.append(
                CameraMetadata.CONTROL_EXTENDED_SCENE_MODE_BOKEH_STILL_CAPTURE, "STILL_CAPTURE");
        mModeNames.append(
                CameraMetadata.CONTROL_EXTENDED_SCENE_MODE_BOKEH_CONTINUOUS, "CONTINUOUS");

        mNextButton = findViewById(R.id.next_button);
        mNextButton.setOnClickListener(v -> {
                if (mNextCombination != null) {
                    mUntestedCombinations.remove(mNextCombination);
                    mTestedCombinations.add(mNextCombination);
                }
                setUntestedCombination();

                if (mNextCombination != null) {
                    if (mNextCombination.mIsStillCapture) {
                        takePicture();
                    } else {
                        if (mCaptureSession != null) {
                            mCaptureSession.close();
                        }
                        startPreview();
                    }
                }
        });

        mBlockingCameraManager = new BlockingCameraManager(mCameraManager);
        mCameraListener = new BlockingStateCallback();
    }

    /**
     * Set an untested combination of resolution and bokeh mode for the current camera.
     * Triggered by next button click.
     */
    private void setUntestedCombination() {
        Optional<CameraCombination> combination = mUntestedCombinations.stream().filter(
            c -> c.mCameraIndex == mCurrentCameraIndex).findFirst();
        if (!combination.isPresent()) {
            Toast.makeText(this, "All Camera " + mCurrentCameraIndex + " tests are done.",
                Toast.LENGTH_SHORT).show();
            mNextCombination = null;

            if (mUntestedCombinations.isEmpty() && mUntestedCameras.isEmpty()) {
                setPassButtonEnabled(true);
            }
            return;
        }

        // There is untested combination for the current camera, set the next untested combination.
        mNextCombination = combination.get();
        int nextMode = mNextCombination.mMode;
        ArrayList<Capability> bokehCaps = mTestCases.get(mCameraId);
        for (Capability cap : bokehCaps) {
            if (cap.getMode() == nextMode) {
                mMaxBokehStreamingSize = cap.getMaxStreamingSize();
            }
        }

        // Update bokeh mode and use case
        String testString = "Mode: " + mModeNames.get(mNextCombination.mMode);
        if (mNextCombination.mIsStillCapture) {
            testString += "\nIntent: Capture";
        } else {
            testString += "\nIntent: Preview";
        }
        testString += "\n\nPress Next if the bokeh effect works as intended";
        mTestLabel.setText(testString);

        // Update preview view and image view bokeh expectation
        boolean previewIsBokehCompatible =
                mSizeComparator.compare(mNextCombination.mPreviewSize, mMaxBokehStreamingSize) <= 0;
        String previewLabel = "Normal preview";
        if (previewIsBokehCompatible || mNextCombination.mIsStillCapture) {
            previewLabel += " with bokeh";
        }
        mPreviewLabel.setText(previewLabel);

        String imageLabel;
        if (mNextCombination.mIsStillCapture) {
            imageLabel = "JPEG with bokeh";
        } else {
            imageLabel = "YUV";
            if (previewIsBokehCompatible) {
                imageLabel += " with bokeh";
            }
        }
        mImageLabel.setText(imageLabel);
    }

    @Override
    public boolean onCreateOptionsMenu(Menu menu) {
        menu.add(Menu.NONE, MENU_ID_PROGRESS, Menu.NONE, "Current Progress");
        return super.onCreateOptionsMenu(menu);
    }

    @Override
    public boolean onOptionsItemSelected(MenuItem item) {
        boolean ret = true;
        switch (item.getItemId()) {
            case MENU_ID_PROGRESS:
                showCombinationsDialog();
                ret = true;
                break;
            default:
                ret = super.onOptionsItemSelected(item);
                break;
        }
        return ret;
    }

    private void showCombinationsDialog() {
        AlertDialog.Builder builder =
                new AlertDialog.Builder(CameraBokehActivity.this);
        builder.setMessage(getTestDetails())
                .setTitle("Current Progress")
                .setPositiveButton("OK", null);
        builder.show();
    }

    @Override
    public void onResume() {
        super.onResume();

        startBackgroundThread();

        int cameraIndex = mCameraSpinner.getSelectedItemPosition();
        if (cameraIndex >= 0) {
            setUpCamera(mCameraSpinner.getSelectedItemPosition());
        }
    }

    @Override
    public void onPause() {
        shutdownCamera();
        stopBackgroundThread();

        super.onPause();
    }

    @Override
    public String getTestDetails() {
        StringBuilder reportBuilder = new StringBuilder();
        reportBuilder.append("Tested combinations:\n");
        for (CameraCombination combination: mTestedCombinations) {
            reportBuilder.append(combination);
            reportBuilder.append("\n");
        }

        reportBuilder.append("Untested cameras:\n");
        for (String untestedCamera : mUntestedCameras) {
            reportBuilder.append(untestedCamera);
            reportBuilder.append("\n");
        }
        reportBuilder.append("Untested combinations:\n");
        for (CameraCombination combination: mUntestedCombinations) {
            reportBuilder.append(combination);
            reportBuilder.append("\n");
        }
        return reportBuilder.toString();
    }

    @Override
    public void onSurfaceTextureAvailable(SurfaceTexture surfaceTexture,
            int width, int height) {
        mPreviewTexture = surfaceTexture;
        mPreviewTexWidth = width;
        mPreviewTexHeight = height;

        mPreviewSurface = new Surface(mPreviewTexture);

        if (mCameraDevice != null) {
            startPreview();
        }
    }

    @Override
    public void onSurfaceTextureSizeChanged(SurfaceTexture surface, int width, int height) {
        // Ignored, Camera does all the work for us
        if (VERBOSE) {
            Log.v(TAG, "onSurfaceTextureSizeChanged: " + width + " x " + height);
        }
    }

    @Override
    public boolean onSurfaceTextureDestroyed(SurfaceTexture surface) {
        mPreviewTexture = null;
        return true;
    }

    @Override
    public void onSurfaceTextureUpdated(SurfaceTexture surface) {
        // Invoked every time there's a new Camera preview frame
    }

    @Override
    public void onImageAvailable(ImageReader reader) {
        Image img = null;
        try {
            img = reader.acquireNextImage();
            final int format = img.getFormat();

            Size configuredSize = (format == ImageFormat.YUV_420_888 ? mPreviewSize : mJpegSize);
            Bitmap imgBitmap = null;
            if (format == ImageFormat.YUV_420_888) {
                ByteBuffer yBuffer = img.getPlanes()[0].getBuffer();
                ByteBuffer uBuffer = img.getPlanes()[1].getBuffer();
                ByteBuffer vBuffer = img.getPlanes()[2].getBuffer();
                yBuffer.rewind();
                uBuffer.rewind();
                vBuffer.rewind();
                int w = configuredSize.getWidth();
                int h = configuredSize.getHeight();
                int stride = img.getPlanes()[0].getRowStride();
                int uStride = img.getPlanes()[1].getRowStride();
                int vStride = img.getPlanes()[2].getRowStride();
                int uPStride = img.getPlanes()[1].getPixelStride();
                int vPStride = img.getPlanes()[2].getPixelStride();
                byte[] row = new byte[configuredSize.getWidth()];
                byte[] uRow = new byte[(configuredSize.getWidth()/2-1)*uPStride + 1];
                byte[] vRow = new byte[(configuredSize.getWidth()/2-1)*vPStride + 1];
                int[] imgArray = new int[w * h];
                for (int y = 0, j = 0, rowStart = 0, uRowStart = 0, vRowStart = 0; y < h;
                     y++, rowStart += stride) {
                    yBuffer.position(rowStart);
                    yBuffer.get(row);
                    if (y % 2 == 0) {
                        uBuffer.position(uRowStart);
                        uBuffer.get(uRow);
                        vBuffer.position(vRowStart);
                        vBuffer.get(vRow);
                        uRowStart += uStride;
                        vRowStart += vStride;
                    }
                    for (int x = 0, i = 0; x < w; x++) {
                        int yval = row[i] & 0xFF;
                        int uval = uRow[i/2 * uPStride] & 0xFF;
                        int vval = vRow[i/2 * vPStride] & 0xFF;
                        // Write YUV directly; the ImageView color filter will convert to RGB for us.
                        imgArray[j] = Color.rgb(yval, uval, vval);
                        i++;
                        j++;
                    }
                }
                img.close();
                imgBitmap = Bitmap.createBitmap(imgArray, w, h, Bitmap.Config.ARGB_8888);
            } else if (format == ImageFormat.JPEG) {
                ByteBuffer jpegBuffer = img.getPlanes()[0].getBuffer();
                jpegBuffer.rewind();
                byte[] jpegData = new byte[jpegBuffer.limit()];
                jpegBuffer.get(jpegData);
                imgBitmap = BitmapFactory.decodeByteArray(jpegData, 0, jpegData.length);
                img.close();
            } else {
                Log.i(TAG, "Unsupported image format: " + format);
            }
            if (imgBitmap != null) {
                final Bitmap bitmap = imgBitmap;
                runOnUiThread(new Runnable() {
                    @Override
                    public void run() {
                        if (format == ImageFormat.YUV_420_888 && (mCurrentColorFilter == null ||
                                !mCurrentColorFilter.equals(sJFIF_YUVToRGB_Filter))) {
                            mCurrentColorFilter = sJFIF_YUVToRGB_Filter;
                            mImageView.setColorFilter(mCurrentColorFilter);
                        } else if (format == ImageFormat.JPEG && mCurrentColorFilter != null &&
                                mCurrentColorFilter.equals(sJFIF_YUVToRGB_Filter)) {
                            mCurrentColorFilter = null;
                            mImageView.clearColorFilter();
                        }
                        mImageView.setImageBitmap(bitmap);
                    }
                });
            }
        } catch (java.lang.IllegalStateException e) {
            // Swallow exceptions
            e.printStackTrace();
        } finally {
            if (img != null) {
                img.close();
            }
        }
    }

    private AdapterView.OnItemSelectedListener mCameraSpinnerListener =
            new AdapterView.OnItemSelectedListener() {
                public void onItemSelected(AdapterView<?> parent,
                        View view, int pos, long id) {
                    if (mCurrentCameraIndex != pos) {
                        setUpCamera(pos);
                    }
                }

                public void onNothingSelected(AdapterView parent) {
                }
            };

    private class SizeComparator implements Comparator<Size> {
        @Override
        public int compare(Size lhs, Size rhs) {
            long lha = lhs.getWidth() * lhs.getHeight();
            long rha = rhs.getWidth() * rhs.getHeight();
            if (lha == rha) {
                lha = lhs.getWidth();
                rha = rhs.getWidth();
            }
            return (lha < rha) ? -1 : (lha > rha ? 1 : 0);
        }
    }

    private void setUpCamera(int index) {
        shutdownCamera();

        mCurrentCameraIndex = index;
        mCameraId = mCameraIdList[index];
        try {
            mCameraCharacteristics = mCameraManager.getCameraCharacteristics(mCameraId);
            mCameraDevice = mBlockingCameraManager.openCamera(mCameraId,
                    mCameraListener, mCameraHandler);
        } catch (CameraAccessException e) {
            e.printStackTrace();
        } catch (BlockingOpenException e) {
            e.printStackTrace();
        }

        // Update untested cameras
        mUntestedCameras.remove("All combinations for Camera " + mCameraId);

        StreamConfigurationMap config =
                mCameraCharacteristics.get(CameraCharacteristics.SCALER_STREAM_CONFIGURATION_MAP);
        Size[] jpegSizes = config.getOutputSizes(ImageFormat.JPEG);
        Arrays.sort(jpegSizes, mSizeComparator);
        mJpegSize = jpegSizes[jpegSizes.length-1];

        Size[] yuvSizes = config.getOutputSizes(ImageFormat.YUV_420_888);
        Arrays.sort(yuvSizes, mSizeComparator);
        Size maxYuvSize = yuvSizes[yuvSizes.length-1];
        if (mSizeComparator.compare(maxYuvSize, FULLHD) > 1) {
            maxYuvSize = FULLHD;
        }

        // Update untested entries
        ArrayList<Capability> currentTestCase = mTestCases.get(mCameraId);
        for (Capability bokehCap : currentTestCase) {
            Size maxStreamingSize = bokehCap.getMaxStreamingSize();
            Size previewSize;
            if ((maxStreamingSize.getWidth() == 0 && maxStreamingSize.getHeight() == 0) ||
                    (mSizeComparator.compare(maxStreamingSize, maxYuvSize) > 0)) {
                previewSize = maxYuvSize;
            } else {
                previewSize = maxStreamingSize;
            }

            CameraCombination combination = new CameraCombination(
                    index, bokehCap.getMode(), previewSize.getWidth(),
                    previewSize.getHeight(), mCameraId,
                    mModeNames.get(bokehCap.getMode()),
                    /*isStillCapture*/false);

            if (!mTestedCombinations.contains(combination)) {
                mUntestedCombinations.add(combination);
            }

            // For BOKEH_MODE_STILL_CAPTURE, add 2 combinations: one streaming, one still capture.
            if (bokehCap.getMode() ==
                    CaptureRequest.CONTROL_EXTENDED_SCENE_MODE_BOKEH_STILL_CAPTURE) {
                CameraCombination combination2 = new CameraCombination(
                        index, bokehCap.getMode(), previewSize.getWidth(),
                        previewSize.getHeight(), mCameraId,
                        mModeNames.get(bokehCap.getMode()),
                        /*isStillCapture*/true);

                if (!mTestedCombinations.contains(combination2)) {
                    mUntestedCombinations.add(combination2);
                }
            }
        }

        mJpegImageReader = ImageReader.newInstance(
                mJpegSize.getWidth(), mJpegSize.getHeight(), ImageFormat.JPEG, 1);
        mJpegImageReader.setOnImageAvailableListener(this, mCameraHandler);

        setUntestedCombination();

        if (mPreviewTexture != null) {
            startPreview();
        }
    }

    private void shutdownCamera() {
        if (null != mCaptureSession) {
            mCaptureSession.close();
            mCaptureSession = null;
        }
        if (null != mCameraDevice) {
            mCameraDevice.close();
            mCameraDevice = null;
        }
        if (null != mJpegImageReader) {
            mJpegImageReader.close();
            mJpegImageReader = null;
        }
        if (null != mYuvImageReader) {
            mYuvImageReader.close();
            mYuvImageReader = null;
        }
    }

    private void configurePreviewTextureTransform() {
        int rotation = getWindowManager().getDefaultDisplay().getRotation();
        Configuration config = getResources().getConfiguration();
        int degrees = 0;
        switch (rotation) {
            case Surface.ROTATION_0: degrees = 0; break;
            case Surface.ROTATION_90: degrees = 90; break;
            case Surface.ROTATION_180: degrees = 180; break;
            case Surface.ROTATION_270: degrees = 270; break;
        }
        Matrix matrix = mPreviewView.getTransform(null);
        int deviceOrientation = Configuration.ORIENTATION_PORTRAIT;
        if ((degrees % 180 == 0 && config.orientation == Configuration.ORIENTATION_LANDSCAPE) ||
                (degrees % 180 == 90 && config.orientation == Configuration.ORIENTATION_PORTRAIT)) {
            deviceOrientation = Configuration.ORIENTATION_LANDSCAPE;
        }
        int effectiveWidth = mPreviewSize.getWidth();
        int effectiveHeight = mPreviewSize.getHeight();
        if (deviceOrientation == Configuration.ORIENTATION_PORTRAIT) {
            int temp = effectiveWidth;
            effectiveWidth = effectiveHeight;
            effectiveHeight = temp;
        }

        RectF viewRect = new RectF(0, 0, mPreviewTexWidth, mPreviewTexHeight);
        RectF bufferRect = new RectF(0, 0, effectiveWidth, effectiveHeight);
        float centerX = viewRect.centerX();
        float centerY = viewRect.centerY();
        bufferRect.offset(centerX - bufferRect.centerX(),
                centerY - bufferRect.centerY());

        matrix.setRectToRect(viewRect, bufferRect, Matrix.ScaleToFit.FILL);

        matrix.postRotate((360 - degrees) % 360, centerX, centerY);
        if ((degrees % 180) == 90) {
            int temp = effectiveWidth;
            effectiveWidth = effectiveHeight;
            effectiveHeight = temp;
        }
        // Scale to fit view, avoiding any crop
        float scale = Math.min(mPreviewTexWidth / (float) effectiveWidth,
                mPreviewTexHeight / (float) effectiveHeight);
        matrix.postScale(scale, scale, centerX, centerY);

        mPreviewView.setTransform(matrix);
    }
    /**
     * Starts a background thread and its {@link Handler}.
     */
    private void startBackgroundThread() {
        mCameraThread = new HandlerThread("CameraBokehBackground");
        mCameraThread.start();
        mCameraHandler = new Handler(mCameraThread.getLooper());
    }

    /**
     * Stops the background thread and its {@link Handler}.
     */
    private void stopBackgroundThread() {
        mCameraThread.quitSafely();
        try {
            mCameraThread.join();
            mCameraThread = null;
            mCameraHandler = null;
        } catch (InterruptedException e) {
            e.printStackTrace();
        }
    }

    private void startPreview() {
        try {
            if (mPreviewSize == null || !mPreviewSize.equals(mNextCombination.mPreviewSize)) {
                mPreviewSize = mNextCombination.mPreviewSize;

                mYuvImageReader = ImageReader.newInstance(mPreviewSize.getWidth(),
                        mPreviewSize.getHeight(), ImageFormat.YUV_420_888, 1);
                mYuvImageReader.setOnImageAvailableListener(this, mCameraHandler);
            };

            mPreviewTexture.setDefaultBufferSize(mPreviewSize.getWidth(), mPreviewSize.getHeight());
            mPreviewRequestBuilder =
                    mCameraDevice.createCaptureRequest(CameraDevice.TEMPLATE_PREVIEW);
            mPreviewRequestBuilder.addTarget(mPreviewSurface);
            mPreviewRequestBuilder.addTarget(mYuvImageReader.getSurface());

            mStillCaptureRequestBuilder =
                    mCameraDevice.createCaptureRequest(CameraDevice.TEMPLATE_STILL_CAPTURE);
            mStillCaptureRequestBuilder.addTarget(mPreviewSurface);
            mStillCaptureRequestBuilder.addTarget(mJpegImageReader.getSurface());

            mSessionListener = new BlockingSessionCallback();
            List<Surface> outputSurfaces = new ArrayList<Surface>(/*capacity*/3);
            outputSurfaces.add(mPreviewSurface);
            outputSurfaces.add(mYuvImageReader.getSurface());
            outputSurfaces.add(mJpegImageReader.getSurface());
            mCameraDevice.createCaptureSession(outputSurfaces, mSessionListener, mCameraHandler);
            mCaptureSession = mSessionListener.waitAndGetSession(/*timeoutMs*/3000);

            configurePreviewTextureTransform();

            /* Set bokeh mode and start streaming */
            int bokehMode = mNextCombination.mMode;
            mPreviewRequestBuilder.set(CaptureRequest.CONTROL_EXTENDED_SCENE_MODE, bokehMode);
            mStillCaptureRequestBuilder.set(CaptureRequest.CONTROL_EXTENDED_SCENE_MODE, bokehMode);
            mPreviewRequest = mPreviewRequestBuilder.build();
            mStillCaptureRequest = mStillCaptureRequestBuilder.build();

            mCaptureSession.setRepeatingRequest(mPreviewRequest, mCaptureCallback, mCameraHandler);
        } catch (CameraAccessException e) {
            e.printStackTrace();
        }
    }

    private void takePicture() {
        try {
            mCaptureSession.stopRepeating();
            mSessionListener.getStateWaiter().waitForState(
                    BlockingSessionCallback.SESSION_READY, SESSION_READY_TIMEOUT_MS);

            mCaptureSession.capture(mStillCaptureRequest, mCaptureCallback, mCameraHandler);
        } catch (CameraAccessException e) {
            e.printStackTrace();
        }
    }

    private void setPassButtonEnabled(boolean enabled) {
        ImageButton pass_button = (ImageButton) findViewById(R.id.pass_button);
        pass_button.setEnabled(enabled);
    }
}
