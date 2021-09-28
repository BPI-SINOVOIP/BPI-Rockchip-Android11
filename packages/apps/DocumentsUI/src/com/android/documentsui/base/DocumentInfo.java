/*
 * Copyright (C) 2013 The Android Open Source Project
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

package com.android.documentsui.base;

import static com.android.documentsui.base.SharedMinimal.DEBUG;

import android.content.ContentProviderClient;
import android.content.ContentResolver;
import android.database.Cursor;
import android.net.Uri;
import android.os.FileUtils;
import android.os.Parcel;
import android.os.Parcelable;
import android.provider.DocumentsContract;
import android.provider.DocumentsContract.Document;
import android.provider.DocumentsProvider;
import android.util.Log;

import androidx.annotation.VisibleForTesting;

import com.android.documentsui.DocumentsApplication;
import com.android.documentsui.archives.ArchivesProvider;
import com.android.documentsui.roots.RootCursorWrapper;
import com.android.documentsui.util.VersionUtils;

import java.io.DataInputStream;
import java.io.DataOutputStream;
import java.io.FileNotFoundException;
import java.io.IOException;
import java.net.ProtocolException;
import java.util.Arrays;
import java.util.Objects;
import java.util.Set;

import javax.annotation.Nullable;

/**
 * Representation of a {@link Document}.
 */
public class DocumentInfo implements Durable, Parcelable {
    private static final String TAG = "DocumentInfo";
    private static final int VERSION_INIT = 1;
    private static final int VERSION_SPLIT_URI = 2;
    private static final int VERSION_USER_ID = 3;

    public UserId userId;
    public String authority;
    public String documentId;
    public String mimeType;
    public String displayName;
    public long lastModified;
    public int flags;
    public String summary;
    public long size;
    public int icon;

    /** Derived fields that aren't persisted */
    public Uri derivedUri;

    public DocumentInfo() {
        reset();
    }

    @Override
    public void reset() {
        userId = UserId.UNSPECIFIED_USER;
        authority = null;
        documentId = null;
        mimeType = null;
        displayName = null;
        lastModified = -1;
        flags = 0;
        summary = null;
        size = -1;
        icon = 0;
        derivedUri = null;
    }

    @Override
    public void read(DataInputStream in) throws IOException {
        final int version = in.readInt();
        switch (version) {
            case VERSION_USER_ID:
                userId = UserId.read(in);
            case VERSION_SPLIT_URI:
                if (version < VERSION_USER_ID) {
                    userId = UserId.CURRENT_USER;
                }
                authority = DurableUtils.readNullableString(in);
                documentId = DurableUtils.readNullableString(in);
                mimeType = DurableUtils.readNullableString(in);
                displayName = DurableUtils.readNullableString(in);
                lastModified = in.readLong();
                flags = in.readInt();
                summary = DurableUtils.readNullableString(in);
                size = in.readLong();
                icon = in.readInt();
                deriveFields();
                break;
            case VERSION_INIT:
                throw new ProtocolException("Ignored upgrade");
            default:
                throw new ProtocolException("Unknown version " + version);
        }
    }

    @Override
    public void write(DataOutputStream out) throws IOException {
        out.writeInt(VERSION_USER_ID);
        UserId.write(out, userId);
        DurableUtils.writeNullableString(out, authority);
        DurableUtils.writeNullableString(out, documentId);
        DurableUtils.writeNullableString(out, mimeType);
        DurableUtils.writeNullableString(out, displayName);
        out.writeLong(lastModified);
        out.writeInt(flags);
        DurableUtils.writeNullableString(out, summary);
        out.writeLong(size);
        out.writeInt(icon);
    }

    @Override
    public int describeContents() {
        return 0;
    }

    @Override
    public void writeToParcel(Parcel dest, int flags) {
        DurableUtils.writeToParcel(dest, this);
    }

    public static final Creator<DocumentInfo> CREATOR = new Creator<DocumentInfo>() {
        @Override
        public DocumentInfo createFromParcel(Parcel in) {
            final DocumentInfo doc = new DocumentInfo();
            DurableUtils.readFromParcel(in, doc);
            return doc;
        }

        @Override
        public DocumentInfo[] newArray(int size) {
            return new DocumentInfo[size];
        }
    };

    public static DocumentInfo fromDirectoryCursor(Cursor cursor) {
        assert (cursor != null);
        assert (cursor.getColumnIndex(RootCursorWrapper.COLUMN_USER_ID) >= 0);
        final UserId userId = UserId.of(getCursorInt(cursor, RootCursorWrapper.COLUMN_USER_ID));
        final String authority = getCursorString(cursor, RootCursorWrapper.COLUMN_AUTHORITY);
        return fromCursor(cursor, userId, authority);
    }

    public static DocumentInfo fromCursor(Cursor cursor, UserId userId, String authority) {
        assert(cursor != null);
        final DocumentInfo info = new DocumentInfo();
        info.updateFromCursor(cursor, userId, authority);
        return info;
    }

    public void updateFromCursor(Cursor cursor, UserId userId, String authority) {
        this.userId = userId;
        this.authority = authority;
        this.documentId = getCursorString(cursor, Document.COLUMN_DOCUMENT_ID);
        this.mimeType = getCursorString(cursor, Document.COLUMN_MIME_TYPE);
        this.displayName = getCursorString(cursor, Document.COLUMN_DISPLAY_NAME);
        this.lastModified = getCursorLong(cursor, Document.COLUMN_LAST_MODIFIED);
        this.flags = getCursorInt(cursor, Document.COLUMN_FLAGS);
        this.summary = getCursorString(cursor, Document.COLUMN_SUMMARY);
        this.size = getCursorLong(cursor, Document.COLUMN_SIZE);
        this.icon = getCursorInt(cursor, Document.COLUMN_ICON);
        this.deriveFields();
    }

    /**
     * Resolves a document info from the uri. The caller should specify the user of the resolver
     * by providing a {@link UserId}.
     */
    public static DocumentInfo fromUri(ContentResolver resolver, Uri uri, UserId userId)
            throws FileNotFoundException {
        final DocumentInfo info = new DocumentInfo();
        info.updateFromUri(resolver, uri, userId);
        return info;
    }

    /**
     * Update a possibly stale restored document against a live {@link DocumentsProvider}.  The
     * caller should specify the user of the resolver by providing a {@link UserId}.
     */
    public void updateSelf(ContentResolver resolver, UserId userId) throws FileNotFoundException {
        updateFromUri(resolver, derivedUri, userId);
    }

    private void updateFromUri(ContentResolver resolver, Uri uri, UserId userId)
            throws FileNotFoundException {
        ContentProviderClient client = null;
        Cursor cursor = null;
        try {
            client = DocumentsApplication.acquireUnstableProviderOrThrow(
                    resolver, uri.getAuthority());
            cursor = client.query(uri, null, null, null, null);
            if (!cursor.moveToFirst()) {
                throw new FileNotFoundException("Missing details for " + uri);
            }
            updateFromCursor(cursor, userId, uri.getAuthority());
        } catch (Throwable t) {
            throw asFileNotFoundException(t);
        } finally {
            FileUtils.closeQuietly(cursor);
            FileUtils.closeQuietly(client);
        }
    }

    @VisibleForTesting
    void deriveFields() {
        derivedUri = DocumentsContract.buildDocumentUri(authority, documentId);
    }

    @Override
    public String toString() {
        return "DocumentInfo{"
                + "docId=" + documentId
                + ", userId=" + userId
                + ", name=" + displayName
                + ", mimeType=" + mimeType
                + ", isContainer=" + isContainer()
                + ", isDirectory=" + isDirectory()
                + ", isArchive=" + isArchive()
                + ", isInArchive=" + isInArchive()
                + ", isPartial=" + isPartial()
                + ", isVirtual=" + isVirtual()
                + ", isDeleteSupported=" + isDeleteSupported()
                + ", isCreateSupported=" + isCreateSupported()
                + ", isMoveSupported=" + isMoveSupported()
                + ", isRenameSupported=" + isRenameSupported()
                + ", isMetadataSupported=" + isMetadataSupported()
                + ", isBlockedFromTree=" + isBlockedFromTree()
                + "} @ "
                + derivedUri;
    }

    public boolean isCreateSupported() {
        return (flags & Document.FLAG_DIR_SUPPORTS_CREATE) != 0;
    }

    public boolean isDeleteSupported() {
        return (flags & Document.FLAG_SUPPORTS_DELETE) != 0;
    }

    public boolean isMetadataSupported() {
        return (flags & Document.FLAG_SUPPORTS_METADATA) != 0;
    }

    public boolean isMoveSupported() {
        return (flags & Document.FLAG_SUPPORTS_MOVE) != 0;
    }

    public boolean isRemoveSupported() {
        return (flags & Document.FLAG_SUPPORTS_REMOVE) != 0;
    }

    public boolean isRenameSupported() {
        return (flags & Document.FLAG_SUPPORTS_RENAME) != 0;
    }

    public boolean isSettingsSupported() {
        return (flags & Document.FLAG_SUPPORTS_SETTINGS) != 0;
    }

    public boolean isThumbnailSupported() {
        return (flags & Document.FLAG_SUPPORTS_THUMBNAIL) != 0;
    }

    public boolean isWeblinkSupported() {
        return (flags & Document.FLAG_WEB_LINKABLE) != 0;
    }

    public boolean isWriteSupported() {
        return (flags & Document.FLAG_SUPPORTS_WRITE) != 0;
    }

    public boolean isDirectory() {
        return Document.MIME_TYPE_DIR.equals(mimeType);
    }

    public boolean isArchive() {
        return ArchivesProvider.isSupportedArchiveType(mimeType);
    }

    public boolean isInArchive() {
        return ArchivesProvider.AUTHORITY.equals(authority);
    }

    public boolean isPartial() {
        return (flags & Document.FLAG_PARTIAL) != 0;
    }

    public boolean isBlockedFromTree() {
        if (VersionUtils.isAtLeastR()) {
            return (flags & Document.FLAG_DIR_BLOCKS_OPEN_DOCUMENT_TREE) != 0;
        } else {
            return false;
        }
    }

    // Containers are documents which can be opened in DocumentsUI as folders.
    public boolean isContainer() {
        return isDirectory() || (isArchive() && !isInArchive() && !isPartial());
    }

    public boolean isVirtual() {
        return (flags & Document.FLAG_VIRTUAL_DOCUMENT) != 0;
    }

    public boolean prefersSortByLastModified() {
        return (flags & Document.FLAG_DIR_PREFERS_LAST_MODIFIED) != 0;
    }

    /**
     * Returns a document uri representing this {@link DocumentInfo}. The URI may contain user
     * information. Use this when uri is needed externally. For usage within DocsUI, use
     * {@link #derivedUri}.
     */
    public Uri getDocumentUri() {
        if (UserId.CURRENT_USER.equals(userId)) {
            return derivedUri;
        }
        return userId.buildDocumentUriAsUser(authority, documentId);
    }


    /**
     * Returns a tree document uri representing this {@link DocumentInfo}. The URI may contain user
     * information. Use this when uri is needed externally.
     */
    public Uri getTreeDocumentUri() {
        if (UserId.CURRENT_USER.equals(userId)) {
            return DocumentsContract.buildTreeDocumentUri(authority, documentId);
        }
        return userId.buildTreeDocumentUriAsUser(authority, documentId);
    }

    @Override
    public int hashCode() {
        return userId.hashCode() + derivedUri.hashCode() + mimeType.hashCode();
    }

    @Override
    public boolean equals(Object o) {
        if (o == null) {
            return false;
        }

        if (this == o) {
            return true;
        }

        if (o instanceof DocumentInfo) {
            DocumentInfo other = (DocumentInfo) o;
            // Uri + mime type should be totally unique.
            return Objects.equals(userId, other.userId)
                    && Objects.equals(derivedUri, other.derivedUri)
                    && Objects.equals(mimeType, other.mimeType);
        }

        return false;
    }

    public static String getCursorString(Cursor cursor, String columnName) {
        if (cursor == null) {
            return null;
        }
        final int index = cursor.getColumnIndex(columnName);
        return (index != -1) ? cursor.getString(index) : null;
    }

    /**
     * Missing or null values are returned as -1.
     */
    public static long getCursorLong(Cursor cursor, String columnName) {
        if (cursor == null) {
            return -1;
        }

        final int index = cursor.getColumnIndex(columnName);
        if (index == -1) return -1;
        final String value = cursor.getString(index);
        if (value == null) return -1;
        try {
            return Long.parseLong(value);
        } catch (NumberFormatException e) {
            return -1;
        }
    }

    /**
     * Missing or null values are returned as 0.
     */
    public static int getCursorInt(Cursor cursor, String columnName) {
        if (cursor == null) {
            return 0;
        }

        final int index = cursor.getColumnIndex(columnName);
        return (index != -1) ? cursor.getInt(index) : 0;
    }

    public static FileNotFoundException asFileNotFoundException(Throwable t)
            throws FileNotFoundException {
        if (t instanceof FileNotFoundException) {
            throw (FileNotFoundException) t;
        }
        final FileNotFoundException fnfe = new FileNotFoundException(t.getMessage());
        fnfe.initCause(t);
        throw fnfe;
    }

    public static Uri getUri(Cursor cursor) {
        return DocumentsContract.buildDocumentUri(
            getCursorString(cursor, RootCursorWrapper.COLUMN_AUTHORITY),
            getCursorString(cursor, Document.COLUMN_DOCUMENT_ID));
    }

    public static UserId getUserId(Cursor cursor) {
        return UserId.of(getCursorInt(cursor, RootCursorWrapper.COLUMN_USER_ID));
    }

    public static void addMimeTypes(ContentResolver resolver, Uri uri, Set<String> mimeTypes) {
        assert(uri != null);
        if ("content".equals(uri.getScheme())) {
            final String type = resolver.getType(uri);
            if (type != null) {
                mimeTypes.add(type);
            } else {
                if (DEBUG) {
                    Log.d(TAG, "resolver.getType(uri) return null, url:" + uri.toSafeString());
                }
            }
            final String[] streamTypes = resolver.getStreamTypes(uri, "*/*");
            if (streamTypes != null) {
                mimeTypes.addAll(Arrays.asList(streamTypes));
            }
        }
    }

    public static String debugString(@Nullable DocumentInfo doc) {
        if (doc == null) {
            return "<null DocumentInfo>";
        }

        if (doc.derivedUri == null) {
            return "<DocumentInfo null derivedUri>";
        }
        return doc.derivedUri.toString();
    }
}
