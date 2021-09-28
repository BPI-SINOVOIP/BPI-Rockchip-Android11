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

import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import android.support.test.uiautomator.UiDevice;

import org.junit.Test;
import org.junit.runner.Description;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;
import org.junit.runners.model.Statement;

import org.mockito.Mockito;

/**
 * Unit test the logic for {@link NaturalOrientationRule}
 */
@RunWith(JUnit4.class)
public class NaturalOrientationRuleTest {
    /**
     * Tests that natural orientation is set before the test and unfrozen afterwards.
     */
    @Test
    public void testNaturalOrientationSetAndUnset() throws Throwable {
        TestableNaturalOrientationRule rule = new TestableNaturalOrientationRule();
        Statement testStatement = new Statement() {
            @Override
            public void evaluate() throws Throwable {
                // Assert that the device has been locked in natural orientation.
                verify(rule.getUiDevice(), times(1)).setOrientationNatural();
                verify(rule.getUiDevice(), never()).unfreezeRotation();
            }
        };
        rule.apply(testStatement, Description.createTestDescription("clzz", "mthd")).evaluate();
        // Assert that the device was unlocked after the test.
        verify(rule.getUiDevice(), times(1)).unfreezeRotation();
    }

    private static class TestableNaturalOrientationRule extends NaturalOrientationRule {
        private UiDevice mUiDevice;

        public TestableNaturalOrientationRule() {
            mUiDevice = Mockito.mock(UiDevice.class);
        }

        @Override
        protected UiDevice getUiDevice() {
            return mUiDevice;
        }
    }
}
