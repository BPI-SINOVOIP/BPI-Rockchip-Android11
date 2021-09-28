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

package com.android.car.settings.storage;

import static com.google.common.truth.Truth.assertThat;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.when;

import android.content.Context;
import android.os.storage.VolumeInfo;

import com.android.settingslib.deviceinfo.PrivateStorageInfo;
import com.android.settingslib.deviceinfo.StorageVolumeProvider;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.RobolectricTestRunner;
import org.robolectric.RuntimeEnvironment;

/** Unit test for {@link VolumeSizesLoader}. */
@RunWith(RobolectricTestRunner.class)
public class VolumeSizesLoaderTest {

    private Context mContext;

    @Before
    public void setUp() throws Exception {
        mContext = RuntimeEnvironment.application;
    }

    @Test
    public void getVolumeSize_getsValidSizes() throws Exception {
        VolumeInfo info = mock(VolumeInfo.class);
        StorageVolumeProvider storageVolumeProvider = mock(StorageVolumeProvider.class);
        when(storageVolumeProvider.getTotalBytes(any(), any())).thenReturn(10000L);
        when(storageVolumeProvider.getFreeBytes(any(), any())).thenReturn(1000L);

        PrivateStorageInfo storageInfo = new VolumeSizesLoader(mContext, storageVolumeProvider,
                null, info).loadInBackground();

        assertThat(storageInfo.freeBytes).isEqualTo(1000L);
        assertThat(storageInfo.totalBytes).isEqualTo(10000L);
    }
}
