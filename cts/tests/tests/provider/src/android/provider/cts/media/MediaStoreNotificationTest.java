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

package android.provider.cts.media;

import static android.provider.cts.media.MediaStoreTest.TAG;

import android.content.ContentResolver;
import android.content.ContentUris;
import android.content.ContentValues;
import android.content.Context;
import android.database.ContentObserver;
import android.database.Cursor;
import android.net.Uri;
import android.os.Handler;
import android.os.HandlerThread;
import android.provider.MediaStore;
import android.util.Log;

import androidx.test.InstrumentationRegistry;

import org.junit.Before;
import org.junit.Ignore;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.Parameterized;
import org.junit.runners.Parameterized.Parameter;
import org.junit.runners.Parameterized.Parameters;

import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;

@RunWith(Parameterized.class)
public class MediaStoreNotificationTest {
    private Context mContext;
    private ContentResolver mResolver;

    private Uri mSpecificImages;
    private Uri mSpecificFiles;
    private Uri mGenericImages;
    private Uri mGenericFiles;

    @Parameter(0)
    public String mVolumeName;

    @Parameters
    public static Iterable<? extends Object> data() {
        return MediaStore.getExternalVolumeNames(InstrumentationRegistry.getTargetContext());
    }

    @Before
    public void setUp() throws Exception {
        mContext = InstrumentationRegistry.getTargetContext();
        mResolver = mContext.getContentResolver();

        Log.d(TAG, "Using volume " + mVolumeName);
        mSpecificImages = MediaStore.Images.Media.getContentUri(mVolumeName);
        mSpecificFiles = MediaStore.Files.getContentUri(mVolumeName);
        mGenericImages = MediaStore.Images.Media.getContentUri(MediaStore.VOLUME_EXTERNAL);
        mGenericFiles = MediaStore.Files.getContentUri(MediaStore.VOLUME_EXTERNAL);
    }

    @Test
    public void testSimple() throws Exception {
        Uri specificImage;
        Uri specificFile;
        Uri genericImage;
        Uri genericFile;

        try (BlockingObserver si = BlockingObserver.createAndRegister(mSpecificImages);
                BlockingObserver sf = BlockingObserver.createAndRegister(mSpecificFiles);
                BlockingObserver gi = BlockingObserver.createAndRegister(mGenericImages);
                BlockingObserver gf = BlockingObserver.createAndRegister(mGenericFiles)) {
            final ContentValues values = new ContentValues();
            values.put(MediaStore.Images.Media.IS_PENDING, 1);
            values.put(MediaStore.Images.Media.MIME_TYPE, "image/jpeg");
            values.put(MediaStore.Images.Media.DISPLAY_NAME, "cts" + System.nanoTime());
            specificImage = mResolver.insert(mSpecificImages, values);

            final long id = ContentUris.parseId(specificImage);
            specificFile = ContentUris.withAppendedId(mSpecificFiles, id);
            genericImage = ContentUris.withAppendedId(mGenericImages, id);
            genericFile = ContentUris.withAppendedId(mGenericFiles, id);
        }

        try (BlockingObserver si = BlockingObserver.createAndRegister(specificImage);
                BlockingObserver sf = BlockingObserver.createAndRegister(specificFile);
                BlockingObserver gi = BlockingObserver.createAndRegister(genericImage);
                BlockingObserver gf = BlockingObserver.createAndRegister(genericFile)) {
            final ContentValues values = new ContentValues();
            values.put(MediaStore.Images.Media.SIZE, 32);
            mResolver.update(specificImage, values, null, null);
        }

        try (BlockingObserver si = BlockingObserver.createAndRegister(specificImage);
                BlockingObserver sf = BlockingObserver.createAndRegister(specificFile);
                BlockingObserver gi = BlockingObserver.createAndRegister(genericImage);
                BlockingObserver gf = BlockingObserver.createAndRegister(genericFile)) {
            mResolver.delete(specificImage, null, null);
        }
    }

    @Test
    public void testCursor() throws Exception {
        try (Cursor si = mResolver.query(mSpecificImages, null, null, null);
                Cursor sf = mResolver.query(mSpecificFiles, null, null, null);
                Cursor gi = mResolver.query(mGenericImages, null, null, null);
                Cursor gf = mResolver.query(mGenericFiles, null, null, null)) {
            try (BlockingObserver sio = BlockingObserver.create();
                    BlockingObserver sfo = BlockingObserver.create();
                    BlockingObserver gio = BlockingObserver.create();
                    BlockingObserver gfo = BlockingObserver.create()) {
                si.registerContentObserver(sio);
                sf.registerContentObserver(sfo);
                gi.registerContentObserver(gio);
                gf.registerContentObserver(gfo);

                // Insert a simple item that will trigger notifications
                final ContentValues values = new ContentValues();
                values.put(MediaStore.Images.Media.IS_PENDING, 1);
                values.put(MediaStore.Images.Media.MIME_TYPE, "image/jpeg");
                values.put(MediaStore.Images.Media.DISPLAY_NAME, "cts" + System.nanoTime());
                final Uri uri = mResolver.insert(mSpecificImages, values);
                mResolver.delete(uri, null, null);
            }
        }
    }

    private static class BlockingObserver extends ContentObserver implements AutoCloseable {
        private final Uri mUri;
        private final CountDownLatch mLatch = new CountDownLatch(1);

        private static HandlerThread sHandlerThread;
        private static Handler sHandler;

        static {
            sHandlerThread = new HandlerThread(TAG);
            sHandlerThread.start();
            sHandler = new Handler(sHandlerThread.getLooper());
        }

        private BlockingObserver(Uri uri) {
            super(sHandler);
            mUri = uri;
        }

        public static BlockingObserver create() {
            return new BlockingObserver(null);
        }

        public static BlockingObserver createAndRegister(Uri uri) {
            final BlockingObserver obs = new BlockingObserver(uri);
            InstrumentationRegistry.getTargetContext().getContentResolver()
                    .registerContentObserver(uri, true, obs);
            return obs;
        }

        @Override
        public void onChange(boolean selfChange, Uri uri) {
            super.onChange(selfChange, uri);
            Log.v(TAG, "Notified about " + uri);
            mLatch.countDown();
        }

        @Override
        public void close() {
            try {
                if (!mLatch.await(5, TimeUnit.SECONDS)) {
                    throw new InterruptedException();
                }
            } catch (InterruptedException e) {
                throw new IllegalStateException("Failed to get notification for " + mUri);
            } finally {
                InstrumentationRegistry.getTargetContext().getContentResolver()
                        .unregisterContentObserver(this);
            }
        }
    }
}
