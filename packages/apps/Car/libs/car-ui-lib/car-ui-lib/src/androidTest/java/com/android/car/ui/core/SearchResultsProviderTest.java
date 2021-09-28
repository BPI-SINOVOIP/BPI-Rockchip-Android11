/*
 * Copyright (C) 2020 The Android Open Source Project
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

package com.android.car.ui.core;

import static com.android.car.ui.core.SearchResultsProvider.CONTENT;
import static com.android.car.ui.core.SearchResultsProvider.ITEM_ID;
import static com.android.car.ui.core.SearchResultsProvider.PRIMARY_IMAGE_BLOB;
import static com.android.car.ui.core.SearchResultsProvider.SEARCH_RESULTS_PROVIDER;
import static com.android.car.ui.core.SearchResultsProvider.SECONDARY_IMAGE_BLOB;
import static com.android.car.ui.core.SearchResultsProvider.SECONDARY_IMAGE_ID;
import static com.android.car.ui.core.SearchResultsProvider.SUBTITLE;
import static com.android.car.ui.core.SearchResultsProvider.TITLE;

import android.content.ContentUris;
import android.content.ContentValues;
import android.content.Context;
import android.database.Cursor;
import android.graphics.Bitmap;
import android.graphics.drawable.BitmapDrawable;
import android.net.Uri;
import android.os.Parcel;
import android.test.ProviderTestCase2;

import androidx.test.core.app.ApplicationProvider;
import androidx.test.ext.junit.runners.AndroidJUnit4;

import com.android.car.ui.test.R;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

/**
 * Unit tests for {@link SearchResultsProvider}.
 */
@RunWith(AndroidJUnit4.class)
public class SearchResultsProviderTest extends ProviderTestCase2<SearchResultsProvider> {

    public static final String AUTHORITY =
            CONTENT + "com.android.car.ui.test" + SEARCH_RESULTS_PROVIDER;

    private final Context mContext = ApplicationProvider.getApplicationContext();
    private ResolverRenamingMockContext mProviderContext;
    private Class<SearchResultsProvider> mProviderClass;
    private SearchResultsProvider mProvider;

    public SearchResultsProviderTest() {
        super(SearchResultsProvider.class, AUTHORITY);
        setName("ProviderSampleTests");
        mProviderClass = SearchResultsProvider.class;
    }

    @Before
    @Override
    public void setUp() throws Exception {
        mProviderContext = new ResolverRenamingMockContext(getContext());
        mProvider = mProviderClass.newInstance();
        assertNotNull(mProvider);
        mProvider.attachInfo(mProviderContext, null);
        mProviderContext.addProvider(AUTHORITY, mProvider);
    }

    @Override
    protected void tearDown() throws Exception {
        super.tearDown();
    }

    @Override
    public SearchResultsProvider getProvider() {
        return mProvider;
    }

    @Test
    public void insert_shouldInsertTheSearchResult() {
        ContentValues values = getRecord();

        SearchResultsProvider provider = getProvider();

        Uri uri = provider.insert(Uri.parse(AUTHORITY), values);
        // length - 1
        assertEquals(0L, ContentUris.parseId(uri));
    }

    @Test
    public void query_shouldHaveValidData() {
        ContentValues values = getRecord();

        SearchResultsProvider provider = getProvider();
        provider.insert(Uri.parse(AUTHORITY), values);
        Cursor cursor = provider.query(Uri.parse(AUTHORITY), null, null, null, null);

        assertNotNull(cursor);
        assertEquals(1, cursor.getCount());
        assertTrue(cursor.moveToFirst());
        assertEquals("1", cursor.getString(cursor.getColumnIndex(ITEM_ID)));
        assertEquals("1", cursor.getString(cursor.getColumnIndex(SECONDARY_IMAGE_ID)));
        assertNotNull(cursor.getBlob(cursor.getColumnIndex(PRIMARY_IMAGE_BLOB)));
        assertNotNull(cursor.getBlob(cursor.getColumnIndex(SECONDARY_IMAGE_BLOB)));
        assertEquals("Title", cursor.getString(cursor.getColumnIndex(TITLE)));
        assertEquals("SubTitle", cursor.getString(cursor.getColumnIndex(SUBTITLE)));
    }

    private byte[] bitmapToByteArray(Bitmap bitmap) {
        Parcel parcel = Parcel.obtain();
        bitmap.writeToParcel(parcel, 0);
        byte[] bytes = parcel.marshall();
        parcel.recycle();
        return bytes;
    }

    private ContentValues getRecord() {
        ContentValues values = new ContentValues();
        int id = 1;
        values.put(ITEM_ID, id);
        values.put(SECONDARY_IMAGE_ID, id);
        BitmapDrawable icon = (BitmapDrawable) mContext.getDrawable(R.drawable.ic_launcher);
        values.put(PRIMARY_IMAGE_BLOB,
                icon != null ? bitmapToByteArray(icon.getBitmap()) : null);
        values.put(SECONDARY_IMAGE_BLOB,
                icon != null ? bitmapToByteArray(icon.getBitmap()) : null);
        values.put(TITLE, "Title");
        values.put(SUBTITLE, "SubTitle");
        return values;
    }
}
