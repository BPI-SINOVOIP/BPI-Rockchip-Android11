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

package com.android.cts.deviceandprofileowner;

import static android.os.UserManager.DISALLOW_CONTENT_SUGGESTIONS;

import static com.android.cts.deviceandprofileowner.ContentSuggestionsActivity.CONTENT_SUGGESTIONS_ACTIVITY_NAME;
import static com.android.cts.deviceandprofileowner.ContentSuggestionsActivity.CONTENT_SUGGESTIONS_PACKAGE_NAME;

import android.content.Intent;

public class ContentSuggestionsRestrictionsTest extends BaseDeviceAdminTest {

    private static final int MAX_TIME_TEMPORARY_SERVICE_CAN_BE_SET= 12000;

    private static final int SLEEP_TIME_WAITING_FOR_SERVICE_CONNECTION_MS = 100;

    private static final String SERVICE_NAME =
            "com.android.cts.devicepolicy.contentsuggestions/.SimpleContentSuggestionsService";

    int mUserId;

    @Override
    protected void setUp() throws Exception {
        super.setUp();

        mUserId = getInstrumentation().getContext().getUserId();
    }

    @Override
    protected void tearDown() throws Exception {
        try {
            disableService();
        } finally {
            mDevicePolicyManager.clearUserRestriction(ADMIN_RECEIVER_COMPONENT,
                    DISALLOW_CONTENT_SUGGESTIONS);
        }
        super.tearDown();
    }

    public void testDisallowContentSuggestions_allowed() throws Exception {
        enableService();

        final boolean enabledBefore = launchActivityAndGetEnabled();
        assertTrue(enabledBefore);

        mDevicePolicyManager.addUserRestriction(
                ADMIN_RECEIVER_COMPONENT, DISALLOW_CONTENT_SUGGESTIONS);

        // Must try a couple times because it will be disabled asynchronously.
        for (int i = 1; i <= 5; i++) {
            final boolean disabledAfter = !launchActivityAndGetEnabled();
            if (disabledAfter) {
                return;
            }
            Thread.sleep(100);
        }
        fail("Not disabled after 2.5s");
    }

    private boolean launchActivityAndGetEnabled() throws Exception {
        final Intent launchIntent = new Intent();
        launchIntent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        launchIntent.setClassName(
                CONTENT_SUGGESTIONS_PACKAGE_NAME, CONTENT_SUGGESTIONS_ACTIVITY_NAME);
        final ContentSuggestionsActivity activity = launchActivity(
                "com.android.cts.deviceandprofileowner", ContentSuggestionsActivity.class, null);
        return activity.isContentSuggestionsEnabled();
    }

    private void enableService() throws Exception {
        runShellCommand("cmd content_suggestions set temporary-service %d %s %d", mUserId,
                SERVICE_NAME, MAX_TIME_TEMPORARY_SERVICE_CAN_BE_SET);
        // TODO: ideally it should wait until the service's onConnected() is called, but that
        // would be too complicated
        Thread.sleep(SLEEP_TIME_WAITING_FOR_SERVICE_CONNECTION_MS);
    }

    private void disableService() {
        runShellCommand("cmd content_suggestions set temporary-service %d", mUserId);
    }
}
