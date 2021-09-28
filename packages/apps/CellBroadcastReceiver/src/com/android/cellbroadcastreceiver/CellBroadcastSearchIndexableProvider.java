/*
 * Copyright (C) 2014 The Android Open Source Project
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

package com.android.cellbroadcastreceiver;

import static android.provider.SearchIndexablesContract.COLUMN_INDEX_NON_INDEXABLE_KEYS_KEY_VALUE;
import static android.provider.SearchIndexablesContract.COLUMN_INDEX_RAW_INTENT_ACTION;
import static android.provider.SearchIndexablesContract.COLUMN_INDEX_RAW_INTENT_TARGET_CLASS;
import static android.provider.SearchIndexablesContract.COLUMN_INDEX_RAW_INTENT_TARGET_PACKAGE;
import static android.provider.SearchIndexablesContract.COLUMN_INDEX_RAW_KEY;
import static android.provider.SearchIndexablesContract.COLUMN_INDEX_RAW_KEYWORDS;
import static android.provider.SearchIndexablesContract.COLUMN_INDEX_RAW_SCREEN_TITLE;
import static android.provider.SearchIndexablesContract.COLUMN_INDEX_RAW_TITLE;
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

import android.annotation.Nullable;
import android.content.Context;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.content.res.Resources;
import android.database.Cursor;
import android.database.MatrixCursor;
import android.provider.SearchIndexableResource;
import android.provider.SearchIndexablesProvider;
import android.telephony.SubscriptionManager;
import android.text.TextUtils;

import com.android.internal.annotations.VisibleForTesting;

import java.util.ArrayList;
import java.util.List;

public class CellBroadcastSearchIndexableProvider extends SearchIndexablesProvider {

    @VisibleForTesting
    // Additional keywords for settings search
    public static final int[] INDEXABLE_KEYWORDS_RESOURCES = {
            R.string.etws_earthquake_warning,
            R.string.etws_tsunami_warning,
            R.string.cmas_presidential_level_alert,
            R.string.cmas_required_monthly_test,
            R.string.emergency_alerts_title
    };

    @VisibleForTesting
    public static final SearchIndexableResource[] INDEXABLE_RES = new SearchIndexableResource[] {
            new SearchIndexableResource(1, R.xml.preferences,
                    CellBroadcastSettings.class.getName(),
                    R.mipmap.ic_launcher_cell_broadcast),
    };

    /**
     * this method is to make this class unit-testable, because super.getContext() is a final
     * method and therefore not mockable
     * @return context
     */
    @VisibleForTesting
    public @Nullable Context getContextMethod() {
        return super.getContext();
    }

    /**
     * this method is to make this class unit-testable, because CellBroadcastSettings.getResources()
     * is a static method and cannot be stubbed.
     * @return resources
     */
    @VisibleForTesting
    public Resources getResourcesMethod() {
        return CellBroadcastSettings.getResources(getContextMethod(),
                SubscriptionManager.DEFAULT_SUBSCRIPTION_ID);
    }

    /**
     * this method is to make this class unit-testable, because
     * CellBroadcastSettings.isTestAlertsToggleVisible is a static method and therefore not mockable
     * @return true if test alerts toggle is Visible
     */
    @VisibleForTesting
    public boolean isTestAlertsToggleVisible() {
        return CellBroadcastSettings.isTestAlertsToggleVisible(getContextMethod());
    }

    @Override
    public boolean onCreate() {
        return true;
    }

    @Override
    public Cursor queryXmlResources(String[] projection) {
        if (isAutomotive()) {
            return null;
        }

        MatrixCursor cursor = new MatrixCursor(INDEXABLES_XML_RES_COLUMNS);
        final int count = INDEXABLE_RES.length;
        for (int n = 0; n < count; n++) {
            Object[] ref = new Object[7];
            ref[COLUMN_INDEX_XML_RES_RANK] = INDEXABLE_RES[n].rank;
            ref[COLUMN_INDEX_XML_RES_RESID] = INDEXABLE_RES[n].xmlResId;
            ref[COLUMN_INDEX_XML_RES_CLASS_NAME] = null;
            ref[COLUMN_INDEX_XML_RES_ICON_RESID] = INDEXABLE_RES[n].iconResId;
            ref[COLUMN_INDEX_XML_RES_INTENT_ACTION] = Intent.ACTION_MAIN;
            ref[COLUMN_INDEX_XML_RES_INTENT_TARGET_PACKAGE] = getContextMethod().getPackageName();
            ref[COLUMN_INDEX_XML_RES_INTENT_TARGET_CLASS] = INDEXABLE_RES[n].className;
            cursor.addRow(ref);
        }
        return cursor;
    }

    @Override
    public Cursor queryRawData(String[] projection) {
        if (isAutomotive()) {
            return null;
        }

        MatrixCursor cursor = new MatrixCursor(INDEXABLES_RAW_COLUMNS);
        final Resources res = getResourcesMethod();

        Object[] raw = new Object[INDEXABLES_RAW_COLUMNS.length];
        raw[COLUMN_INDEX_RAW_TITLE] = res.getString(R.string.sms_cb_settings);
        List<String> keywordList = new ArrayList<>();
        for (int keywordRes : INDEXABLE_KEYWORDS_RESOURCES) {
            keywordList.add(res.getString(keywordRes));
        }

        CellBroadcastChannelManager channelManager = new CellBroadcastChannelManager(
                getContextMethod(),
                SubscriptionManager.DEFAULT_SUBSCRIPTION_ID);

        if (!channelManager.getCellBroadcastChannelRanges(
                R.array.public_safety_messages_channels_range_strings).isEmpty()) {
            keywordList.add(res.getString(R.string.public_safety_message));
        }

        if (!channelManager.getCellBroadcastChannelRanges(
                R.array.state_local_test_alert_range_strings).isEmpty()) {
            keywordList.add(res.getString(R.string.state_local_test_alert));
        }

        raw[COLUMN_INDEX_RAW_KEYWORDS] = TextUtils.join(",", keywordList);

        raw[COLUMN_INDEX_RAW_SCREEN_TITLE] = res.getString(R.string.sms_cb_settings);
        raw[COLUMN_INDEX_RAW_KEY] = CellBroadcastSettings.class.getSimpleName();
        raw[COLUMN_INDEX_RAW_INTENT_ACTION] = Intent.ACTION_MAIN;
        raw[COLUMN_INDEX_RAW_INTENT_TARGET_PACKAGE] = getContextMethod().getPackageName();
        raw[COLUMN_INDEX_RAW_INTENT_TARGET_CLASS] = CellBroadcastSettings.class.getName();

        cursor.addRow(raw);
        return cursor;
    }

    @Override
    public Cursor queryNonIndexableKeys(String[] projection) {
        MatrixCursor cursor = new MatrixCursor(NON_INDEXABLES_KEYS_COLUMNS);

        Resources res = getResourcesMethod();
        Object[] ref;

        if (!res.getBoolean(R.bool.show_presidential_alerts_settings)) {
            ref = new Object[1];
            ref[COLUMN_INDEX_NON_INDEXABLE_KEYS_KEY_VALUE] =
                    CellBroadcastSettings.KEY_ENABLE_CMAS_PRESIDENTIAL_ALERTS;
            cursor.addRow(ref);
        }

        if (!res.getBoolean(R.bool.show_extreme_alert_settings)) {
            // Remove CMAS preference items in emergency alert category.
            ref = new Object[1];
            ref[COLUMN_INDEX_NON_INDEXABLE_KEYS_KEY_VALUE] =
                    CellBroadcastSettings.KEY_ENABLE_CMAS_EXTREME_THREAT_ALERTS;
            cursor.addRow(ref);
        }

        if (!res.getBoolean(R.bool.show_severe_alert_settings)) {

            ref = new Object[1];
            ref[COLUMN_INDEX_NON_INDEXABLE_KEYS_KEY_VALUE] =
                    CellBroadcastSettings.KEY_ENABLE_CMAS_SEVERE_THREAT_ALERTS;
            cursor.addRow(ref);
        }

        if (!res.getBoolean(R.bool.show_amber_alert_settings)) {
            ref = new Object[1];
            ref[COLUMN_INDEX_NON_INDEXABLE_KEYS_KEY_VALUE] =
                    CellBroadcastSettings.KEY_ENABLE_CMAS_AMBER_ALERTS;
            cursor.addRow(ref);
        }

        if (!res.getBoolean(R.bool.config_showAreaUpdateInfoSettings)) {
            ref = new Object[1];
            ref[COLUMN_INDEX_NON_INDEXABLE_KEYS_KEY_VALUE] =
                    CellBroadcastSettings.KEY_ENABLE_AREA_UPDATE_INFO_ALERTS;
            cursor.addRow(ref);
        }

        CellBroadcastChannelManager channelManager = new CellBroadcastChannelManager(
                getContextMethod(), SubscriptionManager.DEFAULT_SUBSCRIPTION_ID);
        if (channelManager.getCellBroadcastChannelRanges(
                R.array.cmas_amber_alerts_channels_range_strings).isEmpty()) {
            ref = new Object[1];
            ref[COLUMN_INDEX_NON_INDEXABLE_KEYS_KEY_VALUE] =
                    CellBroadcastSettings.KEY_ENABLE_CMAS_AMBER_ALERTS;
            cursor.addRow(ref);
        }

        if (channelManager.getCellBroadcastChannelRanges(
                R.array.emergency_alerts_channels_range_strings).isEmpty()) {
            ref = new Object[1];
            ref[COLUMN_INDEX_NON_INDEXABLE_KEYS_KEY_VALUE] =
                    CellBroadcastSettings.KEY_ENABLE_EMERGENCY_ALERTS;
            cursor.addRow(ref);
        }

        if (channelManager.getCellBroadcastChannelRanges(
                R.array.public_safety_messages_channels_range_strings).isEmpty()) {
            ref = new Object[1];
            ref[COLUMN_INDEX_NON_INDEXABLE_KEYS_KEY_VALUE] =
                    CellBroadcastSettings.KEY_ENABLE_PUBLIC_SAFETY_MESSAGES;
            cursor.addRow(ref);
        }

        if (channelManager.getCellBroadcastChannelRanges(
                R.array.state_local_test_alert_range_strings).isEmpty()) {
            ref = new Object[1];
            ref[COLUMN_INDEX_NON_INDEXABLE_KEYS_KEY_VALUE] =
                    CellBroadcastSettings.KEY_ENABLE_STATE_LOCAL_TEST_ALERTS;
            cursor.addRow(ref);
        }

        if (!isTestAlertsToggleVisible()) {
            ref = new Object[1];
            ref[COLUMN_INDEX_NON_INDEXABLE_KEYS_KEY_VALUE] =
                CellBroadcastSettings.KEY_ENABLE_TEST_ALERTS;
        }

        if (res.getString(R.string.emergency_alert_second_language_code).isEmpty()) {
            ref = new Object[1];
            ref[COLUMN_INDEX_NON_INDEXABLE_KEYS_KEY_VALUE] =
                    CellBroadcastSettings.KEY_RECEIVE_CMAS_IN_SECOND_LANGUAGE;
            cursor.addRow(ref);
        }

        return cursor;
    }

    /**
     * Whether or not this is an Android Automotive platform.
     * @return true if the current platform is automotive
     */
    @VisibleForTesting
    public boolean isAutomotive() {
        return getContextMethod().getPackageManager().hasSystemFeature(
                PackageManager.FEATURE_AUTOMOTIVE);
    }
}
