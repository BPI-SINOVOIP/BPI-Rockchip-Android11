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

import static android.provider.DocumentsContract.QUERY_ARG_MIME_TYPES;

import static com.android.documentsui.base.DocumentInfo.getCursorInt;
import static com.android.documentsui.base.DocumentInfo.getCursorLong;
import static com.android.documentsui.base.DocumentInfo.getCursorString;
import static com.android.documentsui.base.Shared.compareToIgnoreCaseNullable;
import static com.android.documentsui.base.SharedMinimal.VERBOSE;

import android.content.Context;
import android.database.Cursor;
import android.graphics.drawable.Drawable;
import android.net.Uri;
import android.os.Parcel;
import android.os.Parcelable;
import android.provider.DocumentsContract;
import android.provider.DocumentsContract.Root;
import android.text.TextUtils;
import android.util.Log;

import androidx.annotation.IntDef;

import com.android.documentsui.IconUtils;
import com.android.documentsui.R;

import java.io.DataInputStream;
import java.io.DataOutputStream;
import java.io.IOException;
import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.net.ProtocolException;
import java.util.Objects;

/**
 * Representation of a {@link Root}.
 */
public class RootInfo implements Durable, Parcelable, Comparable<RootInfo> {

    private static final String TAG = "RootInfo";
    private static final int LOAD_FROM_CONTENT_RESOLVER = -1;
    // private static final int VERSION_INIT = 1; // Not used anymore
    private static final int VERSION_DROP_TYPE = 2;
    private static final int VERSION_SEARCH_TYPE = 3;
    private static final int VERSION_USER_ID = 4;

    // The values of these constants determine the sort order of various roots in the RootsFragment.
    @IntDef(flag = false, value = {
            TYPE_RECENTS,
            TYPE_IMAGES,
            TYPE_VIDEO,
            TYPE_AUDIO,
            TYPE_DOCUMENTS,
            TYPE_DOWNLOADS,
            TYPE_LOCAL,
            TYPE_MTP,
            TYPE_SD,
            TYPE_USB,
            TYPE_OTHER
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface RootType {}
    public static final int TYPE_RECENTS = 1;
    public static final int TYPE_IMAGES = 2;
    public static final int TYPE_VIDEO = 3;
    public static final int TYPE_AUDIO = 4;
    public static final int TYPE_DOCUMENTS = 5;
    public static final int TYPE_DOWNLOADS = 6;
    public static final int TYPE_LOCAL = 7;
    public static final int TYPE_MTP = 8;
    public static final int TYPE_SD = 9;
    public static final int TYPE_USB = 10;
    public static final int TYPE_OTHER = 11;

    public UserId userId;
    public String authority;
    public String rootId;
    public int flags;
    public int icon;
    public String title;
    public String summary;
    public String documentId;
    public long availableBytes;
    public String mimeTypes;
    public String queryArgs;

    /** Derived fields that aren't persisted */
    public String[] derivedMimeTypes;
    public int derivedIcon;
    public @RootType int derivedType;
    // Currently, we are not persisting this and we should be asking Provider whether a Root
    // is in the process of eject. Provider does not have this available yet.
    public transient boolean ejecting;

    public RootInfo() {
        reset();
    }

    @Override
    public void reset() {
        userId = UserId.UNSPECIFIED_USER;
        authority = null;
        rootId = null;
        flags = 0;
        icon = 0;
        title = null;
        summary = null;
        documentId = null;
        availableBytes = -1;
        mimeTypes = null;
        ejecting = false;
        queryArgs = null;

        derivedMimeTypes = null;
        derivedIcon = 0;
        derivedType = 0;
    }

    @Override
    public void read(DataInputStream in) throws IOException {
        final int version = in.readInt();
        switch (version) {
            case VERSION_USER_ID:
                userId = UserId.read(in);
            case VERSION_SEARCH_TYPE:
                if (version < VERSION_USER_ID) {
                    userId = UserId.CURRENT_USER;
                }
                queryArgs = DurableUtils.readNullableString(in);
            case VERSION_DROP_TYPE:
                authority = DurableUtils.readNullableString(in);
                rootId = DurableUtils.readNullableString(in);
                flags = in.readInt();
                icon = in.readInt();
                title = DurableUtils.readNullableString(in);
                summary = DurableUtils.readNullableString(in);
                documentId = DurableUtils.readNullableString(in);
                availableBytes = in.readLong();
                mimeTypes = DurableUtils.readNullableString(in);
                deriveFields();
                break;
            default:
                throw new ProtocolException("Unknown version " + version);
        }
    }

    @Override
    public void write(DataOutputStream out) throws IOException {
        out.writeInt(VERSION_USER_ID);
        UserId.write(out, userId);
        DurableUtils.writeNullableString(out, queryArgs);
        DurableUtils.writeNullableString(out, authority);
        DurableUtils.writeNullableString(out, rootId);
        out.writeInt(flags);
        out.writeInt(icon);
        DurableUtils.writeNullableString(out, title);
        DurableUtils.writeNullableString(out, summary);
        DurableUtils.writeNullableString(out, documentId);
        out.writeLong(availableBytes);
        DurableUtils.writeNullableString(out, mimeTypes);
    }

    @Override
    public int describeContents() {
        return 0;
    }

    @Override
    public void writeToParcel(Parcel dest, int flags) {
        DurableUtils.writeToParcel(dest, this);
    }

    public static final Creator<RootInfo> CREATOR = new Creator<RootInfo>() {
        @Override
        public RootInfo createFromParcel(Parcel in) {
            final RootInfo root = new RootInfo();
            DurableUtils.readFromParcel(in, root);
            return root;
        }

        @Override
        public RootInfo[] newArray(int size) {
            return new RootInfo[size];
        }
    };

    /**
     * Returns a new root info copied from the provided root info.
     */
    public static RootInfo copyRootInfo(RootInfo root) {
        final RootInfo newRoot = new RootInfo();
        newRoot.userId = root.userId;
        newRoot.authority = root.authority;
        newRoot.rootId = root.rootId;
        newRoot.flags = root.flags;
        newRoot.icon = root.icon;
        newRoot.title = root.title;
        newRoot.summary = root.summary;
        newRoot.documentId = root.documentId;
        newRoot.availableBytes = root.availableBytes;
        newRoot.mimeTypes = root.mimeTypes;
        newRoot.queryArgs = root.queryArgs;

        // derived fields
        newRoot.derivedType = root.derivedType;
        newRoot.derivedIcon = root.derivedIcon;
        newRoot.derivedMimeTypes = root.derivedMimeTypes;
        return newRoot;
    }

    public static RootInfo fromRootsCursor(UserId userId, String authority, Cursor cursor) {
        final RootInfo root = new RootInfo();
        root.userId = userId;
        root.authority = authority;
        root.rootId = getCursorString(cursor, Root.COLUMN_ROOT_ID);
        root.flags = getCursorInt(cursor, Root.COLUMN_FLAGS);
        root.icon = getCursorInt(cursor, Root.COLUMN_ICON);
        root.title = getCursorString(cursor, Root.COLUMN_TITLE);
        root.summary = getCursorString(cursor, Root.COLUMN_SUMMARY);
        root.documentId = getCursorString(cursor, Root.COLUMN_DOCUMENT_ID);
        root.availableBytes = getCursorLong(cursor, Root.COLUMN_AVAILABLE_BYTES);
        root.mimeTypes = getCursorString(cursor, Root.COLUMN_MIME_TYPES);
        root.queryArgs = getCursorString(cursor, Root.COLUMN_QUERY_ARGS);
        root.deriveFields();
        return root;
    }

    private void deriveFields() {
        derivedMimeTypes = (mimeTypes != null) ? mimeTypes.split("\n") : null;

        if (isMtp()) {
            derivedType = TYPE_MTP;
            derivedIcon = R.drawable.ic_usb_storage;
        } else if (isUsb()) {
            derivedType = TYPE_USB;
            derivedIcon = R.drawable.ic_usb_storage;
        } else if (isSd()) {
            derivedType = TYPE_SD;
            derivedIcon = R.drawable.ic_sd_storage;
        } else if (isExternalStorage()) {
            derivedType = TYPE_LOCAL;
            derivedIcon = R.drawable.ic_root_smartphone;
        } else if (isDownloads()) {
            derivedType = TYPE_DOWNLOADS;
            derivedIcon = R.drawable.ic_root_download;
        } else if (isImages()) {
            derivedType = TYPE_IMAGES;
            derivedIcon = LOAD_FROM_CONTENT_RESOLVER;
        } else if (isVideos()) {
            derivedType = TYPE_VIDEO;
            derivedIcon = LOAD_FROM_CONTENT_RESOLVER;
        } else if (isAudio()) {
            derivedType = TYPE_AUDIO;
            derivedIcon = LOAD_FROM_CONTENT_RESOLVER;
        } else if (isDocuments()) {
            derivedType = TYPE_DOCUMENTS;
            derivedIcon = LOAD_FROM_CONTENT_RESOLVER;
            // The mime type of Documents root from MediaProvider is "*/*" for performance concern.
            // Align the supported mime types with document search chip
            derivedMimeTypes = MimeTypes.getDocumentMimeTypeArray();
        } else if (isRecents()) {
            derivedType = TYPE_RECENTS;
        } else if (isBugReport()) {
            derivedType = TYPE_OTHER;
            derivedIcon = R.drawable.ic_root_bugreport;
        } else {
            derivedType = TYPE_OTHER;
        }

        if (VERBOSE) Log.v(TAG, "Derived fields: " + this);
    }

    public Uri getUri() {
        return DocumentsContract.buildRootUri(authority, rootId);
    }

    public boolean isBugReport() {
        return Providers.AUTHORITY_BUGREPORT.equals(authority);
    }

    public boolean isRecents() {
        return authority == null && rootId == null;
    }

    /**
     * Return true, if the root is from ExternalStorage and the id is home. Otherwise, return false.
     */
    public boolean isExternalStorageHome() {
        // Note that "home" is the expected root id for the auto-created
        // user home directory on external storage. The "home" value should
        // match ExternalStorageProvider.ROOT_ID_HOME.
        return isExternalStorage() && "home".equals(rootId);
    }

    public boolean isExternalStorage() {
        return Providers.AUTHORITY_STORAGE.equals(authority);
    }

    public boolean isDownloads() {
        return Providers.AUTHORITY_DOWNLOADS.equals(authority);
    }

    public boolean isImages() {
        return Providers.AUTHORITY_MEDIA.equals(authority)
                && Providers.ROOT_ID_IMAGES.equals(rootId);
    }

    public boolean isVideos() {
        return Providers.AUTHORITY_MEDIA.equals(authority)
                && Providers.ROOT_ID_VIDEOS.equals(rootId);
    }

    public boolean isAudio() {
        return Providers.AUTHORITY_MEDIA.equals(authority)
                && Providers.ROOT_ID_AUDIO.equals(rootId);
    }

    public boolean isDocuments() {
        return Providers.AUTHORITY_MEDIA.equals(authority)
                && Providers.ROOT_ID_DOCUMENTS.equals(rootId);
    }

    public boolean isMtp() {
        return Providers.AUTHORITY_MTP.equals(authority);
    }

    /*
     * Return true, if the derivedType of this root is library type. Otherwise, return false.
     */
    public boolean isLibrary() {
        return derivedType == TYPE_IMAGES
                || derivedType == TYPE_VIDEO
                || derivedType == TYPE_AUDIO
                || derivedType == TYPE_RECENTS
                || derivedType == TYPE_DOCUMENTS;
    }

    /*
     * Return true, if the derivedType of this root is storage type. Otherwise, return false.
     */
    public boolean isStorage() {
        return derivedType == TYPE_LOCAL
                || derivedType == TYPE_MTP
                || derivedType == TYPE_USB
                || derivedType == TYPE_SD;
    }

    public boolean isPhoneStorage() {
        return derivedType == TYPE_LOCAL;
    }

    public boolean hasSettings() {
        return (flags & Root.FLAG_HAS_SETTINGS) != 0;
    }

    public boolean supportsChildren() {
        return (flags & Root.FLAG_SUPPORTS_IS_CHILD) != 0;
    }

    public boolean supportsCreate() {
        return (flags & Root.FLAG_SUPPORTS_CREATE) != 0;
    }

    public boolean supportsRecents() {
        return (flags & Root.FLAG_SUPPORTS_RECENTS) != 0;
    }

    public boolean supportsSearch() {
        return (flags & Root.FLAG_SUPPORTS_SEARCH) != 0;
    }

    public boolean supportsMimeTypesSearch() {
        return queryArgs != null && queryArgs.contains(QUERY_ARG_MIME_TYPES);
    }

    public boolean supportsEject() {
        return (flags & Root.FLAG_SUPPORTS_EJECT) != 0;
    }

    public boolean isAdvanced() {
        return (flags & Root.FLAG_ADVANCED) != 0;
    }

    public boolean isLocalOnly() {
        return (flags & Root.FLAG_LOCAL_ONLY) != 0;
    }

    public boolean isEmpty() {
        return (flags & Root.FLAG_EMPTY) != 0;
    }

    public boolean isSd() {
        return (flags & Root.FLAG_REMOVABLE_SD) != 0;
    }

    public boolean isUsb() {
        return (flags & Root.FLAG_REMOVABLE_USB) != 0;
    }

    /**
     * Returns true if this root supports cross profile.
     */
    public boolean supportsCrossProfile() {
        return isLibrary() || isDownloads() || isPhoneStorage();
    }

    private Drawable loadMimeTypeIcon(Context context) {
        switch (derivedType) {
            case TYPE_IMAGES:
                return IconUtils.loadMimeIcon(context, MimeTypes.IMAGE_MIME);
            case TYPE_AUDIO:
                return IconUtils.loadMimeIcon(context, MimeTypes.AUDIO_MIME);
            case TYPE_VIDEO:
                return IconUtils.loadMimeIcon(context, MimeTypes.VIDEO_MIME);
            default:
                return IconUtils.loadMimeIcon(context, MimeTypes.GENERIC_TYPE);
        }
    }

    public Drawable loadIcon(Context context, boolean maybeShowBadge) {
        if (derivedIcon == LOAD_FROM_CONTENT_RESOLVER) {
            return loadMimeTypeIcon(context);
        } else if (derivedIcon != 0) {
            // derivedIcon is set with the resources of the current user.
            return context.getDrawable(derivedIcon);
        } else {
            return IconUtils.loadPackageIcon(context, userId, authority, icon, maybeShowBadge);
        }
    }

    public Drawable loadDrawerIcon(Context context, boolean maybeShowBadge) {
        if (derivedIcon == LOAD_FROM_CONTENT_RESOLVER) {
            return IconUtils.applyTintColor(context, loadMimeTypeIcon(context),
                    R.color.item_root_icon);
        } else if (derivedIcon != 0) {
            return IconUtils.applyTintColor(context, derivedIcon, R.color.item_root_icon);
        } else {
            return IconUtils.loadPackageIcon(context, userId, authority, icon, maybeShowBadge);
        }
    }

    public Drawable loadEjectIcon(Context context) {
        return IconUtils.applyTintColor(context, R.drawable.ic_eject, R.color.item_action_icon);
    }

    @Override
    public boolean equals(Object o) {
        if (o == null) {
            return false;
        }

        if (this == o) {
            return true;
        }

        if (o instanceof RootInfo) {
            RootInfo other = (RootInfo) o;
            return Objects.equals(userId, other.userId)
                    && Objects.equals(authority, other.authority)
                    && Objects.equals(rootId, other.rootId);
        }

        return false;
    }

    @Override
    public int hashCode() {
        return Objects.hash(userId, authority, rootId);
    }

    @Override
    public int compareTo(RootInfo other) {
        // Sort by root type, then title, then summary.
        int score = derivedType - other.derivedType;
        if (score != 0) {
            return score;
        }

        score = compareToIgnoreCaseNullable(title, other.title);
        if (score != 0) {
            return score;
        }

        return compareToIgnoreCaseNullable(summary, other.summary);
    }

    @Override
    public String toString() {
        return "Root{"
                + "userId=" + userId
                + ", authority=" + authority
                + ", rootId=" + rootId
                + ", title=" + title
                + ", isUsb=" + isUsb()
                + ", isSd=" + isSd()
                + ", isMtp=" + isMtp()
                + "} @ "
                + getUri();
    }

    public String toDebugString() {
        return (TextUtils.isEmpty(summary))
                ? "\"" + title + "\" @ " + getUri()
                : "\"" + title + " (" + summary + ")\" @ " + getUri();
    }

    public String getDirectoryString() {
        return !TextUtils.isEmpty(summary) ? summary : title;
    }
}
