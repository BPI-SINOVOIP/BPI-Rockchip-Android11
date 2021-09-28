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

package android.view.cts;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;
import static org.junit.Assert.fail;

import android.app.Instrumentation;
import android.app.UiAutomation;
import android.os.SystemClock;
import android.view.KeyEvent;

import androidx.test.ext.junit.runners.AndroidJUnit4;
import androidx.test.filters.LargeTest;
import androidx.test.platform.app.InstrumentationRegistry;
import androidx.test.rule.ActivityTestRule;

import com.android.compatibility.common.util.SystemUtil;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import java.io.IOException;
import java.util.concurrent.BlockingQueue;
import java.util.concurrent.LinkedBlockingQueue;
import java.util.concurrent.TimeUnit;

@LargeTest
@RunWith(AndroidJUnit4.class)
public class KeyEventInjectionTest implements KeyEvent.Callback {
    private static final String TAG = "KeyEventInjectionTest";

    private static final int TEST_KEYCODE = KeyEvent.KEYCODE_SPACE;

    private KeyEventInjectionActivity mActivity;
    private Instrumentation mInstrumentation;
    private UiAutomation mAutomation;
    private final BlockingQueue<KeyEvent> mEvents = new LinkedBlockingQueue<>();
    private final BlockingQueue<KeyEvent> mLongPressEvents = new LinkedBlockingQueue<>();

    @Rule
    public ActivityTestRule<KeyEventInjectionActivity> mActivityRule =
            new ActivityTestRule<>(KeyEventInjectionActivity.class);

    @Before
    public void setUp() {
        mActivity = mActivityRule.getActivity();
        mActivity.setCallback(this);
        mInstrumentation = InstrumentationRegistry.getInstrumentation();
        mAutomation = mInstrumentation.getUiAutomation();
        mEvents.clear();
        mLongPressEvents.clear();
    }

    /**
     * Long press event can be injected through UiAutomation
     */
    @Test
    public void testLongPressKeyEventInjectedViaInstrumentation() {
        sendKeyViaInstrumentation(TEST_KEYCODE, true /*longPress*/);
        checkKeyLongPress(TEST_KEYCODE);
    }

    /**
     * Long press event can be injected through ADB
     */
    @Test
    public void testLongPressKeyEventInjectedViaAdb() {
        sendKeyViaAdb(TEST_KEYCODE, true /* logPress */);
        checkKeyLongPress(TEST_KEYCODE);
    }

    /**
     * Inject a regular key event through UiAutomation
     */
    @Test
    public void testKeyEventInjectedViaInstrumentation() {
        sendKeyViaInstrumentation(TEST_KEYCODE, false /*longPress*/);
        checkKeyPress(TEST_KEYCODE);
    }

    /**
     * Inject a regular key event through ADB
     */
    @Test
    public void testKeyEventInjectedViaAdb() {
        sendKeyViaAdb(TEST_KEYCODE, false /*longPress*/);
        checkKeyPress(TEST_KEYCODE);
    }

    // KeyEvent.Callback overrides
    @Override
    public boolean onKeyDown(int keyCode, KeyEvent event) {
        mEvents.add(event);
        return true;
    }

    @Override
    public boolean onKeyUp(int keyCode, KeyEvent event) {
        mEvents.add(event);
        return true;
    }

    @Override
    public boolean onKeyLongPress(int keyCode, KeyEvent event) {
        mLongPressEvents.add(event);
        return true;
    }

    @Override
    public boolean onKeyMultiple(int keyCode, int count, KeyEvent event) {
        fail("ACTION_MULTIPLE is deprecated. Received: " + event);
        return true;
    }

    private KeyEvent popEvent(BlockingQueue<KeyEvent> events) {
        try {
            return events.poll(5, TimeUnit.SECONDS);
        } catch (InterruptedException e) {
            fail("Interrupted while waiting for event to be added to queue");
        }
        return null;
    }

    private void sendKeyViaAdb(int keyCode, boolean longPress) {
        // Inject key event through the adb command
        final String command = "input keyevent " + (longPress ? "--longpress " : "") + keyCode;
        try {
            SystemUtil.runShellCommand(mInstrumentation, command);
        } catch (IOException e) {
            fail("Could not send adb command '" + command + "', " + e);
        }
    }

    private void sendKeyViaInstrumentation(int keyCode, boolean longPress) {
        final long downTime = SystemClock.uptimeMillis();
        int repeatCount = 0;
        KeyEvent downEvent =
                new KeyEvent(downTime, downTime, KeyEvent.ACTION_DOWN, keyCode, repeatCount);
        mAutomation.injectInputEvent(downEvent, true);
        if (longPress) {
            repeatCount += 1;
            KeyEvent repeatEvent = new KeyEvent(downTime, SystemClock.uptimeMillis(),
                    KeyEvent.ACTION_DOWN, keyCode, repeatCount);
            mAutomation.injectInputEvent(repeatEvent, true);
        }
        KeyEvent upEvent = new KeyEvent(downTime, SystemClock.uptimeMillis(),
                KeyEvent.ACTION_UP, keyCode, 0 /* repeatCount */);
        mAutomation.injectInputEvent(upEvent, true);
    }

    private void waitForLongPress(int keyCode) {
        KeyEvent event = popEvent(mLongPressEvents);
        assertNotNull(event);
        assertEquals(keyCode, event.getKeyCode());
    }

    private void assertLongPressNotReceived() {
        // Should not have received any long press invocations
        assertTrue(mLongPressEvents.isEmpty());
    }

    private void checkKeyEvent(int action, int keyCode, int repeatCount) {
        KeyEvent event = popEvent(mEvents);
        assertNotNull(event);
        assertEquals("action: ", action, event.getAction());
        assertEquals("Expected " + KeyEvent.keyCodeToString(keyCode) + ", received "
                + KeyEvent.keyCodeToString(event.getKeyCode()),
                keyCode, event.getKeyCode());
        assertEquals("repeatCount: " , repeatCount, event.getRepeatCount());
    }

    private void checkKeyLongPress(int keyCode) {
        checkKeyEvent(KeyEvent.ACTION_DOWN, keyCode, 0 /* repeatCount */);
        checkKeyEvent(KeyEvent.ACTION_DOWN, keyCode, 1 /* repeatCount */);
        checkKeyEvent(KeyEvent.ACTION_UP, keyCode, 0 /* repeatCount */);
        waitForLongPress(keyCode);
    }

    private void checkKeyPress(int keyCode) {
        checkKeyEvent(KeyEvent.ACTION_DOWN, keyCode, 0 /* repeatCount */);
        checkKeyEvent(KeyEvent.ACTION_UP, keyCode, 0 /* repeatCount */);
        assertLongPressNotReceived();
    }
}
