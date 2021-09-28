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
package android.platform.test.rule;

import android.os.Bundle;

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
 * Unit test the logic for {@link PressHomeRule}
 */
@RunWith(JUnit4.class)
public class PressHomeRuleTest {
    /**
     * Tests that press home happens at the end of the test method.
     */
    @Test
    public void testPressHomeEnabled() throws Throwable {

        Bundle pressHomeBundle = new Bundle();
        pressHomeBundle.putString(PressHomeRule.GO_HOME, "true");
        TestablePressHomeRule rule = new TestablePressHomeRule(pressHomeBundle);

        Statement testStatement = new Statement() {
            @Override
            public void evaluate() throws Throwable {
                // Assert that navigate to press home is not initiated.
                verify(rule.getUiDevice(), never()).pressHome();
            }
        };
        rule.apply(testStatement, Description.createTestDescription("clzz", "mthd")).evaluate();
        // Assert that the home button is pressed after the test method.
        verify(rule.getUiDevice(), times(1)).pressHome();
    }

    /**
     * Tests that did not press home if the option is disabled
     */
    @Test
    public void testPressHomeDisabled() throws Throwable {

        Bundle pressHomeBundle = new Bundle();
        pressHomeBundle.putString(PressHomeRule.GO_HOME, "false");
        TestablePressHomeRule rule = new TestablePressHomeRule(pressHomeBundle);

        Statement testStatement = new Statement() {
            @Override
            public void evaluate() throws Throwable {
                // Assert that navigate to press home is not initiated.
                verify(rule.getUiDevice(), never()).pressHome();
            }
        };
        rule.apply(testStatement, Description.createTestDescription("clzz", "mthd")).evaluate();
        // Assert that the home button is not pressed after the test method.
        verify(rule.getUiDevice(), times(0)).pressHome();
    }

    private static class TestablePressHomeRule extends PressHomeRule {
        private UiDevice mUiDevice;
        private Bundle mBundle;

        public TestablePressHomeRule(Bundle bundle) {
            mUiDevice = Mockito.mock(UiDevice.class);
            mBundle = bundle;
        }

        @Override
        protected Bundle getArguments() {
            return mBundle;
        }

        @Override
        protected UiDevice getUiDevice() {
            return mUiDevice;
        }
    }
}
