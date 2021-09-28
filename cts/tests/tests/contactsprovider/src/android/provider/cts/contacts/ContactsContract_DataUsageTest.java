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
 * limitations under the License
 */

package android.provider.cts.contacts;

import static android.provider.ContactsContract.DataUsageFeedback;

import android.content.ContentProviderClient;
import android.content.ContentResolver;
import android.content.ContentValues;
import android.net.Uri;
import android.provider.ContactsContract;
import android.provider.ContactsContract.Contacts;
import android.provider.ContactsContract.Data;
import android.provider.cts.contacts.ContactsContract_TestDataBuilder.TestContact;
import android.provider.cts.contacts.ContactsContract_TestDataBuilder.TestRawContact;
import android.test.AndroidTestCase;
import android.text.TextUtils;

/**
 * Note we no longer support contact affinity as of Q, so times_contacted and
 * last_time_contacted are always 0.
 */
public class ContactsContract_DataUsageTest extends AndroidTestCase {

    private ContentResolver mResolver;
    private ContactsContract_TestDataBuilder mBuilder;

    @Override
    protected void setUp() throws Exception {
        super.setUp();
        mResolver = getContext().getContentResolver();
        ContentProviderClient provider =
                mResolver.acquireContentProviderClient(ContactsContract.AUTHORITY);
        mBuilder = new ContactsContract_TestDataBuilder(provider);
    }

    @Override
    protected void tearDown() throws Exception {
        mBuilder.cleanup();
        super.tearDown();
    }

    public void testSingleDataUsageFeedback_incrementsCorrectDataItems() {
        DatabaseAsserts.ContactIdPair ids = DatabaseAsserts.assertAndCreateContact(mResolver);

        long[] dataIds = setupRawContactDataItems(ids.mRawContactId);

        updateDataUsageAndAssert(dataIds[1], 0);
        updateDataUsageAndAssert(dataIds[1], 0);

        updateDataUsageAndAssert(dataIds[2], 0);
        updateDataUsageAndAssert(dataIds[2], 0);

        updateDataUsageAndAssert(dataIds[1], 0);
        updateDataUsageAndAssert(dataIds[1], 0);

        deleteDataUsage();
        RawContactUtil.delete(mResolver, ids.mRawContactId, true);
    }

    public void testMultiIdDataUsageFeedback_incrementsCorrectDataItems() {
        DatabaseAsserts.ContactIdPair ids = DatabaseAsserts.assertAndCreateContact(mResolver);

        long[] dataIds = setupRawContactDataItems(ids.mRawContactId);

        assertDataUsageEquals(dataIds, 0, 0, 0, 0);

        updateMultipleAndAssertUpdateSuccess(new long[] {dataIds[1], dataIds[2]});
        assertDataUsageEquals(dataIds, 0, 0, 0, 0);

        deleteDataUsage();
        RawContactUtil.delete(mResolver, ids.mRawContactId, true);
    }

    private long[] setupRawContactDataItems(long rawContactId) {
        // Create 4 data items.
        long[] dataIds = new long[4];
        dataIds[0] = DataUtil.insertPhoneNumber(mResolver, rawContactId, "555-5555");
        dataIds[1] = DataUtil.insertPhoneNumber(mResolver, rawContactId, "555-5554");
        dataIds[2] = DataUtil.insertEmail(mResolver, rawContactId, "test@thisisfake.com");
        dataIds[3] = DataUtil.insertPhoneNumber(mResolver, rawContactId, "555-5556");
        return dataIds;
    }

    private void updateMultipleAndAssertUpdateSuccess(long[] dataIds) {
        String[] ids = new String[dataIds.length];
        for (int i = 0; i < dataIds.length; i++) {
            ids[i] = String.valueOf(dataIds[i]);
        }
        Uri uri = DataUsageFeedback.FEEDBACK_URI.buildUpon().appendPath(TextUtils.join(",", ids))
                .appendQueryParameter(DataUsageFeedback.USAGE_TYPE,
                        DataUsageFeedback.USAGE_TYPE_CALL).build();
        int result = mResolver.update(uri, new ContentValues(), null, null);
        assertEquals(0, result); // always 0
    }

    private void updateDataUsageAndAssert(long dataId, int assertValue) {
        Uri uri = DataUsageFeedback.FEEDBACK_URI.buildUpon().appendPath(String.valueOf(dataId))
                .appendQueryParameter(DataUsageFeedback.USAGE_TYPE,
                        DataUsageFeedback.USAGE_TYPE_CALL).build();
        int result = mResolver.update(uri, new ContentValues(), null, null);
        assertEquals(0, result); // always 0

        assertDataUsageEquals(dataId, assertValue);
    }

    private void assertDataUsageEquals(long[] dataIds, int... expectedValues) {
        if (dataIds.length != expectedValues.length) {
            throw new IllegalArgumentException("dataIds and expectedValues must be the same size");
        }

        for (int i = 0; i < dataIds.length; i++) {
            assertDataUsageEquals(dataIds[i], expectedValues[i]);
        }
    }

    /**
     * Assert a single data item has a specific usage value.
     */
    private void assertDataUsageEquals(long dataId, int expectedValue) {
        // Query and assert value is expected.
        String[] projection = new String[]{ContactsContract.Data.TIMES_USED};
        String[] record = DataUtil.queryById(mResolver, dataId, projection);
        assertNotNull(record);
        long actual = 0;
        // Tread null as 0
        if (record[0] != null) {
            actual = Long.parseLong(record[0]);
        }
        assertEquals(expectedValue, actual);

        // Also make sure the rounded value is used in 'where' too.
        assertEquals("Query should match", 1, DataUtil.queryById(mResolver, dataId, projection,
                "ifnull(" + Data.TIMES_USED + ",0)=" + expectedValue, null).length);
    }

    private void deleteDataUsage() {
        mResolver.delete(DataUsageFeedback.DELETE_USAGE_URI, null, null);
    }


    public void testUsageUpdate() throws Exception {
        final TestRawContact rawContact = mBuilder.newRawContact().insert().load();
        final TestContact contact = rawContact.getContact().load();

        ContentValues values;

        values = new ContentValues();
        values.put(Contacts.LAST_TIME_CONTACTED, 123);
        assertEquals(1, ContactUtil.update(mResolver, contact.getId(), values));

        values = new ContentValues();
        values.put(Contacts.LAST_TIME_CONTACTED, 123);
        assertEquals(1, RawContactUtil.update(mResolver, rawContact.getId(), values));

        values = new ContentValues();
        values.put(Contacts.TIMES_CONTACTED, 456);
        assertEquals(1, ContactUtil.update(mResolver, contact.getId(), values));

        values = new ContentValues();
        values.put(Contacts.TIMES_CONTACTED, 456);
        assertEquals(1, RawContactUtil.update(mResolver, rawContact.getId(), values));


        values = new ContentValues();
        values.put(Contacts.LAST_TIME_CONTACTED, 123);
        values.put(Contacts.TIMES_CONTACTED, 456);
        assertEquals(1, ContactUtil.update(mResolver, contact.getId(), values));

        values = new ContentValues();
        values.put(Contacts.LAST_TIME_CONTACTED, 123);
        values.put(Contacts.TIMES_CONTACTED, 456);
        assertEquals(1, RawContactUtil.update(mResolver, rawContact.getId(), values));

        contact.load();
        rawContact.load();

        assertEquals(0, contact.getLong(Contacts.LAST_TIME_CONTACTED));
        assertEquals(0, contact.getLong(Contacts.TIMES_CONTACTED));

        assertEquals(0, rawContact.getLong(Contacts.LAST_TIME_CONTACTED));
        assertEquals(0, rawContact.getLong(Contacts.TIMES_CONTACTED));
    }
}
