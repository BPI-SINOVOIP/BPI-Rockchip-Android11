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
package com.android.tv.modules;

import android.content.Context;

import com.android.tv.MainActivity;
import com.android.tv.SetupPassthroughActivity;
import com.android.tv.TvApplication;
import com.android.tv.common.buildtype.BuildTypeModule;
import com.android.tv.common.concurrent.NamedThreadFactory;
import com.android.tv.common.dagger.ApplicationModule;
import com.android.tv.common.dagger.annotations.ApplicationContext;
import com.android.tv.data.ChannelDataManager;
import com.android.tv.data.ChannelDataManagerFactory;
import com.android.tv.data.epg.EpgFetchService;
import com.android.tv.data.epg.EpgFetcher;
import com.android.tv.data.epg.EpgFetcherImpl;
import com.android.tv.dialog.PinDialogFragment;
import com.android.tv.dvr.DvrDataManager;
import com.android.tv.dvr.DvrDataManagerImpl;
import com.android.tv.dvr.DvrManager;
import com.android.tv.dvr.WritableDvrDataManager;
import com.android.tv.dvr.provider.DvrDbFuture.DvrQueryScheduleFuture;
import com.android.tv.dvr.provider.DvrDbSync;
import com.android.tv.dvr.provider.DvrDbSyncFactory;
import com.android.tv.dvr.provider.DvrQueryScheduleFutureFactory;
import com.android.tv.dvr.ui.playback.DvrPlaybackActivity;
import com.android.tv.menu.MenuRowFactory;
import com.android.tv.menu.MenuRowFactoryFactory;
import com.android.tv.menu.TvOptionsRowAdapter;
import com.android.tv.menu.TvOptionsRowAdapterFactory;
import com.android.tv.onboarding.OnboardingActivity;
import com.android.tv.onboarding.SetupSourcesFragment;
import com.android.tv.setup.SystemSetupActivity;
import com.android.tv.ui.DetailsActivity;
import com.android.tv.util.AsyncDbTask;
import com.android.tv.util.TvInputManagerHelper;

import dagger.Binds;
import dagger.Module;
import dagger.Provides;
import dagger.android.ContributesAndroidInjector;

import com.android.tv.common.flags.LegacyFlags;

import java.util.concurrent.Executor;
import java.util.concurrent.Executors;

import javax.inject.Singleton;

/** Dagger module for {@link TvApplication}. */
@Module(
        includes = {
            ApplicationModule.class,
            BuildTypeModule.class,
            DetailsActivity.Module.class,
            DvrPlaybackActivity.Module.class,
            MainActivity.Module.class,
            OnboardingActivity.Module.class,
            SetupPassthroughActivity.Module.class,
            SetupSourcesFragment.ContentFragment.Module.class,
            SystemSetupActivity.Module.class,
            TvSingletonsModule.class,
        })
public abstract class TvApplicationModule {
    private static final NamedThreadFactory THREAD_FACTORY = new NamedThreadFactory("tv-app-db");

    @Provides
    @AsyncDbTask.DbExecutor
    @Singleton
    static Executor providesDbExecutor() {
        return Executors.newSingleThreadExecutor(THREAD_FACTORY);
    }

    @Provides
    @Singleton
    static TvInputManagerHelper providesTvInputManagerHelper(
            @ApplicationContext Context context, LegacyFlags legacyFlags) {
        TvInputManagerHelper tvInputManagerHelper = new TvInputManagerHelper(context, legacyFlags);
        tvInputManagerHelper.start();
        // Since this is injected as a Lazy in the application start is delayed.
        return tvInputManagerHelper;
    }

    @Provides
    @Singleton
    static ChannelDataManager providesChannelDataManager(ChannelDataManagerFactory factory) {
        ChannelDataManager channelDataManager = factory.create();
        channelDataManager.start();
        // Since this is injected as a Lazy in the application start is delayed.
        return channelDataManager;
    }

    @Provides
    @Singleton
    static DvrManager providesDvrManager(@ApplicationContext Context context) {
        return new DvrManager(context);
    }

    @Binds
    @Singleton
    abstract DvrDataManager providesDvrDataManager(DvrDataManagerImpl impl);

    @Binds
    @Singleton
    abstract WritableDvrDataManager providesWritableDvrDataManager(DvrDataManagerImpl impl);

    @Binds
    @Singleton
    abstract EpgFetcher epgFetcher(EpgFetcherImpl impl);

    @Binds
    abstract DvrDbSync.Factory dvrDbSyncFactory(DvrDbSyncFactory dvrDbSyncFactory);

    @Binds
    abstract DvrQueryScheduleFuture.Factory dvrQueryScheduleFutureFactory(
            DvrQueryScheduleFutureFactory dvrQueryScheduleFutureFactory);

    @Binds
    abstract TvOptionsRowAdapter.Factory tvOptionsRowAdapterFactory(
            TvOptionsRowAdapterFactory impl);

    @Binds
    abstract MenuRowFactory.Factory menuRowFactoryFactory(MenuRowFactoryFactory impl);

    @ContributesAndroidInjector
    abstract PinDialogFragment contributesPinDialogFragment();

    @ContributesAndroidInjector
    abstract EpgFetchService contributesEpgFetchService();
}
