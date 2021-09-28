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

package android.provider.cts.media;

import static android.provider.cts.media.MediaStoreTest.TAG;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;

import android.content.ContentResolver;
import android.content.ContentValues;
import android.content.Context;
import android.database.Cursor;
import android.net.Uri;
import android.os.Environment;
import android.provider.MediaStore;
import android.provider.MediaStore.MediaColumns;
import android.provider.cts.ProviderTestUtils;
import android.provider.cts.R;
import android.util.Log;

import androidx.test.InstrumentationRegistry;

import org.junit.Assume;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.Parameterized;
import org.junit.runners.Parameterized.Parameter;
import org.junit.runners.Parameterized.Parameters;

import java.io.File;

@RunWith(Parameterized.class)
public class MediaStoreTrashedTest {
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

    /**
     * Verify that we can trash and untrash an item that we own.
     */
    @Test
    public void testOwned() throws Exception {
        Assume.assumeFalse(MediaStore.VOLUME_EXTERNAL.equals(mVolumeName));

        final File dir = ProviderTestUtils.getVolumePath(mVolumeName);
        final File file = ProviderTestUtils.stageFile(R.drawable.scenery,
                Environment.buildPath(dir, "DCIM", System.nanoTime() + ".jpg"));
        final Uri uri = ProviderTestUtils.scanFile(file);
        ProviderTestUtils.setOwner(uri, InstrumentationRegistry.getContext().getPackageName());

        doTrashed(file, uri);
    }

    /**
     * Verify that we can trash and untrash items we don't own.
     */
    @Test
    public void testUnowned() throws Exception {
        Assume.assumeFalse(MediaStore.VOLUME_EXTERNAL.equals(mVolumeName));

        final File dir = ProviderTestUtils.getVolumePath(mVolumeName);
        final File file = ProviderTestUtils.stageFile(R.drawable.scenery,
                Environment.buildPath(dir, "DCIM", System.nanoTime() + ".jpg"));
        final Uri uri = ProviderTestUtils.scanFile(file);
        ProviderTestUtils.clearOwner(uri);

        doTrashed(file, uri);
    }

    /**
     * Verify that we can trash and untrash items in odd locations.
     */
    @Test
    public void testAtypical() throws Exception {
        Assume.assumeFalse(MediaStore.VOLUME_EXTERNAL.equals(mVolumeName));

        final File dir = ProviderTestUtils.getVolumePath(mVolumeName);
        final File file = ProviderTestUtils.stageFile(R.drawable.scenery,
                Environment.buildPath(dir, "NOT_DCIM", System.nanoTime() + ".jpg"));
        final Uri uri = ProviderTestUtils.scanFile(file);
        ProviderTestUtils.clearOwner(uri);

        doTrashed(file, uri);
    }

    private void doTrashed(File file, Uri uri) {
        final ContentValues values = new ContentValues();

        // When trashed, display name needs to remain intact
        values.clear();
        values.put(MediaColumns.IS_TRASHED, 1);
        mResolver.update(uri, values, null);
        try (Cursor c = mResolver.query(uri, null, null, null)) {
            assertTrue(c.moveToFirst());
            assertEquals(1,
                    c.getInt(c.getColumnIndexOrThrow(MediaColumns.IS_TRASHED)));
            assertEquals(file.getName(),
                    c.getString(c.getColumnIndexOrThrow(MediaColumns.DISPLAY_NAME)));
        }

        // When untrashed, we returned back to original file
        values.clear();
        values.put(MediaColumns.IS_TRASHED, 0);
        mResolver.update(uri, values, null);
        try (Cursor c = mResolver.query(uri, null, null, null)) {
            assertTrue(c.moveToFirst());
            assertEquals(0,
                    c.getInt(c.getColumnIndexOrThrow(MediaColumns.IS_TRASHED)));
            assertEquals(file.getName(),
                    c.getString(c.getColumnIndexOrThrow(MediaColumns.DISPLAY_NAME)));
            assertEquals(file.getAbsolutePath(),
                    c.getString(c.getColumnIndexOrThrow(MediaColumns.DATA)));
        }
    }
}
