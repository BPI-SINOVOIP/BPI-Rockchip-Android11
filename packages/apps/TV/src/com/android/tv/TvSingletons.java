/*
 * Copyright (C) 2015 The Android Open Source Project
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

package com.android.tv;

import android.content.Context;

import com.android.tv.analytics.Analytics;
import com.android.tv.analytics.Tracker;
import com.android.tv.common.BaseApplication;
import com.android.tv.common.BaseSingletons;
import com.android.tv.common.flags.has.HasUiFlags;
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
import com.android.tv.tunerinputcontroller.HasBuiltInTunerManager;
import com.android.tv.util.SetupUtils;
import com.android.tv.util.TvInputManagerHelper;

import dagger.Lazy;

import com.android.tv.common.flags.BackendKnobsFlags;

import java.util.concurrent.Executor;

/** Interface with getters for application scoped singletons. */
public interface TvSingletons extends BaseSingletons, HasBuiltInTunerManager, HasUiFlags {

    /*
     * Do not add any new methods here.
     *
     * To move a getter to Injection.
     *  1. Make a type injectable @Singleton.
     *  2. Mark the getter here as deprecated.
     *  3. Lazily inject the object in TvApplication.
     *  4. Move easy usages of getters to injection instead.
     *  5. Delete the method when all usages are migrated.
     */

    /**
     * Returns the @{@link TvSingletons} using the application context.
     *
     * @deprecated use injection instead.
     */
    @Deprecated
    static TvSingletons getSingletons(Context context) {
        return (TvSingletons) BaseApplication.getSingletons(context);
    }

    Analytics getAnalytics();

    void handleInputCountChanged();

    @Deprecated
    ChannelDataManager getChannelDataManager();

    /** @deprecated use injection instead. */
    @Deprecated
    ProgramDataManager getProgramDataManager();

    PreviewDataManager getPreviewDataManager();

    /** @deprecated use injection instead. */
    @Deprecated
    DvrDataManager getDvrDataManager();

    DvrScheduleManager getDvrScheduleManager();

    DvrManager getDvrManager();

    RecordingScheduler getRecordingScheduler();

    /** @deprecated use injection instead. */
    @Deprecated
    DvrWatchedPositionManager getDvrWatchedPositionManager();

    InputSessionManager getInputSessionManager();

    Tracker getTracker();

    MainActivityWrapper getMainActivityWrapper();

    boolean isRunningInMainProcess();

    /** @deprecated use injection instead. */
    @Deprecated
    PerformanceMonitor getPerformanceMonitor();

    /** @deprecated use injection instead. */
    @Deprecated
    TvInputManagerHelper getTvInputManagerHelper();

    Lazy<EpgReader> providesEpgReader();

    /** @deprecated use injection instead. */
    @Deprecated
    SetupUtils getSetupUtils();

    /** @deprecated use injection instead. */
    @Deprecated
    Executor getDbExecutor();

    BackendKnobsFlags getBackendKnobs();
}
