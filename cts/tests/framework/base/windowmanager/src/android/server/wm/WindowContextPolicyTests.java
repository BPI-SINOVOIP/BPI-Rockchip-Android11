/*
 * Copyright (C) 2020 The Android Open Source Project
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

package android.server.wm;

import static android.view.WindowManager.LayoutParams.TYPE_APPLICATION;
import static android.view.WindowManager.LayoutParams.TYPE_APPLICATION_OVERLAY;

import android.platform.test.annotations.Presubmit;

import org.junit.Test;

/**
 * Tests that verify the behavior of window context policy
 *
 * Build/Install/Run:
 *     atest CtsWindowManagerDeviceTestCases:WindowContextPolicyTests
 */
@Presubmit
public class WindowContextPolicyTests extends WindowContextTestBase {

    @Test(expected = UnsupportedOperationException.class)
    public void testCreateWindowContextWithNoDisplayContext() {
        mContext.createWindowContext(TYPE_APPLICATION_OVERLAY, null);
    }

    @Test(expected = UnsupportedOperationException.class)
    public void testCreateTooManyWindowContextWithoutViewThrowException() {
        final WindowManagerState.DisplayContent display =  createManagedVirtualDisplaySession()
                .setSimulateDisplay(true).createDisplay();
        for (int i = 0; i < 6; i++) {
            createWindowContext(display.mId);
        }
    }

    @Test(expected = RuntimeException.class)
    public void testWindowContextWithIllegalWindowType() {
        final WindowManagerState.DisplayContent display =  createManagedVirtualDisplaySession()
                .setSimulateDisplay(true).createDisplay();
        createWindowContext(display.mId, TYPE_APPLICATION);
    }
}
