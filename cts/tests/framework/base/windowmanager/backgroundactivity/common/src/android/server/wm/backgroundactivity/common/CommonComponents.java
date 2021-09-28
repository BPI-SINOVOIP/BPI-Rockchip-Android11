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

package android.server.wm.backgroundactivity.common;

import androidx.annotation.IntDef;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * Constant-holding class common to AppA, AppB and tests.
 */
public class CommonComponents {

    public static final String EVENT_NOTIFIER_EXTRA = "EVENT_NOTIFIER_EXTRA";

    @IntDef({
            Event.APP_A_SEND_PENDING_INTENT_BROADCAST_RECEIVED,
            Event.APP_B_START_PENDING_INTENT_BROADCAST_RECEIVED,
            Event.APP_A_START_BACKGROUND_ACTIVITY_BROADCAST_RECEIVED,
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface Event {
        int APP_A_SEND_PENDING_INTENT_BROADCAST_RECEIVED = 0;
        int APP_B_START_PENDING_INTENT_BROADCAST_RECEIVED = 1;
        int APP_A_START_BACKGROUND_ACTIVITY_BROADCAST_RECEIVED = 2;
    }
}
