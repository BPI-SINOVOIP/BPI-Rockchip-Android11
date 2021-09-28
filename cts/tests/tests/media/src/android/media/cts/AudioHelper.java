/*
 * Copyright (C) 2015 The Android Open Source Project
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

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;

import com.android.compatibility.common.util.CddTest;
import com.android.compatibility.common.util.DeviceReportLog;
import com.android.compatibility.common.util.ResultType;
import com.android.compatibility.common.util.ResultUnit;
import java.nio.ByteBuffer;

import org.junit.Assert;

import android.annotation.IntRange;
import android.annotation.NonNull;
import android.annotation.Nullable;
import android.media.AudioAttributes;
import android.media.AudioFormat;
import android.media.AudioManager;
import android.media.AudioRecord;
import android.media.AudioTimestamp;
import android.media.AudioTrack;
import android.os.Looper;
import android.os.PersistableBundle;
import android.util.Log;

import androidx.test.InstrumentationRegistry;

// Used for statistics and loopers in listener tests.
// See AudioRecordTest.java and AudioTrack_ListenerTest.java.
public class AudioHelper {

    // asserts key equals expected in the metrics bundle.
    public static void assertMetricsKeyEquals(
            PersistableBundle metrics, String key, Object expected) {
        Object actual = metrics.get(key);
        assertEquals("metric " + key + " actual " + actual + " != " + " expected " + expected,
                expected, actual);
    }

    // asserts key exists in the metrics bundle.
    public static void assertMetricsKey(PersistableBundle metrics, String key) {
        Object actual = metrics.get(key);
        assertNotNull("metric " + key + " does not exist", actual);
    }

    // create sine waves or chirps for data arrays
    public static byte[] createSoundDataInByteArray(int bufferSamples, final int sampleRate,
            final double frequency, double sweep) {
        final double rad = 2 * Math.PI * frequency / sampleRate;
        byte[] vai = new byte[bufferSamples];
        sweep = Math.PI * sweep / ((double)sampleRate * vai.length);
        for (int j = 0; j < vai.length; j++) {
            int unsigned =  (int)(Math.sin(j * (rad + j * sweep)) * Byte.MAX_VALUE)
                    + Byte.MAX_VALUE & 0xFF;
            vai[j] = (byte) unsigned;
        }
        return vai;
    }

    public static short[] createSoundDataInShortArray(int bufferSamples, final int sampleRate,
            final double frequency, double sweep) {
        final double rad = 2 * Math.PI * frequency / sampleRate;
        short[] vai = new short[bufferSamples];
        sweep = Math.PI * sweep / ((double)sampleRate * vai.length);
        for (int j = 0; j < vai.length; j++) {
            vai[j] = (short)(Math.sin(j * (rad + j * sweep)) * Short.MAX_VALUE);
        }
        return vai;
    }

    public static float[] createSoundDataInFloatArray(int bufferSamples, final int sampleRate,
            final double frequency, double sweep) {
        final double rad = 2 * Math.PI * frequency / sampleRate;
        float[] vaf = new float[bufferSamples];
        sweep = Math.PI * sweep / ((double)sampleRate * vaf.length);
        for (int j = 0; j < vaf.length; j++) {
            vaf[j] = (float)(Math.sin(j * (rad + j * sweep)));
        }
        return vaf;
    }

    /**
     * Create and fill a short array with complete sine waves so we can
     * hear buffer underruns more easily.
     */
    public static short[] createSineWavesShort(int numFrames, int samplesPerFrame,
            int numCycles, double amplitude) {
        final short[] data = new short[numFrames * samplesPerFrame];
        final double rad = numCycles * 2.0 * Math.PI / numFrames;
        for (int j = 0; j < data.length;) {
            short sample = (short)(amplitude * Math.sin(j * rad) * Short.MAX_VALUE);
            for (int sampleIndex = 0; sampleIndex < samplesPerFrame; sampleIndex++) {
                data[j++] = sample;
            }
        }
        return data;
    }

    public static int frameSizeFromFormat(AudioFormat format) {
        return format.getChannelCount()
                * format.getBytesPerSample(format.getEncoding());
    }

    public static int frameCountFromMsec(int ms, AudioFormat format) {
        return ms * format.getSampleRate() / 1000;
    }

    public static class Statistics {
        public void add(double value) {
            final double absValue = Math.abs(value);
            mSum += value;
            mSumAbs += absValue;
            mMaxAbs = Math.max(mMaxAbs, absValue);
            ++mCount;
        }

        public double getAvg() {
            if (mCount == 0) {
                return 0;
            }
            return mSum / mCount;
        }

        public double getAvgAbs() {
            if (mCount == 0) {
                return 0;
            }
            return mSumAbs / mCount;
        }

        public double getMaxAbs() {
            return mMaxAbs;
        }

        private int mCount = 0;
        private double mSum = 0;
        private double mSumAbs = 0;
        private double mMaxAbs = 0;
    }

    // for listener tests
    // lightweight java.util.concurrent.Future*
    public static class FutureLatch<T>
    {
        private T mValue;
        private boolean mSet;
        public void set(T value)
        {
            synchronized (this) {
                assert !mSet;
                mValue = value;
                mSet = true;
                notify();
            }
        }
        public T get()
        {
            T value;
            synchronized (this) {
                while (!mSet) {
                    try {
                        wait();
                    } catch (InterruptedException e) {
                        ;
                    }
                }
                value = mValue;
            }
            return value;
        }
    }

    // for listener tests
    // represents a factory for T
    public interface MakesSomething<T>
    {
        T makeSomething();
    }

    // for listener tests
    // used to construct an object in the context of an asynchronous thread with looper
    public static class MakeSomethingAsynchronouslyAndLoop<T>
    {
        private Thread mThread;
        volatile private Looper mLooper;
        private final MakesSomething<T> mWhatToMake;

        public MakeSomethingAsynchronouslyAndLoop(MakesSomething<T> whatToMake)
        {
            assert whatToMake != null;
            mWhatToMake = whatToMake;
        }

        public T make()
        {
            final FutureLatch<T> futureLatch = new FutureLatch<T>();
            mThread = new Thread()
            {
                @Override
                public void run()
                {
                    Looper.prepare();
                    mLooper = Looper.myLooper();
                    T something = mWhatToMake.makeSomething();
                    futureLatch.set(something);
                    Looper.loop();
                }
            };
            mThread.start();
            return futureLatch.get();
        }
        public void join()
        {
            mLooper.quit();
            try {
                mThread.join();
            } catch (InterruptedException e) {
                ;
            }
            // avoid dangling references
            mLooper = null;
            mThread = null;
        }
    }

    public static int outChannelMaskFromInChannelMask(int channelMask) {
        switch (channelMask) {
            case AudioFormat.CHANNEL_IN_MONO:
                return AudioFormat.CHANNEL_OUT_MONO;
            case AudioFormat.CHANNEL_IN_STEREO:
                return AudioFormat.CHANNEL_OUT_STEREO;
            default:
                return AudioFormat.CHANNEL_INVALID;
        }
    }

    @CddTest(requirement="5.10/C-1-6,C-1-7")
    public static class TimestampVerifier {

        // CDD 5.6 1ms timestamp accuracy
        private static final double TEST_MAX_JITTER_MS_ALLOWED = 6.; // a sanity check
        private static final double TEST_STD_JITTER_MS_ALLOWED = 3.; // flaky tolerance 3x
        private static final double TEST_STD_JITTER_MS_WARN = 1.;    // CDD requirement warning

        // CDD 5.6 100ms track startup latency
        private static final double TEST_STARTUP_TIME_MS_ALLOWED = 500.; // error
        private final double TEST_STARTUP_TIME_MS_WARN;                  // warning
        private static final double TEST_STARTUP_TIME_MS_INFO = 100.;    // informational

        private static final int MILLIS_PER_SECOND = 1000;
        private static final long NANOS_PER_MILLISECOND = 1000000;
        private static final long NANOS_PER_SECOND = NANOS_PER_MILLISECOND * MILLIS_PER_SECOND;
        private static final String REPORT_LOG_NAME = "CtsMediaTestCases";

        private final String mTag;
        private final int mSampleRate;

        // Running statistics
        private int mCount = 0;
        private long mLastFrames = 0;
        private long mLastTimeNs = 0;
        private int mJitterCount = 0;
        private double mMeanJitterMs = 0.;
        private double mSecondMomentJitterMs = 0.;
        private double mMaxAbsJitterMs = 0.;
        private int mWarmupCount = 0;

        public TimestampVerifier(@Nullable String tag, @IntRange(from=4000) int sampleRate,
                boolean isProAudioDevice) {
            mTag = tag;  // Log accepts null
            mSampleRate = sampleRate;
            // Warning if higher than MUST value for pro audio.  Zero means ignore.
            TEST_STARTUP_TIME_MS_WARN = isProAudioDevice ? 200. : 0.;
        }

        public int getJitterCount() { return mJitterCount; }
        public double getMeanJitterMs() { return mMeanJitterMs; }
        public double getStdJitterMs() { return Math.sqrt(mSecondMomentJitterMs / mJitterCount); }
        public double getMaxAbsJitterMs() { return mMaxAbsJitterMs; }
        public double getStartTimeNs() {
            return mLastTimeNs - (mLastFrames * NANOS_PER_SECOND / mSampleRate);
        }

        public void add(@NonNull AudioTimestamp ts) {
            final long frames = ts.framePosition;
            final long timeNs = ts.nanoTime;

            assertTrue("timestamps must have causal time", System.nanoTime() >= timeNs);

            if (mCount > 0) { // need delta info from previous iteration (skipping first)
                final long deltaFrames = frames - mLastFrames;
                final long deltaTimeNs = timeNs - mLastTimeNs;

                if (deltaFrames == 0 && deltaTimeNs == 0) return;

                final double deltaFramesNs = (double)deltaFrames * NANOS_PER_SECOND / mSampleRate;
                final double jitterMs = (deltaTimeNs - deltaFramesNs)  // actual - expected
                        * (1. / NANOS_PER_MILLISECOND);

                Log.d(mTag, "frames(" + frames
                        + ") timeNs(" + timeNs
                        + ") lastframes(" + mLastFrames
                        + ") lastTimeNs(" + mLastTimeNs
                        + ") deltaFrames(" + deltaFrames
                        + ") deltaTimeNs(" + deltaTimeNs
                        + ") jitterMs(" + jitterMs + ")");
                assertTrue("timestamp time should be increasing", deltaTimeNs >= 0);
                assertTrue("timestamp frames should be increasing", deltaFrames >= 0);

                if (mLastFrames != 0) {
                    if (mWarmupCount++ > 1) { // ensure device is warmed up
                        // Welford's algorithm
                        // https://en.wikipedia.org/wiki/Algorithms_for_calculating_variance
                        ++mJitterCount;
                        final double delta = jitterMs - mMeanJitterMs;
                        mMeanJitterMs += delta / mJitterCount;
                        final double delta2 = jitterMs - mMeanJitterMs;
                        mSecondMomentJitterMs += delta * delta2;

                        // jitterMs is signed, so max uses abs() here.
                        final double absJitterMs = Math.abs(jitterMs);
                        if (absJitterMs > mMaxAbsJitterMs) {
                            mMaxAbsJitterMs = absJitterMs;
                        }
                    }
                }
            }
            ++mCount;
            mLastFrames = frames;
            mLastTimeNs = timeNs;
        }

        public void verifyAndLog(long trackStartTimeNs, @Nullable String logName) {
            // enough timestamps?
            assertTrue("need at least 2 jitter measurements", mJitterCount >= 2);

            // Compute startup time and std jitter.
            final int startupTimeMs =
                    (int) ((getStartTimeNs() - trackStartTimeNs) / NANOS_PER_MILLISECOND);
            final double stdJitterMs = getStdJitterMs();

            // Check startup time
            assertTrue("expect startupTimeMs " + startupTimeMs
                            + " <= " + TEST_STARTUP_TIME_MS_ALLOWED,
                    startupTimeMs <= TEST_STARTUP_TIME_MS_ALLOWED);
            if (TEST_STARTUP_TIME_MS_WARN > 0 && startupTimeMs > TEST_STARTUP_TIME_MS_WARN) {
                Log.w(mTag, "CDD warning: startup time " + startupTimeMs
                        + " > " + TEST_STARTUP_TIME_MS_WARN);
            } else if (startupTimeMs > TEST_STARTUP_TIME_MS_INFO) {
                Log.i(mTag, "CDD informational: startup time " + startupTimeMs
                        + " > " + TEST_STARTUP_TIME_MS_INFO);
            }

            // Check maximum jitter
            assertTrue("expect maxAbsJitterMs(" + mMaxAbsJitterMs + ") < "
                            + TEST_MAX_JITTER_MS_ALLOWED,
                    mMaxAbsJitterMs < TEST_MAX_JITTER_MS_ALLOWED);

            // Check std jitter
            if (stdJitterMs > TEST_STD_JITTER_MS_WARN) {
                Log.w(mTag, "CDD warning: std timestamp jitter " + stdJitterMs
                        + " > " + TEST_STD_JITTER_MS_WARN);
            }
            assertTrue("expect stdJitterMs " + stdJitterMs + " < " + TEST_STD_JITTER_MS_ALLOWED,
                    stdJitterMs < TEST_STD_JITTER_MS_ALLOWED);

            Log.d(mTag, "startupTimeMs(" + startupTimeMs
                    + ") meanJitterMs(" + mMeanJitterMs
                    + ") maxAbsJitterMs(" + mMaxAbsJitterMs
                    + ") stdJitterMs(" + stdJitterMs
                    + ")");

            // Log results if logName is provided
            if (logName != null) {
                DeviceReportLog log = new DeviceReportLog(REPORT_LOG_NAME, logName);
                // ReportLog needs at least one Value and Summary.
                log.addValue("startup_time_ms", startupTimeMs,
                        ResultType.LOWER_BETTER, ResultUnit.MS);
                log.addValue("maximum_abs_jitter_ms", mMaxAbsJitterMs,
                        ResultType.LOWER_BETTER, ResultUnit.MS);
                log.addValue("mean_jitter_ms", mMeanJitterMs,
                        ResultType.LOWER_BETTER, ResultUnit.MS);
                log.setSummary("std_jitter_ms", stdJitterMs,
                        ResultType.LOWER_BETTER, ResultUnit.MS);
                log.submit(InstrumentationRegistry.getInstrumentation());
            }
        }
    }

    /* AudioRecordAudit extends AudioRecord to allow concurrent playback
     * of read content to an AudioTrack.  This is for testing only.
     * For general applications, it is NOT recommended to extend AudioRecord.
     * This affects AudioRecord timing.
     */
    public static class AudioRecordAudit extends AudioRecord {
        public AudioRecordAudit(int audioSource, int sampleRate, int channelMask,
                int format, int bufferSize, boolean isChannelIndex) {
            this(audioSource, sampleRate, channelMask, format, bufferSize, isChannelIndex,
                    AudioManager.STREAM_MUSIC, 500 /*delayMs*/);
        }

        public AudioRecordAudit(int audioSource, int sampleRate, int channelMask,
                int format, int bufferSize,
                boolean isChannelIndex, int auditStreamType, int delayMs) {
            // without channel index masks, one could call:
            // super(audioSource, sampleRate, channelMask, format, bufferSize);
            super(new AudioAttributes.Builder()
                            .setInternalCapturePreset(audioSource)
                            .build(),
                    (isChannelIndex
                            ? new AudioFormat.Builder().setChannelIndexMask(channelMask)
                                    : new AudioFormat.Builder().setChannelMask(channelMask))
                            .setEncoding(format)
                            .setSampleRate(sampleRate)
                            .build(),
                    bufferSize,
                    AudioManager.AUDIO_SESSION_ID_GENERATE);

            if (delayMs >= 0) { // create an AudioTrack
                final int channelOutMask = isChannelIndex ? channelMask :
                    outChannelMaskFromInChannelMask(channelMask);
                final int bufferOutFrames = sampleRate * delayMs / 1000;
                final int bufferOutSamples = bufferOutFrames
                        * AudioFormat.channelCountFromOutChannelMask(channelOutMask);
                final int bufferOutSize = bufferOutSamples
                        * AudioFormat.getBytesPerSample(format);

                // Caution: delayMs too large results in buffer sizes that cannot be created.
                mTrack = new AudioTrack.Builder()
                                .setAudioAttributes(new AudioAttributes.Builder()
                                        .setLegacyStreamType(auditStreamType)
                                        .build())
                                .setAudioFormat((isChannelIndex ?
                                  new AudioFormat.Builder().setChannelIndexMask(channelOutMask) :
                                  new AudioFormat.Builder().setChannelMask(channelOutMask))
                                        .setEncoding(format)
                                        .setSampleRate(sampleRate)
                                        .build())
                                .setBufferSizeInBytes(bufferOutSize)
                                .build();
                Assert.assertEquals(AudioTrack.STATE_INITIALIZED, mTrack.getState());
                mPosition = 0;
                mFinishAtMs = 0;
            }
        }

        @Override
        public int read(byte[] audioData, int offsetInBytes, int sizeInBytes) {
            // for byte array access we verify format is 8 bit PCM (typical use)
            Assert.assertEquals(TAG + ": format mismatch",
                    AudioFormat.ENCODING_PCM_8BIT, getAudioFormat());
            int samples = super.read(audioData, offsetInBytes, sizeInBytes);
            if (mTrack != null) {
                Assert.assertEquals(samples, mTrack.write(audioData, offsetInBytes, samples));
                mPosition += samples / mTrack.getChannelCount();
            }
            return samples;
        }

        @Override
        public int read(byte[] audioData, int offsetInBytes, int sizeInBytes, int readMode) {
            // for byte array access we verify format is 8 bit PCM (typical use)
            Assert.assertEquals(TAG + ": format mismatch",
                    AudioFormat.ENCODING_PCM_8BIT, getAudioFormat());
            int samples = super.read(audioData, offsetInBytes, sizeInBytes, readMode);
            if (mTrack != null) {
                Assert.assertEquals(samples, mTrack.write(audioData, offsetInBytes, samples,
                        AudioTrack.WRITE_BLOCKING));
                mPosition += samples / mTrack.getChannelCount();
            }
            return samples;
        }

        @Override
        public int read(short[] audioData, int offsetInShorts, int sizeInShorts) {
            // for short array access we verify format is 16 bit PCM (typical use)
            Assert.assertEquals(TAG + ": format mismatch",
                    AudioFormat.ENCODING_PCM_16BIT, getAudioFormat());
            int samples = super.read(audioData, offsetInShorts, sizeInShorts);
            if (mTrack != null) {
                Assert.assertEquals(samples, mTrack.write(audioData, offsetInShorts, samples));
                mPosition += samples / mTrack.getChannelCount();
            }
            return samples;
        }

        @Override
        public int read(short[] audioData, int offsetInShorts, int sizeInShorts, int readMode) {
            // for short array access we verify format is 16 bit PCM (typical use)
            Assert.assertEquals(TAG + ": format mismatch",
                    AudioFormat.ENCODING_PCM_16BIT, getAudioFormat());
            int samples = super.read(audioData, offsetInShorts, sizeInShorts, readMode);
            if (mTrack != null) {
                Assert.assertEquals(samples, mTrack.write(audioData, offsetInShorts, samples,
                        AudioTrack.WRITE_BLOCKING));
                mPosition += samples / mTrack.getChannelCount();
            }
            return samples;
        }

        @Override
        public int read(float[] audioData, int offsetInFloats, int sizeInFloats, int readMode) {
            // for float array access we verify format is float PCM (typical use)
            Assert.assertEquals(TAG + ": format mismatch",
                    AudioFormat.ENCODING_PCM_FLOAT, getAudioFormat());
            int samples = super.read(audioData, offsetInFloats, sizeInFloats, readMode);
            if (mTrack != null) {
                Assert.assertEquals(samples, mTrack.write(audioData, offsetInFloats, samples,
                        AudioTrack.WRITE_BLOCKING));
                mPosition += samples / mTrack.getChannelCount();
            }
            return samples;
        }

        @Override
        public int read(ByteBuffer audioBuffer, int sizeInBytes) {
            int bytes = super.read(audioBuffer, sizeInBytes);
            if (mTrack != null) {
                // read does not affect position and limit of the audioBuffer.
                // we make a duplicate to change that for writing to the output AudioTrack
                // which does check position and limit.
                ByteBuffer copy = audioBuffer.duplicate();
                copy.position(0).limit(bytes);  // read places data at the start of the buffer.
                Assert.assertEquals(bytes, mTrack.write(copy, bytes, AudioTrack.WRITE_BLOCKING));
                mPosition += bytes /
                        (mTrack.getChannelCount()
                                * AudioFormat.getBytesPerSample(mTrack.getAudioFormat()));
            }
            return bytes;
        }

        @Override
        public int read(ByteBuffer audioBuffer, int sizeInBytes, int readMode) {
            int bytes = super.read(audioBuffer, sizeInBytes, readMode);
            if (mTrack != null) {
                // read does not affect position and limit of the audioBuffer.
                // we make a duplicate to change that for writing to the output AudioTrack
                // which does check position and limit.
                ByteBuffer copy = audioBuffer.duplicate();
                copy.position(0).limit(bytes);  // read places data at the start of the buffer.
                Assert.assertEquals(bytes, mTrack.write(copy, bytes, AudioTrack.WRITE_BLOCKING));
                mPosition += bytes /
                        (mTrack.getChannelCount()
                                * AudioFormat.getBytesPerSample(mTrack.getAudioFormat()));
            }
            return bytes;
        }

        @Override
        public void startRecording() {
            super.startRecording();
            if (mTrack != null) {
                mTrack.play();
            }
        }

        @Override
        public void stop() {
            super.stop();
            if (mTrack != null) {
                if (mPosition > 0) { // stop may be called multiple times.
                    final int remainingFrames = mPosition - mTrack.getPlaybackHeadPosition();
                    mFinishAtMs = System.currentTimeMillis()
                            + remainingFrames * 1000 / mTrack.getSampleRate();
                    mPosition = 0;
                }
                mTrack.stop(); // allows remaining data to play out
            }
        }

        @Override
        public void release() {
            super.release();
            if (mTrack != null) {
                final long remainingMs = mFinishAtMs - System.currentTimeMillis();
                if (remainingMs > 0) {
                    try {
                        Thread.sleep(remainingMs);
                    } catch (InterruptedException e) {
                        ;
                    }
                }
                mTrack.release();
                mTrack = null;
            }
        }

        public AudioTrack mTrack;
        private final static String TAG = "AudioRecordAudit";
        private int mPosition;
        private long mFinishAtMs;
    }

    /* AudioRecordAudit extends AudioRecord to allow concurrent playback
     * of read content to an AudioTrack.  This is for testing only.
     * For general applications, it is NOT recommended to extend AudioRecord.
     * This affects AudioRecord timing.
     */
    public static class AudioRecordAuditNative extends AudioRecordNative {
        public AudioRecordAuditNative() {
            super();
            // Caution: delayMs too large results in buffer sizes that cannot be created.
            mTrack = new AudioTrackNative();
        }

        @Override
        public boolean open(int numChannels, int sampleRate, boolean useFloat, int numBuffers) {
            if (super.open(numChannels, sampleRate, useFloat, numBuffers)) {
                if (!mTrack.open(numChannels, sampleRate, useFloat, 2 /* numBuffers */)) {
                    mTrack = null; // remove track
                }
                return true;
            }
            return false;
        }

        @Override
        public void close() {
            super.close();
            if (mTrack != null) {
                mTrack.close();
            }
        }

        @Override
        public boolean start() {
            if (super.start()) {
                if (mTrack != null) {
                    mTrack.start();
                }
                return true;
            }
            return false;
        }

        @Override
        public boolean stop() {
            if (super.stop()) {
                if (mTrack != null) {
                    mTrack.stop(); // doesn't allow remaining data to play out
                }
                return true;
            }
            return false;
        }

        @Override
        public int read(short[] audioData, int offsetInShorts, int sizeInShorts, int readFlags) {
            int samples = super.read(audioData, offsetInShorts, sizeInShorts, readFlags);
            if (mTrack != null) {
                Assert.assertEquals(samples, mTrack.write(audioData, offsetInShorts, samples,
                        AudioTrackNative.WRITE_FLAG_BLOCKING));
                mPosition += samples / mTrack.getChannelCount();
            }
            return samples;
        }

        @Override
        public int read(float[] audioData, int offsetInFloats, int sizeInFloats, int readFlags) {
            int samples = super.read(audioData, offsetInFloats, sizeInFloats, readFlags);
            if (mTrack != null) {
                Assert.assertEquals(samples, mTrack.write(audioData, offsetInFloats, samples,
                        AudioTrackNative.WRITE_FLAG_BLOCKING));
                mPosition += samples / mTrack.getChannelCount();
            }
            return samples;
        }

        public AudioTrackNative mTrack;
        private final static String TAG = "AudioRecordAuditNative";
        private int mPosition;
    }
}
