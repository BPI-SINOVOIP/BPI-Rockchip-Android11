/*
 * Copyright (C) 2016 The Android Open Source Project
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
 * limitations under the License
 */

package android.server.wm;

import static android.server.wm.app.Components.KeyguardDismissLoggerCallback.ENTRY_ON_DISMISS_CANCELLED;
import static android.server.wm.app.Components.KeyguardDismissLoggerCallback.ENTRY_ON_DISMISS_ERROR;
import static android.server.wm.app.Components.KeyguardDismissLoggerCallback.ENTRY_ON_DISMISS_SUCCEEDED;

import android.app.KeyguardManager;
import android.content.ComponentName;
import android.server.wm.TestJournalProvider.TestJournalContainer;

class KeyguardTestBase extends ActivityManagerTestBase {

    KeyguardManager mKeyguardManager;

    @Override
    public void setUp() throws Exception {
        super.setUp();
        mKeyguardManager = mContext.getSystemService(KeyguardManager.class);
    }

    static void assertOnDismissSucceeded(ComponentName testingComponentName) {
        assertDismissCallback(testingComponentName, ENTRY_ON_DISMISS_SUCCEEDED);
    }

    static void assertOnDismissCancelled(ComponentName testingComponentName) {
        assertDismissCallback(testingComponentName, ENTRY_ON_DISMISS_CANCELLED);
    }

    static void assertOnDismissError(ComponentName testingComponentName) {
        assertDismissCallback(testingComponentName, ENTRY_ON_DISMISS_ERROR);
    }

    private static void assertDismissCallback(ComponentName testingComponentName, String entry) {
        waitForOrFail(entry + " of " + testingComponentName,
                () -> TestJournalContainer.get(testingComponentName).extras.getBoolean(entry));
    }
}
