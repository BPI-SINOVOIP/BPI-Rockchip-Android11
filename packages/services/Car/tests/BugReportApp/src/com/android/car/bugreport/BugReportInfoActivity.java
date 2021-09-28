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

import static com.android.car.bugreport.PackageUtils.getPackageVersion;

import android.app.Activity;
import android.app.NotificationManager;
import android.content.ContentResolver;
import android.content.Intent;
import android.content.res.AssetFileDescriptor;
import android.database.ContentObserver;
import android.net.Uri;
import android.os.AsyncTask;
import android.os.Bundle;
import android.os.Handler;
import android.os.UserHandle;
import android.provider.DocumentsContract;
import android.util.Log;
import android.view.View;
import android.widget.TextView;

import androidx.recyclerview.widget.DividerItemDecoration;
import androidx.recyclerview.widget.LinearLayoutManager;
import androidx.recyclerview.widget.RecyclerView;

import com.google.common.base.Preconditions;
import com.google.common.base.Strings;
import com.google.common.io.ByteStreams;

import java.io.BufferedOutputStream;
import java.io.File;
import java.io.FileDescriptor;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.io.PrintWriter;
import java.lang.ref.WeakReference;
import java.util.ArrayList;
import java.util.List;
import java.util.zip.ZipEntry;
import java.util.zip.ZipInputStream;
import java.util.zip.ZipOutputStream;

/**
 * Provides an activity that provides information on the bugreports that are filed.
 */
public class BugReportInfoActivity extends Activity {
    public static final String TAG = BugReportInfoActivity.class.getSimpleName();

    /** Used for moving bug reports to a new location (e.g. USB drive). */
    private static final int SELECT_DIRECTORY_REQUEST_CODE = 1;

    /** Used to start {@link BugReportActivity} to add audio message. */
    private static final int ADD_AUDIO_MESSAGE_REQUEST_CODE = 2;

    private RecyclerView mRecyclerView;
    private BugInfoAdapter mBugInfoAdapter;
    private RecyclerView.LayoutManager mLayoutManager;
    private NotificationManager mNotificationManager;
    private MetaBugReport mLastSelectedBugReport;
    private BugInfoAdapter.BugInfoViewHolder mLastSelectedBugInfoViewHolder;
    private BugStorageObserver mBugStorageObserver;
    private Config mConfig;
    private boolean mAudioRecordingStarted;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        Preconditions.checkState(Config.isBugReportEnabled(), "BugReport is disabled.");

        super.onCreate(savedInstanceState);
        setContentView(R.layout.bug_report_info_activity);

        mNotificationManager = getSystemService(NotificationManager.class);

        mRecyclerView = findViewById(R.id.rv_bug_report_info);
        mRecyclerView.setHasFixedSize(true);
        // use a linear layout manager
        mLayoutManager = new LinearLayoutManager(this);
        mRecyclerView.setLayoutManager(mLayoutManager);
        mRecyclerView.addItemDecoration(new DividerItemDecoration(mRecyclerView.getContext(),
                DividerItemDecoration.VERTICAL));

        mConfig = new Config();
        mConfig.start();

        mBugInfoAdapter = new BugInfoAdapter(this::onBugReportItemClicked, mConfig);
        mRecyclerView.setAdapter(mBugInfoAdapter);

        mBugStorageObserver = new BugStorageObserver(this, new Handler());

        findViewById(R.id.quit_button).setOnClickListener(this::onQuitButtonClick);
        findViewById(R.id.start_bug_report_button).setOnClickListener(
                this::onStartBugReportButtonClick);
        ((TextView) findViewById(R.id.version_text_view)).setText(
                String.format("v%s", getPackageVersion(this)));

        cancelBugReportFinishedNotification();
    }

    @Override
    protected void onStart() {
        super.onStart();
        new BugReportsLoaderAsyncTask(this).execute();
        // As BugStorageProvider is running under user0, we register using USER_ALL.
        getContentResolver().registerContentObserver(BugStorageProvider.BUGREPORT_CONTENT_URI, true,
                mBugStorageObserver, UserHandle.USER_ALL);
    }

    @Override
    protected void onStop() {
        super.onStop();
        getContentResolver().unregisterContentObserver(mBugStorageObserver);
    }

    /**
     * Dismisses {@link BugReportService#BUGREPORT_FINISHED_NOTIF_ID}, otherwise the notification
     * will stay there forever if this activity opened through the App Launcher.
     */
    private void cancelBugReportFinishedNotification() {
        mNotificationManager.cancel(BugReportService.BUGREPORT_FINISHED_NOTIF_ID);
    }

    private void onBugReportItemClicked(
            int buttonType, MetaBugReport bugReport, BugInfoAdapter.BugInfoViewHolder holder) {
        if (buttonType == BugInfoAdapter.BUTTON_TYPE_UPLOAD) {
            Log.i(TAG, "Uploading " + bugReport.getTimestamp());
            BugStorageUtils.setBugReportStatus(this, bugReport, Status.STATUS_UPLOAD_PENDING, "");
            // Refresh the UI to reflect the new status.
            new BugReportsLoaderAsyncTask(this).execute();
        } else if (buttonType == BugInfoAdapter.BUTTON_TYPE_MOVE) {
            Log.i(TAG, "Moving " + bugReport.getTimestamp());
            mLastSelectedBugReport = bugReport;
            mLastSelectedBugInfoViewHolder = holder;
            startActivityForResult(new Intent(Intent.ACTION_OPEN_DOCUMENT_TREE),
                    SELECT_DIRECTORY_REQUEST_CODE);
        } else if (buttonType == BugInfoAdapter.BUTTON_TYPE_ADD_AUDIO) {
            // Check mAudioRecordingStarted to prevent double click to BUTTON_TYPE_ADD_AUDIO.
            if (!mAudioRecordingStarted) {
                mAudioRecordingStarted = true;
                startActivityForResult(BugReportActivity.buildAddAudioIntent(this, bugReport),
                        ADD_AUDIO_MESSAGE_REQUEST_CODE);
            }
        } else {
            throw new IllegalStateException("unreachable");
        }
    }

    @Override
    protected void onActivityResult(int requestCode, int resultCode, Intent data) {
        super.onActivityResult(requestCode, resultCode, data);
        if (requestCode == SELECT_DIRECTORY_REQUEST_CODE && resultCode == RESULT_OK) {
            int takeFlags =
                    data.getFlags() & (Intent.FLAG_GRANT_READ_URI_PERMISSION
                            | Intent.FLAG_GRANT_WRITE_URI_PERMISSION);
            Uri destDirUri = data.getData();
            getContentResolver().takePersistableUriPermission(destDirUri, takeFlags);
            if (mLastSelectedBugReport == null || mLastSelectedBugInfoViewHolder == null) {
                Log.w(TAG, "No bug report is selected.");
                return;
            }
            MetaBugReport updatedBugReport = BugStorageUtils.setBugReportStatus(this,
                    mLastSelectedBugReport, Status.STATUS_MOVE_IN_PROGRESS, "");
            mBugInfoAdapter.updateBugReportInDataSet(
                    updatedBugReport, mLastSelectedBugInfoViewHolder.getAdapterPosition());
            new AsyncMoveFilesTask(
                this,
                    mBugInfoAdapter,
                    updatedBugReport,
                    mLastSelectedBugInfoViewHolder,
                    destDirUri).execute();
        }
    }

    private void onQuitButtonClick(View view) {
        finish();
    }

    private void onStartBugReportButtonClick(View view) {
        Intent intent = new Intent(this, BugReportActivity.class);
        // Clear top is needed, otherwise multiple BugReportActivity-ies get opened and
        // MediaRecorder crashes.
        intent.setFlags(Intent.FLAG_ACTIVITY_CLEAR_TOP);
        startActivity(intent);
    }

    /**
     * Print the Provider's state into the given stream. This gets invoked if
     * you run "adb shell dumpsys activity BugReportInfoActivity".
     *
     * @param prefix Desired prefix to prepend at each line of output.
     * @param fd The raw file descriptor that the dump is being sent to.
     * @param writer The PrintWriter to which you should dump your state.  This will be
     * closed for you after you return.
     * @param args additional arguments to the dump request.
     */
    public void dump(String prefix, FileDescriptor fd, PrintWriter writer, String[] args) {
        super.dump(prefix, fd, writer, args);
        mConfig.dump(prefix, writer);
    }

    /**
     * Moves bugreport zip to USB drive and updates RecyclerView.
     *
     * <p>It merges bugreport zip file and audio file into one final zip file and moves it.
     */
    private static final class AsyncMoveFilesTask extends AsyncTask<Void, Void, MetaBugReport> {
        private final BugReportInfoActivity mActivity;
        private final MetaBugReport mBugReport;
        private final Uri mDestinationDirUri;
        /** RecyclerView.Adapter that contains all the bug reports. */
        private final BugInfoAdapter mBugInfoAdapter;
        /** ViewHolder for {@link #mBugReport}. */
        private final BugInfoAdapter.BugInfoViewHolder mBugViewHolder;
        private final ContentResolver mResolver;

        AsyncMoveFilesTask(BugReportInfoActivity activity, BugInfoAdapter bugInfoAdapter,
                MetaBugReport bugReport, BugInfoAdapter.BugInfoViewHolder holder,
                Uri destinationDir) {
            mActivity = activity;
            mBugInfoAdapter = bugInfoAdapter;
            mBugReport = bugReport;
            mBugViewHolder = holder;
            mDestinationDirUri = destinationDir;
            mResolver = mActivity.getContentResolver();
        }

        /** Moves the bugreport to the USB drive and returns the updated {@link MetaBugReport}. */
        @Override
        protected MetaBugReport doInBackground(Void... params) {
            try {
                return copyFilesToUsb();
            } catch (IOException e) {
                Log.e(TAG, "Failed to copy bugreport "
                        + mBugReport.getTimestamp() + " to USB", e);
                return BugStorageUtils.setBugReportStatus(
                    mActivity, mBugReport,
                    com.android.car.bugreport.Status.STATUS_MOVE_FAILED, e);
            }
        }

        private MetaBugReport copyFilesToUsb() throws IOException {
            String documentId = DocumentsContract.getTreeDocumentId(mDestinationDirUri);
            Uri parentDocumentUri =
                    DocumentsContract.buildDocumentUriUsingTree(mDestinationDirUri, documentId);
            if (!Strings.isNullOrEmpty(mBugReport.getFilePath())) {
                // There are still old bugreports with deprecated filePath.
                Uri sourceUri = BugStorageProvider.buildUriWithSegment(
                        mBugReport.getId(), BugStorageProvider.URL_SEGMENT_OPEN_FILE);
                copyFileToUsb(
                        new File(mBugReport.getFilePath()).getName(), sourceUri, parentDocumentUri);
            } else {
                mergeFilesAndCopyToUsb(parentDocumentUri);
            }
            Log.d(TAG, "Deleting local bug report files.");
            BugStorageUtils.deleteBugReportFiles(mActivity, mBugReport.getId());
            return BugStorageUtils.setBugReportStatus(mActivity, mBugReport,
                    com.android.car.bugreport.Status.STATUS_MOVE_SUCCESSFUL,
                    "Moved to: " + mDestinationDirUri.getPath());
        }

        private void mergeFilesAndCopyToUsb(Uri parentDocumentUri) throws IOException {
            Uri sourceBugReport = BugStorageProvider.buildUriWithSegment(
                    mBugReport.getId(), BugStorageProvider.URL_SEGMENT_OPEN_BUGREPORT_FILE);
            Uri sourceAudio = BugStorageProvider.buildUriWithSegment(
                    mBugReport.getId(), BugStorageProvider.URL_SEGMENT_OPEN_AUDIO_FILE);
            String mimeType = mResolver.getType(sourceBugReport); // It's a zip file.
            Uri newFileUri = DocumentsContract.createDocument(
                    mResolver, parentDocumentUri, mimeType, mBugReport.getBugReportFileName());
            if (newFileUri == null) {
                throw new IOException(
                        "Unable to create a file " + mBugReport.getBugReportFileName() + " in USB");
            }
            try (InputStream bugReportInput = mResolver.openInputStream(sourceBugReport);
                 AssetFileDescriptor fd = mResolver.openAssetFileDescriptor(newFileUri, "w");
                 OutputStream outputStream = fd.createOutputStream();
                 ZipOutputStream zipOutStream =
                         new ZipOutputStream(new BufferedOutputStream(outputStream))) {
                // Extract bugreport zip file to the final zip file in USB drive.
                ZipInputStream zipInStream = new ZipInputStream(bugReportInput);
                ZipEntry entry;
                while ((entry = zipInStream.getNextEntry()) != null) {
                    ZipUtils.writeInputStreamToZipStream(
                            entry.getName(), zipInStream, zipOutStream);
                }
                // Add audio file to the final zip file.
                if (!Strings.isNullOrEmpty(mBugReport.getAudioFileName())) {
                    try (InputStream audioInput = mResolver.openInputStream(sourceAudio)) {
                        ZipUtils.writeInputStreamToZipStream(
                                mBugReport.getAudioFileName(), audioInput, zipOutStream);
                    }
                }
            }
            try (AssetFileDescriptor fd = mResolver.openAssetFileDescriptor(newFileUri, "w")) {
                // Force sync the written data from memory to the disk.
                fd.getFileDescriptor().sync();
            }
            Log.d(TAG, "Writing to " + newFileUri + " finished");
        }

        private void copyFileToUsb(String filename, Uri sourceUri, Uri parentDocumentUri)
                throws IOException {
            String mimeType = mResolver.getType(sourceUri);
            Uri newFileUri = DocumentsContract.createDocument(
                    mResolver, parentDocumentUri, mimeType, filename);
            if (newFileUri == null) {
                throw new IOException("Unable to create a file " + filename + " in USB");
            }
            try (InputStream input = mResolver.openInputStream(sourceUri);
                 AssetFileDescriptor fd = mResolver.openAssetFileDescriptor(newFileUri, "w")) {
                OutputStream output = fd.createOutputStream();
                ByteStreams.copy(input, output);
                // Force sync the written data from memory to the disk.
                fd.getFileDescriptor().sync();
            }
        }

        @Override
        protected void onPostExecute(MetaBugReport updatedBugReport) {
            // Refresh the UI to reflect the new status.
            mBugInfoAdapter.updateBugReportInDataSet(
                    updatedBugReport, mBugViewHolder.getAdapterPosition());
        }
    }

    /** Asynchronously loads bugreports from {@link BugStorageProvider}. */
    private static final class BugReportsLoaderAsyncTask extends
            AsyncTask<Void, Void, List<MetaBugReport>> {
        private final WeakReference<BugReportInfoActivity> mBugReportInfoActivityWeakReference;

        BugReportsLoaderAsyncTask(BugReportInfoActivity activity) {
            mBugReportInfoActivityWeakReference = new WeakReference<>(activity);
        }

        @Override
        protected List<MetaBugReport> doInBackground(Void... voids) {
            BugReportInfoActivity activity = mBugReportInfoActivityWeakReference.get();
            if (activity == null) {
                Log.w(TAG, "Activity is gone, cancelling BugReportsLoaderAsyncTask.");
                return new ArrayList<>();
            }
            return BugStorageUtils.getAllBugReportsDescending(activity);
        }

        @Override
        protected void onPostExecute(List<MetaBugReport> result) {
            BugReportInfoActivity activity = mBugReportInfoActivityWeakReference.get();
            if (activity == null) {
                Log.w(TAG, "Activity is gone, cancelling onPostExecute.");
                return;
            }
            activity.mBugInfoAdapter.setDataset(result);
        }
    }

    /** Observer for {@link BugStorageProvider}. */
    private static class BugStorageObserver extends ContentObserver {
        private final BugReportInfoActivity mInfoActivity;

        /**
         * Creates a content observer.
         *
         * @param activity A {@link BugReportInfoActivity} instance.
         * @param handler The handler to run {@link #onChange} on, or null if none.
         */
        BugStorageObserver(BugReportInfoActivity activity, Handler handler) {
            super(handler);
            mInfoActivity = activity;
        }

        @Override
        public void onChange(boolean selfChange) {
            new BugReportsLoaderAsyncTask(mInfoActivity).execute();
        }
    }
}
