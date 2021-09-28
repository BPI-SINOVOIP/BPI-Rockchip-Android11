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

package android.telephonyprovider.cts;

import static android.telephonyprovider.cts.DefaultSmsAppHelper.hasTelephony;

import android.content.ContentResolver;
import android.provider.Telephony;
import android.test.InstrumentationTestCase;

public class CellBroadcastProviderTest extends InstrumentationTestCase {
    private ContentResolver mContentResolver;
    private boolean mHasTelephony;

    @Override
    protected void setUp() throws Exception {
        super.setUp();
        mContentResolver = getInstrumentation().getContext().getContentResolver();
        mHasTelephony = hasTelephony();
    }

    // this is only allowed for privileged process
    public void testAccess() {
        if (!mHasTelephony) {
            return;
        }
        try {
            mContentResolver.query(Telephony.CellBroadcasts.CONTENT_URI,
                    null, null, null, null);
            fail("No SecurityException thrown");
        } catch (SecurityException e) {
            // expected
        }
    }

    public void testAccessMessageHistoryWithoutPermission() {
        if (!mHasTelephony) {
            return;
        }
        try {
            mContentResolver.query(Telephony.CellBroadcasts.MESSAGE_HISTORY_URI,
                    null, null, null, null);
            fail("No SecurityException thrown");
        } catch (SecurityException e) {
            // expected
        }
    }
}

