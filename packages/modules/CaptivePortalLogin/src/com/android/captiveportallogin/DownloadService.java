/*
 * Copyright (C) 2020 The Android Open Source Project
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

package com.android.captiveportallogin;

import static java.lang.Math.min;

import android.app.Notification;
import android.app.NotificationChannel;
import android.app.NotificationManager;
import android.app.PendingIntent;
import android.app.Service;
import android.content.Context;
import android.content.Intent;
import android.content.res.Resources;
import android.graphics.drawable.Icon;
import android.icu.text.NumberFormat;
import android.net.Network;
import android.net.Uri;
import android.os.IBinder;
import android.os.ParcelFileDescriptor;
import android.provider.DocumentsContract;
import android.util.Log;

import androidx.annotation.GuardedBy;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import java.io.FileNotFoundException;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.net.HttpURLConnection;
import java.net.URL;
import java.net.URLConnection;
import java.util.ArrayDeque;
import java.util.Queue;
import java.util.concurrent.atomic.AtomicInteger;

/**
 * Foreground {@link Service} that can be used to download files from a specific {@link Network}.
 *
 * If the network is or becomes unusable, the download will fail: the service will not attempt
 * downloading from other networks on the device.
 */
public class DownloadService extends Service {
    private static final String TAG = DownloadService.class.getSimpleName();

    @VisibleForTesting
    static final String ARG_NETWORK = "network";
    @VisibleForTesting
    static final String ARG_USERAGENT = "useragent";
    @VisibleForTesting
    static final String ARG_URL = "url";
    @VisibleForTesting
    static final String ARG_DISPLAY_NAME = "displayname";
    @VisibleForTesting
    static final String ARG_OUTFILE = "outfile";

    private static final String ARG_CANCEL = "cancel";

    private static final String CHANNEL_DOWNLOADS = "downloads";
    private static final String CHANNEL_DOWNLOAD_PROGRESS = "downloads_progress";
    private static final int NOTE_DOWNLOAD_PROGRESS = 1;
    private static final int NOTE_DOWNLOAD_DONE = 2;

    private static final int CONNECTION_TIMEOUT_MS = 30_000;
    // Update download progress up to twice/sec.
    private static final long MAX_PROGRESS_UPDATE_RATE_MS = 500L;
    private static final long CONTENT_LENGTH_UNKNOWN = -1L;

    // All download job IDs <= this value should be cancelled
    private volatile int mMaxCancelDownloadId;

    @GuardedBy("mQueue")
    private final Queue<DownloadTask> mQueue = new ArrayDeque<>(1);
    @GuardedBy("mQueue")
    private boolean mProcessing = false;

    // Tracker for the ID to assign to the next download. The service startId is not used because it
    // is not guaranteed to be monotonically increasing; increasing download IDs are convenient to
    // allow cancelling current downloads when the user tapped the cancel button, but not subsequent
    // download jobs.
    private final AtomicInteger mNextDownloadId = new AtomicInteger(1);

    private static class DownloadTask {
        private final int mId;
        private final Network mNetwork;
        private final String mUserAgent;
        private final String mUrl;
        private final String mDisplayName;
        private final Uri mOutFile;

        private final Notification.Builder mCachedNotificationBuilder;

        private DownloadTask(int id, Network network, String userAgent, String url,
                String displayName, Uri outFile, Context context) {
            this.mId = id;
            this.mNetwork = network;
            this.mUserAgent = userAgent;
            this.mUrl = url;
            this.mDisplayName = displayName;
            this.mOutFile = outFile;

            final Resources res = context.getResources();
            final Intent cancelIntent = new Intent(context, DownloadService.class)
                    .putExtra(ARG_CANCEL, mId)
                    .setIdentifier(String.valueOf(mId));

            final PendingIntent pendingIntent = PendingIntent.getService(context,
                    0 /* requestCode */, cancelIntent, 0 /* flags */);
            final Notification.Action cancelAction = new Notification.Action.Builder(
                    Icon.createWithResource(context, R.drawable.ic_close),
                    res.getString(android.R.string.cancel),
                    pendingIntent).build();
            this.mCachedNotificationBuilder = new Notification.Builder(
                    context, CHANNEL_DOWNLOAD_PROGRESS)
                    .setContentTitle(res.getString(R.string.downloading_paramfile, mDisplayName))
                    .setSmallIcon(R.drawable.ic_cloud_download)
                    .setOnlyAlertOnce(true)
                    .addAction(cancelAction);
        }
    }

    /**
     * Create an intent to be used to start the service.
     *
     * <p>The intent can then be used with {@link Context#startForegroundService(Intent)}.
     * @param packageContext Context to use to resolve the {@link DownloadService}.
     * @param network Network that the download should be done on. No other network will be
     *                considered for the download.
     * @param userAgent UserAgent to use for the download request.
     * @param url URL to download from.
     * @param displayName Name of the downloaded file, to be used when displaying progress UI (does
     *                    not affect the actual output file).
     * @param outFile Output file of the download.
     */
    public static Intent makeDownloadIntent(Context packageContext, Network network,
            String userAgent, String url, String displayName, Uri outFile) {
        final Intent intent = new Intent(packageContext, DownloadService.class);
        intent.putExtra(ARG_NETWORK, network);
        intent.putExtra(ARG_USERAGENT, userAgent);
        intent.putExtra(ARG_URL, url);
        intent.putExtra(ARG_DISPLAY_NAME, displayName);
        intent.putExtra(ARG_OUTFILE, outFile);
        return intent;
    }

    /**
     * Create an intent to be used via {android.app.Activity#startActivityForResult} to create
     * an output file that can be used to start a download.
     *
     * <p>This creates a {@link Intent#ACTION_CREATE_DOCUMENT} intent. Its result must be handled by
     * the calling activity.
     */
    public static Intent makeCreateFileIntent(String mimetype, String filename) {
        final Intent intent = new Intent(Intent.ACTION_CREATE_DOCUMENT);
        intent.addCategory(Intent.CATEGORY_OPENABLE);
        intent.setType(mimetype);
        intent.putExtra(Intent.EXTRA_TITLE, filename);

        return intent;
    }

    @Override
    public void onCreate() {
        createNotificationChannels();
    }

    /**
     * Called when the service needs to process a new command:
     *  - If the intent has ARG_CANCEL extra, all downloads with a download ID <= that argument
     *    should be cancelled.
     *  - Otherwise the intent indicates a new download (with network, useragent, url... args).
     *
     * This method may be called multiple times if the user selects multiple files to download.
     * Files will be queued to be downloaded one by one; if the user cancels the current file, this
     * will not affect the next files that are queued.
     */
    @Override
    public int onStartCommand(@Nullable Intent intent, int flags, int startId) {
        if (intent == null) {
            return START_NOT_STICKY;
        }
        final int cancelDownloadId = intent.getIntExtra(ARG_CANCEL, -1);
        if (cancelDownloadId != -1) {
            mMaxCancelDownloadId = cancelDownloadId;
            return START_NOT_STICKY;
        }

        final Network network = intent.getParcelableExtra(ARG_NETWORK);
        final String userAgent = intent.getStringExtra(ARG_USERAGENT);
        final String url = intent.getStringExtra(ARG_URL);
        final String filename = intent.getStringExtra(ARG_DISPLAY_NAME);
        final Uri outFile = intent.getParcelableExtra(ARG_OUTFILE);

        if (network == null || userAgent == null || url == null || filename == null
                || outFile == null) {
            Log.e(TAG, String.format("Missing parameters; network: %s, userAgent: %s, url: %s, "
                    + "filename: %s, outFile: %s", network, userAgent, url, filename, outFile));
            return START_NOT_STICKY;
        }

        synchronized (mQueue) {
            final DownloadTask task = new DownloadTask(mNextDownloadId.getAndIncrement(), network,
                    userAgent, url, filename, outFile, this);
            mQueue.add(task);
            if (!mProcessing) {
                startForeground(NOTE_DOWNLOAD_PROGRESS, makeProgressNotification(task,
                        null /* progress */));
                new Thread(new ProcessingRunnable()).start();
            }
            mProcessing = true;
        }

        // If the service is killed the download is lost, which is fine because it is unlikely for a
        // foreground service to be killed, and there is no easy way to know whether the download
        // was really not yet completed if the service is restarted with e.g. START_REDELIVER_INTENT
        return START_NOT_STICKY;
    }

    private void createNotificationChannels() {
        final NotificationManager nm = getSystemService(NotificationManager.class);
        final Resources res = getResources();
        final NotificationChannel downloadChannel = new NotificationChannel(CHANNEL_DOWNLOADS,
                res.getString(R.string.channel_name_downloads),
                NotificationManager.IMPORTANCE_DEFAULT);
        downloadChannel.setDescription(res.getString(R.string.channel_description_downloads));
        nm.createNotificationChannel(downloadChannel);

        final NotificationChannel progressChannel = new NotificationChannel(
                CHANNEL_DOWNLOAD_PROGRESS,
                res.getString(R.string.channel_name_download_progress),
                NotificationManager.IMPORTANCE_LOW);
        progressChannel.setDescription(
                res.getString(R.string.channel_description_download_progress));
        nm.createNotificationChannel(progressChannel);
    }

    @Override
    public IBinder onBind(Intent intent) {
        return null;
    }

    private class ProcessingRunnable implements Runnable {
        @Override
        public void run() {
            while (true) {
                final DownloadTask task;
                synchronized (mQueue) {
                    task = mQueue.poll();
                    if (task == null)  {
                        mProcessing = false;
                        stopForeground(true /* removeNotification */);
                        return;
                    }
                }

                processDownload(task);
            }
        }

        private void processDownload(@NonNull final DownloadTask task) {
            final NotificationManager nm = getSystemService(NotificationManager.class);
            // Start by showing an indeterminate progress notification
            nm.notify(NOTE_DOWNLOAD_PROGRESS, makeProgressNotification(task, null /* progress */));
            URLConnection connection = null;
            try {
                final URL url = new URL(task.mUrl);

                // This may fail if the network is not usable anymore, which is the expected
                // behavior: the download should fail if it cannot be completed on the assigned
                // network.
                connection = task.mNetwork.openConnection(url);
                connection.setConnectTimeout(CONNECTION_TIMEOUT_MS);
                connection.setReadTimeout(CONNECTION_TIMEOUT_MS);
                connection.setRequestProperty("User-Agent", task.mUserAgent);

                long contentLength = CONTENT_LENGTH_UNKNOWN;
                if (connection instanceof HttpURLConnection) {
                    final HttpURLConnection httpConn = (HttpURLConnection) connection;
                    final int responseCode = httpConn.getResponseCode();
                    if (responseCode < 200 || responseCode > 299) {
                        throw new IOException("Download error: response code " + responseCode);
                    }

                    contentLength = httpConn.getContentLengthLong();
                }

                try (ParcelFileDescriptor pfd = getContentResolver().openFileDescriptor(
                        task.mOutFile, "rwt");
                     FileOutputStream fop = new FileOutputStream(pfd.getFileDescriptor())) {
                    final InputStream is = connection.getInputStream();

                    if (!downloadToFile(is, fop, contentLength, task, nm)) {
                        // Download cancelled
                        tryDeleteFile(task.mOutFile);
                        // Don't clear the notification: this will be done when the service stops
                        // (foreground service notifications cannot be cleared).
                        return;
                    }
                }

                nm.notify(NOTE_DOWNLOAD_DONE,
                        makeDoneNotification(task.mId, task.mDisplayName, task.mOutFile));
            } catch (IOException e) {
                Log.e(DownloadService.class.getSimpleName(), "Download error", e);
                nm.notify(NOTE_DOWNLOAD_DONE, makeErrorNotification(task.mDisplayName));
                tryDeleteFile(task.mOutFile);
            } finally {
                if (connection instanceof HttpURLConnection) {
                    ((HttpURLConnection) connection).disconnect();
                }
            }
        }

        /**
         * Download the contents of an {@link InputStream} to a {@link FileOutputStream}, and
         * updates the progress notification.
         * @return True if download is completed, false if cancelled
         */
        private boolean downloadToFile(@NonNull InputStream is, @NonNull FileOutputStream fop,
                long contentLength, @NonNull DownloadTask task,
                @NonNull NotificationManager nm) throws IOException {
            final byte[] buffer = new byte[1500];
            int allRead = 0;
            final long maxRead = contentLength == CONTENT_LENGTH_UNKNOWN
                    ? Long.MAX_VALUE : contentLength;
            int lastProgress = -1;
            long lastUpdateTime = -1L;
            while (allRead < maxRead) {
                if (task.mId <= mMaxCancelDownloadId) {
                    return false;
                }

                final int read = is.read(buffer, 0, (int) min(buffer.length, maxRead - allRead));
                if (read < 0) {
                    // End of stream
                    break;
                }

                allRead += read;
                fop.write(buffer, 0, read);

                final Integer progress = getProgress(contentLength, allRead);
                if (progress == null || progress.equals(lastProgress)) continue;

                final long now = System.currentTimeMillis();
                if (maybeNotifyProgress(progress, lastProgress, now, lastUpdateTime, task, nm)) {
                    lastUpdateTime = now;
                }
                lastProgress = progress;
            }
            return true;
        }

        private void tryDeleteFile(@NonNull Uri file) {
            try {
                // The file was not created by the DownloadService, however because the service
                // is only usable from this application, and the file should be created from this
                // same application, the content resolver should be the same.
                DocumentsContract.deleteDocument(getContentResolver(), file);
            } catch (FileNotFoundException e) {
                // Nothing to delete
            }
        }

        private Integer getProgress(long contentLength, long totalRead) {
            if (contentLength == CONTENT_LENGTH_UNKNOWN || contentLength == 0) return null;
            return (int) (totalRead * 100 / contentLength);
        }

        /**
         * Update the progress notification, if it was not updated recently.
         * @return True if progress was updated.
         */
        private boolean maybeNotifyProgress(int progress, int lastProgress, long now,
                long lastProgressUpdateTimeMs, @NonNull DownloadTask task,
                @NonNull NotificationManager nm) {
            if (lastProgress > 0 && progress < 100
                    && lastProgressUpdateTimeMs > 0
                    && now - lastProgressUpdateTimeMs < MAX_PROGRESS_UPDATE_RATE_MS) {
                // Rate-limit intermediate progress updates: NotificationManager will start ignoring
                // notifications from the current process if too many updates are posted too fast.
                // The shown progress will not "lag behind" much in most cases. An alternative
                // would be to delay the progress update to rate-limit, but this would bring
                // synchronization problems.
                return false;
            }
            final Notification note = makeProgressNotification(task, progress);
            nm.notify(NOTE_DOWNLOAD_PROGRESS, note);
            return true;
        }
    }

    @NonNull
    private Notification makeProgressNotification(@NonNull DownloadTask task,
            @Nullable Integer progress) {
        return task.mCachedNotificationBuilder
                .setContentText(progress == null
                        ? null
                        : NumberFormat.getPercentInstance().format(progress.floatValue() / 100))
                .setProgress(100,
                        progress == null ? 0 : progress,
                        progress == null /* indeterminate */)
                .build();
    }

    @NonNull
    private Notification makeDoneNotification(int taskId, @NonNull String displayName,
            @NonNull Uri outFile) {
        final Intent intent = new Intent(Intent.ACTION_VIEW)
                .addFlags(Intent.FLAG_ACTIVITY_NEW_TASK)
                .addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION)
                .setData(outFile)
                .setIdentifier(String.valueOf(taskId));

        final PendingIntent pendingIntent = PendingIntent.getActivity(
                this, 0 /* requestCode */, intent, 0 /* flags */);
        return new Notification.Builder(this, CHANNEL_DOWNLOADS)
                .setContentTitle(getResources().getString(R.string.download_completed))
                .setContentText(displayName)
                .setSmallIcon(R.drawable.ic_cloud_download)
                .setContentIntent(pendingIntent)
                .setAutoCancel(true)
                .build();
    }

    @NonNull
    private Notification makeErrorNotification(@NonNull String filename) {
        final Resources res = getResources();
        return new Notification.Builder(this, CHANNEL_DOWNLOADS)
                .setContentTitle(res.getString(R.string.error_downloading_paramfile, filename))
                .setSmallIcon(R.drawable.ic_cloud_download)
                .build();
    }
}
