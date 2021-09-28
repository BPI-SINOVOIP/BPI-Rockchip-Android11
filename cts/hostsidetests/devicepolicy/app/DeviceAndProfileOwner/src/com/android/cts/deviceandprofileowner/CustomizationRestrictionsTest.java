/*
 * Copyright (C) 2016 The Android Open Source Project
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

package com.android.cts.deviceandprofileowner;

import android.app.WallpaperManager;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.graphics.Bitmap;
import android.os.UserManager;

import com.android.compatibility.common.util.BitmapUtils;

import java.io.Closeable;
import java.io.IOException;
import java.util.concurrent.ArrayBlockingQueue;
import java.util.concurrent.BlockingQueue;
import java.util.concurrent.TimeUnit;

/**
 * These tests verify that the device / profile owner can use appropriate API for customization
 * (DevicePolicyManager.setUserIcon(), WallpaperManager.setBitmap(), etc.) even in case,
 * when appropriate restrictions are set. The tested restrictions are
 * {@link UserManager#DISALLOW_SET_WALLPAPER} and {@link UserManager#DISALLOW_SET_USER_ICON}.
 */
public class CustomizationRestrictionsTest extends BaseDeviceAdminTest {

    private static final int BROADCAST_TIMEOUT_SEC = 3;

    // Class sets/resets restriction in try-with-resources statement.
    private class RestrictionApplicator implements Closeable {
        private final String mRestriction;

        RestrictionApplicator(String restriction) {
            mRestriction = restriction;
            mDevicePolicyManager.addUserRestriction(ADMIN_RECEIVER_COMPONENT, mRestriction);
        }

        @Override
        public void close() throws IOException {
            mDevicePolicyManager.clearUserRestriction(ADMIN_RECEIVER_COMPONENT, mRestriction);
        }
    }

    // Class subscribes/unsubscribe for broadcast notification in try-with-resources statement.
    private class BroadcastReceiverRegistrator implements Closeable {
        private final BlockingBroadcastReceiver mReceiver;

        public BroadcastReceiverRegistrator(String action) {
            final IntentFilter filter = new IntentFilter();
            filter.addAction(action);
            mReceiver = new BlockingBroadcastReceiver();
            mContext.registerReceiver(mReceiver, filter);
        }

        @Override
        public void close() throws IOException {
            mContext.unregisterReceiver(mReceiver);
        }

        public void waitForBroadcast() throws Exception {
            mReceiver.waitForBroadcast();
        }
    }

    private class BlockingBroadcastReceiver extends BroadcastReceiver {
        private BlockingQueue<Integer> mQueue = new ArrayBlockingQueue<Integer>(1);

        @Override
        public void onReceive(Context context, Intent intent) {
            assertTrue(mQueue.add(0));
        }

        public void waitForBroadcast() throws Exception {
            Integer result = mQueue.poll(BROADCAST_TIMEOUT_SEC, TimeUnit.SECONDS);
            assertNotNull(result);
        }
    }

    // The idea of testing is check if a DO/PO can set a wallpapper despite the
    // DISALLOW_SET_WALLPAPER restriction is set. But we can't use
    // pixel-by-pixel comparison of the reference bitmap (the bitmap we want to be a
    // wallpaper) and current wallpaper bitmap, because the reference bitmap can be
    // processed while setting (e.g. crop or scale), and getter may return us different
    // (but visually the same) Bitmap object. Thus in this test we check if the new
    // wallpaper is different from the old one after we ran a setter method.
    public void testDisallowSetWallpaper_allowed() throws Exception {
        final WallpaperManager wallpaperManager = WallpaperManager.getInstance(mContext);
        final Bitmap originalWallpaper = BitmapUtils.getWallpaperBitmap(mContext);
        final Bitmap originalWallpaperCopy =
                originalWallpaper.copy(originalWallpaper.getConfig(), false);

        try (
                // Set restriction and subscribe for the broadcast.
                final RestrictionApplicator restr =
                        new RestrictionApplicator(UserManager.DISALLOW_SET_WALLPAPER);
                final BroadcastReceiverRegistrator bcast =
                        new BroadcastReceiverRegistrator(Intent.ACTION_WALLPAPER_CHANGED);
        ) {
            assertTrue(mUserManager.hasUserRestriction(UserManager.DISALLOW_SET_WALLPAPER));

            // Checking setBitmap() method.
            Bitmap oldWallpaper = originalWallpaperCopy;
            wallpaperManager.setBitmap(BitmapUtils.generateRandomBitmap(97, 73));
            bcast.waitForBroadcast();
            Bitmap newWallpaper = BitmapUtils.getWallpaperBitmap(mContext);
            assertFalse(BitmapUtils.compareBitmaps(newWallpaper, oldWallpaper));

            // Checking setStream() method.
            oldWallpaper = newWallpaper;
            final Bitmap wallpaperForStream = BitmapUtils.generateRandomBitmap(83, 69);
            wallpaperManager.setStream(BitmapUtils.bitmapToInputStream(wallpaperForStream));
            bcast.waitForBroadcast();
            newWallpaper = BitmapUtils.getWallpaperBitmap(mContext);
            assertFalse(BitmapUtils.compareBitmaps(newWallpaper, oldWallpaper));

            // Checking setResource() method.
            oldWallpaper = newWallpaper;
            wallpaperManager.setResource(R.raw.wallpaper);
            bcast.waitForBroadcast();
            newWallpaper = BitmapUtils.getWallpaperBitmap(mContext);
            assertFalse(BitmapUtils.compareBitmaps(newWallpaper, oldWallpaper));
        } finally {
            wallpaperManager.setBitmap(originalWallpaperCopy);
        }
        assertFalse(mUserManager.hasUserRestriction(UserManager.DISALLOW_SET_WALLPAPER));
    }
}
