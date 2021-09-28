/*
 * Copyright (C) 2018 The Android Open Source Project
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
package android.tradefed.contentprovider;

import android.content.ContentProvider;
import android.content.ContentValues;
import android.database.Cursor;
import android.database.MatrixCursor;
import android.net.Uri;
import android.os.Environment;
import android.os.ParcelFileDescriptor;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import android.util.Log;
import android.webkit.MimeTypeMap;

import java.io.File;
import java.io.FileNotFoundException;
import java.io.UnsupportedEncodingException;
import java.net.URLDecoder;
import java.util.Arrays;
import java.util.Comparator;
import java.util.HashMap;
import java.util.Map;

/**
 * Content Provider implementation to hide sd card details away from host/device interactions, and
 * that allows to abstract the host/device interactions more by allowing device and host to
 * communicate files through the provider.
 *
 * <p>This implementation aims to be standard and work in all situations.
 */
public class ManagedFileContentProvider extends ContentProvider {
    public static final String COLUMN_NAME = "name";
    public static final String COLUMN_ABSOLUTE_PATH = "absolute_path";
    public static final String COLUMN_DIRECTORY = "is_directory";
    public static final String COLUMN_MIME_TYPE = "mime_type";
    public static final String COLUMN_METADATA = "metadata";

    // TODO: Complete the list of columns
    public static final String[] COLUMNS =
            new String[] {
                COLUMN_NAME,
                COLUMN_ABSOLUTE_PATH,
                COLUMN_DIRECTORY,
                COLUMN_MIME_TYPE,
                COLUMN_METADATA
            };

    private static final String TAG = "ManagedFileContentProvider";
    private static MimeTypeMap sMimeMap = MimeTypeMap.getSingleton();

    private Map<Uri, ContentValues> mFileTracker = new HashMap<>();

    @Override
    public boolean onCreate() {
        mFileTracker = new HashMap<>();
        return true;
    }

    /**
     * Use a content URI with absolute device path embedded to get information about a file or a
     * directory on the device.
     *
     * @param uri A content uri that contains the path to the desired file/directory.
     * @param projection - not supported.
     * @param selection - not supported.
     * @param selectionArgs - not supported.
     * @param sortOrder - not supported.
     * @return A {@link Cursor} containing the results of the query. Cursor contains a single row
     *     for files and for directories it returns one row for each {@link File} returned by {@link
     *     File#listFiles()}.
     */
    @Nullable
    @Override
    public Cursor query(
            @NonNull Uri uri,
            @Nullable String[] projection,
            @Nullable String selection,
            @Nullable String[] selectionArgs,
            @Nullable String sortOrder) {
        File file = getFileForUri(uri);
        if ("/".equals(file.getAbsolutePath())) {
            // Querying the root will list all the known file (inserted)
            final MatrixCursor cursor = new MatrixCursor(COLUMNS, mFileTracker.size());
            for (Map.Entry<Uri, ContentValues> path : mFileTracker.entrySet()) {
                String metadata = path.getValue().getAsString(COLUMN_METADATA);
                cursor.addRow(getRow(COLUMNS, getFileForUri(path.getKey()), metadata));
            }
            return cursor;
        }

        if (!file.exists()) {
            Log.e(TAG, String.format("Query - File from uri: '%s' does not exists.", uri));
            return null;
        }

        if (!file.isDirectory()) {
            // Just return the information about the file itself.
            final MatrixCursor cursor = new MatrixCursor(COLUMNS, 1);
            cursor.addRow(getRow(COLUMNS, file, /* metadata= */ null));
            return cursor;
        }

        // Otherwise return the content of the directory - similar to doing ls command.
        File[] files = file.listFiles();
        sortFilesByAbsolutePath(files);
        final MatrixCursor cursor = new MatrixCursor(COLUMNS, files.length + 1);
        for (File child : files) {
            cursor.addRow(getRow(COLUMNS, child, /* metadata= */ null));
        }
        return cursor;
    }

    @Nullable
    @Override
    public String getType(@NonNull Uri uri) {
        return getType(getFileForUri(uri));
    }

    @Nullable
    @Override
    public Uri insert(@NonNull Uri uri, @Nullable ContentValues contentValues) {
        String extra = "";
        File file = getFileForUri(uri);
        if (!file.exists()) {
            Log.e(TAG, String.format("Insert - File from uri: '%s' does not exists.", uri));
            return null;
        }
        if (mFileTracker.get(uri) != null) {
            Log.e(
                    TAG,
                    String.format("Insert - File from uri: '%s' already exists, ignoring.", uri));
            return null;
        }
        mFileTracker.put(uri, contentValues);
        return uri;
    }

    @Override
    public int delete(
            @NonNull Uri uri, @Nullable String selection, @Nullable String[] selectionArgs) {
        // Stop Tracking the File of directory if it was tracked and delete it from the disk
        mFileTracker.remove(uri);
        File file = getFileForUri(uri);
        int num = recursiveDelete(file);
        return num;
    }

    @Override
    public int update(
            @NonNull Uri uri,
            @Nullable ContentValues values,
            @Nullable String selection,
            @Nullable String[] selectionArgs) {
        File file = getFileForUri(uri);
        if (!file.exists()) {
            Log.e(TAG, String.format("Update - File from uri: '%s' does not exists.", uri));
            return 0;
        }
        if (mFileTracker.get(uri) == null) {
            Log.e(
                    TAG,
                    String.format(
                            "Update - File from uri: '%s' is not tracked yet, use insert.", uri));
            return 0;
        }
        mFileTracker.put(uri, values);
        return 1;
    }

    @Override
    public ParcelFileDescriptor openFile(@NonNull Uri uri, @NonNull String mode)
            throws FileNotFoundException {
        final File file = getFileForUri(uri);
        final int fileMode = modeToMode(mode);

        if ((fileMode & ParcelFileDescriptor.MODE_CREATE) == ParcelFileDescriptor.MODE_CREATE) {
            // If the file is being created, create all its parent directories that don't already
            // exist.
            file.getParentFile().mkdirs();
            if (!mFileTracker.containsKey(uri)) {
                // Track the file, if not already tracked.
                mFileTracker.put(uri, new ContentValues());
            }
        }
        return ParcelFileDescriptor.open(file, fileMode);
    }

    private Object[] getRow(String[] columns, File file, String metadata) {
        Object[] values = new Object[columns.length];
        for (int i = 0; i < columns.length; i++) {
            values[i] = getColumnValue(columns[i], file, metadata);
        }
        return values;
    }

    private Object getColumnValue(String columnName, File file, String metadata) {
        Object value = null;
        if (COLUMN_NAME.equals(columnName)) {
            value = file.getName();
        } else if (COLUMN_ABSOLUTE_PATH.equals(columnName)) {
            value = file.getAbsolutePath();
        } else if (COLUMN_DIRECTORY.equals(columnName)) {
            value = file.isDirectory();
        } else if (COLUMN_METADATA.equals(columnName)) {
            value = metadata;
        } else if (COLUMN_MIME_TYPE.equals(columnName)) {
            value = file.isDirectory() ? null : getType(file);
        }
        return value;
    }

    private String getType(@NonNull File file) {
        final int lastDot = file.getName().lastIndexOf('.');
        if (lastDot >= 0) {
            final String extension = file.getName().substring(lastDot + 1);
            final String mime = sMimeMap.getMimeTypeFromExtension(extension);
            if (mime != null) {
                return mime;
            }
        }

        return "application/octet-stream";
    }

    private File getFileForUri(@NonNull Uri uri) {
        // TODO: apply the /sdcard resolution to query() too.
        String uriPath = uri.getPath();
        try {
            uriPath = URLDecoder.decode(uriPath, "UTF-8");
        } catch (UnsupportedEncodingException e) {
            throw new RuntimeException(e);
        }
        if (uriPath.startsWith("/sdcard/")) {
            uriPath =
                    uriPath.replaceAll(
                            "/sdcard", Environment.getExternalStorageDirectory().getAbsolutePath());
        }
        return new File(uriPath);
    }

    /** Copied from FileProvider.java. */
    private static int modeToMode(String mode) {
        int modeBits;
        if ("r".equals(mode)) {
            modeBits = ParcelFileDescriptor.MODE_READ_ONLY;
        } else if ("w".equals(mode) || "wt".equals(mode)) {
            modeBits =
                    ParcelFileDescriptor.MODE_WRITE_ONLY
                            | ParcelFileDescriptor.MODE_CREATE
                            | ParcelFileDescriptor.MODE_TRUNCATE;
        } else if ("wa".equals(mode)) {
            modeBits =
                    ParcelFileDescriptor.MODE_WRITE_ONLY
                            | ParcelFileDescriptor.MODE_CREATE
                            | ParcelFileDescriptor.MODE_APPEND;
        } else if ("rw".equals(mode)) {
            modeBits = ParcelFileDescriptor.MODE_READ_WRITE | ParcelFileDescriptor.MODE_CREATE;
        } else if ("rwt".equals(mode)) {
            modeBits =
                    ParcelFileDescriptor.MODE_READ_WRITE
                            | ParcelFileDescriptor.MODE_CREATE
                            | ParcelFileDescriptor.MODE_TRUNCATE;
        } else {
            throw new IllegalArgumentException("Invalid mode: " + mode);
        }
        return modeBits;
    }

    /**
     * Recursively delete given file or directory and all its contents.
     *
     * @param rootDir the directory or file to be deleted; can be null
     * @return The number of deleted files.
     */
    private int recursiveDelete(File rootDir) {
        int count = 0;
        if (rootDir != null) {
            if (rootDir.isDirectory()) {
                File[] childFiles = rootDir.listFiles();
                if (childFiles != null) {
                    for (File child : childFiles) {
                        count += recursiveDelete(child);
                    }
                }
            }
            rootDir.delete();
            count++;
        }
        return count;
    }

    private void sortFilesByAbsolutePath(File[] files) {
        Arrays.sort(
                files,
                new Comparator<File>() {
                    @Override
                    public int compare(File f1, File f2) {
                        return f1.getAbsolutePath().compareTo(f2.getAbsolutePath());
                    }
                });
    }
}
