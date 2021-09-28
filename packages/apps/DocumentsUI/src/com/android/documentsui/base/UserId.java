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

package com.android.documentsui.base;

import static androidx.core.util.Preconditions.checkNotNull;

import android.content.ContentResolver;
import android.content.Context;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.graphics.drawable.Drawable;
import android.net.Uri;
import android.os.Process;
import android.os.UserHandle;
import android.os.UserManager;
import android.provider.DocumentsContract;

import androidx.annotation.VisibleForTesting;
import androidx.loader.content.CursorLoader;

import java.io.DataInputStream;
import java.io.DataOutputStream;
import java.io.IOException;
import java.net.ProtocolException;
import java.util.Objects;
/**
 * Representation of a {@link UserHandle}.
 */
public final class UserId {

    // A unspecified user is used as when the user's value is uninitialized. e.g. rootInfo.reset()
    public static final UserId UNSPECIFIED_USER = UserId.of(UserHandle.of(-1000));
    // A current user represents the user of the app's process. It is mainly used for comparison.
    public static final UserId CURRENT_USER = UserId.of(Process.myUserHandle());
    // A default user represents the user of the app's process. It is mainly used for operation
    // which supports only the current user only.
    public static final UserId DEFAULT_USER = CURRENT_USER;

    private static final int VERSION_INIT = 1;

    private final UserHandle mUserHandle;

    private UserId(UserHandle userHandle) {
        checkNotNull(userHandle);
        mUserHandle = userHandle;
    }

    /**
     * Returns a {@link UserId} for a given {@link UserHandle}.
     */
    public static UserId of(UserHandle userHandle) {
        return new UserId(userHandle);
    }

    /**
     * Returns a {@link UserId} for the given user id identifier.
     *
     * @see UserHandle#getIdentifier
     */
    public static UserId of(int userIdentifier) {
        return of(UserHandle.of(userIdentifier));
    }

    /**
     * Returns the given context if the user is the current user or unspecified. Otherwise, returns
     * an "android" package context as the user.
     *
     * @throws IllegalStateException if android package of the other user does not exist
     */
    @VisibleForTesting
    Context asContext(Context context) {
        if (CURRENT_USER.equals(this) || isUnspecified()) {
            return context;
        }
        try {
            return context.createPackageContextAsUser("android", /* flags= */ 0, mUserHandle);
        } catch (PackageManager.NameNotFoundException e) {
            throw new IllegalStateException("android package not found.");
        }

    }

    /**
     * Return a package manager instance of this user.
     */
    public PackageManager getPackageManager(Context context) {
        return asContext(context).getPackageManager();
    }

    /**
     * Return a content resolver instance of this user.
     */
    public ContentResolver getContentResolver(Context context) {
        return asContext(context).getContentResolver();
    }

    /**
     * Returns a drawable object associated with a particular resource ID in this user.
     */
    public Drawable getDrawable(Context context, int resId) {
        return asContext(context).getDrawable(resId);
    }

    /**
     * If this target user is a managed profile, then this returns a badged copy of the given icon
     * to be able to distinguish it from the original icon.
     */
    public Drawable getUserBadgedIcon(Context context, Drawable drawable) {
        return getPackageManager(context).getUserBadgedIcon(drawable, mUserHandle);
    }

    /**
     * Returns the value of {@link PackageManager#getUserBadgedLabel(CharSequence, UserHandle)} for
     * the user and given label.
     */
    public CharSequence getUserBadgedLabel(Context context, CharSequence label) {
        return getPackageManager(context).getUserBadgedLabel(label, mUserHandle);
    }

    /**
     * Returns true if this user refers to the system user; false otherwise.
     */
    public boolean isSystem() {
        return mUserHandle.isSystem();
    }

    /**
     * Returns true if the this user is a managed profile.
     */
    public boolean isManagedProfile(UserManager userManager) {
        return userManager.isManagedProfile(mUserHandle.getIdentifier());
    }

    /**
     * Returns true if the this user is in quiet mode.
     */
    public boolean isQuietModeEnabled(Context context) {
        final UserManager userManager =
                (UserManager) context.getSystemService(Context.USER_SERVICE);
        return userManager.isQuietModeEnabled(mUserHandle);
    }

    /**
     * Disables quiet mode for a managed profile. The caller should check {@code
     * MODIFY_QUIET_MODE} permission first.
     *
     * @return {@code false} if user's credential is needed in order to turn off quiet mode,
     * {@code true} otherwise
     */
    public boolean requestQuietModeDisabled(Context context) {
        final UserManager userManager =
                (UserManager) context.getSystemService(Context.USER_SERVICE);
        return userManager.requestQuietModeEnabled(false, mUserHandle);
    }

    /**
     * Returns a document uri representing this user.
     */
    public Uri buildDocumentUriAsUser(String authority, String documentId) {
        return DocumentsContract.buildDocumentUriAsUser(authority, documentId, mUserHandle);
    }

    /**
     * Returns a tree document uri representing this user.
     */
    public Uri buildTreeDocumentUriAsUser(String authority, String documentId) {
        String authorityWithUserInfo = buildDocumentUriAsUser(authority, documentId).getAuthority();
        Uri treeUri = DocumentsContract.buildTreeDocumentUri(authority, documentId);

        return treeUri.buildUpon()
                .encodedAuthority(authorityWithUserInfo)
                .build();
    }

    /**
     * Starts activity for this user
     */
    public void startActivityAsUser(Context context, Intent intent) {
        context.startActivityAsUser(intent, mUserHandle);
    }

    /**
     * Returns an identifier stored in this user id. This can be used to recreate the {@link UserId}
     * by {@link UserId#of(int)}.
     */
    public int getIdentifier() {
        return mUserHandle.getIdentifier();
    }

    private boolean isUnspecified() {
        return UNSPECIFIED_USER.equals(this);
    }

    @Override
    public String toString() {
        return isUnspecified() ? "UNSPECIFIED" : String.valueOf(mUserHandle.getIdentifier());
    }

    @Override
    public boolean equals(Object o) {
        if (o == null) {
            return false;
        }

        if (this == o) {
            return true;
        }

        if (o instanceof UserId) {
            UserId other = (UserId) o;
            return Objects.equals(mUserHandle, other.mUserHandle);
        }

        return false;
    }

    @Override
    public int hashCode() {
        return Objects.hash(mUserHandle);
    }

    /**
     * Reads a {@link UserId} from an input stream.
     */
    public static UserId read(DataInputStream in) throws IOException {
        final int version = in.readInt();
        switch (version) {
            case VERSION_INIT:
                int userId = in.readInt();
                return UserId.of(UserHandle.of(userId));
            default:
                throw new ProtocolException("Unknown version " + version);
        }
    }

    /**
     * Writes a {@link UserId} to an output stream.
     */
    public static void write(DataOutputStream out, UserId userId) throws IOException {
        out.writeInt(VERSION_INIT);
        out.writeInt(userId.mUserHandle.getIdentifier());
    }

    /**
     * Create a cursor loader of the user for the given uri.
     */
    public static CursorLoader createCursorLoader(Context context, Uri uri, UserId userId) {
        return new CursorLoader(userId.asContext(context), uri, null, null, null, null);
    }
}
