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
package com.android.cts.verifier.camera.intents;

import android.app.job.JobInfo;
import android.app.job.JobParameters;
import android.app.job.JobScheduler;
import android.content.BroadcastReceiver;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.pm.PackageManager;
import android.content.pm.PermissionInfo;
import android.hardware.Camera;
import android.media.ExifInterface;
import android.media.MediaMetadataRetriever;
import android.net.Uri;
import android.os.AsyncTask;
import android.os.Bundle;
import android.provider.MediaStore;
import android.util.Log;
import android.view.SurfaceHolder;
import android.view.View;
import android.view.View.OnClickListener;
import android.widget.Button;
import android.widget.ImageButton;
import android.widget.TextView;
import androidx.core.content.FileProvider;
import android.Manifest;

import com.android.cts.verifier.camera.intents.CameraContentJobService;
import com.android.cts.verifier.PassFailButtons;
import com.android.cts.verifier.R;
import com.android.cts.verifier.TestResult;
import android.widget.Toast;

import static android.media.MediaMetadataRetriever.METADATA_KEY_HAS_VIDEO;
import static android.media.MediaMetadataRetriever.METADATA_KEY_LOCATION;

import java.io.File;
import java.io.FileInputStream;
import java.io.IOException;
import java.util.TreeSet;
import java.util.Date;
import java.text.SimpleDateFormat;

/**
 * Tests for manual verification of uri trigger and camera intents being fired.
 *
 * MediaStore.Images.Media.EXTERNAL_CONTENT_URI:
 * android.hardware.Camera.ACTION_NEW_PICTURE:
 *  These should fire when a new picture was captured by the camera app, and
 *  it has been added to the media store.
 * MediaStore.Video.Media.EXTERNAL_CONTENT_URI:
 * android.hardware.Camera.ACTION_NEW_VIDEO:
 *  These should fire when a new video has been captured by the camera app, and
 *  it has been added to the media store.
 *
 * The tests verify this both by asking the user to manually launch
 *  the camera activity, as well as by programatically launching the camera
 *  activity via MediaStore intents.
 *
 * Please ensure when replacing the default camera app on a device,
 *  that these intents are still firing as a lot of 3rd party applications
 *  (e.g. social network apps that upload a photo after you take a picture)
 *  rely on this functionality present and correctly working.
 */
public class CameraIntentsActivity extends PassFailButtons.Activity
implements OnClickListener, SurfaceHolder.Callback {

    private static final String TAG = "CameraIntents";
    private static final int STATE_OFF = 0;
    private static final int STATE_STARTED = 1;
    private static final int STATE_SUCCESSFUL = 2;
    private static final int STATE_FAILED = 3;

    private static final int STAGE_APP_PICTURE = 0;
    private static final int STAGE_APP_VIDEO = 1;
    private static final int STAGE_INTENT_PICTURE = 2;
    private static final int STAGE_INTENT_PICTURE_SECURE = 3;
    private static final int STAGE_INTENT_VIDEO = 4;
    private static final int NUM_STAGES = 5;
    private static final String STAGE_INDEX_EXTRA = "stageIndex";

    private static String[]  EXPECTED_INTENTS = new String[] {
        Camera.ACTION_NEW_PICTURE,
        Camera.ACTION_NEW_VIDEO,
        null,
        null,
        Camera.ACTION_NEW_VIDEO
    };

    private ImageButton mPassButton;
    private ImageButton mFailButton;
    private Button mStartTestButton;
    private Button mSettingsButton;
    private File mDebugFolder = null;
    private File mImageTarget = null;
    private File mVideoTarget = null;
    private int mState = STATE_OFF;
    // MediaStore.Images.Media.EXTERNAL_CONTENT_URI or
    // MediaStore.Video.Media.EXTERNAL_CONTENT_URI are successfully received.
    private boolean mUriSuccess = false;
    // android.hardware.Camera.ACTION_NEW_PICTURE or
    // android.hardware.Camera.ACTION_NEW_VIDEO are successfully received.
    private boolean mActionSuccess = false;
    private Object mLock = new Object();

    private BroadcastReceiver mReceiver;
    private IntentFilter mFilterPicture;
    private boolean mActivityResult = false;
    private boolean mDetectCheating = false;

    private StringBuilder mReportBuilder = new StringBuilder();
    private final TreeSet<String> mTestedCombinations = new TreeSet<String>();
    private final TreeSet<String> mUntestedCombinations = new TreeSet<String>();

    private CameraContentJobService.TestEnvironment mTestEnv;
    private static final int CAMERA_JOB_ID = CameraIntentsActivity.class.hashCode();
    private static final int JOB_TYPE_IMAGE = 0;
    private static final int JOB_TYPE_VIDEO = 1;

    private static int[] TEST_JOB_TYPES = new int[] {
        JOB_TYPE_IMAGE,
        JOB_TYPE_VIDEO,
        JOB_TYPE_IMAGE,
        JOB_TYPE_IMAGE,
        JOB_TYPE_VIDEO
    };

    private JobInfo makeJobInfo(int jobType) {
        JobInfo.Builder builder = new JobInfo.Builder(CAMERA_JOB_ID,
                new ComponentName(this, CameraContentJobService.class));
        // Look for specific changes to images in the provider.
        Uri uriToTrigger = null;
        switch (jobType) {
            case JOB_TYPE_IMAGE:
                uriToTrigger = MediaStore.Images.Media.EXTERNAL_CONTENT_URI;
                break;
            case JOB_TYPE_VIDEO:
                uriToTrigger = MediaStore.Video.Media.EXTERNAL_CONTENT_URI;
                break;
            default:
                Log.e(TAG, "Unknown jobType" + jobType);
                return null;
        }
        builder.addTriggerContentUri(new JobInfo.TriggerContentUri(
                uriToTrigger,
                JobInfo.TriggerContentUri.FLAG_NOTIFY_FOR_DESCENDANTS));
        // For testing purposes, react quickly.
        builder.setTriggerContentUpdateDelay(100);
        builder.setTriggerContentMaxDelay(100);
        return builder.build();
    }

    /* Callback from mReceiver#onReceive */
    public void onReceivedIntent(Intent intent) {
        Log.v(TAG, "Received intent " + intent.toString());
        synchronized(mLock) {
            if (mState == STATE_STARTED) {

                /* this can happen if..
                  the camera apps intent finishes,
                  user returns to cts verifier,
                  user leaves cts verifier and tries to fake receiver intents
                  */
                if (mDetectCheating) {
                    Log.w(TAG, "Cheating attempt suppressed");

                    mState = STATE_FAILED;
                }

                String expectedIntent = EXPECTED_INTENTS[getStageIndex()];
                if (expectedIntent != intent.getAction()) {
                    Log.e(TAG, "FAIL: Test # " + getStageIndex()
                        + " must not broadcast "
                        + intent.getAction()
                        + ", expected: "
                        + (expectedIntent != null ? expectedIntent : "no intent"));

                    mState = STATE_FAILED;
                }

                if (mState != STATE_FAILED) {
                    mActionSuccess = true;
                }
                updateSuccessState();
            }
        }
    }

    private void updateSuccessState() {
        if (mActionSuccess && mUriSuccess) {
            mState = STATE_SUCCESSFUL;
        }

        setPassButton(mState == STATE_SUCCESSFUL);
    }

    private void setPassButton(Boolean pass) {
        mPassButton.setEnabled(pass);
        mFailButton.setEnabled(!pass);
    }

    private int getStageIndex()
    {
        final int stageIndex = getIntent().getIntExtra(STAGE_INDEX_EXTRA, 0);
        return stageIndex;
    }

    private String getStageString(int stageIndex)
    {
        if (stageIndex == STAGE_APP_PICTURE) {
            return "Application Picture";
        }
        if (stageIndex == STAGE_APP_VIDEO) {
            return "Application Video";
        }
        if (stageIndex == STAGE_INTENT_PICTURE) {
            return "Intent Picture";
        }
        if (stageIndex == STAGE_INTENT_PICTURE_SECURE) {
            return "Intent Picture Secure";
        }
        if (stageIndex == STAGE_INTENT_VIDEO) {
            return "Intent Video";
        }

        return "Unknown!!!";
    }

    private String getStageIntentString(int stageIndex)
    {
        if (stageIndex == STAGE_APP_PICTURE) {
            return android.hardware.Camera.ACTION_NEW_PICTURE;
        }
        if (stageIndex == STAGE_APP_VIDEO) {
            return android.hardware.Camera.ACTION_NEW_VIDEO;
        }
        if (stageIndex == STAGE_INTENT_PICTURE) {
            return android.hardware.Camera.ACTION_NEW_PICTURE;
        }
        if (stageIndex == STAGE_INTENT_PICTURE_SECURE) {
            return android.hardware.Camera.ACTION_NEW_PICTURE + " (Secure)";
        }
        if (stageIndex == STAGE_INTENT_VIDEO) {
            return android.hardware.Camera.ACTION_NEW_VIDEO;
        }

        return "Unknown Intent!!!";
    }

    private String getStageInstructionLabel(int stageIndex)
    {
        if (stageIndex == STAGE_APP_PICTURE) {
            return getString(R.string.ci_instruction_text_app_picture_label);
        }
        if (stageIndex == STAGE_APP_VIDEO) {
            return getString(R.string.ci_instruction_text_app_video_label);
        }
        if (stageIndex == STAGE_INTENT_PICTURE) {
            return getString(R.string.ci_instruction_text_intent_picture_label);
        }
        if (stageIndex == STAGE_INTENT_PICTURE_SECURE) {
            return getString(R.string.ci_instruction_text_intent_picture_secure_label);
        }
        if (stageIndex == STAGE_INTENT_VIDEO) {
            return getString(R.string.ci_instruction_text_intent_video_label);
        }

        return "Unknown Instruction Label!!!";
    }

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        setContentView(R.layout.ci_main);
        setPassFailButtonClickListeners();
        setInfoResources(R.string.camera_intents, R.string.ci_info, -1);

        mPassButton         = (ImageButton) findViewById(R.id.pass_button);
        mFailButton         = (ImageButton) findViewById(R.id.fail_button);
        mStartTestButton  = (Button) findViewById(R.id.start_test_button);
        mSettingsButton  = (Button) findViewById(R.id.settings_button);
        mStartTestButton.setOnClickListener(this);
        mSettingsButton.setOnClickListener(this);

        // This activity is reused multiple times
        // to test each camera/intents combination
        final int stageIndex = getIntent().getIntExtra(STAGE_INDEX_EXTRA, 0);

        // Hitting the pass button goes to the next test activity.
        // Only the last one uses the PassFailButtons click callback function,
        // which gracefully terminates the activity.
        if (stageIndex + 1 < NUM_STAGES) {
            setPassButtonGoesToNextStage(stageIndex);
        }
        resetButtons();

        // Set initial values

        TextView intentsLabel =
                (TextView) findViewById(R.id.intents_text);
        intentsLabel.setText(
                getString(R.string.ci_intents_label)
                + " "
                + Integer.toString(getStageIndex()+1)
                + " of "
                + Integer.toString(NUM_STAGES)
                + ": "
                + getStageIntentString(getStageIndex())
                );

        TextView instructionLabel =
                (TextView) findViewById(R.id.instruction_text);
        instructionLabel.setText(R.string.ci_instruction_text_photo_label);

        /* Display the instructions to launch camera app and take a photo */
        TextView cameraExtraLabel =
                (TextView) findViewById(R.id.instruction_extra_text);
        cameraExtraLabel.setText(getStageInstructionLabel(getStageIndex()));

        mStartTestButton.setEnabled(true);
        mSettingsButton.setEnabled(true);

        mReceiver = new BroadcastReceiver() {
            @Override
            public void onReceive(Context context, Intent intent) {
                onReceivedIntent(intent);
            }
        };

        mFilterPicture = new IntentFilter();
        mFilterPicture.addAction(Camera.ACTION_NEW_PICTURE);
        mFilterPicture.addAction(Camera.ACTION_NEW_VIDEO);

        try {
            mFilterPicture.addDataType("video/*");
            mFilterPicture.addDataType("image/*");
        }
        catch(IntentFilter.MalformedMimeTypeException e) {
            Log.e(TAG, "Caught exceptione e " + e.toString());
        }
        registerReceiver(mReceiver, mFilterPicture);
    }

    @Override
    public void onDestroy() {
        super.onDestroy();
        Log.v(TAG, "onDestroy");
        this.unregisterReceiver(mReceiver);
    }

    @Override
    public void onResume() {
        super.onResume();
        mFailButton.setEnabled(false);
        /**
         * If location is not enabled, fail buttons should be disabled, since they take us back to
         * the original CTS Verifier activity where other tests might depend on these
         * If we're in STAGE_INTENT_VIDEO even the pass button should be disabled till location
         * access is turned back on for CTS Verifier.
         */
        Boolean locationEnabled = (checkSelfPermission(Manifest.permission.ACCESS_FINE_LOCATION) ==
                PackageManager.PERMISSION_GRANTED);

        if (getStageIndex() == STAGE_INTENT_VIDEO) {
                /**
                 * Don't enable the pass /fail button till the user grants CTS verifier location
                 * access again.
                 */
                if (mActionSuccess) {
                    mState = STATE_SUCCESSFUL;
                }
                mPassButton.setEnabled(false);
                if (locationEnabled) {
                    if (mState == STATE_SUCCESSFUL) {
                        mPassButton.setEnabled(true);
                    } else {
                        mFailButton.setEnabled(true);
                    }
                } else if (mState != STATE_OFF) {
                    Toast.makeText(this, R.string.ci_location_permissions_error,
                            Toast.LENGTH_SHORT).show();
                }
        } else {
            if (locationEnabled) {
                mFailButton.setEnabled(true);
            } else {
                Toast.makeText(this, R.string.ci_location_permissions_fail_error,
                    Toast.LENGTH_SHORT).show();
            }
        }
    }

    @Override
    public void onPause() {
        super.onPause();
        /*
        When testing INTENT_PICTURE, INTENT_VIDEO,
        do not allow user to cheat by going to camera app and re-firing
        the intents by taking a photo/video
        */
        if (getStageIndex() == STAGE_INTENT_PICTURE ||
            getStageIndex() == STAGE_INTENT_PICTURE_SECURE ||
            getStageIndex() == STAGE_INTENT_VIDEO) {

            if (mActivityResult && mState == STATE_STARTED) {
                mDetectCheating = true;
                Log.w(TAG, "Potential cheating detected");
            }
        }

    }

    @Override
    protected void onActivityResult(
        int requestCode, int resultCode, Intent data) {
        int stageIndex = getStageIndex();
        if (requestCode == 1337 + stageIndex) {
            Log.v(TAG, "Activity we launched was finished");
            mActivityResult = true;
            synchronized(mLock) {
                if (mState != STATE_FAILED) {
                    switch (stageIndex) {
                        case STAGE_INTENT_PICTURE:
                        case STAGE_INTENT_PICTURE_SECURE:
                            handleIntentPictureResult();
                            // No broadcast should be received. Proceed to update test result
                            updateSuccessState();
                            break;
                        case STAGE_INTENT_VIDEO:
                            handleIntentVideoResult();
                            break;
                        default:
                            return;
                    }
                }
            }
        }
    }

    private void handleIntentPictureResult() {
        if (mImageTarget == null) {
            Log.d(TAG, "Image target was not set");
            return;
        }
        try {
            if (!mImageTarget.exists() || mImageTarget.length() == 0) {
                Log.d(TAG, "Image target does not exist or is empty");
                mState = STATE_FAILED;
                return;
            }

            try {
                final ExifInterface exif = new ExifInterface(new FileInputStream(mImageTarget));
                if (!checkExifAttribute(exif, ExifInterface.TAG_MAKE)
                    || !checkExifAttribute(exif, ExifInterface.TAG_MODEL)
                    || !checkExifAttribute(exif, ExifInterface.TAG_DATETIME)) {
                    Log.d(TAG, "The required tag does not appear in the exif");
                    mState = STATE_FAILED;
                    return;
                }

                float[] latLong = new float[2];
                if (exif.getLatLong(latLong)) {
                    Log.d(TAG, "Should not contain location information");
                    mState = STATE_FAILED;
                    return;
                }
                mActionSuccess = true;
            } catch (IOException ex) {
                Log.e(TAG, "Failed to verify Exif", ex);
                mState = STATE_FAILED;
                return;
            }
        } finally {
            mImageTarget.delete();
        }
    }

    private void handleIntentVideoResult() {
        if (mVideoTarget == null) {
            Log.d(TAG, "Video target was not set");
            return;
        }
        /**
         * Check that there is no location data in video.
         */
        MediaMetadataRetriever mediaRetriever = new MediaMetadataRetriever();
        mediaRetriever.setDataSource(mVideoTarget.toString());
        if (mediaRetriever.extractMetadata(METADATA_KEY_HAS_VIDEO) == null ||
            mediaRetriever.extractMetadata(METADATA_KEY_LOCATION) != null) {
            mState = STATE_FAILED;
        } else {
            mVideoTarget.delete();
        }
        Log.d(TAG, "METADATA_KEY_HAS_VIDEO: " +
              mediaRetriever.extractMetadata(METADATA_KEY_HAS_VIDEO) +
              " METADATA_KEY_LOCATION: " +
              mediaRetriever.extractMetadata(METADATA_KEY_LOCATION));
        mediaRetriever.release();
        /* successful, unless we get the URI trigger back at some point later on. */
        mActionSuccess = true;
    }

    private boolean checkExifAttribute(ExifInterface exif, String tag) {
        final String res = exif.getAttribute(tag);
        return res != null && res.length() > 0;
    }

    @Override
    public String getTestDetails() {
        return mReportBuilder.toString();
    }

    private class WaitForTriggerTask extends AsyncTask<Void, Void, Boolean> {
        protected Boolean doInBackground(Void... param) {
            try {
                boolean executed = mTestEnv.awaitExecution();
                synchronized(mLock) {
                    // Check latest test param
                    if (executed && mState == STATE_STARTED) {

                        // this can happen if..
                        //  the camera apps intent finishes,
                        //  user returns to cts verifier,
                        //  user leaves cts verifier and tries to fake receiver intents
                        if (mDetectCheating) {
                            Log.w(TAG, "Cheating attempt suppressed");
                            mState = STATE_FAILED;
                        }

                        // For STAGE_INTENT_PICTURE test, if EXTRA_OUTPUT is not assigned in intent,
                        // file should NOT be saved so triggering this is a test failure.
                        if (getStageIndex() == STAGE_INTENT_PICTURE ||
                            getStageIndex() == STAGE_INTENT_PICTURE_SECURE) {
                            Log.e(TAG, "FAIL: STAGE_INTENT_PICTURE or STAGE_INTENT_PICTURE_SECURE test should not create file");
                            mState = STATE_FAILED;
                        }

                        if (mState != STATE_FAILED) {
                            return true;
                        } else {
                            return false;
                        }
                    }
                }
            } catch (InterruptedException e) {
                e.printStackTrace();
            }

            if (getStageIndex() == STAGE_INTENT_PICTURE ||
                getStageIndex() == STAGE_INTENT_PICTURE_SECURE) {
                // STAGE_INTENT_PICTURE or STAGE_INTENT_PICTURE_SECURE should timeout
                return true;
            } else {
                Log.e(TAG, "FAIL: timeout waiting for URI trigger");
                return false;
            }
        }

        protected void onPostExecute(Boolean pass) {
            synchronized(mLock) {
                mUriSuccess = pass;
                updateSuccessState();
            }
        }
    }

    @Override
    public void onClick(View view) {
        Log.v(TAG, "Click detected");

        final int stageIndex = getStageIndex();
        if (view == mSettingsButton) {
            Log.v(TAG, "Opening up Settings app");
            startActivity(new Intent(android.provider.Settings.ACTION_LOCATION_SOURCE_SETTINGS));
        }

        if (view == mStartTestButton) {
            Log.v(TAG, "Starting testing... ");

            mState = STATE_STARTED;
            mUriSuccess = false;
            mActionSuccess = false;

            // Skip URI check for stages that should not receive intent
            if (EXPECTED_INTENTS[stageIndex] == null) {
                mUriSuccess = true;
            }

            JobScheduler jobScheduler = (JobScheduler) getSystemService(
                    Context.JOB_SCHEDULER_SERVICE);
            jobScheduler.cancelAll();

            mTestEnv = CameraContentJobService.TestEnvironment.getTestEnvironment();

            mTestEnv.setUp();

            /**
             * Video intents do not need to wait on a ContentProvider broadcast since we're starting
             * the intent activity with EXTRA_OUTPUT set
             */
            if (stageIndex != STAGE_INTENT_VIDEO &&
                stageIndex != STAGE_INTENT_PICTURE &&
                stageIndex != STAGE_INTENT_PICTURE_SECURE) {
                JobInfo job = makeJobInfo(TEST_JOB_TYPES[stageIndex]);
                jobScheduler.schedule(job);
                new WaitForTriggerTask().execute();
            }

            /* we can allow user to fail immediately if location is on, otherwise they must
             * enable location */
            if (checkSelfPermission(Manifest.permission.ACCESS_FINE_LOCATION) ==
                PackageManager.PERMISSION_GRANTED) {
                mFailButton.setEnabled(true);
            }

            /* trigger an ACTION_IMAGE_CAPTURE intent
                which will run the camera app itself */
            String intentStr = null;
            Intent cameraIntent = null;
            if (stageIndex == STAGE_INTENT_PICTURE) {
                intentStr = android.provider.MediaStore.ACTION_IMAGE_CAPTURE;
            }
            else if (stageIndex == STAGE_INTENT_PICTURE_SECURE) {
                intentStr = android.provider.MediaStore.ACTION_IMAGE_CAPTURE_SECURE;
            }
            else if (stageIndex == STAGE_INTENT_VIDEO) {
                intentStr = android.provider.MediaStore.ACTION_VIDEO_CAPTURE;
            }

            if (intentStr != null) {
                cameraIntent = new Intent(intentStr);
                mDebugFolder = new File(this.getFilesDir(), "debug");
                mDebugFolder.mkdirs();
                if (!mDebugFolder.exists()) {
                    Toast.makeText(this, R.string.ci_directory_creation_error,
                            Toast.LENGTH_SHORT).show();
                    Log.v(TAG, "Could not create directory");
                    return;
                }

                File targetFile;
                String timeStamp = new SimpleDateFormat("yyyyMMdd_HHmmss").format(new Date());
                switch (stageIndex) {
                    case STAGE_INTENT_PICTURE:
                    case STAGE_INTENT_PICTURE_SECURE:
                        mImageTarget = new File(mDebugFolder, timeStamp + "capture.jpg");
                        targetFile = mImageTarget;
                        break;
                    case STAGE_INTENT_VIDEO:
                        mVideoTarget = new File(mDebugFolder, timeStamp  + "video.mp4");
                        targetFile = mVideoTarget;
                        break;
                    default:
                        Log.wtf(TAG, "Unexpected stage index to send intent with extras");
                        return;
                }
                cameraIntent.putExtra(MediaStore.EXTRA_OUTPUT, FileProvider.getUriForFile(this,
                              "com.android.cts.verifier.managedprovisioning.fileprovider",
                              targetFile));
                startActivityForResult(cameraIntent, 1337 + getStageIndex());
            }

            mStartTestButton.setEnabled(false);
        }

        if(view == mPassButton || view == mFailButton) {
            // Stop any running wait
            mTestEnv.cancelWait();

            for (int counter = 0; counter < NUM_STAGES; counter++) {
                String combination = getStageString(counter) + "\n";

                if(counter < stageIndex) {
                    // test already passed, or else wouldn't have made
                    // it to current stageIndex
                    mTestedCombinations.add(combination);
                }

                if(counter == stageIndex) {
                    // current test configuration
                    if(view == mPassButton) {
                        mTestedCombinations.add(combination);
                    }
                    else if(view == mFailButton) {
                        mUntestedCombinations.add(combination);
                    }
                }

                if(counter > stageIndex) {
                    // test not passed yet, since haven't made it to
                    // stageIndex
                    mUntestedCombinations.add(combination);
                }

                counter++;
            }

            mReportBuilder = new StringBuilder();
            mReportBuilder.append("Passed combinations:\n");
            for (String combination : mTestedCombinations) {
                mReportBuilder.append(combination);
            }
            mReportBuilder.append("Failed/untested combinations:\n");
            for (String combination : mUntestedCombinations) {
                mReportBuilder.append(combination);
            }

            if(view == mPassButton) {
                TestResult.setPassedResult(this, "CameraIntentsActivity",
                        getTestDetails());
            }
            if(view == mFailButton) {
                TestResult.setFailedResult(this, "CameraIntentsActivity",
                        getTestDetails());
            }

            // restart activity to test next intents
            Intent intent = new Intent(CameraIntentsActivity.this,
                    CameraIntentsActivity.class);
            intent.setFlags(Intent.FLAG_ACTIVITY_CLEAR_TOP
                    | Intent.FLAG_ACTIVITY_FORWARD_RESULT);
            intent.putExtra(STAGE_INDEX_EXTRA, stageIndex + 1);
            startActivity(intent);
        }
    }

    private void resetButtons() {
        enablePassFailButtons(false);
    }

    private void enablePassFailButtons(boolean enable) {
        mPassButton.setEnabled(enable);
        mFailButton.setEnabled(enable);
    }

    @Override
    public void surfaceChanged(SurfaceHolder holder, int format, int width,
            int height) {
    }

    @Override
    public void surfaceCreated(SurfaceHolder holder) {
        // Auto-generated method stub
    }

    @Override
    public void surfaceDestroyed(SurfaceHolder holder) {
        // Auto-generated method stub
    }

    private void setPassButtonGoesToNextStage(final int stageIndex) {
        findViewById(R.id.pass_button).setOnClickListener(this);
    }

}
