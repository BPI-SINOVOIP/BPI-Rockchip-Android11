/**
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

package android.ext.services.notification;

import static android.ext.services.notification.AssistantSettings.DEFAULT_MAX_SUGGESTIONS;
import static android.provider.DeviceConfig.setProperty;

import static com.android.compatibility.common.util.SystemUtil.runWithShellPermissionIdentity;

import static junit.framework.Assert.assertFalse;
import static junit.framework.Assert.assertTrue;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;

import android.provider.DeviceConfig;
import android.support.test.uiautomator.UiDevice;
import android.testing.TestableContext;

import androidx.test.InstrumentationRegistry;
import androidx.test.runner.AndroidJUnit4;

import com.android.internal.config.sysui.SystemUiDeviceConfigFlags;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import java.io.IOException;

@RunWith(AndroidJUnit4.class)
public class AssistantSettingsTest {
    private static final String CLEAR_DEVICE_CONFIG_KEY_CMD =
            "device_config delete " + DeviceConfig.NAMESPACE_SYSTEMUI;
    private static final String WRITE_DEVICE_CONFIG_PERMISSION =
            "android.permission.WRITE_DEVICE_CONFIG";

    private static final String READ_DEVICE_CONFIG_PERMISSION =
            "android.permission.READ_DEVICE_CONFIG";

    @Rule
    public final TestableContext mContext =
            new TestableContext(InstrumentationRegistry.getContext(), null);

    private AssistantSettings mAssistantSettings;

    @Before
    public void setUp() {
        InstrumentationRegistry.getInstrumentation().getUiAutomation()
                .adoptShellPermissionIdentity(
                        WRITE_DEVICE_CONFIG_PERMISSION,
                        READ_DEVICE_CONFIG_PERMISSION);
        mAssistantSettings = new AssistantSettings();
    }

    @After
    public void tearDown() throws IOException {
        clearDeviceConfig();
        InstrumentationRegistry
                .getInstrumentation()
                .getUiAutomation()
                .dropShellPermissionIdentity();
    }

    @Test
    public void testWrongNamespace() {
        runWithShellPermissionIdentity(() -> setProperty(
                "wrong",
                SystemUiDeviceConfigFlags.NAS_GENERATE_REPLIES,
                "false",
                false /* makeDefault */));
        mAssistantSettings.onDeviceConfigPropertiesChanged("wrong");

        assertTrue(mAssistantSettings.mGenerateReplies);
        assertTrue(mAssistantSettings.shouldGenerateReplies());
    }

    @Test
    public void testGenerateRepliesDisabled() {
        setProperty(
                DeviceConfig.NAMESPACE_SYSTEMUI,
                SystemUiDeviceConfigFlags.NAS_GENERATE_REPLIES,
                "false",
                false /* makeDefault */);
        mAssistantSettings.onDeviceConfigPropertiesChanged(DeviceConfig.NAMESPACE_SYSTEMUI);

        assertFalse(mAssistantSettings.mGenerateReplies);
        assertFalse(mAssistantSettings.shouldGenerateReplies());
    }

    @Test
    public void testGenerateRepliesEnabled() {
        setProperty(
                DeviceConfig.NAMESPACE_SYSTEMUI,
                SystemUiDeviceConfigFlags.NAS_GENERATE_REPLIES,
                "true",
                false /* makeDefault */);
        mAssistantSettings.onDeviceConfigPropertiesChanged(DeviceConfig.NAMESPACE_SYSTEMUI);

        assertTrue(mAssistantSettings.mGenerateReplies);
        assertTrue(mAssistantSettings.shouldGenerateReplies());
    }

    @Test
    public void testGenerateRepliesNullFlag() {
        setProperty(
                DeviceConfig.NAMESPACE_SYSTEMUI,
                SystemUiDeviceConfigFlags.NAS_GENERATE_REPLIES,
                "false",
                false /* makeDefault */);
        mAssistantSettings.onDeviceConfigPropertiesChanged(DeviceConfig.NAMESPACE_SYSTEMUI);

        assertFalse(mAssistantSettings.mGenerateReplies);

        setProperty(
                DeviceConfig.NAMESPACE_SYSTEMUI,
                SystemUiDeviceConfigFlags.NAS_GENERATE_REPLIES,
                null,
                false /* makeDefault */);
        mAssistantSettings.onDeviceConfigPropertiesChanged(DeviceConfig.NAMESPACE_SYSTEMUI);

        // Go back to the default value.
        assertTrue(mAssistantSettings.mGenerateReplies);
    }

    @Test
    public void testGenerateActionsDisabled() {
        setProperty(
                DeviceConfig.NAMESPACE_SYSTEMUI,
                SystemUiDeviceConfigFlags.NAS_GENERATE_ACTIONS,
                "false",
                false /* makeDefault */);
        mAssistantSettings.onDeviceConfigPropertiesChanged(DeviceConfig.NAMESPACE_SYSTEMUI);

        assertFalse(mAssistantSettings.mGenerateActions);
        assertFalse(mAssistantSettings.shouldGenerateActions());
    }

    @Test
    public void testGenerateActionsEnabled() {
        setProperty(
                DeviceConfig.NAMESPACE_SYSTEMUI,
                SystemUiDeviceConfigFlags.NAS_GENERATE_ACTIONS,
                "true",
                false /* makeDefault */);
        mAssistantSettings.onDeviceConfigPropertiesChanged(DeviceConfig.NAMESPACE_SYSTEMUI);

        assertTrue(mAssistantSettings.mGenerateActions);
        assertTrue(mAssistantSettings.shouldGenerateActions());
    }

    @Test
    public void testGenerateActionsNullFlag() {
        setProperty(
                DeviceConfig.NAMESPACE_SYSTEMUI,
                SystemUiDeviceConfigFlags.NAS_GENERATE_ACTIONS,
                "false",
                false /* makeDefault */);
        mAssistantSettings.onDeviceConfigPropertiesChanged(DeviceConfig.NAMESPACE_SYSTEMUI);

        assertFalse(mAssistantSettings.mGenerateActions);

        setProperty(
                DeviceConfig.NAMESPACE_SYSTEMUI,
                SystemUiDeviceConfigFlags.NAS_GENERATE_ACTIONS,
                null,
                false /* makeDefault */);
        mAssistantSettings.onDeviceConfigPropertiesChanged(DeviceConfig.NAMESPACE_SYSTEMUI);

        // Go back to the default value.
        assertTrue(mAssistantSettings.mGenerateActions);
    }

    @Test
    public void testMaxMessagesToExtract() {
        setProperty(
                DeviceConfig.NAMESPACE_SYSTEMUI,
                SystemUiDeviceConfigFlags.NAS_MAX_MESSAGES_TO_EXTRACT,
                "10",
                false /* makeDefault */);
        mAssistantSettings.onDeviceConfigPropertiesChanged(DeviceConfig.NAMESPACE_SYSTEMUI);

        assertEquals(10, mAssistantSettings.mMaxMessagesToExtract);
        assertEquals(10, mAssistantSettings.getMaxMessagesToExtract());
    }

    @Test
    public void testMaxSuggestions() {
        setProperty(
                DeviceConfig.NAMESPACE_SYSTEMUI,
                SystemUiDeviceConfigFlags.NAS_MAX_SUGGESTIONS,
                "5",
                false /* makeDefault */);
        mAssistantSettings.onDeviceConfigPropertiesChanged(DeviceConfig.NAMESPACE_SYSTEMUI);

        assertEquals(5, mAssistantSettings.mMaxSuggestions);
        assertEquals(5, mAssistantSettings.getMaxSuggestions());
    }

    @Test
    public void testMaxSuggestionsEmpty() {
        mAssistantSettings.onDeviceConfigPropertiesChanged(DeviceConfig.NAMESPACE_SYSTEMUI);

        assertEquals(DEFAULT_MAX_SUGGESTIONS, mAssistantSettings.mMaxSuggestions);
    }

    @Test
    public void testCreation() {
        AssistantSettings.Factory factory = AssistantSettings.FACTORY;
        AssistantSettings as = factory.createAndRegister();

        assertNotNull(as);

        // unregister listener to avoid onDeviceConfigPropertiesChanged is called after test done.
        as.unregisterDeviceConfigs();
    }

    private static void clearDeviceConfig() throws IOException {
        UiDevice uiDevice = UiDevice.getInstance(InstrumentationRegistry.getInstrumentation());
        uiDevice.executeShellCommand(
                CLEAR_DEVICE_CONFIG_KEY_CMD + " " + SystemUiDeviceConfigFlags.NAS_GENERATE_ACTIONS);
        uiDevice.executeShellCommand(
                CLEAR_DEVICE_CONFIG_KEY_CMD + " " + SystemUiDeviceConfigFlags.NAS_GENERATE_REPLIES);
        uiDevice.executeShellCommand(
                CLEAR_DEVICE_CONFIG_KEY_CMD + " "
                + SystemUiDeviceConfigFlags.NAS_MAX_MESSAGES_TO_EXTRACT);
        uiDevice.executeShellCommand(
                CLEAR_DEVICE_CONFIG_KEY_CMD + " " + SystemUiDeviceConfigFlags.NAS_MAX_SUGGESTIONS);
    }

}
