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

package com.android.cellbroadcastreceiver.unit;

import static com.google.common.truth.Truth.assertThat;

import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import android.database.Cursor;

import com.android.cellbroadcastreceiver.CellBroadcastSearchIndexableProvider;
import com.android.cellbroadcastreceiver.R;

import org.junit.Before;
import org.junit.Test;

public class CellBroadcastSearchIndexableProviderTest extends CellBroadcastTest {
    CellBroadcastSearchIndexableProvider mSearchIndexableProvider;
    @Before
    public void setUp() throws Exception {
        super.setUp(getClass().getSimpleName());
        mSearchIndexableProvider = spy(new CellBroadcastSearchIndexableProvider());
        doReturn(mContext).when(mSearchIndexableProvider).getContextMethod();
        doReturn(false).when(mSearchIndexableProvider).isAutomotive();
        doReturn("testPackageName").when(mContext).getPackageName();
        doReturn(mResources).when(mSearchIndexableProvider).getResourcesMethod();
        doReturn("testString").when(mResources).getString(anyInt());
        doReturn(null).when(mSearchIndexableProvider).queryRawData(null);
    }

    @Test
    public void testQueryXmlResources() {
        Cursor cursor = mSearchIndexableProvider.queryXmlResources(null);
        assertThat(cursor.getCount()).isEqualTo(
                CellBroadcastSearchIndexableProvider.INDEXABLE_RES.length);
    }

    @Test
    public void testQueryRawData() {
        Cursor cursor = mSearchIndexableProvider.queryRawData(new String[]{""});

        verify(mResources, times(2)).getString(R.string.sms_cb_settings);
        verify(mResources, times(2 + CellBroadcastSearchIndexableProvider
                .INDEXABLE_KEYWORDS_RESOURCES.length)).getString(anyInt());
        assertThat(cursor.getCount()).isEqualTo(1);
    }

    @Test
    public void testQueryNonIndexableKeys() {
        doReturn(false).when(mSearchIndexableProvider).isTestAlertsToggleVisible();
        doReturn(false).when(mResources).getBoolean(anyInt());
        Cursor cursor = mSearchIndexableProvider.queryNonIndexableKeys(new String[]{""});

        assertThat(cursor.getCount()).isEqualTo(9);
    }
}
