/*
 * Copyright (C) 2012 The Android Open Source Project
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

package com.android.camera;

import android.annotation.TargetApi;
import android.app.Activity;
import android.app.ActivityManager;
import android.content.ContentResolver;
import android.content.Intent;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.graphics.RectF;
import android.graphics.SurfaceTexture;
import android.hardware.Camera;
import android.location.Location;
import android.media.CameraProfile;
import android.net.Uri;
import android.os.AsyncTask;
import android.os.Build;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.os.Message;
import android.os.MessageQueue;
import android.os.SystemClock;
import android.provider.MediaStore;
import android.view.KeyEvent;
import android.view.OrientationEventListener;
import android.view.View;
import android.widget.Toast;

import com.android.camera.PhotoModule.NamedImages.NamedEntity;
import com.android.camera.app.AppController;
import com.android.camera.app.CameraAppUI;
import com.android.camera.app.CameraProvider;
import com.android.camera.app.MediaSaver;
import com.android.camera.app.MemoryManager;
import com.android.camera.app.MemoryManager.MemoryListener;
import com.android.camera.app.MotionManager;
import com.android.camera.debug.DebugPropertyHelper;
import com.android.camera.debug.Log;
import com.android.camera.exif.ExifInterface;
import com.android.camera.exif.ExifTag;
import com.android.camera.exif.Rational;
import com.android.camera.hardware.HardwareSpec;
import com.android.camera.hardware.HardwareSpecImpl;
import com.android.camera.hardware.HeadingSensor;
import com.android.camera.module.ModuleController;
import com.android.camera.one.OneCamera;
import com.android.camera.one.OneCameraAccessException;
import com.android.camera.one.OneCameraException;
import com.android.camera.one.OneCameraManager;
import com.android.camera.one.OneCameraModule;
import com.android.camera.remote.RemoteCameraModule;
import com.android.camera.settings.CameraPictureSizesCacher;
import com.android.camera.settings.Keys;
import com.android.camera.settings.ResolutionUtil;
import com.android.camera.settings.SettingsManager;
import com.android.camera.stats.SessionStatsCollector;
import com.android.camera.stats.UsageStatistics;
import com.android.camera.ui.CountDownView;
import com.android.camera.ui.TouchCoordinate;
import com.android.camera.util.AndroidServices;
import com.android.camera.util.ApiHelper;
import com.android.camera.util.CameraUtil;
import com.android.camera.util.GcamHelper;
import com.android.camera.util.GservicesHelper;
import com.android.camera.util.Size;
import com.android.camera2.R;
import com.android.ex.camera2.portability.CameraAgent;
import com.android.ex.camera2.portability.CameraAgent.CameraAFCallback;
import com.android.ex.camera2.portability.CameraAgent.CameraAFMoveCallback;
import com.android.ex.camera2.portability.CameraAgent.CameraPictureCallback;
import com.android.ex.camera2.portability.CameraAgent.CameraProxy;
import com.android.ex.camera2.portability.CameraAgent.CameraShutterCallback;
import com.android.ex.camera2.portability.CameraCapabilities;
import com.android.ex.camera2.portability.CameraDeviceInfo.Characteristics;
import com.android.ex.camera2.portability.CameraSettings;
import com.android.ex.camera2.portability.CameraStateHolder;
import com.google.common.logging.eventprotos;

import java.io.ByteArrayOutputStream;
import java.io.File;
import java.io.FileNotFoundException;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.OutputStream;
import java.lang.ref.WeakReference;
import java.util.ArrayList;
import java.util.List;
import java.util.Vector;


public class PhotoModule
        extends CameraModule
        implements PhotoController,
        ModuleController,
        MemoryListener,
        FocusOverlayManager.Listener,
        SettingsManager.OnSettingChangedListener,
        RemoteCameraModule,
        CountDownView.OnCountDownStatusListener {

    private static final Log.Tag TAG = new Log.Tag("PhotoModule");
    private final boolean DEBUG = true;//DebugPropertyHelper.isDebugOn();

    // We number the request code from 1000 to avoid collision with Gallery.
    private static final int REQUEST_CROP = 1000;

    // Messages defined for the UI thread handler.
    private static final int MSG_FIRST_TIME_INIT = 1;
    private static final int MSG_SET_CAMERA_PARAMETERS_WHEN_IDLE = 2;

    // The subset of parameters we need to update in setCameraParameters().
    private static final int UPDATE_PARAM_INITIALIZE = 1;
    private static final int UPDATE_PARAM_ZOOM = 2;
    private static final int UPDATE_PARAM_PREFERENCE = 4;
    private static final int UPDATE_PARAM_ALL = -1;

    private static final String DEBUG_IMAGE_PREFIX = "DEBUG_";

    private CameraActivity mActivity;
    private CameraProxy mCameraDevice;
    private int mCameraId;
    private CameraCapabilities mCameraCapabilities;
    private List<String> mSupportedColorEffects;
    private List<String> mSupportedSaturations;
    private List<String> mSupportedContrasts;
    private List<String> mSupportedSharpnesses;
    private List<String> mSupportedBrightnesses;
    private List<String> mSupportedHues;
    private List<String> mSupportedAntiBandings;
    private List<String> mSupportedFocusModes;
    private boolean m3dnrSupported;
    private boolean mExposureSupported;
    private CameraSettings mCameraSettings;
    private HardwareSpec mHardwareSpec;
    private boolean mPaused;

    private CameraAppUI.BottomBarUISpec mBottomBarSpec;

    private PhotoUI mUI;

    // The activity is going to switch to the specified camera id. This is
    // needed because texture copy is done in GL thread. -1 means camera is not
    // switching.
    protected int mPendingSwitchCameraId = -1;

    // When setCameraParametersWhenIdle() is called, we accumulate the subsets
    // needed to be updated in mUpdateSet.
    private int mUpdateSet;

    private float mZoomValue; // The current zoom ratio.
    private int mTimerDuration;
    /** Set when a volume button is clicked to take photo */
    private boolean mVolumeButtonClickedFlag = false;

    private boolean mFocusAreaSupported;
    private boolean mMeteringAreaSupported;
    private boolean mAeLockSupported;
    private boolean mAwbLockSupported;
    private boolean mContinuousFocusSupported;
    private boolean mManualFocusSupported;

    private boolean mManualFocusing = false;
    private int mMinManualFocusValue;
    private int mMaxManualFocusValue;
    private int mManualFocusValueRange;

    private final static String FOCUS_MODE_MANUAL = "manual";
    private final static String KEY_MANUAL_FOCUS_VAL_RANGE = "manual-focus-val-range";
    private final static String KEY_MANUAL_FOCUS_VAL = "manual-focus-val";
    private final static String KEY_MANUAL_FOCUS_LAST_VAL = "last-lens-position";

    // The degrees of the device rotated clockwise from its natural orientation.
    private int mOrientation = OrientationEventListener.ORIENTATION_UNKNOWN;

    private static final String sTempCropFilename = "crop-temp";

    private boolean mFaceDetectionStarted = false;

    private boolean mSmileShutterStarted = false;

    // mCropValue and mSaveUri are used only if isImageCaptureIntent() is true.
    private String mCropValue;
    private Uri mSaveUri;

    private Uri mDebugUri;

    // We use a queue to generated names of the images to be used later
    // when the image is ready to be saved.
    private NamedImages mNamedImages;

    private final Runnable mDoSnapRunnable = new Runnable() {
        @Override
        public void run() {
            onShutterButtonClick();
        }
    };

    private final Runnable mDoSmileShutterRunnable = new Runnable() {
        @Override
        public void run() {
            mAppController.getCameraAppUI().performShutterButtonClick();
        }
    };

    private final Runnable mBurstCaptureRunnable = new Runnable() {

        @Override
        public void run() {
            // TODO Auto-generated method stub
            burstcapture();
        }
    };

    /**
     * An unpublished intent flag requesting to return as soon as capturing is
     * completed. TODO: consider publishing by moving into MediaStore.
     */
    private static final String EXTRA_QUICK_CAPTURE =
            "android.intent.extra.quickCapture";

    // The display rotation in degrees. This is only valid when mCameraState is
    // not PREVIEW_STOPPED.
    private int mDisplayRotation;
    // The value for UI components like indicators.
    private int mDisplayOrientation;
    // The value for cameradevice.CameraSettings.setPhotoRotationDegrees.
    private int mJpegRotation;
    // Indicates whether we are using front camera
    private boolean mMirror;
    private boolean mFirstTimeInitialized;
    private boolean mIsImageCaptureIntent;

    private int mCameraState = PREVIEW_STOPPED;
    private boolean mSnapshotOnIdle = false;

    private boolean mShutterButtonLongClick = false;

    private ContentResolver mContentResolver;

    private AppController mAppController;
    private OneCameraManager mOneCameraManager;

    private final PostViewPictureCallback mPostViewPictureCallback =
            new PostViewPictureCallback();
    private final RawPictureCallback mRawPictureCallback =
            new RawPictureCallback();
    private final AutoFocusCallback mAutoFocusCallback =
            new AutoFocusCallback();
    private final Object mAutoFocusMoveCallback =
            ApiHelper.HAS_AUTO_FOCUS_MOVE_CALLBACK
                    ? new AutoFocusMoveCallback()
                    : null;

    private long mFocusStartTime;
    private long mShutterCallbackTime;
    private long mPostViewPictureCallbackTime;
    private long mRawPictureCallbackTime;
    private long mJpegPictureCallbackTime;
    private long mOnResumeTime;
    private byte[] mJpegImageData;
    /** Touch coordinate for shutter button press. */
    private TouchCoordinate mShutterTouchCoordinate;


    // These latency time are for the CameraLatency test.
    public long mAutoFocusTime;
    public long mShutterLag;
    public long mShutterToPictureDisplayedTime;
    public long mPictureDisplayedToJpegCallbackTime;
    public long mJpegCallbackFinishTime;
    public long mCaptureStartTime;

    // This handles everything about focus.
    private FocusOverlayManager mFocusManager;

    private final int mGcamModeIndex;
    private SoundPlayer mCountdownSoundPlayer;
    private boolean mSoundplayer=false;

    private CameraCapabilities.SceneMode mSceneMode;

    private final Handler mHandler = new MainHandler(this);

    private boolean mQuickCapture;

    /** Used to detect motion. We use this to release focus lock early. */
    private MotionManager mMotionManager;

    private HeadingSensor mHeadingSensor;

    /** True if all the parameters needed to start preview is ready. */
    private boolean mCameraPreviewParamsReady = false;

    private final MediaSaver.OnMediaSavedListener mOnMediaSavedListener =
            new MediaSaver.OnMediaSavedListener() {

                @Override
                public void onMediaSaved(Uri uri) {
                    if (mActivity == null 
                            || mActivity.isDestroyed()) return;
                    if (uri != null) {
                        mActivity.notifyNewMedia(uri);
                    } else {
                        onError();
                    }
                    if (mActivity.getSettingsManager().getBoolean(SettingsManager.SCOPE_GLOBAL,
                            Keys.KEY_BURST_CAPTURE_ON)) {
                        mAppController.setShutterEnabled(true);
                        enableButtonsWhenSmileShutterOff();
                        if (mAppController.getSettingsManager().getBoolean
                                (SettingsManager.SCOPE_GLOBAL, Keys.KEY_SMILE_SHUTTER_ON)
                                && DebugPropertyHelper.isSmileShutterAuto())
                            mHandler.postDelayed(mDoSmileShutterRunnable, 2000);
                    }
                }
            };

    private final int mBurstPictureLength = 10;
    private int mBurstPictureLengthInterrupt = mBurstPictureLength;
    private int mBurstPictureIndex;
    private boolean mBurstingPicture = false;
    private class onMediaSavedListenerAfterBurstCapture implements MediaSaver.OnMediaSavedListener {
        private final int mCurIndex;

        public onMediaSavedListenerAfterBurstCapture(int index) {
            // TODO Auto-generated constructor stub
            mCurIndex = index;
        }

        @Override
        public void onMediaSaved(Uri uri) {
            // TODO Auto-generated method stub
            Log.i(TAG, "save media mCurIndex = " + mCurIndex
                    + ",burst lenght = " + mBurstPictureLength 
                    + ",interrupt length = " + mBurstPictureLengthInterrupt);
            if (mActivity == null 
                    || mActivity.isDestroyed()) return;
            boolean animation = false;

            mUI.setBurstSavingProgress(((mBurstPictureLength - mCurIndex) * 1.0f)
                    / mBurstPictureLengthInterrupt);

            if (mCurIndex == 
                    (mBurstPictureLength - mBurstPictureLengthInterrupt)) {
                resetStateAfterCapture();
                //Toast.makeText(mActivity, R.string.pref_burst_finish, Toast.LENGTH_SHORT).show();
                mUI.setBurstSavingMessage(R.string.pref_burst_finish, true);
                mHandler.postDelayed(new Runnable() {

                    @Override
                    public void run() {
                        // TODO Auto-generated method stub
                        mUI.setBurstSavingProgressVisible(false);
                    }
                }, 200);
                animation = true;
            }
            
            if (uri != null) {
                mActivity.notifyNewMedia(uri);
            }
        }       
    }

    /**
     * Displays error dialog and allows use to enter feedback. Does not shut
     * down the app.
     */
    private void onError() {
        mAppController.getFatalErrorHandler().onMediaStorageFailure();
    }

    private boolean mShouldResizeTo16x9 = false;

    /**
     * We keep the flash setting before entering scene modes (HDR)
     * and restore it after HDR is off.
     */
    private String mFlashModeBeforeSceneMode;
    private String mFlashModeOfAutoScene;
    private String mFlashModeOfNightScene;

    private String mWhiteBalanceModeBeforeSceneMode;

    private void checkDisplayRotation() {
        // Need to just be a no-op for the quick resume-pause scenario.
        if (mPaused) {
            return;
        }
        // Set the display orientation if display rotation has changed.
        // Sometimes this happens when the device is held upside
        // down and camera app is opened. Rotation animation will
        // take some time and the rotation value we have got may be
        // wrong. Framework does not have a callback for this now.
        if (CameraUtil.getDisplayRotation() != mDisplayRotation) {
            setDisplayOrientation();
        }
        if (SystemClock.uptimeMillis() - mOnResumeTime < 5000) {
            mHandler.postDelayed(new Runnable() {
                @Override
                public void run() {
                    checkDisplayRotation();
                }
            }, 100);
        }
    }

    /**
     * This Handler is used to post message back onto the main thread of the
     * application
     */
    private static class MainHandler extends Handler {
        private final WeakReference<PhotoModule> mModule;

        public MainHandler(PhotoModule module) {
            super(Looper.getMainLooper());
            mModule = new WeakReference<PhotoModule>(module);
        }

        @Override
        public void handleMessage(Message msg) {
            PhotoModule module = mModule.get();
            if (module == null) {
                return;
            }
            switch (msg.what) {
                case MSG_FIRST_TIME_INIT: {
                    module.initializeFirstTime();
                    break;
                }

                case MSG_SET_CAMERA_PARAMETERS_WHEN_IDLE: {
                    module.setCameraParametersWhenIdle(0);
                    break;
                }
            }
        }
    }

    private void switchToGcamCapture() {
        if (mActivity != null && mGcamModeIndex != 0) {
            SettingsManager settingsManager = mActivity.getSettingsManager();
            settingsManager.set(SettingsManager.SCOPE_GLOBAL,
                                Keys.KEY_CAMERA_HDR_PLUS, true);

            // Disable the HDR+ button to prevent callbacks from being
            // queued before the correct callback is attached to the button
            // in the new module.  The new module will set the enabled/disabled
            // of this button when the module's preferred camera becomes available.
            ButtonManager buttonManager = mActivity.getButtonManager();

            buttonManager.disableButtonClick(ButtonManager.BUTTON_HDR_PLUS);

            mAppController.getCameraAppUI().freezeScreenUntilPreviewReady();

            // Do not post this to avoid this module switch getting interleaved with
            // other button callbacks.
            mActivity.onModeSelected(mGcamModeIndex);

            buttonManager.enableButtonClick(ButtonManager.BUTTON_HDR_PLUS);
        }
    }

    /**
     * Constructs a new photo module.
     */
    public PhotoModule(AppController app) {
        super(app);
        mGcamModeIndex = app.getAndroidContext().getResources()
                .getInteger(R.integer.camera_mode_gcam);
    }

    @Override
    public String getPeekAccessibilityString() {
        return mAppController.getAndroidContext()
            .getResources().getString(R.string.photo_accessibility_peek);
    }

    @Override
    public void init(CameraActivity activity, boolean isSecureCamera, boolean isCaptureIntent) {
        mActivity = activity;
        // TODO: Need to look at the controller interface to see if we can get
        // rid of passing in the activity directly.
        mAppController = mActivity;

        mUI = new PhotoUI(mActivity, this, mActivity.getModuleLayoutRoot());
        mActivity.setPreviewStatusListener(mUI);

        SettingsManager settingsManager = mActivity.getSettingsManager();
        // TODO: Move this to SettingsManager as a part of upgrade procedure.
        // Aspect Ratio selection dialog is only shown for Nexus 4, 5 and 6.
        if (mAppController.getCameraAppUI().shouldShowAspectRatioDialog()) {
            // Switch to back camera to set aspect ratio.
            settingsManager.setToDefault(SettingsManager.SCOPE_GLOBAL, Keys.KEY_CAMERA_ID);
        }
        mCameraId = settingsManager.getInteger(SettingsManager.SCOPE_GLOBAL,
                                               Keys.KEY_CAMERA_ID);

        mContentResolver = mActivity.getContentResolver();

        // Surface texture is from camera screen nail and startPreview needs it.
        // This must be done before startPreview.
        mIsImageCaptureIntent = isImageCaptureIntent();
        mUI.setCountdownFinishedListener(this);

        mQuickCapture = mActivity.getIntent().getBooleanExtra(EXTRA_QUICK_CAPTURE, false);
        mHeadingSensor = new HeadingSensor(AndroidServices.instance().provideSensorManager());
        if(!mSoundplayer){
            mCountdownSoundPlayer = new SoundPlayer(mAppController.getAndroidContext());
            mSoundplayer = true;
        }
        try {
            mOneCameraManager = OneCameraModule.provideOneCameraManager();
        } catch (OneCameraException e) {
            Log.e(TAG, "Hardware manager failed to open.");
        }

        // TODO: Make this a part of app controller API.
        View cancelButton = mActivity.findViewById(R.id.shutter_cancel_button);
        cancelButton.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View view) {
                cancelCountDown();
            }
        });
    }

    private void cancelCountDown() {
        if (mUI.isCountingDown()) {
            // Cancel on-going countdown.
            mUI.cancelCountDown();
            mAppController.getCameraAppUI().transitionToCapture();
            mAppController.getCameraAppUI().showModeOptions();
            enableButtonsWhenSmileShutterOff();
            mAppController.setShutterEnabled(true);
        }
    }

    @Override
    public boolean isUsingBottomBar() {
        return true;
    }

    private void initializeControlByIntent() {
        if (mIsImageCaptureIntent) {
            mActivity.getCameraAppUI().transitionToIntentCaptureLayout();
            setupCaptureParams();
            if (mUI.isReviewVisible()) {
                onCaptureRetake();
            }
        }
    }

    private void onPreviewStarted() {
        mAppController.onPreviewStarted();
        enableButtonsWhenSmileShutterOff();
        mAppController.setShutterEnabled(true);
        if (mActivity.getSettingsManager().getBoolean(SettingsManager.SCOPE_GLOBAL,
                Keys.KEY_BURST_CAPTURE_ON) && !mIsImageCaptureIntent)
            mAppController.setShutterLongClickEnabled(true);
        else 
            mAppController.setShutterLongClickEnabled(false);
        setCameraState(IDLE);
        startFaceDetection();
        if (mUI.isReviewVisible()) {
            onCaptureRetake();
        }
    }

    @Override
    public void onPreviewUIReady() {
        Log.i(TAG, "onPreviewUIReady");
        startPreview();
    }

    @Override
    public void onPreviewUIDestroyed() {
        if (mCameraDevice == null) {
            return;
        }
        mCameraDevice.setPreviewTexture(null);
        stopPreview();
    }

    @Override
    public void startPreCaptureAnimation() {
        mAppController.startFlashAnimation(false);
    }

    private void onCameraOpened() {
        openCameraCommon();
        initializeControlByIntent();
    }

    private void switchCamera() {
        if (mPaused) {
            return;
        }
        cancelCountDown();
        if (!mFocusManager.isFocusCompleted() && mCameraDevice != null) {
            Log.i(TAG,"cancel autofocus before switch camera");
            mCameraDevice.cancelAutoFocus();
        }

        mAppController.freezeScreenUntilPreviewReady();
        SettingsManager settingsManager = mActivity.getSettingsManager();

        Log.i(TAG, "Start to switch camera. id=" + mPendingSwitchCameraId);
        closeCamera();
        mCameraId = mPendingSwitchCameraId;

        settingsManager.set(SettingsManager.SCOPE_GLOBAL, Keys.KEY_CAMERA_ID, mCameraId);
        requestCameraOpen();
        mUI.clearFaces();
        if (mFocusManager != null) {
            mFocusManager.removeMessages();
        }

        mMirror = isCameraFrontFacing();
        mFocusManager.setMirror(mMirror);
        // Start switch camera animation. Post a message because
        // onFrameAvailable from the old camera may already exist.
    }

    /**
     * Uses the {@link CameraProvider} to open the currently-selected camera
     * device, using {@link GservicesHelper} to choose between API-1 and API-2.
     */
    private void requestCameraOpen() {
        Log.v(TAG, "requestCameraOpen");
        if (isResumeFromLockscreen()) {
            Intent intent = mActivity.getIntent();
            if (intent.getBooleanExtra("android.intent.extra.USE_FRONT_CAMERA", false) ||
                    intent.getBooleanExtra("com.google.assistant.extra.USE_FRONT_CAMERA", false)) {
                mCameraId = mActivity.getCameraProvider().getFirstFrontCameraId();
            } else {
                mCameraId = mActivity.getSettingsManager().getInteger(SettingsManager.SCOPE_GLOBAL, Keys.KEY_CAMERA_ID);
            }
        }
        mActivity.getCameraProvider().requestCamera(mCameraId,
                        GservicesHelper.useCamera2ApiThroughPortabilityLayer(mActivity
                                .getContentResolver()));
    }

    private final ButtonManager.ButtonCallback mCameraCallback =
            new ButtonManager.ButtonCallback() {
                @Override
                public void onStateChanged(int state) {
                    // At the time this callback is fired, the camera id
                    // has be set to the desired camera.

                    if (mPaused || mAppController.getCameraProvider().waitingForCamera()) {
                        return;
                    }
                    // If switching to back camera, and HDR+ is still on,
                    // switch back to gcam, otherwise handle callback normally.
                    SettingsManager settingsManager = mActivity.getSettingsManager();
                    if (Keys.isCameraBackFacing(settingsManager,
                                                SettingsManager.SCOPE_GLOBAL)) {
                        if (Keys.requestsReturnToHdrPlus(settingsManager,
                                                         mAppController.getModuleScope())) {
                            switchToGcamCapture();
                            return;
                        }
                    }

                    ButtonManager buttonManager = mActivity.getButtonManager();
                    buttonManager.disableCameraButtonAndBlock();
                    mAppController.getButtonManager().resetModeOptionsButtonScroll();

                    mPendingSwitchCameraId = state;

                    Log.d(TAG, "Start to switch camera. cameraId=" + state);
                    // We need to keep a preview frame for the animation before
                    // releasing the camera. This will trigger
                    // onPreviewTextureCopied.
                    // TODO: Need to animate the camera switch
                    switchCamera();
                }
            };

    private final ButtonManager.ButtonCallback mHdrPlusCallback =
            new ButtonManager.ButtonCallback() {
                @Override
                public void onStateChanged(int state) {
                    if (mPaused) return;
                    SettingsManager settingsManager = mActivity.getSettingsManager();
                    if (GcamHelper.hasGcamAsSeparateModule(
                            mAppController.getCameraFeatureConfig())) {
                        // Set the camera setting to default backfacing.
                        settingsManager.setToDefault(SettingsManager.SCOPE_GLOBAL,
                                                     Keys.KEY_CAMERA_ID);
                        switchToGcamCapture();
                    } else {
//                        if (Keys.isHdrOn(settingsManager)) {
//                            Log.i(TAG,"hdrcallback set scene hdr");
//                            settingsManager.set(mAppController.getCameraScope(), Keys.KEY_SCENE_MODE,
//                                    mCameraCapabilities.getStringifier().stringify(
//                                            CameraCapabilities.SceneMode.HDR));
//                        } else {
//                            Log.i(TAG,"hdrcallback set scene auto");
//                            settingsManager.set(mAppController.getCameraScope(), Keys.KEY_SCENE_MODE,
//                                    mCameraCapabilities.getStringifier().stringify(
//                                            CameraCapabilities.SceneMode.AUTO));
//                        }
                        updateParametersSceneMode();
                        if (mCameraDevice != null) {
                            mCameraDevice.applySettings(mCameraSettings);
                        }
                        updateSceneMode();
                    }
                }
            };

    private final View.OnClickListener mCancelCallback = new View.OnClickListener() {
        @Override
        public void onClick(View v) {
            onCaptureCancelled();
        }
    };

    private final View.OnClickListener mDoneCallback = new View.OnClickListener() {
        @Override
        public void onClick(View v) {
            onCaptureDone();
        }
    };

    private final View.OnClickListener mRetakeCallback = new View.OnClickListener() {
        @Override
        public void onClick(View v) {
            mActivity.getCameraAppUI().transitionToIntentCaptureLayout();
            onCaptureRetake();
            mActivity.getCameraAppUI().enableModeOptions();
        }
    };

    @Override
    public void hardResetSettings(SettingsManager settingsManager) {
        // PhotoModule should hard reset HDR+ to off,
        // and HDR to off if HDR+ is supported.
        settingsManager.set(SettingsManager.SCOPE_GLOBAL, Keys.KEY_CAMERA_HDR_PLUS, false);
        if (GcamHelper.hasGcamAsSeparateModule(mAppController.getCameraFeatureConfig())) {
            settingsManager.set(SettingsManager.SCOPE_GLOBAL, Keys.KEY_CAMERA_HDR, false);
        }
    }

    @Override
    public HardwareSpec getHardwareSpec() {
        if (mHardwareSpec == null) {
            mHardwareSpec = (mCameraSettings != null ?
                    new HardwareSpecImpl(getCameraProvider(), mCameraCapabilities,
                            mAppController.getCameraFeatureConfig(), isCameraFrontFacing()) : null);
        }
        return mHardwareSpec;
    }

    @Override
    public CameraAppUI.BottomBarUISpec getBottomBarSpec() {
        CameraAppUI.BottomBarUISpec bottomBarSpec = new CameraAppUI.BottomBarUISpec();

        bottomBarSpec.enableCamera = true;
        bottomBarSpec.cameraCallback = mCameraCallback;
        bottomBarSpec.enableFlash = !mAppController.getSettingsManager()
            .getBoolean(SettingsManager.SCOPE_GLOBAL, Keys.KEY_CAMERA_HDR);
        bottomBarSpec.enableHdr = !mAppController.getSettingsManager()
                .getBoolean(SettingsManager.SCOPE_GLOBAL, Keys.KEY_BURST_CAPTURE_ON);
        bottomBarSpec.hdrCallback = mHdrPlusCallback;
        bottomBarSpec.enableGridLines = true;
        bottomBarSpec.enableZsl = false;//!mAppController.getSettingsManager()
                //.getBoolean(SettingsManager.SCOPE_GLOBAL, Keys.KEY_CAMERA_HDR);
        bottomBarSpec.enable3dnr = m3dnrSupported;
        if (mCameraCapabilities != null) {
            bottomBarSpec.enableSmileShutter = (mCameraCapabilities.getMaxNumOfFacesSupported() > 0
                    && DebugPropertyHelper.showCaptureDebugUI());
            bottomBarSpec.enableExposureCompensation = mExposureSupported;
            bottomBarSpec.exposureCompensationSetCallback =
                new CameraAppUI.BottomBarUISpec.ExposureCompensationSetCallback() {
                @Override
                public void setExposure(int value) {
                    if (mPaused || mCameraSettings == null
                            || mCameraCapabilities == null) return;
                    if (DEBUG) Log.i(TAG, "ExposureCompensationSetCallback");
                    setExposureCompensation(value);
                }
            };
            bottomBarSpec.minExposureCompensation =
                mCameraCapabilities.getMinExposureCompensation();
            bottomBarSpec.maxExposureCompensation =
                mCameraCapabilities.getMaxExposureCompensation();
            bottomBarSpec.exposureCompensationStep =
                mCameraCapabilities.getExposureCompensationStep();

            if (DEBUG) {
                Log.i(TAG,"minExposureCompensation = " + bottomBarSpec.minExposureCompensation
                        + ",maxExposureCompensation = " + bottomBarSpec.maxExposureCompensation
                        + ",exposureCompensationStep = " + bottomBarSpec.exposureCompensationStep);
            }

            String scenemode = mAppController.getSettingsManager()
                    .getString(mAppController.getCameraScope(), Keys.KEY_SCENE_MODE);
            Log.d(TAG, "scenemode = " + scenemode);
            bottomBarSpec.enableWhiteBalance = Camera.Parameters.SCENE_MODE_AUTO.equals(scenemode)
                    || Camera.Parameters.SCENE_MODE_HDR.equals(scenemode);
            bottomBarSpec.supportedWhiteBalances = mCameraCapabilities.getSupportedWhiteBalance();
            bottomBarSpec.whiteBalanceSetCallback = 
                new CameraAppUI.BottomBarUISpec.WhiteBalanceSetCallback() {
                
                @Override
                public void setWhiteBalance(String value) {
                    // TODO Auto-generated method stub
                    if (mPaused || mCameraSettings == null
                            || mCameraCapabilities == null) return;
                    if (DEBUG) Log.i(TAG, "WhiteBalanceSetCallback");
                    setWhiteBalanceInternal(value);
                }
            };
            if (DEBUG) {
                Log.i(TAG, "supportedWhiteBalances size = " 
                        + bottomBarSpec.supportedWhiteBalances.size());
                for (CameraCapabilities.WhiteBalance wb : bottomBarSpec.supportedWhiteBalances) {
                    Log.i(TAG, "supported wb = " + wb.name());
                }
            }

//            if (bottomBarSpec.supportedWhiteBalances.size() != 5) {
//                bottomBarSpec.supportedWhiteBalances.clear();
//                bottomBarSpec.supportedWhiteBalances.add(CameraCapabilities.WhiteBalance.CLOUDY_DAYLIGHT);
//                bottomBarSpec.supportedWhiteBalances.add(CameraCapabilities.WhiteBalance.INCANDESCENT);
//                bottomBarSpec.supportedWhiteBalances.add(CameraCapabilities.WhiteBalance.FLUORESCENT);
//                bottomBarSpec.supportedWhiteBalances.add(CameraCapabilities.WhiteBalance.DAYLIGHT);
//                bottomBarSpec.supportedWhiteBalances.add(CameraCapabilities.WhiteBalance.AUTO);
//            }

            bottomBarSpec.enableScene = true;
            bottomBarSpec.supportedSceneModes = mCameraCapabilities.getSupportedSceneModes();
            bottomBarSpec.sceneSetCallback = 
                    new CameraAppUI.BottomBarUISpec.SceneSetCallback() {
                        
                        @Override
                        public void setScene(String value) {
                            // TODO Auto-generated method stub
                            if (mPaused || mCameraSettings == null
                                    || mCameraCapabilities == null) return;
                            if (DEBUG) Log.i(TAG, "sceneSetCallback");
                            setSceneMode(value);
                        }
                    };
            if (DEBUG) {
                Log.i(TAG, "supportedScenModes size = " 
                        + bottomBarSpec.supportedSceneModes.size());
                for (CameraCapabilities.SceneMode scene : bottomBarSpec.supportedSceneModes) {
                    Log.i(TAG, "supported scene = " + scene.name());
                }
            }
//            if (bottomBarSpec.supportedSceneModes.size() != 6) {
//                bottomBarSpec.supportedSceneModes.clear();
//                bottomBarSpec.supportedSceneModes.add(CameraCapabilities.SceneMode.AUTO);
//                bottomBarSpec.supportedSceneModes.add(CameraCapabilities.SceneMode.PORTRAIT);
//                bottomBarSpec.supportedSceneModes.add(CameraCapabilities.SceneMode.LANDSCAPE);
//                bottomBarSpec.supportedSceneModes.add(CameraCapabilities.SceneMode.NIGHT);
//                bottomBarSpec.supportedSceneModes.add(CameraCapabilities.SceneMode.SPORTS);
//                bottomBarSpec.supportedSceneModes.add(CameraCapabilities.SceneMode.BARCODE);
//            }

            bottomBarSpec.enableColorEffect = true;
            bottomBarSpec.supportedColorEffects = mSupportedColorEffects;
            bottomBarSpec.colorEffectSetCallback = new CameraAppUI.BottomBarUISpec.ColorEffectSetCallback() {
                
                @Override
                public void setColorEffect(String value) {
                    // TODO Auto-generated method stub
                    if (mPaused || mCameraSettings == null) return;
                    if (DEBUG) Log.i(TAG, "colorEffectSetCallback");
                    setColorEffectInternal(value);
                }
            };
            if (bottomBarSpec.supportedColorEffects != null) {
                if (DEBUG)
                    for (String effect : bottomBarSpec.supportedColorEffects)
                        Log.i(TAG, "support coloreffect = " + effect);
            } else {
                bottomBarSpec.supportedColorEffects = new ArrayList<String>();
            }
//            if (bottomBarSpec.supportedColorEffects.size() != 4) {
//                bottomBarSpec.supportedColorEffects.clear();
//                bottomBarSpec.supportedColorEffects.add(Camera.Parameters.EFFECT_NONE);
//                bottomBarSpec.supportedColorEffects.add(Camera.Parameters.EFFECT_MONO);
//                bottomBarSpec.supportedColorEffects.add(Camera.Parameters.EFFECT_NEGATIVE);
//                bottomBarSpec.supportedColorEffects.add(Camera.Parameters.EFFECT_SEPIA);
//            }

            bottomBarSpec.enableSaturation = true;
            bottomBarSpec.supportedSaturations= mSupportedSaturations;
            bottomBarSpec.saturationSetCallback = new CameraAppUI.BottomBarUISpec.SaturationSetCallback() {

                @Override
                public void setSaturation(String value) {
                    // TODO Auto-generated method stub
                    if (mPaused || mCameraSettings == null) return;
                    if (DEBUG) Log.i(TAG, "saturationSetCallback");
                    setSaturationInternal(value);
                }
            };
            if (bottomBarSpec.supportedSaturations != null) {
                if (DEBUG)
                    for (String saturation : bottomBarSpec.supportedSaturations)
                        Log.i(TAG, "support saturation = " + saturation);
            } else {
                bottomBarSpec.supportedSaturations = new ArrayList<String>();
            }
//            if (bottomBarSpec.supportedSaturations.size() != 3) {
//                bottomBarSpec.supportedSaturations.clear();
//                bottomBarSpec.supportedSaturations.add("low");
//                bottomBarSpec.supportedSaturations.add("normal");
//                bottomBarSpec.supportedSaturations.add("high");
//            }

            bottomBarSpec.enableContrast = true;
            bottomBarSpec.supportedContrasts= mSupportedContrasts;
            bottomBarSpec.contrastSetCallback = new CameraAppUI.BottomBarUISpec.ContrastSetCallback() {
                
                @Override
                public void setContrast(String value) {
                    // TODO Auto-generated method stub
                    if (mPaused || mCameraSettings == null) return;
                    if (DEBUG) Log.i(TAG, "contrastSetCallback");
                    setContrastInternal(value);
                }
            };
            if (bottomBarSpec.supportedContrasts != null) {
                if (DEBUG)
                    for (String contrast : bottomBarSpec.supportedContrasts)
                        Log.i(TAG, "support contrast = " + contrast);
            } else {
                bottomBarSpec.supportedContrasts = new ArrayList<String>();
            }
//            if (bottomBarSpec.supportedContrasts.size() != 3) {
//                bottomBarSpec.supportedContrasts.clear();
//                bottomBarSpec.supportedContrasts.add("low");
//                bottomBarSpec.supportedContrasts.add("normal");
//                bottomBarSpec.supportedContrasts.add("high");
//            }

            bottomBarSpec.enableSharpness = true;
            bottomBarSpec.supportedSharpnesses= mSupportedSharpnesses;
            bottomBarSpec.sharpnessSetCallback = new CameraAppUI.BottomBarUISpec.SharpnessSetCallback() {
                
                @Override
                public void setSharpness(String value) {
                    // TODO Auto-generated method stub
                    if (mPaused || mCameraSettings == null) return;
                    if (DEBUG) Log.i(TAG, "sharpnessSetCallback");
                    setSharpnessInternal(value);
                }
            };
            if (bottomBarSpec.supportedSharpnesses != null) {
                if (DEBUG)
                    for (String sharpness : bottomBarSpec.supportedSharpnesses)
                        Log.i(TAG, "support sharpness = " + sharpness);
            } else {
                bottomBarSpec.supportedSharpnesses = new ArrayList<String>();
            }
//            if (bottomBarSpec.supportedSharpnesses.size() != 3) {
//                bottomBarSpec.supportedSharpnesses.clear();
//                bottomBarSpec.supportedSharpnesses.add("low");
//                bottomBarSpec.supportedSharpnesses.add("normal");
//                bottomBarSpec.supportedSharpnesses.add("high");
//            }

            bottomBarSpec.enableBrightness = true;
            bottomBarSpec.supportedBrightnesses= mSupportedBrightnesses;
            bottomBarSpec.brightnessSetCallback = new CameraAppUI.BottomBarUISpec.BrightnessSetCallback() {
                
                @Override
                public void setBrightness(String value) {
                    // TODO Auto-generated method stub
                    if (mPaused || mCameraSettings == null) return;
                    if (DEBUG) Log.i(TAG, "brightnessSetCallback");
                    setBrightnessInternal(value);
                }
            };
            if (bottomBarSpec.supportedBrightnesses != null) {
                if (DEBUG)
                    for (String brightness : bottomBarSpec.supportedBrightnesses)
                        Log.i(TAG, "support brightness = " + brightness);
            } else {
                bottomBarSpec.supportedBrightnesses = new ArrayList<String>();
            }
//            if (bottomBarSpec.supportedBrightnesses.size() != 3) {
//                bottomBarSpec.supportedBrightnesses.clear();
//                bottomBarSpec.supportedBrightnesses.add("low");
//                bottomBarSpec.supportedBrightnesses.add("normal");
//                bottomBarSpec.supportedBrightnesses.add("high");
//            }

            bottomBarSpec.enableHue = false;
            bottomBarSpec.supportedHues= mSupportedHues;
            bottomBarSpec.hueSetCallback = new CameraAppUI.BottomBarUISpec.HueSetCallback() {
                
                @Override
                public void setHue(String value) {
                    // TODO Auto-generated method stub
                    if (mPaused || mCameraSettings == null) return;
                    if (DEBUG) Log.i(TAG, "hueSetCallback");
                    setHueInternal(value);
                }
            };
            if (bottomBarSpec.supportedHues != null) {
                if (DEBUG)
                    for (String hue : bottomBarSpec.supportedHues)
                        Log.i(TAG, "support hue = " + hue);
            } else {
                bottomBarSpec.supportedHues = new ArrayList<String>();
            }
//            if (bottomBarSpec.supportedHues.size() != 3) {
//                bottomBarSpec.supportedHues.clear();
//                bottomBarSpec.supportedHues.add("low");
//                bottomBarSpec.supportedHues.add("normal");
//                bottomBarSpec.supportedHues.add("high");
//            }
        }

        bottomBarSpec.enableSelfTimer = true;
        bottomBarSpec.showSelfTimer = true;

        if (isImageCaptureIntent()) {
            bottomBarSpec.showCancel = true;
            bottomBarSpec.cancelCallback = mCancelCallback;
            bottomBarSpec.showDone = true;
            bottomBarSpec.doneCallback = mDoneCallback;
            bottomBarSpec.showRetake = true;
            bottomBarSpec.retakeCallback = mRetakeCallback;
        }
        mBottomBarSpec = bottomBarSpec;
        return bottomBarSpec;
    }

    // either open a new camera or switch cameras
    private void openCameraCommon() {
        mUI.onCameraOpened(mCameraCapabilities, mCameraSettings);
        if (mIsImageCaptureIntent) {
            // Set hdr plus to default: off.
            SettingsManager settingsManager = mActivity.getSettingsManager();
            settingsManager.setToDefault(SettingsManager.SCOPE_GLOBAL,
                                         Keys.KEY_CAMERA_HDR_PLUS);
        }
        updateSceneMode();
    }

    @Override
    public void updatePreviewAspectRatio(float aspectRatio) {
        mAppController.updatePreviewAspectRatio(aspectRatio);
    }

    private void resetExposureCompensation() {
        SettingsManager settingsManager = mActivity.getSettingsManager();
        if (settingsManager == null) {
            Log.e(TAG, "Settings manager is null!");
            return;
        }
        settingsManager.setToDefault(mAppController.getCameraScope(),
                                     Keys.KEY_EXPOSURE);
    }

    private void resetWhiteBalance() {
        SettingsManager settingsManager = mActivity.getSettingsManager();
        if (settingsManager == null) {
            Log.e(TAG, "Settings manager is null!");
            return;
        }
        settingsManager.setToDefault(mAppController.getCameraScope(),
                                     Keys.KEY_WHITEBALANCE);
    }

    // Snapshots can only be taken after this is called. It should be called
    // once only. We could have done these things in onCreate() but we want to
    // make preview screen appear as soon as possible.
    private void initializeFirstTime() {
        if (mFirstTimeInitialized || mPaused) {
            return;
        }

        mUI.initializeFirstTime();

        // We set the listener only when both service and shutterbutton
        // are initialized.
        getServices().getMemoryManager().addListener(this);

        mNamedImages = new NamedImages();

        mFirstTimeInitialized = true;
        addIdleHandler();

        mActivity.updateStorageSpaceAndHint(null);
    }

    // If the activity is paused and resumed, this method will be called in
    // onResume.
    private void initializeSecondTime() {
        getServices().getMemoryManager().addListener(this);
        mNamedImages = new NamedImages();
        mUI.initializeSecondTime(mCameraCapabilities, mCameraSettings);
    }

    private void addIdleHandler() {
        MessageQueue queue = Looper.myQueue();
        queue.addIdleHandler(new MessageQueue.IdleHandler() {
            @Override
            public boolean queueIdle() {
                Storage.ensureOSXCompatible();
                return false;
            }
        });
    }

    private void startSmileShutter() {
        if (!mSmileShutterStarted) {
            try {
                //if (mIntelCamera != null) {
                    Log.i(TAG, "startSmileShutter");
                //    mIntelCamera.startSmileShutter();
                    mSmileShutterStarted = true;
                    mAppController.setShutterLongClickEnabled(false);
                    if (mAppController.getSettingsManager().getBoolean
                            (SettingsManager.SCOPE_GLOBAL, Keys.KEY_SMILE_SHUTTER_ON)
                            && DebugPropertyHelper.isSmileShutterAuto())
                        mHandler.post(mDoSmileShutterRunnable);
                //}
            } catch (RuntimeException e) {
                Log.e(TAG, "SmileShutter has already started exception");
            }
        }
    }

    private void stopSmileShutter() {
        if (mSmileShutterStarted) {
            //if (mIntelCamera != null) {
                Log.i(TAG, "stopSmileShutter");
                cancelSmileShutterIfOn();
            //    mIntelCamera.stopSmileShutter();
                mSmileShutterStarted = false;
                SettingsManager settingsManager = mAppController.getSettingsManager();
                if (settingsManager.getBoolean(SettingsManager.SCOPE_GLOBAL,
                        Keys.KEY_BURST_CAPTURE_ON) && !mIsImageCaptureIntent)
                    mAppController.setShutterLongClickEnabled(true);
                else
                    mAppController.setShutterLongClickEnabled(false);
            //}
        }
    }

    private boolean isColorModeOptionsEnabled() {
        if (mBottomBarSpec != null) {
            if (mBottomBarSpec.enableColorEffect 
                    && mBottomBarSpec.supportedColorEffects.size() > 1)
                return true;
        }
        return false;
    }

    private boolean isSceneModeOptionsEnabled() {
        if (mBottomBarSpec != null) {
            if (mBottomBarSpec.enableScene 
                    && mBottomBarSpec.supportedSceneModes.size() > 0)
                return true;
        }
        return false;
    }

    private void disableButtonsWhenSmileShutterOn() {
        CameraAppUI appUi = mAppController.getCameraAppUI();
        appUi.smileShutterAnimator(true);
        ButtonManager buttonManager = mActivity.getButtonManager(); 
        if (buttonManager.isVisible(ButtonManager.BUTTON_COLOR)) {
            buttonManager.disableButton(ButtonManager.BUTTON_COLOR);
        }
        if (buttonManager.isVisible(ButtonManager.BUTTON_SCENE)) {
            buttonManager.disableButton(ButtonManager.BUTTON_SCENE);
        }
        if (DebugPropertyHelper.ZSL_NORMAL_MODE.equals(DebugPropertyHelper.getCaptureMode()))
            buttonManager.disableButton(ButtonManager.BUTTON_ZSL);
        appUi.setSwipeEnabled(false);
        mUI.setZoomEnabled(false);
    }

    private void enableButtonsWhenSmileShutterOff() {
        CameraAppUI appUi = mAppController.getCameraAppUI();
        appUi.smileShutterAnimator(false);
        ButtonManager buttonManager = mActivity.getButtonManager();
        if (isColorModeOptionsEnabled()) {
            buttonManager.enableButton(ButtonManager.BUTTON_COLOR);
        }
        if (isSceneModeOptionsEnabled()) {
            buttonManager.enableButton(ButtonManager.BUTTON_SCENE);
        }
        if (mSceneMode != CameraCapabilities.SceneMode.HDR
                && DebugPropertyHelper.ZSL_NORMAL_MODE.equals(DebugPropertyHelper.getCaptureMode()))
            buttonManager.enableButton(ButtonManager.BUTTON_ZSL);
        appUi.setSwipeEnabled(true);
        mUI.setZoomEnabled(true);
    }

    @Override
    public void startFaceDetection() {
        if (mFaceDetectionStarted || mCameraDevice == null) {
            return;
        }
        if (mCameraCapabilities.getMaxNumOfFacesSupported() > 0
                && mActivity.getSettingsManager().getBoolean(SettingsManager.SCOPE_GLOBAL,
                        Keys.KEY_FACE_DETECTION_ENABLED)) {
            Log.i(TAG,"startFaceDetection");
            mFaceDetectionStarted = true;
            mUI.onStartFaceDetection(mDisplayOrientation, isCameraFrontFacing());
            mCameraDevice.setFaceDetectionCallback(mHandler, mUI);
            mCameraDevice.startFaceDetection();
            SessionStatsCollector.instance().faceScanActive(true);
            updateSmileShutterState();
        }
    }

    @Override
    public void stopFaceDetection() {
        if (!mFaceDetectionStarted || mCameraDevice == null) {
            return;
        }
        if (mCameraCapabilities.getMaxNumOfFacesSupported() > 0
                && mActivity.getSettingsManager().getBoolean(SettingsManager.SCOPE_GLOBAL,
                        Keys.KEY_FACE_DETECTION_ENABLED)) {
            Log.i(TAG,"stopFaceDetection");
            stopSmileShutter();
            mFaceDetectionStarted = false;
            mCameraDevice.setFaceDetectionCallback(null, null);
            mCameraDevice.stopFaceDetection();
            mUI.clearFaces();
            SessionStatsCollector.instance().faceScanActive(false);
        }
    }

    private final class ShutterCallback
            implements CameraShutterCallback {

        private final boolean mNeedsAnimation;

        public ShutterCallback(boolean needsAnimation) {
            mNeedsAnimation = needsAnimation;
        }

        @Override
        public void onShutter(CameraProxy camera) {
            mShutterCallbackTime = System.currentTimeMillis();
            mShutterLag = mShutterCallbackTime - mCaptureStartTime;
            Log.v(TAG, "mShutterLag = " + mShutterLag + "ms");
            if (mNeedsAnimation) {
                mActivity.runOnUiThread(new Runnable() {
                    @Override
                    public void run() {
                        animateAfterShutter();
                    }
                });
            }
        }
    }

    private final class PostViewPictureCallback
            implements CameraPictureCallback {
        @Override
        public void onPictureTaken(byte[] data, CameraProxy camera) {
            mPostViewPictureCallbackTime = System.currentTimeMillis();
            Log.v(TAG, "mShutterToPostViewCallbackTime = "
                    + (mPostViewPictureCallbackTime - mShutterCallbackTime)
                    + "ms");
        }
    }

    private final class RawPictureCallback
            implements CameraPictureCallback {
        @Override
        public void onPictureTaken(byte[] rawData, CameraProxy camera) {
            mRawPictureCallbackTime = System.currentTimeMillis();
            Log.v(TAG, "mShutterToRawCallbackTime = "
                    + (mRawPictureCallbackTime - mShutterCallbackTime) + "ms");
        }
    }

    private static class ResizeBundle {
        byte[] jpegData;
        float targetAspectRatio;
        ExifInterface exif;
    }

    /**
     * @return Cropped image if the target aspect ratio is larger than the jpeg
     *         aspect ratio on the long axis. The original jpeg otherwise.
     */
    private ResizeBundle cropJpegDataToAspectRatio(ResizeBundle dataBundle) {

        final byte[] jpegData = dataBundle.jpegData;
        final ExifInterface exif = dataBundle.exif;
        float targetAspectRatio = dataBundle.targetAspectRatio;

        Bitmap original = BitmapFactory.decodeByteArray(jpegData, 0, jpegData.length);
        int originalWidth = original.getWidth();
        int originalHeight = original.getHeight();
        int newWidth;
        int newHeight;

        if (originalWidth > originalHeight) {
            newHeight = (int) (originalWidth / targetAspectRatio);
            newWidth = originalWidth;
        } else {
            newWidth = (int) (originalHeight / targetAspectRatio);
            newHeight = originalHeight;
        }
        int xOffset = (originalWidth - newWidth)/2;
        int yOffset = (originalHeight - newHeight)/2;

        if (xOffset < 0 || yOffset < 0) {
            return dataBundle;
        }

        try {
            Bitmap b1 = Bitmap.createBitmap(original,xOffset,yOffset,newWidth, newHeight);
            if (b1 != original) {
                original.recycle();
                original = b1;
            }
        } catch (OutOfMemoryError e) {
            // We have no memory to rotate. Return the original bitmap.
            System.gc();
        }
        Bitmap resized = original;//Bitmap.createBitmap(original,xOffset,yOffset,newWidth, newHeight);
        exif.setTagValue(ExifInterface.TAG_PIXEL_X_DIMENSION, new Integer(newWidth));
        exif.setTagValue(ExifInterface.TAG_PIXEL_Y_DIMENSION, new Integer(newHeight));

        ByteArrayOutputStream stream = new ByteArrayOutputStream();

        resized.compress(Bitmap.CompressFormat.JPEG, 90, stream);
        resized.recycle();
        resized = null;
        dataBundle.jpegData = stream.toByteArray();
        return dataBundle;
    }

    private final class BurstPictureCallback implements CameraPictureCallback {
        Location mLocation;
        int mIndex;

        public BurstPictureCallback(Location loc) {
            mLocation = loc;
            mIndex = 0;
        }

        @Override
        public void onPictureTaken(final byte[] originalJpegData, final CameraProxy camera) {
            Log.i(TAG, "BurstCapture onPictureTaken index = " + mIndex 
                    + ",max length = " + mBurstPictureLength
                    + ",mPaused = " + mPaused);
            mBurstPictureIndex = mIndex;
            mBurstingPicture = true;
            //mAppController.setShutterEnabled(true);
            if (mPaused) {
                if (mCameraState != PREVIEW_STOPPED) {
                    mUI.setBurstCountdown(1);
                    mBurstPictureLengthInterrupt = mIndex - 1;
                    mBurstingPicture = false;
                }
                return;
            }

            if (originalJpegData == null) {
                Log.i(TAG, "BurstPictureCallback null");
                mUI.setBurstCountdown(1);
                resetStateAfterCapture();
                mBurstPictureLengthInterrupt = mIndex - 1;
                mBurstingPicture = false;
                return;
            }

            mJpegPictureCallbackTime = System.currentTimeMillis();
            // If postview callback has arrived, the captured image is displayed
            // in postview callback. If not, the captured image is displayed in
            // raw picture callback.
            if (mPostViewPictureCallbackTime != 0) {
                mShutterToPictureDisplayedTime =
                        mPostViewPictureCallbackTime - mShutterCallbackTime;
                mPictureDisplayedToJpegCallbackTime =
                        mJpegPictureCallbackTime - mPostViewPictureCallbackTime;
            } else {
                mShutterToPictureDisplayedTime =
                        mRawPictureCallbackTime - mShutterCallbackTime;
                mPictureDisplayedToJpegCallbackTime =
                        mJpegPictureCallbackTime - mRawPictureCallbackTime;
            }
            Log.v(TAG, "mPictureDisplayedToJpegCallbackTime = "
                    + mPictureDisplayedToJpegCallbackTime + "ms");

            long now = System.currentTimeMillis();
            mJpegCallbackFinishTime = now - mJpegPictureCallbackTime;
            Log.v(TAG, "mJpegCallbackFinishTime = " + mJpegCallbackFinishTime + "ms");
            mJpegPictureCallbackTime = 0;

            final ExifInterface exif = Exif.getExif(originalJpegData);

            if (mShouldResizeTo16x9) {
                final ResizeBundle dataBundle = new ResizeBundle();
                dataBundle.jpegData = originalJpegData;
                dataBundle.targetAspectRatio = ResolutionUtil.NEXUS_5_LARGE_16_BY_9_ASPECT_RATIO;
                dataBundle.exif = exif;
                new AsyncTask<ResizeBundle, Void, ResizeBundle>() {

                    @Override
                    protected ResizeBundle doInBackground(ResizeBundle... resizeBundles) {
                        return cropJpegDataToAspectRatio(resizeBundles[0]);
                    }

                    @Override
                    protected void onPostExecute(ResizeBundle result) {
                        saveFinalPhoto(result.jpegData, result.exif, camera);
                    }
                }.executeOnExecutor(AsyncTask.THREAD_POOL_EXECUTOR, dataBundle);

            } else {
                saveFinalPhoto(originalJpegData, exif, camera);
            }
            
            if ((mBurstPictureLength - mIndex) == 1)
                mBurstPictureLengthInterrupt = mIndex;
            else
                mIndex++;
        }

        void saveFinalPhoto(final byte[] jpegData, final ExifInterface exif, CameraProxy camera) {
            if (mNamedImages == null)
                mNamedImages = new NamedImages();
            if (mNamedImages.mQueue.size() == 0)
                mNamedImages.nameNewImage(System.currentTimeMillis());

            int orientation = Exif.getOrientation(exif);

            float zoomValue = 1.0f;
            if (mCameraCapabilities.supports(CameraCapabilities.Feature.ZOOM)) {
                zoomValue = mCameraSettings.getCurrentZoomRatio();
            }
            boolean hdrOn = CameraCapabilities.SceneMode.HDR == mSceneMode;
            String flashSetting =
                    mActivity.getSettingsManager().getString(mAppController.getCameraScope(),
                                                             Keys.KEY_FLASH_MODE);
            boolean gridLinesOn = Keys.areGridLinesOn(mActivity.getSettingsManager());
            UsageStatistics.instance().photoCaptureDoneEvent(
                    eventprotos.NavigationChange.Mode.PHOTO_CAPTURE,
                    mNamedImages.mQueue.lastElement().title + ".jpg", exif,
                    isCameraFrontFacing(), hdrOn, zoomValue, flashSetting, gridLinesOn,
                    (float) mTimerDuration, null, mShutterTouchCoordinate, mVolumeButtonClickedFlag,
                    null, null, null);
            mShutterTouchCoordinate = null;
            mVolumeButtonClickedFlag = false;

            if (!mIsImageCaptureIntent) {
                // Calculate the width and the height of the jpeg.
                Integer exifWidth = exif.getTagIntValue(ExifInterface.TAG_PIXEL_X_DIMENSION);
                Integer exifHeight = exif.getTagIntValue(ExifInterface.TAG_PIXEL_Y_DIMENSION);
                int width, height;
                if (mShouldResizeTo16x9 && exifWidth != null && exifHeight != null) {
                    width = exifWidth;
                    height = exifHeight;
                } else {
                    Size s;
                    s = new Size(mCameraSettings.getCurrentPhotoSize());
                    if ((mJpegRotation + orientation) % 180 == 0) {
                        width = s.width();
                        height = s.height();
                    } else {
                        width = s.height();
                        height = s.width();
                    }
                }
                NamedEntity name = mNamedImages.getNextNameEntity();
                String title = (name == null) ? null : name.title;
                long date = (name == null) ? -1 : name.date;

                // Handle debug mode outputs
                if (mDebugUri != null) {
                    // If using a debug uri, save jpeg there.
                    saveToDebugUri(jpegData);

                    // Adjust the title of the debug image shown in mediastore.
                    if (title != null) {
                        title = DEBUG_IMAGE_PREFIX + title;
                    }
                }

                if (title == null) {
                    Log.e(TAG, "Unbalanced name/data pair");
                } else {
                    if (date == -1) {
                        date = mCaptureStartTime;
                    }
                    int heading = mHeadingSensor.getCurrentHeading();
                    if (heading != HeadingSensor.INVALID_HEADING) {
                        // heading direction has been updated by the sensor.
                        ExifTag directionRefTag = exif.buildTag(
                                ExifInterface.TAG_GPS_IMG_DIRECTION_REF,
                                ExifInterface.GpsTrackRef.MAGNETIC_DIRECTION);
                        ExifTag directionTag = exif.buildTag(
                                ExifInterface.TAG_GPS_IMG_DIRECTION,
                                new Rational(heading, 1));
                        exif.setTag(directionRefTag);
                        exif.setTag(directionTag);
                    }

                    onMediaSavedListenerAfterBurstCapture mediaSavedListener = new onMediaSavedListenerAfterBurstCapture( 
                            (mBurstPictureLength - mIndex));
                    getServices().getMediaSaver().addImage(
                            jpegData, title, date, mLocation, width, height,
                            orientation, exif, mediaSavedListener);
                }
                // Animate capture with real jpeg data instead of a preview
                // frame.
                //mUI.animateCapture(jpegData, orientation, mMirror);
            } else {
                mJpegImageData = jpegData;
                if (!mQuickCapture) {
                    mUI.showCapturedImageForReview(jpegData, orientation, mMirror);
                } else {
                    onCaptureDone();
                }
            }

            // Send the taken photo to remote shutter listeners, if any are
            // registered.
            getServices().getRemoteShutterListener().onPictureTaken(jpegData);

            // Check this in advance of each shot so we don't add to shutter
            // latency. It's true that someone else could write to the SD card
            // in the mean time and fill it, but that could have happened
            // between the shutter press and saving the JPEG too.
            mActivity.updateStorageSpaceAndHint(null);
            
            mUI.setBurstCountdown((mBurstPictureLength - mIndex));
        }
    }

    private final class JpegPictureCallback
            implements CameraPictureCallback {
        Location mLocation;

        public JpegPictureCallback(Location loc) {
            mLocation = loc;
        }

        @Override
        public void onPictureTaken(final byte[] originalJpegData, final CameraProxy camera) {
            Log.i(TAG, "onPictureTaken");
            if (!mActivity.getSettingsManager().getBoolean(SettingsManager.SCOPE_GLOBAL,
                        Keys.KEY_BURST_CAPTURE_ON)) {
                mAppController.setShutterEnabled(true);
                enableButtonsWhenSmileShutterOff();
            }
            if (mPaused) {
                return;
            }
            if (mIsImageCaptureIntent) {
                stopPreview();
                mFocusManager.resetTouchFocus();
            }
            if (mSceneMode == CameraCapabilities.SceneMode.HDR) {
                mUI.setSwipingEnabled(true);
            }

            mJpegPictureCallbackTime = System.currentTimeMillis();
            // If postview callback has arrived, the captured image is displayed
            // in postview callback. If not, the captured image is displayed in
            // raw picture callback.
            if (mPostViewPictureCallbackTime != 0) {
                mShutterToPictureDisplayedTime =
                        mPostViewPictureCallbackTime - mShutterCallbackTime;
                mPictureDisplayedToJpegCallbackTime =
                        mJpegPictureCallbackTime - mPostViewPictureCallbackTime;
            } else {
                mShutterToPictureDisplayedTime =
                        mRawPictureCallbackTime - mShutterCallbackTime;
                mPictureDisplayedToJpegCallbackTime =
                        mJpegPictureCallbackTime - mRawPictureCallbackTime;
            }
            Log.v(TAG, "mPictureDisplayedToJpegCallbackTime = "
                    + mPictureDisplayedToJpegCallbackTime + "ms");

            if (!mIsImageCaptureIntent) {
                if (mActivity.getSettingsManager().getBoolean(SettingsManager.SCOPE_GLOBAL,
                        Keys.KEY_BURST_CAPTURE_ON)) {
                    resetStateAfterCapture();
                    mAppController.setShutterEnabled(false);
                } else
                    setupPreview();
            }

            if (originalJpegData == null) {
                Log.i(TAG, "JpegPictureCallback null");
                mAppController.setShutterEnabled(true);
                return;
            }

            long now = System.currentTimeMillis();
            mJpegCallbackFinishTime = now - mJpegPictureCallbackTime;
            Log.v(TAG, "mJpegCallbackFinishTime = " + mJpegCallbackFinishTime + "ms");
            mJpegPictureCallbackTime = 0;

            final ExifInterface exif = Exif.getExif(originalJpegData);
            if (mNamedImages == null)
                mNamedImages = new NamedImages();
            if (mNamedImages.mQueue.size() == 0)
                mNamedImages.nameNewImage(System.currentTimeMillis());
            final NamedEntity name = mNamedImages.getNextNameEntity();
            if (mShouldResizeTo16x9) {
                final ResizeBundle dataBundle = new ResizeBundle();
                dataBundle.jpegData = originalJpegData;
                dataBundle.targetAspectRatio = ResolutionUtil.NEXUS_5_LARGE_16_BY_9_ASPECT_RATIO;
                dataBundle.exif = exif;
                new AsyncTask<ResizeBundle, Void, ResizeBundle>() {

                    @Override
                    protected ResizeBundle doInBackground(ResizeBundle... resizeBundles) {
                        return cropJpegDataToAspectRatio(resizeBundles[0]);
                    }

                    @Override
                    protected void onPostExecute(ResizeBundle result) {
                        saveFinalPhoto(result.jpegData, name, result.exif, camera);
                    }
                }.executeOnExecutor(AsyncTask.THREAD_POOL_EXECUTOR, dataBundle);

            } else {
                saveFinalPhoto(originalJpegData, name, exif, camera);
            }
        }

        void saveFinalPhoto(final byte[] jpegData, NamedEntity name, final ExifInterface exif,
                CameraProxy camera) {
            int orientation = Exif.getOrientation(exif);

            float zoomValue = 1.0f;
            if (mCameraCapabilities.supports(CameraCapabilities.Feature.ZOOM)) {
                zoomValue = mCameraSettings.getCurrentZoomRatio();
            }
            boolean hdrOn = CameraCapabilities.SceneMode.HDR == mSceneMode;
            String flashSetting =
                    mActivity.getSettingsManager().getString(mAppController.getCameraScope(),
                                                             Keys.KEY_FLASH_MODE);
            boolean gridLinesOn = Keys.areGridLinesOn(mActivity.getSettingsManager());
            UsageStatistics.instance().photoCaptureDoneEvent(
                    eventprotos.NavigationChange.Mode.PHOTO_CAPTURE,
                    name.title + ".jpg", exif,
                    isCameraFrontFacing(), hdrOn, zoomValue, flashSetting, gridLinesOn,
                    (float) mTimerDuration, null, mShutterTouchCoordinate, mVolumeButtonClickedFlag,
                    null, null, null);
            mShutterTouchCoordinate = null;
            mVolumeButtonClickedFlag = false;

            if (!mIsImageCaptureIntent) {
                // Calculate the width and the height of the jpeg.
                Integer exifWidth = exif.getTagIntValue(ExifInterface.TAG_PIXEL_X_DIMENSION);
                Integer exifHeight = exif.getTagIntValue(ExifInterface.TAG_PIXEL_Y_DIMENSION);
                int width, height;
                if (mShouldResizeTo16x9 && exifWidth != null && exifHeight != null) {
                    width = exifWidth;
                    height = exifHeight;
                } else {
                    Size s = new Size(mCameraSettings.getCurrentPhotoSize());
                    if ((mJpegRotation + orientation) % 180 == 0) {
                        width = s.width();
                        height = s.height();
                    } else {
                        width = s.height();
                        height = s.width();
                    }
                }
                String title = (name == null) ? null : name.title;
                long date = (name == null) ? -1 : name.date;

                // Handle debug mode outputs
                if (mDebugUri != null) {
                    // If using a debug uri, save jpeg there.
                    saveToDebugUri(jpegData);

                    // Adjust the title of the debug image shown in mediastore.
                    if (title != null) {
                        title = DEBUG_IMAGE_PREFIX + title;
                    }
                }

                if (title == null) {
                    Log.e(TAG, "Unbalanced name/data pair");
                } else {
                    if (date == -1) {
                        date = mCaptureStartTime;
                    }
                    int heading = mHeadingSensor.getCurrentHeading();
                    if (heading != HeadingSensor.INVALID_HEADING) {
                        // heading direction has been updated by the sensor.
                        ExifTag directionRefTag = exif.buildTag(
                                ExifInterface.TAG_GPS_IMG_DIRECTION_REF,
                                ExifInterface.GpsTrackRef.MAGNETIC_DIRECTION);
                        ExifTag directionTag = exif.buildTag(
                                ExifInterface.TAG_GPS_IMG_DIRECTION,
                                new Rational(heading, 1));
                        exif.setTag(directionRefTag);
                        exif.setTag(directionTag);
                    }
                    getServices().getMediaSaver().addImage(
                            jpegData, title, date, mLocation, width, height,
                            orientation, exif, mOnMediaSavedListener);
                }
                // Animate capture with real jpeg data instead of a preview
                // frame.
                mUI.animateCapture(jpegData, orientation, mMirror);
            } else {
                mJpegImageData = jpegData;
                if (!mQuickCapture) {
                    Log.v(TAG, "showing UI");
                    mUI.showCapturedImageForReview(jpegData, orientation, mMirror);
                } else {
                    onCaptureDone();
                }
            }

            // Send the taken photo to remote shutter listeners, if any are
            // registered.
            getServices().getRemoteShutterListener().onPictureTaken(jpegData);

            // Check this in advance of each shot so we don't add to shutter
            // latency. It's true that someone else could write to the SD card
            // in the mean time and fill it, but that could have happened
            // between the shutter press and saving the JPEG too.
            mActivity.updateStorageSpaceAndHint(null);
        }
    }

    private final class AutoFocusCallback implements CameraAFCallback {
        @Override
        public void onAutoFocus(boolean focused, CameraProxy camera) {
            Log.i(TAG, "AutoFocusCallback");
            if (mCameraState == SNAPSHOT_IN_PROGRESS) {
                Log.w(TAG, "taking picture, not invoke AutoFocusCallback");
                return;
            }
            SessionStatsCollector.instance().autofocusResult(focused);
            if (mPaused) {
                return;
            }

            mAutoFocusTime = System.currentTimeMillis() - mFocusStartTime;
            Log.v(TAG, "mAutoFocusTime = " + mAutoFocusTime + "ms   focused = "+focused);
            setCameraState(IDLE);
            mFocusManager.onAutoFocus(focused, false);
        }
    }

    private final class AutoFocusMoveCallback
            implements CameraAFMoveCallback {
        @Override
        public void onAutoFocusMoving(
                boolean moving, CameraProxy camera) {
            Log.i(TAG, "onAutoFocusMoving = " + moving);
            if (mCameraState == SNAPSHOT_IN_PROGRESS) {
                Log.w(TAG, "taking picture, not invoke focusmoving");
                return;
            }
            mFocusManager.onAutoFocusMoving(moving);
            SessionStatsCollector.instance().autofocusMoving(moving);
        }
    }

    /**
     * This class is just a thread-safe queue for name,date holder objects.
     */
    public static class NamedImages {
        private final Vector<NamedEntity> mQueue;

        public NamedImages() {
            mQueue = new Vector<NamedEntity>();
        }

        public void nameNewImage(long date) {
            NamedEntity r = new NamedEntity();
            r.title = CameraUtil.instance().createJpegName(date);
            r.date = date;
            mQueue.add(r);
        }

        public NamedEntity getNextNameEntity() {
            synchronized (mQueue) {
                if (!mQueue.isEmpty()) {
                    return mQueue.remove(0);
                }
            }
            return null;
        }

        public static class NamedEntity {
            public String title;
            public long date;
        }
    }

    private void setCameraState(int state) {
        mCameraState = state;
        switch (state) {
            case PREVIEW_STOPPED:
            case SNAPSHOT_IN_PROGRESS:
            case SWITCHING_CAMERA:
                // TODO: Tell app UI to disable swipe
                break;
            case PhotoController.IDLE:
                // TODO: Tell app UI to enable swipe
                break;
        }
    }

    private void animateAfterShutter() {
        // Only animate when in full screen capture mode
        // i.e. If monkey/a user swipes to the gallery during picture taking,
        // don't show animation
        if (!mIsImageCaptureIntent) {
            mUI.animateFlash();
        }
    }

    @Override
    public boolean capture() {
        Log.i(TAG, "capture");
        // If we are already in the middle of taking a snapshot or the image
        // save request is full then ignore.
        Log.i(TAG, "mCameraDevice = " + mCameraDevice
                + ",mCameraState = " + mCameraState
                + ",isShutterEnabled = " + mAppController.isShutterEnabled());
        if (mCameraDevice == null || mCameraState == SNAPSHOT_IN_PROGRESS
                || mCameraState == SWITCHING_CAMERA) {
            return false;
        }
        setCameraState(SNAPSHOT_IN_PROGRESS);

        mCaptureStartTime = System.currentTimeMillis();

        mPostViewPictureCallbackTime = 0;
        mJpegImageData = null;

        final boolean animateBefore = (mSceneMode == CameraCapabilities.SceneMode.HDR);

        if (animateBefore) {
            animateAfterShutter();
        }

        Location loc = mActivity.getLocationManager().getCurrentLocation();
        CameraUtil.setGpsParameters(mCameraSettings, loc);
        mCameraDevice.applySettings(mCameraSettings);

        // Set JPEG orientation. Even if screen UI is locked in portrait, camera orientation should
        // still match device orientation (e.g., users should always get landscape photos while
        // capturing by putting device in landscape.)
        if (mOrientation == OrientationEventListener.ORIENTATION_UNKNOWN)
            mOrientation = mDisplayRotation;
//        Characteristics info = mActivity.getCameraProvider().getCharacteristics(mCameraId);
//        int sensorOrientation = info.getSensorOrientation();
//        int deviceOrientation =
//                mAppController.getOrientationManager().getDeviceOrientation().getDegrees();
//        boolean isFrontCamera = info.isFacingFront();
//        mJpegRotation =
//                CameraUtil.getImageRotation(sensorOrientation, deviceOrientation, isFrontCamera);
//        mCameraDevice.setJpegOrientation(mJpegRotation);
//
//        Log.v(TAG, "capture orientation (screen:device:used:jpeg) " +
//                mDisplayRotation + ":" + deviceOrientation + ":" +
//                sensorOrientation + ":" + mJpegRotation);

        int orientation = mActivity.isAutoRotateScreen() ? mDisplayRotation : mOrientation;
        Characteristics info = mActivity.getCameraProvider().getCharacteristics(mCameraId);
        mJpegRotation = info.getJpegOrientation(orientation);
        mCameraDevice.setJpegOrientation(mJpegRotation);

        Log.v(TAG, "capture orientation (screen:device:used:jpeg) " +
                mDisplayRotation + ":" + mOrientation + ":" +
                orientation + ":" + mJpegRotation);

        mCameraDevice.takePicture(mHandler,
                new ShutterCallback(!animateBefore),
                mRawPictureCallback, mPostViewPictureCallback,
                new JpegPictureCallback(loc));

        if (mNamedImages == null)
            mNamedImages = new NamedImages();
        mNamedImages.nameNewImage(mCaptureStartTime);

        mFaceDetectionStarted = false;
        return true;
    }

    private boolean burstcapture() {
        Log.i(TAG,"Burst Capture");
        // If we are already in the middle of taking a snapshot or the image
        // save request is full then ignore.
        if (mCameraDevice == null || mCameraState == SNAPSHOT_IN_PROGRESS
                || mCameraState == SWITCHING_CAMERA) {
            return false;
        }
        setCameraState(SNAPSHOT_IN_PROGRESS);

        mCaptureStartTime = System.currentTimeMillis();

        mPostViewPictureCallbackTime = 0;
        mJpegImageData = null;

        Location loc = mActivity.getLocationManager().getCurrentLocation();
        CameraUtil.setGpsParameters(mCameraSettings, loc);
        mCameraDevice.applySettings(mCameraSettings);

        // Set JPEG orientation. Even if screen UI is locked in portrait, camera orientation should
        // still match device orientation (e.g., users should always get landscape photos while
        // capturing by putting device in landscape.)
        if (mOrientation == OrientationEventListener.ORIENTATION_UNKNOWN)
            mOrientation = mDisplayRotation;
//        Characteristics info = mActivity.getCameraProvider().getCharacteristics(mCameraId);
//        int sensorOrientation = info.getSensorOrientation();
//        int deviceOrientation =
//                mAppController.getOrientationManager().getDeviceOrientation().getDegrees();
//        boolean isFrontCamera = info.isFacingFront();
//        mJpegRotation =
//                CameraUtil.getImageRotation(sensorOrientation, deviceOrientation, isFrontCamera);
//        mCameraDevice.setJpegOrientation(mJpegRotation);

        int orientation = mActivity.isAutoRotateScreen() ? mDisplayRotation : mOrientation;
        Characteristics info = mActivity.getCameraProvider().getCharacteristics(mCameraId);
        mJpegRotation = info.getJpegOrientation(orientation);
        mCameraDevice.setJpegOrientation(mJpegRotation);

        Log.v(TAG, "burstcapture orientation (screen:device:used:jpeg) " +
                mDisplayRotation + ":" + mOrientation + ":" +
                orientation + ":" + mJpegRotation);
        
        // We don't want user to press the button again while taking a
        // multi-second HDR photo.
        mCameraDevice.takePicture(mHandler,
                new ShutterCallback(false),
                mRawPictureCallback, mPostViewPictureCallback,
                new BurstPictureCallback(loc));

        if (mNamedImages == null)
            mNamedImages = new NamedImages();
        mNamedImages.nameNewImage(mCaptureStartTime);

        mFaceDetectionStarted = false;
        return true;

    }

    @Override
    public void setFocusParameters() {
        setCameraParameters(UPDATE_PARAM_PREFERENCE);
    }

    private void updateSceneMode() {
        // If scene mode is set, we cannot set flash mode, white balance, and
        // focus mode, instead, we read it from driver. Some devices don't have
        // any scene modes, so we must check both NO_SCENE_MODE in addition to
        // AUTO to check where there is no actual scene mode set.
        if (!(CameraCapabilities.SceneMode.AUTO == mSceneMode ||
                CameraCapabilities.SceneMode.NO_SCENE_MODE == mSceneMode
                || CameraCapabilities.SceneMode.NIGHT == mSceneMode)) {
            overrideCameraSettings(mCameraSettings.getCurrentFlashMode(),
                    mCameraSettings.getCurrentFocusMode());
        }
    }

    private void overrideCameraSettings(CameraCapabilities.FlashMode flashMode,
            CameraCapabilities.FocusMode focusMode) {
        CameraCapabilities.Stringifier stringifier = mCameraCapabilities.getStringifier();
        SettingsManager settingsManager = mActivity.getSettingsManager();
        if ((flashMode != null) && (!CameraCapabilities.FlashMode.NO_FLASH.equals(flashMode))) {
            String flashModeString = stringifier.stringify(flashMode);
            Log.v(TAG, "override flash setting to: " + flashModeString);
            settingsManager.set(mAppController.getCameraScope(), Keys.KEY_FLASH_MODE,
                    flashModeString);
        } else {
            Log.v(TAG, "skip setting flash mode on override due to NO_FLASH");
        }
        if (focusMode != null) {
            String focusModeString = stringifier.stringify(focusMode);
            Log.v(TAG, "override focus setting to: " + focusModeString);
            settingsManager.set(mAppController.getCameraScope(), Keys.KEY_FOCUS_MODE,
                    focusModeString);
        }
    }

    @Override
    public void onOrientationChanged(int orientation) {
        // TODO Auto-generated method stub
        if (orientation == OrientationEventListener.ORIENTATION_UNKNOWN) {
            return;
        }

        if (!CameraUtil.AUTO_ROTATE_SENSOR) {
            if (CameraUtil.mLastOrientation != OrientationEventListener.ORIENTATION_UNKNOWN
                    && mOrientation != OrientationEventListener.ORIENTATION_UNKNOWN) {
                mOrientation = (360 - orientation) % 360;
                if (CameraUtil.mLastOrientation != mOrientation) {
                    Log.i(TAG, "mLastOrientation = " + CameraUtil.mLastOrientation
                            + ",mOrientation = " + mOrientation
                            + ",mLastUIRotatedRawOrientation = " + CameraUtil.mLastUIRotatedRawOrientation
                            + ",mRawOrientation = " + CameraUtil.mRawOrientation);
                    if (Math.abs(CameraUtil.mRawOrientation - CameraUtil.mLastUIRotatedRawOrientation) < 10) return;
                    CameraUtil.calculateCurrentScreenOrientation(CameraUtil.mLastOrientation, mOrientation, mActivity);
                    mUI.updateUIByOrientation();
                    CameraUtil.mLastUIRotatedRawOrientation = CameraUtil.mRawOrientation;
                    CameraUtil.mLastOrientation = mOrientation;
                }
            } else {
                if (CameraUtil.mLastOrientation != OrientationEventListener.ORIENTATION_UNKNOWN) {
                    //CameraUtil.mLastOrientation = (360 - orientation) % 360;
                } else
                    CameraUtil.mLastOrientation = CameraUtil.getDisplayRotation();
                Log.i(TAG, "init mLastOrientation from displayRotation = " + CameraUtil.mLastOrientation
                        + ",mOrientation = " + mOrientation
                        + ",getDisplayRotation = " + CameraUtil.getDisplayRotation());
                mOrientation = (360 - orientation) % 360;
                CameraUtil.calculateCurrentScreenOrientation(CameraUtil.mLastOrientation, mOrientation, mActivity);
                mUI.updateUIByOrientation();
                CameraUtil.mLastUIRotatedRawOrientation = CameraUtil.mRawOrientation;
                CameraUtil.mLastOrientation = mOrientation;
                return;
            }
        } else {
            // TODO: Document orientation compute logic and unify them in OrientationManagerImpl.
            // b/17443789
            // Flip to counter-clockwise orientation.
            mOrientation = (360 - orientation) % 360;
        }
    }

    @Override
    public void onCameraAvailable(CameraProxy cameraProxy) {
        Log.i(TAG, "onCameraAvailable");
        if (mPaused) {
            return;
        }
        mCameraDevice = cameraProxy;

        initializeCapabilities();
        // mCameraCapabilities is guaranteed to initialized at this point.
        mAppController.getCameraAppUI().showAccessibilityZoomUI(
                mCameraCapabilities.getMaxZoomRatio());


        // Reset zoom value index.
        mZoomValue = 1.0f;
        if (mFocusManager == null) {
            initializeFocusManager();
        }
        mFocusManager.updateCapabilities(mCameraCapabilities);

        // Do camera parameter dependent initialization.
        mCameraSettings = mCameraDevice.getSettings();
        // Set a default flash mode and focus mode
        if (mCameraSettings.getCurrentFlashMode() == null) {
            mCameraSettings.setFlashMode(CameraCapabilities.FlashMode.NO_FLASH);
        }
        if (mCameraSettings.getCurrentFocusMode() == null) {
            mCameraSettings.setFocusMode(CameraCapabilities.FocusMode.AUTO);
        }

        setCameraParameters(UPDATE_PARAM_ALL);
        // Set a listener which updates camera parameters based
        // on changed settings.
        SettingsManager settingsManager = mActivity.getSettingsManager();
        settingsManager.addListener(this);
        mCameraPreviewParamsReady = true;

        startPreview();

        onCameraOpened();

        mHardwareSpec = new HardwareSpecImpl(getCameraProvider(), mCameraCapabilities,
                mAppController.getCameraFeatureConfig(), isCameraFrontFacing());

        ButtonManager buttonManager = mActivity.getButtonManager();
        buttonManager.enableCameraButton();
    }

    @Override
    public void onCaptureCancelled() {
        mActivity.setResultEx(Activity.RESULT_CANCELED, new Intent());
        mActivity.finish();
    }

    @Override
    public void onCaptureRetake() {
        Log.i(TAG, "onCaptureRetake");
        if (mPaused) {
            return;
        }
        mUI.hidePostCaptureAlert();
        mUI.hideIntentReviewImageView();
        setupPreview();
    }

    @Override
    public void onCaptureDone() {
        Log.i(TAG, "onCaptureDone");
        if (mPaused) {
            return;
        }

        byte[] data = mJpegImageData;

        if (mCropValue == null) {
            // First handle the no crop case -- just return the value. If the
            // caller specifies a "save uri" then write the data to its
            // stream. Otherwise, pass back a scaled down version of the bitmap
            // directly in the extras.
            if (mSaveUri != null) {
                OutputStream outputStream = null;
                try {
                    outputStream = mContentResolver.openOutputStream(mSaveUri);
                    outputStream.write(data);
                    outputStream.close();

                    Log.v(TAG, "saved result to URI: " + mSaveUri);
                    mActivity.setResultEx(Activity.RESULT_OK);
                    mActivity.finish();
                } catch (IOException ex) {
                    onError();
                } finally {
                    CameraUtil.closeSilently(outputStream);
                }
            } else {
                ExifInterface exif = Exif.getExif(data);
                int orientation = Exif.getOrientation(exif);
                Bitmap bitmap = CameraUtil.makeBitmap(data, 50 * 1024);
                Bitmap b1 = CameraUtil.rotate(bitmap, orientation);
                if (b1 != bitmap) {
                    bitmap.recycle();
                    bitmap = b1;
                }
                Log.v(TAG, "inlined bitmap into capture intent result");
                mActivity.setResultEx(Activity.RESULT_OK,
                        new Intent("inline-data").putExtra("data", bitmap));
                mActivity.finish();
            }
        } else {
            // Save the image to a temp file and invoke the cropper
            Uri tempUri = null;
            FileOutputStream tempStream = null;
            try {
                File path = mActivity.getFileStreamPath(sTempCropFilename);
                path.delete();
                tempStream = mActivity.openFileOutput(sTempCropFilename, 0);
                tempStream.write(data);
                tempStream.close();
                tempUri = Uri.fromFile(path);
                Log.v(TAG, "wrote temp file for cropping to: " + sTempCropFilename);
            } catch (FileNotFoundException ex) {
                Log.w(TAG, "error writing temp cropping file to: " + sTempCropFilename, ex);
                mActivity.setResultEx(Activity.RESULT_CANCELED);
                onError();
                return;
            } catch (IOException ex) {
                Log.w(TAG, "error writing temp cropping file to: " + sTempCropFilename, ex);
                mActivity.setResultEx(Activity.RESULT_CANCELED);
                onError();
                return;
            } finally {
                CameraUtil.closeSilently(tempStream);
            }

            Bundle newExtras = new Bundle();
            if (mCropValue.equals("circle")) {
                newExtras.putString("circleCrop", "true");
            }
            if (mSaveUri != null) {
                Log.v(TAG, "setting output of cropped file to: " + mSaveUri);
                newExtras.putParcelable(MediaStore.EXTRA_OUTPUT, mSaveUri);
            } else {
                newExtras.putBoolean(CameraUtil.KEY_RETURN_DATA, true);
            }
            if (mActivity.isSecureCamera()) {
                newExtras.putBoolean(CameraUtil.KEY_SHOW_WHEN_LOCKED, true);
            }

            // TODO: Share this constant.
            final String CROP_ACTION = "com.android.camera.action.CROP";
            Intent cropIntent = new Intent(CROP_ACTION);

            cropIntent.setData(tempUri);
            cropIntent.putExtras(newExtras);
            Log.v(TAG, "starting CROP intent for capture");
            mActivity.startActivityForResult(cropIntent, REQUEST_CROP);
        }
    }

    @Override
    public void onShutterCoordinate(TouchCoordinate coord) {
        mShutterTouchCoordinate = coord;
    }

    @Override
    public void onShutterButtonFocus(boolean pressed) {
        // Do nothing. We don't support half-press to focus anymore.
    }

    @Override
    public void onShutterButtonClick() {
        if (mPaused || (mCameraState == SWITCHING_CAMERA)
                || (mCameraState == PREVIEW_STOPPED)
                || !mAppController.isShutterEnabled()
                || mAppController.getButtonManager().isZslButtonChanging()
                || !mAppController.getCameraAppUI().isModeCoverHide()) {
            mVolumeButtonClickedFlag = false;
            return;
        }
        if (mAppController.getSettingsManager().getBoolean
                (SettingsManager.SCOPE_GLOBAL, Keys.KEY_SMILE_SHUTTER_ON)
                && DebugPropertyHelper.isSmileShutterAuto())
            mHandler.removeCallbacks(mDoSmileShutterRunnable);

        Log.i(TAG,"onShutterButtonClick");
        // Do not take the picture if there is not enough storage.
        if (mActivity.getStorageSpaceBytes() <= Storage.LOW_STORAGE_THRESHOLD_BYTES) {
            Log.i(TAG, "Not enough space or storage not ready. remaining="
                    + mActivity.getStorageSpaceBytes());
            mVolumeButtonClickedFlag = false;
            return;
        }
        Log.d(TAG, "onShutterButtonClick: mCameraState=" + mCameraState +
                " mVolumeButtonClickedFlag=" + mVolumeButtonClickedFlag);

        mAppController.getCameraAppUI().disableModeOptions();
        disableButtonsWhenSmileShutterOn();

        mAppController.setShutterEnabled(false);

        int countDownDuration = mActivity.getSettingsManager()
            .getInteger(SettingsManager.SCOPE_GLOBAL, Keys.KEY_COUNTDOWN_DURATION);
        mTimerDuration = countDownDuration;
        if (countDownDuration > 0) {
            // Start count down.
            mAppController.getCameraAppUI().transitionToCancel();
            mAppController.getCameraAppUI().hideModeOptions();
            mUI.startCountdown(countDownDuration);
            return;
        } else {
            focusAndCapture();
        }
    }

    private void setBurstEnable(boolean enable) {
        if (enable) {
            if (mCameraDevice != null /*&& mIntelCamera != null*/) {
                Log.i(TAG, "setBurstParameters length = " + mBurstPictureLength);
                mCameraSettings.setBurstLength(mBurstPictureLength);
                mCameraDevice.applySettings(mCameraSettings);
            }
        } else {
            if (mCameraDevice != null /*&& mIntelCamera != null*/) {
                Log.i(TAG, "setBurstParameters length = 1");
                mCameraSettings.setBurstLength(1);
                mCameraDevice.applySettings(mCameraSettings);
            }
        }
    }

    @Override
    public void onShutterButtonLongPressed() {
        if (mPaused || (mCameraState == SWITCHING_CAMERA)
                || (mCameraState == PREVIEW_STOPPED)
                || !mAppController.isShutterEnabled()
                || mAppController.getButtonManager().isZslButtonChanging()
                || !mAppController.getSettingsManager().getBoolean(
                        SettingsManager.SCOPE_GLOBAL, Keys.KEY_BURST_CAPTURE_ON)
                || !mAppController.getCameraAppUI().isModeCoverHide()) {
            mVolumeButtonClickedFlag = false;
            return;
        }
        // Do not take the picture if there is not enough storage.
        if (mActivity.getStorageSpaceBytes() <= Storage.LOW_STORAGE_THRESHOLD_BYTES) {
            Log.i(TAG, "Not enough space or storage not ready. remaining="
                    + mActivity.getStorageSpaceBytes());
            mVolumeButtonClickedFlag = false;
            return;
        }
        Log.i(TAG,"onShutterButtonLongClick");
        mAppController.getCameraAppUI().disableModeOptions();
        disableButtonsWhenSmileShutterOn();
        mAppController.setShutterEnabled(false);
        mShutterButtonLongClick = true;
        setBurstEnable(true);
        mHandler.post(mBurstCaptureRunnable);
    }

    @Override
    public void onShutterButtonLongClickRelease() {
        Log.i(TAG,"onShutterButtonLongClickRelease");
        mShutterButtonLongClick = false;
        mHandler.postDelayed(new Runnable() {
            
            @Override
            public void run() {
                // TODO Auto-generated method stub
                setBurstEnable(false);
            }
        }, 100);
        
    }

    private void focusAndCapture() {
        Log.i(TAG, "focusAndCapture");
        disableButtonsWhenSmileShutterOn();
        mAppController.getCameraAppUI().disableModeOptions();
        if (mSceneMode == CameraCapabilities.SceneMode.HDR) {
            mUI.setSwipingEnabled(false);
        }
        // If the user wants to do a snapshot while the previous one is still
        // in progress, remember the fact and do it after we finish the previous
        // one and re-start the preview. Snapshot in progress also includes the
        // state that autofocus is focusing and a picture will be taken when
        // focus callback arrives.
        Log.i(TAG, "isFocusingSnapOnFinish = " + mFocusManager.isFocusingSnapOnFinish()
                + ", mCameraState = " + mCameraState);
        if ((mFocusManager.isFocusingSnapOnFinish() || mCameraState == SNAPSHOT_IN_PROGRESS)) {
            if (!mIsImageCaptureIntent) {
                mSnapshotOnIdle = true;
            }
            return;
        }

        mSnapshotOnIdle = false;
        mFocusManager.focusAndCapture(mCameraSettings.getCurrentFocusMode());
    }

    @Override
    public void onRemainingSecondsChanged(int remainingSeconds) {
        if (remainingSeconds == 1) {
            mCountdownSoundPlayer.play(R.raw.timer_final_second, 0.6f);
        } else if (remainingSeconds == 2 || remainingSeconds == 3) {
            mCountdownSoundPlayer.play(R.raw.timer_increment, 0.6f);
        }
    }

    @Override
    public void onCountDownFinished() {
        mAppController.getCameraAppUI().transitionToCapture();
        mAppController.getCameraAppUI().showModeOptions();
        if (mPaused) {
            return;
        }
        focusAndCapture();
    }

    @Override
    public void resume() {
        mPaused = false;
        Log.d(TAG,"resume");
        if(!mSoundplayer){
            mCountdownSoundPlayer = new SoundPlayer(mAppController.getAndroidContext());
            mSoundplayer = true;
        }
        mCountdownSoundPlayer.loadSound(R.raw.timer_final_second);
        mCountdownSoundPlayer.loadSound(R.raw.timer_increment);
        if (mFocusManager != null) {
            // If camera is not open when resume is called, focus manager will
            // not be initialized yet, in which case it will start listening to
            // preview area size change later in the initialization.
            mAppController.addPreviewAreaSizeChangedListener(mFocusManager);
        }
        mAppController.addPreviewAreaSizeChangedListener(mUI);

        CameraProvider camProvider = mActivity.getCameraProvider();
        if (camProvider == null) {
            // No camera provider, the Activity is destroyed already.
            return;
        }

        // Close the review UI if it's currently visible.
        mUI.hidePostCaptureAlert();
        mUI.hideIntentReviewImageView();

        requestCameraOpen();

        mJpegPictureCallbackTime = 0;
        mZoomValue = 1.0f;

        mOnResumeTime = SystemClock.uptimeMillis();
        checkDisplayRotation();

        // If first time initialization is not finished, put it in the
        // message queue.
        if (!mFirstTimeInitialized) {
            mHandler.sendEmptyMessage(MSG_FIRST_TIME_INIT);
        } else {
            initializeSecondTime();
        }

        mHeadingSensor.activate();

        getServices().getRemoteShutterListener().onModuleReady(this);
        SessionStatsCollector.instance().sessionActive(true);
    }

    /**
     * @return Whether the currently active camera is front-facing.
     */
    private boolean isCameraFrontFacing() {
        return mAppController.getCameraProvider().getCharacteristics(mCameraId)
                .isFacingFront();
    }

    /**
     * The focus manager is the first UI related element to get initialized, and
     * it requires the RenderOverlay, so initialize it here
     */
    private void initializeFocusManager() {
        // Create FocusManager object. startPreview needs it.
        // if mFocusManager not null, reuse it
        // otherwise create a new instance
        if (mFocusManager != null) {
            mFocusManager.removeMessages();
        } else {
            mMirror = isCameraFrontFacing();
            String[] defaultFocusModesStrings = mActivity.getResources().getStringArray(
                    R.array.pref_camera_focusmode_default_array);
            ArrayList<CameraCapabilities.FocusMode> defaultFocusModes =
                    new ArrayList<CameraCapabilities.FocusMode>();
            CameraCapabilities.Stringifier stringifier = mCameraCapabilities.getStringifier();
            for (String modeString : defaultFocusModesStrings) {
                CameraCapabilities.FocusMode mode = stringifier.focusModeFromString(modeString);
                if (mode != null) {
                    defaultFocusModes.add(mode);
                }
            }
            mFocusManager =
                    new FocusOverlayManager(mAppController, defaultFocusModes,
                            mCameraCapabilities, this, mMirror, mActivity.getMainLooper(),
                            mUI.getFocusRing());
            mMotionManager = getServices().getMotionManager();
            if (mMotionManager != null) {
                mMotionManager.addListener(mFocusManager);
            }
        }
        mAppController.addPreviewAreaSizeChangedListener(mFocusManager);
    }

    /**
     * @return Whether we are resuming from within the lockscreen.
     */
    private boolean isResumeFromLockscreen() {
        String action = mActivity.getIntent().getAction();
        return (MediaStore.INTENT_ACTION_STILL_IMAGE_CAMERA.equals(action)
                || MediaStore.INTENT_ACTION_STILL_IMAGE_CAMERA_SECURE.equals(action));
    }

    @Override
    public void pause() {
        Log.v(TAG, "pause");
        mPaused = true;
        getServices().getRemoteShutterListener().onModuleExit();
        SessionStatsCollector.instance().sessionActive(false);

        mHeadingSensor.deactivate();

        // Reset the focus first. Camera CTS does not guarantee that
        // cancelAutoFocus is allowed after preview stops.
        if (mCameraDevice != null && mCameraState != PREVIEW_STOPPED) {
            mCameraDevice.cancelAutoFocus();
        }

        // If the camera has not been opened asynchronously yet,
        // and startPreview hasn't been called, then this is a no-op.
        // (e.g. onResume -> onPause -> onResume).
        stopPreview();
        cancelCountDown();
        mAppController.getCameraAppUI().smileShutterAnimator(false);
        if(mSoundplayer){
            mCountdownSoundPlayer.unloadSound(R.raw.timer_final_second);
            mCountdownSoundPlayer.unloadSound(R.raw.timer_increment);
            mCountdownSoundPlayer.release();
            mSoundplayer = false;
        }

        mNamedImages = null;
        // If we are in an image capture intent and has taken
        // a picture, we just clear it in onPause.
        mJpegImageData = null;

        // Remove the messages and runnables in the queue.
        mHandler.removeCallbacksAndMessages(null);

        if (mMotionManager != null) {
            mMotionManager.removeListener(mFocusManager);
            mMotionManager = null;
        }

        closeCamera();
        mActivity.enableKeepScreenOn(false);
        mUI.onPause();

        mPendingSwitchCameraId = -1;
        if (mFocusManager != null) {
            mFocusManager.removeMessages();
        }
        getServices().getMemoryManager().removeListener(this);
        mAppController.removePreviewAreaSizeChangedListener(mFocusManager);
        mAppController.removePreviewAreaSizeChangedListener(mUI);

        SettingsManager settingsManager = mActivity.getSettingsManager();
        settingsManager.removeListener(this);
    }

    @Override
    public void destroy() {
        Log.d(TAG,"destroy");
        if(mSoundplayer){
            mCountdownSoundPlayer.release();
            mSoundplayer = false;
        }
    }

    @Override
    public void onLayoutOrientationChanged(boolean isLandscape) {
        setDisplayOrientation();
    }

    @Override
    public void updateCameraOrientation() {
        if (mDisplayRotation != CameraUtil.getDisplayRotation()) {
            setDisplayOrientation();
        }
    }

    private boolean canTakePicture() {
        return isCameraIdle()
                && (mActivity.getStorageSpaceBytes() > Storage.LOW_STORAGE_THRESHOLD_BYTES);
    }

    @Override
    public void autoFocus() {
        if (mCameraDevice == null) {
            return;
        }
        Log.d(TAG,"Starting auto focus");
        mFocusStartTime = System.currentTimeMillis();
        mCameraDevice.autoFocus(mHandler, mAutoFocusCallback);
        SessionStatsCollector.instance().autofocusManualTrigger();
        setCameraState(FOCUSING);
    }

    @Override
    public void cancelAutoFocus() {
        if (mCameraDevice == null) {
            return;
        }
        mCameraDevice.cancelAutoFocus();
        setCameraState(IDLE);
        setCameraParameters(UPDATE_PARAM_PREFERENCE);
    }

    @Override
    public void onSingleTapUp(View view, int x, int y) {
        if (mPaused || mCameraDevice == null || !mFirstTimeInitialized
                || mCameraState == SNAPSHOT_IN_PROGRESS
                || mCameraState == SWITCHING_CAMERA
                || mCameraState == PREVIEW_STOPPED
                || mManualFocusing
                || mSmileShutterStarted
                || !isCurrentSceneModeSupportedFocus()
                || mAppController.getButtonManager().isZslButtonChanging()
                || !mAppController.isShutterEnabled()) {
            if (mManualFocusing) {
                mUI.hideManualFocusBar();
            }
            return;
        }

        // Check if metering area or focus area is supported.
        if (!mFocusAreaSupported && !mMeteringAreaSupported) {
            return;
        }
        mFocusManager.onSingleTapUp(x, y);
    }

    private boolean isCurrentSceneModeSupportedFocus() {
        return mSceneMode == CameraCapabilities.SceneMode.AUTO
                || mSceneMode == CameraCapabilities.SceneMode.PORTRAIT
                || mSceneMode == CameraCapabilities.SceneMode.HDR
                || mSceneMode == CameraCapabilities.SceneMode.BARCODE;
    }

    @Override
    public void onManualFocusChanged(int progress, int max) {
        // TODO Auto-generated method stub
        if (mPaused || mCameraDevice == null || !mFirstTimeInitialized
                || mCameraState == SNAPSHOT_IN_PROGRESS
                || mCameraState == SWITCHING_CAMERA
                || mCameraState == PREVIEW_STOPPED
                || mAppController.getButtonManager().isZslButtonChanging()) {
            return;
        }
        if (mManualFocusSupported) {
            if (mCameraDevice != null) {
                Camera.Parameters parameters = CameraUtil.getParameters(mCameraDevice);
                if (parameters.getFocusMode() != FOCUS_MODE_MANUAL)
                    parameters.setFocusMode(FOCUS_MODE_MANUAL);
                int value = (int) ((float) progress / (float) max * (float) mManualFocusValueRange);
                value = (value + mMinManualFocusValue) > mMaxManualFocusValue ? 
                        mMaxManualFocusValue : (value + mMinManualFocusValue);
                parameters.set(KEY_MANUAL_FOCUS_VAL, value);
                mCameraDevice.setParameters(parameters);
            }
        }
    }

    @Override
    public void onManualFocusStart(View parent) {
        // TODO Auto-generated method stub
        Log.i(TAG, "onManualFocusStart");
        if (mPaused || mCameraDevice == null || !mFirstTimeInitialized
                || mCameraState == SNAPSHOT_IN_PROGRESS
                || mCameraState == SWITCHING_CAMERA
                || mCameraState == PREVIEW_STOPPED
                || mAppController.getButtonManager().isZslButtonChanging()) {
            return;
        }
        mManualFocusing = true;
        mFocusManager.setManualFocusState(true);
        Camera.Parameters parameters = CameraUtil.getParameters(mCameraDevice);
        if (parameters.getFocusMode() != FOCUS_MODE_MANUAL)
            parameters.setFocusMode(FOCUS_MODE_MANUAL);
        mCameraDevice.setParameters(parameters);
        Camera.Parameters newParameters = CameraUtil.getParameters(mCameraDevice);
        String manualFocusValue = newParameters.get(KEY_MANUAL_FOCUS_LAST_VAL);
        if (manualFocusValue != null) {
            mUI.initManualFocusBarProgress(parent, Integer.parseInt(manualFocusValue),
                    mMaxManualFocusValue);
        }
        mFocusManager.onManualFocusStart();
    }

    @Override
    public void onManualFocusEnd() {
        // TODO Auto-generated method stub
        Log.i(TAG, "onManualFocusEnd");
        mManualFocusing = false;
        mFocusManager.setManualFocusState(false);
        if (mPaused || mCameraDevice == null || !mFirstTimeInitialized
                || mCameraState == SNAPSHOT_IN_PROGRESS
                || mCameraState == SWITCHING_CAMERA
                || mCameraState == PREVIEW_STOPPED
                || mAppController.getButtonManager().isZslButtonChanging()) {
            return;
        }
        mFocusManager.onManualFocusEnd();
    }

    @Override
    public boolean isManualFocusEnabled() {
        // TODO Auto-generated method stub
        return mManualFocusSupported && mManualFocusValueRange > 0
                && isCameraIdle();
    }

    @Override
    public boolean onBackPressed() {
        return mUI.onBackPressed();
    }

    @Override
    public boolean cancelSmileShutterIfOn() {
        // TODO Auto-generated method stub
        if ((!mAppController.isShutterEnabled() || DebugPropertyHelper.isSmileShutterAuto()) 
                && mSmileShutterStarted) {
            //if (mIntelCamera != null) {
                Log.i(TAG, "cancelSmileShutterIfOn");
                if (mCameraDevice != null) {
                    CameraStateHolder stateHolder = mCameraDevice.getCameraState();
                    int state = stateHolder.getState();
                    Log.i(TAG,"cur CameraStateHolder = " + state);
                    int waitTime = 0;
                    while (true) {
                        if (state >= (1 << 3)) {
                            stateHolder.setState(1 << 1);
                            break;
                        } if (waitTime > 2){
                            return false;
                        } else {
                            waitTime++;
                            Log.w(TAG, "has not capturing, not to cancel smileShutter waitTime = " + waitTime);
                            try {
                                Thread.sleep(300);
                                state = stateHolder.getState();
                                Log.i(TAG,"after wait CameraStateHolder = " + state);
                            } catch (Exception e) {
                                Log.e(TAG, "wait caputuring error:" + e);
                            }
                        }
                    }
                }
                //mIntelCamera.cancelSmartShutterPicture();
                enableButtonsWhenSmileShutterOff();
                resetStateAfterCapture();
                if (mAppController.getSettingsManager().getBoolean
                        (SettingsManager.SCOPE_GLOBAL, Keys.KEY_SMILE_SHUTTER_ON)
                        && DebugPropertyHelper.isSmileShutterAuto()) {
                    mHandler.removeCallbacks(mDoSmileShutterRunnable);
                    mAppController.getSettingsManager().set(SettingsManager.SCOPE_GLOBAL,
                            Keys.KEY_SMILE_SHUTTER_ON, false);
                }
                return true;
            //}
        }
        return false;
    }

    @Override
    public boolean onKeyDown(int keyCode, KeyEvent event) {
        switch (keyCode) {
            case KeyEvent.KEYCODE_VOLUME_UP:
            case KeyEvent.KEYCODE_VOLUME_DOWN:
            case KeyEvent.KEYCODE_FOCUS:
                if (/* TODO: mActivity.isInCameraApp() && */mFirstTimeInitialized &&
                    !mActivity.getCameraAppUI().isInIntentReview()) {
                    if (event.getRepeatCount() == 0) {
                        onShutterButtonFocus(true);
                    }
                    return true;
                }
                return false;
            case KeyEvent.KEYCODE_CAMERA:
                if (mFirstTimeInitialized && event.getRepeatCount() == 0) {
                    onShutterButtonClick();
                }
                return true;
            case KeyEvent.KEYCODE_DPAD_CENTER:
                // If we get a dpad center event without any focused view, move
                // the focus to the shutter button and press it.
                if (mFirstTimeInitialized && event.getRepeatCount() == 0) {
                    // Start auto-focus immediately to reduce shutter lag. After
                    // the shutter button gets the focus, onShutterButtonFocus()
                    // will be called again but it is fine.
                    onShutterButtonFocus(true);
                }
                return true;
        }
        return false;
    }

    @Override
    public boolean onKeyUp(int keyCode, KeyEvent event) {
        switch (keyCode) {
            case KeyEvent.KEYCODE_VOLUME_UP:
            case KeyEvent.KEYCODE_VOLUME_DOWN:
                if (/* mActivity.isInCameraApp() && */mFirstTimeInitialized &&
                    !mActivity.getCameraAppUI().isInIntentReview()) {
                    if (mUI.isCountingDown()) {
                        cancelCountDown();
                    } else {
                        mVolumeButtonClickedFlag = true;
                        onShutterButtonClick();
                    }
                    return true;
                }
                return false;
            case KeyEvent.KEYCODE_FOCUS:
                if (mFirstTimeInitialized) {
                    onShutterButtonFocus(false);
                }
                return true;
        }
        return false;
    }

    private void closeCamera() {
        if (mCameraDevice != null) {
            stopFaceDetection();
            mCameraDevice.setZoomChangeListener(null);
            mCameraDevice.setFaceDetectionCallback(null, null);

            mFaceDetectionStarted = false;
            mActivity.getCameraProvider().releaseCamera(mCameraDevice.getCameraId());
            mCameraDevice = null;
            setCameraState(PREVIEW_STOPPED);
            mFocusManager.onCameraReleased();
        }
    }

    private void setDisplayOrientation() {
        mDisplayRotation = CameraUtil.getDisplayRotation();
        Characteristics info =
                mActivity.getCameraProvider().getCharacteristics(mCameraId);
        mDisplayOrientation = info.getPreviewOrientation(mDisplayRotation);
        mUI.setDisplayOrientation(mDisplayOrientation);
        if (mFocusManager != null) {
            mFocusManager.setDisplayOrientation(mDisplayOrientation);
        }
        // Change the camera display orientation
        if (mCameraDevice != null) {
            mCameraDevice.setDisplayOrientation(mDisplayRotation);
        }
        Log.v(TAG, "setDisplayOrientation (screen:preview) " +
                mDisplayRotation + ":" + mDisplayOrientation);
    }

    /** Only called by UI thread. */
    private void setupPreview() {
        Log.i(TAG, "setupPreview");
        mFocusManager.resetTouchFocus();
        startPreview();
    }

    private void resetStateAfterCapture() {
        mBurstPictureLengthInterrupt = mBurstPictureLength;
        mBurstingPicture = false;
        //mFocusManager.updateFocusUI(); // Ensure focus indicator is hidden.
        mFocusManager.resetTouchFocus();
        mFocusManager.onPreviewStarted();
        mAppController.onPreviewStarted();
        enableButtonsWhenSmileShutterOff();
        mAppController.setShutterEnabled(true);
        setCameraState(IDLE);
        startFaceDetection();
    }

    /**
     * Returns whether we can/should start the preview or not.
     */
    private boolean checkPreviewPreconditions() {
        if (mPaused) {
            return false;
        }

        if (mCameraDevice == null) {
            Log.w(TAG, "startPreview: camera device not ready yet.");
            return false;
        }

        SurfaceTexture st = mActivity.getCameraAppUI().getSurfaceTexture();
        if (st == null) {
            Log.w(TAG, "startPreview: surfaceTexture is not ready.");
            return false;
        }

        if (!mCameraPreviewParamsReady) {
            Log.w(TAG, "startPreview: parameters for preview is not ready.");
            return false;
        }
        return true;
    }

    /**
     * The start/stop preview should only run on the UI thread.
     */
    private void startPreview() {
        if (mCameraDevice == null) {
            Log.i(TAG, "attempted to start preview before camera device");
            // do nothing
            return;
        }

        if (!checkPreviewPreconditions()) {
            return;
        }

        setDisplayOrientation();

        if (!mSnapshotOnIdle) {
            // If the focus mode is continuous autofocus, call cancelAutoFocus
            // to resume it because it may have been paused by autoFocus call.
            if (mFocusManager.getFocusMode(mCameraSettings.getCurrentFocusMode()) ==
                    CameraCapabilities.FocusMode.CONTINUOUS_PICTURE) {
                mCameraDevice.cancelAutoFocus();
            }
            mFocusManager.setAeAwbLock(false); // Unlock AE and AWB.
        }

        
        if (mActivity.getSettingsManager().getBoolean(SettingsManager.SCOPE_GLOBAL,
                Keys.KEY_BURST_CAPTURE_ON)) {
            if (mCameraDevice != null /*&& mIntelCamera != null*/) {
                Log.i(TAG, "setBurstParameters on");
                mCameraSettings.setBurstMode("on");
                mCameraSettings.setBurstLength(1);
            }
        } else {
            if (mCameraDevice != null /*&& mIntelCamera != null*/) {
                Log.i(TAG, "setBurstParameters off");
                mCameraSettings.setBurstMode("off");
                mCameraSettings.setBurstLength(1);
            }
        }

        // Nexus 4 must have picture size set to > 640x480 before other
        // parameters are set in setCameraParameters, b/18227551. This call to
        // updateParametersPictureSize should occur before setCameraParameters
        // to address the issue.
        updateParametersPictureSize();

        setCameraParameters(UPDATE_PARAM_ALL);

        mCameraDevice.setPreviewTexture(mActivity.getCameraAppUI().getSurfaceTexture());

        Log.i(TAG, "startPreview");
        // If we're using API2 in portability layers, don't use startPreviewWithCallback()
        // b/17576554
        CameraAgent.CameraStartPreviewCallback startPreviewCallback =
            new CameraAgent.CameraStartPreviewCallback() {
                @Override
                public void onPreviewStarted() {
                    mFocusManager.onPreviewStarted();
                    PhotoModule.this.onPreviewStarted();
                    SessionStatsCollector.instance().previewActive(true);
                    if (mSnapshotOnIdle
                            || (mAppController.getSettingsManager().getBoolean
                            (SettingsManager.SCOPE_GLOBAL, Keys.KEY_SMILE_SHUTTER_ON)
                            && DebugPropertyHelper.isSmileShutterAuto())) {
                        mHandler.postDelayed(mDoSmileShutterRunnable, 2000);
                    }
                }
            };
        if (GservicesHelper.useCamera2ApiThroughPortabilityLayer(mActivity.getContentResolver())) {
            mCameraDevice.startPreview();
            startPreviewCallback.onPreviewStarted();
        } else {
            mCameraDevice.startPreviewWithCallback(new Handler(Looper.getMainLooper()),
                    startPreviewCallback);
        }
    }

    @Override
    public void stopPreview() {
        Log.i(TAG, "mCameraState = " + mCameraState);
        if (mCameraDevice != null && mCameraState != PREVIEW_STOPPED) {
            if (mBurstingPicture) {
                mUI.setBurstCountdown(1);
                mBurstPictureLengthInterrupt = mBurstPictureIndex;
                mUI.setBurstSavingProgressVisible(false);
            }
            stopSmileShutter();
            Log.i(TAG, "stopPreview");
            mCameraDevice.stopPreview();
            mFaceDetectionStarted = false;
        }
        setCameraState(PREVIEW_STOPPED);
        if (mFocusManager != null) {
            mFocusManager.onPreviewStopped();
        }
        disableButtonsWhenSmileShutterOn();
        mAppController.setShutterEnabled(false);
        SessionStatsCollector.instance().previewActive(false);
    }

    @Override
    public void onSettingChanged(SettingsManager settingsManager, String key) {
        if (mPaused) return;
        Log.i(TAG, "onSettingChanged = " + key);
        if (key.equals(Keys.KEY_FLASH_MODE)) {
            updateParametersFlashMode();
        }
        /*if (key.equals(Keys.KEY_CAMERA_HDR)) {
            if (settingsManager.getBoolean(SettingsManager.SCOPE_GLOBAL,
                                           Keys.KEY_CAMERA_HDR)) {
                // HDR is on.
                mAppController.getButtonManager().disableButton(ButtonManager.BUTTON_FLASH);
                mFlashModeBeforeSceneMode = settingsManager.getString(
                        mAppController.getCameraScope(), Keys.KEY_FLASH_MODE);
            } else {
                if (mFlashModeBeforeSceneMode != null) {
                    settingsManager.set(mAppController.getCameraScope(),
                                        Keys.KEY_FLASH_MODE,
                                        mFlashModeBeforeSceneMode);
                    updateParametersFlashMode();
                    mFlashModeBeforeSceneMode = null;
                }
                mAppController.getButtonManager().enableButton(ButtonManager.BUTTON_FLASH);
            }
        }*/

        if (key.equals(Keys.KEY_SCENE_MODE)) {
            String curSceneMode = settingsManager.getString(mAppController.getCameraScope(), 
                    Keys.KEY_SCENE_MODE);
            if (mAppController.getButtonManager().isVisible(ButtonManager.BUTTON_WHITEBALANCE)) {
                if (!Camera.Parameters.SCENE_MODE_AUTO.equals(curSceneMode)) {
                    Log.i(TAG, "disable wb button");
                    if (mAppController.getButtonManager().isVisible(ButtonManager.BUTTON_WHITEBALANCE))
                        mAppController.getButtonManager().disableButton(ButtonManager.BUTTON_WHITEBALANCE);
                    mWhiteBalanceModeBeforeSceneMode = settingsManager.getString(
                            mAppController.getCameraScope(), Keys.KEY_WHITEBALANCE);
                } else {
                    Log.i(TAG, "enable wb button");
                    if (mWhiteBalanceModeBeforeSceneMode != null) {
                        settingsManager.set(mAppController.getCameraScope(),
                                Keys.KEY_WHITEBALANCE,
                                mWhiteBalanceModeBeforeSceneMode);
                        updateParametersWhiteBalance();
                        mWhiteBalanceModeBeforeSceneMode = null;
                    }
                    if (mAppController.getButtonManager().isVisible(ButtonManager.BUTTON_WHITEBALANCE))
                        mAppController.getButtonManager().enableButton(ButtonManager.BUTTON_WHITEBALANCE);
                }
            }
            if (mAppController.getButtonManager().isVisible(ButtonManager.BUTTON_FLASH)) {
                if (Camera.Parameters.SCENE_MODE_AUTO.equals(curSceneMode)) {
                    if (mFlashModeOfAutoScene != null) {
                        Log.d(TAG, "restore autoscene falshMode = " + mFlashModeOfAutoScene);
                        settingsManager.set(mAppController.getCameraScope(),
                                            Keys.KEY_FLASH_MODE,
                                            mFlashModeOfAutoScene);
                        updateParametersFlashMode();
                        mFlashModeOfAutoScene = null;
                    }
                    mAppController.getButtonManager().enableButton(ButtonManager.BUTTON_FLASH);
                }
                if (Camera.Parameters.SCENE_MODE_NIGHT.equals(curSceneMode)) {
                    if (mFlashModeOfNightScene != null) {
                        Log.d(TAG, "restore nightscene falshMode = " + mFlashModeOfNightScene);
                        settingsManager.set(mAppController.getCameraScope(),
                                            Keys.KEY_FLASH_MODE,
                                            mFlashModeOfNightScene);
                        updateParametersFlashMode();
                        mFlashModeOfNightScene = null;
                    }
                    mAppController.getButtonManager().enableButton(ButtonManager.BUTTON_FLASH);
                }
            }
            setFocusParameters();
        }

        if (key.equals(Keys.KEY_SMILE_SHUTTER_ON)) {
            if (settingsManager.getBoolean(SettingsManager.SCOPE_GLOBAL,
                    Keys.KEY_SMILE_SHUTTER_ON) && mFaceDetectionStarted) {
                startSmileShutter();
            } else {
                stopSmileShutter();
            }
        }

        if (key.equals(Keys.KEY_3DNR_ON)) {
            if (settingsManager.getBoolean(SettingsManager.SCOPE_GLOBAL,
                    Keys.KEY_3DNR_ON)) {
                if (mCameraSettings != null)
                    mCameraSettings.set3dnrEnabled(true);
            } else {
                if (mCameraSettings != null)
                    mCameraSettings.set3dnrEnabled(false);
            }
        }

        if (key.equals(Keys.KEY_BURST_CAPTURE_ON)) {
            boolean settingsFirstRun = !settingsManager.isSet(SettingsManager.SCOPE_GLOBAL,
                    Keys.KEY_SETTINGS_FIRST_RUN);
            Log.i(TAG, "settingsFirstRun = " + settingsFirstRun);
            mAppController.getButtonManager().setZslButtonChangeOver();
            if (!settingsFirstRun) {
                if (mFocusManager != null && !mFocusManager.isFocusCompleted() && mCameraDevice != null) {
                    Log.i(TAG,"cancel autofocus before switch zsl");
                    mCameraDevice.cancelAutoFocus();
                }
                stopPreview();
                startPreview();
            }
        } else {
            if (mCameraDevice != null) {
                mCameraDevice.applySettings(mCameraSettings);
            }
        }
    }

    private void updateCameraParametersInitialize() {
        // Reset preview frame rate to the maximum because it may be lowered by
        // video camera application.
        int[] fpsRange = CameraUtil.getPhotoPreviewFpsRange(mCameraCapabilities);
        if (fpsRange != null && fpsRange.length > 0) {
            mCameraSettings.setPreviewFpsRange(fpsRange[0], fpsRange[1]);
        }

        mCameraSettings.setRecordingHintEnabled(false);

        if (mCameraCapabilities.supports(CameraCapabilities.Feature.VIDEO_STABILIZATION)) {
            mCameraSettings.setVideoStabilization(false);
        }
    }

    private void updateCameraParametersZoom() {
        // Set zoom.
        if (mCameraCapabilities.supports(CameraCapabilities.Feature.ZOOM)) {
            mCameraSettings.setZoomRatio(mZoomValue);
        }
    }

    @TargetApi(Build.VERSION_CODES.JELLY_BEAN)
    private void setAutoExposureLockIfSupported() {
        if (mAeLockSupported) {
            mCameraSettings.setAutoExposureLock(mFocusManager.getAeAwbLock());
        }
    }

    @TargetApi(Build.VERSION_CODES.JELLY_BEAN)
    private void setAutoWhiteBalanceLockIfSupported() {
        if (mAwbLockSupported) {
            mCameraSettings.setAutoWhiteBalanceLock(mFocusManager.getAeAwbLock());
        }
    }

    private void setFocusAreasIfSupported() {
        if (mFocusAreaSupported) {
            mCameraSettings.setFocusAreas(mFocusManager.getFocusAreas());
        }
    }

    private void setMeteringAreasIfSupported() {
        if (mMeteringAreaSupported) {
            mCameraSettings.setMeteringAreas(mFocusManager.getMeteringAreas());
        }
    }

    private void updateCameraParametersPreference() {
        // some monkey tests can get here when shutting the app down
        // make sure mCameraDevice is still valid, b/17580046
        if (mCameraDevice == null) {
            return;
        }

        setAutoExposureLockIfSupported();
        setAutoWhiteBalanceLockIfSupported();
        setFocusAreasIfSupported();
        setMeteringAreasIfSupported();

        // Initialize focus mode.
        mFocusManager.overrideFocusMode(null);
        mCameraSettings
                .setFocusMode(mFocusManager.getFocusMode(mCameraSettings.getCurrentFocusMode()));
        SessionStatsCollector.instance().autofocusActive(
                mFocusManager.getFocusMode(mCameraSettings.getCurrentFocusMode()) ==
                        CameraCapabilities.FocusMode.CONTINUOUS_PICTURE
        );

        // Set JPEG quality.
        updateParametersPictureQuality();

        // For the following settings, we need to check if the settings are
        // still supported by latest driver, if not, ignore the settings.

        // Set exposure compensation
        updateParametersExposureCompensation();
        
        updateParametersWhiteBalance();
        
        updateParametersColorEffect();
        updateParametersAntiBanding();
        updateParametersSaturation();
        updateParametersContrast();
        updateParametersSharpness();
        updateParametersBrightness();
        updateParametersHue();

        update3dnrState();

        // Set the scene mode: also sets flash and white balance.
        updateParametersSceneMode();

        if (mContinuousFocusSupported && ApiHelper.HAS_AUTO_FOCUS_MOVE_CALLBACK) {
            updateAutoFocusMoveCallback();
        }
    }

    /**
     * This method sets picture size parameters. Size parameters should only be
     * set when the preview is stopped, and so this method is only invoked in
     * {@link #startPreview()} just before starting the preview.
     */
    private void updateParametersPictureSize() {
        if (mCameraDevice == null) {
            Log.w(TAG, "attempting to set picture size without caemra device");
            return;
        }

        List<Size> supported = Size.convert(mCameraCapabilities.getSupportedPhotoSizes());
        CameraPictureSizesCacher.updateSizesForCamera(mAppController.getAndroidContext(),
                mCameraDevice.getCameraId(), supported);

        CameraPictureSizesCacher.updateMaxDetectionFacesForCamera(mAppController.getAndroidContext(),
                mCameraDevice.getCameraId(), mCameraCapabilities.getMaxNumOfFacesSupported());

        OneCamera.Facing cameraFacing =
              isCameraFrontFacing() ? OneCamera.Facing.FRONT : OneCamera.Facing.BACK;
        Size pictureSize;
        try {
            pictureSize = mAppController.getResolutionSetting().getPictureSize(
                  mAppController.getCameraProvider().getCurrentCameraId(),
                  cameraFacing);
        } catch (OneCameraAccessException ex) {
            mAppController.getFatalErrorHandler().onGenericCameraAccessFailure();
            return;
        }

        mCameraSettings.setPhotoSize(pictureSize.toPortabilitySize());

        if (ApiHelper.IS_NEXUS_5) {
            if (ResolutionUtil.NEXUS_5_LARGE_16_BY_9.equals(pictureSize)) {
                mShouldResizeTo16x9 = true;
            } else {
                mShouldResizeTo16x9 = false;
            }
        }

        // Set a preview size that is closest to the viewfinder height and has
        // the right aspect ratio.
        List<Size> sizes = Size.convert(mCameraCapabilities.getSupportedPreviewSizes());
        Size optimalSize = CameraUtil.getOptimalPreviewSize(sizes,
                (double) pictureSize.width() / pictureSize.height());
        Size original = new Size(mCameraSettings.getCurrentPreviewSize());
        if (!optimalSize.equals(original)) {
            Log.v(TAG, "setting preview size. optimal: " + optimalSize + "original: " + original);
            mCameraSettings.setPreviewSize(optimalSize.toPortabilitySize());

            mCameraDevice.applySettings(mCameraSettings);
            mCameraSettings = mCameraDevice.getSettings();
        }

        if (optimalSize.width() != 0 && optimalSize.height() != 0) {
            Log.v(TAG, "updating aspect ratio");
            mUI.updatePreviewAspectRatio((float) optimalSize.width()
                    / (float) optimalSize.height());
        }
        Log.d(TAG, "Preview size is " + optimalSize);
    }

    private void updateParametersPictureQuality() {
        int jpegQuality = CameraProfile.getJpegEncodingQualityParameter(mCameraId,
                CameraProfile.QUALITY_HIGH);
        mCameraSettings.setPhotoJpegCompressionQuality(jpegQuality);
    }

    private void updateParametersExposureCompensation() {
        SettingsManager settingsManager = mActivity.getSettingsManager();
        if (settingsManager.getBoolean(SettingsManager.SCOPE_GLOBAL,
                                       Keys.KEY_EXPOSURE_COMPENSATION_ENABLED)) {
            int value = settingsManager.getInteger(mAppController.getCameraScope(),
                                                   Keys.KEY_EXPOSURE);
            int max = mCameraCapabilities.getMaxExposureCompensation();
            int min = mCameraCapabilities.getMinExposureCompensation();
            if (value >= min && value <= max) {
                mCameraSettings.setExposureCompensationIndex(value);
            } else {
                Log.w(TAG, "invalid exposure range: " + value);
            }
        } else {
            // If exposure compensation is not enabled, reset the exposure compensation value.
            setExposureCompensation(0);
        }
    }

    private void updateParametersSaturation() {
        SettingsManager settingsManager = mActivity.getSettingsManager();
        String saturation = settingsManager.getString(mAppController.getCameraScope(),
                Keys.KEY_SATURATION);
        if (mSupportedSaturations != null && mSupportedSaturations.contains(saturation)
                && settingsManager.getBoolean(SettingsManager.SCOPE_GLOBAL,
                        Keys.KEY_EXPOSURE_COMPENSATION_ENABLED)) {
            mCameraSettings.setSaturation(saturation);
        } else {
            if (DEBUG) Log.i(TAG, "default saturation");
            setSaturationInternal("normal");
        }
    }

    private void updateParametersContrast() {
        SettingsManager settingsManager = mActivity.getSettingsManager();
        String contrast = settingsManager.getString(mAppController.getCameraScope(),
                Keys.KEY_CONTRAST);
        if (mSupportedContrasts != null && mSupportedContrasts.contains(contrast)
                && settingsManager.getBoolean(SettingsManager.SCOPE_GLOBAL,
                        Keys.KEY_EXPOSURE_COMPENSATION_ENABLED)) {
            mCameraSettings.setContrast(contrast);
        } else {
            if (DEBUG) Log.i(TAG, "default contrast");
            setContrastInternal("normal");
        }
    }

    private void updateParametersSharpness() {
        SettingsManager settingsManager = mActivity.getSettingsManager();
        String sharpness = settingsManager.getString(mAppController.getCameraScope(),
                Keys.KEY_SHARPNESS);
        if (mSupportedSharpnesses != null && mSupportedSharpnesses.contains(sharpness)
                && settingsManager.getBoolean(SettingsManager.SCOPE_GLOBAL,
                        Keys.KEY_EXPOSURE_COMPENSATION_ENABLED)) {
            mCameraSettings.setSharpness(sharpness);
        } else {
            if (DEBUG) Log.i(TAG, "default sharpness");
            setSharpnessInternal("normal");
        }
    }

    private void updateParametersBrightness() {
        SettingsManager settingsManager = mActivity.getSettingsManager();
        String brightness = settingsManager.getString(mAppController.getCameraScope(),
                Keys.KEY_BRIGHTNESS);
        if (mSupportedBrightnesses != null && mSupportedBrightnesses.contains(brightness)
                && settingsManager.getBoolean(SettingsManager.SCOPE_GLOBAL,
                        Keys.KEY_EXPOSURE_COMPENSATION_ENABLED)) {
            mCameraSettings.setBrightness(brightness);
        } else {
            if (DEBUG) Log.i(TAG, "default brightness");
            setBrightnessInternal("normal");
        }
    }

    private void updateParametersHue() {
        SettingsManager settingsManager = mActivity.getSettingsManager();
        String hue = settingsManager.getString(mAppController.getCameraScope(),
                Keys.KEY_HUE);
        if (mSupportedHues != null && mSupportedHues.contains(hue)
                && settingsManager.getBoolean(SettingsManager.SCOPE_GLOBAL,
                        Keys.KEY_EXPOSURE_COMPENSATION_ENABLED)) {
            mCameraSettings.setHue(hue);
        } else {
            if (DEBUG) Log.i(TAG, "default hue");
            setHueInternal("normal");
        }
    }

    private void updateSmileShutterState() {
        Log.i(TAG,"updateSmileShutterState");
        SettingsManager settingsManager = mActivity.getSettingsManager();
        boolean on = settingsManager.getBoolean(SettingsManager.SCOPE_GLOBAL,
                Keys.KEY_SMILE_SHUTTER_ON);
        if (on) {
            startSmileShutter();
        } else {
            stopSmileShutter();
        }
    }

    private void update3dnrState() {
        Log.i(TAG,"update3dnrState");
        SettingsManager settingsManager = mActivity.getSettingsManager();
        boolean on = settingsManager.getBoolean(SettingsManager.SCOPE_GLOBAL,
                Keys.KEY_3DNR_ON);
        if (on) {
            mCameraSettings.set3dnrEnabled(true);
        } else {
            mCameraSettings.set3dnrEnabled(false);
        }
    }

    private void updateParametersWhiteBalance() {
        SettingsManager settingsManager = mActivity.getSettingsManager();
        CameraCapabilities.Stringifier stringifier = mCameraCapabilities.getStringifier();
        if (settingsManager.getBoolean(SettingsManager.SCOPE_GLOBAL,
                                       Keys.KEY_WHITEBALANCE_ENABLED)) {
            String value = settingsManager.getString(mAppController.getCameraScope(),
                                                   Keys.KEY_WHITEBALANCE);
            CameraCapabilities.WhiteBalance wb = stringifier.whiteBalanceFromString(value);
            if (mCameraCapabilities.supports(wb)) {
                mCameraSettings.setWhiteBalance(wb);
            } else {
                Log.w(TAG, "invalid whitebalance range: " + value);
            }
        } else {
            if (DEBUG) Log.i(TAG, "default WhiteBalance");
            setWhiteBalanceInternal(stringifier.stringify(CameraCapabilities.WhiteBalance.AUTO));
        }
    }

    private void updateParametersColorEffect() {
        SettingsManager settingsManager = mActivity.getSettingsManager();
        String effect = settingsManager.getString(mAppController.getCameraScope(),
                Keys.KEY_COLOR_EFFECT);
        if (mSupportedColorEffects != null && mSupportedColorEffects.contains(effect)) {
            mCameraSettings.setColorEffect(effect);
        } else {
            if (DEBUG) Log.i(TAG, "default color effect");
            setColorEffectInternal(Camera.Parameters.EFFECT_NONE);
        }
    }

    private void updateParametersAntiBanding() {
        SettingsManager settingsManager = mActivity.getSettingsManager();
        String antibanding = settingsManager.getString(SettingsManager.SCOPE_GLOBAL,
                Keys.KEY_ANTIBANDING);
        if (mSupportedAntiBandings != null && mSupportedAntiBandings.contains(antibanding)) {
            mCameraSettings.setAntiBanding(antibanding);
        } else {
            if (DEBUG) Log.i(TAG, "default antibanding");
            setAntiBandingInternal(Camera.Parameters.ANTIBANDING_AUTO);
        }
    }

    private void updateParametersSceneMode() {
        if (mCameraDevice == null) return;
        CameraCapabilities.Stringifier stringifier = mCameraCapabilities.getStringifier();
        SettingsManager settingsManager = mActivity.getSettingsManager();

        final CameraCapabilities.SceneMode sceneModeHistory = mSceneMode;
        mSceneMode = stringifier.
            sceneModeFromString(settingsManager.getString(mAppController.getCameraScope(),
                                                          Keys.KEY_SCENE_MODE));
        Log.i(TAG, "updateParametersSceneMode mSceneMode = " + mSceneMode
            + ",flashmode = " + mCameraSettings.getCurrentFlashMode());
        if (mCameraCapabilities.supports(mSceneMode)) {
            if (mCameraSettings.getCurrentSceneMode() != mSceneMode) {
                if (mAppController.getButtonManager().isVisible(ButtonManager.BUTTON_FLASH)) {
                    if (CameraCapabilities.SceneMode.AUTO != mSceneMode) {
                        if (CameraCapabilities.SceneMode.AUTO == sceneModeHistory)
                            mFlashModeOfAutoScene = settingsManager.getString(
                                mAppController.getCameraScope(), Keys.KEY_FLASH_MODE);
                        Log.d(TAG, "store autoscene falshMode = " + mFlashModeOfAutoScene);
                    }
                    if (CameraCapabilities.SceneMode.NIGHT != mSceneMode) {
                        if (CameraCapabilities.SceneMode.NIGHT == sceneModeHistory)
                            mFlashModeOfNightScene = settingsManager.getString(
                                mAppController.getCameraScope(), Keys.KEY_FLASH_MODE);
                        Log.d(TAG, "store nightscene falshMode = " + mFlashModeOfNightScene);
                    }
                }
                if (CameraCapabilities.SceneMode.AUTO == mSceneMode) {
                    if (CameraCapabilities.SceneMode.AUTO != sceneModeHistory)
                        settingsManager.setToDefault(mAppController.getCameraScope(), Keys.KEY_FOCUS_MODE);
                }
                mCameraSettings.setSceneMode(mSceneMode);

                // Setting scene mode will change the settings of flash mode,
                // white balance, and focus mode. Here we read back the
                // parameters, so we can know those settings.
                mCameraDevice.applySettings(mCameraSettings);
                mCameraSettings = mCameraDevice.getSettings();
            }
        } else {
            mSceneMode = mCameraSettings.getCurrentSceneMode();
            if (mSceneMode == null) {
                mSceneMode = CameraCapabilities.SceneMode.AUTO;
            }
        }
        /*if (mAppController.getSettingsManager()
                .getBoolean(SettingsManager.SCOPE_GLOBAL, Keys.KEY_BURST_CAPTURE_ON)) {
            if (mAppController.getButtonManager().isVisible(ButtonManager.BUTTON_FLASH))
                mAppController.getButtonManager().disableButton(ButtonManager.BUTTON_FLASH);
        } else */if (CameraCapabilities.SceneMode.AUTO != mSceneMode
                && CameraCapabilities.SceneMode.NIGHT != mSceneMode) {
            if (mAppController.getButtonManager().isVisible(ButtonManager.BUTTON_FLASH))
                mAppController.getButtonManager().disableButton(ButtonManager.BUTTON_FLASH);
        }

        if (CameraCapabilities.SceneMode.AUTO == mSceneMode
                || CameraCapabilities.SceneMode.NIGHT == mSceneMode) {
            // Set flash mode.
            updateParametersFlashMode();

            // Set focus mode.
            mFocusManager.overrideFocusMode(null);
            mCameraSettings.setFocusMode(
                    mFocusManager.getFocusMode(mCameraSettings.getCurrentFocusMode()));
        } else {
            mFocusManager.overrideFocusMode(mCameraSettings.getCurrentFocusMode());
            mWhiteBalanceModeBeforeSceneMode = settingsManager.getString(
                    mAppController.getCameraScope(), Keys.KEY_WHITEBALANCE);
        }
    }

    private void updateParametersFlashMode() {
        SettingsManager settingsManager = mActivity.getSettingsManager();

        CameraCapabilities.FlashMode flashMode = mCameraCapabilities.getStringifier()
            .flashModeFromString(settingsManager.getString(mAppController.getCameraScope(),
                                                           Keys.KEY_FLASH_MODE));
        if (mCameraCapabilities.supports(flashMode)) {
            mCameraSettings.setFlashMode(flashMode);
        }
    }

    @TargetApi(Build.VERSION_CODES.JELLY_BEAN)
    private void updateAutoFocusMoveCallback() {
        if (mCameraDevice == null) {
            return;
        }
        if (mCameraSettings.getCurrentFocusMode() ==
                CameraCapabilities.FocusMode.CONTINUOUS_PICTURE) {
            Log.i(TAG, "FocusMode.CONTINUOUS_PICTURE setAutoFocusMoveCallback");
            mCameraDevice.setAutoFocusMoveCallback(mHandler,
                    (CameraAFMoveCallback) mAutoFocusMoveCallback);
        } else {
            Log.i(TAG, "FocusMode.CONTINUOUS_PICTURE setAutoFocusMoveCallback null");
            mCameraDevice.setAutoFocusMoveCallback(null, null);
        }
    }

    /**
     * Sets the exposure compensation to the given value and also updates settings.
     *
     * @param value exposure compensation value to be set
     */
    public void setExposureCompensation(int value) {
        if (DEBUG) Log.i(TAG,"setExposureCompensation = " + value);
        int max = mCameraCapabilities.getMaxExposureCompensation();
        int min = mCameraCapabilities.getMinExposureCompensation();
        if (value >= min && value <= max) {
            mCameraSettings.setExposureCompensationIndex(value);
            SettingsManager settingsManager = mActivity.getSettingsManager();
            settingsManager.set(mAppController.getCameraScope(),
                                Keys.KEY_EXPOSURE, value);
        } else {
            Log.w(TAG, "invalid exposure range: " + value);
        }
    }

    public void setWhiteBalanceInternal(String value)  {
        if (DEBUG) Log.i(TAG,"setWhiteBalance = " + value);
        CameraCapabilities.Stringifier stringifier = mCameraCapabilities.getStringifier();
        CameraCapabilities.WhiteBalance wb = stringifier.whiteBalanceFromString(value);
        if (mCameraCapabilities.supports(wb)) {
            mCameraSettings.setWhiteBalance(wb);
            SettingsManager settingsManager = mActivity.getSettingsManager();
            settingsManager.set(mAppController.getCameraScope(),
                    Keys.KEY_WHITEBALANCE, value);
        } else {
            Log.w(TAG, "invalid whitebalance: " + value);
        }
    }

    public void setSceneMode(String value) {
        if (DEBUG) Log.i(TAG,"setSceneMode = " + value);
        CameraCapabilities.Stringifier stringifier = mCameraCapabilities.getStringifier();
        CameraCapabilities.SceneMode scene = stringifier.sceneModeFromString(value);
//        if (mCameraCapabilities.supports(scene)) {
//            mCameraSettings.setSceneMode(scene);
//            SettingsManager settingsManager = mActivity.getSettingsManager();
//            settingsManager.set(mAppController.getCameraScope(),
//                    Keys.KEY_SCENE_MODE, value);
//        } else {
//            Log.w(TAG, "invalid scene: " + value);
//        }
        if (mCameraCapabilities.supports(scene)) {
            SettingsManager settingsManager = mActivity.getSettingsManager();
            if (Keys.isHdrOn(settingsManager) 
                    && !Camera.Parameters.SCENE_MODE_HDR.equals(scene)) {
                settingsManager.set(SettingsManager.SCOPE_GLOBAL, Keys.KEY_CAMERA_HDR, false);
            }
            settingsManager.set(mAppController.getCameraScope(), Keys.KEY_SCENE_MODE,
                    mCameraCapabilities.getStringifier().stringify(
                            scene));

            updateParametersSceneMode();
            if (mCameraDevice != null) {
                mCameraDevice.applySettings(mCameraSettings);
            }
            updateSceneMode();
        } else {
            Log.w(TAG, "invalid scene: " + value);
            if (mAppController.getSettingsManager()
                    .getBoolean(SettingsManager.SCOPE_GLOBAL, Keys.KEY_BURST_CAPTURE_ON)) {
                if (mAppController.getButtonManager().isVisible(ButtonManager.BUTTON_FLASH))
                    mAppController.getButtonManager().disableButton(ButtonManager.BUTTON_FLASH);
            } else if (CameraCapabilities.SceneMode.AUTO != mSceneMode
                    && CameraCapabilities.SceneMode.NIGHT != mSceneMode) {
                if (mAppController.getButtonManager().isVisible(ButtonManager.BUTTON_FLASH))
                    mAppController.getButtonManager().disableButton(ButtonManager.BUTTON_FLASH);
            }
        }
    }

    public void setColorEffectInternal(String value) {
        if (DEBUG) Log.i(TAG,"setColorEffect = " + value);
        if (mSupportedColorEffects != null && mSupportedColorEffects.contains(value)) {
            mCameraSettings.setColorEffect(value);
            SettingsManager settingsManager = mActivity.getSettingsManager();
            settingsManager.set(mAppController.getCameraScope(),
                    Keys.KEY_COLOR_EFFECT, value);
        } else {
            Log.w(TAG, "invalid color effect: " + value);
        }
    }

    public void setSaturationInternal(String value) {
        if (DEBUG) Log.i(TAG,"setSaturation = " + value);
        if (mSupportedSaturations != null && mSupportedSaturations.contains(value)) {
            mCameraSettings.setSaturation(value);
            SettingsManager settingsManager = mActivity.getSettingsManager();
            settingsManager.set(mAppController.getCameraScope(),
                    Keys.KEY_SATURATION, value);
        } else {
            Log.w(TAG, "invalid saturation: " + value);
        }
    }

    public void setContrastInternal(String value) {
        if (DEBUG) Log.i(TAG,"setContrast = " + value);
        if (mSupportedContrasts != null && mSupportedContrasts.contains(value)) {
            mCameraSettings.setContrast(value);
            SettingsManager settingsManager = mActivity.getSettingsManager();
            settingsManager.set(mAppController.getCameraScope(),
                    Keys.KEY_CONTRAST, value);
        } else {
            Log.w(TAG, "invalid contrast: " + value);
        }
    }

    public void setSharpnessInternal(String value) {
        if (DEBUG) Log.i(TAG,"setSharpness = " + value);
        if (mSupportedSharpnesses != null && mSupportedSharpnesses.contains(value)) {
            mCameraSettings.setSharpness(value);
            SettingsManager settingsManager = mActivity.getSettingsManager();
            settingsManager.set(mAppController.getCameraScope(),
                    Keys.KEY_SHARPNESS, value);
        } else {
            Log.w(TAG, "invalid sharpness: " + value);
        }
    }

    public void setBrightnessInternal(String value) {
        if (DEBUG) Log.i(TAG,"setBrightness = " + value);
        if (mSupportedBrightnesses != null && mSupportedBrightnesses.contains(value)) {
            mCameraSettings.setBrightness(value);
            SettingsManager settingsManager = mActivity.getSettingsManager();
            settingsManager.set(mAppController.getCameraScope(),
                    Keys.KEY_BRIGHTNESS, value);
        } else {
            Log.w(TAG, "invalid brightness: " + value);
        }
    }

    public void setHueInternal(String value) {
        if (DEBUG) Log.i(TAG,"setHue = " + value);
        if (mSupportedHues != null && mSupportedHues.contains(value)) {
            mCameraSettings.setHue(value);
            SettingsManager settingsManager = mActivity.getSettingsManager();
            settingsManager.set(mAppController.getCameraScope(),
                    Keys.KEY_HUE, value);
        } else {
            Log.w(TAG, "invalid hue: " + value);
        }
    }

    public void setAntiBandingInternal(String value) {
        if (DEBUG) Log.i(TAG,"setAntiBanding = " + value);
        if (mSupportedAntiBandings != null && mSupportedAntiBandings.contains(value)) {
            mCameraSettings.setAntiBanding(value);
            SettingsManager settingsManager = mActivity.getSettingsManager();
            settingsManager.set(SettingsManager.SCOPE_GLOBAL,
                    Keys.KEY_ANTIBANDING, value);
        } else {
            Log.w(TAG, "invalid AntiBanding: " + value);
        }
    }

    // We separate the parameters into several subsets, so we can update only
    // the subsets actually need updating. The PREFERENCE set needs extra
    // locking because the preference can be changed from GLThread as well.
    private void setCameraParameters(int updateSet) {
        if ((updateSet & UPDATE_PARAM_INITIALIZE) != 0) {
            updateCameraParametersInitialize();
        }

        if ((updateSet & UPDATE_PARAM_ZOOM) != 0) {
            updateCameraParametersZoom();
        }

        if ((updateSet & UPDATE_PARAM_PREFERENCE) != 0) {
            updateCameraParametersPreference();
        }

        if (mCameraDevice != null) {
            mCameraDevice.applySettings(mCameraSettings);
        }
    }

    // If the Camera is idle, update the parameters immediately, otherwise
    // accumulate them in mUpdateSet and update later.
    private void setCameraParametersWhenIdle(int additionalUpdateSet) {
        mUpdateSet |= additionalUpdateSet;
        if (mCameraDevice == null) {
            // We will update all the parameters when we open the device, so
            // we don't need to do anything now.
            mUpdateSet = 0;
            return;
        } else if (isCameraIdle()) {
            setCameraParameters(mUpdateSet);
            updateSceneMode();
            mUpdateSet = 0;
        } else {
            if (!mHandler.hasMessages(MSG_SET_CAMERA_PARAMETERS_WHEN_IDLE)) {
                mHandler.sendEmptyMessageDelayed(MSG_SET_CAMERA_PARAMETERS_WHEN_IDLE, 1000);
            }
        }
    }

    @Override
    public boolean isCameraIdle() {
        return (mCameraState == IDLE) ||
                (mCameraState == PREVIEW_STOPPED) ||
                ((mFocusManager != null) && mFocusManager.isFocusCompleted()
                && (mCameraState != SWITCHING_CAMERA));
    }

    @Override
    public boolean isImageCaptureIntent() {
        String action = mActivity.getIntent().getAction();
        return (MediaStore.ACTION_IMAGE_CAPTURE.equals(action)
        || CameraActivity.ACTION_IMAGE_CAPTURE_SECURE.equals(action));
    }

    private void setupCaptureParams() {
        Bundle myExtras = mActivity.getIntent().getExtras();
        if (myExtras != null) {
            mSaveUri = (Uri) myExtras.getParcelable(MediaStore.EXTRA_OUTPUT);
            mCropValue = myExtras.getString("crop");
        }
    }

    private void initializeCapabilities() {
        mCameraCapabilities = mCameraDevice.getCapabilities();
        mFocusAreaSupported = mCameraCapabilities.supports(CameraCapabilities.Feature.FOCUS_AREA);
        mMeteringAreaSupported = mCameraCapabilities.supports(CameraCapabilities.Feature.METERING_AREA);
        mAeLockSupported = mCameraCapabilities.supports(CameraCapabilities.Feature.AUTO_EXPOSURE_LOCK);
        mAwbLockSupported = mCameraCapabilities.supports(CameraCapabilities.Feature.AUTO_WHITE_BALANCE_LOCK);
        mContinuousFocusSupported =
                mCameraCapabilities.supports(CameraCapabilities.FocusMode.CONTINUOUS_PICTURE);

        mSupportedColorEffects = mCameraCapabilities.getSupportedColorEffects();
        mSupportedSaturations = mCameraCapabilities.getSupportedSaturations();
        mSupportedContrasts = mCameraCapabilities.getSupportedContrasts();
        mSupportedSharpnesses = mCameraCapabilities.getSupportedSharpnesses();
        mSupportedBrightnesses = mCameraCapabilities.getSupportedBrightnesses();
        mSupportedHues = mCameraCapabilities.getSupportedHues();
        mSupportedAntiBandings = mCameraCapabilities.getSupportedAntiBanding();
        mSupportedFocusModes = CameraUtil.getParameters(mCameraDevice).getSupportedFocusModes();
        mManualFocusSupported = mSupportedFocusModes.contains(FOCUS_MODE_MANUAL);
        String manualFocusRange = CameraUtil.getParameters(mCameraDevice).get(KEY_MANUAL_FOCUS_VAL_RANGE);
        if (manualFocusRange != null) {
            String[] range = manualFocusRange.split(",");
            if (range != null && range.length == 2) {
                mMinManualFocusValue = Integer.parseInt(range[0]);
                mMaxManualFocusValue = Integer.parseInt(range[1]);
                mManualFocusValueRange = mMaxManualFocusValue - mMinManualFocusValue;
            }
        }
        String m3dnrEnabled = CameraUtil.getParameters(mCameraDevice).get("3dnr_enabled");
        if (m3dnrEnabled != null)
            m3dnrSupported = false;
        else
            m3dnrSupported = false;
        String value = CameraUtil.getParameters(mCameraDevice).get("exposure_for_cts");
        if ("1".equals(value))
            mExposureSupported = false;
        else
            mExposureSupported = true;
        mAppController.getButtonManager().setCameraCapabilities(mCameraCapabilities);

        Log.i(TAG,"CameraCapabilities--->" 
                + ",mFocusAreaSupported = " + mFocusAreaSupported
                + ",mMeteringAreaSupported = " + mMeteringAreaSupported
                + ",mAeLockSupported = " + mAeLockSupported
                + ",mAwbLockSupported = " + mAwbLockSupported
                + ",mContinuousFocusSupported = " + mContinuousFocusSupported
                + ",mManualFocusSupported = " + mManualFocusSupported
                + ",manualFocusRange = " + manualFocusRange);
    }

    @Override
    public void onZoomChanged(float ratio) {
        // Not useful to change zoom value when the activity is paused.
        if (mPaused) {
            return;
        }
        mZoomValue = ratio;
        if (mCameraSettings == null || mCameraDevice == null) {
            return;
        }
        // Set zoom parameters asynchronously
        mCameraSettings.setZoomRatio(mZoomValue);
        mCameraDevice.applySettings(mCameraSettings);
    }

    @Override
    public int getCameraState() {
        return mCameraState;
    }

    @Override
    public void onMemoryStateChanged(int state) {
        mAppController.setShutterEnabled(state == MemoryManager.STATE_OK);
        if (state == MemoryManager.STATE_OK)
            enableButtonsWhenSmileShutterOff();
        else
            disableButtonsWhenSmileShutterOn();
    }

    @Override
    public void onLowMemory() {
        // Not much we can do in the photo module.
    }

    // For debugging only.
    public void setDebugUri(Uri uri) {
        mDebugUri = uri;
    }

    // For debugging only.
    private void saveToDebugUri(byte[] data) {
        if (mDebugUri != null) {
            OutputStream outputStream = null;
            try {
                outputStream = mContentResolver.openOutputStream(mDebugUri);
                outputStream.write(data);
                outputStream.close();
            } catch (IOException e) {
                Log.e(TAG, "Exception while writing debug jpeg file", e);
            } finally {
                CameraUtil.closeSilently(outputStream);
            }
        }
    }

    @Override
    public void onRemoteShutterPress() {
        mHandler.post(new Runnable() {
            @Override
            public void run() {
                focusAndCapture();
            }
        });
    }

    @Override
    public void onNonDecorWindowSizeChanged() {
        // TODO Auto-generated method stub
        
    }
}
