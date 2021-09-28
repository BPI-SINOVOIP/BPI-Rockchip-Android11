/*
 * Copyright (C) 2013 The Android Open Source Project
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

package android.telephonyprovider.device.cts;

import android.content.ContentResolver;
import android.database.Cursor;
import android.provider.Telephony.Carriers;

import androidx.test.InstrumentationRegistry;
import androidx.test.runner.AndroidJUnit4;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

@RunWith(AndroidJUnit4.class)
public class TelephonyProviderTest {
    private ContentResolver mContentResolver;
    private static final String[] APN_PROJECTION = {
            Carriers.TYPE,
            Carriers.MMSC,
            Carriers.MMSPROXY,
            Carriers.MMSPORT,
            Carriers.MVNO_TYPE,
            Carriers.MVNO_MATCH_DATA
    };

    @Before
    public void setUp() throws Exception {
        mContentResolver = InstrumentationRegistry.getInstrumentation().getContext().getContentResolver();
    }

    // In JB MR1 access to the TelephonyProvider's Carriers table was clamped down and would
    // throw a SecurityException when queried. That was fixed in JB MR2. Verify that 3rd parties
    // can access the APN info the carriers table, after JB MR1.

    // However, in R, a security bug was discovered that let apps read the password by querying
    // multiple times and matching passwords against a regex in the query. Due to this hole, we're
    // locking down the API and no longer allowing the exception. Accordingly, the behavior of this
    // test is now reversed and we expect a SecurityException to be thrown.
    @Test
    public void testAccessToApnsWithChangeEnabled() {
        try {
            String selection = Carriers.CURRENT + " IS NOT NULL";
            String[] selectionArgs = null;
            Cursor cursor = mContentResolver.query(Carriers.CONTENT_URI,
                    APN_PROJECTION, selection, selectionArgs, null);
            Assert.fail("No SecurityException thrown");
        } catch (SecurityException e) {
            // expected
        }
    }

    @Test
    public void testAccessToApnsWithChangeDisabled() {
        try {
            String selection = Carriers.CURRENT + " IS NOT NULL";
            String[] selectionArgs = null;
            Cursor cursor = mContentResolver.query(Carriers.CONTENT_URI,
                    APN_PROJECTION, selection, selectionArgs, null);
        } catch (SecurityException e) {
            Assert.fail("SecurityException thrown");
        }
    }
}
