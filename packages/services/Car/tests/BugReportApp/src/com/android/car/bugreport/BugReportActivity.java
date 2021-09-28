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
package com.android.car.bugreport;

import static com.android.car.bugreport.BugReportService.MAX_PROGRESS_VALUE;

import android.Manifest;
import android.app.Activity;
import android.car.Car;
import android.car.CarNotConnectedException;
import android.car.drivingstate.CarDrivingStateEvent;
import android.car.drivingstate.CarDrivingStateManager;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.ServiceConnection;
import android.content.pm.PackageManager;
import android.media.AudioAttributes;
import android.media.AudioFocusRequest;
import android.media.AudioManager;
import android.media.MediaRecorder;
import android.os.AsyncTask;
import android.os.Bundle;
import android.os.Handler;
import android.os.IBinder;
import android.os.Looper;
import android.os.UserManager;
import android.util.Log;
import android.view.View;
import android.view.Window;
import android.widget.Button;
import android.widget.ProgressBar;
import android.widget.TextView;
import android.widget.Toast;

import com.google.common.base.Preconditions;
import com.google.common.io.ByteStreams;

import java.io.File;
import java.io.FileInputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.util.Arrays;
import java.util.Date;
import java.util.Random;

/**
 * Activity that shows two types of dialogs: starting a new bug report and current status of already
 * in progress bug report.
 *
 * <p>If there is no in-progress bug report, it starts recording voice message. After clicking
 * submit button it initiates {@link BugReportService}.
 *
 * <p>If bug report is in-progress, it shows a progress bar.
 */
public class BugReportActivity extends Activity {
    private static final String TAG = BugReportActivity.class.getSimpleName();

    /** Starts silent (no audio message recording) bugreporting. */
    private static final String ACTION_START_SILENT =
            "com.android.car.bugreport.action.START_SILENT";

    /** This is deprecated action. Please start SILENT bugreport using {@link BugReportService}. */
    private static final String ACTION_ADD_AUDIO =
            "com.android.car.bugreport.action.ADD_AUDIO";

    private static final int VOICE_MESSAGE_MAX_DURATION_MILLIS = 60 * 1000;
    private static final int AUDIO_PERMISSIONS_REQUEST_ID = 1;

    private static final String EXTRA_BUGREPORT_ID = "bugreport-id";

    /**
     * NOTE: mRecorder related messages are cleared when the activity finishes.
     */
    private final Handler mHandler = new Handler(Looper.getMainLooper());

    /** Look up string length, e.g. [ABCDEF]. */
    static final int LOOKUP_STRING_LENGTH = 6;

    private TextView mInProgressTitleText;
    private ProgressBar mProgressBar;
    private TextView mProgressText;
    private TextView mAddAudioText;
    private VoiceRecordingView mVoiceRecordingView;
    private View mVoiceRecordingFinishedView;
    private View mSubmitBugReportLayout;
    private View mInProgressLayout;
    private View mShowBugReportsButton;
    private Button mSubmitButton;

    private boolean mBound;
    /** Audio message recording process started (including waiting for permission). */
    private boolean mAudioRecordingStarted;
    /** Audio recording using MIC is running (permission given). */
    private boolean mAudioRecordingIsRunning;
    private boolean mIsNewBugReport;
    private boolean mIsOnActivityStartedWithBugReportServiceBoundCalled;
    private boolean mIsSubmitButtonClicked;
    private BugReportService mService;
    private MediaRecorder mRecorder;
    private MetaBugReport mMetaBugReport;
    private File mAudioFile;
    private Car mCar;
    private CarDrivingStateManager mDrivingStateManager;
    private AudioManager mAudioManager;
    private AudioFocusRequest mLastAudioFocusRequest;
    private Config mConfig;

    /** Defines callbacks for service binding, passed to bindService() */
    private ServiceConnection mConnection = new ServiceConnection() {
        @Override
        public void onServiceConnected(ComponentName className, IBinder service) {
            BugReportService.ServiceBinder binder = (BugReportService.ServiceBinder) service;
            mService = binder.getService();
            mBound = true;
            onActivityStartedWithBugReportServiceBound();
        }

        @Override
        public void onServiceDisconnected(ComponentName arg0) {
            // called when service connection breaks unexpectedly.
            mBound = false;
        }
    };

    /**
     * Builds an intent that starts {@link BugReportActivity} to add audio message to the existing
     * bug report.
     */
    static Intent buildAddAudioIntent(Context context, MetaBugReport bug) {
        Intent addAudioIntent = new Intent(context, BugReportActivity.class);
        addAudioIntent.setAction(ACTION_ADD_AUDIO);
        addAudioIntent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK | Intent.FLAG_ACTIVITY_CLEAR_TASK);
        addAudioIntent.putExtra(EXTRA_BUGREPORT_ID, bug.getId());
        return addAudioIntent;
    }

    @Override
    public void onCreate(Bundle savedInstanceState) {
        Preconditions.checkState(Config.isBugReportEnabled(), "BugReport is disabled.");

        super.onCreate(savedInstanceState);
        requestWindowFeature(Window.FEATURE_NO_TITLE);

        // Bind to BugReportService.
        Intent intent = new Intent(this, BugReportService.class);
        bindService(intent, mConnection, BIND_AUTO_CREATE);
    }

    @Override
    protected void onStart() {
        super.onStart();

        if (mBound) {
            onActivityStartedWithBugReportServiceBound();
        }
    }

    @Override
    protected void onStop() {
        super.onStop();
        // If SUBMIT button is clicked, cancelling audio has been taken care of.
        if (!mIsSubmitButtonClicked) {
            cancelAudioMessageRecording();
        }
        if (mBound) {
            mService.removeBugReportProgressListener();
        }
        // Reset variables for the next onStart().
        mAudioRecordingStarted = false;
        mAudioRecordingIsRunning = false;
        mIsSubmitButtonClicked = false;
        mIsOnActivityStartedWithBugReportServiceBoundCalled = false;
        mMetaBugReport = null;
        mAudioFile = null;
    }

    @Override
    public void onDestroy() {
        if (mRecorder != null) {
            mHandler.removeCallbacksAndMessages(/* token= */ mRecorder);
        }
        if (mBound) {
            unbindService(mConnection);
            mBound = false;
        }
        if (mCar != null && mCar.isConnected()) {
            mCar.disconnect();
            mCar = null;
        }
        super.onDestroy();
    }

    private void onCarDrivingStateChanged(CarDrivingStateEvent event) {
        if (mShowBugReportsButton == null) {
            Log.w(TAG, "Cannot handle driving state change, UI is not ready");
            return;
        }
        // When adding audio message to the existing bugreport, do not show "Show Bug Reports"
        // button, users either should explicitly Submit or Cancel.
        if (mAudioRecordingStarted && !mIsNewBugReport) {
            mShowBugReportsButton.setVisibility(View.GONE);
            return;
        }
        if (event.eventValue == CarDrivingStateEvent.DRIVING_STATE_PARKED
                || event.eventValue == CarDrivingStateEvent.DRIVING_STATE_IDLING) {
            mShowBugReportsButton.setVisibility(View.VISIBLE);
        } else {
            mShowBugReportsButton.setVisibility(View.GONE);
        }
    }

    private void onProgressChanged(float progress) {
        int progressValue = (int) progress;
        mProgressBar.setProgress(progressValue);
        mProgressText.setText(progressValue + "%");
        if (progressValue == MAX_PROGRESS_VALUE) {
            mInProgressTitleText.setText(R.string.bugreport_dialog_in_progress_title_finished);
        }
    }

    private void prepareUi() {
        if (mSubmitBugReportLayout != null) {
            return;
        }
        setContentView(R.layout.bug_report_activity);

        // Connect to the services here, because they are used only when showing the dialog.
        // We need to minimize system state change when performing SILENT bug report.
        mConfig = new Config();
        mConfig.start();
        mCar = Car.createCar(this, /* handler= */ null,
                Car.CAR_WAIT_TIMEOUT_DO_NOT_WAIT, this::onCarLifecycleChanged);

        mInProgressTitleText = findViewById(R.id.in_progress_title_text);
        mProgressBar = findViewById(R.id.progress_bar);
        mProgressText = findViewById(R.id.progress_text);
        mAddAudioText = findViewById(R.id.bug_report_add_audio_to_existing);
        mVoiceRecordingView = findViewById(R.id.voice_recording_view);
        mVoiceRecordingFinishedView = findViewById(R.id.voice_recording_finished_text_view);
        mSubmitBugReportLayout = findViewById(R.id.submit_bug_report_layout);
        mInProgressLayout = findViewById(R.id.in_progress_layout);
        mShowBugReportsButton = findViewById(R.id.button_show_bugreports);
        mSubmitButton = findViewById(R.id.button_submit);

        mShowBugReportsButton.setOnClickListener(this::buttonShowBugReportsClick);
        mSubmitButton.setOnClickListener(this::buttonSubmitClick);
        findViewById(R.id.button_cancel).setOnClickListener(this::buttonCancelClick);
        findViewById(R.id.button_close).setOnClickListener(this::buttonCancelClick);

        if (mIsNewBugReport) {
            mSubmitButton.setText(R.string.bugreport_dialog_submit);
        } else {
            mSubmitButton.setText(mConfig.getAutoUpload()
                    ? R.string.bugreport_dialog_upload : R.string.bugreport_dialog_save);
        }
    }

    private void onCarLifecycleChanged(Car car, boolean ready) {
        if (!ready) {
            mDrivingStateManager = null;
            mCar = null;
            Log.d(TAG, "Car service is not ready, ignoring");
            // If car service is not ready for this activity, just ignore it - as it's only
            // used to control UX restrictions.
            return;
        }
        try {
            mDrivingStateManager = (CarDrivingStateManager) car.getCarManager(
                    Car.CAR_DRIVING_STATE_SERVICE);
            mDrivingStateManager.registerListener(
                    BugReportActivity.this::onCarDrivingStateChanged);
            // Call onCarDrivingStateChanged(), because it's not called when Car is connected.
            onCarDrivingStateChanged(mDrivingStateManager.getCurrentCarDrivingState());
        } catch (CarNotConnectedException e) {
            Log.w(TAG, "Failed to get CarDrivingStateManager", e);
        }
    }

    private void showInProgressUi() {
        mSubmitBugReportLayout.setVisibility(View.GONE);
        mInProgressLayout.setVisibility(View.VISIBLE);
        mInProgressTitleText.setText(R.string.bugreport_dialog_in_progress_title);
        onProgressChanged(mService.getBugReportProgress());
    }

    private void showSubmitBugReportUi(boolean isRecording) {
        mSubmitBugReportLayout.setVisibility(View.VISIBLE);
        mInProgressLayout.setVisibility(View.GONE);
        if (isRecording) {
            mVoiceRecordingFinishedView.setVisibility(View.GONE);
            mVoiceRecordingView.setVisibility(View.VISIBLE);
        } else {
            mVoiceRecordingFinishedView.setVisibility(View.VISIBLE);
            mVoiceRecordingView.setVisibility(View.GONE);
        }
        // NOTE: mShowBugReportsButton visibility is also handled in #onCarDrivingStateChanged().
        mShowBugReportsButton.setVisibility(View.GONE);
        if (mDrivingStateManager != null) {
            try {
                onCarDrivingStateChanged(mDrivingStateManager.getCurrentCarDrivingState());
            } catch (CarNotConnectedException e) {
                Log.e(TAG, "Failed to get current driving state.", e);
            }
        }
    }

    /**
     * Initializes MetaBugReport in a local DB and starts audio recording.
     *
     * <p>This method expected to be called when the activity is started and bound to the service.
     */
    private void onActivityStartedWithBugReportServiceBound() {
        if (mIsOnActivityStartedWithBugReportServiceBoundCalled) {
            return;
        }
        mIsOnActivityStartedWithBugReportServiceBoundCalled = true;

        if (mService.isCollectingBugReport()) {
            Log.i(TAG, "Bug report is already being collected.");
            mService.setBugReportProgressListener(this::onProgressChanged);
            prepareUi();
            showInProgressUi();
            return;
        }

        if (ACTION_START_SILENT.equals(getIntent().getAction())) {
            Log.i(TAG, "Starting a silent bugreport.");
            MetaBugReport bugReport = createBugReport(this, MetaBugReport.TYPE_SILENT);
            startBugReportCollection(bugReport);
            finish();
            return;
        }

        // Close the notification shade and other dialogs when showing BugReportActivity dialog.
        sendBroadcast(new Intent(Intent.ACTION_CLOSE_SYSTEM_DIALOGS));

        if (ACTION_ADD_AUDIO.equals(getIntent().getAction())) {
            addAudioToExistingBugReport(
                    getIntent().getIntExtra(EXTRA_BUGREPORT_ID, /* defaultValue= */ -1));
            return;
        }

        Log.i(TAG, "Starting an interactive bugreport.");
        createNewBugReportWithAudioMessage();
    }

    private void addAudioToExistingBugReport(int bugreportId) {
        MetaBugReport bug = BugStorageUtils.findBugReport(this, bugreportId).orElseThrow(
                () -> new RuntimeException("Failed to find bug report with id " + bugreportId));
        Log.i(TAG, "Adding audio to the existing bugreport " + bug.getTimestamp());
        if (bug.getStatus() != Status.STATUS_AUDIO_PENDING.getValue()) {
            Log.e(TAG, "Failed to add audio, bad status, expected "
                    + Status.STATUS_AUDIO_PENDING.getValue() + ", got " + bug.getStatus());
            finish();
        }
        File audioFile;
        try {
            audioFile = File.createTempFile("audio", "mp3", getCacheDir());
        } catch (IOException e) {
            throw new RuntimeException("failed to create temp audio file");
        }
        startAudioMessageRecording(/* isNewBugReport= */ false, bug, audioFile);
    }

    private void createNewBugReportWithAudioMessage() {
        MetaBugReport bug = createBugReport(this, MetaBugReport.TYPE_INTERACTIVE);
        startAudioMessageRecording(
                /* isNewBugReport= */ true,
                bug,
                FileUtils.getFileWithSuffix(this, bug.getTimestamp(), "-message.3gp"));
    }

    /** Shows a dialog UI and starts recording audio message. */
    private void startAudioMessageRecording(
            boolean isNewBugReport, MetaBugReport bug, File audioFile) {
        if (mAudioRecordingStarted) {
            Log.i(TAG, "Audio message recording is already started.");
            return;
        }
        mAudioRecordingStarted = true;
        mAudioManager = getSystemService(AudioManager.class);
        mIsNewBugReport = isNewBugReport;
        mMetaBugReport = bug;
        mAudioFile = audioFile;
        prepareUi();
        showSubmitBugReportUi(/* isRecording= */ true);
        if (isNewBugReport) {
            mAddAudioText.setVisibility(View.GONE);
        } else {
            mAddAudioText.setVisibility(View.VISIBLE);
            mAddAudioText.setText(String.format(
                    getString(R.string.bugreport_dialog_add_audio_to_existing),
                    mMetaBugReport.getTimestamp()));
        }

        if (!hasRecordPermissions()) {
            requestRecordPermissions();
        } else {
            startRecordingWithPermission();
        }
    }

    /**
     * Cancels bugreporting by stopping audio recording and deleting temp files.
     */
    private void cancelAudioMessageRecording() {
        // If audio recording is not running, most likely there were permission issues,
        // so leave the bugreport as is without cancelling it.
        if (!mAudioRecordingIsRunning) {
            Log.w(TAG, "Cannot cancel, audio recording is not running.");
            return;
        }
        stopAudioRecording();
        if (mIsNewBugReport) {
            // The app creates a temp dir only for new INTERACTIVE bugreports.
            File tempDir = FileUtils.getTempDir(this, mMetaBugReport.getTimestamp());
            new DeleteFilesAndDirectoriesAsyncTask().execute(tempDir);
        } else {
            BugStorageUtils.deleteBugReportFiles(this, mMetaBugReport.getId());
            new DeleteFilesAndDirectoriesAsyncTask().execute(mAudioFile);
        }
        BugStorageUtils.setBugReportStatus(
                this, mMetaBugReport, Status.STATUS_USER_CANCELLED, "");
        Log.i(TAG, "Bug report " + mMetaBugReport.getTimestamp() + " is cancelled");
        mAudioRecordingStarted = false;
        mAudioRecordingIsRunning = false;
    }

    private void buttonCancelClick(View view) {
        finish();
    }

    private void buttonSubmitClick(View view) {
        stopAudioRecording();
        mIsSubmitButtonClicked = true;
        if (mIsNewBugReport) {
            Log.i(TAG, "Starting bugreport service.");
            startBugReportCollection(mMetaBugReport);
        } else {
            Log.i(TAG, "Adding audio file to the bugreport " + mMetaBugReport.getTimestamp());
            new AddAudioToBugReportAsyncTask(this, mConfig, mMetaBugReport, mAudioFile).execute();
        }
        setResult(Activity.RESULT_OK);
        finish();
    }

    /** Starts the {@link BugReportService} to collect bug report. */
    private void startBugReportCollection(MetaBugReport bug) {
        Bundle bundle = new Bundle();
        bundle.putParcelable(BugReportService.EXTRA_META_BUG_REPORT, bug);
        Intent intent = new Intent(this, BugReportService.class);
        intent.putExtras(bundle);
        startForegroundService(intent);
    }

    /**
     * Starts {@link BugReportInfoActivity} and finishes current activity, so it won't be running
     * in the background and closing {@link BugReportInfoActivity} will not open the current
     * activity again.
     */
    private void buttonShowBugReportsClick(View view) {
        // First cancel the audio recording, then delete the bug report from database.
        cancelAudioMessageRecording();
        // Delete the bugreport from database, otherwise pressing "Show Bugreports" button will
        // create unnecessary cancelled bugreports.
        if (mMetaBugReport != null) {
            BugStorageUtils.completeDeleteBugReport(this, mMetaBugReport.getId());
        }
        Intent intent = new Intent(this, BugReportInfoActivity.class);
        startActivity(intent);
        finish();
    }

    private void requestRecordPermissions() {
        requestPermissions(
                new String[]{Manifest.permission.RECORD_AUDIO}, AUDIO_PERMISSIONS_REQUEST_ID);
    }

    private boolean hasRecordPermissions() {
        return checkSelfPermission(Manifest.permission.RECORD_AUDIO)
                == PackageManager.PERMISSION_GRANTED;
    }

    @Override
    public void onRequestPermissionsResult(
            int requestCode, String[] permissions, int[] grantResults) {
        if (requestCode != AUDIO_PERMISSIONS_REQUEST_ID) {
            return;
        }
        for (int i = 0; i < grantResults.length; i++) {
            if (Manifest.permission.RECORD_AUDIO.equals(permissions[i])
                    && grantResults[i] == PackageManager.PERMISSION_GRANTED) {
                // Start recording from UI thread, otherwise when MediaRecord#start() fails,
                // stack trace gets confusing.
                mHandler.post(this::startRecordingWithPermission);
                return;
            }
        }
        handleNoPermission(permissions);
    }

    private void handleNoPermission(String[] permissions) {
        String text = this.getText(R.string.toast_permissions_denied) + " : "
                + Arrays.toString(permissions);
        Log.w(TAG, text);
        Toast.makeText(this, text, Toast.LENGTH_LONG).show();
        if (mMetaBugReport == null) {
            finish();
            return;
        }
        if (mIsNewBugReport) {
            BugStorageUtils.setBugReportStatus(this, mMetaBugReport,
                    Status.STATUS_USER_CANCELLED, text);
        } else {
            BugStorageUtils.setBugReportStatus(this, mMetaBugReport,
                    Status.STATUS_AUDIO_PENDING, text);
        }
        finish();
    }

    private void startRecordingWithPermission() {
        Log.i(TAG, "Started voice recording, and saving audio to " + mAudioFile);

        mLastAudioFocusRequest = new AudioFocusRequest.Builder(
                        AudioManager.AUDIOFOCUS_GAIN_TRANSIENT_EXCLUSIVE)
                .setOnAudioFocusChangeListener(focusChange ->
                        Log.d(TAG, "AudioManager focus change " + focusChange))
                .setAudioAttributes(new AudioAttributes.Builder()
                        .setUsage(AudioAttributes.USAGE_VOICE_COMMUNICATION)
                        .setContentType(AudioAttributes.CONTENT_TYPE_SPEECH)
                        .build())
                .setAcceptsDelayedFocusGain(true)
                .build();
        int focusGranted = mAudioManager.requestAudioFocus(mLastAudioFocusRequest);
        // NOTE: We will record even if the audio focus was not granted.
        Log.d(TAG,
                "AudioFocus granted " + (focusGranted == AudioManager.AUDIOFOCUS_REQUEST_GRANTED));

        mRecorder = new MediaRecorder();
        mRecorder.setAudioSource(MediaRecorder.AudioSource.MIC);
        mRecorder.setOutputFormat(MediaRecorder.OutputFormat.THREE_GPP);
        mRecorder.setAudioEncoder(MediaRecorder.AudioEncoder.AAC);
        mRecorder.setOnInfoListener((MediaRecorder recorder, int what, int extra) ->
                Log.i(TAG, "OnMediaRecorderInfo: what=" + what + ", extra=" + extra));
        mRecorder.setOnErrorListener((MediaRecorder recorder, int what, int extra) ->
                Log.i(TAG, "OnMediaRecorderError: what=" + what + ", extra=" + extra));
        mRecorder.setOutputFile(mAudioFile);

        try {
            mRecorder.prepare();
        } catch (IOException e) {
            Log.e(TAG, "Failed on MediaRecorder#prepare(), filename: " + mAudioFile, e);
            finish();
            return;
        }

        mRecorder.start();
        mVoiceRecordingView.setRecorder(mRecorder);
        mAudioRecordingIsRunning = true;

        // Messages with token mRecorder are cleared when the activity finishes or recording stops.
        mHandler.postDelayed(() -> {
            Log.i(TAG, "Timed out while recording voice message, cancelling.");
            stopAudioRecording();
            showSubmitBugReportUi(/* isRecording= */ false);
        }, /* token= */ mRecorder, VOICE_MESSAGE_MAX_DURATION_MILLIS);
    }

    private void stopAudioRecording() {
        if (mRecorder != null) {
            Log.i(TAG, "Recording ended, stopping the MediaRecorder.");
            mHandler.removeCallbacksAndMessages(/* token= */ mRecorder);
            try {
                mRecorder.stop();
            } catch (RuntimeException e) {
                // Sometimes MediaRecorder doesn't start and stopping it throws an error.
                // We just log these cases, no need to crash the app.
                Log.w(TAG, "Couldn't stop media recorder", e);
            }
            mRecorder.release();
            mRecorder = null;
        }
        if (mLastAudioFocusRequest != null) {
            int focusAbandoned = mAudioManager.abandonAudioFocusRequest(mLastAudioFocusRequest);
            Log.d(TAG, "Audio focus abandoned "
                    + (focusAbandoned == AudioManager.AUDIOFOCUS_REQUEST_GRANTED));
            mLastAudioFocusRequest = null;
        }
        mVoiceRecordingView.setRecorder(null);
    }

    private static String getCurrentUserName(Context context) {
        UserManager um = UserManager.get(context);
        return um.getUserName();
    }

    /**
     * Creates a {@link MetaBugReport} and saves it in a local sqlite database.
     *
     * @param context an Android context.
     * @param type bug report type, {@link MetaBugReport.BugReportType}.
     */
    static MetaBugReport createBugReport(Context context, int type) {
        String timestamp = MetaBugReport.toBugReportTimestamp(new Date());
        String username = getCurrentUserName(context);
        String title = BugReportTitleGenerator.generateBugReportTitle(timestamp, username);
        return BugStorageUtils.createBugReport(context, title, timestamp, username, type);
    }

    /** A helper class to generate bugreport title. */
    private static final class BugReportTitleGenerator {
        /** Contains easily readable characters. */
        private static final char[] CHARS_FOR_RANDOM_GENERATOR =
                new char[]{'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'J', 'K', 'L', 'M', 'N', 'P',
                        'R', 'S', 'T', 'U', 'W', 'X', 'Y', 'Z'};

        /**
         * Generates a bugreport title from given timestamp and username.
         *
         * <p>Example: "[A45E8] Feedback from user Driver at 2019-09-21_12:00:00"
         */
        static String generateBugReportTitle(String timestamp, String username) {
            // Lookup string is used to search a bug in Buganizer (see b/130915969).
            String lookupString = generateRandomString(LOOKUP_STRING_LENGTH);
            return "[" + lookupString + "] Feedback from user " + username + " at " + timestamp;
        }

        private static String generateRandomString(int length) {
            Random random = new Random();
            StringBuilder builder = new StringBuilder();
            for (int i = 0; i < length; i++) {
                int randomIndex = random.nextInt(CHARS_FOR_RANDOM_GENERATOR.length);
                builder.append(CHARS_FOR_RANDOM_GENERATOR[randomIndex]);
            }
            return builder.toString();
        }
    }

    /** AsyncTask that recursively deletes files and directories. */
    private static class DeleteFilesAndDirectoriesAsyncTask extends AsyncTask<File, Void, Void> {
        @Override
        protected Void doInBackground(File... files) {
            for (File file : files) {
                Log.i(TAG, "Deleting " + file.getAbsolutePath());
                if (file.isFile()) {
                    file.delete();
                } else {
                    FileUtils.deleteDirectory(file);
                }
            }
            return null;
        }
    }

    /**
     * AsyncTask that moves audio file to the system user's {@link FileUtils#getPendingDir} and
     * sets status to either STATUS_UPLOAD_PENDING or STATUS_PENDING_USER_ACTION.
     */
    private static class AddAudioToBugReportAsyncTask extends AsyncTask<Void, Void, Void> {
        private final Context mContext;
        private final Config mConfig;
        private final File mAudioFile;
        private final MetaBugReport mOriginalBug;

        AddAudioToBugReportAsyncTask(
                Context context, Config config, MetaBugReport bug, File audioFile) {
            mContext = context;
            mConfig = config;
            mOriginalBug = bug;
            mAudioFile = audioFile;
        }

        @Override
        protected Void doInBackground(Void... voids) {
            String audioFileName = FileUtils.getAudioFileName(
                    MetaBugReport.toBugReportTimestamp(new Date()), mOriginalBug);
            MetaBugReport bug = BugStorageUtils.update(mContext,
                    mOriginalBug.toBuilder().setAudioFileName(audioFileName).build());
            try (OutputStream out = BugStorageUtils.openAudioMessageFileToWrite(mContext, bug);
                 InputStream input = new FileInputStream(mAudioFile)) {
                ByteStreams.copy(input, out);
            } catch (IOException e) {
                BugStorageUtils.setBugReportStatus(mContext, bug,
                        com.android.car.bugreport.Status.STATUS_WRITE_FAILED,
                        "Failed to write audio to bug report");
                Log.e(TAG, "Failed to write audio to bug report", e);
                return null;
            }
            if (mConfig.getAutoUpload()) {
                BugStorageUtils.setBugReportStatus(mContext, bug,
                        com.android.car.bugreport.Status.STATUS_UPLOAD_PENDING, "");
            } else {
                BugStorageUtils.setBugReportStatus(mContext, bug,
                        com.android.car.bugreport.Status.STATUS_PENDING_USER_ACTION, "");
                BugReportService.showBugReportFinishedNotification(mContext, bug);
            }
            mAudioFile.delete();
            return null;
        }
    }
}
