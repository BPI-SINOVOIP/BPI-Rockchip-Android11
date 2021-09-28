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
import android.annotation.Nullable;
import android.annotation.StringDef;
import android.content.ContentProvider;
import android.content.ContentValues;
import android.content.Context;
import android.content.UriMatcher;
import android.database.Cursor;
import android.database.sqlite.SQLiteDatabase;
import android.database.sqlite.SQLiteOpenHelper;
import android.net.Uri;
import android.os.CancellationSignal;
import android.os.ParcelFileDescriptor;
import android.util.Log;

import com.google.common.base.Preconditions;
import com.google.common.base.Strings;

import java.io.File;
import java.io.FileDescriptor;
import java.io.FileNotFoundException;
import java.io.PrintWriter;
import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.function.Function;


/**
 * Provides a bug storage interface to save and upload bugreports filed from all users.
 * In Android Automotive user 0 runs as the system and all the time, while other users won't once
 * their session ends. This content provider enables bug reports to be uploaded even after
 * user session ends.
 *
 * <p>A bugreport constists of two files: bugreport zip file and audio file. Audio file is added
 * later through notification. {@link SimpleUploaderAsyncTask} merges two files into one zip file
 * before uploading.
 *
 * <p>All files are stored under system user's {@link FileUtils#getPendingDir}.
 */
public class BugStorageProvider extends ContentProvider {
    private static final String TAG = BugStorageProvider.class.getSimpleName();

    private static final String AUTHORITY = "com.android.car.bugreport";
    private static final String BUG_REPORTS_TABLE = "bugreports";

    /** Deletes files associated with a bug report. */
    static final String URL_SEGMENT_DELETE_FILES = "deleteZipFile";
    /** Destructively deletes a bug report. */
    static final String URL_SEGMENT_COMPLETE_DELETE = "completeDelete";
    /** Opens bugreport file of a bug report, uses column {@link #COLUMN_BUGREPORT_FILENAME}. */
    static final String URL_SEGMENT_OPEN_BUGREPORT_FILE = "openBugReportFile";
    /** Opens audio file of a bug report, uses column {@link #URL_MATCHED_OPEN_AUDIO_FILE}. */
    static final String URL_SEGMENT_OPEN_AUDIO_FILE = "openAudioFile";
    /**
     * Opens final bugreport zip file, uses column {@link #COLUMN_FILEPATH}.
     *
     * <p>NOTE: This is the old way of storing final zipped bugreport. In
     * {@code BugStorageProvider#AUDIO_VERSION} {@link #COLUMN_FILEPATH} is dropped. But there are
     * still some devices with this field set.
     */
    static final String URL_SEGMENT_OPEN_FILE = "openFile";

    // URL Matcher IDs.
    private static final int URL_MATCHED_BUG_REPORTS_URI = 1;
    private static final int URL_MATCHED_BUG_REPORT_ID_URI = 2;
    private static final int URL_MATCHED_DELETE_FILES = 3;
    private static final int URL_MATCHED_COMPLETE_DELETE = 4;
    private static final int URL_MATCHED_OPEN_BUGREPORT_FILE = 5;
    private static final int URL_MATCHED_OPEN_AUDIO_FILE = 6;
    private static final int URL_MATCHED_OPEN_FILE = 7;

    @StringDef({
            URL_SEGMENT_DELETE_FILES,
            URL_SEGMENT_COMPLETE_DELETE,
            URL_SEGMENT_OPEN_BUGREPORT_FILE,
            URL_SEGMENT_OPEN_AUDIO_FILE,
            URL_SEGMENT_OPEN_FILE,
    })
    @Retention(RetentionPolicy.SOURCE)
    @interface UriActionSegments {}

    static final Uri BUGREPORT_CONTENT_URI =
            Uri.parse("content://" + AUTHORITY + "/" + BUG_REPORTS_TABLE);

    /** See {@link MetaBugReport} for column descriptions. */
    static final String COLUMN_ID = "_ID";
    static final String COLUMN_USERNAME = "username";
    static final String COLUMN_TITLE = "title";
    static final String COLUMN_TIMESTAMP = "timestamp";
    /** not used anymore */
    static final String COLUMN_DESCRIPTION = "description";
    /** not used anymore, but some devices still might have bugreports with this field set. */
    static final String COLUMN_FILEPATH = "filepath";
    static final String COLUMN_STATUS = "status";
    static final String COLUMN_STATUS_MESSAGE = "message";
    static final String COLUMN_TYPE = "type";
    static final String COLUMN_BUGREPORT_FILENAME = "bugreport_filename";
    static final String COLUMN_AUDIO_FILENAME = "audio_filename";

    private DatabaseHelper mDatabaseHelper;
    private final UriMatcher mUriMatcher;
    private Config mConfig;

    /**
     * A helper class to work with sqlite database.
     */
    private static class DatabaseHelper extends SQLiteOpenHelper {
        private static final String TAG = DatabaseHelper.class.getSimpleName();

        private static final String DATABASE_NAME = "bugreport.db";

        /**
         * All changes in database versions should be recorded here.
         * 1: Initial version.
         * 2: Add integer column details_needed.
         * 3: Add string column audio_filename and bugreport_filename.
         */
        private static final int INITIAL_VERSION = 1;
        private static final int TYPE_VERSION = 2;
        private static final int AUDIO_VERSION = 3;
        private static final int DATABASE_VERSION = AUDIO_VERSION;

        private static final String CREATE_TABLE = "CREATE TABLE " + BUG_REPORTS_TABLE + " ("
                + COLUMN_ID + " INTEGER PRIMARY KEY,"
                + COLUMN_USERNAME + " TEXT,"
                + COLUMN_TITLE + " TEXT,"
                + COLUMN_TIMESTAMP + " TEXT NOT NULL,"
                + COLUMN_DESCRIPTION + " TEXT NULL,"
                + COLUMN_FILEPATH + " TEXT DEFAULT NULL,"
                + COLUMN_STATUS + " INTEGER DEFAULT " + Status.STATUS_WRITE_PENDING.getValue() + ","
                + COLUMN_STATUS_MESSAGE + " TEXT NULL,"
                + COLUMN_TYPE + " INTEGER DEFAULT " + MetaBugReport.TYPE_INTERACTIVE + ","
                + COLUMN_BUGREPORT_FILENAME + " TEXT DEFAULT NULL,"
                + COLUMN_AUDIO_FILENAME + " TEXT DEFAULT NULL"
                + ");";

        DatabaseHelper(Context context) {
            super(context, DATABASE_NAME, null, DATABASE_VERSION);
        }

        @Override
        public void onCreate(SQLiteDatabase db) {
            db.execSQL(CREATE_TABLE);
        }

        @Override
        public void onUpgrade(SQLiteDatabase db, int oldVersion, int newVersion) {
            Log.w(TAG, "Upgrading from " + oldVersion + " to " + newVersion);
            if (oldVersion < TYPE_VERSION) {
                db.execSQL("ALTER TABLE " + BUG_REPORTS_TABLE + " ADD COLUMN "
                        + COLUMN_TYPE + " INTEGER DEFAULT " + MetaBugReport.TYPE_INTERACTIVE);
            }
            if (oldVersion < AUDIO_VERSION) {
                db.execSQL("ALTER TABLE " + BUG_REPORTS_TABLE + " ADD COLUMN "
                        + COLUMN_BUGREPORT_FILENAME + " TEXT DEFAULT NULL");
                db.execSQL("ALTER TABLE " + BUG_REPORTS_TABLE + " ADD COLUMN "
                        + COLUMN_AUDIO_FILENAME + " TEXT DEFAULT NULL");
            }
        }
    }

    /**
     * Builds an {@link Uri} that points to the single bug report and performs an action
     * defined by given URI segment.
     */
    static Uri buildUriWithSegment(int bugReportId, @UriActionSegments String segment) {
        return Uri.parse("content://" + AUTHORITY + "/" + BUG_REPORTS_TABLE + "/"
                + segment + "/" + bugReportId);
    }

    public BugStorageProvider() {
        mUriMatcher = new UriMatcher(UriMatcher.NO_MATCH);
        mUriMatcher.addURI(AUTHORITY, BUG_REPORTS_TABLE, URL_MATCHED_BUG_REPORTS_URI);
        mUriMatcher.addURI(AUTHORITY, BUG_REPORTS_TABLE + "/#", URL_MATCHED_BUG_REPORT_ID_URI);
        mUriMatcher.addURI(
                AUTHORITY, BUG_REPORTS_TABLE + "/" + URL_SEGMENT_DELETE_FILES + "/#",
                URL_MATCHED_DELETE_FILES);
        mUriMatcher.addURI(
                AUTHORITY, BUG_REPORTS_TABLE + "/" + URL_SEGMENT_COMPLETE_DELETE + "/#",
                URL_MATCHED_COMPLETE_DELETE);
        mUriMatcher.addURI(
                AUTHORITY, BUG_REPORTS_TABLE + "/" + URL_SEGMENT_OPEN_BUGREPORT_FILE + "/#",
                URL_MATCHED_OPEN_BUGREPORT_FILE);
        mUriMatcher.addURI(
                AUTHORITY, BUG_REPORTS_TABLE + "/" + URL_SEGMENT_OPEN_AUDIO_FILE + "/#",
                URL_MATCHED_OPEN_AUDIO_FILE);
        mUriMatcher.addURI(
                AUTHORITY, BUG_REPORTS_TABLE + "/" + URL_SEGMENT_OPEN_FILE + "/#",
                URL_MATCHED_OPEN_FILE);
    }

    @Override
    public boolean onCreate() {
        if (!Config.isBugReportEnabled()) {
            return false;
        }
        mDatabaseHelper = new DatabaseHelper(getContext());
        mConfig = new Config();
        mConfig.start();
        return true;
    }

    @Override
    public Cursor query(
            @NonNull Uri uri,
            @Nullable String[] projection,
            @Nullable String selection,
            @Nullable String[] selectionArgs,
            @Nullable String sortOrder) {
        return query(uri, projection, selection, selectionArgs, sortOrder, null);
    }

    @Nullable
    @Override
    public Cursor query(
            @NonNull Uri uri,
            @Nullable String[] projection,
            @Nullable String selection,
            @Nullable String[] selectionArgs,
            @Nullable String sortOrder,
            @Nullable CancellationSignal cancellationSignal) {
        Preconditions.checkState(Config.isBugReportEnabled(), "BugReport is disabled.");
        String table;
        switch (mUriMatcher.match(uri)) {
            // returns the list of bugreports that match the selection criteria.
            case URL_MATCHED_BUG_REPORTS_URI:
                table = BUG_REPORTS_TABLE;
                break;
            //  returns the bugreport that match the id.
            case URL_MATCHED_BUG_REPORT_ID_URI:
                table = BUG_REPORTS_TABLE;
                if (selection != null || selectionArgs != null) {
                    throw new IllegalArgumentException("selection is not allowed for "
                            + URL_MATCHED_BUG_REPORT_ID_URI);
                }
                selection = COLUMN_ID + "=?";
                selectionArgs = new String[]{ uri.getLastPathSegment() };
                break;
            default:
                throw new IllegalArgumentException("Unknown URL " + uri);
        }
        SQLiteDatabase db = mDatabaseHelper.getReadableDatabase();
        Cursor cursor = db.query(false, table, null, selection, selectionArgs, null, null,
                sortOrder, null, cancellationSignal);
        cursor.setNotificationUri(getContext().getContentResolver(), uri);
        return cursor;
    }

    @Nullable
    @Override
    public Uri insert(@NonNull Uri uri, @Nullable ContentValues values) {
        Preconditions.checkState(Config.isBugReportEnabled(), "BugReport is disabled.");
        String table;
        if (values == null) {
            throw new IllegalArgumentException("values cannot be null");
        }
        switch (mUriMatcher.match(uri)) {
            case URL_MATCHED_BUG_REPORTS_URI:
                table = BUG_REPORTS_TABLE;
                break;
            default:
                throw new IllegalArgumentException("unknown uri" + uri);
        }
        SQLiteDatabase db = mDatabaseHelper.getWritableDatabase();
        long rowId = db.insert(table, null, values);
        if (rowId > 0) {
            Uri resultUri = Uri.parse("content://" + AUTHORITY + "/" + table + "/" + rowId);
            // notify registered content observers
            getContext().getContentResolver().notifyChange(resultUri, null);
            return resultUri;
        }
        return null;
    }

    @Nullable
    @Override
    public String getType(@NonNull Uri uri) {
        switch (mUriMatcher.match(uri)) {
            case URL_MATCHED_OPEN_BUGREPORT_FILE:
            case URL_MATCHED_OPEN_FILE:
                return "application/zip";
            case URL_MATCHED_OPEN_AUDIO_FILE:
                return "audio/3gpp";
            default:
                throw new IllegalArgumentException("unknown uri:" + uri);
        }
    }

    @Override
    public int delete(
            @NonNull Uri uri, @Nullable String selection, @Nullable String[] selectionArgs) {
        Preconditions.checkState(Config.isBugReportEnabled(), "BugReport is disabled.");
        SQLiteDatabase db = mDatabaseHelper.getReadableDatabase();
        switch (mUriMatcher.match(uri)) {
            case URL_MATCHED_DELETE_FILES:
                if (selection != null || selectionArgs != null) {
                    throw new IllegalArgumentException("selection is not allowed for "
                            + URL_MATCHED_DELETE_FILES);
                }
                if (deleteFilesFor(getBugReportFromUri(uri))) {
                    getContext().getContentResolver().notifyChange(uri, null);
                    return 1;
                }
                return 0;
            case URL_MATCHED_COMPLETE_DELETE:
                if (selection != null || selectionArgs != null) {
                    throw new IllegalArgumentException("selection is not allowed for "
                            + URL_MATCHED_COMPLETE_DELETE);
                }
                selection = COLUMN_ID + " = ?";
                selectionArgs = new String[]{uri.getLastPathSegment()};
                // Ignore the results of zip file deletion, possibly it wasn't even created.
                deleteFilesFor(getBugReportFromUri(uri));
                getContext().getContentResolver().notifyChange(uri, null);
                return db.delete(BUG_REPORTS_TABLE, selection, selectionArgs);
            default:
                throw new IllegalArgumentException("Unknown URL " + uri);
        }
    }

    @Override
    public int update(
            @NonNull Uri uri,
            @Nullable ContentValues values,
            @Nullable String selection,
            @Nullable String[] selectionArgs) {
        Preconditions.checkState(Config.isBugReportEnabled(), "BugReport is disabled.");
        if (values == null) {
            throw new IllegalArgumentException("values cannot be null");
        }
        String table;
        switch (mUriMatcher.match(uri)) {
            case URL_MATCHED_BUG_REPORTS_URI:
                table = BUG_REPORTS_TABLE;
                break;
            default:
                throw new IllegalArgumentException("Unknown URL " + uri);
        }
        SQLiteDatabase db = mDatabaseHelper.getWritableDatabase();
        int rowCount = db.update(table, values, selection, selectionArgs);
        if (rowCount > 0) {
            // notify registered content observers
            getContext().getContentResolver().notifyChange(uri, null);
        }
        Integer status = values.getAsInteger(COLUMN_STATUS);
        // When the status is set to STATUS_UPLOAD_PENDING, we schedule an UploadJob under the
        // current user, which is the primary user.
        if (status != null && status.equals(Status.STATUS_UPLOAD_PENDING.getValue())) {
            JobSchedulingUtils.scheduleUploadJob(BugStorageProvider.this.getContext());
        }
        return rowCount;
    }

    /**
     * This is called when a file is opened.
     *
     * <p>See {@link BugStorageUtils#openBugReportFileToWrite},
     * {@link BugStorageUtils#openAudioMessageFileToWrite}.
     */
    @Nullable
    @Override
    public ParcelFileDescriptor openFile(@NonNull Uri uri, @NonNull String mode)
            throws FileNotFoundException {
        Preconditions.checkState(Config.isBugReportEnabled(), "BugReport is disabled.");
        Function<MetaBugReport, String> fileNameExtractor;
        switch (mUriMatcher.match(uri)) {
            case URL_MATCHED_OPEN_BUGREPORT_FILE:
                fileNameExtractor = MetaBugReport::getBugReportFileName;
                break;
            case URL_MATCHED_OPEN_AUDIO_FILE:
                fileNameExtractor = MetaBugReport::getAudioFileName;
                break;
            case URL_MATCHED_OPEN_FILE:
                File file = new File(getBugReportFromUri(uri).getFilePath());
                Log.v(TAG, "Opening file " + file + " with mode " + mode);
                return ParcelFileDescriptor.open(file, ParcelFileDescriptor.parseMode(mode));
            default:
                throw new IllegalArgumentException("unknown uri:" + uri);
        }
        // URI contains bugreport ID as the last segment, see the matched urls.
        MetaBugReport bugReport = getBugReportFromUri(uri);
        File file = new File(
                FileUtils.getPendingDir(getContext()), fileNameExtractor.apply(bugReport));
        Log.v(TAG, "Opening file " + file + " with mode " + mode);
        int modeBits = ParcelFileDescriptor.parseMode(mode);
        return ParcelFileDescriptor.open(file, modeBits);
    }

    private MetaBugReport getBugReportFromUri(@NonNull Uri uri) {
        int bugreportId = Integer.parseInt(uri.getLastPathSegment());
        return BugStorageUtils.findBugReport(getContext(), bugreportId)
                .orElseThrow(() -> new IllegalArgumentException("No record found for " + uri));
    }

    /**
     * Print the Provider's state into the given stream. This gets invoked if
     * you run "dumpsys activity provider com.android.car.bugreport/.BugStorageProvider".
     *
     * @param fd The raw file descriptor that the dump is being sent to.
     * @param writer The PrintWriter to which you should dump your state.  This will be
     * closed for you after you return.
     * @param args additional arguments to the dump request.
     */
    public void dump(FileDescriptor fd, PrintWriter writer, String[] args) {
        writer.println("BugStorageProvider:");
        mConfig.dump(/* prefix= */ "  ", writer);
    }

    private boolean deleteFilesFor(MetaBugReport bugReport) {
        if (!Strings.isNullOrEmpty(bugReport.getFilePath())) {
            // Old bugreports have only filePath.
            return new File(bugReport.getFilePath()).delete();
        }
        File pendingDir = FileUtils.getPendingDir(getContext());
        boolean result = true;
        if (!Strings.isNullOrEmpty(bugReport.getAudioFileName())) {
            result = new File(pendingDir, bugReport.getAudioFileName()).delete();
        }
        if (!Strings.isNullOrEmpty(bugReport.getBugReportFileName())) {
            result = result && new File(pendingDir, bugReport.getBugReportFileName()).delete();
        }
        return result;
    }
}
