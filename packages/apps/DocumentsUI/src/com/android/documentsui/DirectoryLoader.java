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

import static com.android.documentsui.base.SharedMinimal.VERBOSE;

import android.content.ContentProviderClient;
import android.content.ContentResolver;
import android.content.Context;
import android.database.Cursor;
import android.database.MergeCursor;
import android.net.Uri;
import android.os.Bundle;
import android.os.CancellationSignal;
import android.os.FileUtils;
import android.os.OperationCanceledException;
import android.os.RemoteException;
import android.provider.DocumentsContract;
import android.provider.DocumentsContract.Document;
import android.util.Log;

import androidx.annotation.Nullable;
import androidx.loader.content.AsyncTaskLoader;

import com.android.documentsui.archives.ArchivesProvider;
import com.android.documentsui.base.DebugFlags;
import com.android.documentsui.base.DocumentInfo;
import com.android.documentsui.base.Features;
import com.android.documentsui.base.FilteringCursorWrapper;
import com.android.documentsui.base.Lookup;
import com.android.documentsui.base.MimeTypes;
import com.android.documentsui.base.RootInfo;
import com.android.documentsui.base.State;
import com.android.documentsui.base.UserId;
import com.android.documentsui.roots.RootCursorWrapper;
import com.android.documentsui.sorting.SortModel;

import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.Executor;

public class DirectoryLoader extends AsyncTaskLoader<DirectoryResult> {

    private static final String TAG = "DirectoryLoader";

    private static final String[] SEARCH_REJECT_MIMES = new String[] { Document.MIME_TYPE_DIR };
    private static final String[] PHOTO_PICKING_ACCEPT_MIMES = new String[]
            {Document.MIME_TYPE_DIR, MimeTypes.IMAGE_MIME};

    private final LockingContentObserver mObserver;
    private final RootInfo mRoot;
    private final State mState;
    private final Uri mUri;
    private final SortModel mModel;
    private final Lookup<String, String> mFileTypeLookup;
    private final boolean mSearchMode;
    private final Bundle mQueryArgs;
    private final boolean mPhotoPicking;

    @Nullable
    private DocumentInfo mDoc;
    private CancellationSignal mSignal;
    private DirectoryResult mResult;

    private Features mFeatures;

    public DirectoryLoader(
            Features features,
            Context context,
            State state,
            Uri uri,
            Lookup<String, String> fileTypeLookup,
            ContentLock lock,
            Bundle queryArgs) {

        super(context);
        mFeatures = features;
        mState = state;
        mRoot = state.stack.getRoot();
        mUri = uri;
        mModel = state.sortModel;
        mDoc = state.stack.peek();
        mFileTypeLookup = fileTypeLookup;
        mSearchMode = queryArgs != null;
        mQueryArgs = queryArgs;
        mObserver = new LockingContentObserver(lock, this::onContentChanged);
        mPhotoPicking = state.isPhotoPicking();
    }

    @Override
    protected Executor getExecutor() {
        return ProviderExecutor.forAuthority(mRoot.authority);
    }

    @Override
    public final DirectoryResult loadInBackground() {
        synchronized (this) {
            if (isLoadInBackgroundCanceled()) {
                throw new OperationCanceledException();
            }
            mSignal = new CancellationSignal();
        }

        final String authority = mUri.getAuthority();

        final DirectoryResult result = new DirectoryResult();
        result.doc = mDoc;

        ContentProviderClient client = null;
        Cursor cursor;
        try {
            final Bundle queryArgs = new Bundle();
            mModel.addQuerySortArgs(queryArgs);

            final List<UserId> userIds = new ArrayList<>();
            if (mSearchMode) {
                queryArgs.putAll(mQueryArgs);
                if (shouldSearchAcrossProfile()) {
                    for (UserId userId : DocumentsApplication.getUserIdManager(
                            getContext()).getUserIds()) {
                        if (mState.canInteractWith(userId)) {
                            userIds.add(userId);
                        }
                    }
                }
            }
            if (userIds.isEmpty()) {
                userIds.add(mRoot.userId);
            }

            if (userIds.size() == 1) {
                if (!mState.canInteractWith(mRoot.userId)) {
                    result.exception = new CrossProfileNoPermissionException();
                    return result;
                } else if (mRoot.userId.isQuietModeEnabled(getContext())) {
                    result.exception = new CrossProfileQuietModeException(mRoot.userId);
                    return result;
                } else if (mDoc == null) {
                    // TODO (b/35996595): Consider plumbing through the actual exception, though it
                    // might not be very useful (always pointing to
                    // DatabaseUtils#readExceptionFromParcel()).
                    result.exception = new IllegalStateException("Failed to load root document.");
                    return result;
                }
            }

            if (mDoc != null && mDoc.isInArchive()) {
                final ContentResolver resolver = mRoot.userId.getContentResolver(getContext());
                client = DocumentsApplication.acquireUnstableProviderOrThrow(resolver, authority);
                ArchivesProvider.acquireArchive(client, mUri);
                result.client = client;
            }

            if (mFeatures.isContentPagingEnabled()) {
                // TODO: At some point we don't want forced flags to override real paging...
                // and that point is when we have real paging.
                DebugFlags.addForcedPagingArgs(queryArgs);
            }

            cursor = queryOnUsers(userIds, authority, queryArgs);

            if (cursor == null) {
                throw new RemoteException("Provider returned null");
            }
            cursor.registerContentObserver(mObserver);

            // Filter hidden files.
            cursor = new FilteringCursorWrapper(cursor, mState.showHiddenFiles);

            if (mSearchMode && !mFeatures.isFoldersInSearchResultsEnabled()) {
                // There is no findDocumentPath API. Enable filtering on folders in search mode.
                cursor = new FilteringCursorWrapper(cursor, null, SEARCH_REJECT_MIMES);
            }

            if (mPhotoPicking) {
                cursor = new FilteringCursorWrapper(cursor, PHOTO_PICKING_ACCEPT_MIMES, null);
            }

            // TODO: When API tweaks have landed, use ContentResolver.EXTRA_HONORED_ARGS
            // instead of checking directly for ContentResolver.QUERY_ARG_SORT_COLUMNS (won't work)
            if (mFeatures.isContentPagingEnabled()
                    && cursor.getExtras().containsKey(ContentResolver.QUERY_ARG_SORT_COLUMNS)) {
                if (VERBOSE) Log.d(TAG, "Skipping sort of pre-sorted cursor. Booya!");
            } else {
                cursor = mModel.sortCursor(cursor, mFileTypeLookup);
            }
            result.cursor = cursor;
        } catch (Exception e) {
            Log.w(TAG, "Failed to query", e);
            result.exception = e;
            FileUtils.closeQuietly(client);
        } finally {
            synchronized (this) {
                mSignal = null;
            }
        }

        return result;
    }

    private boolean shouldSearchAcrossProfile() {
        return mState.supportsCrossProfile()
                && mRoot.supportsCrossProfile()
                && mQueryArgs.containsKey(DocumentsContract.QUERY_ARG_DISPLAY_NAME);
    }

    @Nullable
    private Cursor queryOnUsers(List<UserId> userIds, String authority, Bundle queryArgs)
            throws RemoteException {
        final List<Cursor> cursors = new ArrayList<>(userIds.size());
        for (UserId userId : userIds) {
            try (ContentProviderClient userClient =
                         DocumentsApplication.acquireUnstableProviderOrThrow(
                                 userId.getContentResolver(getContext()), authority)) {
                Cursor c = userClient.query(mUri, /* projection= */null, queryArgs, mSignal);
                if (c != null) {
                    cursors.add(new RootCursorWrapper(userId, mUri.getAuthority(), mRoot.rootId,
                            c, /* maxCount= */-1));
                }
            } catch (RemoteException e) {
                Log.d(TAG, "Failed to query for user " + userId, e);
                // Searching on other profile may not succeed because profile may be in quiet mode.
                if (UserId.CURRENT_USER.equals(userId)) {
                    throw e;
                }
            }
        }
        int size = cursors.size();
        switch (size) {
            case 0:
                return null;
            case 1:
                return cursors.get(0);
            default:
                return new MergeCursor(cursors.toArray(new Cursor[size]));
        }
    }

    @Override
    public void cancelLoadInBackground() {
        super.cancelLoadInBackground();

        synchronized (this) {
            if (mSignal != null) {
                mSignal.cancel();
            }
        }
    }

    @Override
    public void deliverResult(DirectoryResult result) {
        if (isReset()) {
            FileUtils.closeQuietly(result);
            return;
        }
        DirectoryResult oldResult = mResult;
        mResult = result;

        if (isStarted()) {
            super.deliverResult(result);
        }

        if (oldResult != null && oldResult != result) {
            FileUtils.closeQuietly(oldResult);
        }
    }

    @Override
    protected void onStartLoading() {
        boolean isCursorStale = checkIfCursorStale(mResult);
        if (mResult != null && !isCursorStale) {
            deliverResult(mResult);
        }
        if (takeContentChanged() || mResult == null || isCursorStale) {
            forceLoad();
        }
    }

    @Override
    protected void onStopLoading() {
        cancelLoad();
    }

    @Override
    public void onCanceled(DirectoryResult result) {
        FileUtils.closeQuietly(result);
    }

    @Override
    protected void onReset() {
        super.onReset();

        // Ensure the loader is stopped
        onStopLoading();

        if (mResult != null && mResult.cursor != null && mObserver != null) {
            mResult.cursor.unregisterContentObserver(mObserver);
        }

        FileUtils.closeQuietly(mResult);
        mResult = null;
    }

    private boolean checkIfCursorStale(DirectoryResult result) {
        if (result == null || result.cursor == null || result.cursor.isClosed()) {
            return true;
        }
        Cursor cursor = result.cursor;
        try {
            cursor.moveToPosition(-1);
            for (int pos = 0; pos < cursor.getCount(); ++pos) {
                if (!cursor.moveToNext()) {
                    return true;
                }
            }
        } catch (Exception e) {
            return true;
        }
        return false;
    }
}
