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

import com.android.atest.AtestUtils;
import com.android.atest.Constants;
import com.intellij.execution.configurations.ConfigurationFactory;
import com.intellij.execution.configurations.ConfigurationType;
import com.intellij.execution.configurations.RunConfiguration;
import com.intellij.openapi.project.Project;
import org.jetbrains.annotations.NotNull;

/** Factory for running Atest configuration instances. */
public class AtestConfigurationFactory extends ConfigurationFactory {

    private static final String FACTORY_NAME = "Atest config";

    protected AtestConfigurationFactory(ConfigurationType type) {
        super(type);
    }

    /**
     * Creates a new template run configuration within the context of the specified project.
     *
     * @param project the project in which the run configuration will be used.
     * @return the run configuration instance.
     */
    @NotNull
    @Override
    public RunConfiguration createTemplateConfiguration(@NotNull Project project) {
        return new AtestRunConfiguration(project, this, Constants.ATEST_NAME);
    }

    /**
     * Checks if the project is applicable for executing Atest.
     *
     * <p>It returns {@code false} to hide the configuration from 'New' popup in 'Edit
     * Configurations' dialog. It will be still possible to create this configuration by clicking on
     * 'n more items' in the 'New' popup.
     *
     * @param project the project in which the run configuration will be used.
     * @return {@code true} if it makes sense to create configurations of this type in {@code
     *     project}
     */
    @Override
    public boolean isApplicable(@NotNull Project project) {
        return AtestUtils.getAndroidRoot(project.getBasePath()) != null;
    }

    /**
     * Gets the name of the run configuration.
     *
     * @return the name of the run configuration.
     */
    @NotNull
    @Override
    public String getName() {
        return FACTORY_NAME;
    }
}
