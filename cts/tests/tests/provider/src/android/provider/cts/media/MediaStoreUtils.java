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
 * limitations under the License
 */

package android.provider.cts.media;

import android.content.ContentValues;
import android.content.Context;
import android.net.Uri;
import android.os.ParcelFileDescriptor;
import android.provider.MediaStore;
import android.provider.MediaStore.DownloadColumns;
import android.provider.MediaStore.Downloads;
import android.provider.MediaStore.MediaColumns;
import android.text.format.DateUtils;

import org.junit.Test;

import java.io.FileNotFoundException;
import java.io.OutputStream;
import java.util.Objects;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

public class MediaStoreUtils {
    @Test
    public void testStub() {
    }

    /**
     * Create a new pending media item using the given parameters. Pending items
     * are expected to have a short lifetime, and owners should either
     * {@link PendingSession#publish()} or {@link PendingSession#abandon()} a
     * pending item within a few hours after first creating it.
     *
     * @return token which can be passed to {@link #openPending(Context, Uri)}
     *         to work with this pending item.
     * @see MediaColumns#IS_PENDING
     * @see MediaStore#setIncludePending(Uri)
     * @see MediaStore#createPending(Context, PendingParams)
     * @removed
     */
    @Deprecated
    public static @NonNull Uri createPending(@NonNull Context context,
            @NonNull PendingParams params) {
        return context.getContentResolver().insert(params.insertUri, params.insertValues);
    }

    /**
     * Open a pending media item to make progress on it. You can open a pending
     * item multiple times before finally calling either
     * {@link PendingSession#publish()} or {@link PendingSession#abandon()}.
     *
     * @param uri token which was previously returned from
     *            {@link #createPending(Context, PendingParams)}.
     * @removed
     */
    @Deprecated
    public static @NonNull PendingSession openPending(@NonNull Context context, @NonNull Uri uri) {
        return new PendingSession(context, uri);
    }

    /**
     * Parameters that describe a pending media item.
     *
     * @removed
     */
    @Deprecated
    public static class PendingParams {
        /** {@hide} */
        public final Uri insertUri;
        /** {@hide} */
        public final ContentValues insertValues;

        /**
         * Create parameters that describe a pending media item.
         *
         * @param insertUri the {@code content://} Uri where this pending item
         *            should be inserted when finally published. For example, to
         *            publish an image, use
         *            {@link MediaStore.Images.Media#getContentUri(String)}.
         */
        public PendingParams(@NonNull Uri insertUri, @NonNull String displayName,
                @NonNull String mimeType) {
            this.insertUri = Objects.requireNonNull(insertUri);
            final long now = System.currentTimeMillis() / 1000;
            this.insertValues = new ContentValues();
            this.insertValues.put(MediaColumns.DISPLAY_NAME, Objects.requireNonNull(displayName));
            this.insertValues.put(MediaColumns.MIME_TYPE, Objects.requireNonNull(mimeType));
            this.insertValues.put(MediaColumns.DATE_ADDED, now);
            this.insertValues.put(MediaColumns.DATE_MODIFIED, now);
            this.insertValues.put(MediaColumns.IS_PENDING, 1);
            this.insertValues.put(MediaColumns.DATE_EXPIRES,
                    (System.currentTimeMillis() + DateUtils.DAY_IN_MILLIS) / 1000);
        }

        public void setPath(@Nullable String path) {
            if (path == null) {
                this.insertValues.remove(MediaColumns.RELATIVE_PATH);
            } else {
                this.insertValues.put(MediaColumns.RELATIVE_PATH, path);
            }
        }

        /**
         * Optionally set the Uri from where the file has been downloaded. This is used
         * for files being added to {@link Downloads} table.
         *
         * @see DownloadColumns#DOWNLOAD_URI
         */
        public void setDownloadUri(@Nullable Uri downloadUri) {
            if (downloadUri == null) {
                this.insertValues.remove(DownloadColumns.DOWNLOAD_URI);
            } else {
                this.insertValues.put(DownloadColumns.DOWNLOAD_URI, downloadUri.toString());
            }
        }

        /**
         * Optionally set the Uri indicating HTTP referer of the file. This is used for
         * files being added to {@link Downloads} table.
         *
         * @see DownloadColumns#REFERER_URI
         */
        public void setRefererUri(@Nullable Uri refererUri) {
            if (refererUri == null) {
                this.insertValues.remove(DownloadColumns.REFERER_URI);
            } else {
                this.insertValues.put(DownloadColumns.REFERER_URI, refererUri.toString());
            }
        }
    }

    /**
     * Session actively working on a pending media item. Pending items are
     * expected to have a short lifetime, and owners should either
     * {@link PendingSession#publish()} or {@link PendingSession#abandon()} a
     * pending item within a few hours after first creating it.
     *
     * @removed
     */
    @Deprecated
    public static class PendingSession implements AutoCloseable {
        /** {@hide} */
        private final Context mContext;
        /** {@hide} */
        private final Uri mUri;

        /** {@hide} */
        public PendingSession(Context context, Uri uri) {
            mContext = Objects.requireNonNull(context);
            mUri = Objects.requireNonNull(uri);
        }

        /**
         * Open the underlying file representing this media item. When a media
         * item is successfully completed, you should
         * {@link ParcelFileDescriptor#close()} and then {@link #publish()} it.
         *
         * @see #notifyProgress(int)
         */
        public @NonNull ParcelFileDescriptor open() throws FileNotFoundException {
            return mContext.getContentResolver().openFileDescriptor(mUri, "rw");
        }

        /**
         * Open the underlying file representing this media item. When a media
         * item is successfully completed, you should
         * {@link OutputStream#close()} and then {@link #publish()} it.
         *
         * @see #notifyProgress(int)
         */
        public @NonNull OutputStream openOutputStream() throws FileNotFoundException {
            return mContext.getContentResolver().openOutputStream(mUri);
        }

        /**
         * When this media item is successfully completed, call this method to
         * publish and make the final item visible to the user.
         *
         * @return the final {@code content://} Uri representing the newly
         *         published media.
         */
        public @NonNull Uri publish() {
            final ContentValues values = new ContentValues();
            values.put(MediaColumns.IS_PENDING, 0);
            values.putNull(MediaColumns.DATE_EXPIRES);
            mContext.getContentResolver().update(mUri, values, null, null);
            return mUri;
        }

        /**
         * When this media item has failed to be completed, call this method to
         * destroy the pending item record and any data related to it.
         */
        public void abandon() {
            mContext.getContentResolver().delete(mUri, null, null);
        }

        @Override
        public void close() {
            // No resources to close, but at least we can inform people that no
            // progress is being actively made.
        }
    }
}
