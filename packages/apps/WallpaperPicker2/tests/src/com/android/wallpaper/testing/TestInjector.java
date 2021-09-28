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
 * limitations under the License.
 */
package com.android.wallpaper.testing;

import android.content.Context;
import android.content.Intent;
import android.net.Uri;

import androidx.fragment.app.Fragment;

import com.android.wallpaper.compat.WallpaperManagerCompat;
import com.android.wallpaper.model.CategoryProvider;
import com.android.wallpaper.model.WallpaperInfo;
import com.android.wallpaper.module.AlarmManagerWrapper;
import com.android.wallpaper.module.BitmapCropper;
import com.android.wallpaper.module.CurrentWallpaperInfoFactory;
import com.android.wallpaper.module.DefaultLiveWallpaperInfoFactory;
import com.android.wallpaper.module.DrawableLayerResolver;
import com.android.wallpaper.module.ExploreIntentChecker;
import com.android.wallpaper.module.FormFactorChecker;
import com.android.wallpaper.module.Injector;
import com.android.wallpaper.module.LiveWallpaperInfoFactory;
import com.android.wallpaper.module.LoggingOptInStatusProvider;
import com.android.wallpaper.module.NetworkStatusNotifier;
import com.android.wallpaper.module.PackageStatusNotifier;
import com.android.wallpaper.module.PartnerProvider;
import com.android.wallpaper.module.SystemFeatureChecker;
import com.android.wallpaper.module.UserEventLogger;
import com.android.wallpaper.module.WallpaperPersister;
import com.android.wallpaper.module.WallpaperPreferences;
import com.android.wallpaper.module.WallpaperRefresher;
import com.android.wallpaper.module.WallpaperRotationRefresher;
import com.android.wallpaper.monitor.PerformanceMonitor;
import com.android.wallpaper.network.Requester;
import com.android.wallpaper.picker.ImagePreviewFragment;
import com.android.wallpaper.picker.individual.IndividualPickerFragment;

/**
 * Test implementation of the dependency injector.
 */
public class TestInjector implements Injector {

    private BitmapCropper mBitmapCropper;
    private CategoryProvider mCategoryProvider;
    private PartnerProvider mPartnerProvider;
    private WallpaperPreferences mPrefs;
    private WallpaperPersister mWallpaperPersister;
    private WallpaperRefresher mWallpaperRefresher;
    private Requester mRequester;
    private WallpaperManagerCompat mWallpaperManagerCompat;
    private CurrentWallpaperInfoFactory mCurrentWallpaperInfoFactory;
    private NetworkStatusNotifier mNetworkStatusNotifier;
    private AlarmManagerWrapper mAlarmManagerWrapper;
    private UserEventLogger mUserEventLogger;
    private ExploreIntentChecker mExploreIntentChecker;
    private SystemFeatureChecker mSystemFeatureChecker;
    private FormFactorChecker mFormFactorChecker;
    private WallpaperRotationRefresher mWallpaperRotationRefresher;
    private PerformanceMonitor mPerformanceMonitor;
    private LoggingOptInStatusProvider mLoggingOptInStatusProvider;

    @Override
    public BitmapCropper getBitmapCropper() {
        if (mBitmapCropper == null) {
            mBitmapCropper = new com.android.wallpaper.testing.TestBitmapCropper();
        }
        return mBitmapCropper;
    }

    @Override
    public CategoryProvider getCategoryProvider(Context context) {
        if (mCategoryProvider == null) {
            mCategoryProvider = new TestCategoryProvider();
        }
        return mCategoryProvider;
    }

    @Override
    public PartnerProvider getPartnerProvider(Context context) {
        if (mPartnerProvider == null) {
            mPartnerProvider = new TestPartnerProvider();
        }
        return mPartnerProvider;
    }

    @Override
    public WallpaperPreferences getPreferences(Context context) {
        if (mPrefs == null) {
            mPrefs = new TestWallpaperPreferences();
        }
        return mPrefs;
    }

    @Override
    public WallpaperPersister getWallpaperPersister(Context context) {
        if (mWallpaperPersister == null) {
            mWallpaperPersister = new TestWallpaperPersister(context.getApplicationContext());
        }
        return mWallpaperPersister;
    }

    @Override
    public WallpaperRefresher getWallpaperRefresher(Context context) {
        if (mWallpaperRefresher == null) {
            mWallpaperRefresher = new TestWallpaperRefresher(context.getApplicationContext());
        }
        return mWallpaperRefresher;
    }

    @Override
    public Requester getRequester(Context unused) {
        return null;
    }

    @Override
    public WallpaperManagerCompat getWallpaperManagerCompat(Context context) {
        if (mWallpaperManagerCompat == null) {
            mWallpaperManagerCompat = new com.android.wallpaper.testing.TestWallpaperManagerCompat(
                    context.getApplicationContext());
        }
        return mWallpaperManagerCompat;
    }

    @Override
    public CurrentWallpaperInfoFactory getCurrentWallpaperFactory(Context context) {
        if (mCurrentWallpaperInfoFactory == null) {
            mCurrentWallpaperInfoFactory =
                    new TestCurrentWallpaperInfoFactory(context.getApplicationContext());
        }
        return mCurrentWallpaperInfoFactory;
    }

    @Override
    public LoggingOptInStatusProvider getLoggingOptInStatusProvider(Context context) {
        if (mLoggingOptInStatusProvider == null) {
            mLoggingOptInStatusProvider = new TestLoggingOptInStatusProvider();
        }
        return mLoggingOptInStatusProvider;
    }

    @Override
    public NetworkStatusNotifier getNetworkStatusNotifier(Context context) {
        if (mNetworkStatusNotifier == null) {
            mNetworkStatusNotifier = new TestNetworkStatusNotifier();
        }
        return mNetworkStatusNotifier;
    }

    @Override
    public AlarmManagerWrapper getAlarmManagerWrapper(Context unused) {
        if (mAlarmManagerWrapper == null) {
            mAlarmManagerWrapper = new TestAlarmManagerWrapper();
        }
        return mAlarmManagerWrapper;
    }

    @Override
    public UserEventLogger getUserEventLogger(Context unused) {
        if (mUserEventLogger == null) {
            mUserEventLogger = new com.android.wallpaper.testing.TestUserEventLogger();
        }
        return mUserEventLogger;
    }

    @Override
    public ExploreIntentChecker getExploreIntentChecker(Context unused) {
        if (mExploreIntentChecker == null) {
            mExploreIntentChecker = new TestExploreIntentChecker();
        }
        return mExploreIntentChecker;
    }

    @Override
    public SystemFeatureChecker getSystemFeatureChecker() {
        if (mSystemFeatureChecker == null) {
            mSystemFeatureChecker = new com.android.wallpaper.testing.TestSystemFeatureChecker();
        }
        return mSystemFeatureChecker;
    }

    @Override
    public FormFactorChecker getFormFactorChecker(Context unused) {
        if (mFormFactorChecker == null) {
            mFormFactorChecker = new TestFormFactorChecker();
        }
        return mFormFactorChecker;
    }

    @Override
    public WallpaperRotationRefresher getWallpaperRotationRefresher() {
        if (mWallpaperRotationRefresher == null) {
            mWallpaperRotationRefresher = (context, listener) -> {
                // Not implemented
                listener.onError();
            };
        }
        return mWallpaperRotationRefresher;
    }

    @Override
    public Fragment getPreviewFragment(Context context, WallpaperInfo wallpaperInfo, int mode,
            boolean viewAsHome, boolean testingModeEnabled) {
        return ImagePreviewFragment.newInstance(wallpaperInfo, mode, viewAsHome,
                testingModeEnabled);
    }

    @Override
    public PackageStatusNotifier getPackageStatusNotifier(Context context) {
        return null;
    }

    @Override
    public IndividualPickerFragment getIndividualPickerFragment(String collectionId) {
        return IndividualPickerFragment.newInstance(collectionId);
    }

    @Override
    public LiveWallpaperInfoFactory getLiveWallpaperInfoFactory(Context context) {
        return new DefaultLiveWallpaperInfoFactory();
    }

    @Override
    public DrawableLayerResolver getDrawableLayerResolver() {
        return null;
    }

    @Override
    public Intent getDeepLinkRedirectIntent(Context context, Uri uri) {
        return null;
    }

    @Override
    public PerformanceMonitor getPerformanceMonitor() {
        if (mPerformanceMonitor == null) {
            mPerformanceMonitor = new TestPerformanceMonitor();
        }
        return mPerformanceMonitor;
    }
}
