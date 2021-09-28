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

package com.android.mms.service;

import static com.google.common.truth.Truth.assertThat;

import static org.testng.Assert.assertThrows;

import android.content.ContentProvider;
import android.content.ContentValues;
import android.content.Context;
import android.database.Cursor;
import android.net.Uri;
import android.provider.Telephony;
import android.telephony.data.ApnSetting;

import androidx.test.core.app.ApplicationProvider;

import com.android.mms.service.exception.ApnException;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.RobolectricTestRunner;
import org.robolectric.fakes.RoboCursor;
import org.robolectric.shadows.ShadowContentResolver;

import java.util.Arrays;

@RunWith(RobolectricTestRunner.class)
public final class ApnSettingsTest {

    private Context context;

    @Before
    public void setUp() {
        context = ApplicationProvider.getApplicationContext();
    }

    @Test
    public void load_noApnSettings_throwsApnException() {
        assertThrows(ApnException.class,
                () -> ApnSettings.load(context, "apnName", /* subId= */ 0, "requestId"));
    }

    @Test
    public void getApnSettingsFromCursor_validSettings_correctApnSettingsLoaded() throws Exception {
        createApnSettingsCursor("mmscUrl", "mmsProxy", /* proxyPort= */ "123");
        ApnSettings apnSettings = ApnSettings.load(context, "apnName", /* subId= */ 0, "requestId");

        assertThat(apnSettings.getProxyPort()).isEqualTo(123);
        assertThat(apnSettings.getMmscUrl()).isEqualTo("mmscUrl");
        assertThat(apnSettings.getProxyAddress()).isEqualTo("mmsProxy");
    }

    @Test
    public void getApnSettingsFromCursor_nullMmsPort_defaultProxyPortUsed() throws Exception {
        createApnSettingsCursor("mmscUrl", "mmsProxy", /* proxyPort= */ null);
        ApnSettings apnSettings = ApnSettings.load(context, "apnName", /* subId= */ 0, "requestId");
        assertThat(apnSettings.getProxyPort()).isEqualTo(80);
    }

    @Test
    public void getApnSettingsFromCursor_emptyMmsPort_defaultProxyPortUsed() throws Exception {
        createApnSettingsCursor("mmscUrl", "mmsProxy",
                /* proxyPort= */ "");
        ApnSettings apnSettings = ApnSettings.load(context, "apnName", /* subId= */ 0, "requestId");
        assertThat(apnSettings.getProxyPort()).isEqualTo(80);
    }

    private void createApnSettingsCursor(String mmscUrl, String mmsProxy, String proxyPort) {
        Object[][] apnValues =
                {new Object[]{ApnSetting.TYPE_MMS_STRING, mmscUrl, mmsProxy, proxyPort}};
        RoboCursor cursor = new RoboCursor();
        cursor.setResults(apnValues);
        cursor.setColumnNames(Arrays.asList(Telephony.Carriers.TYPE, Telephony.Carriers.MMSC,
                Telephony.Carriers.MMSPROXY, Telephony.Carriers.MMSPORT));

        ShadowContentResolver.registerProviderInternal(
                Telephony.Carriers.CONTENT_URI.getAuthority(), new FakeApnSettingsProvider(cursor));
    }

    private final class FakeApnSettingsProvider extends ContentProvider {

        private final Cursor cursor;

        FakeApnSettingsProvider(Cursor cursor) {
            this.cursor = cursor;
        }

        @Override
        public boolean onCreate() {
            return false;
        }

        @Override
        public Cursor query(Uri uri, String[] projection,
                String selection, String[] selectionArgs, String sortOrder) {
            return cursor;
        }

        @Override
        public String getType(Uri uri) {
            return null;
        }

        @Override
        public Uri insert(Uri uri, ContentValues values) {
            return null;
        }

        @Override
        public int delete(Uri uri, String selection, String[] selectionArgs) {
            return 0;
        }

        @Override
        public int update(Uri uri, ContentValues values, String selection, String[] selectionArgs) {
            return 0;
        }
    }

}
