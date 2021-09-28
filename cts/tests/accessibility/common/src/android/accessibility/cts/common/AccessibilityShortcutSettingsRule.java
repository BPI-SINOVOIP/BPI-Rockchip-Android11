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

package android.accessibility.cts.common;

import static android.accessibility.cts.common.InstrumentedAccessibilityService.TIMEOUT_SERVICE_ENABLE;
import static android.accessibility.cts.common.ServiceControlUtils.waitForConditionWithServiceStateChange;
import static android.app.UiAutomation.FLAG_DONT_SUPPRESS_ACCESSIBILITY_SERVICES;
import static android.provider.Settings.Secure.ACCESSIBILITY_SHORTCUT_TARGET_SERVICE;

import static com.android.compatibility.common.util.SystemUtil.runWithShellPermissionIdentity;

import android.app.Instrumentation;
import android.app.Service;
import android.app.UiAutomation;
import android.content.ContentResolver;
import android.provider.Settings;
import android.text.TextUtils;
import android.view.accessibility.AccessibilityManager;

import androidx.test.InstrumentationRegistry;

import org.junit.rules.ExternalResource;

import java.util.List;

/**
 * JUnit rule used to restore Accessibility Shortcut Settings after the test. It also provides
 * utils to update shortcut targets to Accessibility Shortcut Settings.
 */
public final class AccessibilityShortcutSettingsRule extends ExternalResource {
    public static final int ACCESSIBILITY_BUTTON = 0;
    public static final int ACCESSIBILITY_SHORTCUT_KEY = 1;

    private static final String ACCESSIBILITY_BUTTON_TARGETS =
            "accessibility_button_targets";

    private static final char DELIMITER = ':';

    private final Instrumentation mInstrumentation;
    private final ContentResolver mContentResolver;
    private final AccessibilityManager mAccessibilityManager;

    // These are the shortcut settings before the tests. Roll back them after the tests.
    private String mA11yShortcutTargetSetting;
    private String mA11yButtonTargetSetting;

    public AccessibilityShortcutSettingsRule() {
        mInstrumentation = InstrumentationRegistry.getInstrumentation();
        mContentResolver = mInstrumentation.getContext().getContentResolver();
        mAccessibilityManager = (AccessibilityManager) mInstrumentation.getContext()
                .getSystemService(Service.ACCESSIBILITY_SERVICE);
    }

    @Override
    protected void before() {
        // Read current settings
        mA11yShortcutTargetSetting = Settings.Secure.getString(mContentResolver,
                ACCESSIBILITY_SHORTCUT_TARGET_SERVICE);
        mA11yButtonTargetSetting = Settings.Secure.getString(mContentResolver,
                ACCESSIBILITY_BUTTON_TARGETS);
    }

    @Override
    protected void after() {
        // Rollback all settings
        final UiAutomation uiAutomation = mInstrumentation.getUiAutomation(
                FLAG_DONT_SUPPRESS_ACCESSIBILITY_SERVICES);
        updateShortcutSetting(uiAutomation, ACCESSIBILITY_SHORTCUT_KEY, mA11yShortcutTargetSetting);
        updateShortcutSetting(uiAutomation, ACCESSIBILITY_BUTTON, mA11yButtonTargetSetting);
    }

    /**
     * Updates shortcut targets to accessibility shortcut.
     *
     * @param uiAutomation The UiAutomation.
     * @param newUseShortcutList The component names which use the shortcut.
     * @return true if the new targets updated.
     */
    public boolean configureAccessibilityShortcut(UiAutomation uiAutomation,
            String ... newUseShortcutList) {
        final String useShortcutList = getComponentIdString(newUseShortcutList);
        return updateShortcutSetting(uiAutomation, ACCESSIBILITY_SHORTCUT_KEY, useShortcutList);
    }

    /**
     * Updates shortcut targets to accessibility button.
     *
     * @param uiAutomation The UiAutomation.
     * @param newUseShortcutList The component names which use the shortcut.
     * @return true if the new targets updated.
     */
    public boolean configureAccessibilityButton(UiAutomation uiAutomation,
            String ... newUseShortcutList) {
        final String useShortcutList = getComponentIdString(newUseShortcutList);
        return updateShortcutSetting(uiAutomation, ACCESSIBILITY_BUTTON, useShortcutList);
    }

    /**
     * Waits for the ACCESSIBILITY_SHORTCUT_KEY state changed, and gets current shortcut list which
     * is the same with expected one.
     *
     * @param uiAutomation The UiAutomation.
     * @param expectedList The expected shortcut targets returned from
     *        {@link AccessibilityManager#getAccessibilityShortcutTargets(int)}.
     */
    public void waitForAccessibilityShortcutStateChange(UiAutomation uiAutomation,
            List<String> expectedList) {
        waitForShortcutStateChange(uiAutomation, ACCESSIBILITY_SHORTCUT_KEY, expectedList);
    }

    /**
     * Waits for the ACCESSIBILITY_BUTTON state changed, and gets current shortcut list which
     * is the same with expected one.
     *
     * @param uiAutomation The UiAutomation.
     * @param expectedList The expected shortcut targets returned from
     *        {@link AccessibilityManager#getAccessibilityShortcutTargets(int)}.
     */
    public void waitForAccessibilityButtonStateChange(UiAutomation uiAutomation,
            List<String> expectedList) {
        waitForShortcutStateChange(uiAutomation, ACCESSIBILITY_BUTTON, expectedList);
    }

    /**
     * Returns a colon-separated component name string by given string array.
     *
     * @param componentIds The array of component names.
     * @return A colon-separated component name string.
     */
    private String getComponentIdString(String ... componentIds) {
        final StringBuilder stringBuilder = new StringBuilder();
        for (String componentId : componentIds) {
            if (TextUtils.isEmpty(componentId)) {
                continue;
            }
            if (stringBuilder.length() != 0) {
                stringBuilder.append(DELIMITER);
            }
            stringBuilder.append(componentId);
        }
        return stringBuilder.toString();
    }

    /**
     * Updates the setting key of the accessibility shortcut.
     *
     * @param uiAutomation The uiautomation instance.
     * @param shortcutType The shortcut type.
     * @param newUseShortcutList The value of ACCESSIBILITY_SHORTCUT_TARGET_SERVICE
     * @return True if setting is updated.
     */
    private boolean updateShortcutSetting(UiAutomation uiAutomation, int shortcutType,
            String newUseShortcutList) {
        final String settingName = shortcutTypeToSettingName(shortcutType);
        if (TextUtils.isEmpty(settingName)) {
            return false;
        }
        final ShellCommandBuilder command = ShellCommandBuilder.create(uiAutomation);
        final String useShortcutList = Settings.Secure.getString(mContentResolver, settingName);
        boolean changes = false;
        if (!TextUtils.equals(useShortcutList, newUseShortcutList)) {
            if (TextUtils.isEmpty(newUseShortcutList)) {
                command.deleteSecureSetting(settingName);
            } else {
                command.putSecureSetting(settingName, newUseShortcutList);
            }
            changes = true;
        }
        if (changes) {
            command.run();
        }
        return changes;
    }

    private String shortcutTypeToSettingName(int shortcutType) {
        switch (shortcutType) {
            case ACCESSIBILITY_BUTTON: return ACCESSIBILITY_BUTTON_TARGETS;
            case ACCESSIBILITY_SHORTCUT_KEY: return ACCESSIBILITY_SHORTCUT_TARGET_SERVICE;
            default: return "";
        }
    }

    /**
     * Waits for the shortcut state changed, and gets current shortcut list is the same with
     * expected one.
     *
     * @param uiAutomation The UiAutomation.
     * @param shortcutType The shortcut type.
     * @param expectedList The expected shortcut targets returned from
     *        {@link AccessibilityManager#getAccessibilityShortcutTargets(int)}.
     */
    private void waitForShortcutStateChange(UiAutomation uiAutomation, int shortcutType,
            List<String> expectedList) {
        final StringBuilder message = new StringBuilder();
        message.append(shortcutTypeToString(shortcutType));
        message.append(" expect:").append(expectedList);
        runWithShellPermissionIdentity(uiAutomation, () ->
                waitForConditionWithServiceStateChange(mInstrumentation.getContext(), () -> {
                    final List<String> currentShortcuts =
                            mAccessibilityManager.getAccessibilityShortcutTargets(shortcutType);
                    if (currentShortcuts.size() != expectedList.size()) {
                        return false;
                    }
                    for (String expect : expectedList) {
                        if (!currentShortcuts.contains(expect)) {
                            return false;
                        }
                    }
                    return true;
                }, TIMEOUT_SERVICE_ENABLE, message.toString()));
    }

    private String shortcutTypeToString(int shortcutType) {
        switch (shortcutType) {
            case ACCESSIBILITY_BUTTON: return "ACCESSIBILITY_BUTTON";
            case ACCESSIBILITY_SHORTCUT_KEY: return "ACCESSIBILITY_SHORTCUT_KEY";
            default: return "";
        }
    }
}