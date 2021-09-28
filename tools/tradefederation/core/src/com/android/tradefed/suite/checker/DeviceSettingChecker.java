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
package com.android.tradefed.suite.checker;

import com.android.tradefed.config.Option;
import com.android.tradefed.device.DeviceNotAvailableException;
import com.android.tradefed.device.ITestDevice;
import com.android.tradefed.log.LogUtil.CLog;
import com.android.tradefed.suite.checker.StatusCheckerResult.CheckStatus;
import com.android.tradefed.util.ArrayUtil;

import com.google.common.collect.MapDifference;
import com.google.common.collect.MapDifference.ValueDifference;
import com.google.common.collect.Maps;

import java.util.HashMap;
import java.util.HashSet;
import java.util.LinkedList;
import java.util.List;
import java.util.Map;
import java.util.Set;

/** Check if device settings have changed during a module run. */
public class DeviceSettingChecker implements ISystemStatusChecker {

    private static final String NAMESPACE_SYSTEM = "system";
    private static final String NAMESPACE_GLOBAL = "global";
    private static final String NAMESPACE_SECURE = "secure";
    private static final Set<String> NAMESPACES = new HashSet<>();

    static {
        NAMESPACES.add(NAMESPACE_SYSTEM);
        NAMESPACES.add(NAMESPACE_GLOBAL);
        NAMESPACES.add(NAMESPACE_SECURE);
    }

    @Option(
        name = "ignore-system-setting",
        description = "Specify a system setting that's exempted from the checker; may be repeated"
    )
    private Set<String> mIgnoredSystemSettings = new HashSet<>();

    @Option(
        name = "ignore-secure-setting",
        description = "Specify a secure setting that's exempted from the checker; may be repeated"
    )
    private Set<String> mIgnoredSecureSettings = new HashSet<>();

    @Option(
        name = "ignore-global-setting",
        description = "Specify a global setting that's exempted from the checker; may be repeated"
    )
    private Set<String> mIgnoredGlobalSettings = new HashSet<>();

    // The key of map is namespace, the value is settings' key-value map.
    private Map<String, Map<String, String>> mSettingValPair = new HashMap<>();

    /** {@inheritDoc} */
    @Override
    public StatusCheckerResult preExecutionCheck(ITestDevice device)
            throws DeviceNotAvailableException {
        mSettingValPair = new HashMap<>();
        CLog.d("Begin preExecutionCheck for checking device setting");
        Set<String> failedNamespaces = new HashSet<>();
        for (String namespace : NAMESPACES) {
            Map<String, String> newSettings = getSettingsHelper(device, namespace);
            if (newSettings == null || newSettings.isEmpty()) {
                failedNamespaces.add(namespace);
            } else {
                mSettingValPair.put(namespace, newSettings);
            }
        }
        if (failedNamespaces.isEmpty()) {
            return new StatusCheckerResult(CheckStatus.SUCCESS);
        } else {
            StatusCheckerResult result = new StatusCheckerResult(CheckStatus.FAILED);
            String errorMessage =
                    String.format(
                            "Failed to get setting in pre-execution: %s\n",
                            String.join(",", failedNamespaces));
            CLog.w(errorMessage);
            result.setErrorMessage(errorMessage);
            return result;
        }
    }

    /** {@inheritDoc} */
    @Override
    public StatusCheckerResult postExecutionCheck(ITestDevice device)
            throws DeviceNotAvailableException {
        CLog.d("Begin postExecution for checking device setting");
        Map<String, List<String>> failureMap = new HashMap<>();
        for (String namespace : NAMESPACES) {
            List<String> failureInfo = new LinkedList<>();
            Map<String, String> newSettings = getSettingsHelper(device, namespace);
            Map<String, String> oldSettings = mSettingValPair.get(namespace);
            if (oldSettings == null) {
                continue;
            }
            MapDifference<String, String> diff = Maps.difference(oldSettings, newSettings);
            // Settings exist in both side but have different values.
            Map<String, ValueDifference<String>> entriesDiff = diff.entriesDiffering();
            for (Map.Entry<String, ValueDifference<String>> entry : entriesDiff.entrySet()) {
                String failure =
                        String.format(
                                "%s changes from %s to %s",
                                entry.getKey(),
                                entry.getValue().leftValue(),
                                entry.getValue().rightValue());
                failureInfo.add(failure);
            }
            // Settings only exist in the old one.
            Set<String> settingsOnlyInOld = diff.entriesOnlyOnLeft().keySet();
            if (!settingsOnlyInOld.isEmpty()) {
                failureInfo.add(
                        String.format(
                                "%s is not found after module run",
                                String.join(",", settingsOnlyInOld)));
            }
            // Settings only exist in the new one.
            Set<String> settingsOnlyInNew = diff.entriesOnlyOnRight().keySet();
            if (!settingsOnlyInNew.isEmpty()) {
                failureInfo.add(
                        String.format(
                                "%s is not found before module run",
                                String.join(",", settingsOnlyInNew)));
            }
            if (!failureInfo.isEmpty()) {
                failureMap.put(namespace, failureInfo);
            }
        }

        if (failureMap.isEmpty()) {
            return new StatusCheckerResult(CheckStatus.SUCCESS);
        } else {
            StatusCheckerResult result = new StatusCheckerResult(CheckStatus.FAILED);
            StringBuilder failureMessage = new StringBuilder();
            for (String namespace : failureMap.keySet()) {
                failureMessage.append(
                        String.format(
                                "Setting in namespace %s fails:\n %s\n",
                                namespace, ArrayUtil.join("\n", failureMap.get(namespace))));
            }
            CLog.w(failureMessage.toString());
            result.setErrorMessage(failureMessage.toString());
            return result;
        }
    }

    /**
     * Get all device settings given namespace, and remove ignored key from map.
     *
     * @param device The {@link ITestDevice}
     * @param namespace namespace of the setting.
     * @return a Map of the settings for a namespace
     * @throws DeviceNotAvailableException
     */
    private Map<String, String> getSettingsHelper(ITestDevice device, String namespace)
            throws DeviceNotAvailableException {
        Map<String, String> settingsPair = device.getAllSettings(namespace);
        Set<String> ignoredSettings = new HashSet<>();
        switch (namespace) {
            case NAMESPACE_GLOBAL:
                ignoredSettings = mIgnoredGlobalSettings;
                break;
            case NAMESPACE_SECURE:
                ignoredSettings = mIgnoredSecureSettings;
                break;
            case NAMESPACE_SYSTEM:
                ignoredSettings = mIgnoredSystemSettings;
                break;
        }
        for (String ignoredSetting : ignoredSettings) {
            settingsPair.remove(ignoredSetting);
        }
        return settingsPair;
    }
}
