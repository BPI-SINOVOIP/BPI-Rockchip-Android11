/*
 * Copyright (C) 2018 The Android Open Source Project
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

package com.android.cts.mediastorageapp;

import static com.android.compatibility.common.util.SystemUtil.runShellCommand;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.junit.Assert.fail;

import android.app.Activity;
import android.app.Instrumentation;
import android.app.PendingIntent;
import android.app.RecoverableSecurityException;
import android.content.ContentResolver;
import android.content.ContentUris;
import android.content.ContentValues;
import android.content.Context;
import android.content.Intent;
import android.database.Cursor;
import android.graphics.Bitmap;
import android.net.Uri;
import android.os.Environment;
import android.os.FileUtils;
import android.os.ParcelFileDescriptor;
import android.provider.MediaStore;
import android.provider.MediaStore.MediaColumns;
import android.support.test.uiautomator.UiDevice;
import android.support.test.uiautomator.UiObject;
import android.support.test.uiautomator.UiSelector;

import androidx.test.InstrumentationRegistry;
import androidx.test.runner.AndroidJUnit4;

import com.android.cts.mediastorageapp.MediaStoreUtils.PendingParams;
import com.android.cts.mediastorageapp.MediaStoreUtils.PendingSession;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import java.io.File;
import java.io.FileNotFoundException;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.util.Arrays;
import java.util.HashSet;
import java.util.concurrent.Callable;

@RunWith(AndroidJUnit4.class)
public class MediaStorageTest {
    private static final File TEST_JPG = Environment.buildPath(
            Environment.getExternalStorageDirectory(),
            Environment.DIRECTORY_DOWNLOADS, "mediastoragetest_file1.jpg");
    private static final File TEST_PDF = Environment.buildPath(
            Environment.getExternalStorageDirectory(),
            Environment.DIRECTORY_DOWNLOADS, "mediastoragetest_file2.pdf");

    private Context mContext;
    private ContentResolver mContentResolver;
    private int mUserId;

    private static int currentAttempt = 0;
    private static final int MAX_NUMBER_OF_ATTEMPT = 10;

    @Before
    public void setUp() throws Exception {
        mContext = InstrumentationRegistry.getTargetContext();
        mContentResolver = mContext.getContentResolver();
        mUserId = mContext.getUserId();
    }

    @Test
    public void testSandboxed() throws Exception {
        doSandboxed(true);
    }

    @Test
    public void testNotSandboxed() throws Exception {
        doSandboxed(false);
    }

    @Test
    public void testStageFiles() throws Exception {
        final File jpg = stageFile(TEST_JPG);
        assertTrue(jpg.exists());
        final File pdf = stageFile(TEST_PDF);
        assertTrue(pdf.exists());
    }

    @Test
    public void testClearFiles() throws Exception {
        TEST_JPG.delete();
        assertNull(MediaStore.scanFile(mContentResolver, TEST_JPG));
        TEST_PDF.delete();
        assertNull(MediaStore.scanFile(mContentResolver, TEST_PDF));
    }

    private void doSandboxed(boolean sandboxed) throws Exception {
        assertEquals(!sandboxed, Environment.isExternalStorageLegacy());

        // We can always see mounted state
        assertEquals(Environment.MEDIA_MOUNTED, Environment.getExternalStorageState());

        // We might have top-level access
        final File probe = new File(Environment.getExternalStorageDirectory(),
                "cts" + System.nanoTime());
        if (!sandboxed) {
            assertTrue(probe.createNewFile());
            assertNotNull(Environment.getExternalStorageDirectory().list());
        }

        // We always have our package directories
        final File probePackage = new File(mContext.getExternalFilesDir(null),
                "cts" + System.nanoTime());
        assertTrue(probePackage.createNewFile());

        assertTrue(TEST_JPG.exists());
        assertTrue(TEST_PDF.exists());

        final Uri jpgUri = MediaStore.scanFile(mContentResolver, TEST_JPG);
        final Uri pdfUri = MediaStore.scanFile(mContentResolver, TEST_PDF);

        final HashSet<Long> seen = new HashSet<>();
        try (Cursor c = mContentResolver.query(
                MediaStore.Files.getContentUri(MediaStore.VOLUME_EXTERNAL),
                new String[] { MediaColumns._ID }, null, null)) {
            while (c.moveToNext()) {
                seen.add(c.getLong(0));
            }
        }

        if (sandboxed) {
            // If we're sandboxed, we should only see the image
            assertTrue(seen.contains(ContentUris.parseId(jpgUri)));
            assertFalse(seen.contains(ContentUris.parseId(pdfUri)));
        } else {
            // If we're not sandboxed, we should see both
            assertTrue(seen.contains(ContentUris.parseId(jpgUri)));
            assertTrue(seen.contains(ContentUris.parseId(pdfUri)));
        }
    }

    @Test
    public void testMediaNone() throws Exception {
        doMediaNone(MediaStore.Audio.Media.EXTERNAL_CONTENT_URI, MediaStorageTest::createAudio);
        doMediaNone(MediaStore.Video.Media.EXTERNAL_CONTENT_URI, MediaStorageTest::createVideo);
        doMediaNone(MediaStore.Images.Media.EXTERNAL_CONTENT_URI, MediaStorageTest::createImage);

        // But since we don't hold the Music permission, we can't read the
        // indexed metadata
        try (Cursor c = mContentResolver.query(MediaStore.Audio.Artists.EXTERNAL_CONTENT_URI,
                null, null, null)) {
            assertEquals(0, c.getCount());
        }
        try (Cursor c = mContentResolver.query(MediaStore.Audio.Albums.EXTERNAL_CONTENT_URI,
                null, null, null)) {
            assertEquals(0, c.getCount());
        }
        try (Cursor c = mContentResolver.query(MediaStore.Audio.Genres.EXTERNAL_CONTENT_URI,
                null, null, null)) {
            assertEquals(0, c.getCount());
        }
    }

    private void doMediaNone(Uri collection, Callable<Uri> create) throws Exception {
        final Uri red = create.call();
        final Uri blue = create.call();

        clearMediaOwner(blue, mUserId);

        // Since we have no permissions, we should only be able to see media
        // that we've contributed
        final HashSet<Long> seen = new HashSet<>();
        try (Cursor c = mContentResolver.query(collection,
                new String[] { MediaColumns._ID }, null, null)) {
            while (c.moveToNext()) {
                seen.add(c.getLong(0));
            }
        }

        assertTrue(seen.contains(ContentUris.parseId(red)));
        assertFalse(seen.contains(ContentUris.parseId(blue)));

        try (ParcelFileDescriptor pfd = mContentResolver.openFileDescriptor(red, "rw")) {
        }
        try (ParcelFileDescriptor pfd = mContentResolver.openFileDescriptor(blue, "r")) {
            fail("Expected read access to be blocked");
        } catch (SecurityException | FileNotFoundException expected) {
        }
        try (ParcelFileDescriptor pfd = mContentResolver.openFileDescriptor(blue, "w")) {
            fail("Expected write access to be blocked");
        } catch (SecurityException | FileNotFoundException expected) {
        }
    }

    @Test
    public void testMediaRead() throws Exception {
        doMediaRead(MediaStore.Audio.Media.EXTERNAL_CONTENT_URI, MediaStorageTest::createAudio);
        doMediaRead(MediaStore.Video.Media.EXTERNAL_CONTENT_URI, MediaStorageTest::createVideo);
        doMediaRead(MediaStore.Images.Media.EXTERNAL_CONTENT_URI, MediaStorageTest::createImage);
    }

    private void doMediaRead(Uri collection, Callable<Uri> create) throws Exception {
        final Uri red = create.call();
        final Uri blue = create.call();

        clearMediaOwner(blue, mUserId);

        // Holding read permission we can see items we don't own
        final HashSet<Long> seen = new HashSet<>();
        try (Cursor c = mContentResolver.query(collection,
                new String[] { MediaColumns._ID }, null, null)) {
            while (c.moveToNext()) {
                seen.add(c.getLong(0));
            }
        }

        assertTrue(seen.contains(ContentUris.parseId(red)));
        assertTrue(seen.contains(ContentUris.parseId(blue)));

        try (ParcelFileDescriptor pfd = mContentResolver.openFileDescriptor(red, "rw")) {
        }
        try (ParcelFileDescriptor pfd = mContentResolver.openFileDescriptor(blue, "r")) {
        }
        try (ParcelFileDescriptor pfd = mContentResolver.openFileDescriptor(blue, "w")) {
            fail("Expected write access to be blocked");
        } catch (SecurityException | FileNotFoundException expected) {
        }
    }

    @Test
    public void testMediaWrite() throws Exception {
        doMediaWrite(MediaStore.Audio.Media.EXTERNAL_CONTENT_URI, MediaStorageTest::createAudio);
        doMediaWrite(MediaStore.Video.Media.EXTERNAL_CONTENT_URI, MediaStorageTest::createVideo);
        doMediaWrite(MediaStore.Images.Media.EXTERNAL_CONTENT_URI, MediaStorageTest::createImage);
    }

    private void doMediaWrite(Uri collection, Callable<Uri> create) throws Exception {
        final Uri red = create.call();
        final Uri blue = create.call();

        clearMediaOwner(blue, mUserId);

        // Holding read permission we can see items we don't own
        final HashSet<Long> seen = new HashSet<>();
        try (Cursor c = mContentResolver.query(collection,
                new String[] { MediaColumns._ID }, null, null)) {
            while (c.moveToNext()) {
                seen.add(c.getLong(0));
            }
        }

        assertTrue(seen.contains(ContentUris.parseId(red)));
        assertTrue(seen.contains(ContentUris.parseId(blue)));

        try (ParcelFileDescriptor pfd = mContentResolver.openFileDescriptor(red, "rw")) {
        }
        try (ParcelFileDescriptor pfd = mContentResolver.openFileDescriptor(blue, "r")) {
        }
        if (Environment.isExternalStorageLegacy()) {
            try (ParcelFileDescriptor pfd = mContentResolver.openFileDescriptor(blue, "w")) {
            }
        } else {
            try (ParcelFileDescriptor pfd = mContentResolver.openFileDescriptor(blue, "w")) {
                fail("Expected write access to be blocked");
            } catch (SecurityException | FileNotFoundException expected) {
            }
        }
    }

    @Test
    public void testMediaEscalation_Open() throws Exception {
        doMediaEscalation_Open(MediaStorageTest::createAudio);
        doMediaEscalation_Open(MediaStorageTest::createVideo);
        doMediaEscalation_Open(MediaStorageTest::createImage);
    }

    private void doMediaEscalation_Open(Callable<Uri> create) throws Exception {
        final Uri red = create.call();
        clearMediaOwner(red, mUserId);

        RecoverableSecurityException exception = null;
        try (ParcelFileDescriptor pfd = mContentResolver.openFileDescriptor(red, "w")) {
            fail("Expected write access to be blocked");
        } catch (RecoverableSecurityException expected) {
            exception = expected;
        }

        doEscalation(exception);

        try (ParcelFileDescriptor pfd = mContentResolver.openFileDescriptor(red, "w")) {
        }
    }

    @Test
    public void testMediaEscalation_Update() throws Exception {
        doMediaEscalation_Update(MediaStorageTest::createAudio);
        doMediaEscalation_Update(MediaStorageTest::createVideo);
        doMediaEscalation_Update(MediaStorageTest::createImage);
    }

    private void doMediaEscalation_Update(Callable<Uri> create) throws Exception {
        final Uri red = create.call();
        clearMediaOwner(red, mUserId);

        final ContentValues values = new ContentValues();
        values.put(MediaColumns.DISPLAY_NAME, "cts" + System.nanoTime());

        RecoverableSecurityException exception = null;
        try {
            mContentResolver.update(red, values, null, null);
            fail("Expected update access to be blocked");
        } catch (RecoverableSecurityException expected) {
            exception = expected;
        }

        doEscalation(exception);

        assertEquals(1, mContentResolver.update(red, values, null, null));
    }

    @Test
    public void testMediaEscalation_Delete() throws Exception {
        doMediaEscalation_Delete(MediaStorageTest::createAudio);
        doMediaEscalation_Delete(MediaStorageTest::createVideo);
        doMediaEscalation_Delete(MediaStorageTest::createImage);
    }

    private void doMediaEscalation_Delete(Callable<Uri> create) throws Exception {
        final Uri red = create.call();
        clearMediaOwner(red, mUserId);

        RecoverableSecurityException exception = null;
        try {
            mContentResolver.delete(red, null, null);
            fail("Expected update access to be blocked");
        } catch (RecoverableSecurityException expected) {
            exception = expected;
        }

        doEscalation(exception);

        assertEquals(1, mContentResolver.delete(red, null, null));
    }

    @Test
    public void testMediaEscalation_RequestWrite() throws Exception {
        doMediaEscalation_RequestWrite(MediaStorageTest::createAudio);
        doMediaEscalation_RequestWrite(MediaStorageTest::createVideo);
        doMediaEscalation_RequestWrite(MediaStorageTest::createImage);
    }

    private void doMediaEscalation_RequestWrite(Callable<Uri> create) throws Exception {
        final Uri red = create.call();
        clearMediaOwner(red, mUserId);

        try (ParcelFileDescriptor pfd = mContentResolver.openFileDescriptor(red, "w")) {
            fail("Expected write access to be blocked");
        } catch (RecoverableSecurityException expected) {
        }

        doEscalation(MediaStore.createWriteRequest(mContentResolver, Arrays.asList(red)));

        try (ParcelFileDescriptor pfd = mContentResolver.openFileDescriptor(red, "w")) {
        }
    }

    @Test
    public void testMediaEscalation_RequestTrash() throws Exception {
        doMediaEscalation_RequestTrash(MediaStorageTest::createAudio);
        doMediaEscalation_RequestTrash(MediaStorageTest::createVideo);
        doMediaEscalation_RequestTrash(MediaStorageTest::createImage);
    }

    private void doMediaEscalation_RequestTrash(Callable<Uri> create) throws Exception {
        final Uri red = create.call();
        clearMediaOwner(red, mUserId);

        assertEquals("0", queryForSingleColumn(red, MediaColumns.IS_TRASHED));
        doEscalation(MediaStore.createTrashRequest(mContentResolver, Arrays.asList(red), true));
        assertEquals("1", queryForSingleColumn(red, MediaColumns.IS_TRASHED));
        doEscalation(MediaStore.createTrashRequest(mContentResolver, Arrays.asList(red), false));
        assertEquals("0", queryForSingleColumn(red, MediaColumns.IS_TRASHED));
    }

    @Test
    public void testMediaEscalation_RequestFavorite() throws Exception {
        doMediaEscalation_RequestFavorite(MediaStorageTest::createAudio);
        doMediaEscalation_RequestFavorite(MediaStorageTest::createVideo);
        doMediaEscalation_RequestFavorite(MediaStorageTest::createImage);
    }

    private void doMediaEscalation_RequestFavorite(Callable<Uri> create) throws Exception {
        final Uri red = create.call();
        clearMediaOwner(red, mUserId);

        assertEquals("0", queryForSingleColumn(red, MediaColumns.IS_FAVORITE));
        doEscalation(MediaStore.createFavoriteRequest(mContentResolver, Arrays.asList(red), true));
        assertEquals("1", queryForSingleColumn(red, MediaColumns.IS_FAVORITE));
        doEscalation(MediaStore.createFavoriteRequest(mContentResolver, Arrays.asList(red), false));
        assertEquals("0", queryForSingleColumn(red, MediaColumns.IS_FAVORITE));
    }

    @Test
    public void testMediaEscalation_RequestDelete() throws Exception {
        doMediaEscalation_RequestDelete(MediaStorageTest::createAudio);
        doMediaEscalation_RequestDelete(MediaStorageTest::createVideo);
        doMediaEscalation_RequestDelete(MediaStorageTest::createImage);
    }

    private void doMediaEscalation_RequestDelete(Callable<Uri> create) throws Exception {
        final Uri red = create.call();
        clearMediaOwner(red, mUserId);

        try (Cursor c = mContentResolver.query(red, null, null, null)) {
            assertEquals(1, c.getCount());
        }
        doEscalation(MediaStore.createDeleteRequest(mContentResolver, Arrays.asList(red)));
        try (Cursor c = mContentResolver.query(red, null, null, null)) {
            assertEquals(0, c.getCount());
        }
    }

    private void doEscalation(RecoverableSecurityException exception) throws Exception {
        doEscalation(exception.getUserAction().getActionIntent());
    }

    private void doEscalation(PendingIntent pi) throws Exception {
        // Try launching the action to grant ourselves access
        final Instrumentation inst = InstrumentationRegistry.getInstrumentation();
        final Intent intent = new Intent(inst.getContext(), GetResultActivity.class);
        intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);

        // Wake up the device and dismiss the keyguard before the test starts
        final UiDevice device = UiDevice.getInstance(inst);
        device.executeShellCommand("input keyevent KEYCODE_WAKEUP");
        device.executeShellCommand("wm dismiss-keyguard");

        final GetResultActivity activity = (GetResultActivity) inst.startActivitySync(intent);
        device.waitForIdle();
        activity.clearResult();
        activity.startIntentSenderForResult(pi.getIntentSender(), 42, null, 0, 0, 0);

        device.waitForIdle();

        // Some dialogs may have granted access automatically, so we're willing
        // to keep rolling forward if we can't find our grant button
        final UiSelector grant = new UiSelector().textMatches("(?i)Allow");
        if (new UiObject(grant).waitForExists(2_000)) {
            device.findObject(grant).click();
        }

        // Verify that we now have access
        final GetResultActivity.Result res = activity.getResult();
        assertEquals(Activity.RESULT_OK, res.resultCode);
    }

    private static Uri createAudio() throws IOException {
        final Context context = InstrumentationRegistry.getTargetContext();
        final String displayName = "cts" + System.nanoTime();
        final PendingParams params = new PendingParams(
                MediaStore.Audio.Media.EXTERNAL_CONTENT_URI, displayName, "audio/mpeg");
        final Uri pendingUri = MediaStoreUtils.createPending(context, params);
        try (PendingSession session = MediaStoreUtils.openPending(context, pendingUri)) {
            try (InputStream in = context.getResources().getAssets().open("testmp3.mp3");
                    OutputStream out = session.openOutputStream()) {
                FileUtils.copy(in, out);
            }
            return session.publish();
        }
    }

    private static Uri createVideo() throws IOException {
        final Context context = InstrumentationRegistry.getTargetContext();
        final String displayName = "cts" + System.nanoTime();
        final PendingParams params = new PendingParams(
                MediaStore.Video.Media.EXTERNAL_CONTENT_URI, displayName, "video/mpeg");
        final Uri pendingUri = MediaStoreUtils.createPending(context, params);
        try (PendingSession session = MediaStoreUtils.openPending(context, pendingUri)) {
            try (InputStream in = context.getResources().getAssets().open("testmp3.mp3");
                    OutputStream out = session.openOutputStream()) {
                FileUtils.copy(in, out);
            }
            return session.publish();
        }
    }

    private static Uri createImage() throws IOException {
        final Context context = InstrumentationRegistry.getTargetContext();
        final String displayName = "cts" + System.nanoTime();
        final PendingParams params = new PendingParams(
                MediaStore.Images.Media.EXTERNAL_CONTENT_URI, displayName, "image/png");
        final Uri pendingUri = MediaStoreUtils.createPending(context, params);
        try (PendingSession session = MediaStoreUtils.openPending(context, pendingUri)) {
            try (OutputStream out = session.openOutputStream()) {
                final Bitmap bitmap = Bitmap.createBitmap(32, 32, Bitmap.Config.ARGB_8888);
                bitmap.compress(Bitmap.CompressFormat.PNG, 90, out);
            }
            return session.publish();
        }
    }

    private static String queryForSingleColumn(Uri uri, String column) throws Exception {
        final ContentResolver resolver = InstrumentationRegistry.getTargetContext()
                .getContentResolver();
        try (Cursor c = resolver.query(uri, new String[] { column }, null, null)) {
            assertEquals(c.getCount(), 1);
            assertTrue(c.moveToFirst());
            return c.getString(0);
        }
    }

    private static void clearMediaOwner(Uri uri, int userId) throws IOException {
        final String cmd = String.format(
                "content update --uri %s --user %d --bind owner_package_name:n:",
                uri, userId);
        runShellCommand(InstrumentationRegistry.getInstrumentation(), cmd);
    }

    static File stageFile(File file) throws Exception {
        // Sometimes file creation fails due to slow permission update, try more times 
        while(currentAttempt < MAX_NUMBER_OF_ATTEMPT) {
            try {
                file.getParentFile().mkdirs();
                file.createNewFile();
                return file;
            } catch(IOException e) {
                currentAttempt++;
                // wait 500ms
                Thread.sleep(500);
            }
        } 
        return file;
    }
}
