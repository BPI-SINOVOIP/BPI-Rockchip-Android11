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
package android.platform.test.rule;

import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.os.RemoteException;
import android.support.test.uiautomator.UiDevice;

import org.junit.Test;
import org.junit.runner.Description;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;
import org.junit.runners.model.Statement;

import org.mockito.Mockito;

/** Unit test the logic for {@link UnlockScreenRule} */
@RunWith(JUnit4.class)
public class UnlockScreenRuleTest {

    /** Tests that unlock a screen off and locked phone before the test. */
    @Test
    public void testScreenOff() throws Throwable {
        TestableUnlockScreenRule rule = new TestableUnlockScreenRule(false, false);
        Statement testStatement =
                new Statement() {
                    @Override
                    public void evaluate() throws Throwable {
                        // Assert that the device wake up and unlock screen.
                        verify(rule.getUiDevice(), times(1)).wakeUp();
                        verify(rule.getUiDevice(), times(1)).pressMenu();
                    }
                };
        rule.apply(testStatement, Description.createTestDescription("clzz", "mthd")).evaluate();
    }

    /** Tests that unlock a screen on but locked phone before the test. */
    @Test
    public void testScreenLocked() throws Throwable {
        TestableUnlockScreenRule rule = new TestableUnlockScreenRule(true, false);
        Statement testStatement =
                new Statement() {
                    @Override
                    public void evaluate() throws Throwable {
                        // Assert that the device has unlocked screen.
                        verify(rule.getUiDevice(), times(0)).wakeUp();
                        verify(rule.getUiDevice(), times(1)).pressMenu();
                    }
                };
        rule.apply(testStatement, Description.createTestDescription("clzz", "mthd")).evaluate();
    }

    /** Tests that unlock a phone which is already screen on before the test. */
    @Test
    public void testScreenOn() throws Throwable {
        TestableUnlockScreenRule rule = new TestableUnlockScreenRule(true, true);
        Statement testStatement =
                new Statement() {
                    @Override
                    public void evaluate() throws Throwable {
                        // Assert that the device did nothing.
                        verify(rule.getUiDevice(), times(0)).wakeUp();
                        verify(rule.getUiDevice(), times(0)).pressMenu();
                    }
                };
        rule.apply(testStatement, Description.createTestDescription("clzz", "mthd")).evaluate();
    }

    private static class TestableUnlockScreenRule extends UnlockScreenRule {
        private UiDevice mUiDevice;
        private boolean mIsScreenOn;
        private boolean mIsUnlocked;

        public TestableUnlockScreenRule(boolean isScreenOn, boolean isUnlocked) {
            mUiDevice = Mockito.mock(UiDevice.class);
            mIsScreenOn = isScreenOn;
            mIsUnlocked = isUnlocked;
        }

        @Override
        protected UiDevice getUiDevice() {
            try {
                when(mUiDevice.isScreenOn()).thenReturn(mIsScreenOn);
                when(mUiDevice.hasObject(SCREEN_LOCK)).thenReturn(!mIsUnlocked);
            } catch (RemoteException e) {
                throw new RuntimeException("Could not unlock device.", e);
            }
            return mUiDevice;
        }
    }
}
