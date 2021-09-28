/*
 * Copyright (C) 2011 The Android Open Source Project
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

import android.media.MediaFormat;
import android.media.MediaPlayer;
import android.media.MediaPlayer.TrackInfo;
import android.media.TimedMetaData;
import android.net.Uri;
import android.os.Bundle;
import android.os.Looper;
import android.os.PowerManager;
import android.os.SystemClock;
import android.platform.test.annotations.AppModeFull;
import android.test.InstrumentationTestRunner;
import android.util.Log;
import android.webkit.cts.CtsTestServer;

import com.android.compatibility.common.util.DynamicConfigDeviceSide;
import com.android.compatibility.common.util.MediaUtils;

import java.io.IOException;
import java.io.InterruptedIOException;
import java.net.HttpCookie;
import java.net.Socket;
import java.util.concurrent.atomic.AtomicInteger;
import org.apache.http.impl.DefaultHttpServerConnection;
import org.apache.http.impl.io.SocketOutputBuffer;
import org.apache.http.io.SessionOutputBuffer;
import org.apache.http.params.HttpParams;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

/**
 * Tests of MediaPlayer streaming capabilities.
 */
@NonMediaMainlineTest
@AppModeFull(reason = "TODO: evaluate and port to instant")
public class StreamingMediaPlayerTest extends MediaPlayerTestBase {

    private static final String TAG = "StreamingMediaPlayerTest";

    private static final String HTTP_H263_AMR_VIDEO_1_KEY =
            "streaming_media_player_test_http_h263_amr_video1";
    private static final String HTTP_H263_AMR_VIDEO_2_KEY =
            "streaming_media_player_test_http_h263_amr_video2";
    private static final String HTTP_H264_BASE_AAC_VIDEO_1_KEY =
            "streaming_media_player_test_http_h264_base_aac_video1";
    private static final String HTTP_H264_BASE_AAC_VIDEO_2_KEY =
            "streaming_media_player_test_http_h264_base_aac_video2";
    private static final String HTTP_MPEG4_SP_AAC_VIDEO_1_KEY =
            "streaming_media_player_test_http_mpeg4_sp_aac_video1";
    private static final String HTTP_MPEG4_SP_AAC_VIDEO_2_KEY =
            "streaming_media_player_test_http_mpeg4_sp_aac_video2";
    private static final String MODULE_NAME = "CtsMediaTestCases";

    private static final int LOCAL_HLS_BITS_PER_MS = 100 * 1000;

    private DynamicConfigDeviceSide dynamicConfig;

    private CtsTestServer mServer;

    private String mInputUrl;

    @Override
    protected void setUp() throws Exception {
        // if launched with InstrumentationTestRunner to pass a command line argument
        if (getInstrumentation() instanceof InstrumentationTestRunner) {
            InstrumentationTestRunner testRunner =
                    (InstrumentationTestRunner)getInstrumentation();

            Bundle arguments = testRunner.getArguments();
            mInputUrl = arguments.getString("url");
            Log.v(TAG, "setUp: arguments: " + arguments);
            if (mInputUrl != null) {
                Log.v(TAG, "setUp: arguments[url] " + mInputUrl);
            }
        }

        super.setUp();
        dynamicConfig = new DynamicConfigDeviceSide(MODULE_NAME);
    }

/* RTSP tests are more flaky and vulnerable to network condition.
   Disable until better solution is available
    // Streaming RTSP video from YouTube
    public void testRTSP_H263_AMR_Video1() throws Exception {
        playVideoTest("rtsp://v2.cache7.c.youtube.com/video.3gp?cid=0x271de9756065677e"
                + "&fmt=13&user=android-device-test", 176, 144);
    }
    public void testRTSP_H263_AMR_Video2() throws Exception {
        playVideoTest("rtsp://v2.cache7.c.youtube.com/video.3gp?cid=0xc80658495af60617"
                + "&fmt=13&user=android-device-test", 176, 144);
    }

    public void testRTSP_MPEG4SP_AAC_Video1() throws Exception {
        playVideoTest("rtsp://v2.cache7.c.youtube.com/video.3gp?cid=0x271de9756065677e"
                + "&fmt=17&user=android-device-test", 176, 144);
    }
    public void testRTSP_MPEG4SP_AAC_Video2() throws Exception {
        playVideoTest("rtsp://v2.cache7.c.youtube.com/video.3gp?cid=0xc80658495af60617"
                + "&fmt=17&user=android-device-test", 176, 144);
    }

    public void testRTSP_H264Base_AAC_Video1() throws Exception {
        playVideoTest("rtsp://v2.cache7.c.youtube.com/video.3gp?cid=0x271de9756065677e"
                + "&fmt=18&user=android-device-test", 480, 270);
    }
    public void testRTSP_H264Base_AAC_Video2() throws Exception {
        playVideoTest("rtsp://v2.cache7.c.youtube.com/video.3gp?cid=0xc80658495af60617"
                + "&fmt=18&user=android-device-test", 480, 270);
    }
*/
    // Streaming HTTP video from YouTube
    public void testHTTP_H263_AMR_Video1() throws Exception {
        if (!MediaUtils.checkDecoder(MediaFormat.MIMETYPE_VIDEO_H263, MediaFormat.MIMETYPE_AUDIO_AMR_NB)) {
            return; // skip
        }

        String urlString = dynamicConfig.getValue(HTTP_H263_AMR_VIDEO_1_KEY);
        playVideoTest(urlString, 176, 144);
    }

    public void testHTTP_H263_AMR_Video2() throws Exception {
        if (!MediaUtils.checkDecoder(MediaFormat.MIMETYPE_VIDEO_H263, MediaFormat.MIMETYPE_AUDIO_AMR_NB)) {
            return; // skip
        }

        String urlString = dynamicConfig.getValue(HTTP_H263_AMR_VIDEO_2_KEY);
        playVideoTest(urlString, 176, 144);
    }

    public void testHTTP_MPEG4SP_AAC_Video1() throws Exception {
        if (!MediaUtils.checkDecoder(MediaFormat.MIMETYPE_VIDEO_MPEG4)) {
            return; // skip
        }

        String urlString = dynamicConfig.getValue(HTTP_MPEG4_SP_AAC_VIDEO_1_KEY);
        playVideoTest(urlString, 176, 144);
    }

    public void testHTTP_MPEG4SP_AAC_Video2() throws Exception {
        if (!MediaUtils.checkDecoder(MediaFormat.MIMETYPE_VIDEO_MPEG4)) {
            return; // skip
        }

        String urlString = dynamicConfig.getValue(HTTP_MPEG4_SP_AAC_VIDEO_2_KEY);
        playVideoTest(urlString, 176, 144);
    }

    public void testHTTP_H264Base_AAC_Video1() throws Exception {
        if (!MediaUtils.checkDecoder(MediaFormat.MIMETYPE_VIDEO_AVC)) {
            return; // skip
        }

        String urlString = dynamicConfig.getValue(HTTP_H264_BASE_AAC_VIDEO_1_KEY);
        playVideoTest(urlString, 640, 360);
    }

    public void testHTTP_H264Base_AAC_Video2() throws Exception {
        if (!MediaUtils.checkDecoder(MediaFormat.MIMETYPE_VIDEO_AVC)) {
            return; // skip
        }

        String urlString = dynamicConfig.getValue(HTTP_H264_BASE_AAC_VIDEO_2_KEY);
        playVideoTest(urlString, 640, 360);
    }

    // Streaming HLS video downloaded from YouTube
    public void testHLS() throws Exception {
        if (!MediaUtils.checkDecoder(MediaFormat.MIMETYPE_VIDEO_AVC)) {
            return; // skip
        }

        // Play stream for 60 seconds
        // limit rate to workaround multiplication overflow in framework
        localHlsTest("hls_variant/index.m3u8", 60 * 1000, LOCAL_HLS_BITS_PER_MS, false /*isAudioOnly*/);
    }

    public void testHlsWithHeadersCookies() throws Exception {
        if (!MediaUtils.checkDecoder(MediaFormat.MIMETYPE_VIDEO_AVC)) {
            return; // skip
        }

        // TODO: dummy values for headers/cookies till we find a server that actually needs them
        HashMap<String, String> headers = new HashMap<>();
        headers.put("header0", "value0");
        headers.put("header1", "value1");

        String cookieName = "auth_1234567";
        String cookieValue = "0123456789ABCDEF0123456789ABCDEF";
        HttpCookie cookie = new HttpCookie(cookieName, cookieValue);
        cookie.setHttpOnly(true);
        cookie.setDomain("www.youtube.com");
        cookie.setPath("/");        // all paths
        cookie.setSecure(false);
        cookie.setDiscard(false);
        cookie.setMaxAge(24 * 3600);  // 24hrs

        java.util.Vector<HttpCookie> cookies = new java.util.Vector<HttpCookie>();
        cookies.add(cookie);

        // Play stream for 60 seconds
        // limit rate to workaround multiplication overflow in framework
        localHlsTest("hls_variant/index.m3u8", 60 * 1000, LOCAL_HLS_BITS_PER_MS, false /*isAudioOnly*/);
    }

    public void testHlsSampleAes_bbb_audio_only_overridable() throws Exception {
        if (!MediaUtils.checkDecoder(MediaFormat.MIMETYPE_VIDEO_AVC)) {
            return; // skip
        }

        // Play stream for 60 seconds
        if (mInputUrl != null) {
            // if url override provided
            playLiveAudioOnlyTest(mInputUrl, 60 * 1000);
        } else {
            localHlsTest("audio_only/index.m3u8", 60 * 1000, -1, true /*isAudioOnly*/);
        }

    }

    public void testHlsSampleAes_bbb_unmuxed_1500k() throws Exception {
        if (!MediaUtils.checkDecoder(MediaFormat.MIMETYPE_VIDEO_AVC)) {
            return; // skip
        }
        MediaFormat format = MediaFormat.createVideoFormat(MediaFormat.MIMETYPE_VIDEO_AVC, 1920, 1080);
        String[] decoderNames = MediaUtils.getDecoderNames(false, format);

        if (decoderNames.length == 0) {
            MediaUtils.skipTest("No decoders for " + format);
        } else {
            // Play stream for 60 seconds
            localHlsTest("unmuxed_1500k/index.m3u8", 60 * 1000, -1, false /*isAudioOnly*/);
        }
    }


    // Streaming audio from local HTTP server
    public void testPlayMp3Stream1() throws Throwable {
        localHttpAudioStreamTest("ringer.mp3", false, false);
    }
    public void testPlayMp3Stream2() throws Throwable {
        localHttpAudioStreamTest("ringer.mp3", false, false);
    }
    public void testPlayMp3StreamRedirect() throws Throwable {
        localHttpAudioStreamTest("ringer.mp3", true, false);
    }
    public void testPlayMp3StreamNoLength() throws Throwable {
        localHttpAudioStreamTest("noiseandchirps.mp3", false, true);
    }
    public void testPlayOggStream() throws Throwable {
        localHttpAudioStreamTest("noiseandchirps.ogg", false, false);
    }
    public void testPlayOggStreamRedirect() throws Throwable {
        localHttpAudioStreamTest("noiseandchirps.ogg", true, false);
    }
    public void testPlayOggStreamNoLength() throws Throwable {
        localHttpAudioStreamTest("noiseandchirps.ogg", false, true);
    }
    public void testPlayMp3Stream1Ssl() throws Throwable {
        localHttpsAudioStreamTest("ringer.mp3", false, false);
    }

    private void localHttpAudioStreamTest(final String name, boolean redirect, boolean nolength)
            throws Throwable {
        mServer = new CtsTestServer(mContext);
        try {
            String stream_url = null;
            if (redirect) {
                // Stagefright doesn't have a limit, but we can't test support of infinite redirects
                // Up to 4 redirects seems reasonable though.
                stream_url = mServer.getRedirectingAssetUrl(name, 4);
            } else {
                stream_url = mServer.getAssetUrl(name);
            }
            if (nolength) {
                stream_url = stream_url + "?" + CtsTestServer.NOLENGTH_POSTFIX;
            }

            if (!MediaUtils.checkCodecsForPath(mContext, stream_url)) {
                return; // skip
            }

            mMediaPlayer.setDataSource(stream_url);

            mMediaPlayer.setDisplay(getActivity().getSurfaceHolder());
            mMediaPlayer.setScreenOnWhilePlaying(true);

            mOnBufferingUpdateCalled.reset();
            mMediaPlayer.setOnBufferingUpdateListener(new MediaPlayer.OnBufferingUpdateListener() {
                @Override
                public void onBufferingUpdate(MediaPlayer mp, int percent) {
                    mOnBufferingUpdateCalled.signal();
                }
            });
            mMediaPlayer.setOnErrorListener(new MediaPlayer.OnErrorListener() {
                @Override
                public boolean onError(MediaPlayer mp, int what, int extra) {
                    fail("Media player had error " + what + " playing " + name);
                    return true;
                }
            });

            assertFalse(mOnBufferingUpdateCalled.isSignalled());
            mMediaPlayer.prepare();

            if (nolength) {
                mMediaPlayer.start();
                Thread.sleep(LONG_SLEEP_TIME);
                assertFalse(mMediaPlayer.isPlaying());
            } else {
                mOnBufferingUpdateCalled.waitForSignal();
                mMediaPlayer.start();
                Thread.sleep(SLEEP_TIME);
            }
            mMediaPlayer.stop();
            mMediaPlayer.reset();
        } finally {
            mServer.shutdown();
        }
    }
    private void localHttpsAudioStreamTest(final String name, boolean redirect, boolean nolength)
            throws Throwable {
        mServer = new CtsTestServer(mContext, true);
        try {
            String stream_url = null;
            if (redirect) {
                // Stagefright doesn't have a limit, but we can't test support of infinite redirects
                // Up to 4 redirects seems reasonable though.
                stream_url = mServer.getRedirectingAssetUrl(name, 4);
            } else {
                stream_url = mServer.getAssetUrl(name);
            }
            if (nolength) {
                stream_url = stream_url + "?" + CtsTestServer.NOLENGTH_POSTFIX;
            }

            mMediaPlayer.setDataSource(stream_url);

            mMediaPlayer.setDisplay(getActivity().getSurfaceHolder());
            mMediaPlayer.setScreenOnWhilePlaying(true);

            mOnBufferingUpdateCalled.reset();
            mMediaPlayer.setOnBufferingUpdateListener(new MediaPlayer.OnBufferingUpdateListener() {
                @Override
                public void onBufferingUpdate(MediaPlayer mp, int percent) {
                    mOnBufferingUpdateCalled.signal();
                }
            });
            mMediaPlayer.setOnErrorListener(new MediaPlayer.OnErrorListener() {
                @Override
                public boolean onError(MediaPlayer mp, int what, int extra) {
                    fail("Media player had error " + what + " playing " + name);
                    return true;
                }
            });

            assertFalse(mOnBufferingUpdateCalled.isSignalled());
            try {
                mMediaPlayer.prepare();
            } catch (Exception ex) {
                return;
            }
            fail("https playback should have failed");
        } finally {
            mServer.shutdown();
        }
    }

    public void testPlayHlsStream() throws Throwable {
        if (!MediaUtils.checkDecoder(MediaFormat.MIMETYPE_VIDEO_AVC)) {
            return; // skip
        }
        localHlsTest("hls.m3u8", false, false, false /*isAudioOnly*/);
    }

    public void testPlayHlsStreamWithQueryString() throws Throwable {
        if (!MediaUtils.checkDecoder(MediaFormat.MIMETYPE_VIDEO_AVC)) {
            return; // skip
        }
        localHlsTest("hls.m3u8", true, false, false /*isAudioOnly*/);
    }

    public void testPlayHlsStreamWithRedirect() throws Throwable {
        if (!MediaUtils.checkDecoder(MediaFormat.MIMETYPE_VIDEO_AVC)) {
            return; // skip
        }
        localHlsTest("hls.m3u8", false, true, false /*isAudioOnly*/);
    }

    public void testPlayHlsStreamWithTimedId3() throws Throwable {
        if (!MediaUtils.checkDecoder(MediaFormat.MIMETYPE_VIDEO_AVC)) {
            Log.d(TAG, "Device doesn't have video codec, skipping test");
            return;
        }

        mServer = new CtsTestServer(mContext);
        try {
            // counter must be final if we want to access it inside onTimedMetaData;
            // use AtomicInteger so we can have a final counter object with mutable integer value.
            final AtomicInteger counter = new AtomicInteger();
            String stream_url = mServer.getAssetUrl("prog_index.m3u8");
            mMediaPlayer.setDataSource(stream_url);
            mMediaPlayer.setDisplay(getActivity().getSurfaceHolder());
            mMediaPlayer.setScreenOnWhilePlaying(true);
            mMediaPlayer.setWakeMode(mContext, PowerManager.PARTIAL_WAKE_LOCK);
            mMediaPlayer.setOnTimedMetaDataAvailableListener(new MediaPlayer.OnTimedMetaDataAvailableListener() {
                @Override
                public void onTimedMetaDataAvailable(MediaPlayer mp, TimedMetaData md) {
                    counter.incrementAndGet();
                    int pos = mp.getCurrentPosition();
                    long timeUs = md.getTimestamp();
                    byte[] rawData = md.getMetaData();
                    // Raw data contains an id3 tag holding the decimal string representation of
                    // the associated time stamp rounded to the closest half second.

                    int offset = 0;
                    offset += 3; // "ID3"
                    offset += 2; // version
                    offset += 1; // flags
                    offset += 4; // size
                    offset += 4; // "TXXX"
                    offset += 4; // frame size
                    offset += 2; // frame flags
                    offset += 1; // "\x03" : UTF-8 encoded Unicode
                    offset += 1; // "\x00" : null-terminated empty description

                    int length = rawData.length;
                    length -= offset;
                    length -= 1; // "\x00" : terminating null

                    String data = new String(rawData, offset, length);
                    int dataTimeUs = Integer.parseInt(data);
                    assertTrue("Timed ID3 timestamp does not match content",
                            Math.abs(dataTimeUs - timeUs) < 500000);
                    assertTrue("Timed ID3 arrives after timestamp", pos * 1000 < timeUs);
                }
            });

            final Object completion = new Object();
            mMediaPlayer.setOnCompletionListener(new MediaPlayer.OnCompletionListener() {
                int run;
                @Override
                public void onCompletion(MediaPlayer mp) {
                    if (run++ == 0) {
                        mMediaPlayer.seekTo(0);
                        mMediaPlayer.start();
                    } else {
                        mMediaPlayer.stop();
                        synchronized (completion) {
                            completion.notify();
                        }
                    }
                }
            });

            mMediaPlayer.prepare();
            mMediaPlayer.start();
            assertTrue("MediaPlayer not playing", mMediaPlayer.isPlaying());

            int i = -1;
            TrackInfo[] trackInfos = mMediaPlayer.getTrackInfo();
            for (i = 0; i < trackInfos.length; i++) {
                TrackInfo trackInfo = trackInfos[i];
                if (trackInfo.getTrackType() == TrackInfo.MEDIA_TRACK_TYPE_METADATA) {
                    break;
                }
            }
            assertTrue("Stream has no timed ID3 track", i >= 0);
            mMediaPlayer.selectTrack(i);

            synchronized (completion) {
                completion.wait();
            }

            // There are a total of 19 metadata access units in the test stream; every one of them
            // should be received twice: once before the seek and once after.
            assertTrue("Incorrect number of timed ID3s recieved", counter.get() == 38);
        } finally {
            mServer.shutdown();
        }
    }

    private static class WorkerWithPlayer implements Runnable {
        private final Object mLock = new Object();
        private Looper mLooper;
        private MediaPlayer mMediaPlayer;

        /**
         * Creates a worker thread with the given name. The thread
         * then runs a {@link android.os.Looper}.
         * @param name A name for the new thread
         */
        WorkerWithPlayer(String name) {
            Thread t = new Thread(null, this, name);
            t.setPriority(Thread.MIN_PRIORITY);
            t.start();
            synchronized (mLock) {
                while (mLooper == null) {
                    try {
                        mLock.wait();
                    } catch (InterruptedException ex) {
                    }
                }
            }
        }

        public MediaPlayer getPlayer() {
            return mMediaPlayer;
        }

        @Override
        public void run() {
            synchronized (mLock) {
                Looper.prepare();
                mLooper = Looper.myLooper();
                mMediaPlayer = new MediaPlayer();
                mLock.notifyAll();
            }
            Looper.loop();
        }

        public void quit() {
            mLooper.quit();
            mMediaPlayer.release();
        }
    }

    public void testBlockingReadRelease() throws Throwable {

        mServer = new CtsTestServer(mContext);

        WorkerWithPlayer worker = new WorkerWithPlayer("player");
        final MediaPlayer mp = worker.getPlayer();

        try {
            String path = mServer.getDelayedAssetUrl("noiseandchirps.ogg", 15000);
            mp.setDataSource(path);
            mp.setOnPreparedListener(new MediaPlayer.OnPreparedListener() {
                @Override
                public void onPrepared(MediaPlayer mp) {
                    fail("prepare should not succeed");
                }
            });
            mp.prepareAsync();
            Thread.sleep(1000);
            long start = SystemClock.elapsedRealtime();
            mp.release();
            long end = SystemClock.elapsedRealtime();
            long releaseDuration = (end - start);
            assertTrue("release took too long: " + releaseDuration, releaseDuration < 1000);
        } catch (IllegalArgumentException e) {
            fail(e.getMessage());
        } catch (SecurityException e) {
            fail(e.getMessage());
        } catch (IllegalStateException e) {
            fail(e.getMessage());
        } catch (IOException e) {
            fail(e.getMessage());
        } catch (InterruptedException e) {
            fail(e.getMessage());
        } finally {
            mServer.shutdown();
        }

        // give the worker a bit of time to start processing the message before shutting it down
        Thread.sleep(5000);
        worker.quit();
    }

    private void localHlsTest(final String name, boolean appendQueryString,
            boolean redirect, boolean isAudioOnly) throws Exception {
        localHlsTest(name, null, null, appendQueryString, redirect, 10, -1, isAudioOnly);
    }

    private void localHlsTest(final String name, int playTime, int bitsPerMs, boolean isAudioOnly)
            throws Exception {
        localHlsTest(name, null, null, false, false, playTime, bitsPerMs, isAudioOnly);
    }

    private void localHlsTest(String name, Map<String, String> headers, List<HttpCookie> cookies,
            boolean appendQueryString, boolean redirect, int playTime, int bitsPerMs,
            boolean isAudioOnly) throws Exception {
        if (bitsPerMs >= 0) {
            mServer = new CtsTestServer(mContext) {
                @Override
                protected DefaultHttpServerConnection createHttpServerConnection() {
                    return new RateLimitHttpServerConnection(bitsPerMs);
                }
            };
        } else {
            mServer = new CtsTestServer(mContext);
        }
        try {
            String stream_url = null;
            if (redirect) {
                stream_url = mServer.getQueryRedirectingAssetUrl(name);
            } else {
                stream_url = mServer.getAssetUrl(name);
            }
            if (appendQueryString) {
                stream_url += "?foo=bar/baz";
            }
            if (isAudioOnly) {
                playLiveAudioOnlyTest(Uri.parse(stream_url), headers, cookies, playTime);
            } else {
                playLiveVideoTest(Uri.parse(stream_url), headers, cookies, playTime);
            }
        } finally {
            mServer.shutdown();
        }
    }

    private static final class RateLimitHttpServerConnection extends DefaultHttpServerConnection {

        private final int mBytesPerMs;
        private int mBytesWritten;

        public RateLimitHttpServerConnection(int bitsPerMs) {
            mBytesPerMs = bitsPerMs / 8;
        }

        @Override
        protected SessionOutputBuffer createHttpDataTransmitter(
                Socket socket, int buffersize, HttpParams params) throws IOException {
            return createSessionOutputBuffer(socket, buffersize, params);
        }

        SessionOutputBuffer createSessionOutputBuffer(
                Socket socket, int buffersize, HttpParams params) throws IOException {
            return new SocketOutputBuffer(socket, buffersize, params) {
                @Override
                public void write(int b) throws IOException {
                    write(new byte[] {(byte)b});
                }

                @Override
                public void write(byte[] b) throws IOException {
                    write(b, 0, b.length);
                }

                @Override
                public synchronized void write(byte[] b, int off, int len) throws IOException {
                    mBytesWritten += len;
                    if (mBytesWritten >= mBytesPerMs * 10) {
                        int r = mBytesWritten % mBytesPerMs;
                        int nano = 999999 * r / mBytesPerMs;
                        delay(mBytesWritten / mBytesPerMs, nano);
                        mBytesWritten = 0;
                    }
                    super.write(b, off, len);
                }

                private void delay(long millis, int nanos) throws IOException {
                    try {
                        Thread.sleep(millis, nanos);
                        flush();
                    } catch (InterruptedException e) {
                        throw new InterruptedIOException();
                    }
                }

            };
        }
    }

}
