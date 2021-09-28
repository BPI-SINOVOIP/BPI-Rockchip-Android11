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
package android.app.notification.legacy.cts;

import android.content.ComponentName;
import android.service.notification.Condition;

public class LegacyConditionProviderService extends PollableConditionProviderService {
    private static PollableConditionProviderService sInstance = null;

    public static ComponentName getId() {
        return new ComponentName(LegacyConditionProviderService.class.getPackage().getName(),
                LegacyConditionProviderService.class.getName());
    }

    public static PollableConditionProviderService getInstance() {
        return sInstance;
    }

    @Override
    public void onConnected() {
        super.onConnected();
        sInstance = this;
    }

    @Override
    public void onDestroy() {
        sInstance = null;
        super.onDestroy();
    }

    public void toggleDND(boolean on) {
        notifyCondition(
                new Condition(conditionId, "", on ? Condition.STATE_TRUE : Condition.STATE_FALSE));
    }
}
