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

package android.server.wm.second;

import android.content.ComponentName;
import android.server.wm.component.ComponentsBase;

public class Components extends ComponentsBase {

    public static final ComponentName EMBEDDING_ACTIVITY = component("EmbeddingActivity");

    public static class EmbeddingActivity {
        public static final String ACTION_EMBEDDING_TEST_ACTIVITY_START =
                "broadcast_test_activity_start";
        public static final String EXTRA_EMBEDDING_COMPONENT_NAME = "component_name";
        public static final String EXTRA_EMBEDDING_TARGET_DISPLAY = "target_display";
    }

    public static final ComponentName SECOND_ACTIVITY = component("SecondActivity");

    public static class SecondActivity {
        public static final String EXTRA_DISPLAY_ACCESS_CHECK = "display_access_check";
    }

    public static final ComponentName SECOND_NO_EMBEDDING_ACTIVITY =
            component("SecondActivityNoEmbedding");

    public static final ComponentName TEST_ACTIVITY_WITH_SAME_AFFINITY_DIFFERENT_UID =
            component("TestActivityWithSameAffinityDifferentUid");

    public static final ComponentName SECOND_LAUNCH_BROADCAST_RECEIVER =
            component("LaunchBroadcastReceiver");
    /** See AndroidManifest.xml. */
    public static final String SECOND_LAUNCH_BROADCAST_ACTION =
            getPackageName() + ".LAUNCH_BROADCAST_ACTION";

    private static ComponentName component(String className) {
        return component(Components.class, className);
    }

    private static String getPackageName() {
        return getPackageName(Components.class);
    }
}
