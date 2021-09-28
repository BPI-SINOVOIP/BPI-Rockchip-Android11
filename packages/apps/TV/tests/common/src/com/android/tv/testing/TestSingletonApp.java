/*
 * Copyright (C) 2017 The Android Open Source Project
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
 * limitations under the License
 */

package com.android.tv.testing;

import android.app.Application;
import android.media.tv.TvInputManager;
import android.os.AsyncTask;

import com.android.tv.InputSessionManager;
import com.android.tv.MainActivityWrapper;
import com.android.tv.TvSingletons;
import com.android.tv.analytics.Analytics;
import com.android.tv.analytics.Tracker;
import com.android.tv.common.BaseApplication;
import com.android.tv.common.flags.impl.DefaultBackendKnobsFlags;
import com.android.tv.common.flags.impl.DefaultCloudEpgFlags;
import com.android.tv.common.flags.impl.DefaultUiFlags;
import com.android.tv.common.flags.impl.SettableFlagsModule;
import com.android.tv.common.recording.RecordingStorageStatusManager;
import com.android.tv.common.singletons.HasSingletons;
import com.android.tv.common.util.Clock;
import com.android.tv.data.ChannelDataManager;
import com.android.tv.data.PreviewDataManager;
import com.android.tv.data.ProgramDataManager;
import com.android.tv.data.epg.EpgReader;
import com.android.tv.dvr.DvrDataManager;
import com.android.tv.dvr.DvrManager;
import com.android.tv.dvr.DvrScheduleManager;
import com.android.tv.dvr.DvrWatchedPositionManager;
import com.android.tv.dvr.recorder.RecordingScheduler;
import com.android.tv.perf.PerformanceMonitor;
import com.android.tv.perf.stub.StubPerformanceMonitor;
import com.android.tv.testing.dvr.DvrDataManagerInMemoryImpl;
import com.android.tv.testing.fakes.FakeClock;
import com.android.tv.testing.testdata.TestData;
import com.android.tv.tuner.singletons.TunerSingletons;
import com.android.tv.tunerinputcontroller.BuiltInTunerManager;
import com.android.tv.util.AsyncDbTask.DbExecutor;
import com.android.tv.util.SetupUtils;
import com.android.tv.util.TvInputManagerHelper;

import com.google.common.base.Optional;

import dagger.Lazy;

import java.util.concurrent.Executor;

/** Test application for TV app. */
public class TestSingletonApp extends Application
        implements TvSingletons, TunerSingletons, HasSingletons<TvSingletons> {
    public final FakeClock fakeClock = FakeClock.createWithCurrentTime();
    public final FakeEpgReader epgReader = new FakeEpgReader(fakeClock);
    public final FakeEpgFetcher epgFetcher = new FakeEpgFetcher();
    public final SettableFlagsModule flagsModule = new SettableFlagsModule();

    public FakeTvInputManagerHelper tvInputManagerHelper;
    public SetupUtils setupUtils;
    public DvrManager dvrManager;
    public DvrDataManager mDvrDataManager;
    @DbExecutor public Executor dbExecutor = AsyncTask.SERIAL_EXECUTOR;

    private final Lazy<EpgReader> mEpgReaderProvider = () -> epgReader;
    private final Optional<BuiltInTunerManager> mBuiltInTunerManagerOptional = Optional.absent();

    private final PerformanceMonitor mPerformanceMonitor = new StubPerformanceMonitor();
    private ChannelDataManager mChannelDataManager;

    @Override
    public void onCreate() {
        super.onCreate();
        tvInputManagerHelper = new FakeTvInputManagerHelper(this);
        setupUtils = new SetupUtils(this, mBuiltInTunerManagerOptional);
        tvInputManagerHelper.start();
        mChannelDataManager =
                new ChannelDataManager(
                        this, tvInputManagerHelper, dbExecutor, getContentResolver());
        mChannelDataManager.start();
        mDvrDataManager = new DvrDataManagerInMemoryImpl(this, fakeClock);
        // HACK reset the singleton for tests
        BaseApplication.sSingletons = this;
    }

    public void loadTestData(TestData testData, long durationMs) {
        tvInputManagerHelper
                .getFakeTvInputManager()
                .add(testData.getTvInputInfo(), TvInputManager.INPUT_STATE_CONNECTED);
        testData.init(this, fakeClock, durationMs);
    }

    @Override
    public Analytics getAnalytics() {
        return null;
    }

    @Override
    public void handleInputCountChanged() {}

    @Override
    public ChannelDataManager getChannelDataManager() {
        return mChannelDataManager;
    }

    @Override
    public ProgramDataManager getProgramDataManager() {
        return null;
    }

    @Override
    public PreviewDataManager getPreviewDataManager() {
        return null;
    }

    @Override
    public DvrDataManager getDvrDataManager() {
        return mDvrDataManager;
    }

    @Override
    public DvrScheduleManager getDvrScheduleManager() {
        return null;
    }

    @Override
    public DvrManager getDvrManager() {
        return dvrManager;
    }

    @Override
    public RecordingScheduler getRecordingScheduler() {
        return null;
    }

    @Override
    public DvrWatchedPositionManager getDvrWatchedPositionManager() {
        return null;
    }

    @Override
    public InputSessionManager getInputSessionManager() {
        return new InputSessionManager(this);
    }

    @Override
    public Tracker getTracker() {
        return null;
    }

    @Override
    public TvInputManagerHelper getTvInputManagerHelper() {
        return tvInputManagerHelper;
    }

    @Override
    public Lazy<EpgReader> providesEpgReader() {
        return mEpgReaderProvider;
    }

    @Override
    public SetupUtils getSetupUtils() {
        return setupUtils;
    }

    @Override
    public Optional<BuiltInTunerManager> getBuiltInTunerManager() {
        return mBuiltInTunerManagerOptional;
    }

    @Override
    public MainActivityWrapper getMainActivityWrapper() {
        return null;
    }

    @Override
    public Clock getClock() {
        return fakeClock;
    }

    @Override
    public RecordingStorageStatusManager getRecordingStorageStatusManager() {
        return null;
    }

    @Override
    public boolean isRunningInMainProcess() {
        return false;
    }

    @Override
    public PerformanceMonitor getPerformanceMonitor() {
        return mPerformanceMonitor;
    }

    @Override
    public String getEmbeddedTunerInputId() {
        return "com.android.tv/.tuner.tvinput.TunerTvInputService";
    }

    @Override
    public Executor getDbExecutor() {
        return dbExecutor;
    }

    @Override
    public DefaultBackendKnobsFlags getBackendKnobs() {
        return flagsModule.backendKnobsFlags;
    }

    @Override
    public DefaultCloudEpgFlags getCloudEpgFlags() {
        return flagsModule.cloudEpgFlags;
    }

    @Override
    public DefaultUiFlags getUiFlags() {
        return flagsModule.uiFlags;
    }

    @Override
    public BuildType getBuildType() {
        return BuildType.ENG;
    }

    @Override
    public TvSingletons singletons() {
        return this;
    }
}
