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

package android.provider.cts.media;

import static android.provider.cts.ProviderTestUtils.containsId;
import static android.provider.cts.media.MediaStoreTest.TAG;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import android.content.ContentResolver;
import android.content.ContentUris;
import android.content.ContentValues;
import android.content.Context;
import android.net.Uri;
import android.os.Bundle;
import android.provider.MediaStore;
import android.provider.MediaStore.MediaColumns;
import android.provider.cts.ProviderTestUtils;
import android.provider.cts.R;
import android.util.Log;

import androidx.test.InstrumentationRegistry;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.Parameterized;
import org.junit.runners.Parameterized.Parameter;
import org.junit.runners.Parameterized.Parameters;

@RunWith(Parameterized.class)
public class MediaStoreMatchTest {
    private Context mContext;
    private ContentResolver mResolver;

    private Uri mExternalImages;

    @Parameter(0)
    public String mVolumeName;

    @Parameters
    public static Iterable<? extends Object> data() {
        return ProviderTestUtils.getSharedVolumeNames();
    }

    @Before
    public void setUp() throws Exception {
        mContext = InstrumentationRegistry.getTargetContext();
        mResolver = mContext.getContentResolver();

        Log.d(TAG, "Using volume " + mVolumeName);
        mExternalImages = MediaStore.Images.Media.getContentUri(mVolumeName);
    }

    @Test
    public void testMatch_Pending() throws Exception {
        verifyMatch(MediaColumns.IS_PENDING, MediaStore.QUERY_ARG_MATCH_PENDING);
    }

    @Test
    public void testMatch_Trashed() throws Exception {
        verifyMatch(MediaColumns.IS_TRASHED, MediaStore.QUERY_ARG_MATCH_TRASHED);
    }

    @Test
    public void testMatch_Favorite() throws Exception {
        verifyMatch(MediaColumns.IS_FAVORITE, MediaStore.QUERY_ARG_MATCH_FAVORITE);
    }

    private void verifyMatch(String columnName, String queryArg) throws Exception {
        final Uri pos = ProviderTestUtils.stageMedia(R.raw.scenery, mExternalImages);
        final Uri neg = ProviderTestUtils.stageMedia(R.raw.scenery, mExternalImages);

        final ContentValues values = new ContentValues();
        values.put(columnName, 1);
        mResolver.update(pos, values, null);
        values.put(columnName, 0);
        mResolver.update(neg, values, null);

        final long posId = ContentUris.parseId(pos);
        final long negId = ContentUris.parseId(neg);

        final Bundle extras = new Bundle();
        extras.putInt(queryArg, MediaStore.MATCH_INCLUDE);
        assertTrue(containsId(mExternalImages, extras, posId));
        assertTrue(containsId(mExternalImages, extras, negId));

        extras.putInt(queryArg, MediaStore.MATCH_EXCLUDE);
        assertFalse(containsId(mExternalImages, extras, posId));
        assertTrue(containsId(mExternalImages, extras, negId));

        extras.putInt(queryArg, MediaStore.MATCH_ONLY);
        assertTrue(containsId(mExternalImages, extras, posId));
        assertFalse(containsId(mExternalImages, extras, negId));
    }
}
