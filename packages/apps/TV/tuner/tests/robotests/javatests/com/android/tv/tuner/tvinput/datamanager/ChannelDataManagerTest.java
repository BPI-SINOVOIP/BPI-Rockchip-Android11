/*
 * Copyright (C) 2015 The Android Open Source Project
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

package com.android.tv.tuner.tvinput.datamanager;

import static com.google.common.truth.Truth.assertThat;

import android.content.ContentValues;
import android.content.pm.ProviderInfo;
import android.media.tv.TvContract;

import com.android.tv.testing.TestSingletonApp;
import com.android.tv.testing.constants.ConfigConstants;
import com.android.tv.testing.fakes.FakeTvProvider;
import com.android.tv.tuner.data.Channel;
import com.android.tv.tuner.data.TunerChannel;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.Robolectric;
import org.robolectric.RobolectricTestRunner;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowContentResolver;
import org.robolectric.shadows.ShadowContextWrapper;

/** Tests for {@link com.android.tv.tuner.tvinput.datamanager.ChannelDataManager}. */
@RunWith(RobolectricTestRunner.class)
@Config(sdk = ConfigConstants.SDK, application = TestSingletonApp.class)
public class ChannelDataManagerTest {

    private ChannelDataManager mChannelDataManager;

    @Before
    public void setup() {
        ProviderInfo info = new ProviderInfo();
        info.authority = TvContract.AUTHORITY;
        FakeTvProvider provider =
                Robolectric.buildContentProvider(FakeTvProvider.class).create(info).get();
        provider.setCallingPackage("com.android.tv");
        provider.onCreate();
        ShadowContextWrapper shadowContextWrapper = new ShadowContextWrapper();
        shadowContextWrapper.grantPermissions(android.Manifest.permission.MODIFY_PARENTAL_CONTROLS);
        ShadowContentResolver.registerProviderInternal(TvContract.AUTHORITY, provider);
        provider.delete(TvContract.Channels.CONTENT_URI, null, null);
        ContentValues contentValues = new ContentValues();
        contentValues.put(TvContract.Channels.COLUMN_INPUT_ID, "com.android.tv");
        contentValues.put(
                TvContract.Channels.COLUMN_INTERNAL_PROVIDER_DATA,
                Channel.TunerChannelProto.getDefaultInstance().toByteArray());
        contentValues.put(TvContract.Channels.COLUMN_LOCKED, 0);
        provider.insert(TvContract.Channels.CONTENT_URI, contentValues);
        contentValues.put(TvContract.Channels.COLUMN_LOCKED, 1);
        provider.insert(TvContract.Channels.CONTENT_URI, contentValues);

        mChannelDataManager = new ChannelDataManager(RuntimeEnvironment.application, "testInput");
    }

    @After
    public void tearDown() {
        mChannelDataManager.releaseSafely();
    }

    @Test
    public void getChannel_locked() {
        TunerChannel tunerChannel = mChannelDataManager.getChannel(2L);
        assertThat(tunerChannel.isLocked()).isTrue();
    }

    @Test
    public void getChannel_unlocked() {
        TunerChannel tunerChannel = mChannelDataManager.getChannel(1L);
        assertThat(tunerChannel.isLocked()).isFalse();
    }
}
