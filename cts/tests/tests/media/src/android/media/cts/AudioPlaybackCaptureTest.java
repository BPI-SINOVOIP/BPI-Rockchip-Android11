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

package android.media.cts;

import static android.media.AudioAttributes.ALLOW_CAPTURE_BY_ALL;
import static android.media.AudioAttributes.ALLOW_CAPTURE_BY_NONE;
import static android.media.AudioAttributes.ALLOW_CAPTURE_BY_SYSTEM;

import static org.hamcrest.Matchers.greaterThan;
import static org.hamcrest.Matchers.lessThan;
import static org.junit.Assert.assertArrayEquals;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotSame;
import static org.junit.Assert.assertThat;
import static org.junit.Assert.assertTrue;
import static org.junit.Assert.fail;
import static org.testng.Assert.assertThrows;

import android.media.AudioAttributes;
import android.media.AudioAttributes.AttributeUsage;
import android.media.AudioAttributes.CapturePolicy;
import android.media.AudioFormat;
import android.media.AudioManager;
import android.media.AudioPlaybackCaptureConfiguration;
import android.media.AudioRecord;
import android.media.MediaPlayer;
import android.media.projection.MediaProjection;
import android.os.Handler;
import android.os.Looper;
import android.platform.test.annotations.Presubmit;

import androidx.test.rule.ActivityTestRule;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;

import java.nio.ByteBuffer;
import java.nio.ShortBuffer;
import java.util.Stack;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;

/**
 * Test audio playback capture through MediaProjection.
 *
 * The tests do the following:
 *   - retrieve a MediaProjection through MediaProjectionActivity
 *   - play some audio
 *   - use that MediaProjection to record the audio playing
 *   - check that some audio was recorded.
 *
 * Currently the test that some audio was recorded just check that at least one sample is non 0.
 * A better check needs to be used, eg: compare the power spectrum.
 */
@NonMediaMainlineTest
public class AudioPlaybackCaptureTest {
    private static final String TAG = "AudioPlaybackCaptureTest";
    private static final int SAMPLE_RATE = 44100;
    private static final int BUFFER_SIZE = SAMPLE_RATE * 2; // 1s at 44.1k/s 16bit mono

    private AudioManager mAudioManager;
    private boolean mPlaybackBeforeCapture;
    private int mUid; //< UID of this test
    private MediaProjectionActivity mActivity;
    private MediaProjection mMediaProjection;
    @Rule
    public ActivityTestRule<MediaProjectionActivity> mActivityRule =
                new ActivityTestRule<>(MediaProjectionActivity.class);

    private static class APCTestConfig {
        public @AttributeUsage int[] matchingUsages;
        public @AttributeUsage int[] excludeUsages;
        public int[] matchingUids;
        public int[] excludeUids;
        private AudioPlaybackCaptureConfiguration build(MediaProjection projection)
                throws Exception {
            AudioPlaybackCaptureConfiguration.Builder apccBuilder =
                    new AudioPlaybackCaptureConfiguration.Builder(projection);

            if (matchingUsages != null) {
                for (int usage : matchingUsages) {
                    apccBuilder.addMatchingUsage(usage);
                }
            }
            if (excludeUsages != null) {
                for (int usage : excludeUsages) {
                    apccBuilder.excludeUsage(usage);
                }
            }
            if (matchingUids != null) {
                for (int uid : matchingUids) {
                    apccBuilder.addMatchingUid(uid);
                }
            }
            if (excludeUids != null) {
                for (int uid : excludeUids) {
                    apccBuilder.excludeUid(uid);
                }
            }
            AudioPlaybackCaptureConfiguration config = apccBuilder.build();
            assertCorreclyBuilt(config);
            return config;
        }

        private void assertCorreclyBuilt(AudioPlaybackCaptureConfiguration config) {
            assertEqualNullIsEmpty("matchingUsages", matchingUsages, config.getMatchingUsages());
            assertEqualNullIsEmpty("excludeUsages", excludeUsages, config.getExcludeUsages());
            assertEqualNullIsEmpty("matchingUids", matchingUids, config.getMatchingUids());
            assertEqualNullIsEmpty("excludeUids", excludeUids, config.getExcludeUids());
        }

        private void assertEqualNullIsEmpty(String msg, int[] expected, int[] found) {
            if (expected == null) {
                assertEquals(msg, 0, found.length);
            } else {
                assertArrayEquals(msg, expected, found);
            }
        }
    };
    private APCTestConfig mAPCTestConfig;

    @Before
    public void setup() throws Exception {
        mPlaybackBeforeCapture = false;
        mAPCTestConfig = new APCTestConfig();
        mActivity = mActivityRule.getActivity();
        mAudioManager = mActivity.getSystemService(AudioManager.class);
        mUid = mActivity.getApplicationInfo().uid;
        mMediaProjection = mActivity.waitForMediaProjection();
    }

    private AudioRecord createDefaultPlaybackCaptureRecord() throws Exception {
        return createPlaybackCaptureRecord(
            new AudioFormat.Builder()
                 .setEncoding(AudioFormat.ENCODING_PCM_16BIT)
                 .setSampleRate(SAMPLE_RATE)
                 .setChannelMask(AudioFormat.CHANNEL_IN_MONO)
                 .build());
    }

    private AudioRecord createPlaybackCaptureRecord(AudioFormat audioFormat) throws Exception {
        AudioPlaybackCaptureConfiguration apcConfig = mAPCTestConfig.build(mMediaProjection);

        AudioRecord audioRecord = new AudioRecord.Builder()
                .setAudioPlaybackCaptureConfig(apcConfig)
                .setAudioFormat(audioFormat)
                .build();
        assertEquals("AudioRecord failed to initialized", AudioRecord.STATE_INITIALIZED,
                     audioRecord.getState());
        return audioRecord;
    }

    private MediaPlayer createMediaPlayer(@CapturePolicy int capturePolicy, int resid,
                                          @AttributeUsage int usage) {
        MediaPlayer mediaPlayer = MediaPlayer.create(
                mActivity,
                resid,
                new AudioAttributes.Builder()
                    .setContentType(AudioAttributes.CONTENT_TYPE_MUSIC)
                    .setUsage(usage)
                    .setAllowedCapturePolicy(capturePolicy)
                    .build(),
                mAudioManager.generateAudioSessionId());
        mediaPlayer.setLooping(true);
        return mediaPlayer;
    }

    private static ByteBuffer readToBuffer(AudioRecord audioRecord, int bufferSize)
            throws Exception {
        assertEquals("AudioRecord is not recording", AudioRecord.RECORDSTATE_RECORDING,
                     audioRecord.getRecordingState());
        ByteBuffer buffer = ByteBuffer.allocateDirect(bufferSize);
        int retry = 100;
        boolean silence = true;
        while (silence && buffer.hasRemaining()) {
            assertNotSame(buffer.remaining() + "/" + bufferSize + "remaining", 0, retry--);
            int written = audioRecord.read(buffer, buffer.remaining());
            assertThat("audioRecord did not read frames", written, greaterThan(0));
            for (int i = 0; i < written; i++) {
                silence &= buffer.get() == 0;
            }
        }
        buffer.rewind();
        return buffer;
    }

    private static boolean onlySilence(ShortBuffer buffer) {
        while (buffer.hasRemaining()) {
            if (buffer.get() != 0) {
                return false;
            }
        }
        return true;
    }

    public void testPlaybackCapture(@CapturePolicy int capturePolicy,
                                    @AttributeUsage int playbackUsage,
                                    boolean dataPresent) throws Exception {
        MediaPlayer mediaPlayer = createMediaPlayer(capturePolicy, R.raw.testwav_16bit_44100hz,
                                                    playbackUsage);
        try {
            if (mPlaybackBeforeCapture) {
                mediaPlayer.start();
                // Make sure the player is actually playing, thus forcing a rerouting
                Thread.sleep(100);
            }

            AudioRecord audioRecord = createDefaultPlaybackCaptureRecord();

            try {
                audioRecord.startRecording();
                mediaPlayer.start();
                ByteBuffer rawBuffer = readToBuffer(audioRecord, BUFFER_SIZE);
                audioRecord.stop(); // Force an reroute
                mediaPlayer.stop();
                assertEquals(AudioRecord.RECORDSTATE_STOPPED, audioRecord.getRecordingState());
                if (dataPresent) {
                    assertFalse("Expected data, but only silence was recorded",
                                onlySilence(rawBuffer.asShortBuffer()));
                } else {
                    assertTrue("Expected silence, but some data was recorded",
                               onlySilence(rawBuffer.asShortBuffer()));
                }
            } finally {
                audioRecord.release();
            }
        } finally {
            mediaPlayer.release();
        }
    }

    public void testPlaybackCapture(boolean allowCapture,
                                    @AttributeUsage int playbackUsage,
                                    boolean dataPresent) throws Exception {
        if (allowCapture) {
            testPlaybackCapture(ALLOW_CAPTURE_BY_ALL, playbackUsage, dataPresent);
        } else {
            testPlaybackCapture(ALLOW_CAPTURE_BY_SYSTEM, playbackUsage, dataPresent);
            testPlaybackCapture(ALLOW_CAPTURE_BY_NONE, playbackUsage, dataPresent);

            try {
                mAudioManager.setAllowedCapturePolicy(ALLOW_CAPTURE_BY_SYSTEM);
                testPlaybackCapture(ALLOW_CAPTURE_BY_ALL, playbackUsage, dataPresent);
                mAudioManager.setAllowedCapturePolicy(ALLOW_CAPTURE_BY_NONE);
                testPlaybackCapture(ALLOW_CAPTURE_BY_ALL, playbackUsage, dataPresent);
            } finally {
                // Do not impact followup test is case of failure
                mAudioManager.setAllowedCapturePolicy(ALLOW_CAPTURE_BY_ALL);
            }
        }
    }

    private static final boolean OPT_IN = true;
    private static final boolean OPT_OUT = false;

    private static final boolean EXPECT_DATA = true;
    private static final boolean EXPECT_SILENCE = false;

    private static final @AttributeUsage int[] ALLOWED_USAGES = new int[]{
            AudioAttributes.USAGE_UNKNOWN,
            AudioAttributes.USAGE_MEDIA,
            AudioAttributes.USAGE_GAME
    };
    private static final @AttributeUsage int[] FORBIDEN_USAGES = new int[]{
            AudioAttributes.USAGE_ALARM,
            AudioAttributes.USAGE_ASSISTANCE_ACCESSIBILITY,
            AudioAttributes.USAGE_ASSISTANCE_NAVIGATION_GUIDANCE,
            AudioAttributes.USAGE_ASSISTANCE_SONIFICATION,
            AudioAttributes.USAGE_ASSISTANT,
            AudioAttributes.USAGE_NOTIFICATION,
            AudioAttributes.USAGE_VOICE_COMMUNICATION
    };

    @Presubmit
    @Test
    public void testPlaybackCaptureFast() throws Exception {
        mAPCTestConfig.matchingUsages = new int[]{ AudioAttributes.USAGE_MEDIA };
        testPlaybackCapture(OPT_IN, AudioAttributes.USAGE_MEDIA, EXPECT_DATA);
        testPlaybackCapture(OPT_OUT, AudioAttributes.USAGE_MEDIA, EXPECT_SILENCE);
    }

    @Test
    public void testPlaybackCaptureRerouting() throws Exception {
        mPlaybackBeforeCapture = true;
        mAPCTestConfig.matchingUsages = new int[]{ AudioAttributes.USAGE_MEDIA };
        testPlaybackCapture(OPT_IN, AudioAttributes.USAGE_MEDIA, EXPECT_DATA);
    }

    @Test(expected = IllegalArgumentException.class)
    public void testMatchNothing() throws Exception {
        testPlaybackCapture(OPT_IN, AudioAttributes.USAGE_UNKNOWN, EXPECT_SILENCE);
    }

    @Test(expected = IllegalStateException.class)
    public void testCombineUsages() throws Exception {
        mAPCTestConfig.matchingUsages = new int[]{ AudioAttributes.USAGE_UNKNOWN };
        mAPCTestConfig.excludeUsages = new int[]{ AudioAttributes.USAGE_MEDIA };
        testPlaybackCapture(OPT_IN, AudioAttributes.USAGE_UNKNOWN, EXPECT_SILENCE);
    }

    @Test(expected = IllegalStateException.class)
    public void testCombineUid() throws Exception {
        mAPCTestConfig.matchingUids = new int[]{ mUid };
        mAPCTestConfig.excludeUids = new int[]{ 0 };
        testPlaybackCapture(OPT_IN, AudioAttributes.USAGE_UNKNOWN, EXPECT_SILENCE);
    }

    @Test
    public void testCaptureMatchingAllowedUsage() throws Exception {
        for (int usage : ALLOWED_USAGES) {
            mAPCTestConfig.matchingUsages = new int[]{ usage };
            testPlaybackCapture(OPT_IN, usage, EXPECT_DATA);
            testPlaybackCapture(OPT_OUT, usage, EXPECT_SILENCE);

            mAPCTestConfig.matchingUsages = ALLOWED_USAGES;
            testPlaybackCapture(OPT_IN, usage, EXPECT_DATA);
            testPlaybackCapture(OPT_OUT, usage, EXPECT_SILENCE);
        }
    }

    @Test
    public void testCaptureMatchingForbidenUsage() throws Exception {
        for (int usage : FORBIDEN_USAGES) {
            mAPCTestConfig.matchingUsages = new int[]{ usage };
            testPlaybackCapture(OPT_IN, usage, EXPECT_SILENCE);

            mAPCTestConfig.matchingUsages = ALLOWED_USAGES;
            testPlaybackCapture(OPT_IN, usage, EXPECT_SILENCE);
        }
    }

    @Test
    public void testCaptureExcludeUsage() throws Exception {
        for (int usage : ALLOWED_USAGES) {
            mAPCTestConfig.excludeUsages = new int[]{ usage };
            testPlaybackCapture(OPT_IN, usage, EXPECT_SILENCE);

            mAPCTestConfig.excludeUsages = ALLOWED_USAGES;
            testPlaybackCapture(OPT_IN, usage, EXPECT_SILENCE);

            mAPCTestConfig.excludeUsages = FORBIDEN_USAGES;
            testPlaybackCapture(OPT_IN, usage, EXPECT_DATA);
        }
    }

    @Test
    public void testCaptureMatchingUid() throws Exception {
        mAPCTestConfig.matchingUids = new int[]{ mUid };
        testPlaybackCapture(OPT_IN, AudioAttributes.USAGE_GAME, EXPECT_DATA);
        testPlaybackCapture(OPT_OUT, AudioAttributes.USAGE_GAME, EXPECT_SILENCE);
        testPlaybackCapture(OPT_IN, AudioAttributes.USAGE_VOICE_COMMUNICATION, EXPECT_SILENCE);

        mAPCTestConfig.matchingUids = new int[]{ 0 };
        testPlaybackCapture(OPT_IN, AudioAttributes.USAGE_GAME, EXPECT_SILENCE);
    }

    @Test
    public void testCaptureExcludeUid() throws Exception {
        mAPCTestConfig.excludeUids = new int[]{ 0 };
        testPlaybackCapture(OPT_IN, AudioAttributes.USAGE_GAME, EXPECT_DATA);
        testPlaybackCapture(OPT_OUT, AudioAttributes.USAGE_UNKNOWN, EXPECT_SILENCE);
        testPlaybackCapture(OPT_IN, AudioAttributes.USAGE_VOICE_COMMUNICATION, EXPECT_SILENCE);

        mAPCTestConfig.excludeUids = new int[]{ mUid };
        testPlaybackCapture(OPT_IN, AudioAttributes.USAGE_GAME, EXPECT_SILENCE);
    }

    @Test(expected = UnsupportedOperationException.class)
    public void testStoppedMediaProjection() throws Exception {
        mMediaProjection.stop();
        mAPCTestConfig.matchingUsages = new int[]{ AudioAttributes.USAGE_MEDIA };
        testPlaybackCapture(OPT_IN, AudioAttributes.USAGE_MEDIA, EXPECT_DATA);
    }

    @Test
    public void testStopMediaProjectionDuringCapture() throws Exception {
        final int STOP_TIMEOUT_MS = 1000;

        mAPCTestConfig.matchingUsages = new int[]{ AudioAttributes.USAGE_MEDIA };

        MediaPlayer mediaPlayer = createMediaPlayer(ALLOW_CAPTURE_BY_ALL,
                                                    R.raw.testwav_16bit_44100hz,
                                                    AudioAttributes.USAGE_MEDIA);
        mediaPlayer.start();

        AudioRecord audioRecord = createDefaultPlaybackCaptureRecord();
        audioRecord.startRecording();
        ByteBuffer rawBuffer = readToBuffer(audioRecord, BUFFER_SIZE);
        assertFalse("Expected data, but only silence was recorded",
                    onlySilence(rawBuffer.asShortBuffer()));

        final int nativeBufferSize = audioRecord.getBufferSizeInFrames()
                                     * audioRecord.getChannelCount();

        // Stop the media projection
        CountDownLatch stopCDL = new CountDownLatch(1);
        mMediaProjection.registerCallback(new MediaProjection.Callback() {
                public void onStop() {
                    stopCDL.countDown();
                }
            }, new Handler(Looper.getMainLooper()));
        mMediaProjection.stop();
        assertTrue("Could not stop the MediaProjection in " + STOP_TIMEOUT_MS + "ms",
                   stopCDL.await(STOP_TIMEOUT_MS, TimeUnit.MILLISECONDS));

        // With the remote submix disabled, no new samples should feed the track buffer.
        // As a result, read() should fail after at most the total buffer size read.
        // Even if the projection is stopped, the policy unregisteration is async,
        // so double that to be on the conservative side.
        final int MAX_READ_SIZE = 8 * nativeBufferSize;
        int readSize = 0;
        ByteBuffer buffer = ByteBuffer.allocateDirect(BUFFER_SIZE);
        int status;
        while ((status = audioRecord.read(buffer, BUFFER_SIZE)) > 0) {
            readSize += status;
            assertThat("audioRecord did not stop, current state is "
                       + audioRecord.getRecordingState(), readSize, lessThan(MAX_READ_SIZE));
        }
        audioRecord.stop();
        audioRecord.startRecording();

        // Check that the audioRecord can no longer receive audio
        assertThat("Can still record after policy unregistration",
                   audioRecord.read(buffer, BUFFER_SIZE), lessThan(0));

        audioRecord.release();
        mediaPlayer.stop();
        mediaPlayer.release();
    }


    @Test
    public void testPlaybackCaptureDoS() throws Exception {
        final int UPPER_BOUND_TO_CONCURENT_PLAYBACK_CAPTURE = 1000;
        final int MIN_NB_OF_CONCURENT_PLAYBACK_CAPTURE = 5;

        mAPCTestConfig.matchingUsages = new int[]{ AudioAttributes.USAGE_MEDIA };

        Stack<AudioRecord> audioRecords = new Stack<>();
        MediaPlayer mediaPlayer = createMediaPlayer(ALLOW_CAPTURE_BY_ALL,
                                                    R.raw.testwav_16bit_44100hz,
                                                    AudioAttributes.USAGE_MEDIA);
        try {
            mediaPlayer.start();

            // Lets create as many audio playback capture as we can
            try {
                for (int i = 0; i < UPPER_BOUND_TO_CONCURENT_PLAYBACK_CAPTURE; i++) {
                    audioRecords.push(createDefaultPlaybackCaptureRecord());
                }
                fail("Playback capture never failed even with " + audioRecords.size()
                        + " concurrent ones. Are errors silently dropped ?");
            } catch (Exception e) {
                assertThat("Number of supported concurrent playback capture", audioRecords.size(),
                           greaterThan(MIN_NB_OF_CONCURENT_PLAYBACK_CAPTURE));
            }

            // Should not be able to create a new audio playback capture record",
            assertThrows(Exception.class, this::createDefaultPlaybackCaptureRecord);

            // Check that all record can all be started
            for (AudioRecord audioRecord : audioRecords) {
                audioRecord.startRecording();
            }

            // Check that they all record audio
            for (AudioRecord audioRecord : audioRecords) {
                ByteBuffer rawBuffer = readToBuffer(audioRecord, BUFFER_SIZE);
                assertFalse("Expected data, but only silence was recorded",
                            onlySilence(rawBuffer.asShortBuffer()));
            }

            // Stopping one AR must allow creating a new one
            audioRecords.peek().stop();
            audioRecords.pop().release();
            final long SLEEP_AFTER_STOP_FOR_INACTIVITY_MS = 1000;
            Thread.sleep(SLEEP_AFTER_STOP_FOR_INACTIVITY_MS);
            audioRecords.push(createDefaultPlaybackCaptureRecord());

            // That new one must still be able to capture
            audioRecords.peek().startRecording();
            ByteBuffer rawBuffer = readToBuffer(audioRecords.peek(), BUFFER_SIZE);
            assertFalse("Expected data, but only silence was recorded",
                        onlySilence(rawBuffer.asShortBuffer()));

            // cleanup
            mediaPlayer.stop();
        } finally {
            mediaPlayer.release();
            try {
                for (AudioRecord audioRecord : audioRecords) {
                    audioRecord.stop();
                }
            } finally {
                for (AudioRecord audioRecord : audioRecords) {
                    audioRecord.release();
                }
            }
        }
    }

}
