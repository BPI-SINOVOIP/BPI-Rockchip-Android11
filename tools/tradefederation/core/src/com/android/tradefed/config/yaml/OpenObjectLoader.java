/*
 * Copyright (C) 2020 The Android Open Source Project
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
package com.android.tradefed.config.yaml;

import com.android.tradefed.build.DependenciesResolver;
import com.android.tradefed.config.Configuration;
import com.android.tradefed.config.OptionSetter;
import com.android.tradefed.log.FileLogger;
import com.android.tradefed.result.suite.SuiteResultReporter;

/** Loader for the default objects available in AOSP. */
public class OpenObjectLoader implements IDefaultObjectLoader {

    @Override
    public void addDefaultObjects(LoaderConfiguration loadConfiguration) {
        // Only add the objects below if it's created as a stand alone configuration, in suite as
        // a module, it will be resolving object from the top level suite.
        if (loadConfiguration.createdAsModule()) {
            return;
        }
        // Logger
        loadConfiguration
                .getConfigDef()
                .addConfigObjectDef(Configuration.LOGGER_TYPE_NAME, FileLogger.class.getName());
        // Result Reporters
        loadConfiguration
                .getConfigDef()
                .addConfigObjectDef(
                        Configuration.RESULT_REPORTER_TYPE_NAME,
                        SuiteResultReporter.class.getName());
        // Build
        int classCount =
                loadConfiguration
                        .getConfigDef()
                        .addConfigObjectDef(
                                Configuration.BUILD_PROVIDER_TYPE_NAME,
                                DependenciesResolver.class.getName());
        // Set all the dependencies on the provider
        for (String depencency : loadConfiguration.getDependencies()) {
            String optionName =
                    String.format(
                            "%s%c%d%c%s",
                            DependenciesResolver.class.getName(),
                            OptionSetter.NAMESPACE_SEPARATOR,
                            classCount,
                            OptionSetter.NAMESPACE_SEPARATOR,
                            "dependency");
            loadConfiguration
                    .getConfigDef()
                    .addOptionDef(
                            optionName,
                            null,
                            depencency,
                            loadConfiguration.getConfigDef().getName(),
                            Configuration.BUILD_PROVIDER_TYPE_NAME);
        }
    }
}
