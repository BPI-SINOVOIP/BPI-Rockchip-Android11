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
package com.android.tv.app;

import com.android.tv.common.flags.impl.DefaultFlagsModule;
import com.android.tv.data.epg.EpgReader;
import com.android.tv.data.epg.StubEpgReader;
import com.android.tv.modules.TvApplicationModule;
import com.android.tv.perf.PerformanceMonitor;
import com.android.tv.perf.stub.StubPerformanceMonitor;
import com.android.tv.tunerinputcontroller.BuiltInTunerManager;
import com.android.tv.ui.sidepanel.DeveloperOptionFragment;
import com.android.tv.util.account.AccountHelper;
import com.android.tv.util.account.AccountHelperImpl;
import com.google.common.base.Optional;
import dagger.Module;
import dagger.Provides;
import javax.inject.Singleton;

/** Dagger module for {@link LiveTvApplication}. */
@Module(includes = {DefaultFlagsModule.class, TvApplicationModule.class})
class LiveTvModule {

    @Provides
    static AccountHelper providesAccountHelper(AccountHelperImpl impl) {
        return impl;
    }

    @Provides
    static Optional<DeveloperOptionFragment.AdditionalDeveloperItemsFactory>
            providesAdditionalDeveloperItemsFactory() {
        return Optional.absent();
    }

    @Provides
    Optional<BuiltInTunerManager> providesBuiltInTunerManager() {
        return Optional.absent();
    }

    @Provides
    @Singleton
    PerformanceMonitor providesPerformanceMonitor() {
        return new StubPerformanceMonitor();
    }

    @Provides
    @Singleton
    static EpgReader providesEpgReader(StubEpgReader impl) {
        return impl;
    }
}
