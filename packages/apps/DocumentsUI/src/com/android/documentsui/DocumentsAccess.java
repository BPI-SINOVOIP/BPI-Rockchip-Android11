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

package com.android.documentsui;

import static android.content.ContentResolver.wrap;

import android.content.ContentProviderClient;
import android.content.ContentResolver;
import android.content.Context;
import android.database.Cursor;
import android.net.Uri;
import android.os.ParcelFileDescriptor;
import android.os.RemoteException;
import android.provider.DocumentsContract;
import android.provider.DocumentsContract.Path;
import android.util.Log;

import androidx.annotation.Nullable;

import com.android.documentsui.archives.ArchivesProvider;
import com.android.documentsui.base.DocumentInfo;
import com.android.documentsui.base.RootInfo;
import com.android.documentsui.base.State;
import com.android.documentsui.base.UserId;

import java.io.FileNotFoundException;
import java.util.ArrayList;
import java.util.List;

/**
 * Provides synchronous access to {@link DocumentInfo} instances given some identifying information
 * and some documents API.
 */
public interface DocumentsAccess {

    @Nullable DocumentInfo getRootDocument(RootInfo root);
    @Nullable DocumentInfo getDocument(Uri uri, UserId userId);
    @Nullable DocumentInfo getArchiveDocument(Uri uri, UserId userId);

    boolean isDocumentUri(Uri uri);

    @Nullable
    Path findDocumentPath(Uri uri, UserId userId)
            throws RemoteException, FileNotFoundException, CrossProfileNoPermissionException;

    List<DocumentInfo> getDocuments(UserId userId, String authority, List<String> docIds)
            throws RemoteException, CrossProfileNoPermissionException;

    @Nullable Uri createDocument(DocumentInfo parentDoc, String mimeType, String displayName);

    public static DocumentsAccess create(Context context, State state) {
        return new RuntimeDocumentAccess(context, state);
    }

    public final class RuntimeDocumentAccess implements DocumentsAccess {

        private static final String TAG = "DocumentAccess";

        private final Context mContext;
        private final State mState;

        private RuntimeDocumentAccess(Context context, State state) {
            mContext = context;
            mState = state;
        }

        @Override
        @Nullable
        public DocumentInfo getRootDocument(RootInfo root) {
            return getDocument(DocumentsContract.buildDocumentUri(root.authority, root.documentId),
                    root.userId);
        }

        @Override
        public @Nullable DocumentInfo getDocument(Uri uri, UserId userId) {
            try {
                if (mState.canInteractWith(userId)) {
                    return DocumentInfo.fromUri(userId.getContentResolver(mContext), uri, userId);
                }
            } catch (FileNotFoundException e) {
                Log.w(TAG, "Couldn't create DocumentInfo for uri: " + uri);
            }

            return null;
        }

        @Override
        public List<DocumentInfo> getDocuments(UserId userId, String authority, List<String> docIds)
                throws RemoteException, CrossProfileNoPermissionException {
            if (!mState.canInteractWith(userId)) {
                throw new CrossProfileNoPermissionException();
            }
            try (ContentProviderClient client = DocumentsApplication.acquireUnstableProviderOrThrow(
                    userId.getContentResolver(mContext), authority)) {

                List<DocumentInfo> result = new ArrayList<>(docIds.size());
                for (String docId : docIds) {
                    final Uri uri = DocumentsContract.buildDocumentUri(authority, docId);
                    try (final Cursor cursor = client.query(uri, null, null, null, null)) {
                        if (!cursor.moveToNext()) {
                            Log.e(TAG, "Couldn't create DocumentInfo for Uri: " + uri);
                            throw new RemoteException("Failed to move cursor.");
                        }

                        result.add(DocumentInfo.fromCursor(cursor, userId, authority));
                    }
                }

                return result;
            }
        }

        @Override
        public DocumentInfo getArchiveDocument(Uri uri, UserId userId) {
            return getDocument(
                    ArchivesProvider.buildUriForArchive(uri, ParcelFileDescriptor.MODE_READ_ONLY),
                    userId);
        }

        @Override
        public boolean isDocumentUri(Uri uri) {
            return DocumentsContract.isDocumentUri(mContext, uri);
        }

        @Override
        public Path findDocumentPath(Uri docUri, UserId userId)
                throws RemoteException, FileNotFoundException, CrossProfileNoPermissionException {
            if (!mState.canInteractWith(userId)) {
                throw new CrossProfileNoPermissionException();
            }
            final ContentResolver resolver = userId.getContentResolver(mContext);
            try (final ContentProviderClient client = DocumentsApplication
                    .acquireUnstableProviderOrThrow(resolver, docUri.getAuthority())) {
                return DocumentsContract.findDocumentPath(wrap(client), docUri);
            }
        }

        @Override
        public Uri createDocument(DocumentInfo parentDoc, String mimeType, String displayName) {
            final ContentResolver resolver = parentDoc.userId.getContentResolver(mContext);
            try (ContentProviderClient client = DocumentsApplication.acquireUnstableProviderOrThrow(
                    resolver, parentDoc.derivedUri.getAuthority())) {
                Uri createUri = DocumentsContract.createDocument(
                        wrap(client), parentDoc.derivedUri, mimeType, displayName);
                // If the document info's user is the current user, we can simply return the uri.
                // Otherwise, we need to create document with the content resolver from the other
                // user. The uri returned from that content resolver does not contain the user
                // info. Hence we need to append the other user info to the uri otherwise an app
                // will think the uri is from the current user.
                // The way to append a userInfo is to use the authority which contains user info
                // obtained from the parentDoc.getDocumentUri().
                return UserId.CURRENT_USER.equals(parentDoc.userId)
                        ? createUri : appendEncodedParentAuthority(parentDoc, createUri);
            } catch (Exception e) {
                Log.w(TAG, "Failed to create document", e);
                return null;
            }
        }

        private Uri appendEncodedParentAuthority(DocumentInfo parentDoc, Uri uri) {
            return uri.buildUpon().encodedAuthority(
                    parentDoc.getDocumentUri().getAuthority()).build();
        }
    }
}
