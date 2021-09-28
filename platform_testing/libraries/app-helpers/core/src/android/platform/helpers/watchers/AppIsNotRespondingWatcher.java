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

package android.platform.helpers.watchers;

import android.app.Instrumentation;
import android.support.test.uiautomator.By;
import android.support.test.uiautomator.UiWatcher;
import android.support.test.uiautomator.UiDevice;
import android.support.test.uiautomator.UiObject2;
import android.util.Log;

/** Clicks the Close button to close Application Isn't Responding dialogs */
public class AppIsNotRespondingWatcher implements UiWatcher {
    private static final String LOG_TAG = AppIsNotRespondingWatcher.class.getSimpleName();
    private final UiDevice mDevice;

    private static final String UI_ANDROID_PACKAGE = "android";
    private static final String UI_CLOSE_APP_BUTTON_ID = "aerr_close";
    private static final String UI_ALERT_TITLE_ID = "alertTitle";

    public AppIsNotRespondingWatcher(Instrumentation instrumentation) {
        mDevice = UiDevice.getInstance(instrumentation);
    }

    /** {@inheritDoc} */
    @Override
    public boolean checkForCondition() {
        UiObject2 closeButton =
                mDevice.findObject(By.res(UI_ANDROID_PACKAGE, UI_CLOSE_APP_BUTTON_ID));
        if (closeButton != null) {
            UiObject2 alertTitle =
                    mDevice.findObject(By.res(UI_ANDROID_PACKAGE, UI_ALERT_TITLE_ID));
            Log.w(LOG_TAG, String.format(
                    "Found the \"%s\" dialog and dismissed it",
                    alertTitle == null ? "Application Isn't Responding" : alertTitle.getText()));

            closeButton.click();
            return true;
        }
        return false;
    }
}
