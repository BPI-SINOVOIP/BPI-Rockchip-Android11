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

package com.android.cts.cpptools;

import com.android.tradefed.testtype.DeviceTestCase;

import org.junit.Test;

/**
 * Please talk to Android Studio team first if you want to modify or delete
 * these tests.
 */
public final class RunasPermissionsTest extends DeviceTestCase {

    private static final String TEST_APP_PACKAGE = "com.android.cts.domainsocketapp";
    private static final String TEST_APP_CLASS = "DomainSocketActivity";
    private static final String CONNECTOR_EXE_NAME = "connector";
    private static final String START_TEST_APP_COMMAND = String.format("cmd activity start-activity -S -W %s/%s.%s",
            TEST_APP_PACKAGE, TEST_APP_PACKAGE, TEST_APP_CLASS);

    // Test app ('DomainSocketActivity') would pass string "Connection Succeeded." to the
    // connector, and then the connector would append a newline and print it out.
    private static final String EXPECTED_CONNECTOR_OUTPUT = "Connection Succeeded.\n";

    public void testRunasCanConnectToAppsSocket() throws Exception {
        // Start the app activity and wait for it to complete.
        // The app will open a domain socket.
        getDevice().executeShellCommand(START_TEST_APP_COMMAND);

        // Sleep for 0.3 second (300 milliseconds) so the test app has sufficient time
        // to open the socket.
        try {
            Thread.sleep(300);
        } catch (InterruptedException e) {
            // ignored
        }

        // Start a run-as process that attempts to connect to the socket opened by the
        // app.
        int currentUser = getDevice().getCurrentUser();
        getDevice().executeShellCommand(String.format(
                "run-as %s --user %d sh -c 'cp -f /data/local/tmp/%s ./code_cache/'",
                        TEST_APP_PACKAGE, currentUser, CONNECTOR_EXE_NAME));
        String results = getDevice().executeShellCommand(String.format(
                "run-as %s --user %d sh -c './code_cache/%s'",
                        TEST_APP_PACKAGE, currentUser, CONNECTOR_EXE_NAME));
        assertEquals(EXPECTED_CONNECTOR_OUTPUT, results);
    }

}
