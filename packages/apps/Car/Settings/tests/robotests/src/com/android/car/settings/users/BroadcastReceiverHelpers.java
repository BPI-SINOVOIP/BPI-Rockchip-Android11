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

package com.android.car.settings.users;

import static junit.framework.Assert.fail;

import static org.robolectric.Shadows.shadowOf;

import android.content.BroadcastReceiver;
import android.content.IntentFilter;

import org.robolectric.RuntimeEnvironment;
import org.robolectric.shadows.ShadowApplication;

import java.util.List;

public class BroadcastReceiverHelpers {

    private BroadcastReceiverHelpers() { }

    public static BroadcastReceiver getRegisteredReceiverWithActions(List<String> actions) {
        ShadowApplication.Wrapper wrapper = getRegisteredReceiverWrapperWithActions(actions);
        return wrapper == null ? null : wrapper.broadcastReceiver;
    }

    private static ShadowApplication.Wrapper getRegisteredReceiverWrapperWithActions(
            List<String> actions) {
        List<ShadowApplication.Wrapper> registeredReceivers =
                shadowOf(RuntimeEnvironment.application).getRegisteredReceivers();
        if (registeredReceivers.isEmpty()) {
            return null;
        }
        ShadowApplication.Wrapper returnWrapper = null;
        for (ShadowApplication.Wrapper wrapper : registeredReceivers) {
            if (matchesActions(wrapper.getIntentFilter(), actions)) {
                if (returnWrapper == null) {
                    returnWrapper = wrapper;
                } else {
                    fail("There are multiple receivers registered with this IntentFilter. "
                            + "This util method cannot handle multiple receivers.");
                }
            }
        }
        return returnWrapper;
    }

    private static boolean matchesActions(IntentFilter filter, List<String> actions) {
        if (filter.countActions() != actions.size()) {
            return false;
        }

        for (String action : actions) {
            if (!filter.matchAction(action)) {
                return false;
            }
        }

        return true;
    }
}
