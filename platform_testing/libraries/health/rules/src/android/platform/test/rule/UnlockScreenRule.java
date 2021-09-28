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
package android.platform.test.rule;

import android.os.RemoteException;
import android.support.test.uiautomator.By;
import android.support.test.uiautomator.BySelector;

import org.junit.runner.Description;

/** This rule will unlock phone screen before a test case. */
public class UnlockScreenRule extends TestWatcher {

    protected static final BySelector SCREEN_LOCK =
            By.res("com.android.systemui", "keyguard_bottom_area");

    @Override
    protected void starting(Description description) {
        try {
            // Turn on the screen if necessary.
            if (!getUiDevice().isScreenOn()) {
                getUiDevice().wakeUp();
            }
            if (getUiDevice().hasObject(SCREEN_LOCK)) {
                getUiDevice().pressMenu();
                getUiDevice().waitForIdle();
            }
        } catch (RemoteException e) {
            throw new RuntimeException("Could not unlock device.", e);
        }
    }
}
