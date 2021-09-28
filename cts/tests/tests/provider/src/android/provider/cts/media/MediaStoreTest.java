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

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;
import static org.junit.Assert.fail;

import android.content.ContentResolver;
import android.content.Context;
import android.content.pm.PackageInfo;
import android.content.pm.PackageManager;
import android.content.pm.ProviderInfo;
import android.database.Cursor;
import android.net.Uri;
import android.os.Build;
import android.os.storage.StorageManager;
import android.os.storage.StorageVolume;
import android.provider.BaseColumns;
import android.provider.MediaStore;
import android.provider.MediaStore.MediaColumns;
import android.provider.cts.ProviderTestUtils;
import android.provider.cts.R;
import android.util.Log;

import androidx.test.InstrumentationRegistry;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.Parameterized;
import org.junit.runners.Parameterized.Parameter;
import org.junit.runners.Parameterized.Parameters;

import java.util.Set;

@RunWith(Parameterized.class)
public class MediaStoreTest {
    static final String TAG = "MediaStoreTest";

    private static final long SIZE_DELTA = 32_000;

    private Context mContext;
    private ContentResolver mContentResolver;

    private Uri mExternalImages;

    @Parameter(0)
    public String mVolumeName;

    @Parameters
    public static Iterable<? extends Object> data() {
        return ProviderTestUtils.getSharedVolumeNames();
    }

    private Context getContext() {
        return InstrumentationRegistry.getTargetContext();
    }

    @Before
    public void setUp() throws Exception {
        mContext = InstrumentationRegistry.getTargetContext();
        mContentResolver = mContext.getContentResolver();

        Log.d(TAG, "Using volume " + mVolumeName);
        mExternalImages = MediaStore.Images.Media.getContentUri(mVolumeName);
    }

    @After
    public void tearDown() throws Exception {
        InstrumentationRegistry.getInstrumentation().getUiAutomation()
                .dropShellPermissionIdentity();
    }

    /**
     * Sure this is pointless, but czars demand test coverage.
     */
    @Test
    public void testConstructors() {
        new MediaStore();
        new MediaStore.Audio();
        new MediaStore.Audio.Albums();
        new MediaStore.Audio.Artists();
        new MediaStore.Audio.Artists.Albums();
        new MediaStore.Audio.Genres();
        new MediaStore.Audio.Genres.Members();
        new MediaStore.Audio.Media();
        new MediaStore.Audio.Playlists();
        new MediaStore.Audio.Playlists.Members();
        new MediaStore.Files();
        new MediaStore.Images();
        new MediaStore.Images.Media();
        new MediaStore.Images.Thumbnails();
        new MediaStore.Video();
        new MediaStore.Video.Media();
        new MediaStore.Video.Thumbnails();
    }

    @Test
    public void testRequireOriginal() {
        assertFalse(MediaStore.getRequireOriginal(mExternalImages));
        assertTrue(MediaStore.getRequireOriginal(MediaStore.setRequireOriginal(mExternalImages)));
    }

    @Test
    public void testGetMediaScannerUri() {
        // query
        Cursor c = mContentResolver.query(MediaStore.getMediaScannerUri(), null,
                null, null, null);
        assertEquals(1, c.getCount());
        c.close();
    }

    @Test
    public void testGetVersion() {
        // We should have valid versions to help detect data wipes
        assertNotNull(MediaStore.getVersion(getContext()));
        assertNotNull(MediaStore.getVersion(getContext(), MediaStore.VOLUME_INTERNAL));
        assertNotNull(MediaStore.getVersion(getContext(), MediaStore.VOLUME_EXTERNAL));
        assertNotNull(MediaStore.getVersion(getContext(), MediaStore.VOLUME_EXTERNAL_PRIMARY));
    }

    @Test
    public void testGetExternalVolumeNames() {
        Set<String> volumeNames = MediaStore.getExternalVolumeNames(getContext());

        assertFalse(volumeNames.contains(MediaStore.VOLUME_INTERNAL));
        assertFalse(volumeNames.contains(MediaStore.VOLUME_EXTERNAL));
        assertTrue(volumeNames.contains(MediaStore.VOLUME_EXTERNAL_PRIMARY));
    }

    @Test
    public void testGetRecentExternalVolumeNames() {
        Set<String> volumeNames = MediaStore.getRecentExternalVolumeNames(getContext());

        assertFalse(volumeNames.contains(MediaStore.VOLUME_INTERNAL));
        assertFalse(volumeNames.contains(MediaStore.VOLUME_EXTERNAL));
        assertTrue(volumeNames.contains(MediaStore.VOLUME_EXTERNAL_PRIMARY));
    }

    @Test
    public void testGetStorageVolume() throws Exception {
        final Uri uri = ProviderTestUtils.stageMedia(R.raw.volantis, mExternalImages);

        final StorageManager sm = mContext.getSystemService(StorageManager.class);
        final StorageVolume sv = sm.getStorageVolume(uri);

        // We should always have a volume for media we just created
        assertNotNull(sv);

        if (MediaStore.VOLUME_EXTERNAL_PRIMARY.equals(mVolumeName)) {
            assertEquals(sm.getPrimaryStorageVolume(), sv);
        }
    }

    @Test
    public void testGetStorageVolume_Unrelated() throws Exception {
        final StorageManager sm = mContext.getSystemService(StorageManager.class);
        try {
            sm.getStorageVolume(Uri.parse("content://com.example/path/to/item/"));
            fail("getStorageVolume unrelated should throw exception");
        } catch (IllegalArgumentException expected) {
        }
    }

    @Test
    public void testRewriteToLegacy() throws Exception {
        final Uri before = MediaStore.Images.Media
                .getContentUri(MediaStore.VOLUME_EXTERNAL_PRIMARY);
        final Uri after = MediaStore.rewriteToLegacy(before);

        assertEquals(MediaStore.AUTHORITY, before.getAuthority());
        assertEquals(MediaStore.AUTHORITY_LEGACY, after.getAuthority());
    }

    /**
     * When upgrading from an older device, we really need our legacy provider
     * to be present to ensure that we don't lose user data like
     * {@link BaseColumns#_ID} and {@link MediaColumns#IS_FAVORITE}.
     */
    @Test
    public void testLegacy() throws Exception {
        final ProviderInfo legacy = getContext().getPackageManager()
                .resolveContentProvider(MediaStore.AUTHORITY_LEGACY, 0);
        if (legacy == null) {
            if (Build.VERSION.FIRST_SDK_INT >= Build.VERSION_CODES.R) {
                // If we're a brand new device, we don't require a legacy
                // provider, since there's nothing to upgrade
                return;
            } else {
                fail("Upgrading devices must have a legacy MediaProvider at "
                        + "MediaStore.AUTHORITY_LEGACY to upgrade user data from");
            }
        }

        // Verify that legacy provider is protected
        assertEquals("Legacy provider at MediaStore.AUTHORITY_LEGACY must protect its data",
                android.Manifest.permission.WRITE_MEDIA_STORAGE, legacy.readPermission);
        assertEquals("Legacy provider at MediaStore.AUTHORITY_LEGACY must protect its data",
                android.Manifest.permission.WRITE_MEDIA_STORAGE, legacy.writePermission);

        // And finally verify that legacy provider is headless
        final PackageInfo legacyPackage = getContext().getPackageManager().getPackageInfo(
                legacy.packageName, PackageManager.GET_ACTIVITIES | PackageManager.GET_PROVIDERS
                        | PackageManager.GET_RECEIVERS | PackageManager.GET_SERVICES);
        assertEmpty("Headless legacy MediaProvider must have no activities",
                legacyPackage.activities);
        assertEquals("Headless legacy MediaProvider must have exactly one provider",
                1, legacyPackage.providers.length);
        assertEmpty("Headless legacy MediaProvider must have no receivers",
                legacyPackage.receivers);
        assertEmpty("Headless legacy MediaProvider must have no services",
                legacyPackage.services);
    }

    private static <T> void assertEmpty(String message, T[] array) {
        if (array != null && array.length > 0) {
            fail(message);
        }
    }
}
