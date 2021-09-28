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

package com.android.car.settings.search;

import static android.provider.SearchIndexablesContract.COLUMN_INDEX_NON_INDEXABLE_KEYS_KEY_VALUE;
import static android.provider.SearchIndexablesContract.COLUMN_INDEX_RAW_CLASS_NAME;
import static android.provider.SearchIndexablesContract.COLUMN_INDEX_RAW_ENTRIES;
import static android.provider.SearchIndexablesContract.COLUMN_INDEX_RAW_ICON_RESID;
import static android.provider.SearchIndexablesContract.COLUMN_INDEX_RAW_INTENT_ACTION;
import static android.provider.SearchIndexablesContract.COLUMN_INDEX_RAW_INTENT_TARGET_CLASS;
import static android.provider.SearchIndexablesContract.COLUMN_INDEX_RAW_INTENT_TARGET_PACKAGE;
import static android.provider.SearchIndexablesContract.COLUMN_INDEX_RAW_KEY;
import static android.provider.SearchIndexablesContract.COLUMN_INDEX_RAW_KEYWORDS;
import static android.provider.SearchIndexablesContract.COLUMN_INDEX_RAW_SCREEN_TITLE;
import static android.provider.SearchIndexablesContract.COLUMN_INDEX_RAW_SUMMARY_OFF;
import static android.provider.SearchIndexablesContract.COLUMN_INDEX_RAW_SUMMARY_ON;
import static android.provider.SearchIndexablesContract.COLUMN_INDEX_RAW_TITLE;
import static android.provider.SearchIndexablesContract.COLUMN_INDEX_RAW_USER_ID;
import static android.provider.SearchIndexablesContract.COLUMN_INDEX_XML_RES_CLASS_NAME;
import static android.provider.SearchIndexablesContract.COLUMN_INDEX_XML_RES_ICON_RESID;
import static android.provider.SearchIndexablesContract.COLUMN_INDEX_XML_RES_INTENT_ACTION;
import static android.provider.SearchIndexablesContract.COLUMN_INDEX_XML_RES_INTENT_TARGET_CLASS;
import static android.provider.SearchIndexablesContract.COLUMN_INDEX_XML_RES_INTENT_TARGET_PACKAGE;
import static android.provider.SearchIndexablesContract.COLUMN_INDEX_XML_RES_RANK;
import static android.provider.SearchIndexablesContract.COLUMN_INDEX_XML_RES_RESID;

import static com.google.common.truth.Truth.assertThat;

import android.content.Context;
import android.database.Cursor;
import android.provider.SearchIndexableResource;

import androidx.test.core.app.ApplicationProvider;
import androidx.test.ext.junit.runners.AndroidJUnit4;

import com.android.settingslib.search.Indexable;
import com.android.settingslib.search.SearchIndexableData;
import com.android.settingslib.search.SearchIndexableRaw;
import com.android.settingslib.search.SearchIndexableResources;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import java.util.Collection;
import java.util.Collections;
import java.util.HashSet;
import java.util.List;
import java.util.Set;

@RunWith(AndroidJUnit4.class)
public class CarSettingsSearchIndexablesProviderTest {
    private final Context mContext = ApplicationProvider.getApplicationContext();

    private CarSettingsSearchIndexablesProvider mProvider;
    private SearchIndexableResources mSearchIndexableResources;

    @Before
    public void setup() {
        mSearchIndexableResources = new SearchIndexableResources() {
            Set<SearchIndexableData> mValues = new HashSet<>();

            @Override
            public Collection<SearchIndexableData> getProviderValues() {
                return mValues;
            }

            @Override
            public void addIndex(SearchIndexableData indexBundle) {
                mValues.add(indexBundle);
            }
        };

        mProvider = new CarSettingsSearchIndexablesProvider();
        mProvider.setResources(mSearchIndexableResources);
    }

    @Test
    public void queryXmlResources_onEmptyResources_returnsEmptyList() {
        assertThat(mProvider.queryXmlResources(null).getCount()).isEqualTo(0);
    }

    @Test
    public void queryXmlResources_fillsColumns() {
        SearchIndexableResource res = makeResource(/* seed= */ 50);

        TestSearchIndexProvider provider = new TestSearchIndexProvider();
        provider.setResources(Collections.singletonList(res));
        mSearchIndexableResources.addIndex(
                new SearchIndexableData(TestSearchIndexProvider.class,
                        provider));

        Cursor c = mProvider.queryXmlResources(null);

        assertThat(c.getCount()).isEqualTo(1);

        c.moveToFirst();

        assertEqual(c, res);
    }

    @Test
    public void queryXmlResources_returnsAllRows() {
        List<SearchIndexableResource> resources = List.of(makeResource(/* seed= */ 1),
                makeResource(/* seed= */ 2),
                makeResource(/* seed= */ 3));

        TestSearchIndexProvider provider = new TestSearchIndexProvider();
        provider.setResources(resources);

        mSearchIndexableResources.addIndex(
                new SearchIndexableData(TestSearchIndexProvider.class,
                        provider));

        Cursor c = mProvider.queryXmlResources(null);

        assertThat(c.getCount()).isEqualTo(resources.size());

        int i = 0;
        while (c.moveToNext()) {
            assertEqual(c, resources.get(i++));
        }
    }

    @Test
    public void queryXmlResources_onPartiallyFilledFields_returnsPartialRows() {
        SearchIndexableResource res = makeResource(/* seed= */ 100);
        res.intentTargetPackage = null;
        TestSearchIndexProvider provider = new TestSearchIndexProvider();
        provider.setResources(Collections.singletonList(res));

        mSearchIndexableResources.addIndex(
                new SearchIndexableData(TestSearchIndexProvider.class,
                        provider));


        Cursor c = mProvider.queryXmlResources(null);

        assertThat(c.getCount()).isEqualTo(1);

        c.moveToFirst();

        assertEqual(c, res);
    }

    @Test
    public void queryXmlResources_onNullResources_returnsEmptyList() {
        mSearchIndexableResources.addIndex(
                new SearchIndexableData(TestSearchIndexProvider.class,
                        new TestSearchIndexProvider()));

        assertThat(mProvider.queryXmlResources(null).getCount()).isEqualTo(0);
    }

    @Test
    public void queryRawData_fillsColumns() {
        SearchIndexableRaw rawData = makeRawData(/* seed= */ 99);

        TestSearchIndexProvider provider = new TestSearchIndexProvider();
        provider.setRawData(Collections.singletonList(rawData));

        mSearchIndexableResources.addIndex(
                new SearchIndexableData(TestSearchIndexProvider.class, provider));

        Cursor c = mProvider.queryRawData(null);

        assertThat(c.getCount()).isEqualTo(1);

        c.moveToFirst();

        assertEqual(c, rawData);
    }

    @Test
    public void queryRawData_returnsAllRows() {
        List<SearchIndexableRaw> rawData = List.of(makeRawData(/* seed= */ 1),
                makeRawData(/* seed= */ 2), makeRawData(/* seed= */ 3));

        TestSearchIndexProvider provider = new TestSearchIndexProvider();
        provider.setRawData(rawData);

        mSearchIndexableResources.addIndex(
                new SearchIndexableData(TestSearchIndexProvider.class, provider));

        Cursor c = mProvider.queryRawData(null);

        assertThat(c.getCount()).isEqualTo(rawData.size());
        int i = 0;

        while (c.moveToNext()) {
            assertEqual(c, rawData.get(i++));
        }
    }

    @Test
    public void queryRawData_onPartiallyFilledFields_returnsPartialRows() {
        SearchIndexableRaw rawData = makeRawData(/* seed= */ 99);
        rawData.className = null;
        rawData.keywords = null;

        TestSearchIndexProvider provider = new TestSearchIndexProvider();
        provider.setRawData(Collections.singletonList(rawData));

        mSearchIndexableResources.addIndex(
                new SearchIndexableData(TestSearchIndexProvider.class, provider));

        Cursor c = mProvider.queryRawData(null);

        assertThat(c.getCount()).isEqualTo(1);

        c.moveToFirst();

        assertEqual(c, rawData);
    }

    @Test
    public void queryRawData_onEmptyResources_returnsEmpty() {
        assertThat(mProvider.queryRawData(null).getCount()).isEqualTo(0);
    }

    @Test
    public void queryRawData_onNullRawDataList_returnsEmptyList() {
        mSearchIndexableResources.addIndex(
                new SearchIndexableData(TestSearchIndexProvider.class,
                        new TestSearchIndexProvider()));

        assertThat(mProvider.queryRawData(null).getCount()).isEqualTo(0);
    }

    @Test
    public void queryNonIndexableKeys_fillsColumns() {
        String key = "key1";

        TestSearchIndexProvider provider = new TestSearchIndexProvider();
        provider.setNonIndexableKeys(Collections.singletonList(key));

        mSearchIndexableResources.addIndex(
                new SearchIndexableData(TestSearchIndexProvider.class, provider));

        Cursor c = mProvider.queryNonIndexableKeys(null);

        assertThat(c.getCount()).isEqualTo(1);

        c.moveToFirst();

        assertThat(c.getString(COLUMN_INDEX_NON_INDEXABLE_KEYS_KEY_VALUE)).isEqualTo(key);
    }

    @Test
    public void queryNonIndexableKeys_returnsAllRows() {
        List<String> keys = List.of("key1", "key2", "key3");

        TestSearchIndexProvider provider = new TestSearchIndexProvider();
        provider.setNonIndexableKeys(keys);

        mSearchIndexableResources.addIndex(
                new SearchIndexableData(TestSearchIndexProvider.class, provider));

        Cursor c = mProvider.queryNonIndexableKeys(null);

        assertThat(c.getCount()).isEqualTo(keys.size());
        int i = 0;

        while (c.moveToNext()) {
            assertThat(c.getString(COLUMN_INDEX_NON_INDEXABLE_KEYS_KEY_VALUE)).isEqualTo(
                    keys.get(i++));
        }
    }

    @Test
    public void queryNonIndexableKeys_onEmptyResources_returnsEmpty() {
        assertThat(mProvider.queryNonIndexableKeys(null).getCount()).isEqualTo(0);
    }

    @Test
    public void queryNonIndexableKeys_onNullNonIndexableKeysList_returnsEmptyList() {
        mSearchIndexableResources.addIndex(
                new SearchIndexableData(TestSearchIndexProvider.class,
                        new TestSearchIndexProvider()));

        assertThat(mProvider.queryNonIndexableKeys(null).getCount()).isEqualTo(0);
    }

    private SearchIndexableResource makeResource(int seed) {
        int rank = seed;
        int resId = seed + 1;
        String className = seed + "className";
        int iconResId = seed + 2;
        String action = seed + "action";
        String intentPackage = seed + "package";
        String intentClass = seed + "class";

        SearchIndexableResource resource = new SearchIndexableResource(rank, resId, className,
                iconResId);
        resource.intentAction = action;
        resource.intentTargetPackage = intentPackage;
        resource.intentTargetClass = intentClass;

        return resource;
    }

    private void assertEqual(Cursor c, SearchIndexableResource res) {
        assertThat(c.getInt(COLUMN_INDEX_XML_RES_RANK)).isEqualTo(res.rank);
        assertThat(c.getInt(COLUMN_INDEX_XML_RES_RESID)).isEqualTo(res.xmlResId);
        assertThat(c.getString(COLUMN_INDEX_XML_RES_CLASS_NAME)).isEqualTo(res.className);
        assertThat(c.getInt(COLUMN_INDEX_XML_RES_ICON_RESID)).isEqualTo(res.iconResId);
        assertThat(c.getString(COLUMN_INDEX_XML_RES_INTENT_ACTION)).isEqualTo(res.intentAction);
        assertThat(c.getString(COLUMN_INDEX_XML_RES_INTENT_TARGET_PACKAGE)).isEqualTo(
                res.intentTargetPackage);
        assertThat(c.getString(COLUMN_INDEX_XML_RES_INTENT_TARGET_CLASS)).isEqualTo(
                res.intentTargetClass);
    }

    private SearchIndexableRaw makeRawData(int seed) {
        SearchIndexableRaw rawData = new SearchIndexableRaw(mContext);
        rawData.title = seed + "title";
        rawData.summaryOn = seed + "summaryOn";
        rawData.summaryOff = seed + "summaryOff";
        rawData.entries = seed + "entries";
        rawData.keywords = seed + "keywords";
        rawData.screenTitle = seed + "screenTitle";
        rawData.className = seed + "className";
        rawData.iconResId = seed;
        rawData.intentAction = seed + "intentAction";
        rawData.intentTargetPackage = seed + "intentTargetPackage";
        rawData.intentTargetClass = seed + "intentTargetClass";
        rawData.key = seed + "key";
        rawData.userId = seed + 1;
        return rawData;
    }

    private void assertEqual(Cursor c, SearchIndexableRaw raw) {
        assertThat(c.getString(COLUMN_INDEX_RAW_TITLE)).isEqualTo(raw.title);
        assertThat(c.getString(COLUMN_INDEX_RAW_SUMMARY_ON)).isEqualTo(raw.summaryOn);
        assertThat(c.getString(COLUMN_INDEX_RAW_SUMMARY_OFF)).isEqualTo(raw.summaryOff);
        assertThat(c.getString(COLUMN_INDEX_RAW_ENTRIES)).isEqualTo(raw.entries);
        assertThat(c.getString(COLUMN_INDEX_RAW_KEYWORDS)).isEqualTo(raw.keywords);
        assertThat(c.getString(COLUMN_INDEX_RAW_SCREEN_TITLE)).isEqualTo(raw.screenTitle);
        assertThat(c.getString(COLUMN_INDEX_RAW_CLASS_NAME)).isEqualTo(raw.className);
        assertThat(c.getInt(COLUMN_INDEX_RAW_ICON_RESID)).isEqualTo(raw.iconResId);
        assertThat(c.getString(COLUMN_INDEX_RAW_INTENT_ACTION)).isEqualTo(raw.intentAction);
        assertThat(c.getString(COLUMN_INDEX_RAW_INTENT_TARGET_PACKAGE)).isEqualTo(
                raw.intentTargetPackage);
        assertThat(c.getString(COLUMN_INDEX_RAW_INTENT_TARGET_CLASS)).isEqualTo(
                raw.intentTargetClass);
        assertThat(c.getString(COLUMN_INDEX_RAW_KEY)).isEqualTo(raw.key);
        assertThat(c.getInt(COLUMN_INDEX_RAW_USER_ID)).isEqualTo(raw.userId);
    }

    private static class TestSearchIndexProvider implements Indexable.SearchIndexProvider {
        private List<SearchIndexableResource> mResources;
        private List<SearchIndexableRaw> mRawData;
        private List<String> mNonIndexableKeys;

        TestSearchIndexProvider() {
        }

        @Override
        public List<SearchIndexableResource> getXmlResourcesToIndex(Context context,
                boolean enabled) {
            return mResources;
        }

        @Override
        public List<SearchIndexableRaw> getRawDataToIndex(Context context, boolean enabled) {
            return mRawData;
        }

        @Override
        public List<SearchIndexableRaw> getDynamicRawDataToIndex(Context context, boolean enabled) {
            return mRawData;
        }

        @Override
        public List<String> getNonIndexableKeys(Context context) {
            return mNonIndexableKeys;
        }

        void setResources(List<SearchIndexableResource> resources) {
            mResources = resources;
        }

        void setRawData(List<SearchIndexableRaw> rawData) {
            mRawData = rawData;
        }

        void setNonIndexableKeys(List<String> nonIndexableKeys) {
            mNonIndexableKeys = nonIndexableKeys;
        }
    }
}
