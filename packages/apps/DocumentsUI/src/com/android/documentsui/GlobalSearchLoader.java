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

package com.android.documentsui;

import android.content.Context;
import android.database.Cursor;
import android.net.Uri;
import android.os.Bundle;
import android.provider.DocumentsContract;

import androidx.annotation.NonNull;

import com.android.documentsui.base.Lookup;
import com.android.documentsui.base.RootInfo;
import com.android.documentsui.base.State;
import com.android.documentsui.base.UserId;
import com.android.documentsui.roots.ProvidersAccess;
import com.android.documentsui.roots.RootCursorWrapper;

import java.util.List;
import java.util.concurrent.Executor;

/*
 * The class to query multiple roots support {@link DocumentsContract.Root#FLAG_LOCAL_ONLY}
 * and {@link DocumentsContract.Root#FLAG_SUPPORTS_SEARCH} from
 * {@link android.provider.DocumentsProvider}.
 */
public class GlobalSearchLoader extends MultiRootDocumentsLoader {
    private final Bundle mQueryArgs;
    private final UserId mUserId;

    /*
     * Create the loader to query multiple roots support
     * {@link DocumentsContract.Root#FLAG_LOCAL_ONLY} and
     * {@link DocumentsContract.Root#FLAG_SUPPORTS_SEARCH} from
     * {@link android.provider.DocumentsProvider}.
     *
     * @param context the context
     * @param providers the providers
     * @param state current state
     * @param features the feature flags
     * @param executors the executors of authorities
     * @param fileTypeMap the map of mime types and file types
     * @param queryArgs the bundle of query arguments
     */
    GlobalSearchLoader(Context context, ProvidersAccess providers, State state,
            Lookup<String, Executor> executors, Lookup<String, String> fileTypeMap,
            @NonNull Bundle queryArgs, UserId userId) {
        super(context, providers, state, executors, fileTypeMap);
        mQueryArgs = queryArgs;
        mUserId = userId;
    }

    @Override
    protected boolean shouldIgnoreRoot(RootInfo root) {
        // Only support local search in GlobalSearchLoader
        if (!root.isLocalOnly() || !root.supportsSearch()) {
            return true;
        }

        if (mState.supportsCrossProfile() && root.supportsCrossProfile()
                && !mQueryArgs.containsKey(DocumentsContract.QUERY_ARG_DISPLAY_NAME)
                && !mUserId.equals(root.userId)) {
            // Ignore cross-profile roots if it is not a text search. For example, the user may
            // just filter documents by mime type.
            return true;
        }

        // To prevent duplicate files on search result, ignore storage root because its almost
        // files include in media root.
        return root.isStorage();
    }

    @Override
    protected QueryTask getQueryTask(String authority, List<RootInfo> rootInfos) {
        return new SearchTask(authority, rootInfos);
    }

    private class SearchTask extends QueryTask {

        public SearchTask(String authority, List<RootInfo> rootInfos) {
            super(authority, rootInfos);
        }

        @Override
        protected void addQueryArgs(@NonNull Bundle queryArgs) {
            queryArgs.putBoolean(DocumentsContract.QUERY_ARG_EXCLUDE_MEDIA, true);
            queryArgs.putAll(mQueryArgs);
        }

        @Override
        protected Uri getQueryUri(RootInfo rootInfo) {
            // For the new querySearchDocuments, we put the query string into queryArgs.
            // Use the empty string to build the query uri.
            return DocumentsContract.buildSearchDocumentsUri(authority,
                    rootInfo.rootId, "" /* query */);
        }

        @Override
        protected RootCursorWrapper generateResultCursor(RootInfo rootInfo, Cursor oriCursor) {
            return new RootCursorWrapper(rootInfo.userId, authority, rootInfo.rootId, oriCursor,
                    -1 /* maxCount */);
        }
    }
}
