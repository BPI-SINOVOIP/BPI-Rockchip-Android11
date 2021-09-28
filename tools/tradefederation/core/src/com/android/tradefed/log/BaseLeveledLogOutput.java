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
package com.android.tradefed.log;

import com.android.ddmlib.Log.LogLevel;
import com.android.tradefed.config.IConfiguration;
import com.android.tradefed.config.Option;
import com.android.tradefed.log.LogUtil.CLog;

import com.google.common.collect.ImmutableMap;

import java.util.Collection;
import java.util.HashMap;
import java.util.HashSet;
import java.util.Map;
import java.util.Set;

/**
 * A base implementation for {@link ILeveledLogOutput} that allows to filtering some tags based on
 * their name or components.
 */
public abstract class BaseLeveledLogOutput implements ILeveledLogOutput {

    @Option(
        name = "include-log-tag",
        description = "List of tags that should be included in the display"
    )
    private Set<String> mIncludeTag = new HashSet<>();

    @Option(
        name = "class-verbosity",
        description = "Sets the verbosity of a particular class tag. For example: StubTest INFO."
    )
    private Map<String, LogLevel> mClassVerbosity = new HashMap<>();

    @Option(
        name = "component-verbosity",
        description =
                "Sets the verbosity of a group of Tradefed components."
                        + "For example: target_preparer WARN."
    )
    private Map<String, LogLevel> mComponentVerbosity = new HashMap<>();

    @Option(
            name = "force-verbosity-map",
            description = "Enforce a pre-set verbosity of some component to avoid extreme logging.")
    private boolean mEnableForcedVerbosity = true;

    // Add components we have less control over (ddmlib for example) to ensure they don't flood
    // us. This will still write to the log.
    private Map<String, LogLevel> mForcedVerbosity = ImmutableMap.of("ddms", LogLevel.WARN);

    private Map<String, LogLevel> mVerbosityMap = new HashMap<>();

    /** Initialize the components filters based on the invocation {@link IConfiguration}. */
    public final void initFilters(IConfiguration config) {
        initComponentVerbosity(mComponentVerbosity, mVerbosityMap, config);
        mVerbosityMap.putAll(mClassVerbosity);
    }

    /**
     * Whether or not a particular statement should be displayed base on its tag.
     *
     * @param forceStdout Whether or not to force the output to stdout.
     * @param invocationLogLevel The current logLevel for the information.
     * @param messageLogLevel The message evaluated log level.
     * @param tag The logging tag of the message considered.
     * @return True if it should be displayed, false otherwise.
     */
    public final boolean shouldDisplay(
            boolean forceStdout,
            LogLevel invocationLogLevel,
            LogLevel messageLogLevel,
            String tag) {
        if (forceStdout) {
            return true;
        }
        // If we have a specific verbosity use it. Otherwise use the invocation verbosity.
        if (mVerbosityMap.get(tag) != null) {
            if (messageLogLevel.getPriority() >= mVerbosityMap.get(tag).getPriority()) {
                return true;
            }
        } else if (messageLogLevel.getPriority() >= invocationLogLevel.getPriority()) {
            return true;
        }
        return false;
    }

    private void initComponentVerbosity(
            Map<String, LogLevel> components,
            Map<String, LogLevel> receiver,
            IConfiguration config) {
        for (String component : components.keySet()) {
            Collection<Object> objs = config.getAllConfigurationObjectsOfType(component);
            if (objs == null) {
                continue;
            }
            for (Object o : objs) {
                String objTag = CLog.parseClassName(o.getClass().getName());
                receiver.put(objTag, components.get(component));
            }
        }
        if (shouldForceVerbosity()) {
            mVerbosityMap.putAll(mForcedVerbosity);
        }
    }

    /** {@inheritDoc} */
    @Override
    public abstract ILeveledLogOutput clone();

    /** Whether or not to enforce the verbosity map. */
    public boolean shouldForceVerbosity() {
        return mEnableForcedVerbosity;
    }

    /** Returns the map of the forced verbosity. */
    public Map<String, LogLevel> getForcedVerbosityMap() {
        return mForcedVerbosity;
    }
}
