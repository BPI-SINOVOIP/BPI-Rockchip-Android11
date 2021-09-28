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
import static android.provider.SearchIndexablesContract.INDEXABLES_RAW_COLUMNS;
import static android.provider.SearchIndexablesContract.INDEXABLES_XML_RES_COLUMNS;
import static android.provider.SearchIndexablesContract.NON_INDEXABLES_KEYS_COLUMNS;

import android.database.Cursor;
import android.database.MatrixCursor;
import android.provider.SearchIndexablesProvider;

import com.android.car.settings.common.Logger;
import com.android.internal.annotations.VisibleForTesting;
import com.android.settingslib.search.SearchIndexableData;
import com.android.settingslib.search.SearchIndexableRaw;
import com.android.settingslib.search.SearchIndexableResources;
import com.android.settingslib.search.SearchIndexableResourcesAuto;

import java.util.Objects;

/**
 * Automotive Settings Provider for Search.
 */
public class CarSettingsSearchIndexablesProvider extends SearchIndexablesProvider {
    private static final Logger LOG = new Logger(CarSettingsSearchIndexablesProvider.class);

    private SearchIndexableResources mSearchIndexableResources;

    @Override
    public Cursor queryXmlResources(String[] projection) {
        MatrixCursor cursor = new MatrixCursor(INDEXABLES_XML_RES_COLUMNS);

        getResources()
                .getProviderValues()
                .stream()
                .map(SearchIndexableData::getSearchIndexProvider)
                .map(i -> i.getXmlResourcesToIndex(getContext(), true))
                .filter(Objects::nonNull)
                .forEach(list -> list.forEach(val -> {
                    Object[] ref = new Object[INDEXABLES_XML_RES_COLUMNS.length];
                    ref[COLUMN_INDEX_XML_RES_RANK] = val.rank;
                    ref[COLUMN_INDEX_XML_RES_RESID] = val.xmlResId;
                    ref[COLUMN_INDEX_XML_RES_CLASS_NAME] = val.className;
                    ref[COLUMN_INDEX_XML_RES_ICON_RESID] = val.iconResId;
                    ref[COLUMN_INDEX_XML_RES_INTENT_ACTION] = val.intentAction;
                    ref[COLUMN_INDEX_XML_RES_INTENT_TARGET_PACKAGE] = val.intentTargetPackage;
                    ref[COLUMN_INDEX_XML_RES_INTENT_TARGET_CLASS] = val.intentTargetClass;
                    cursor.addRow(ref);
                }));

        return cursor;
    }

    @Override
    public Cursor queryRawData(String[] projection) {
        MatrixCursor cursor = new MatrixCursor(INDEXABLES_RAW_COLUMNS);

        getResources()
                .getProviderValues()
                .stream()
                .map(SearchIndexableData::getSearchIndexProvider)
                .map(p -> p.getRawDataToIndex(getContext(), true))
                .filter(Objects::nonNull)
                .forEach(list -> list.forEach(
                        raw -> {
                            cursor.addRow(createIndexableRawColumnObjects(raw));
                        }));

        return cursor;
    }

    private static Object[] createIndexableRawColumnObjects(SearchIndexableRaw raw) {
        Object[] ref = new Object[INDEXABLES_RAW_COLUMNS.length];
        ref[COLUMN_INDEX_RAW_TITLE] = raw.title;
        ref[COLUMN_INDEX_RAW_SUMMARY_ON] = raw.summaryOn;
        ref[COLUMN_INDEX_RAW_SUMMARY_OFF] = raw.summaryOff;
        ref[COLUMN_INDEX_RAW_ENTRIES] = raw.entries;
        ref[COLUMN_INDEX_RAW_KEYWORDS] = raw.keywords;
        ref[COLUMN_INDEX_RAW_SCREEN_TITLE] = raw.screenTitle;
        ref[COLUMN_INDEX_RAW_CLASS_NAME] = raw.className;
        ref[COLUMN_INDEX_RAW_ICON_RESID] = raw.iconResId;
        ref[COLUMN_INDEX_RAW_INTENT_ACTION] = raw.intentAction;
        ref[COLUMN_INDEX_RAW_INTENT_TARGET_PACKAGE] = raw.intentTargetPackage;
        ref[COLUMN_INDEX_RAW_INTENT_TARGET_CLASS] = raw.intentTargetClass;
        ref[COLUMN_INDEX_RAW_KEY] = raw.key;
        ref[COLUMN_INDEX_RAW_USER_ID] = raw.userId;
        return ref;
    }

    @Override
    public Cursor queryNonIndexableKeys(String[] projection) {
        MatrixCursor cursor = new MatrixCursor(NON_INDEXABLES_KEYS_COLUMNS);

        getResources().getProviderValues()
                .stream()
                .map(SearchIndexableData::getSearchIndexProvider)
                .map(provider -> {
                    try {
                        return provider.getNonIndexableKeys(getContext());
                    } catch (Exception e) {
                        LOG.w("Could not get keys for provider " + provider.toString());
                        return null;
                    }
                })
                .filter(Objects::nonNull)
                .forEach(keys ->
                        keys.forEach(key -> {
                            Object[] ref = new Object[NON_INDEXABLES_KEYS_COLUMNS.length];
                            ref[COLUMN_INDEX_NON_INDEXABLE_KEYS_KEY_VALUE] = key;
                            cursor.addRow(ref);
                        }));

        return cursor;
    }

    @Override
    public boolean onCreate() {
        return true;
    }

    private SearchIndexableResources getResources() {
        if (mSearchIndexableResources == null) {
            mSearchIndexableResources = new SearchIndexableResourcesAuto();
        }
        return mSearchIndexableResources;
    }

    @VisibleForTesting
    void setResources(SearchIndexableResources resources) {
        mSearchIndexableResources = resources;
    }
}
