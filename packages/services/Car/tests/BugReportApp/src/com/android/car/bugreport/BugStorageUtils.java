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

import static com.android.car.bugreport.BugStorageProvider.COLUMN_AUDIO_FILENAME;
import static com.android.car.bugreport.BugStorageProvider.COLUMN_BUGREPORT_FILENAME;
import static com.android.car.bugreport.BugStorageProvider.COLUMN_FILEPATH;
import static com.android.car.bugreport.BugStorageProvider.COLUMN_ID;
import static com.android.car.bugreport.BugStorageProvider.COLUMN_STATUS;
import static com.android.car.bugreport.BugStorageProvider.COLUMN_STATUS_MESSAGE;
import static com.android.car.bugreport.BugStorageProvider.COLUMN_TIMESTAMP;
import static com.android.car.bugreport.BugStorageProvider.COLUMN_TITLE;
import static com.android.car.bugreport.BugStorageProvider.COLUMN_TYPE;
import static com.android.car.bugreport.BugStorageProvider.COLUMN_USERNAME;

import android.annotation.NonNull;
import android.annotation.Nullable;
import android.content.ContentResolver;
import android.content.ContentValues;
import android.content.Context;
import android.database.Cursor;
import android.net.Uri;
import android.util.Log;

import com.google.api.client.auth.oauth2.TokenResponseException;
import com.google.common.base.Strings;

import java.io.FileNotFoundException;
import java.io.InputStream;
import java.io.OutputStream;
import java.text.DateFormat;
import java.text.SimpleDateFormat;
import java.time.Instant;
import java.util.ArrayList;
import java.util.Date;
import java.util.List;
import java.util.Optional;

/**
 * A class that hides details when communicating with the bug storage provider.
 */
final class BugStorageUtils {
    private static final String TAG = BugStorageUtils.class.getSimpleName();

    /**
     * When time/time-zone set incorrectly, Google API returns "400: invalid_grant" error with
     * description containing this text.
     */
    private static final String CLOCK_SKEW_ERROR = "clock with skew to account";

    /** When time/time-zone set incorrectly, Google API returns this error. */
    private static final String INVALID_GRANT = "invalid_grant";

    private static final DateFormat TIMESTAMP_FORMAT = new SimpleDateFormat("yyyy-MM-dd_HH-mm-ss");

    /**
     * Creates a new {@link Status#STATUS_WRITE_PENDING} bug report record in a local sqlite
     * database.
     *
     * @param context   - an application context.
     * @param title     - title of the bug report.
     * @param timestamp - timestamp when the bug report was initiated.
     * @param username  - current user name. Note, it's a user name, not an account name.
     * @param type      - bug report type, {@link MetaBugReport.BugReportType}.
     * @return an instance of {@link MetaBugReport} that was created in a database.
     */
    @NonNull
    static MetaBugReport createBugReport(
            @NonNull Context context,
            @NonNull String title,
            @NonNull String timestamp,
            @NonNull String username,
            @MetaBugReport.BugReportType int type) {
        // insert bug report username and title
        ContentValues values = new ContentValues();
        values.put(COLUMN_TITLE, title);
        values.put(COLUMN_TIMESTAMP, timestamp);
        values.put(COLUMN_USERNAME, username);
        values.put(COLUMN_TYPE, type);

        ContentResolver r = context.getContentResolver();
        Uri uri = r.insert(BugStorageProvider.BUGREPORT_CONTENT_URI, values);
        return findBugReport(context, Integer.parseInt(uri.getLastPathSegment())).get();
    }

    /** Returns an output stream to write the zipped file to. */
    @NonNull
    static OutputStream openBugReportFileToWrite(
            @NonNull Context context, @NonNull MetaBugReport metaBugReport)
            throws FileNotFoundException {
        ContentResolver r = context.getContentResolver();
        return r.openOutputStream(BugStorageProvider.buildUriWithSegment(
                metaBugReport.getId(), BugStorageProvider.URL_SEGMENT_OPEN_BUGREPORT_FILE));
    }

    /** Returns an output stream to write the audio message file to. */
    static OutputStream openAudioMessageFileToWrite(
            @NonNull Context context, @NonNull MetaBugReport metaBugReport)
            throws FileNotFoundException {
        ContentResolver r = context.getContentResolver();
        return r.openOutputStream(BugStorageProvider.buildUriWithSegment(
                metaBugReport.getId(), BugStorageProvider.URL_SEGMENT_OPEN_AUDIO_FILE));
    }

    /**
     * Returns an input stream to read the final zip file from.
     *
     * <p>NOTE: This is the old way of storing final zipped bugreport. See
     * {@link BugStorageProvider#URL_SEGMENT_OPEN_FILE} for more info.
     */
    static InputStream openFileToRead(Context context, MetaBugReport bug)
            throws FileNotFoundException {
        return context.getContentResolver().openInputStream(
                BugStorageProvider.buildUriWithSegment(
                        bug.getId(), BugStorageProvider.URL_SEGMENT_OPEN_FILE));
    }

    /** Returns an input stream to read the bug report zip file from. */
    static InputStream openBugReportFileToRead(Context context, MetaBugReport bug)
            throws FileNotFoundException {
        return context.getContentResolver().openInputStream(
                BugStorageProvider.buildUriWithSegment(
                        bug.getId(), BugStorageProvider.URL_SEGMENT_OPEN_BUGREPORT_FILE));
    }

    /** Returns an input stream to read the audio file from. */
    static InputStream openAudioFileToRead(Context context, MetaBugReport bug)
            throws FileNotFoundException {
        return context.getContentResolver().openInputStream(
                BugStorageProvider.buildUriWithSegment(
                        bug.getId(), BugStorageProvider.URL_SEGMENT_OPEN_AUDIO_FILE));
    }

    /**
     * Deletes {@link MetaBugReport} record from a local database and deletes the associated file.
     *
     * <p>WARNING: destructive operation.
     *
     * @param context     - an application context.
     * @param bugReportId - a bug report id.
     * @return true if the record was deleted.
     */
    static boolean completeDeleteBugReport(@NonNull Context context, int bugReportId) {
        ContentResolver r = context.getContentResolver();
        return r.delete(BugStorageProvider.buildUriWithSegment(
                bugReportId, BugStorageProvider.URL_SEGMENT_COMPLETE_DELETE), null, null) == 1;
    }

    /** Deletes all files for given bugreport id; doesn't delete sqlite3 record. */
    static boolean deleteBugReportFiles(@NonNull Context context, int bugReportId) {
        ContentResolver r = context.getContentResolver();
        return r.delete(BugStorageProvider.buildUriWithSegment(
                bugReportId, BugStorageProvider.URL_SEGMENT_DELETE_FILES), null, null) == 1;
    }

    /**
     * Returns all the bugreports that are waiting to be uploaded.
     */
    @NonNull
    public static List<MetaBugReport> getUploadPendingBugReports(@NonNull Context context) {
        String selection = COLUMN_STATUS + "=?";
        String[] selectionArgs = new String[]{
                Integer.toString(Status.STATUS_UPLOAD_PENDING.getValue())};
        return getBugreports(context, selection, selectionArgs, null);
    }

    /**
     * Returns all bugreports in descending order by the ID field. ID is the index in the
     * database.
     */
    @NonNull
    public static List<MetaBugReport> getAllBugReportsDescending(@NonNull Context context) {
        return getBugreports(context, null, null, COLUMN_ID + " DESC");
    }

    /** Returns {@link MetaBugReport} for given bugreport id. */
    static Optional<MetaBugReport> findBugReport(Context context, int bugreportId) {
        String selection = COLUMN_ID + " = ?";
        String[] selectionArgs = new String[]{Integer.toString(bugreportId)};
        List<MetaBugReport> bugs = BugStorageUtils.getBugreports(
                context, selection, selectionArgs, null);
        if (bugs.isEmpty()) {
            return Optional.empty();
        }
        return Optional.of(bugs.get(0));
    }

    private static List<MetaBugReport> getBugreports(
            Context context, String selection, String[] selectionArgs, String order) {
        ArrayList<MetaBugReport> bugReports = new ArrayList<>();
        String[] projection = {
                COLUMN_ID,
                COLUMN_USERNAME,
                COLUMN_TITLE,
                COLUMN_TIMESTAMP,
                COLUMN_BUGREPORT_FILENAME,
                COLUMN_AUDIO_FILENAME,
                COLUMN_FILEPATH,
                COLUMN_STATUS,
                COLUMN_STATUS_MESSAGE,
                COLUMN_TYPE};
        ContentResolver r = context.getContentResolver();
        Cursor c = r.query(BugStorageProvider.BUGREPORT_CONTENT_URI, projection,
                selection, selectionArgs, order);

        int count = (c != null) ? c.getCount() : 0;

        if (count > 0) c.moveToFirst();
        for (int i = 0; i < count; i++) {
            MetaBugReport meta = MetaBugReport.builder()
                    .setId(getInt(c, COLUMN_ID))
                    .setTimestamp(getString(c, COLUMN_TIMESTAMP))
                    .setUserName(getString(c, COLUMN_USERNAME))
                    .setTitle(getString(c, COLUMN_TITLE))
                    .setBugReportFileName(getString(c, COLUMN_BUGREPORT_FILENAME))
                    .setAudioFileName(getString(c, COLUMN_AUDIO_FILENAME))
                    .setFilePath(getString(c, COLUMN_FILEPATH))
                    .setStatus(getInt(c, COLUMN_STATUS))
                    .setStatusMessage(getString(c, COLUMN_STATUS_MESSAGE))
                    .setType(getInt(c, COLUMN_TYPE))
                    .build();
            bugReports.add(meta);
            c.moveToNext();
        }
        if (c != null) c.close();
        return bugReports;
    }

    /**
     * returns 0 if the column is not found. Otherwise returns the column value.
     */
    private static int getInt(Cursor c, String colName) {
        int colIndex = c.getColumnIndex(colName);
        if (colIndex == -1) {
            Log.w(TAG, "Column " + colName + " not found.");
            return 0;
        }
        return c.getInt(colIndex);
    }

    /**
     * Returns the column value. If the column is not found returns empty string.
     */
    private static String getString(Cursor c, String colName) {
        int colIndex = c.getColumnIndex(colName);
        if (colIndex == -1) {
            Log.w(TAG, "Column " + colName + " not found.");
            return "";
        }
        return Strings.nullToEmpty(c.getString(colIndex));
    }

    /**
     * Sets bugreport status to uploaded successfully.
     */
    public static void setUploadSuccess(Context context, MetaBugReport bugReport) {
        setBugReportStatus(context, bugReport, Status.STATUS_UPLOAD_SUCCESS,
                "Upload time: " + currentTimestamp());
    }

    /**
     * Sets bugreport status pending, and update the message to last exception message.
     *
     * <p>Used when a transient error has occurred.
     */
    public static void setUploadRetry(Context context, MetaBugReport bugReport, Exception e) {
        setBugReportStatus(context, bugReport, Status.STATUS_UPLOAD_PENDING,
                getRootCauseMessage(e));
    }

    /**
     * Sets bugreport status pending and update the message to last message.
     *
     * <p>Used when a transient error has occurred.
     */
    public static void setUploadRetry(Context context, MetaBugReport bugReport, String msg) {
        setBugReportStatus(context, bugReport, Status.STATUS_UPLOAD_PENDING, msg);
    }

    /**
     * Sets {@link MetaBugReport} status {@link Status#STATUS_EXPIRED}.
     * Deletes the associated zip file from disk.
     *
     * @return true if succeeded.
     */
    static boolean expireBugReport(@NonNull Context context,
            @NonNull MetaBugReport metaBugReport, @NonNull Instant expiredAt) {
        metaBugReport = setBugReportStatus(
                context, metaBugReport, Status.STATUS_EXPIRED, "Expired on " + expiredAt);
        if (metaBugReport.getStatus() != Status.STATUS_EXPIRED.getValue()) {
            return false;
        }
        return deleteBugReportFiles(context, metaBugReport.getId());
    }

    /** Gets the root cause of the error. */
    @NonNull
    private static String getRootCauseMessage(@Nullable Throwable t) {
        if (t == null) {
            return "No error";
        } else if (t instanceof TokenResponseException) {
            TokenResponseException ex = (TokenResponseException) t;
            if (ex.getDetails().getError().equals(INVALID_GRANT)
                    && ex.getDetails().getErrorDescription().contains(CLOCK_SKEW_ERROR)) {
                return "Auth error. Check if time & time-zone is correct.";
            }
        }
        while (t.getCause() != null) t = t.getCause();
        return t.getMessage();
    }

    /**
     * Updates bug report record status.
     *
     * <p>NOTE: When status is set to STATUS_UPLOAD_PENDING, BugStorageProvider automatically
     * schedules the bugreport to be uploaded.
     *
     * @return Updated {@link MetaBugReport}.
     */
    static MetaBugReport setBugReportStatus(
            Context context, MetaBugReport bugReport, Status status, String message) {
        return update(context, bugReport.toBuilder()
                .setStatus(status.getValue())
                .setStatusMessage(message)
                .build());
    }

    /**
     * Updates bug report record status.
     *
     * <p>NOTE: When status is set to STATUS_UPLOAD_PENDING, BugStorageProvider automatically
     * schedules the bugreport to be uploaded.
     *
     * @return Updated {@link MetaBugReport}.
     */
    static MetaBugReport setBugReportStatus(
            Context context, MetaBugReport bugReport, Status status, Exception e) {
        return setBugReportStatus(context, bugReport, status, getRootCauseMessage(e));
    }

    /**
     * Updates the bugreport and returns the updated version.
     *
     * <p>NOTE: doesn't update all the fields.
     */
    static MetaBugReport update(Context context, MetaBugReport bugReport) {
        // Update only necessary fields.
        ContentValues values = new ContentValues();
        values.put(COLUMN_BUGREPORT_FILENAME, bugReport.getBugReportFileName());
        values.put(COLUMN_AUDIO_FILENAME, bugReport.getAudioFileName());
        values.put(COLUMN_STATUS, bugReport.getStatus());
        values.put(COLUMN_STATUS_MESSAGE, bugReport.getStatusMessage());
        String where = COLUMN_ID + "=" + bugReport.getId();
        context.getContentResolver().update(
                BugStorageProvider.BUGREPORT_CONTENT_URI, values, where, null);
        return findBugReport(context, bugReport.getId()).orElseThrow(
                () -> new IllegalArgumentException("Bug " + bugReport.getId() + " not found"));
    }

    private static String currentTimestamp() {
        return TIMESTAMP_FORMAT.format(new Date());
    }
}
