/*
 * Copyright (C) 2016 The Android Open Source Project
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
package android.provider.cts.contacts;

import android.accounts.Account;
import android.accounts.AccountManager;
import android.content.ContentResolver;
import android.content.pm.PackageManager.NameNotFoundException;
import android.database.Cursor;
import android.database.DatabaseUtils;
import android.net.Uri;
import android.os.SystemClock;
import android.provider.ContactsContract;
import android.provider.ContactsContract.Contacts;
import android.provider.ContactsContract.Directory;
import android.provider.cts.contacts.galprovider.CtsGalProvider;
import android.test.AndroidTestCase;
import android.util.Log;

import org.json.JSONObject;

/**
 * Note, this test creates an account in setUp() and removes it in tearDown(), which causes
 * a churn in the contacts provider.  So this class should have only one test method and do all
 * the check in there, so it won't create the account multiple times.
 */
public class ContactsContract_DirectoryTest extends AndroidTestCase {
    private static final String TAG = "ContactsContract_DirectoryTest";

    private ContentResolver mResolver;
    private AccountManager mAccountManager;
    private Account mAccount;

    private static final int DIRECTORY_WAIT_TIMEOUT_SEC = 60;

    @Override
    protected void setUp() throws Exception {
        super.setUp();

        mResolver = getContext().getContentResolver();

        mAccountManager = getContext().getSystemService(AccountManager.class);
        mAccount = new Account(CtsGalProvider.ACCOUNT_NAME, CtsGalProvider.ACCOUNT_TYPE);

        // The directory table is populated asynchronously.  Wait for it...
        waitForDirectorySetup();
    }

    @Override
    protected void tearDown() throws Exception {
        mAccountManager.removeAccountExplicitly(mAccount);

        super.tearDown();
    }

    private static String getString(Cursor c, String column) {
        return c.getString(c.getColumnIndex(column));
    }

    /**
     * Wait until the directory row is populated in the directory table, and return its ID.
     */
    private long waitForDirectorySetup() throws Exception {
        final long timeout = SystemClock.elapsedRealtime() + DIRECTORY_WAIT_TIMEOUT_SEC * 1000;

        while (SystemClock.elapsedRealtime() < timeout) {
            try (Cursor c = getContext().getContentResolver().query(Directory.CONTENT_URI,
                    null, Directory.ACCOUNT_NAME + "=? and " + Directory.ACCOUNT_TYPE + "=?",
                    new String[]{CtsGalProvider.ACCOUNT_NAME, CtsGalProvider.ACCOUNT_TYPE},
                    null)) {
                if (c.getCount() == 0) {
                    Thread.sleep(1000);
                    continue;
                }
                assertTrue(c.moveToPosition(0));
                assertEquals(CtsGalProvider.GAL_PACKAGE_NAME, getString(c, Directory.PACKAGE_NAME));
                assertEquals(CtsGalProvider.AUTHORITY,
                        getString(c, Directory.DIRECTORY_AUTHORITY));
                assertEquals(CtsGalProvider.DISPLAY_NAME, getString(c, Directory.DISPLAY_NAME));
                assertEquals(CtsGalProvider.ACCOUNT_NAME, getString(c, Directory.ACCOUNT_NAME));
                assertEquals(CtsGalProvider.ACCOUNT_TYPE, getString(c, Directory.ACCOUNT_TYPE));
                return c.getLong(c.getColumnIndex(Directory._ID));
            }
        }
        fail("Directory didn't show up");
        return -1;
    }

    public void testQueryParameters() throws Exception {
        // Test for content types.
        assertEquals(Directory.CONTENT_TYPE, mResolver.getType(Directory.CONTENT_URI));
        assertEquals(Directory.CONTENT_ITEM_TYPE, mResolver.getType(
                Directory.CONTENT_URI.buildUpon().appendPath("1").build()));


        // Get the directory ID.
        final long directoryId = waitForDirectorySetup();

        final Uri queryUri = Contacts.CONTENT_FILTER_URI.buildUpon()
                .appendPath("[QUERY]")
                .appendQueryParameter(ContactsContract.LIMIT_PARAM_KEY, "12")
                .appendQueryParameter(ContactsContract.DIRECTORY_PARAM_KEY, "" + directoryId)

                // This should be ignored.
                .appendQueryParameter(Directory.CALLER_PACKAGE_PARAM_KEY, "abcdef")
                .build();

        try (Cursor c = getContext().getContentResolver().query(
                queryUri, null, null, null, null)) {

            DatabaseUtils.dumpCursor(c);

            assertNotNull(c);
            assertEquals(1, c.getCount());

            assertTrue(c.moveToPosition(0));

            // The result is stored in the display_name column.
            final JSONObject result = new JSONObject(getString(c, Contacts.DISPLAY_NAME));

            if (result.has(CtsGalProvider.ERROR_MESSAGE_KEY)) {
                fail(result.getString(CtsGalProvider.ERROR_MESSAGE_KEY));
            }

            assertEquals("12", result.getString(CtsGalProvider.LIMIT_KEY));
            assertEquals("[QUERY]", result.getString(CtsGalProvider.QUERY_KEY));
            assertEquals(getContext().getPackageName(),
                    result.getString(CtsGalProvider.CALLER_PACKAGE_NAME_KEY));
        }

        // After getting any result from the gal provider, the package will become visible.
        assertTrue("GAL provider should be visible here", isGalProviderVisible());
    }

    private boolean isGalProviderVisible() {
        try {
            String pkg = CtsGalProvider.GAL_PACKAGE_NAME;
            int uid = getContext().getPackageManager().getPackageUid(pkg, 0);
            Log.w(TAG, "UID of " + pkg + " = " + uid);
            return true;
        } catch (NameNotFoundException e) {
            return false;
        }
    }
}
