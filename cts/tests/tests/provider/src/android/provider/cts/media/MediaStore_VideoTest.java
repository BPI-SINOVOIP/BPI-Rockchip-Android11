/*
 * Copyright (C) 2009 The Android Open Source Project
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

import static android.provider.cts.media.MediaStoreTest.TAG;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;

import android.content.ContentResolver;
import android.content.ContentValues;
import android.content.Context;
import android.database.Cursor;
import android.net.Uri;
import android.provider.MediaStore;
import android.provider.MediaStore.Video;
import android.provider.MediaStore.Video.VideoColumns;
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

import java.io.File;

@RunWith(Parameterized.class)
public class MediaStore_VideoTest {
    private Context mContext;
    private ContentResolver mResolver;

    private Uri mExternalVideo;

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
        mExternalVideo = MediaStore.Video.Media.getContentUri(mVolumeName);
    }

    @Test
    public void testQuery() throws Exception {
        ContentValues values = new ContentValues();

        final File file = new File(ProviderTestUtils.stageDir(mVolumeName),
                "testVideo" + System.nanoTime() + ".3gp");
        final String valueOfData = file.getAbsolutePath();
        ProviderTestUtils.stageFile(R.raw.testvideo, file);

        values.put(VideoColumns.DATA, valueOfData);

        Uri newUri = mResolver.insert(mExternalVideo, values);
        assertNotNull(newUri);

        Cursor c = Video.query(mResolver, newUri, new String[] { VideoColumns.DATA });
        assertEquals(1, c.getCount());
        c.moveToFirst();
        assertEquals(valueOfData, c.getString(c.getColumnIndex(VideoColumns.DATA)));
        c.close();
    }
}
