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

import android.annotation.NonNull;
import android.content.Context;
import android.os.AsyncTask;
import android.text.TextUtils;
import android.util.Log;

import com.google.api.client.extensions.android.http.AndroidHttp;
import com.google.api.client.googleapis.auth.oauth2.GoogleCredential;
import com.google.api.client.http.HttpTransport;
import com.google.api.client.http.InputStreamContent;
import com.google.api.client.json.JsonFactory;
import com.google.api.client.json.jackson2.JacksonFactory;
import com.google.api.services.storage.Storage;
import com.google.api.services.storage.model.StorageObject;
import com.google.common.base.Strings;
import com.google.common.collect.ImmutableMap;

import java.io.BufferedOutputStream;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.util.Collections;
import java.util.List;
import java.util.Map;
import java.util.zip.ZipOutputStream;

/**
 * Uploads a bugreport files to GCS using a simple (no-multipart / no-resume) upload policy.
 *
 * <p>It merges bugreport zip file and audio file into one final zip file and uploads it.
 *
 * <p>Please see {@code res/values/configs.xml} and {@code res/raw/gcs_credentials.json} for the
 * configuration.
 */
class SimpleUploaderAsyncTask extends AsyncTask<Void, Void, Boolean> {
    private static final String TAG = SimpleUploaderAsyncTask.class.getSimpleName();

    private static final String ACCESS_SCOPE =
            "https://www.googleapis.com/auth/devstorage.read_write";

    private static final String STORAGE_METADATA_TITLE = "title";

    private final Context mContext;
    private final Result mResult;

    /**
     * The uploader uploads only one bugreport each time it is called. This interface is
     * used to reschedule upload job, if there are more bugreports waiting.
     *
     * Pass true to reschedule upload job, false not to reschedule.
     */
    interface Result {
        void reschedule(boolean s);
    }

    /** Constructs SimpleUploaderAsyncTask. */
    SimpleUploaderAsyncTask(@NonNull Context context, @NonNull Result result) {
        mContext = context;
        mResult = result;
    }

    private StorageObject uploadSimple(
            Storage storage, MetaBugReport bugReport, String fileName, InputStream data)
            throws IOException {
        InputStreamContent mediaContent = new InputStreamContent("application/zip", data);

        String bucket = mContext.getString(R.string.config_gcs_bucket);
        if (TextUtils.isEmpty(bucket)) {
            throw new RuntimeException("config_gcs_bucket is empty.");
        }

        // Create GCS MetaData.
        Map<String, String> metadata = ImmutableMap.of(
                STORAGE_METADATA_TITLE, bugReport.getTitle()
        );
        StorageObject object = new StorageObject()
                .setBucket(bucket)
                .setName(fileName)
                .setMetadata(metadata)
                .setContentDisposition("attachment");
        Storage.Objects.Insert insertObject = storage.objects().insert(bucket, object,
                mediaContent);

        // The media uploader gzips content by default, and alters the Content-Encoding accordingly.
        // GCS dutifully stores content as-uploaded. This line disables the media uploader behavior,
        // so the service stores exactly what is in the InputStream, without transformation.
        insertObject.getMediaHttpUploader().setDisableGZipContent(true);
        Log.v(TAG, "started uploading object " + fileName + " to bucket " + bucket);
        return insertObject.execute();
    }

    private void upload(MetaBugReport bugReport) throws IOException {
        GoogleCredential credential = GoogleCredential
                .fromStream(mContext.getResources().openRawResource(R.raw.gcs_credentials))
                .createScoped(Collections.singleton(ACCESS_SCOPE));
        Log.v(TAG, "Created credential");
        HttpTransport httpTransport = AndroidHttp.newCompatibleTransport();
        JsonFactory jsonFactory = JacksonFactory.getDefaultInstance();

        Storage storage = new Storage.Builder(httpTransport, jsonFactory, credential)
                .setApplicationName("Bugreportupload/1.0").build();

        File tmpBugReportFile = zipBugReportFiles(bugReport);
        Log.d(TAG, "Uploading file " + tmpBugReportFile);
        try {
            // Upload filename is bugreport filename, although, now it contains the audio message.
            String fileName = bugReport.getBugReportFileName();
            if (Strings.isNullOrEmpty(fileName)) {
                // Old bugreports don't contain getBugReportFileName, fallback to getFilePath.
                fileName = new File(bugReport.getFilePath()).getName();
            }
            try (FileInputStream inputStream = new FileInputStream(tmpBugReportFile)) {
                StorageObject object = uploadSimple(storage, bugReport, fileName, inputStream);
                Log.v(TAG, "finished uploading object " + object.getName() + " file " + fileName);
            }
            File pendingDir = FileUtils.getPendingDir(mContext);
            // Delete only after successful upload; the files are needed for retry.
            if (!Strings.isNullOrEmpty(bugReport.getAudioFileName())) {
                Log.v(TAG, "Deleting file " + bugReport.getAudioFileName());
                new File(pendingDir, bugReport.getAudioFileName()).delete();
            }
            if (!Strings.isNullOrEmpty(bugReport.getBugReportFileName())) {
                Log.v(TAG, "Deleting file " + bugReport.getBugReportFileName());
                new File(pendingDir, bugReport.getBugReportFileName()).delete();
            }
        } finally {
            // Delete the temp file if it's not a MetaBugReport#getFilePath, because it's needed
            // for retry.
            if (Strings.isNullOrEmpty(bugReport.getFilePath())) {
                Log.v(TAG, "Deleting file " + tmpBugReportFile);
                tmpBugReportFile.delete();
            }
        }
    }

    private File zipBugReportFiles(MetaBugReport bugReport) throws IOException {
        if (!Strings.isNullOrEmpty(bugReport.getFilePath())) {
            // Old bugreports still have this field.
            return new File(bugReport.getFilePath());
        }
        File finalZipFile =
                File.createTempFile("bugreport", ".zip", mContext.getCacheDir());
        File pendingDir = FileUtils.getPendingDir(mContext);
        try (ZipOutputStream zipStream = new ZipOutputStream(
                new BufferedOutputStream(new FileOutputStream(finalZipFile)))) {
            ZipUtils.extractZippedFileToZipStream(
                    new File(pendingDir, bugReport.getBugReportFileName()), zipStream);
            ZipUtils.addFileToZipStream(
                    new File(pendingDir, bugReport.getAudioFileName()), zipStream);
        }
        return finalZipFile;
    }

    @Override
    protected void onPostExecute(Boolean success) {
        mResult.reschedule(success);
    }

    /** Returns true is there are more files to upload. */
    @Override
    protected Boolean doInBackground(Void... voids) {
        List<MetaBugReport> bugReports = BugStorageUtils.getUploadPendingBugReports(mContext);

        for (MetaBugReport bugReport : bugReports) {
            try {
                if (isCancelled()) {
                    BugStorageUtils.setUploadRetry(mContext, bugReport, "Upload Job Cancelled");
                    return true;
                }
                upload(bugReport);
                BugStorageUtils.setUploadSuccess(mContext, bugReport);
            } catch (Exception e) {
                Log.e(TAG, String.format("Failed uploading %s - likely a transient error",
                        bugReport.getTimestamp()), e);
                BugStorageUtils.setUploadRetry(mContext, bugReport, e);
            }
        }
        return false;
    }

    @Override
    protected void onCancelled(Boolean success) {
        mResult.reschedule(true);
    }
}
