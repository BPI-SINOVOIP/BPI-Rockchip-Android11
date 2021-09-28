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

import android.view.accessibility.AccessibilityEvent;

import java.util.concurrent.TimeoutException;

/**
 * Base class for test cases using {@link GridActivity}.
 */
abstract class AbstractGridActivityTestCase
        extends AutoFillServiceTestCase.AutoActivityLaunch<GridActivity> {

    protected GridActivity mActivity;

    @Override
    protected AutofillActivityTestRule<GridActivity> getActivityRule() {
        return new AutofillActivityTestRule<GridActivity>(GridActivity.class) {
            @Override
            protected void afterActivityLaunched() {
                mActivity = getActivity();
                postActivityLaunched();
            }
        };
    }

    /**
     * Hook for subclass to customize activity after it's launched.
     */
    protected void postActivityLaunched() {
    }

    /**
     * Focus to a cell and expect window event
     */
    protected void focusCell(int row, int column) throws TimeoutException {
        mUiBot.waitForWindowChange(() -> mActivity.focusCell(row, column));
    }

    /**
     * Focus to a cell and expect no window event.
     */
    protected void focusCellNoWindowChange(int row, int column) {
        final AccessibilityEvent event;
        try {
            event = mUiBot.waitForWindowChange(() -> mActivity.focusCell(row, column),
                    Timeouts.WINDOW_CHANGE_NOT_GENERATED_NAPTIME_MS);
        } catch (WindowChangeTimeoutException ex) {
            // no window events! looking good
            return;
        }
        throw new IllegalStateException(String.format("Expect no window event when focusing to"
                + " column %d row %d, but event happened: %s", row, column, event));
    }
}
