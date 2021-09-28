/*
 * Copyright (C) 2008 The Android Open Source Project
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

package com.android.internal.app;

import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.when;

import android.annotation.Nullable;
import android.app.usage.UsageStatsManager;
import android.content.ContentResolver;
import android.content.Context;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.content.pm.ResolveInfo;
import android.content.res.Resources;
import android.database.Cursor;
import android.graphics.Bitmap;
import android.net.Uri;
import android.os.UserHandle;
import android.util.Size;

import com.android.internal.app.ResolverListAdapter.ResolveInfoPresentationGetter;
import com.android.internal.app.chooser.DisplayResolveInfo;
import com.android.internal.app.chooser.TargetInfo;
import com.android.internal.logging.MetricsLogger;
import com.android.internal.logging.nano.MetricsProto.MetricsEvent;

import java.util.List;
import java.util.function.Function;

public class ChooserWrapperActivity extends ChooserActivity {
    /*
     * Simple wrapper around chooser activity to be able to initiate it under test
     */
    static final OverrideData sOverrides = new OverrideData();
    private UsageStatsManager mUsm;

    @Override
    protected AbstractMultiProfilePagerAdapter createMultiProfilePagerAdapter(
            Intent[] initialIntents, List<ResolveInfo> rList, boolean filterLastUsed) {
        AbstractMultiProfilePagerAdapter multiProfilePagerAdapter =
                super.createMultiProfilePagerAdapter(initialIntents, rList, filterLastUsed);
        multiProfilePagerAdapter.setInjector(sOverrides.multiPagerAdapterInjector);
        return multiProfilePagerAdapter;
    }

    @Override
    public ChooserListAdapter createChooserListAdapter(Context context, List<Intent> payloadIntents,
            Intent[] initialIntents, List<ResolveInfo> rList, boolean filterLastUsed,
            ResolverListController resolverListController) {
        PackageManager packageManager =
                sOverrides.packageManager == null ? context.getPackageManager()
                        : sOverrides.packageManager;
        return new ChooserListAdapter(context, payloadIntents, initialIntents, rList,
                filterLastUsed, resolverListController,
                this, this, packageManager);
    }

    ChooserListAdapter getAdapter() {
        return mChooserMultiProfilePagerAdapter.getActiveListAdapter();
    }

    ChooserListAdapter getPersonalListAdapter() {
        return ((ChooserGridAdapter) mMultiProfilePagerAdapter.getAdapterForIndex(0))
                .getListAdapter();
    }

    ChooserListAdapter getWorkListAdapter() {
        if (mMultiProfilePagerAdapter.getInactiveListAdapter() == null) {
            return null;
        }
        return ((ChooserGridAdapter) mMultiProfilePagerAdapter.getAdapterForIndex(1))
                .getListAdapter();
    }

    boolean getIsSelected() { return mIsSuccessfullySelected; }

    UsageStatsManager getUsageStatsManager() {
        if (mUsm == null) {
            mUsm = (UsageStatsManager) getSystemService(Context.USAGE_STATS_SERVICE);
        }
        return mUsm;
    }

    @Override
    public boolean isVoiceInteraction() {
        if (sOverrides.isVoiceInteraction != null) {
            return sOverrides.isVoiceInteraction;
        }
        return super.isVoiceInteraction();
    }

    @Override
    public void safelyStartActivity(com.android.internal.app.chooser.TargetInfo cti) {
        if (sOverrides.onSafelyStartCallback != null &&
                sOverrides.onSafelyStartCallback.apply(cti)) {
            return;
        }
        super.safelyStartActivity(cti);
    }

    @Override
    protected ResolverListController createListController(UserHandle userHandle) {
        if (userHandle == UserHandle.SYSTEM) {
            when(sOverrides.resolverListController.getUserHandle()).thenReturn(UserHandle.SYSTEM);
            return sOverrides.resolverListController;
        }
        when(sOverrides.workResolverListController.getUserHandle()).thenReturn(userHandle);
        return sOverrides.workResolverListController;
    }

    @Override
    public PackageManager getPackageManager() {
        if (sOverrides.createPackageManager != null) {
            return sOverrides.createPackageManager.apply(super.getPackageManager());
        }
        return super.getPackageManager();
    }

    @Override
    public Resources getResources() {
        if (sOverrides.resources != null) {
            return sOverrides.resources;
        }
        return super.getResources();
    }

    @Override
    protected Bitmap loadThumbnail(Uri uri, Size size) {
        if (sOverrides.previewThumbnail != null) {
            return sOverrides.previewThumbnail;
        }
        return super.loadThumbnail(uri, size);
    }

    @Override
    protected boolean isImageType(String mimeType) {
        return sOverrides.isImageType;
    }

    @Override
    protected MetricsLogger getMetricsLogger() {
        return sOverrides.metricsLogger;
    }

    @Override
    protected ChooserActivityLogger getChooserActivityLogger() {
        return sOverrides.chooserActivityLogger;
    }

    @Override
    public Cursor queryResolver(ContentResolver resolver, Uri uri) {
        if (sOverrides.resolverCursor != null) {
            return sOverrides.resolverCursor;
        }

        if (sOverrides.resolverForceException) {
            throw new SecurityException("Test exception handling");
        }

        return super.queryResolver(resolver, uri);
    }

    @Override
    protected boolean isWorkProfile() {
        if (sOverrides.alternateProfileSetting != 0) {
            return sOverrides.alternateProfileSetting == MetricsEvent.MANAGED_PROFILE;
        }
        return super.isWorkProfile();
    }

    public DisplayResolveInfo createTestDisplayResolveInfo(Intent originalIntent, ResolveInfo pri,
            CharSequence pLabel, CharSequence pInfo, Intent replacementIntent,
            @Nullable ResolveInfoPresentationGetter resolveInfoPresentationGetter) {
        return new DisplayResolveInfo(originalIntent, pri, pLabel, pInfo, replacementIntent,
                resolveInfoPresentationGetter);
    }

    @Override
    protected UserHandle getWorkProfileUserHandle() {
        return sOverrides.workProfileUserHandle;
    }

    protected UserHandle getCurrentUserHandle() {
        return mMultiProfilePagerAdapter.getCurrentUserHandle();
    }

    @Override
    public Context createContextAsUser(UserHandle user, int flags) {
        // return the current context as a work profile doesn't really exist in these tests
        return getApplicationContext();
    }

    @Override
    protected void queryDirectShareTargets(ChooserListAdapter adapter,
            boolean skipAppPredictionService) {
        if (sOverrides.onQueryDirectShareTargets != null) {
            sOverrides.onQueryDirectShareTargets.apply(adapter);
        }
        super.queryDirectShareTargets(adapter, skipAppPredictionService);
    }

    @Override
    protected void queryTargetServices(ChooserListAdapter adapter) {
        if (sOverrides.onQueryTargetServices != null) {
            sOverrides.onQueryTargetServices.apply(adapter);
        }
        super.queryTargetServices(adapter);
    }

    @Override
    protected boolean isQuietModeEnabled(UserHandle userHandle) {
        return sOverrides.isQuietModeEnabled;
    }

    @Override
    protected boolean isUserRunning(UserHandle userHandle) {
        if (userHandle.equals(UserHandle.SYSTEM)) {
            return super.isUserRunning(userHandle);
        }
        return sOverrides.isWorkProfileUserRunning;
    }

    @Override
    protected boolean isUserUnlocked(UserHandle userHandle) {
        if (userHandle.equals(UserHandle.SYSTEM)) {
            return super.isUserUnlocked(userHandle);
        }
        return sOverrides.isWorkProfileUserUnlocked;
    }

    /**
     * We cannot directly mock the activity created since instrumentation creates it.
     * <p>
     * Instead, we use static instances of this object to modify behavior.
     */
    static class OverrideData {
        @SuppressWarnings("Since15")
        public Function<PackageManager, PackageManager> createPackageManager;
        public Function<TargetInfo, Boolean> onSafelyStartCallback;
        public Function<ChooserListAdapter, Void> onQueryDirectShareTargets;
        public Function<ChooserListAdapter, Void> onQueryTargetServices;
        public ResolverListController resolverListController;
        public ResolverListController workResolverListController;
        public Boolean isVoiceInteraction;
        public boolean isImageType;
        public Cursor resolverCursor;
        public boolean resolverForceException;
        public Bitmap previewThumbnail;
        public MetricsLogger metricsLogger;
        public ChooserActivityLogger chooserActivityLogger;
        public int alternateProfileSetting;
        public Resources resources;
        public UserHandle workProfileUserHandle;
        public boolean hasCrossProfileIntents;
        public boolean isQuietModeEnabled;
        public boolean isWorkProfileUserRunning;
        public boolean isWorkProfileUserUnlocked;
        public AbstractMultiProfilePagerAdapter.Injector multiPagerAdapterInjector;
        public PackageManager packageManager;

        public void reset() {
            onSafelyStartCallback = null;
            onQueryDirectShareTargets = null;
            onQueryTargetServices = null;
            isVoiceInteraction = null;
            createPackageManager = null;
            previewThumbnail = null;
            isImageType = false;
            resolverCursor = null;
            resolverForceException = false;
            resolverListController = mock(ResolverListController.class);
            workResolverListController = mock(ResolverListController.class);
            metricsLogger = mock(MetricsLogger.class);
            chooserActivityLogger = new ChooserActivityLoggerFake();
            alternateProfileSetting = 0;
            resources = null;
            workProfileUserHandle = null;
            hasCrossProfileIntents = true;
            isQuietModeEnabled = false;
            isWorkProfileUserRunning = true;
            isWorkProfileUserUnlocked = true;
            packageManager = null;
            multiPagerAdapterInjector = new AbstractMultiProfilePagerAdapter.Injector() {
                @Override
                public boolean hasCrossProfileIntents(List<Intent> intents, int sourceUserId,
                        int targetUserId) {
                    return hasCrossProfileIntents;
                }

                @Override
                public boolean isQuietModeEnabled(UserHandle workProfileUserHandle) {
                    return isQuietModeEnabled;
                }

                @Override
                public void requestQuietModeEnabled(boolean enabled,
                        UserHandle workProfileUserHandle) {
                    isQuietModeEnabled = enabled;
                }
            };
        }
    }
}
