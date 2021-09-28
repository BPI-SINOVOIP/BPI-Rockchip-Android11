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
 * limitations under the License
 */

package android.server.wm;

import static android.content.Intent.FLAG_ACTIVITY_NEW_TASK;
import static android.server.wm.ActivityManagerTestBase.launchHomeActivityNoWait;
import static android.server.wm.UiDeviceUtils.pressUnlockButton;
import static android.server.wm.UiDeviceUtils.pressWakeupButton;
import static android.view.Display.DEFAULT_DISPLAY;

import static androidx.test.platform.app.InstrumentationRegistry.getInstrumentation;

import static org.junit.Assert.assertEquals;

import android.app.Activity;
import android.app.ActivityOptions;
import android.content.Intent;
import android.os.Bundle;

import com.android.compatibility.common.util.SystemUtil;

import org.junit.Before;

import java.lang.reflect.Array;
import java.util.ArrayList;
import java.util.List;

import javax.annotation.concurrent.GuardedBy;

public class WindowManagerTestBase extends MultiDisplayTestBase {
    static final long TIMEOUT_WINDOW_FOCUS_CHANGED = 1000; // milliseconds

    @Before
    public void setupBase() {
        pressWakeupButton();
        pressUnlockButton();
        launchHomeActivityNoWait();
    }

    static <T extends FocusableActivity> T startActivity(Class<T> cls) {
        return startActivity(cls, DEFAULT_DISPLAY);
    }

    static <T extends FocusableActivity> T startActivity(Class<T> cls, int displayId,
            boolean hasFocus) {
        final Bundle options = (displayId == DEFAULT_DISPLAY
                ? null : ActivityOptions.makeBasic().setLaunchDisplayId(displayId).toBundle());
        final T[] activity = (T[]) Array.newInstance(FocusableActivity.class, 1);
        SystemUtil.runWithShellPermissionIdentity(() -> {
            activity[0] = (T) getInstrumentation().startActivitySync(
                    new Intent(getInstrumentation().getTargetContext(), cls)
                            .addFlags(FLAG_ACTIVITY_NEW_TASK), options);
            activity[0].waitAndAssertWindowFocusState(hasFocus);
        });
        return activity[0];
    }

    static <T extends FocusableActivity> T startActivity(Class<T> cls, int displayId) {
      return startActivity(cls, displayId, true /* hasFocus */);
    }

    static class FocusableActivity extends Activity {
        private final Object mLockWindowFocus = new Object();

        @GuardedBy("mLockWindowFocus")
        private boolean mHasWindowFocus;

        final String getLogTag() {
            return ComponentNameUtils.getLogTag(getComponentName());
        }

        @Override
        public void onWindowFocusChanged(boolean hasFocus) {
            synchronized (mLockWindowFocus) {
                mHasWindowFocus = hasFocus;
                mLockWindowFocus.notify();
            }
        }

        void assertWindowFocusState(boolean hasFocus) {
            synchronized (mLockWindowFocus) {
                assertEquals(getLogTag() + " must" + (hasFocus ? "" : " not")
                        + " have window focus.", hasFocus, mHasWindowFocus);
            }
        }

        void waitAndAssertWindowFocusState(boolean hasFocus) {
            synchronized (mLockWindowFocus) {
                if (mHasWindowFocus != hasFocus) {
                    try {
                        mLockWindowFocus.wait(TIMEOUT_WINDOW_FOCUS_CHANGED);
                    } catch (InterruptedException e) {
                    }
                }
            }
            assertWindowFocusState(hasFocus);
        }
    }
}
