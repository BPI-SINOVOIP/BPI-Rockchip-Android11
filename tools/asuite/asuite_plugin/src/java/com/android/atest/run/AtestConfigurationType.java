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
package com.android.atest.run;

import com.android.atest.Constants;
import com.intellij.execution.configurations.ConfigurationFactory;
import com.intellij.execution.configurations.ConfigurationType;
import com.intellij.openapi.util.IconLoader;
import javax.swing.Icon;
import org.jetbrains.annotations.NotNull;

/**
 * Atest run configuration type. It is the starting point for implementing run configuration type.
 */
public class AtestConfigurationType implements ConfigurationType {

    protected static final String ID = "atestRunConfiguration";

    /**
     * Returns the display name of the configuration type. This is used to represent the
     * configuration type in the run configurations tree, and also as the name of the action used to
     * create the configuration.
     *
     * @return Atest display name of the configuration type.
     */
    @NotNull
    @Override
    public String getDisplayName() {
        return Constants.ATEST_NAME;
    }

    /**
     * Gets the description of the configuration type.
     *
     * @return the description of Atest configuration type.
     */
    @Override
    public String getConfigurationTypeDescription() {
        return Constants.ATEST_NAME;
    }

    /**
     * Gets the 16x16 icon used to represent the configuration type.
     *
     * @return the icon.
     */
    @Override
    public Icon getIcon() {
        return IconLoader.getIcon(Constants.ATEST_ICON_PATH);
    }

    /**
     * The ID of the configuration type. Should be camel-cased without dashes, underscores, spaces
     * and quotation marks. The ID is used to store run configuration settings in a project or
     * workspace file and must not change between plugin versions.
     *
     * @return Atest configuration ID.
     */
    @NotNull
    @Override
    public String getId() {
        return ID;
    }

    /**
     * Returns the configuration factories used by this configuration type. Normally each
     * configuration type provides just a single factory.
     *
     * @return the run configuration factories.
     */
    @Override
    public ConfigurationFactory[] getConfigurationFactories() {
        return new ConfigurationFactory[] {new AtestConfigurationFactory(this)};
    }
}
