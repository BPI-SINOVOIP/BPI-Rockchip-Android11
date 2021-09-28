/*
 * Copyright (C) 2017 The Android Open Source Project
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

package android.view.cts;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.fail;

import android.app.Instrumentation;
import android.app.UiAutomation;
import android.os.SystemClock;
import android.view.KeyEvent;

import androidx.test.ext.junit.runners.AndroidJUnit4;
import androidx.test.filters.MediumTest;
import androidx.test.platform.app.InstrumentationRegistry;
import androidx.test.rule.ActivityTestRule;

import com.android.compatibility.common.util.PollingCheck;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

/**
 * Certain KeyEvents should never be delivered to apps. These keys are:
 *      KEYCODE_ASSIST
 *      KEYCODE_VOICE_ASSIST
 *      KEYCODE_HOME
 * This test launches an Activity and injects KeyEvents with the corresponding key codes.
 * The test will fail if any of these keys are received by the activity.
 * Note: The ASSIST tests were removed because they caused a side-effect of launching the
 * assistant asynchronously (as intended), which causes problems with tests which happen to
 * be running later and lose focus/visibility because of that extra window.
 *
 * Certain combinations of keys should be treated as shortcuts. Those are:
 *      KEYCODE_META_* + KEYCODE_ENTER --> KEYCODE_BACK
 *      KEYCODE_META_* + KEYCODE_DEL --> KEYCODE_HOME
 * For those combinations, we make sure that they are either delivered to the app
 * as the desired key (KEYCODE_BACK), or not delivered to the app (KEYCODE_HOME).
 */
@MediumTest
@RunWith(AndroidJUnit4.class)
public class KeyEventInterceptTest {
    private InputEventInterceptTestActivity mActivity;
    private Instrumentation mInstrumentation;

    @Rule
    public ActivityTestRule<InputEventInterceptTestActivity> mActivityRule =
            new ActivityTestRule<>(InputEventInterceptTestActivity.class);

    @Before
    public void setup() {
        mInstrumentation = InstrumentationRegistry.getInstrumentation();
        mActivity = mActivityRule.getActivity();
        PollingCheck.waitFor(mActivity::hasWindowFocus);
    }

    @Test
    public void testKeyCodeHome() {
        testKey(KeyEvent.KEYCODE_HOME);
    }

    @Test
    public void testKeyCodeHomeShortcutLeftMeta() {
        testKeyCodeHomeShortcut(KeyEvent.META_META_LEFT_ON | KeyEvent.META_META_ON);
    }

    @Test
    public void testKeyCodeHomeShortcutRightMeta() {
        testKeyCodeHomeShortcut(KeyEvent.META_META_RIGHT_ON | KeyEvent.META_META_ON);
    }

    @Test
    public void testKeyCodeBackShortcutLeftMeta() {
        testKeyCodeBackShortcut(KeyEvent.META_META_LEFT_ON | KeyEvent.META_META_ON);
    }

    @Test
    public void testKeyCodeBackShortcutRightMeta() {
        testKeyCodeBackShortcut(KeyEvent.META_META_RIGHT_ON | KeyEvent.META_META_ON);
    }

    private void testKeyCodeHomeShortcut(int metaState) {
        final long downTime = SystemClock.uptimeMillis();
        injectEvent(new KeyEvent(downTime, downTime, KeyEvent.ACTION_DOWN,
                KeyEvent.KEYCODE_ENTER, 0, metaState));
        injectEvent(new KeyEvent(downTime, SystemClock.uptimeMillis(), KeyEvent.ACTION_UP,
                KeyEvent.KEYCODE_ENTER, 0, metaState));

        assertKeyNotReceived();
    }

    private void testKeyCodeBackShortcut(int metaState) {
        long downTime = SystemClock.uptimeMillis();
        injectEvent(new KeyEvent(downTime, downTime, KeyEvent.ACTION_DOWN,
                KeyEvent.KEYCODE_DEL, 0, metaState));
        injectEvent(new KeyEvent(downTime, downTime + 1, KeyEvent.ACTION_UP,
                KeyEvent.KEYCODE_DEL, 0, metaState));

        assertKeyReceived(KeyEvent.KEYCODE_BACK, KeyEvent.ACTION_DOWN);
        assertKeyReceived(KeyEvent.KEYCODE_BACK, KeyEvent.ACTION_UP);
        assertKeyNotReceived();
    }

    private void testKey(int keyCode) {
        sendKey(keyCode);
        assertKeyNotReceived();
    }

    private void sendKey(int keyCodeToSend) {
        long downTime = SystemClock.uptimeMillis();
        injectEvent(new KeyEvent(downTime, downTime, KeyEvent.ACTION_DOWN,
                keyCodeToSend, 0, 0));
        injectEvent(new KeyEvent(downTime, downTime + 1, KeyEvent.ACTION_UP,
                keyCodeToSend, 0, 0));
    }

    private void injectEvent(KeyEvent event) {
        final UiAutomation automation = mInstrumentation.getUiAutomation();
        automation.injectInputEvent(event, true);
    }

    private void assertKeyNotReceived() {
        KeyEvent keyEvent = mActivity.mKeyEvents.poll();
        if (keyEvent == null) {
            return;
        }
        fail("Should not have received " + KeyEvent.keyCodeToString(keyEvent.getKeyCode()));
    }

    private void assertKeyReceived(int keyCode, int action) {
        KeyEvent keyEvent = mActivity.mKeyEvents.poll();
        if (keyEvent == null) {
            fail("Did not receive " + KeyEvent.keyCodeToString(keyCode) + ", queue is empty");
        }
        assertEquals(keyCode, keyEvent.getKeyCode());
        assertEquals(action, keyEvent.getAction());
    }
}
