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

import com.android.tradefed.config.ConfigurationDef;

import java.util.LinkedHashSet;
import java.util.Set;

/**
 * Interface for loading the default objects that should be part of our YAML configuration. This
 * allows to customize the YAML configuration with any objects we need based on the context.
 */
public interface IDefaultObjectLoader {

    /** Allows to add any default objects as necessary. */
    public void addDefaultObjects(LoaderConfiguration loaderConfiguration);

    /** The loading configuration object to pass information to the loader. */
    public class LoaderConfiguration {

        private ConfigurationDef mConfigDef;
        private boolean mCreatedAsModule = false;
        private Set<String> mDependencies = new LinkedHashSet<>();

        public LoaderConfiguration setConfigurationDef(ConfigurationDef configDef) {
            mConfigDef = configDef;
            return this;
        }

        public LoaderConfiguration addDependencies(Set<String> dependencies) {
            mDependencies.addAll(dependencies);
            return this;
        }

        public LoaderConfiguration addDependency(String dependency) {
            mDependencies.add(dependency);
            return this;
        }

        public LoaderConfiguration setCreatedAsModule(boolean createdAsModule) {
            mCreatedAsModule = createdAsModule;
            return this;
        }

        public ConfigurationDef getConfigDef() {
            return mConfigDef;
        }

        public Set<String> getDependencies() {
            return mDependencies;
        }

        public boolean createdAsModule() {
            return mCreatedAsModule;
        }
    }
}
