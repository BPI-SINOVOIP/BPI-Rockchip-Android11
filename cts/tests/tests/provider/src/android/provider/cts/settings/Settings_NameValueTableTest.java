/*
 * Copyright (C) 2008 The Android Open Source Project
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

package android.provider.cts.settings;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;

import android.content.ContentResolver;
import android.database.Cursor;
import android.net.Uri;
import android.provider.Settings;
import android.provider.Settings.NameValueTable;

import androidx.test.InstrumentationRegistry;
import androidx.test.runner.AndroidJUnit4;

import com.android.compatibility.common.util.AdoptShellPermissionsRule;

import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

@RunWith(AndroidJUnit4.class)
public class Settings_NameValueTableTest {

    @Rule
    public AdoptShellPermissionsRule shellPermRule = new AdoptShellPermissionsRule();

    @Test
    public void testPutString() {
        final ContentResolver cr = InstrumentationRegistry.getTargetContext().getContentResolver();

        Uri uri = Settings.System.CONTENT_URI;
        String name = Settings.System.NEXT_ALARM_FORMATTED;
        String value = "value1";

        // before putString
        Cursor c = cr.query(uri, null, null, null, null);
        try {
            assertNotNull(c);
            c.close();

            MyNameValueTable.putString(cr, uri, name, value);
            c = cr.query(uri, null, null, null, null);
            assertNotNull(c);
            c.close();

            // query this row
            String selection = NameValueTable.NAME + "=\"" + name + "\"";
            c = cr.query(uri, null, selection, null, null);
            assertNotNull(c);
            assertEquals(1, c.getCount());
            c.moveToFirst();
            assertEquals(name, c.getString(c.getColumnIndexOrThrow(NameValueTable.NAME)));
            assertEquals(value, c.getString(c.getColumnIndexOrThrow(NameValueTable.VALUE)));
            c.close();
        } finally {
            // TODO should clean up more better
            c.close();
        }
    }

    @Test
    public void testGetUriFor() {
        Uri uri = Uri.parse("content://authority/path");
        String name = "table";

        Uri res = NameValueTable.getUriFor(uri, name);
        assertNotNull(res);
        assertEquals(Uri.withAppendedPath(uri, name), res);
    }

    private static class MyNameValueTable extends NameValueTable {
        protected static boolean putString(ContentResolver resolver, Uri uri, String name,
                String value) {
            return NameValueTable.putString(resolver, uri, name, value);
        }
    }
}
