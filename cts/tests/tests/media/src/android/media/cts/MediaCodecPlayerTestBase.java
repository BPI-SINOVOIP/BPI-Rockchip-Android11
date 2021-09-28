/*
 * Copyright 2019 Google Inc. All rights reserved.
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

package android.media.cts;

import android.app.Activity;
import android.content.Context;
import android.content.pm.PackageManager;
import android.media.MediaCasException;
import android.media.MediaCodecList;
import android.media.MediaCryptoException;
import android.media.MediaFormat;
import android.net.Uri;
import android.test.ActivityInstrumentationTestCase2;
import android.util.Log;
import android.view.Surface;
import java.io.IOException;
import java.util.List;
import java.util.concurrent.TimeUnit;

class MediaCodecPlayerTestBase<T extends Activity> extends ActivityInstrumentationTestCase2<T> {

    private static final String TAG = MediaCodecPlayerTestBase.class.getSimpleName();
    private static final int CONNECTION_RETRIES = 10;
    private static final int SLEEP_TIME_MS = 1000;
    private static final long PLAY_TIME_MS = TimeUnit.MILLISECONDS.convert(25, TimeUnit.SECONDS);

    protected Context mContext;
    protected MediaCodecClearKeyPlayer mMediaCodecPlayer;

    public MediaCodecPlayerTestBase(Class<T> clz) {
        super(clz);
    }

    @Override
    protected void setUp() throws Exception {
        super.setUp();
        mContext = getInstrumentation().getTargetContext();
    }

    /**
     * Tests clear content playback.
     */
    protected void testPlayback(
            String videoMime, String[] videoFeatures,
            Uri audioUrl,
            Uri videoUrl,
            int videoWidth, int videoHeight,
            List<Surface> surfaces) throws Exception {

        if (isWatchDevice()) {
            return;
        }

        if (!preparePlayback(videoMime, videoFeatures,
                audioUrl, false /* audioEncrypted */,
                videoUrl, false /* videoEncrypted */, videoWidth, videoHeight,
                false /* scrambled */, null /* sessionId */, surfaces)) {
            return;
        }

        // starts video playback
        playUntilEnd();
    }

    protected boolean isWatchDevice() {
        return mContext.getPackageManager().hasSystemFeature(PackageManager.FEATURE_WATCH);
    }

    protected boolean preparePlayback(String videoMime, String[] videoFeatures, Uri audioUrl,
            boolean audioEncrypted, Uri videoUrl, boolean videoEncrypted, int videoWidth,
            int videoHeight, boolean scrambled, byte[] sessionId, List<Surface> surfaces)
            throws IOException, MediaCryptoException, MediaCasException {
        if (false == playbackPreCheck(videoMime, videoFeatures, videoUrl,
                videoWidth, videoHeight)) {
            Log.e(TAG, "Failed playback precheck");
            return false;
        }

        mMediaCodecPlayer = new MediaCodecClearKeyPlayer(
                surfaces,
                sessionId, scrambled,
                mContext.getResources());

        mMediaCodecPlayer.setAudioDataSource(audioUrl, null, audioEncrypted);
        mMediaCodecPlayer.setVideoDataSource(videoUrl, null, videoEncrypted);
        mMediaCodecPlayer.start();
        mMediaCodecPlayer.prepare();
        return true;
    }

    protected void playUntilEnd() throws InterruptedException {
        mMediaCodecPlayer.startThread();

        long timeOut = System.currentTimeMillis() + PLAY_TIME_MS;
        while (timeOut > System.currentTimeMillis() && !mMediaCodecPlayer.isEnded()) {
            Thread.sleep(SLEEP_TIME_MS);
            if (mMediaCodecPlayer.getCurrentPosition() >= mMediaCodecPlayer.getDuration() ) {
                Log.d(TAG, "current pos = " + mMediaCodecPlayer.getCurrentPosition() +
                        ">= duration = " + mMediaCodecPlayer.getDuration());
                break;
            }
        }

        Log.d(TAG, "playVideo player.reset()");
        mMediaCodecPlayer.reset();
    }

    // Verify if we can support playback resolution and has network connection.
    protected boolean playbackPreCheck(String videoMime, String[] videoFeatures,
            Uri videoUrl, int videoWidth, int videoHeight) {
        if (!isResolutionSupported(videoMime, videoFeatures, videoWidth, videoHeight)) {
            Log.i(TAG, "Device does not support " +
                    videoWidth + "x" + videoHeight + " resolution for " + videoMime);
            return false;
        }

        IConnectionStatus connectionStatus = new ConnectionStatus(mContext);
        if (!connectionStatus.isAvailable()) {
            throw new Error("Network is not available, reason: " +
                    connectionStatus.getNotConnectedReason());
        }

        // If device is not online, recheck the status a few times.
        int retries = 0;
        while (!connectionStatus.isConnected()) {
            if (retries++ >= CONNECTION_RETRIES) {
                throw new Error("Device is not online, reason: " +
                        connectionStatus.getNotConnectedReason());
            }
            try {
                Thread.sleep(100);
            } catch (InterruptedException e) {
            }
        }
        connectionStatus.testConnection(videoUrl);
        return true;
    }

    private boolean isResolutionSupported(String mime, String[] features,
            int videoWidth, int videoHeight) {
        MediaFormat format = MediaFormat.createVideoFormat(mime, videoWidth, videoHeight);
        for (String feature: features) {
            format.setFeatureEnabled(feature, true);
        }
        MediaCodecList mcl = new MediaCodecList(MediaCodecList.ALL_CODECS);
        if (mcl.findDecoderForFormat(format) == null) {
            Log.i(TAG, "could not find codec for " + format);
            return false;
        }
        return true;
    }

}
