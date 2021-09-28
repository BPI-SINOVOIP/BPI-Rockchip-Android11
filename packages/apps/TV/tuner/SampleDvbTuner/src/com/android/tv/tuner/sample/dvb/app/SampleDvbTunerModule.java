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
package com.android.tv.tuner.sample.dvb.app;

import com.android.tv.common.dagger.ApplicationModule;
import com.android.tv.common.flags.impl.DefaultFlagsModule;
import com.android.tv.tuner.api.TunerFactory;
import com.android.tv.tuner.dvb.DvbTunerHalFactory;
import com.android.tv.tuner.modules.TunerModule;
import com.android.tv.tuner.sample.dvb.setup.SampleDvbTunerSetupActivity;
import com.android.tv.tuner.sample.dvb.tvinput.SampleDvbTunerTvInputService;

import dagger.Module;
import dagger.Provides;

/** Dagger module for {@link SampleDvbTuner}. */
@Module(
        includes = {
            ApplicationModule.class,
            DefaultFlagsModule.class,
            SampleDvbTunerTvInputService.Module.class,
            SampleDvbTunerSetupActivity.Module.class,
            TunerModule.class,
        })
class SampleDvbTunerModule {

    @Provides
    TunerFactory providesTunerFactory() {
        return DvbTunerHalFactory.INSTANCE;
    }
}
