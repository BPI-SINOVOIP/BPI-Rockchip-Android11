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

import static android.view.WindowManager.LayoutParams.TYPE_APPLICATION_OVERLAY;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import android.content.Context;
import android.os.UserHandle;
import android.platform.test.annotations.Presubmit;
import android.view.View;
import android.view.WindowManager;

import org.junit.Test;

@Presubmit
public class AddWindowAsUserTest extends ActivityManagerTestBase  {
    @Test
    public void testAddWindowSecondaryUser() {
        // Get original userId from context first, not every platform use SYSTEM as default user.
        final int myUserId = mContext.getUserId();
        testAddWindowWithUser(UserHandle.of(myUserId), false /* shouldCatchException */);

        // Doesn't grant INTERACT_ACROSS_USERS_FULL permission, so any other user should not
        // able to add window.
        testAddWindowWithUser(UserHandle.ALL, true);
    }

    private void testAddWindowWithUser(UserHandle user, boolean shouldCatchException) {
        mInstrumentation.runOnMainSync(() -> {
            final View view = new View(mContext);
            final WindowManager wm = getWindowManagerForUser(user);
            boolean catchException = false;
            try {
                wm.addView(view, new WindowManager.LayoutParams(TYPE_APPLICATION_OVERLAY));
            } catch (WindowManager.BadTokenException exception) {
                catchException = true;
            } finally {
                if (!catchException) {
                    wm.removeViewImmediate(view);
                }
            }
            if (shouldCatchException) {
                assertTrue("Should receive exception for user " + user, catchException);
            } else {
                assertFalse("Shouldn't receive exception for user " + user, catchException);
            }
        });
    }

    private WindowManager getWindowManagerForUser(UserHandle user) {
        final Context userContext = mContext.createContextAsUser(user, 0);
        return userContext.getSystemService(WindowManager.class);
    }
}
