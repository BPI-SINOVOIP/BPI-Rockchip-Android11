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
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;
import static org.junit.Assert.fail;

import android.content.ContentResolver;
import android.content.res.Configuration;
import android.database.Cursor;
import android.net.Uri;
import android.os.SystemClock;
import android.platform.test.annotations.SecurityTest;
import android.provider.Settings;
import android.provider.Settings.SettingNotFoundException;
import android.provider.Settings.System;

import androidx.test.InstrumentationRegistry;
import androidx.test.runner.AndroidJUnit4;

import org.junit.AfterClass;
import org.junit.BeforeClass;
import org.junit.Test;
import org.junit.runner.RunWith;

@RunWith(AndroidJUnit4.class)
public class Settings_SystemTest {
    private static final String INT_FIELD = System.END_BUTTON_BEHAVIOR;
    private static final String LONG_FIELD = System.SCREEN_OFF_TIMEOUT;
    private static final String FLOAT_FIELD = System.FONT_SCALE;
    private static final String STRING_FIELD = System.NEXT_ALARM_FORMATTED;

    @BeforeClass
    public static void setUp() throws Exception {
        final String packageName = InstrumentationRegistry.getTargetContext().getPackageName();
        InstrumentationRegistry.getInstrumentation().getUiAutomation().executeShellCommand(
                "appops set " + packageName + " android:write_settings allow");

        // Wait a beat to persist the change
        SystemClock.sleep(500);
    }

    @AfterClass
    public static void tearDown() throws Exception {
        final String packageName = InstrumentationRegistry.getTargetContext().getPackageName();
        InstrumentationRegistry.getInstrumentation().getUiAutomation().executeShellCommand(
                "appops set " + packageName + " android:write_settings default");
    }

    @Test
    public void testSystemSettings() throws SettingNotFoundException {
        final ContentResolver cr = InstrumentationRegistry.getTargetContext().getContentResolver();

        /**
         * first query the existing settings in System table, and then insert four
         * rows: an int, a long, a float, a String.
         * Get these four rows to check whether insert succeeded and then restore the original
         * values.
         */

        // first query existing rows
        Cursor c = cr.query(System.CONTENT_URI, null, null, null, null);

        // backup fontScale
        Configuration cfg = new Configuration();
        System.getConfiguration(cr, cfg);
        float store = cfg.fontScale;

        //store all original values
        final String originalIntValue = System.getString(cr, INT_FIELD);
        final String originalLongValue = System.getString(cr, LONG_FIELD);
        final String originalStringValue = System.getString(cr, STRING_FIELD);

        try {
            assertNotNull(c);
            c.close();

            String stringValue = "cts";

            // insert 4 rows, and update 1 rows
            assertTrue(System.putInt(cr, INT_FIELD, 2));
            assertTrue(System.putLong(cr, LONG_FIELD, 20l));
            assertTrue(System.putFloat(cr, FLOAT_FIELD, 1.3f));
            assertTrue(System.putString(cr, STRING_FIELD, stringValue));

            c = cr.query(System.CONTENT_URI, null, null, null, null);
            assertNotNull(c);
            c.close();

            // get these rows to assert
            assertEquals(2, System.getInt(cr, INT_FIELD));
            assertEquals(20l, System.getLong(cr, LONG_FIELD));
            assertEquals(1.3f, System.getFloat(cr, FLOAT_FIELD), 0.001);
            assertEquals(stringValue, System.getString(cr, STRING_FIELD));

            c = cr.query(System.CONTENT_URI, null, null, null, null);
            assertNotNull(c);

            // update fontScale row
            cfg = new Configuration();
            cfg.fontScale = 1.2f;
            assertTrue(System.putConfiguration(cr, cfg));

            System.getConfiguration(cr, cfg);
            assertEquals(1.2f, cfg.fontScale, 0.001);
        } finally {
            // TODO should clean up more better
            c.close();

            //Restore all original values into system
            assertTrue(System.putString(cr, INT_FIELD, originalIntValue));
            assertTrue(System.putString(cr, LONG_FIELD, originalLongValue));
            assertTrue(System.putString(cr, STRING_FIELD, originalStringValue));

            // restore the fontScale
            try {
                // Delay helps ActivityManager in completing its previous font-change processing.
                Thread.sleep(1000);
            } catch (Exception e){}

            cfg.fontScale = store;
            assertTrue(System.putConfiguration(cr, cfg));
        }
    }

    /**
     * Verifies that the invalid values for the font scale setting are rejected.
     */
    @SecurityTest(minPatchLevel = "2021-02")
    @Test
    public void testSystemSettingsRejectInvalidFontSizeScale() throws SettingNotFoundException {
        final ContentResolver cr = InstrumentationRegistry.getTargetContext().getContentResolver();
        // First put in a valid value
        assertTrue(System.putFloat(cr, FLOAT_FIELD, 1.15f));
        try {
            assertFalse(System.putFloat(cr, FLOAT_FIELD, Float.MAX_VALUE));
            fail("Should throw");
        } catch (IllegalArgumentException e) {
        }
        try {
            assertFalse(System.putFloat(cr, FLOAT_FIELD, -1f));
            fail("Should throw");
        } catch (IllegalArgumentException e) {
        }
        try {
            assertFalse(System.putFloat(cr, FLOAT_FIELD, 0.1f));
            fail("Should throw");
        } catch (IllegalArgumentException e) {
        }
        try {
            assertFalse(System.putFloat(cr, FLOAT_FIELD, 30.0f));
            fail("Should throw");
        } catch (IllegalArgumentException e) {
        }
        assertEquals(1.15f, System.getFloat(cr, FLOAT_FIELD), 0.001);
    }

    @Test
    public void testGetDefaultValues() {
        final ContentResolver cr = InstrumentationRegistry.getTargetContext().getContentResolver();

        assertEquals(10, System.getInt(cr, "int", 10));
        assertEquals(20, System.getLong(cr, "long", 20l));
        assertEquals(30.0f, System.getFloat(cr, "float", 30.0f), 0.001);
    }

    @Test
    public void testGetUriFor() {
        String name = "table";

        Uri uri = System.getUriFor(name);
        assertNotNull(uri);
        assertEquals(Uri.withAppendedPath(System.CONTENT_URI, name), uri);
    }
}
