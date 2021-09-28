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

import static org.mockito.Mockito.verify;

import android.platform.helpers.INotificationHelper;

import org.junit.Test;
import org.junit.runner.Description;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;
import org.junit.runners.model.Statement;
import org.mockito.Mockito;

/**
 * Unit test the logic for {@link NotificationPressureRule}
 */
@RunWith(JUnit4.class)
public class NotificationPressureRuleTest {

    private static final int TEST_NOTIFICATION_COUNT = 50;
    private static final String TEST_PACKAGE = "package.name";

    /** Tests that notifications are posted before the test method and cancelled at the end. */
    @Test
    public void testPostNotifications_withoutPackage() throws Throwable {
        TestableNotificationPressureRule rule =
                new TestableNotificationPressureRule(TEST_NOTIFICATION_COUNT);
        Statement testStatement =
                new Statement() {
                    @Override
                    public void evaluate() throws Throwable {
                        // Assert that device posted new notifications.
                        verify(rule.initNotificationHelper())
                                .postNotifications(TEST_NOTIFICATION_COUNT, null);
                    }
                };

        rule.apply(testStatement, Description.createTestDescription("clzz", "mthd"))
                .evaluate();

        // Assert that all notifications are cancelled at the end of the test.
        verify(rule.initNotificationHelper()).cancelNotifications();
    }

    /**
     * Tests that notifications are posted with package before the test method and cancelled at the
     * end.
     */
    @Test
    public void testPostNotifications_withPackage() throws Throwable {
        TestableNotificationPressureRule rule =
                new TestableNotificationPressureRule(TEST_NOTIFICATION_COUNT, TEST_PACKAGE);
        Statement testStatement =
                new Statement() {
                    @Override
                    public void evaluate() throws Throwable {
                        // Assert that device posted new notifications.
                        verify(rule.initNotificationHelper())
                                .postNotifications(TEST_NOTIFICATION_COUNT, TEST_PACKAGE);
                    }
                };

        rule.apply(testStatement, Description.createTestDescription("clzz", "mthd")).evaluate();

        // Assert that all notifications are cancelled at the end of the test.
        verify(rule.initNotificationHelper()).cancelNotifications();
    }

    private static final class TestableNotificationPressureRule extends NotificationPressureRule {
        private INotificationHelper mNotificationHelper;

        TestableNotificationPressureRule(int notificationCount) {
            super(notificationCount);
        }

        TestableNotificationPressureRule(int notificationCount, String pkg) {
            super(notificationCount, pkg);
        }

        @Override
        protected INotificationHelper initNotificationHelper() {
            if (mNotificationHelper == null) {
                mNotificationHelper = Mockito.mock(INotificationHelper.class);
            }
            return mNotificationHelper;
        }
    }
}
