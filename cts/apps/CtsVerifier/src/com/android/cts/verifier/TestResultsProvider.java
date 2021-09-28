/*
 * Copyright (C) 2010 The Android Open Source Project
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

package com.android.cts.verifier;

import android.app.backup.BackupManager;
import android.content.ContentProvider;
import android.content.ContentResolver;
import android.content.ContentValues;
import android.content.Context;
import android.content.UriMatcher;
import android.database.Cursor;
import android.database.MatrixCursor;
import android.database.sqlite.SQLiteDatabase;
import android.database.sqlite.SQLiteOpenHelper;
import android.database.sqlite.SQLiteQueryBuilder;
import android.net.Uri;
import android.os.ParcelFileDescriptor;

import com.android.compatibility.common.util.ReportLog;

import java.io.ByteArrayOutputStream;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileNotFoundException;
import java.io.IOException;
import java.io.ObjectOutputStream;
import java.util.Arrays;
import java.util.Comparator;

import androidx.annotation.NonNull;

/**
 * {@link ContentProvider} that provides read and write access to the test results.
 */
public class TestResultsProvider extends ContentProvider {

    static final String _ID = "_id";
    /** String name of the test like "com.android.cts.verifier.foo.FooTestActivity" */
    static final String COLUMN_TEST_NAME = "testname";
    /** Integer test result corresponding to constants in {@link TestResult}. */
    static final String COLUMN_TEST_RESULT = "testresult";
    /** Boolean indicating whether the test info has been seen. */
    static final String COLUMN_TEST_INFO_SEEN = "testinfoseen";
    /** String containing the test's details. */
    static final String COLUMN_TEST_DETAILS = "testdetails";
    /** ReportLog containing the test result metrics. */
    static final String COLUMN_TEST_METRICS = "testmetrics";
    /** TestResultHistory containing the test run histories. */
    static final String COLUMN_TEST_RESULT_HISTORY = "testresulthistory";

    /**
     * Report saved location
     */
    private static final String REPORTS_PATH = "reports";
    private static final String RESULTS_PATH = "results";
    private static final UriMatcher URI_MATCHER = new UriMatcher(UriMatcher.NO_MATCH);
    private static final int RESULTS_ALL = 1;
    private static final int RESULTS_ID = 2;
    private static final int RESULTS_TEST_NAME = 3;
    private static final int REPORT = 4;
    private static final int REPORT_ROW = 5;
    private static final int REPORT_FILE_NAME = 6;
    private static final int REPORT_LATEST = 7;
    private static final String TABLE_NAME = "results";
    private SQLiteOpenHelper mOpenHelper;
    private BackupManager mBackupManager;

    /**
     * Get the URI from the result content.
     *
     * @param context
     * @return Uri
     */
    public static Uri getResultContentUri(Context context) {
        final String packageName = context.getPackageName();
        final Uri contentUri = Uri.parse("content://" + packageName + ".testresultsprovider");
        return Uri.withAppendedPath(contentUri, RESULTS_PATH);
    }

    /**
     * Get the URI from the test name.
     *
     * @param context
     * @param testName
     * @return Uri
     */
    public static Uri getTestNameUri(Context context) {
        final String testName = context.getClass().getName();
        return Uri.withAppendedPath(getResultContentUri(context), testName);
    }

    static void setTestResult(Context context, String testName, int testResult,
        String testDetails, ReportLog reportLog, TestResultHistoryCollection historyCollection) {
        ContentValues values = new ContentValues(2);
        values.put(TestResultsProvider.COLUMN_TEST_RESULT, testResult);
        values.put(TestResultsProvider.COLUMN_TEST_NAME, testName);
        values.put(TestResultsProvider.COLUMN_TEST_DETAILS, testDetails);
        values.put(TestResultsProvider.COLUMN_TEST_METRICS, serialize(reportLog));
        values.put(TestResultsProvider.COLUMN_TEST_RESULT_HISTORY, serialize(historyCollection));

        final Uri uri = getResultContentUri(context);
        ContentResolver resolver = context.getContentResolver();
        int numUpdated = resolver.update(uri, values,
                TestResultsProvider.COLUMN_TEST_NAME + " = ?",
                new String[]{testName});

        if (numUpdated == 0) {
            resolver.insert(uri, values);
        }
    }

    private static byte[] serialize(Object o) {
        ByteArrayOutputStream byteStream = new ByteArrayOutputStream();
        ObjectOutputStream objectOutput = null;
        try {
            objectOutput = new ObjectOutputStream(byteStream);
            objectOutput.writeObject(o);
            return byteStream.toByteArray();
        } catch (IOException e) {
            return null;
        } finally {
            try {
                if (objectOutput != null) {
                    objectOutput.close();
                }
                byteStream.close();
            } catch (IOException e) {
                // Ignore close exception.
            }
        }
    }

    @Override
    public boolean onCreate() {
        final String authority = getContext().getPackageName() + ".testresultsprovider";

        URI_MATCHER.addURI(authority, RESULTS_PATH, RESULTS_ALL);
        URI_MATCHER.addURI(authority, RESULTS_PATH + "/#", RESULTS_ID);
        URI_MATCHER.addURI(authority, RESULTS_PATH + "/*", RESULTS_TEST_NAME);
        URI_MATCHER.addURI(authority, REPORTS_PATH, REPORT);
        URI_MATCHER.addURI(authority, REPORTS_PATH + "/latest", REPORT_LATEST);
        URI_MATCHER.addURI(authority, REPORTS_PATH + "/#", REPORT_ROW);
        URI_MATCHER.addURI(authority, REPORTS_PATH + "/*", REPORT_FILE_NAME);

        mOpenHelper = new TestResultsOpenHelper(getContext());
        mBackupManager = new BackupManager(getContext());
        return false;
    }

    @Override
    public Cursor query(Uri uri, String[] projection, String selection, String[] selectionArgs,
                        String sortOrder) {
        SQLiteQueryBuilder query = new SQLiteQueryBuilder();
        query.setTables(TABLE_NAME);

        int match = URI_MATCHER.match(uri);
        switch (match) {
            case RESULTS_ALL:
                break;

            case RESULTS_ID:
                query.appendWhere(_ID);
                query.appendWhere("=");
                query.appendWhere(uri.getPathSegments().get(1));
                break;

            case RESULTS_TEST_NAME:
                query.appendWhere(COLUMN_TEST_NAME);
                query.appendWhere("=");
                query.appendWhere("\"" + uri.getPathSegments().get(1) + "\"");
                break;

            case REPORT:
                final MatrixCursor cursor = new MatrixCursor(new String[]{"filename"});
                for (String filename : getFileList()) {
                    cursor.addRow(new Object[]{filename});
                }
                return cursor;

            case REPORT_FILE_NAME:
            case REPORT_ROW:
            case REPORT_LATEST:
                throw new IllegalArgumentException(
                        "Report query not supported. Use content read.");

            default:
                throw new IllegalArgumentException("Unknown URI: " + uri);
        }

        SQLiteDatabase db = mOpenHelper.getReadableDatabase();
        return query.query(db, projection, selection, selectionArgs, null, null, sortOrder);
    }

    @Override
    public Uri insert(Uri uri, ContentValues values) {
        int match = URI_MATCHER.match(uri);
        switch (match) {
            case REPORT:
                throw new IllegalArgumentException(
                        "Report insert not supported. Use content query.");
            case REPORT_FILE_NAME:
            case REPORT_ROW:
            case REPORT_LATEST:
                throw new IllegalArgumentException(
                        "Report insert not supported. Use content read.");
            default:
                break;
        }
        SQLiteDatabase db = mOpenHelper.getWritableDatabase();
        long id = db.insert(TABLE_NAME, null, values);
        getContext().getContentResolver().notifyChange(uri, null);
        mBackupManager.dataChanged();
        return Uri.withAppendedPath(getResultContentUri(getContext()), "" + id);
    }

    @Override
    public int update(Uri uri, ContentValues values, String selection, String[] selectionArgs) {
        int match = URI_MATCHER.match(uri);
        switch (match) {
            case RESULTS_ALL:
                break;

            case RESULTS_ID:
                String idSelection = _ID + "=" + uri.getPathSegments().get(1);
                if (selection != null && selection.length() > 0) {
                    selection = idSelection + " AND " + selection;
                } else {
                    selection = idSelection;
                }
                break;

            case RESULTS_TEST_NAME:
                String testNameSelection = COLUMN_TEST_NAME + "=\""
                        + uri.getPathSegments().get(1) + "\"";
                if (selection != null && selection.length() > 0) {
                    selection = testNameSelection + " AND " + selection;
                } else {
                    selection = testNameSelection;
                }
                break;
            case REPORT:
                throw new IllegalArgumentException(
                        "Report update not supported. Use content query.");
            case REPORT_FILE_NAME:
            case REPORT_ROW:
            case REPORT_LATEST:
                throw new IllegalArgumentException(
                        "Report update not supported. Use content read.");
            default:
                throw new IllegalArgumentException("Unknown URI: " + uri);
        }

        SQLiteDatabase db = mOpenHelper.getWritableDatabase();
        int numUpdated = db.update(TABLE_NAME, values, selection, selectionArgs);
        if (numUpdated > 0) {
            getContext().getContentResolver().notifyChange(uri, null);
            mBackupManager.dataChanged();
        }
        return numUpdated;
    }

    @Override
    public int delete(Uri uri, String selection, String[] selectionArgs) {
        int match = URI_MATCHER.match(uri);
        switch (match) {
            case REPORT:
                throw new IllegalArgumentException(
                        "Report delete not supported. Use content query.");
            case REPORT_FILE_NAME:
            case REPORT_ROW:
            case REPORT_LATEST:
                throw new IllegalArgumentException(
                        "Report delete not supported. Use content read.");
            default:
                break;
        }

        SQLiteDatabase db = mOpenHelper.getWritableDatabase();
        int numDeleted = db.delete(TABLE_NAME, selection, selectionArgs);
        if (numDeleted > 0) {
            getContext().getContentResolver().notifyChange(uri, null);
            mBackupManager.dataChanged();
        }
        return numDeleted;
    }

    @Override
    public String getType(Uri uri) {
        return null;
    }

    @Override
    public ParcelFileDescriptor openFile(@NonNull Uri uri, @NonNull String mode)
            throws FileNotFoundException {
        String fileName;
        String[] fileList;
        File file;
        int match = URI_MATCHER.match(uri);
        switch (match) {
            case REPORT_ROW:
                int rowId = Integer.parseInt(uri.getPathSegments().get(1));
                file = getFileByIndex(rowId);
                break;

            case REPORT_FILE_NAME:
                fileName = uri.getPathSegments().get(1);
                file = getFileByName(fileName);
                break;

            case REPORT_LATEST:
                file = getLatestFile();
                break;

            case REPORT:
                throw new IllegalArgumentException("Read not supported. Use content query.");

            case RESULTS_ALL:
            case RESULTS_ID:
            case RESULTS_TEST_NAME:
                throw new IllegalArgumentException("Read not supported for URI: " + uri);

            default:
                throw new IllegalArgumentException("Unknown URI: " + uri);
        }
        try {
            FileInputStream fis = new FileInputStream(file);
            return ParcelFileDescriptor.dup(fis.getFD());
        } catch (IOException e) {
            throw new IllegalArgumentException("Cannot open file.");
        }
    }


    private File getFileByIndex(int index) {
        File[] files = getFiles();
        if (files.length == 0) {
            throw new IllegalArgumentException("No report saved at " + index + ".");
        }
        return files[index];
    }

    private File getFileByName(String fileName) {
        File[] files = getFiles();
        if (files.length == 0) {
            throw new IllegalArgumentException("No reports saved.");
        }
        for (File file : files) {
            if (fileName.equals(file.getName())) {
                return file;
            }
        }
        throw new IllegalArgumentException(fileName + " not found.");
    }

    private File getLatestFile() {
        File[] files = getFiles();
        if (files.length == 0) {
            throw new IllegalArgumentException("No reports saved.");
        }
        return files[files.length - 1];
    }

    private String[] getFileList() {
        return Arrays.stream(getFiles()).map(File::getName).toArray(String[]::new);
    }

    private File[] getFiles() {
        File dir = getContext().getDir(ReportExporter.REPORT_DIRECTORY, Context.MODE_PRIVATE);
        File[] files = dir.listFiles();
        Arrays.sort(files, Comparator.comparingLong(File::lastModified));
        return files;
    }

    private static class TestResultsOpenHelper extends SQLiteOpenHelper {

        private static final String DATABASE_NAME = "results.db";

        private static final int DATABASE_VERSION = 6;

        TestResultsOpenHelper(Context context) {
            super(context, DATABASE_NAME, null, DATABASE_VERSION);
        }

        @Override
        public void onCreate(SQLiteDatabase db) {
            db.execSQL("CREATE TABLE " + TABLE_NAME + " ("
                    + _ID + " INTEGER PRIMARY KEY AUTOINCREMENT, "
                    + COLUMN_TEST_NAME + " TEXT, "
                    + COLUMN_TEST_RESULT + " INTEGER,"
                    + COLUMN_TEST_INFO_SEEN + " INTEGER DEFAULT 0,"
                    + COLUMN_TEST_DETAILS + " TEXT,"
                    + COLUMN_TEST_METRICS + " BLOB,"
                    + COLUMN_TEST_RESULT_HISTORY + " BLOB);");
        }

        @Override
        public void onUpgrade(SQLiteDatabase db, int oldVersion, int newVersion) {
            db.execSQL("DROP TABLE IF EXISTS " + TABLE_NAME);
            onCreate(db);
        }
    }
}
