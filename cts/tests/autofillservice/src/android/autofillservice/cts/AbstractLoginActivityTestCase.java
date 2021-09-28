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

package android.autofillservice.cts;

import android.view.View;
import android.view.accessibility.AccessibilityEvent;

import java.util.concurrent.TimeoutException;

/**
 * Base class for test cases using {@link LoginActivity}.
 */
public abstract class AbstractLoginActivityTestCase
        extends AutoFillServiceTestCase.AutoActivityLaunch<LoginActivity> {

    protected LoginActivity mActivity;

    protected AbstractLoginActivityTestCase() {
    }

    protected AbstractLoginActivityTestCase(UiBot inlineUiBot) {
        super(inlineUiBot);
    }

    @Override
    protected AutofillActivityTestRule<LoginActivity> getActivityRule() {
        return new AutofillActivityTestRule<LoginActivity>(
                LoginActivity.class) {
            @Override
            protected void afterActivityLaunched() {
                mActivity = getActivity();
            }
        };
    }

    /**
     * Requests focus on username and expect Window event happens.
     */
    protected void requestFocusOnUsername() throws TimeoutException {
        mUiBot.waitForWindowChange(() -> mActivity.onUsername(View::requestFocus));
    }

    /**
     * Requests focus on username and expect no Window event happens.
     */
    protected void requestFocusOnUsernameNoWindowChange() {
        final AccessibilityEvent event;
        try {
            event = mUiBot.waitForWindowChange(() -> mActivity.onUsername(View::requestFocus),
                    Timeouts.WINDOW_CHANGE_NOT_GENERATED_NAPTIME_MS);
        } catch (WindowChangeTimeoutException ex) {
            // no window events! looking good
            return;
        }
        throw new IllegalStateException("Expect no window event when focusing to"
                + " username, but event happened: " + event);
    }

    /**
     * Requests focus on password and expect Window event happens.
     */
    protected void requestFocusOnPassword() throws TimeoutException {
        mUiBot.waitForWindowChange(() -> mActivity.onPassword(View::requestFocus));
    }

    /**
     * Clears focus from input fields by focusing on the parent layout.
     */
    protected void clearFocus() {
        mActivity.clearFocus();
    }
}
