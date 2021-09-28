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

package com.android.documentsui.roots;

import static android.provider.DocumentsContract.QUERY_ARG_MIME_TYPES;

import static androidx.core.util.Preconditions.checkNotNull;

import static com.android.documentsui.base.SharedMinimal.DEBUG;
import static com.android.documentsui.base.SharedMinimal.VERBOSE;

import android.content.BroadcastReceiver.PendingResult;
import android.content.ContentProviderClient;
import android.content.ContentResolver;
import android.content.Context;
import android.content.Intent;
import android.content.pm.ApplicationInfo;
import android.content.pm.PackageManager;
import android.content.pm.ProviderInfo;
import android.content.pm.ResolveInfo;
import android.database.ContentObserver;
import android.database.Cursor;
import android.net.Uri;
import android.os.AsyncTask;
import android.os.Bundle;
import android.os.FileUtils;
import android.os.Handler;
import android.os.Looper;
import android.os.SystemClock;
import android.provider.DocumentsContract;
import android.provider.DocumentsContract.Root;
import android.util.Log;

import androidx.annotation.GuardedBy;
import androidx.annotation.Nullable;
import androidx.localbroadcastmanager.content.LocalBroadcastManager;

import com.android.documentsui.DocumentsApplication;
import com.android.documentsui.R;
import com.android.documentsui.UserIdManager;
import com.android.documentsui.UserPackage;
import com.android.documentsui.archives.ArchivesProvider;
import com.android.documentsui.base.LookupApplicationName;
import com.android.documentsui.base.Providers;
import com.android.documentsui.base.RootInfo;
import com.android.documentsui.base.State;
import com.android.documentsui.base.UserId;

import com.google.common.collect.ArrayListMultimap;
import com.google.common.collect.Multimap;

import java.util.ArrayList;
import java.util.Collection;
import java.util.Collections;
import java.util.HashMap;
import java.util.HashSet;
import java.util.List;
import java.util.Map;
import java.util.Objects;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;
import java.util.function.Function;

/**
 * Cache of known storage backends and their roots.
 */
public class ProvidersCache implements ProvidersAccess, LookupApplicationName {
    private static final String TAG = "ProvidersCache";

    // Not all providers are equally well written. If a provider returns
    // empty results we don't cache them...unless they're in this magical list
    // of beloved providers.
    private static final List<String> PERMIT_EMPTY_CACHE = new ArrayList<String>() {{
        // MTP provider commonly returns no roots (if no devices are attached).
        add(Providers.AUTHORITY_MTP);
        // ArchivesProvider doesn't support any roots.
        add(ArchivesProvider.AUTHORITY);
    }};

    private final Context mContext;

    @GuardedBy("mRootsChangedObservers")
    private final Map<UserId, RootsChangedObserver> mRootsChangedObservers = new HashMap<>();

    @GuardedBy("mRecentsRoots")
    private final Map<UserId, RootInfo> mRecentsRoots = new HashMap<>();

    private final Object mLock = new Object();
    private final CountDownLatch mFirstLoad = new CountDownLatch(1);

    @GuardedBy("mLock")
    private boolean mFirstLoadDone;
    @GuardedBy("mLock")
    private PendingResult mBootCompletedResult;

    @GuardedBy("mLock")
    private Multimap<UserAuthority, RootInfo> mRoots = ArrayListMultimap.create();
    @GuardedBy("mLock")
    private HashSet<UserAuthority> mStoppedAuthorities = new HashSet<>();

    @GuardedBy("mObservedAuthoritiesDetails")
    private final Map<UserAuthority, PackageDetails> mObservedAuthoritiesDetails = new HashMap<>();

    private final UserIdManager mUserIdManager;

    public ProvidersCache(Context context, UserIdManager userIdManager) {
        mContext = context;
        mUserIdManager = userIdManager;
    }

    private RootInfo generateRecentsRoot(UserId rootUserId) {
        return new RootInfo() {{
            // Special root for recents
            userId = rootUserId;
            derivedIcon = R.drawable.ic_root_recent;
            derivedType = RootInfo.TYPE_RECENTS;
            flags = Root.FLAG_LOCAL_ONLY | Root.FLAG_SUPPORTS_IS_CHILD | Root.FLAG_SUPPORTS_SEARCH;
            queryArgs = QUERY_ARG_MIME_TYPES;
            title = mContext.getString(R.string.root_recent);
            availableBytes = -1;
        }};
    }

    private RootInfo createOrGetRecentsRoot(UserId userId) {
        return createOrGetByUserId(mRecentsRoots, userId, user -> generateRecentsRoot(user));
    }

    private RootsChangedObserver createOrGetRootsChangedObserver(UserId userId) {
        return createOrGetByUserId(mRootsChangedObservers, userId,
                user -> new RootsChangedObserver(user));
    }

    private static <T> T createOrGetByUserId(Map<UserId, T> map, UserId userId,
            Function<UserId, T> supplier) {
        synchronized (map) {
            if (!map.containsKey(userId)) {
                map.put(userId, supplier.apply(userId));
            }
        }
        return map.get(userId);
    }

    private class RootsChangedObserver extends ContentObserver {

        private final UserId mUserId;

        RootsChangedObserver(UserId userId) {
            super(new Handler(Looper.getMainLooper()));
            mUserId = userId;
        }

        @Override
        public void onChange(boolean selfChange, Uri uri) {
            if (uri == null) {
                Log.w(TAG, "Received onChange event for null uri. Skipping.");
                return;
            }
            if (DEBUG) {
                Log.i(TAG, "Updating roots due to change on user " + mUserId + "at " + uri);
            }
            updateAuthorityAsync(mUserId, uri.getAuthority());
        }
    }

    @Override
    public String getApplicationName(UserId userId, String authority) {
        return mObservedAuthoritiesDetails.get(
                new UserAuthority(userId, authority)).applicationName;
    }

    @Override
    public String getPackageName(UserId userId, String authority) {
        return mObservedAuthoritiesDetails.get(new UserAuthority(userId, authority)).packageName;
    }

    public void updateAsync(boolean forceRefreshAll, @Nullable Runnable callback) {

        // NOTE: This method is called when the UI language changes.
        // For that reason we update our RecentsRoot to reflect
        // the current language.
        final String title = mContext.getString(R.string.root_recent);
        for (UserId userId : mUserIdManager.getUserIds()) {
            RootInfo recentRoot = createOrGetRecentsRoot(userId);
            recentRoot.title = title;
            // Nothing else about the root should ever change.
            assert (recentRoot.authority == null);
            assert (recentRoot.rootId == null);
            assert (recentRoot.derivedIcon == R.drawable.ic_root_recent);
            assert (recentRoot.derivedType == RootInfo.TYPE_RECENTS);
            assert (recentRoot.flags == (Root.FLAG_LOCAL_ONLY | Root.FLAG_SUPPORTS_IS_CHILD));
            assert (recentRoot.availableBytes == -1);
        }

        new UpdateTask(forceRefreshAll, null, callback).executeOnExecutor(
                AsyncTask.THREAD_POOL_EXECUTOR);
    }

    public void updatePackageAsync(UserId userId, String packageName) {
        new UpdateTask(false, new UserPackage(userId, packageName),
                /* callback= */ null).executeOnExecutor(AsyncTask.THREAD_POOL_EXECUTOR);
    }

    public void updateAuthorityAsync(UserId userId, String authority) {
        final ProviderInfo info = userId.getPackageManager(mContext).resolveContentProvider(
                authority, 0);
        if (info != null) {
            updatePackageAsync(userId, info.packageName);
        }
    }

    void setBootCompletedResult(PendingResult result) {
        synchronized (mLock) {
            // Quickly check if we've already finished loading, otherwise hang
            // out until first pass is finished.
            if (mFirstLoadDone) {
                result.finish();
            } else {
                mBootCompletedResult = result;
            }
        }
    }

    /**
     * Block until the first {@link UpdateTask} pass has finished.
     *
     * @return {@code true} if cached roots is ready to roll, otherwise
     * {@code false} if we timed out while waiting.
     */
    private boolean waitForFirstLoad() {
        boolean success = false;
        try {
            success = mFirstLoad.await(15, TimeUnit.SECONDS);
        } catch (InterruptedException e) {
        }
        if (!success) {
            Log.w(TAG, "Timeout waiting for first update");
        }
        return success;
    }

    /**
     * Load roots from authorities that are in stopped state. Normal
     * {@link UpdateTask} passes ignore stopped applications.
     */
    private void loadStoppedAuthorities() {
        synchronized (mLock) {
            for (UserAuthority userAuthority : mStoppedAuthorities) {
                mRoots.replaceValues(userAuthority, loadRootsForAuthority(userAuthority, true));
            }
            mStoppedAuthorities.clear();
        }
    }

    /**
     * Load roots from a stopped authority. Normal {@link UpdateTask} passes
     * ignore stopped applications.
     */
    private void loadStoppedAuthority(UserAuthority userAuthority) {
        synchronized (mLock) {
            if (!mStoppedAuthorities.contains(userAuthority)) {
                return;
            }
            if (DEBUG) {
                Log.d(TAG, "Loading stopped authority " + userAuthority);
            }
            mRoots.replaceValues(userAuthority, loadRootsForAuthority(userAuthority, true));
            mStoppedAuthorities.remove(userAuthority);
        }
    }

    /**
     * Bring up requested provider and query for all active roots. Will consult cached
     * roots if not forceRefresh. Will query when cached roots is empty (which should never happen).
     */
    private Collection<RootInfo> loadRootsForAuthority(UserAuthority userAuthority,
            boolean forceRefresh) {
        UserId userId = userAuthority.userId;
        String authority = userAuthority.authority;
        if (VERBOSE) Log.v(TAG, "Loading roots on user " + userId + " for " + authority);

        ContentResolver resolver = userId.getContentResolver(mContext);
        final ArrayList<RootInfo> roots = new ArrayList<>();
        final PackageManager pm = userId.getPackageManager(mContext);
        ProviderInfo provider = pm.resolveContentProvider(
                authority, PackageManager.GET_META_DATA);
        if (provider == null) {
            Log.w(TAG, "Failed to get provider info for " + authority);
            return roots;
        }
        if (!provider.exported) {
            Log.w(TAG, "Provider is not exported. Failed to load roots for " + authority);
            return roots;
        }
        if (!provider.grantUriPermissions) {
            Log.w(TAG, "Provider doesn't grantUriPermissions. Failed to load roots for "
                    + authority);
            return roots;
        }
        if (!android.Manifest.permission.MANAGE_DOCUMENTS.equals(provider.readPermission)
                || !android.Manifest.permission.MANAGE_DOCUMENTS.equals(provider.writePermission)) {
            Log.w(TAG, "Provider is not protected by MANAGE_DOCUMENTS. Failed to load roots for "
                    + authority);
            return roots;
        }

        synchronized (mObservedAuthoritiesDetails) {
            if (!mObservedAuthoritiesDetails.containsKey(userAuthority)) {
                CharSequence appName = pm.getApplicationLabel(provider.applicationInfo);
                String packageName = provider.applicationInfo.packageName;

                mObservedAuthoritiesDetails.put(
                        userAuthority, new PackageDetails(appName.toString(), packageName));

                // Watch for any future updates
                final Uri rootsUri = DocumentsContract.buildRootsUri(authority);
                resolver.registerContentObserver(rootsUri, true,
                        createOrGetRootsChangedObserver(userId));
            }
        }

        final Uri rootsUri = DocumentsContract.buildRootsUri(authority);
        if (!forceRefresh) {
            // Look for roots data that we might have cached for ourselves in the
            // long-lived system process.
            final Bundle systemCache = resolver.getCache(rootsUri);
            if (systemCache != null) {
                ArrayList<RootInfo> cachedRoots = systemCache.getParcelableArrayList(TAG);
                assert (cachedRoots != null);
                if (!cachedRoots.isEmpty() || PERMIT_EMPTY_CACHE.contains(authority)) {
                    if (VERBOSE) Log.v(TAG, "System cache hit for " + authority);
                    return cachedRoots;
                } else {
                    Log.w(TAG, "Ignoring empty system cache hit for " + authority);
                }
            }
        }

        ContentProviderClient client = null;
        Cursor cursor = null;
        try {
            client = DocumentsApplication.acquireUnstableProviderOrThrow(resolver, authority);
            cursor = client.query(rootsUri, null, null, null, null);
            while (cursor.moveToNext()) {
                final RootInfo root = RootInfo.fromRootsCursor(userId, authority, cursor);
                roots.add(root);
            }
        } catch (Exception e) {
            Log.w(TAG, "Failed to load some roots from " + authority, e);
            // We didn't load every root from the provider. Don't put it to
            // system cache so that we'll try loading them again next time even
            // if forceRefresh is false.
            return roots;
        } finally {
            FileUtils.closeQuietly(cursor);
            FileUtils.closeQuietly(client);
        }

        // Cache these freshly parsed roots over in the long-lived system
        // process, in case our process goes away. The system takes care of
        // invalidating the cache if the package or Uri changes.
        final Bundle systemCache = new Bundle();
        if (roots.isEmpty() && !PERMIT_EMPTY_CACHE.contains(authority)) {
            Log.i(TAG, "Provider returned no roots. Possibly naughty: " + authority);
        } else {
            systemCache.putParcelableArrayList(TAG, roots);
            resolver.putCache(rootsUri, systemCache);
        }

        return roots;
    }

    @Override
    public RootInfo getRootOneshot(UserId userId, String authority, String rootId) {
        return getRootOneshot(userId, authority, rootId, false);
    }

    public RootInfo getRootOneshot(UserId userId, String authority, String rootId,
            boolean forceRefresh) {
        synchronized (mLock) {
            UserAuthority userAuthority = new UserAuthority(userId, authority);
            RootInfo root = forceRefresh ? null : getRootLocked(userAuthority, rootId);
            if (root == null) {
                mRoots.replaceValues(userAuthority,
                        loadRootsForAuthority(userAuthority, forceRefresh));
                root = getRootLocked(userAuthority, rootId);
            }
            return root;
        }
    }

    public RootInfo getRootBlocking(UserId userId, String authority, String rootId) {
        waitForFirstLoad();
        loadStoppedAuthorities();
        synchronized (mLock) {
            return getRootLocked(new UserAuthority(userId, authority), rootId);
        }
    }

    private RootInfo getRootLocked(UserAuthority userAuthority, String rootId) {
        for (RootInfo root : mRoots.get(userAuthority)) {
            if (Objects.equals(root.rootId, rootId)) {
                return root;
            }
        }
        return null;
    }

    @Override
    public RootInfo getRecentsRoot(UserId userId) {
        return createOrGetRecentsRoot(userId);
    }

    public boolean isRecentsRoot(RootInfo root) {
        return mRecentsRoots.containsValue(root);
    }

    @Override
    public Collection<RootInfo> getRootsBlocking() {
        waitForFirstLoad();
        loadStoppedAuthorities();
        synchronized (mLock) {
            return mRoots.values();
        }
    }

    @Override
    public Collection<RootInfo> getMatchingRootsBlocking(State state) {
        waitForFirstLoad();
        loadStoppedAuthorities();
        synchronized (mLock) {
            return ProvidersAccess.getMatchingRoots(mRoots.values(), state);
        }
    }

    @Override
    public Collection<RootInfo> getRootsForAuthorityBlocking(UserId userId, String authority) {
        waitForFirstLoad();
        UserAuthority userAuthority = new UserAuthority(userId, authority);
        loadStoppedAuthority(userAuthority);
        synchronized (mLock) {
            final Collection<RootInfo> roots = mRoots.get(userAuthority);
            return roots != null ? roots : Collections.<RootInfo>emptyList();
        }
    }

    @Override
    public RootInfo getDefaultRootBlocking(State state) {
        RootInfo root = ProvidersAccess.getDefaultRoot(getRootsBlocking(), state);
        return root != null ? root : createOrGetRecentsRoot(UserId.CURRENT_USER);
    }

    public void logCache() {
        StringBuilder output = new StringBuilder();

        for (UserAuthority userAuthority : mObservedAuthoritiesDetails.keySet()) {
            List<String> roots = new ArrayList<>();
            Uri rootsUri = DocumentsContract.buildRootsUri(userAuthority.authority);
            Bundle systemCache = userAuthority.userId.getContentResolver(mContext).getCache(
                    rootsUri);
            if (systemCache != null) {
                ArrayList<RootInfo> cachedRoots = systemCache.getParcelableArrayList(TAG);
                for (RootInfo root : cachedRoots) {
                    roots.add(root.toDebugString());
                }
            }

            output.append((output.length() == 0) ? "System cache: " : ", ");
            output.append(userAuthority).append("=").append(roots);
        }

        Log.i(TAG, output.toString());
    }

    private class UpdateTask extends AsyncTask<Void, Void, Void> {
        private final boolean mForceRefreshAll;
        @Nullable
        private final UserPackage mForceRefreshUserPackage;
        @Nullable
        private final Runnable mCallback;

        private final Multimap<UserAuthority, RootInfo> mTaskRoots = ArrayListMultimap.create();
        private final HashSet<UserAuthority> mTaskStoppedAuthorities = new HashSet<>();

        /**
         * Create task to update roots cache.
         *
         * @param forceRefreshAll when true, all previously cached values for
         *            all packages should be ignored.
         * @param forceRefreshUserPackage when non-null, all previously cached
         *            values for this specific user package should be ignored.
         * @param callback when non-null, it will be invoked after the task is executed.
         */
        UpdateTask(boolean forceRefreshAll, @Nullable UserPackage forceRefreshUserPackage,
                @Nullable Runnable callback) {
            mForceRefreshAll = forceRefreshAll;
            mForceRefreshUserPackage = forceRefreshUserPackage;
            mCallback = callback;
        }

        @Override
        protected Void doInBackground(Void... params) {
            final long start = SystemClock.elapsedRealtime();

            for (UserId userId : mUserIdManager.getUserIds()) {
                final RootInfo recents = createOrGetRecentsRoot(userId);
                mTaskRoots.put(new UserAuthority(recents.userId, recents.authority), recents);

                final PackageManager pm = userId.getPackageManager(mContext);
                // Pick up provider with action string
                final Intent intent = new Intent(DocumentsContract.PROVIDER_INTERFACE);
                final List<ResolveInfo> providers = pm.queryIntentContentProviders(intent, 0);
                for (ResolveInfo info : providers) {
                    ProviderInfo providerInfo = info.providerInfo;
                    if (providerInfo.authority != null) {
                        handleDocumentsProvider(providerInfo, userId);
                    }
                }
            }

            final long delta = SystemClock.elapsedRealtime() - start;
            if (VERBOSE) Log.v(TAG,
                    "Update found " + mTaskRoots.size() + " roots in " + delta + "ms");
            synchronized (mLock) {
                mFirstLoadDone = true;
                if (mBootCompletedResult != null) {
                    mBootCompletedResult.finish();
                    mBootCompletedResult = null;
                }
                mRoots = mTaskRoots;
                mStoppedAuthorities = mTaskStoppedAuthorities;
            }
            mFirstLoad.countDown();
            LocalBroadcastManager.getInstance(mContext).sendBroadcast(new Intent(BROADCAST_ACTION));
            return null;
        }

        @Override
        protected void onPostExecute(Void aVoid) {
            if (mCallback != null) {
                mCallback.run();
            }
        }

        private void handleDocumentsProvider(ProviderInfo info, UserId userId) {
            UserAuthority userAuthority = new UserAuthority(userId, info.authority);
            // Ignore stopped packages for now; we might query them
            // later during UI interaction.
            if ((info.applicationInfo.flags & ApplicationInfo.FLAG_STOPPED) != 0) {
                if (VERBOSE) {
                    Log.v(TAG, "Ignoring stopped authority " + info.authority + ", user " + userId);
                }
                mTaskStoppedAuthorities.add(userAuthority);
                return;
            }

            final boolean forceRefresh = mForceRefreshAll
                    || Objects.equals(new UserPackage(userId, info.packageName),
                    mForceRefreshUserPackage);
            mTaskRoots.putAll(userAuthority, loadRootsForAuthority(userAuthority, forceRefresh));
        }

    }

    private static class UserAuthority {
        private final UserId userId;
        @Nullable
        private final String authority;

        private UserAuthority(UserId userId, @Nullable String authority) {
            this.userId = checkNotNull(userId);
            this.authority = authority;
        }

        @Override
        public boolean equals(Object o) {
            if (o == null) {
                return false;
            }

            if (this == o) {
                return true;
            }

            if (o instanceof UserAuthority) {
                UserAuthority other = (UserAuthority) o;
                return Objects.equals(userId, other.userId)
                        && Objects.equals(authority, other.authority);
            }

            return false;
        }


        @Override
        public int hashCode() {
            return Objects.hash(userId, authority);
        }
    }

    private static class PackageDetails {
        private String applicationName;
        private String packageName;

        public PackageDetails(String appName, String pckgName) {
            applicationName = appName;
            packageName = pckgName;
        }
    }
}
