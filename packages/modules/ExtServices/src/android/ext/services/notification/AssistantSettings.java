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

import android.os.Handler;
import android.os.Looper;
import android.provider.DeviceConfig;
import android.util.Log;

import androidx.annotation.VisibleForTesting;

import com.android.textclassifier.notification.SmartSuggestionsConfig;

/**
 * Observes the settings for {@link Assistant}.
 */
final class AssistantSettings implements SmartSuggestionsConfig {
    private static final String LOG_TAG = "AssistantSettings";
    public static Factory FACTORY = AssistantSettings::createAndRegister;
    private static final boolean DEFAULT_GENERATE_REPLIES = true;
    private static final boolean DEFAULT_GENERATE_ACTIONS = true;
    private static final int DEFAULT_MAX_MESSAGES_TO_EXTRACT = 5;
    @VisibleForTesting
    static final int DEFAULT_MAX_SUGGESTIONS = 3;


    // Copied from SystemUiDeviceConfigFlags.java
    /**
     * Whether the Notification Assistant should generate replies for notifications.
     */
    static final String NAS_GENERATE_REPLIES = "nas_generate_replies";

    /**
     * Whether the Notification Assistant should generate contextual actions for notifications.
     */
    static final String NAS_GENERATE_ACTIONS = "nas_generate_actions";

    /**
     * The maximum number of messages the Notification Assistant should extract from a
     * conversation when constructing responses for that conversation.
     */
    static final String NAS_MAX_MESSAGES_TO_EXTRACT = "nas_max_messages_to_extract";

    /**
     * The maximum number of suggestions the Notification Assistant should provide for a
     * messaging conversation.
     */
    static final String NAS_MAX_SUGGESTIONS = "nas_max_suggestions";

    private final Handler mHandler;

    // Actual configuration settings.
    boolean mGenerateReplies = DEFAULT_GENERATE_REPLIES;
    boolean mGenerateActions = DEFAULT_GENERATE_ACTIONS;
    int mMaxMessagesToExtract = DEFAULT_MAX_MESSAGES_TO_EXTRACT;
    int mMaxSuggestions = DEFAULT_MAX_SUGGESTIONS;

    @VisibleForTesting
    DeviceConfig.OnPropertiesChangedListener mDeviceConfigChangedListener;

    public AssistantSettings() {
        mHandler = new Handler(Looper.getMainLooper());
    }

    private static AssistantSettings createAndRegister() {
        AssistantSettings assistantSettings = new AssistantSettings();
        assistantSettings.registerDeviceConfigs();
        return assistantSettings;
    }

    private void registerDeviceConfigs() {
        mDeviceConfigChangedListener =
                properties -> onDeviceConfigPropertiesChanged(properties.getNamespace());
        DeviceConfig.addOnPropertiesChangedListener(
                DeviceConfig.NAMESPACE_SYSTEMUI,
                this::postToHandler,
                mDeviceConfigChangedListener);

        // Update the fields in this class from the current state of the device config.
        updateFromDeviceConfigFlags();
    }

    private void postToHandler(Runnable r) {
        this.mHandler.post(r);
    }

    @VisibleForTesting
    void unregisterDeviceConfigs() {
        if (mDeviceConfigChangedListener != null) {
            DeviceConfig.removeOnPropertiesChangedListener(mDeviceConfigChangedListener);
            mDeviceConfigChangedListener = null;
        }
    }

    @VisibleForTesting
    void onDeviceConfigPropertiesChanged(String namespace) {
        if (!DeviceConfig.NAMESPACE_SYSTEMUI.equals(namespace)) {
            Log.e(LOG_TAG, "Received update from DeviceConfig for unrelated namespace: "
                    + namespace);
            return;
        }

        updateFromDeviceConfigFlags();
    }

    private void updateFromDeviceConfigFlags() {
        mGenerateReplies = DeviceConfig.getBoolean(DeviceConfig.NAMESPACE_SYSTEMUI,
                NAS_GENERATE_REPLIES, DEFAULT_GENERATE_REPLIES);

        mGenerateActions = DeviceConfig.getBoolean(DeviceConfig.NAMESPACE_SYSTEMUI,
                NAS_GENERATE_ACTIONS, DEFAULT_GENERATE_ACTIONS);

        mMaxMessagesToExtract = DeviceConfig.getInt(DeviceConfig.NAMESPACE_SYSTEMUI,
                NAS_MAX_MESSAGES_TO_EXTRACT,
                DEFAULT_MAX_MESSAGES_TO_EXTRACT);

        mMaxSuggestions = DeviceConfig.getInt(DeviceConfig.NAMESPACE_SYSTEMUI,
                NAS_MAX_SUGGESTIONS, DEFAULT_MAX_SUGGESTIONS);

    }

    @Override
    public boolean shouldGenerateReplies() {
        return mGenerateReplies;
    }

    @Override
    public boolean shouldGenerateActions() {
        return mGenerateActions;
    }

    @Override
    public int getMaxSuggestions() {
        return mMaxSuggestions;
    }

    @Override
    public int getMaxMessagesToExtract() {
        return mMaxMessagesToExtract;
    }

    public interface Factory {
        AssistantSettings createAndRegister();
    }
}
