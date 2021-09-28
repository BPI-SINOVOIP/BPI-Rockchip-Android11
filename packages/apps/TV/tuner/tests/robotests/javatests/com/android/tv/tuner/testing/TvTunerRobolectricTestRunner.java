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
 * limitations under the License
 */

package com.android.tv.tuner.testing;

import org.junit.runners.model.InitializationError;
import org.robolectric.RobolectricTestRunner;
import org.robolectric.annotation.Config;
import org.robolectric.manifest.AndroidManifest;
import org.robolectric.res.Fs;
import org.robolectric.res.ResourcePath;

import java.util.List;

/**
 * Custom test runner TV tuner. This is needed because the default behavior for robolectric is just
 * to grab the resource directory in the target package. We want to override this to add several
 * spanning different projects.
 *
 * <p><b>Note</b> copied from
 * http://cs/android/packages/apps/Settings/tests/robotests/src/com/android/settings/testutils/SettingsRobolectricTestRunner.java
 */
public class TvTunerRobolectricTestRunner extends RobolectricTestRunner {

    /** We don't actually want to change this behavior, so we just call super. */
    public TvTunerRobolectricTestRunner(Class<?> testClass) throws InitializationError {
        super(testClass);
    }

    /**
     * We are going to create our own custom manifest so that we can add multiple resource paths to
     * it.
     */
    @Override
    protected AndroidManifest getAppManifest(Config config) {
        final String packageName = "com.android.tv.tuner";

        // By adding any resources from libraries we need the AndroidManifest, we can access
        // them from within the parallel universe's resource loader.
        return new AndroidManifest(
                Fs.fileFromPath(config.manifest()),
                Fs.fileFromPath(config.resourceDir()),
                Fs.fileFromPath(config.assetDir()),
                packageName) {
            @Override
            public List<ResourcePath> getIncludedResourcePaths() {
                List<ResourcePath> paths = super.getIncludedResourcePaths();
                TvTunerRobolectricTestRunner.getIncludedResourcePaths(paths);
                return paths;
            }
        };
    }

    public static void getIncludedResourcePaths(List<ResourcePath> paths) {
        paths.add(new ResourcePath(null, Fs.fileFromPath("./packages/apps/TV/tuner/res"), null));
    }
}
