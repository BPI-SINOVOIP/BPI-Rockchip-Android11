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

package android.content.cts;

import android.app.ActivityManager;
import android.content.ContentResolver;
import android.os.UserHandle;
import android.test.AndroidTestCase;

/**
 * Test system behavior of a buggy provider.
 *
 * see @{@link MockBuggyProvider}
 */
public class BuggyProviderTest extends AndroidTestCase {

    public void testGetTypeDoesntCrashSystem() {
        // ensure the system doesn't crash when a provider takes too long to respond
        try {
            ActivityManager.getService().getProviderMimeType(
                    MockBuggyProvider.CONTENT_URI, UserHandle.USER_CURRENT);
        } catch (Exception e) {
            fail("Unexpected exception while fetching type: " + e.getMessage());
        }
    }

    public void testGetTypeViaResolverDoesntCrashSystem() {
        // ensure the system doesn't crash when a provider takes too long to respond
        ContentResolver resolver = mContext.getContentResolver();
        try {
            resolver.getType(MockBuggyProvider.CONTENT_URI);
        } catch (Exception e) {
            fail("Unexpected exception while fetching type: " + e.getMessage());
        }
    }
}
