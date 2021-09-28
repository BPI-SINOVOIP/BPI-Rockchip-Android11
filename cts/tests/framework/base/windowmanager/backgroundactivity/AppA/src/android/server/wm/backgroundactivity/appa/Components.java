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

package android.server.wm.backgroundactivity.appa;

import android.content.ComponentName;
import android.server.wm.component.ComponentsBase;

public class Components extends ComponentsBase {

    public static final ComponentName APP_A_BACKGROUND_ACTIVITY =
            component(Components.class, "BackgroundActivity");
    public static final ComponentName APP_A_SECOND_BACKGROUND_ACTIVITY =
            component(Components.class, "SecondBackgroundActivity");
    public static final ComponentName APP_A_FOREGROUND_ACTIVITY =
            component(Components.class, "ForegroundActivity");
    public static final ComponentName APP_A_SEND_PENDING_INTENT_RECEIVER =
            component(Components.class, "SendPendingIntentReceiver");
    public static final ComponentName APP_A_START_ACTIVITY_RECEIVER =
            component(Components.class, "StartBackgroundActivityReceiver");
    public static final ComponentName APP_A_SIMPLE_ADMIN_RECEIVER =
            component(Components.class, "SimpleAdminReceiver");

    /** Extra key constants for {@link #APP_A_FOREGROUND_ACTIVITY}. */
    public static class ForegroundActivity {
        public static final String LAUNCH_BACKGROUND_ACTIVITY_EXTRA =
                "LAUNCH_BACKGROUND_ACTIVITY_EXTRA";
        public static final String LAUNCH_SECOND_BACKGROUND_ACTIVITY_EXTRA =
                "LAUNCH_SECOND_BACKGROUND_ACTIVITY_EXTRA";
        public static final String RELAUNCH_FOREGROUND_ACTIVITY_EXTRA =
                "RELAUNCH_FOREGROUND_ACTIVITY_EXTRA";
        public static final String START_ACTIVITY_FROM_FG_ACTIVITY_DELAY_MS_EXTRA =
                "START_ACTIVITY_FROM_FG_ACTIVITY_DELAY_MS_EXTRA";
        public static final String START_ACTIVITY_FROM_FG_ACTIVITY_NEW_TASK_EXTRA =
                "START_ACTIVITY_FROM_FG_ACTIVITY_NEW_TASK_EXTRA";
        public static final String LAUNCH_INTENTS_EXTRA = "LAUNCH_INTENTS_EXTRA";

        public static final String ACTION_LAUNCH_BACKGROUND_ACTIVITIES =
                Components.class.getPackage().getName() + ".ACTION_LAUNCH_BACKGROUND_ACTIVITIES";
        public static final String ACTION_FINISH_ACTIVITY =
                Components.class.getPackage().getName() + ".ACTION_FINISH_ACTIVITY";
    }

    /** Extra key constants for {@link #APP_A_SEND_PENDING_INTENT_RECEIVER} */
    public static class SendPendingIntentReceiver {
        public static final String IS_BROADCAST_EXTRA = "IS_BROADCAST_EXTRA";
    }

    /** Extra key constants for {@link #APP_A_START_ACTIVITY_RECEIVER} */
    public static class StartBackgroundActivityReceiver {
        public static final String START_ACTIVITY_DELAY_MS_EXTRA =
                "START_ACTIVITY_FROM_FG_ACTIVITY_DELAY_MS_EXTRA";
    }

}
