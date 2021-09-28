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

import static android.car.CarBugreportManager.CarBugreportManagerCallback.CAR_BUGREPORT_DUMPSTATE_CONNECTION_FAILED;
import static android.car.CarBugreportManager.CarBugreportManagerCallback.CAR_BUGREPORT_DUMPSTATE_FAILED;
import static android.car.CarBugreportManager.CarBugreportManagerCallback.CAR_BUGREPORT_SERVICE_NOT_AVAILABLE;
import static android.view.Display.DEFAULT_DISPLAY;
import static android.view.WindowManager.LayoutParams.TYPE_APPLICATION_OVERLAY;

import static com.android.car.bugreport.PackageUtils.getPackageVersion;

import android.annotation.FloatRange;
import android.annotation.StringRes;
import android.app.Notification;
import android.app.NotificationChannel;
import android.app.NotificationManager;
import android.app.PendingIntent;
import android.app.Service;
import android.car.Car;
import android.car.CarBugreportManager;
import android.car.CarNotConnectedException;
import android.content.Context;
import android.content.Intent;
import android.hardware.display.DisplayManager;
import android.media.AudioManager;
import android.media.Ringtone;
import android.media.RingtoneManager;
import android.net.Uri;
import android.os.Binder;
import android.os.Build;
import android.os.Bundle;
import android.os.Handler;
import android.os.IBinder;
import android.os.Message;
import android.os.ParcelFileDescriptor;
import android.util.Log;
import android.view.Display;
import android.widget.Toast;

import com.google.common.base.Preconditions;
import com.google.common.io.ByteStreams;
import com.google.common.util.concurrent.AtomicDouble;

import java.io.BufferedOutputStream;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.OutputStream;
import java.util.concurrent.Executors;
import java.util.concurrent.ScheduledExecutorService;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.atomic.AtomicBoolean;
import java.util.zip.ZipOutputStream;

/**
 * Service that captures screenshot and bug report using dumpstate and bluetooth snoop logs.
 *
 * <p>After collecting all the logs it sets the {@link MetaBugReport} status to
 * {@link Status#STATUS_AUDIO_PENDING} or {@link Status#STATUS_PENDING_USER_ACTION} depending
 * on {@link MetaBugReport#getType}.
 *
 * <p>If the service is started with action {@link #ACTION_START_SILENT}, it will start
 * bugreporting without showing dialog and recording audio message, see
 * {@link MetaBugReport#TYPE_SILENT}.
 */
public class BugReportService extends Service {
    private static final String TAG = BugReportService.class.getSimpleName();

    /**
     * Extra data from intent - current bug report.
     */
    static final String EXTRA_META_BUG_REPORT = "meta_bug_report";

    /** Starts silent (no audio message recording) bugreporting. */
    private static final String ACTION_START_SILENT =
            "com.android.car.bugreport.action.START_SILENT";

    // Wait a short time before starting to capture the bugreport and the screen, so that
    // bugreport activity can detach from the view tree.
    // It is ugly to have a timeout, but it is ok here because such a delay should not really
    // cause bugreport to be tainted with so many other events. If in the future we want to change
    // this, the best option is probably to wait for onDetach events from view tree.
    private static final int ACTIVITY_FINISH_DELAY_MILLIS = 1000;

    /** Stop the service only after some delay, to allow toasts to show on the screen. */
    private static final int STOP_SERVICE_DELAY_MILLIS = 1000;

    /**
     * Wait a short time before showing "bugreport started" toast message, because the service
     * will take a screenshot of the screen.
     */
    private static final int BUGREPORT_STARTED_TOAST_DELAY_MILLIS = 2000;

    private static final String BT_SNOOP_LOG_LOCATION = "/data/misc/bluetooth/logs/btsnoop_hci.log";
    private static final boolean DEBUG = Log.isLoggable(TAG, Log.DEBUG);

    /** Notifications on this channel will silently appear in notification bar. */
    private static final String PROGRESS_CHANNEL_ID = "BUGREPORT_PROGRESS_CHANNEL";

    /** Notifications on this channel will pop-up. */
    private static final String STATUS_CHANNEL_ID = "BUGREPORT_STATUS_CHANNEL";

    /** Persistent notification is shown when bugreport is in progress or waiting for audio. */
    private static final int BUGREPORT_IN_PROGRESS_NOTIF_ID = 1;

    /** Dismissible notification is shown when bugreport is collected. */
    static final int BUGREPORT_FINISHED_NOTIF_ID = 2;

    private static final String OUTPUT_ZIP_FILE = "output_file.zip";
    private static final String EXTRA_OUTPUT_ZIP_FILE = "extra_output_file.zip";

    private static final String MESSAGE_FAILURE_DUMPSTATE = "Failed to grab dumpstate";
    private static final String MESSAGE_FAILURE_ZIP = "Failed to zip files";

    private static final int PROGRESS_HANDLER_EVENT_PROGRESS = 1;
    private static final String PROGRESS_HANDLER_DATA_PROGRESS = "progress";

    static final float MAX_PROGRESS_VALUE = 100f;

    /** Binder given to clients. */
    private final IBinder mBinder = new ServiceBinder();

    /** True if {@link BugReportService} is already collecting bugreport, including zipping. */
    private final AtomicBoolean mIsCollectingBugReport = new AtomicBoolean(false);
    private final AtomicDouble mBugReportProgress = new AtomicDouble(0);

    private MetaBugReport mMetaBugReport;
    private NotificationManager mNotificationManager;
    private ScheduledExecutorService mSingleThreadExecutor;
    private BugReportProgressListener mBugReportProgressListener;
    private Car mCar;
    private CarBugreportManager mBugreportManager;
    private CarBugreportManager.CarBugreportManagerCallback mCallback;
    private Config mConfig;
    private Context mWindowContext;

    /** A handler on the main thread. */
    private Handler mHandler;
    /**
     * A handler to the main thread to show toast messages, it will be cleared when the service
     * finishes. We need to clear it otherwise when bugreport fails, it will show "bugreport start"
     * toast, which will confuse users.
     */
    private Handler mHandlerStartedToast;

    /** A listener that's notified when bugreport progress changes. */
    interface BugReportProgressListener {
        /**
         * Called when bug report progress changes.
         *
         * @param progress - a bug report progress in [0.0, 100.0].
         */
        void onProgress(float progress);
    }

    /** Client binder. */
    public class ServiceBinder extends Binder {
        BugReportService getService() {
            // Return this instance of LocalService so clients can call public methods
            return BugReportService.this;
        }
    }

    /** A handler on the main thread. */
    private class BugReportHandler extends Handler {
        @Override
        public void handleMessage(Message message) {
            switch (message.what) {
                case PROGRESS_HANDLER_EVENT_PROGRESS:
                    if (mBugReportProgressListener != null) {
                        float progress = message.getData().getFloat(PROGRESS_HANDLER_DATA_PROGRESS);
                        mBugReportProgressListener.onProgress(progress);
                    }
                    showProgressNotification();
                    break;
                default:
                    Log.d(TAG, "Unknown event " + message.what + ", ignoring.");
            }
        }
    }

    @Override
    public void onCreate() {
        Preconditions.checkState(Config.isBugReportEnabled(), "BugReport is disabled.");

        DisplayManager dm = getSystemService(DisplayManager.class);
        Display primaryDisplay = dm.getDisplay(DEFAULT_DISPLAY);
        mWindowContext = createDisplayContext(primaryDisplay)
                .createWindowContext(TYPE_APPLICATION_OVERLAY, null);

        mNotificationManager = getSystemService(NotificationManager.class);
        mNotificationManager.createNotificationChannel(new NotificationChannel(
                PROGRESS_CHANNEL_ID,
                getString(R.string.notification_bugreport_channel_name),
                NotificationManager.IMPORTANCE_DEFAULT));
        mNotificationManager.createNotificationChannel(new NotificationChannel(
                STATUS_CHANNEL_ID,
                getString(R.string.notification_bugreport_channel_name),
                NotificationManager.IMPORTANCE_HIGH));
        mSingleThreadExecutor = Executors.newSingleThreadScheduledExecutor();
        mHandler = new BugReportHandler();
        mHandlerStartedToast = new Handler();
        mConfig = new Config();
        mConfig.start();
    }

    @Override
    public void onDestroy() {
        if (DEBUG) {
            Log.d(TAG, "Service destroyed");
        }
        disconnectFromCarService();
    }

    @Override
    public int onStartCommand(final Intent intent, int flags, int startId) {
        if (mIsCollectingBugReport.getAndSet(true)) {
            Log.w(TAG, "bug report is already being collected, ignoring");
            Toast.makeText(mWindowContext,
                    R.string.toast_bug_report_in_progress, Toast.LENGTH_SHORT).show();
            return START_NOT_STICKY;
        }

        Log.i(TAG, String.format("Will start collecting bug report, version=%s",
                getPackageVersion(this)));

        if (ACTION_START_SILENT.equals(intent.getAction())) {
            Log.i(TAG, "Starting a silent bugreport.");
            mMetaBugReport = BugReportActivity.createBugReport(this, MetaBugReport.TYPE_SILENT);
        } else {
            Bundle extras = intent.getExtras();
            mMetaBugReport = extras.getParcelable(EXTRA_META_BUG_REPORT);
        }

        mBugReportProgress.set(0);

        startForeground(BUGREPORT_IN_PROGRESS_NOTIF_ID, buildProgressNotification());
        showProgressNotification();

        collectBugReport();

        // Show a short lived "bugreport started" toast message after a short delay.
        mHandlerStartedToast.postDelayed(() -> {
            Toast.makeText(mWindowContext,
                    getText(R.string.toast_bug_report_started), Toast.LENGTH_LONG).show();
        }, BUGREPORT_STARTED_TOAST_DELAY_MILLIS);

        // If the service process gets killed due to heavy memory pressure, do not restart.
        return START_NOT_STICKY;
    }

    private void onCarLifecycleChanged(Car car, boolean ready) {
        // not ready - car service is crashed or is restarting.
        if (!ready) {
            mBugreportManager = null;
            mCar = null;

            // NOTE: dumpstate still might be running, but we can't kill it or reconnect to it
            //       so we ignore it.
            handleBugReportManagerError(CAR_BUGREPORT_SERVICE_NOT_AVAILABLE);
            return;
        }
        try {
            mBugreportManager = (CarBugreportManager) car.getCarManager(Car.CAR_BUGREPORT_SERVICE);
        } catch (CarNotConnectedException | NoClassDefFoundError e) {
            throw new IllegalStateException("Failed to get CarBugreportManager.", e);
        }
    }

    /** Shows an updated progress notification. */
    private void showProgressNotification() {
        if (isCollectingBugReport()) {
            mNotificationManager.notify(
                    BUGREPORT_IN_PROGRESS_NOTIF_ID, buildProgressNotification());
        }
    }

    private Notification buildProgressNotification() {
        Intent intent = new Intent(getApplicationContext(), BugReportInfoActivity.class);
        PendingIntent startBugReportInfoActivity =
                PendingIntent.getActivity(getApplicationContext(), 0, intent, 0);
        return new Notification.Builder(this, PROGRESS_CHANNEL_ID)
                .setContentTitle(getText(R.string.notification_bugreport_in_progress))
                .setContentText(mMetaBugReport.getTitle())
                .setSubText(String.format("%.1f%%", mBugReportProgress.get()))
                .setSmallIcon(R.drawable.download_animation)
                .setCategory(Notification.CATEGORY_STATUS)
                .setOngoing(true)
                .setProgress((int) MAX_PROGRESS_VALUE, (int) mBugReportProgress.get(), false)
                .setContentIntent(startBugReportInfoActivity)
                .build();
    }

    /** Returns true if bugreporting is in progress. */
    public boolean isCollectingBugReport() {
        return mIsCollectingBugReport.get();
    }

    /** Returns current bugreport progress. */
    public float getBugReportProgress() {
        return (float) mBugReportProgress.get();
    }

    /** Sets a bugreport progress listener. The listener is called on a main thread. */
    public void setBugReportProgressListener(BugReportProgressListener listener) {
        mBugReportProgressListener = listener;
    }

    /** Removes the bugreport progress listener. */
    public void removeBugReportProgressListener() {
        mBugReportProgressListener = null;
    }

    @Override
    public IBinder onBind(Intent intent) {
        return mBinder;
    }

    private void showToast(@StringRes int resId) {
        // run on ui thread.
        mHandler.post(
                () -> Toast.makeText(mWindowContext, getText(resId), Toast.LENGTH_LONG).show());
    }

    private void disconnectFromCarService() {
        if (mCar != null) {
            mCar.disconnect();
            mCar = null;
        }
        mBugreportManager = null;
    }

    private void connectToCarServiceSync() {
        if (mCar == null || !(mCar.isConnected() || mCar.isConnecting())) {
            mCar = Car.createCar(this, /* handler= */ null,
                    Car.CAR_WAIT_TIMEOUT_WAIT_FOREVER, this::onCarLifecycleChanged);
        }
    }

    private void collectBugReport() {
        // Connect to the car service before collecting bugreport, because when car service crashes,
        // BugReportService doesn't automatically reconnect to it.
        connectToCarServiceSync();

        if (Build.IS_USERDEBUG || Build.IS_ENG) {
            mSingleThreadExecutor.schedule(
                    this::grabBtSnoopLog, ACTIVITY_FINISH_DELAY_MILLIS, TimeUnit.MILLISECONDS);
        }
        mSingleThreadExecutor.schedule(
                this::saveBugReport, ACTIVITY_FINISH_DELAY_MILLIS, TimeUnit.MILLISECONDS);
    }

    private void grabBtSnoopLog() {
        Log.i(TAG, "Grabbing bt snoop log");
        File result = FileUtils.getFileWithSuffix(this, mMetaBugReport.getTimestamp(),
                "-btsnoop.bin.log");
        File snoopFile = new File(BT_SNOOP_LOG_LOCATION);
        if (!snoopFile.exists()) {
            Log.w(TAG, BT_SNOOP_LOG_LOCATION + " not found, skipping");
            return;
        }
        try (FileInputStream input = new FileInputStream(snoopFile);
             FileOutputStream output = new FileOutputStream(result)) {
            ByteStreams.copy(input, output);
        } catch (IOException e) {
            // this regularly happens when snooplog is not enabled so do not log as an error
            Log.i(TAG, "Failed to grab bt snooplog, continuing to take bug report.", e);
        }
    }

    private void saveBugReport() {
        Log.i(TAG, "Dumpstate to file");
        File outputFile = FileUtils.getFile(this, mMetaBugReport.getTimestamp(), OUTPUT_ZIP_FILE);
        File extraOutputFile = FileUtils.getFile(this, mMetaBugReport.getTimestamp(),
                EXTRA_OUTPUT_ZIP_FILE);
        try (ParcelFileDescriptor outFd = ParcelFileDescriptor.open(outputFile,
                ParcelFileDescriptor.MODE_CREATE | ParcelFileDescriptor.MODE_READ_WRITE);
             ParcelFileDescriptor extraOutFd = ParcelFileDescriptor.open(extraOutputFile,
                ParcelFileDescriptor.MODE_CREATE | ParcelFileDescriptor.MODE_READ_WRITE)) {
            requestBugReport(outFd, extraOutFd);
        } catch (IOException | RuntimeException e) {
            Log.e(TAG, "Failed to grab dump state", e);
            BugStorageUtils.setBugReportStatus(this, mMetaBugReport, Status.STATUS_WRITE_FAILED,
                    MESSAGE_FAILURE_DUMPSTATE);
            showToast(R.string.toast_status_dump_state_failed);
            disconnectFromCarService();
            mIsCollectingBugReport.set(false);
        }
    }

    private void sendProgressEventToHandler(float progress) {
        Message message = new Message();
        message.what = PROGRESS_HANDLER_EVENT_PROGRESS;
        message.getData().putFloat(PROGRESS_HANDLER_DATA_PROGRESS, progress);
        mHandler.sendMessage(message);
    }

    private void requestBugReport(ParcelFileDescriptor outFd, ParcelFileDescriptor extraOutFd) {
        if (DEBUG) {
            Log.d(TAG, "Requesting a bug report from CarBugReportManager.");
        }
        mCallback = new CarBugreportManager.CarBugreportManagerCallback() {
            @Override
            public void onError(@CarBugreportErrorCode int errorCode) {
                Log.e(TAG, "CarBugreportManager failed: " + errorCode);
                disconnectFromCarService();
                handleBugReportManagerError(errorCode);
            }

            @Override
            public void onProgress(@FloatRange(from = 0f, to = MAX_PROGRESS_VALUE) float progress) {
                mBugReportProgress.set(progress);
                sendProgressEventToHandler(progress);
            }

            @Override
            public void onFinished() {
                Log.d(TAG, "CarBugreportManager finished");
                disconnectFromCarService();
                mBugReportProgress.set(MAX_PROGRESS_VALUE);
                sendProgressEventToHandler(MAX_PROGRESS_VALUE);
                mSingleThreadExecutor.submit(BugReportService.this::zipDirectoryAndUpdateStatus);
            }
        };
        if (mBugreportManager == null) {
            mHandler.post(() -> Toast.makeText(mWindowContext,
                    "Car service is not ready", Toast.LENGTH_LONG).show());
            Log.e(TAG, "CarBugReportManager is not ready");
            return;
        }
        mBugreportManager.requestBugreport(outFd, extraOutFd, mCallback);
    }

    private void handleBugReportManagerError(
            @CarBugreportManager.CarBugreportManagerCallback.CarBugreportErrorCode int errorCode) {
        if (mMetaBugReport == null) {
            Log.w(TAG, "No bugreport is running");
            mIsCollectingBugReport.set(false);
            return;
        }
        // We let the UI know that bug reporting is finished, because the next step is to
        // zip everything and upload.
        mBugReportProgress.set(MAX_PROGRESS_VALUE);
        sendProgressEventToHandler(MAX_PROGRESS_VALUE);
        showToast(R.string.toast_status_failed);
        BugStorageUtils.setBugReportStatus(
                BugReportService.this, mMetaBugReport,
                Status.STATUS_WRITE_FAILED, getBugReportFailureStatusMessage(errorCode));
        mHandler.postDelayed(() -> {
            mNotificationManager.cancel(BUGREPORT_IN_PROGRESS_NOTIF_ID);
            stopForeground(true);
        }, STOP_SERVICE_DELAY_MILLIS);
        mHandlerStartedToast.removeCallbacksAndMessages(null);
        mMetaBugReport = null;
        mIsCollectingBugReport.set(false);
    }

    private static String getBugReportFailureStatusMessage(
            @CarBugreportManager.CarBugreportManagerCallback.CarBugreportErrorCode int errorCode) {
        switch (errorCode) {
            case CAR_BUGREPORT_DUMPSTATE_CONNECTION_FAILED:
            case CAR_BUGREPORT_DUMPSTATE_FAILED:
                return "Failed to connect to dumpstate. Retry again after a minute.";
            case CAR_BUGREPORT_SERVICE_NOT_AVAILABLE:
                return "Car service is not available. Retry again.";
            default:
                return "Car service bugreport collection failed: " + errorCode;
        }
    }

    /**
     * Shows a clickable bugreport finished notification. When clicked it opens
     * {@link BugReportInfoActivity}.
     */
    static void showBugReportFinishedNotification(Context context, MetaBugReport bug) {
        Intent intent = new Intent(context, BugReportInfoActivity.class);
        PendingIntent startBugReportInfoActivity =
                PendingIntent.getActivity(context, 0, intent, 0);
        Notification notification = new Notification
                .Builder(context, STATUS_CHANNEL_ID)
                .setContentTitle(context.getText(R.string.notification_bugreport_finished_title))
                .setContentText(bug.getTitle())
                .setCategory(Notification.CATEGORY_STATUS)
                .setSmallIcon(R.drawable.ic_upload)
                .setContentIntent(startBugReportInfoActivity)
                .build();
        context.getSystemService(NotificationManager.class)
                .notify(BUGREPORT_FINISHED_NOTIF_ID, notification);
    }

    /**
     * Zips the temp directory, writes to the system user's {@link FileUtils#getPendingDir} and
     * updates the bug report status.
     *
     * <p>For {@link MetaBugReport#TYPE_INTERACTIVE}: Sets status to either STATUS_UPLOAD_PENDING or
     * STATUS_PENDING_USER_ACTION and shows a regular notification.
     *
     * <p>For {@link MetaBugReport#TYPE_SILENT}: Sets status to STATUS_AUDIO_PENDING and shows
     * a dialog to record audio message.
     */
    private void zipDirectoryAndUpdateStatus() {
        try {
            // All the generated zip files, images and audio messages are located in this dir.
            // This is located under the current user.
            String bugreportFileName = FileUtils.getZipFileName(mMetaBugReport);
            Log.d(TAG, "Zipping bugreport into " + bugreportFileName);
            mMetaBugReport = BugStorageUtils.update(this,
                    mMetaBugReport.toBuilder().setBugReportFileName(bugreportFileName).build());
            File bugReportTempDir = FileUtils.createTempDir(this, mMetaBugReport.getTimestamp());
            zipDirectoryToOutputStream(bugReportTempDir,
                    BugStorageUtils.openBugReportFileToWrite(this, mMetaBugReport));
        } catch (IOException e) {
            Log.e(TAG, "Failed to zip files", e);
            BugStorageUtils.setBugReportStatus(this, mMetaBugReport, Status.STATUS_WRITE_FAILED,
                    MESSAGE_FAILURE_ZIP);
            showToast(R.string.toast_status_failed);
            return;
        }
        if (mMetaBugReport.getType() == MetaBugReport.TYPE_SILENT) {
            BugStorageUtils.setBugReportStatus(BugReportService.this,
                    mMetaBugReport, Status.STATUS_AUDIO_PENDING, /* message= */ "");
            playNotificationSound();
            startActivity(BugReportActivity.buildAddAudioIntent(this, mMetaBugReport));
        } else {
            // NOTE: If bugreport type is INTERACTIVE, it will already contain an audio message.
            Status status = mConfig.getAutoUpload()
                    ? Status.STATUS_UPLOAD_PENDING : Status.STATUS_PENDING_USER_ACTION;
            BugStorageUtils.setBugReportStatus(BugReportService.this,
                    mMetaBugReport, status, /* message= */ "");
            showBugReportFinishedNotification(this, mMetaBugReport);
        }
        mHandler.post(() -> {
            mNotificationManager.cancel(BUGREPORT_IN_PROGRESS_NOTIF_ID);
            stopForeground(true);
        });
        mHandlerStartedToast.removeCallbacksAndMessages(null);
        mMetaBugReport = null;
        mIsCollectingBugReport.set(false);
    }

    private void playNotificationSound() {
        Uri notification = RingtoneManager.getDefaultUri(RingtoneManager.TYPE_NOTIFICATION);
        Ringtone ringtone = RingtoneManager.getRingtone(getApplicationContext(), notification);
        if (ringtone == null) {
            Log.w(TAG, "No notification ringtone found.");
            return;
        }
        float volume = ringtone.getVolume();
        // Use volume from audio manager, otherwise default ringtone volume can be too loud.
        AudioManager audioManager = getSystemService(AudioManager.class);
        if (audioManager != null) {
            int currentVolume = audioManager.getStreamVolume(AudioManager.STREAM_NOTIFICATION);
            int maxVolume = audioManager.getStreamMaxVolume(AudioManager.STREAM_NOTIFICATION);
            volume = (currentVolume + 0.0f) / maxVolume;
        }
        Log.v(TAG, "Using volume " + volume);
        ringtone.setVolume(volume);
        ringtone.play();
    }

    /**
     * Compresses a directory into a zip file. The method is not recursive. Any sub-directory
     * contained in the main directory and any files contained in the sub-directories will be
     * skipped.
     *
     * @param dirToZip  The path of the directory to zip
     * @param outStream The output stream to write the zip file to
     * @throws IOException if the directory does not exist, its files cannot be read, or the output
     *                     zip file cannot be written.
     */
    private void zipDirectoryToOutputStream(File dirToZip, OutputStream outStream)
            throws IOException {
        if (!dirToZip.isDirectory()) {
            throw new IOException("zip directory does not exist");
        }
        Log.v(TAG, "zipping directory " + dirToZip.getAbsolutePath());

        File[] listFiles = dirToZip.listFiles();
        try (ZipOutputStream zipStream = new ZipOutputStream(new BufferedOutputStream(outStream))) {
            for (File file : listFiles) {
                if (file.isDirectory()) {
                    continue;
                }
                String filename = file.getName();
                // only for the zipped output file, we add individual entries to zip file.
                if (filename.equals(OUTPUT_ZIP_FILE) || filename.equals(EXTRA_OUTPUT_ZIP_FILE)) {
                    ZipUtils.extractZippedFileToZipStream(file, zipStream);
                } else {
                    ZipUtils.addFileToZipStream(file, zipStream);
                }
            }
        } finally {
            outStream.close();
        }
        // Zipping successful, now cleanup the temp dir.
        FileUtils.deleteDirectory(dirToZip);
    }
}
