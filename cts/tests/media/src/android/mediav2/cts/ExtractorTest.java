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

package android.mediav2.cts;

import android.content.Context;
import android.content.res.AssetFileDescriptor;
import android.media.MediaCodec;
import android.media.MediaDataSource;
import android.media.MediaExtractor;
import android.media.MediaFormat;
import android.net.Uri;
import android.os.ParcelFileDescriptor;
import android.os.PersistableBundle;
import android.util.Log;

import androidx.test.filters.LargeTest;
import androidx.test.filters.SmallTest;
import androidx.test.platform.app.InstrumentationRegistry;

import org.junit.After;
import org.junit.Before;
import org.junit.Ignore;
import org.junit.Rule;
import org.junit.Test;
import org.junit.experimental.runners.Enclosed;
import org.junit.rules.TestName;
import org.junit.runner.RunWith;
import org.junit.runners.Parameterized;

import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.nio.ByteBuffer;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collection;
import java.util.Collections;
import java.util.List;
import java.util.Random;

import static org.junit.Assert.assertTrue;
import static org.junit.Assert.fail;
import static org.junit.Assume.assumeTrue;

class TestMediaDataSource extends MediaDataSource {
    private static final String LOG_TAG = TestMediaDataSource.class.getSimpleName();
    private static final boolean ENABLE_LOGS = false;
    private byte[] mData;
    private boolean mFatalGetSize;
    private boolean mFatalReadAt;
    private boolean mIsClosed = false;

    static TestMediaDataSource fromString(String inpPath, boolean failSize, boolean failRead)
            throws IOException {
        try (FileInputStream fInp = new FileInputStream(inpPath)) {
            int size = (int) new File(inpPath).length();
            byte[] data = new byte[size];
            fInp.read(data, 0, size);
            return new TestMediaDataSource(data, failSize, failRead);
        }
    }

    private TestMediaDataSource(byte[] data, boolean fatalGetSize, boolean fatalReadAt) {
        mData = data;
        mFatalGetSize = fatalGetSize;
        mFatalReadAt = fatalReadAt;
    }

    @Override
    public synchronized int readAt(long srcOffset, byte[] buffer, int dstOffset, int size)
            throws IOException {
        if (mFatalReadAt) {
            throw new IOException("malformed media data source");
        }
        if (srcOffset >= mData.length) {
            return -1;
        }
        if (srcOffset + size > mData.length) {
            size = mData.length - (int) srcOffset;
        }
        System.arraycopy(mData, (int) srcOffset, buffer, dstOffset, size);
        return size;
    }

    @Override
    public synchronized long getSize() throws IOException {
        if (mFatalGetSize) {
            throw new IOException("malformed media data source");
        }
        if (ENABLE_LOGS) {
            Log.v(LOG_TAG, "getSize: " + mData.length);
        }
        return mData.length;
    }

    @Override
    public synchronized void close() {
        mIsClosed = true;
    }

    public boolean isClosed() {
        return mIsClosed;
    }
}

@RunWith(Enclosed.class)
public class ExtractorTest {
    private static final String LOG_TAG = ExtractorTest.class.getSimpleName();
    private static final boolean ENABLE_LOGS = false;
    private static final int MAX_SAMPLE_SIZE = 4 * 1024 * 1024;
    private static final String EXT_SEL_KEY = "ext-sel";
    static private final List<String> codecListforTypeMp4 =
            Arrays.asList(MediaFormat.MIMETYPE_AUDIO_MPEG, MediaFormat.MIMETYPE_AUDIO_AAC,
                    MediaFormat.MIMETYPE_AUDIO_FLAC, MediaFormat.MIMETYPE_AUDIO_VORBIS,
                    MediaFormat.MIMETYPE_AUDIO_OPUS, MediaFormat.MIMETYPE_VIDEO_MPEG2,
                    MediaFormat.MIMETYPE_VIDEO_MPEG4, MediaFormat.MIMETYPE_VIDEO_H263,
                    MediaFormat.MIMETYPE_VIDEO_AVC, MediaFormat.MIMETYPE_VIDEO_HEVC);
    static private final List<String> codecListforTypeWebm =
            Arrays.asList(MediaFormat.MIMETYPE_AUDIO_VORBIS, MediaFormat.MIMETYPE_AUDIO_OPUS,
                    MediaFormat.MIMETYPE_VIDEO_VP8, MediaFormat.MIMETYPE_VIDEO_VP9);
    static private final List<String> codecListforType3gp =
            Arrays.asList(MediaFormat.MIMETYPE_AUDIO_AAC, MediaFormat.MIMETYPE_AUDIO_AMR_NB,
                    MediaFormat.MIMETYPE_AUDIO_AMR_WB, MediaFormat.MIMETYPE_VIDEO_MPEG4,
                    MediaFormat.MIMETYPE_VIDEO_H263, MediaFormat.MIMETYPE_VIDEO_AVC);
    static private final List<String> codecListforTypeMkv =
            Arrays.asList(MediaFormat.MIMETYPE_AUDIO_MPEG, MediaFormat.MIMETYPE_AUDIO_AAC,
                    MediaFormat.MIMETYPE_AUDIO_FLAC, MediaFormat.MIMETYPE_AUDIO_VORBIS,
                    MediaFormat.MIMETYPE_AUDIO_OPUS, MediaFormat.MIMETYPE_VIDEO_MPEG2,
                    MediaFormat.MIMETYPE_VIDEO_MPEG4, MediaFormat.MIMETYPE_VIDEO_H263,
                    MediaFormat.MIMETYPE_VIDEO_AVC, MediaFormat.MIMETYPE_VIDEO_HEVC,
                    MediaFormat.MIMETYPE_VIDEO_VP8, MediaFormat.MIMETYPE_VIDEO_VP9);
    static private final List<String> codecListforTypeOgg =
            Arrays.asList(MediaFormat.MIMETYPE_AUDIO_VORBIS, MediaFormat.MIMETYPE_AUDIO_OPUS);
    static private final List<String> codecListforTypeTs =
            Arrays.asList(MediaFormat.MIMETYPE_AUDIO_AAC, MediaFormat.MIMETYPE_VIDEO_MPEG2,
                    MediaFormat.MIMETYPE_VIDEO_AVC);
    static private final List<String> codecListforTypeRaw =
            Arrays.asList(MediaFormat.MIMETYPE_AUDIO_AAC, MediaFormat.MIMETYPE_AUDIO_FLAC,
                    MediaFormat.MIMETYPE_AUDIO_MPEG, MediaFormat.MIMETYPE_AUDIO_AMR_NB,
                    MediaFormat.MIMETYPE_AUDIO_AMR_WB, MediaFormat.MIMETYPE_AUDIO_RAW);
    // List of codecs that are not required to be supported as per CDD but are tested
    static private final List<String> codecListSupp =
            Arrays.asList(MediaFormat.MIMETYPE_VIDEO_AV1);
    private static String mInpPrefix = WorkDir.getMediaDirString();
    private static String extSel;

    static {
        android.os.Bundle args = InstrumentationRegistry.getArguments();
        final String defSel = "mp4;webm;3gp;mkv;ogg;supp";
        extSel = (null == args.getString(EXT_SEL_KEY)) ? defSel : args.getString(EXT_SEL_KEY);
    }

    static private boolean shouldRunTest(String mime) {
        boolean result = false;
        if ((extSel.contains("mp4") && codecListforTypeMp4.contains(mime)) ||
                (extSel.contains("webm") && codecListforTypeWebm.contains(mime)) ||
                (extSel.contains("3gp") && codecListforType3gp.contains(mime)) ||
                (extSel.contains("mkv") && codecListforTypeMkv.contains(mime)) ||
                (extSel.contains("ogg") && codecListforTypeOgg.contains(mime)) ||
                (extSel.contains("supp") && codecListSupp.contains(mime)))
            result = true;
        return result;
    }

    private static boolean isExtractorOKonEOS(MediaExtractor extractor) {
        return extractor.getSampleTrackIndex() < 0 && extractor.getSampleSize() < 0 &&
                extractor.getSampleFlags() < 0 && extractor.getSampleTime() < 0;
    }

    private static boolean isSampleInfoIdentical(MediaCodec.BufferInfo refSample,
            MediaCodec.BufferInfo testSample) {
        return refSample.flags == testSample.flags && refSample.size == testSample.size &&
                refSample.presentationTimeUs == testSample.presentationTimeUs;
    }

    private static boolean isSampleInfoValidAndIdentical(MediaCodec.BufferInfo refSample,
            MediaCodec.BufferInfo testSample) {
        return refSample.flags == testSample.flags && refSample.size == testSample.size &&
                Math.abs(refSample.presentationTimeUs - testSample.presentationTimeUs) <= 1 &&
                refSample.flags >= 0 && refSample.size >= 0 && refSample.presentationTimeUs >= 0;
    }

    static boolean isCSDIdentical(MediaFormat refFormat, MediaFormat testFormat) {
        String mime = refFormat.getString(MediaFormat.KEY_MIME);
        for (int i = 0; ; i++) {
            String csdKey = "csd-" + i;
            boolean refHasCSD = refFormat.containsKey(csdKey);
            boolean testHasCSD = testFormat.containsKey(csdKey);
            if (refHasCSD != testHasCSD) {
                if (ENABLE_LOGS) {
                    Log.w(LOG_TAG, "error, ref fmt has CSD: " + refHasCSD + " test fmt has CSD: " +
                            testHasCSD);
                }
                return false;
            }
            if (refHasCSD) {
                Log.v(LOG_TAG, mime + " has " + csdKey);
                ByteBuffer r = refFormat.getByteBuffer(csdKey);
                ByteBuffer t = testFormat.getByteBuffer(csdKey);
                if (!r.equals(t)) {
                    if (ENABLE_LOGS) {
                        Log.w(LOG_TAG, "ref CSD and test CSD buffers are not identical");
                    }
                    return false;
                }
            } else break;
        }
        return true;
    }

    private static boolean isFormatSimilar(MediaFormat refFormat, MediaFormat testFormat) {
        String refMime = refFormat.getString(MediaFormat.KEY_MIME);
        String testMime = testFormat.getString(MediaFormat.KEY_MIME);

        if (!refMime.equals(testMime)) return false;
        if (!isCSDIdentical(refFormat, testFormat)) return false;
        if (refMime.startsWith("audio/")) {
            return refFormat.getInteger(MediaFormat.KEY_CHANNEL_COUNT) ==
                    testFormat.getInteger(MediaFormat.KEY_CHANNEL_COUNT) &&
                    refFormat.getInteger(MediaFormat.KEY_SAMPLE_RATE) ==
                            testFormat.getInteger(MediaFormat.KEY_SAMPLE_RATE);
        } else if (refMime.startsWith("video/")) {
            return refFormat.getInteger(MediaFormat.KEY_WIDTH) ==
                    testFormat.getInteger(MediaFormat.KEY_WIDTH) &&
                    refFormat.getInteger(MediaFormat.KEY_HEIGHT) ==
                            testFormat.getInteger(MediaFormat.KEY_HEIGHT);
        }
        return true;
    }

    private static boolean isMediaSimilar(MediaExtractor refExtractor, MediaExtractor testExtractor,
            String mime, int sampleLimit) {
        ByteBuffer refBuffer = ByteBuffer.allocate(MAX_SAMPLE_SIZE);
        ByteBuffer testBuffer = ByteBuffer.allocate(MAX_SAMPLE_SIZE);

        int noOfTracksMatched = 0;
        for (int refTrackID = 0; refTrackID < refExtractor.getTrackCount(); refTrackID++) {
            MediaFormat refFormat = refExtractor.getTrackFormat(refTrackID);
            String refMime = refFormat.getString(MediaFormat.KEY_MIME);
            if (mime != null && !refMime.equals(mime)) {
                continue;
            }
            for (int testTrackID = 0; testTrackID < testExtractor.getTrackCount(); testTrackID++) {
                MediaFormat testFormat = testExtractor.getTrackFormat(testTrackID);
                if (!isFormatSimilar(refFormat, testFormat)) {
                    continue;
                }
                refExtractor.selectTrack(refTrackID);
                testExtractor.selectTrack(testTrackID);

                MediaCodec.BufferInfo refSampleInfo = new MediaCodec.BufferInfo();
                MediaCodec.BufferInfo testSampleInfo = new MediaCodec.BufferInfo();
                boolean areTracksIdentical = true;
                for (int frameCount = 0; ; frameCount++) {
                    refSampleInfo.set(0, (int) refExtractor.getSampleSize(),
                            refExtractor.getSampleTime(), refExtractor.getSampleFlags());
                    testSampleInfo.set(0, (int) testExtractor.getSampleSize(),
                            testExtractor.getSampleTime(), testExtractor.getSampleFlags());
                    if (!isSampleInfoValidAndIdentical(refSampleInfo, testSampleInfo)) {
                        if (ENABLE_LOGS) {
                            Log.d(LOG_TAG,
                                    " Mime: " + refMime + " mismatch for sample: " + frameCount);
                            Log.d(LOG_TAG, " flags exp/got: " +
                                    refSampleInfo.flags + '/' + testSampleInfo.flags);
                            Log.d(LOG_TAG, " size exp/got: " +
                                    refSampleInfo.size + '/' + testSampleInfo.size);
                            Log.d(LOG_TAG, " ts exp/got: " + refSampleInfo.presentationTimeUs +
                                    '/' + testSampleInfo.presentationTimeUs);
                        }
                        areTracksIdentical = false;
                        break;
                    }
                    int refSz = refExtractor.readSampleData(refBuffer, 0);
                    if (refSz != refSampleInfo.size) {
                        if (ENABLE_LOGS) {
                            Log.d(LOG_TAG, "Mime: " + refMime + " Size exp/got: " +
                                    refSampleInfo.size + '/' + refSz);
                        }
                        areTracksIdentical = false;
                        break;
                    }
                    int testSz = testExtractor.readSampleData(testBuffer, 0);
                    if (testSz != testSampleInfo.size) {
                        if (ENABLE_LOGS) {
                            Log.d(LOG_TAG, "Mime: " + refMime + " Size exp/got: " +
                                    testSampleInfo.size + '/' + testSz);
                        }
                        areTracksIdentical = false;
                        break;
                    }
                    int trackIndex = refExtractor.getSampleTrackIndex();
                    if (trackIndex != refTrackID) {
                        if (ENABLE_LOGS) {
                            Log.d(LOG_TAG, "Mime: " + refMime +
                                    " TrackID exp/got: " + refTrackID + '/' + trackIndex);
                        }
                        areTracksIdentical = false;
                        break;
                    }
                    trackIndex = testExtractor.getSampleTrackIndex();
                    if (trackIndex != testTrackID) {
                        if (ENABLE_LOGS) {
                            Log.d(LOG_TAG, "Mime: " + refMime +
                                    " TrackID exp/got: " + testTrackID + '/' + trackIndex);
                        }
                        areTracksIdentical = false;
                        break;
                    }
                    if (!testBuffer.equals(refBuffer)) {
                        if (ENABLE_LOGS) {
                            Log.d(LOG_TAG, "Mime: " + refMime + " sample data is not identical");
                        }
                        areTracksIdentical = false;
                        break;
                    }
                    boolean haveRefSamples = refExtractor.advance();
                    boolean haveTestSamples = testExtractor.advance();
                    if (haveRefSamples != haveTestSamples) {
                        if (ENABLE_LOGS) {
                            Log.d(LOG_TAG, "Mime: " + refMime + " Mismatch in sampleCount");
                        }
                        areTracksIdentical = false;
                        break;
                    }

                    if (!haveRefSamples && !isExtractorOKonEOS(refExtractor)) {
                        if (ENABLE_LOGS) {
                            Log.d(LOG_TAG, "Mime: " + refMime + " calls post advance() are not OK");
                        }
                        areTracksIdentical = false;
                        break;
                    }
                    if (!haveTestSamples && !isExtractorOKonEOS(testExtractor)) {
                        if (ENABLE_LOGS) {
                            Log.d(LOG_TAG, "Mime: " + refMime + " calls post advance() are not OK");
                        }
                        areTracksIdentical = false;
                        break;
                    }
                    if (ENABLE_LOGS) {
                        Log.v(LOG_TAG, "Mime: " + refMime + " Sample: " + frameCount +
                                " flags: " + refSampleInfo.flags +
                                " size: " + refSampleInfo.size +
                                " ts: " + refSampleInfo.presentationTimeUs);
                    }
                    if (!haveRefSamples || frameCount >= sampleLimit) {
                        break;
                    }
                }
                testExtractor.unselectTrack(testTrackID);
                refExtractor.unselectTrack(refTrackID);
                if (areTracksIdentical) {
                    noOfTracksMatched++;
                    break;
                }
            }
            if (mime != null && noOfTracksMatched > 0) break;
        }
        if (mime == null) {
            return noOfTracksMatched == refExtractor.getTrackCount();
        } else {
            return noOfTracksMatched > 0;
        }
    }

    /**
     * Tests setDataSource(...) Api by observing the extractor behavior after its successful
     * instantiation using a media stream.
     */
    @SmallTest
    public static class SetDataSourceTest {
        @Rule
        public TestName testName = new TestName();

        private static final String mInpMedia = "ForBiggerEscapes.mp4";
        private static final String mInpMediaUrl =
                "http://commondatastorage.googleapis.com/gtv-videos-bucket/sample/ForBiggerEscapes.mp4";

        private MediaExtractor mRefExtractor;

        static {
            System.loadLibrary("ctsmediav2extractor_jni");
        }

        @Before
        public void setUp() throws IOException {
            mRefExtractor = new MediaExtractor();
            mRefExtractor.setDataSource(mInpPrefix + mInpMedia);
        }

        @After
        public void tearDown() {
            mRefExtractor.release();
            mRefExtractor = null;
        }

        private static boolean areMetricsIdentical(MediaExtractor refExtractor,
                MediaExtractor testExtractor) {
            PersistableBundle bundle = refExtractor.getMetrics();
            int refNumTracks = bundle.getInt(MediaExtractor.MetricsConstants.TRACKS);
            String refFormat = bundle.getString(MediaExtractor.MetricsConstants.FORMAT);
            String refMime = bundle.getString(MediaExtractor.MetricsConstants.MIME_TYPE);
            bundle = testExtractor.getMetrics();
            int testNumTracks = bundle.getInt(MediaExtractor.MetricsConstants.TRACKS);
            String testFormat = bundle.getString(MediaExtractor.MetricsConstants.FORMAT);
            String testMime = bundle.getString(MediaExtractor.MetricsConstants.MIME_TYPE);
            boolean result = testNumTracks == refNumTracks && testFormat.equals(refFormat) &&
                    testMime.equals(refMime);
            if (ENABLE_LOGS) {
                Log.d(LOG_TAG, " NumTracks exp/got: " + refNumTracks + '/' + testNumTracks);
                Log.d(LOG_TAG, " Format exp/got: " + refFormat + '/' + testFormat);
                Log.d(LOG_TAG, " Mime exp/got: " + refMime + '/' + testMime);
            }
            return result;
        }

        private static boolean isSeekOk(MediaExtractor refExtractor, MediaExtractor testExtractor) {
            final long maxEstDuration = 14000000;
            final int MAX_SEEK_POINTS = 7;
            final long mSeed = 0x12b9b0a1;
            final Random randNum = new Random(mSeed);
            MediaCodec.BufferInfo refSampleInfo = new MediaCodec.BufferInfo();
            MediaCodec.BufferInfo testSampleInfo = new MediaCodec.BufferInfo();
            boolean result = true;
            for (int trackID = 0; trackID < refExtractor.getTrackCount() && result; trackID++) {
                refExtractor.selectTrack(trackID);
                testExtractor.selectTrack(trackID);
                for (int i = 0; i < MAX_SEEK_POINTS && result; i++) {
                    long pts = (long) (randNum.nextDouble() * maxEstDuration);
                    for (int mode = MediaExtractor.SEEK_TO_PREVIOUS_SYNC;
                         mode <= MediaExtractor.SEEK_TO_CLOSEST_SYNC; mode++) {
                        refExtractor.seekTo(pts, mode);
                        testExtractor.seekTo(pts, mode);
                        refSampleInfo.set(0, (int) refExtractor.getSampleSize(),
                                refExtractor.getSampleTime(), refExtractor.getSampleFlags());
                        testSampleInfo.set(0, (int) testExtractor.getSampleSize(),
                                testExtractor.getSampleTime(), testExtractor.getSampleFlags());
                        result = isSampleInfoIdentical(refSampleInfo, testSampleInfo);
                        int refTrackIdx = refExtractor.getSampleTrackIndex();
                        int testTrackIdx = testExtractor.getSampleTrackIndex();
                        result &= (refTrackIdx == testTrackIdx);
                        if (ENABLE_LOGS) {
                            Log.d(LOG_TAG, " mode/pts/trackId:" + mode + "/" + pts + "/" + trackID);
                            Log.d(LOG_TAG, " trackId exp/got: " + refTrackIdx + '/' + testTrackIdx);
                            Log.d(LOG_TAG, " flags exp/got: " +
                                    refSampleInfo.flags + '/' + testSampleInfo.flags);
                            Log.d(LOG_TAG, " size exp/got: " +
                                    refSampleInfo.size + '/' + testSampleInfo.size);
                            Log.d(LOG_TAG, " ts exp/got: " + refSampleInfo.presentationTimeUs +
                                    '/' + testSampleInfo.presentationTimeUs);
                        }
                    }
                }
                refExtractor.unselectTrack(trackID);
                testExtractor.unselectTrack(trackID);
            }
            return result;
        }

        @Test
        public void testAssetFD() throws IOException {
            File inpFile = new File(mInpPrefix + mInpMedia);
            MediaExtractor testExtractor = new MediaExtractor();
            try (ParcelFileDescriptor parcelFD = ParcelFileDescriptor
                    .open(inpFile, ParcelFileDescriptor.MODE_READ_ONLY);
                 AssetFileDescriptor afd = new AssetFileDescriptor(parcelFD, 0,
                         AssetFileDescriptor.UNKNOWN_LENGTH)) {
                testExtractor.setDataSource(afd);
            }
            assertTrue(testExtractor.getCachedDuration() < 0);
            if (!isMediaSimilar(mRefExtractor, testExtractor, null, Integer.MAX_VALUE) ||
                    !areMetricsIdentical(mRefExtractor, testExtractor) ||
                    !isSeekOk(mRefExtractor, testExtractor)) {
                fail("setDataSource failed: " + testName.getMethodName());
            }
            testExtractor.release();
        }

        @Test
        public void testFileDescriptor() throws IOException {
            File inpFile = new File(mInpPrefix + mInpMedia);
            MediaExtractor testExtractor = new MediaExtractor();
            try (FileInputStream fInp = new FileInputStream(inpFile)) {
                testExtractor.setDataSource(fInp.getFD());
            }
            assertTrue(testExtractor.getCachedDuration() < 0);
            if (!isMediaSimilar(mRefExtractor, testExtractor, null, Integer.MAX_VALUE) ||
                    !areMetricsIdentical(mRefExtractor, testExtractor) ||
                    !isSeekOk(mRefExtractor, testExtractor)) {
                fail("setDataSource failed: " + testName.getMethodName());
            }
            testExtractor.release();
        }

        @Test
        public void testFileDescriptorLenOffset() throws IOException {
            File inpFile = new File(mInpPrefix + mInpMedia);
            File outFile = File.createTempFile("temp", ".out");
            byte[] garbageAppend = "PrefixGarbage".getBytes();
            try (FileInputStream fInp = new FileInputStream(inpFile);
                 FileOutputStream fOut = new FileOutputStream(outFile)) {
                fOut.write(garbageAppend);
                byte[] data = new byte[(int) new File(inpFile.toString()).length()];
                if (fInp.read(data) == -1) {
                    fail("Failed to read input file");
                }
                fOut.write(data);
                fOut.write(garbageAppend);
            }
            MediaExtractor testExtractor = new MediaExtractor();
            try (FileInputStream fInp = new FileInputStream(outFile)) {
                testExtractor.setDataSource(fInp.getFD(), garbageAppend.length,
                        inpFile.length());
            }
            assertTrue(testExtractor.getCachedDuration() < 0);
            if (!isMediaSimilar(mRefExtractor, testExtractor, null, Integer.MAX_VALUE) ||
                    !areMetricsIdentical(mRefExtractor, testExtractor) ||
                    !isSeekOk(mRefExtractor, testExtractor)) {
                fail("setDataSource failed: " + testName.getMethodName());
            }
            testExtractor.release();
            outFile.delete();
        }

        @Test
        public void testMediaDataSource() throws Exception {
            TestMediaDataSource dataSource =
                    TestMediaDataSource.fromString(mInpPrefix + mInpMedia, false, false);
            MediaExtractor testExtractor = new MediaExtractor();
            testExtractor.setDataSource(dataSource);
            assertTrue(testExtractor.getCachedDuration() < 0);
            if (!isMediaSimilar(mRefExtractor, testExtractor, null, Integer.MAX_VALUE) ||
                    !areMetricsIdentical(mRefExtractor, testExtractor) ||
                    !isSeekOk(mRefExtractor, testExtractor)) {
                fail("setDataSource failed: " + testName.getMethodName());
            }
            testExtractor.release();
            assertTrue(dataSource.isClosed());
        }

        @Test
        public void testContextUri() throws IOException {
            Context context = InstrumentationRegistry.getInstrumentation().getContext();
            String path = "android.resource://android.mediav2.cts/" + R.raw.forbiggerescapes;
            MediaExtractor testExtractor = new MediaExtractor();
            testExtractor.setDataSource(context, Uri.parse(path), null);
            assertTrue(testExtractor.getCachedDuration() < 0);
            if (!isMediaSimilar(mRefExtractor, testExtractor, null, Integer.MAX_VALUE) ||
                    !areMetricsIdentical(mRefExtractor, testExtractor) ||
                    !isSeekOk(mRefExtractor, testExtractor)) {
                fail("setDataSource failed: " + testName.getMethodName());
            }
            testExtractor.release();
        }

        @Test
        public void testUrlDataSource() throws Exception {
            MediaExtractor testExtractor = new MediaExtractor();
            testExtractor.setDataSource(mInpMediaUrl, null);
            if (!isMediaSimilar(mRefExtractor, testExtractor, null, Integer.MAX_VALUE) ||
                    !areMetricsIdentical(mRefExtractor, testExtractor) ||
                    !isSeekOk(mRefExtractor, testExtractor)) {
                fail("setDataSource failed: " + testName.getMethodName());
            }
            testExtractor.selectTrack(0);
            for (int idx = 0; ; idx++) {
                if ((idx & (idx - 1)) == 0) {
                    long cachedDuration = testExtractor.getCachedDuration();
                    if (ENABLE_LOGS) {
                        Log.v(LOG_TAG, "cachedDuration at frame: " + idx + " is:" + cachedDuration);
                    }
                    assertTrue("cached duration should be non-negative", cachedDuration >= 0);
                }
                if (!testExtractor.advance()) break;
            }
            assertTrue(testExtractor.hasCacheReachedEndOfStream());
            testExtractor.unselectTrack(0);
            testExtractor.release();
        }

        private native boolean nativeTestDataSource(String srcPath, String srcUrl);

        @Test
        public void testDataSourceNative() {
            assertTrue(testName.getMethodName() + " failed ",
                    nativeTestDataSource(mInpPrefix + mInpMedia, mInpMediaUrl));
        }
    }

    /**
     * Encloses extractor functionality tests
     */
    @RunWith(Parameterized.class)
    public static class FunctionalityTest {
        private static final int MAX_SEEK_POINTS = 7;
        private static final long mSeed = 0x12b9b0a1;
        private final Random mRandNum = new Random(mSeed);
        private String[] mSrcFiles;
        private String mMime;

        static {
            System.loadLibrary("ctsmediav2extractor_jni");
        }

        @Rule
        public TestName testName = new TestName();

        @Parameterized.Parameters(name = "{index}({0})")
        public static Collection<Object[]> input() {
            /* TODO(b/157108639) - add missing test files */
            return Arrays.asList(new Object[][]{
                    {MediaFormat.MIMETYPE_VIDEO_MPEG2, new String[]{
                            "bbb_cif_768kbps_30fps_mpeg2_stereo_48kHz_192kbps_mp3.mp4",
                            "bbb_cif_768kbps_30fps_mpeg2.mkv",}},
                    {MediaFormat.MIMETYPE_VIDEO_H263, new String[]{
                            "bbb_cif_768kbps_30fps_h263.mp4",
                            "bbb_cif_768kbps_30fps_h263_mono_8kHz_12kbps_amrnb.3gp",}},
                    {MediaFormat.MIMETYPE_VIDEO_MPEG4, new String[]{
                            "bbb_cif_768kbps_30fps_mpeg4_stereo_48kHz_192kbps_flac.mp4",
                            "bbb_cif_768kbps_30fps_mpeg4_mono_16kHz_20kbps_amrwb.3gp",}},
                    {MediaFormat.MIMETYPE_VIDEO_AVC, new String[]{
                            "bbb_cif_768kbps_30fps_avc_stereo_48kHz_192kbps_vorbis.mp4",
                            "bbb_cif_768kbps_30fps_avc_stereo_48kHz_192kbps_aac.mkv",
                            "bbb_cif_768kbps_30fps_avc_stereo_48kHz_192kbps_aac.3gp",}},
                    {MediaFormat.MIMETYPE_VIDEO_HEVC, new String[]{
                            "bbb_cif_768kbps_30fps_hevc_stereo_48kHz_192kbps_opus.mp4",
                            "bbb_cif_768kbps_30fps_hevc_stereo_48kHz_192kbps_mp3.mkv",}},
                    {MediaFormat.MIMETYPE_VIDEO_VP8, new String[]{
                            "bbb_cif_768kbps_30fps_vp8_stereo_48kHz_192kbps_vorbis.webm",
                            "bbb_cif_768kbps_30fps_vp8_stereo_48kHz_192kbps_vorbis.mkv"}},
                    {MediaFormat.MIMETYPE_VIDEO_VP9, new String[]{
                            "bbb_cif_768kbps_30fps_vp9_stereo_48kHz_192kbps_opus.webm",
                            "bbb_cif_768kbps_30fps_vp9_stereo_48kHz_192kbps_opus.mkv",}},
                    {MediaFormat.MIMETYPE_VIDEO_AV1, new String[]{
                            "bbb_cif_768kbps_30fps_av1.mp4",
                            "bbb_cif_768kbps_30fps_av1.webm",
                            "bbb_cif_768kbps_30fps_av1.mkv",}},
                    {MediaFormat.MIMETYPE_AUDIO_VORBIS, new String[]{
                            "bbb_cif_768kbps_30fps_avc_stereo_48kHz_192kbps_vorbis.mp4",
                            "bbb_cif_768kbps_30fps_vp8_stereo_48kHz_192kbps_vorbis.mkv",
                            "bbb_cif_768kbps_30fps_vp8_stereo_48kHz_192kbps_vorbis.webm",
                            "bbb_stereo_48kHz_192kbps_vorbis.ogg",}},
                    {MediaFormat.MIMETYPE_AUDIO_OPUS, new String[]{
                            "bbb_cif_768kbps_30fps_vp9_stereo_48kHz_192kbps_opus.webm",
                            "bbb_cif_768kbps_30fps_vp9_stereo_48kHz_192kbps_opus.mkv",
                            "bbb_cif_768kbps_30fps_hevc_stereo_48kHz_192kbps_opus.mp4",
                            "bbb_stereo_48kHz_192kbps_opus.ogg",}},
                    {MediaFormat.MIMETYPE_AUDIO_MPEG, new String[]{
                            "bbb_stereo_48kHz_192kbps_mp3.mp3",
                            "bbb_cif_768kbps_30fps_mpeg2_stereo_48kHz_192kbps_mp3.mp4",
                            "bbb_cif_768kbps_30fps_hevc_stereo_48kHz_192kbps_mp3.mkv",}},
                    {MediaFormat.MIMETYPE_AUDIO_AAC, new String[]{
                            "bbb_stereo_48kHz_192kbps_aac.mp4",
                            "bbb_cif_768kbps_30fps_avc_stereo_48kHz_192kbps_aac.3gp",
                            "bbb_cif_768kbps_30fps_avc_stereo_48kHz_192kbps_aac.mkv",}},
                    {MediaFormat.MIMETYPE_AUDIO_AMR_NB, new String[]{
                            "bbb_cif_768kbps_30fps_h263_mono_8kHz_12kbps_amrnb.3gp",
                            "bbb_mono_8kHz_12kbps_amrnb.amr",}},
                    {MediaFormat.MIMETYPE_AUDIO_AMR_WB, new String[]{
                            "bbb_cif_768kbps_30fps_mpeg4_mono_16kHz_20kbps_amrwb.3gp",
                            "bbb_mono_16kHz_20kbps_amrwb.amr"}},
                    {MediaFormat.MIMETYPE_AUDIO_FLAC, new String[]{
                            "bbb_cif_768kbps_30fps_mpeg4_stereo_48kHz_192kbps_flac.mp4",
                            "bbb_cif_768kbps_30fps_h263_stereo_48kHz_192kbps_flac.mkv",}},
            });
        }

        private native boolean nativeTestExtract(String srcPath, String refPath, String mime);

        private native boolean nativeTestSeek(String srcPath, String mime);

        private native boolean nativeTestSeekFlakiness(String srcPath, String mime);

        private native boolean nativeTestSeekToZero(String srcPath, String mime);

        private native boolean nativeTestFileFormat(String srcPath);

        public FunctionalityTest(String mime, String[] srcFiles) {
            mMime = mime;
            mSrcFiles = srcFiles;
        }

        // content necessary for testing seek are grouped in this class
        private class SeekTestParams {
            MediaCodec.BufferInfo mExpected;
            long mTimeStamp;
            int mMode;

            SeekTestParams(MediaCodec.BufferInfo expected, long timeStamp, int mode) {
                mExpected = expected;
                mTimeStamp = timeStamp;
                mMode = mode;
            }
        }

        private ArrayList<MediaCodec.BufferInfo> getSeekablePoints(String srcFile, String mime)
                throws IOException {
            ArrayList<MediaCodec.BufferInfo> bookmarks = null;
            if (mime == null) return null;
            MediaExtractor extractor = new MediaExtractor();
            extractor.setDataSource(mInpPrefix + srcFile);
            for (int trackID = 0; trackID < extractor.getTrackCount(); trackID++) {
                MediaFormat format = extractor.getTrackFormat(trackID);
                if (!mime.equals(format.getString(MediaFormat.KEY_MIME))) continue;
                extractor.selectTrack(trackID);
                bookmarks = new ArrayList<>();
                do {
                    int sampleFlags = extractor.getSampleFlags();
                    if ((sampleFlags & MediaExtractor.SAMPLE_FLAG_SYNC) != 0) {
                        MediaCodec.BufferInfo sampleInfo = new MediaCodec.BufferInfo();
                        sampleInfo.set(0, (int) extractor.getSampleSize(),
                                extractor.getSampleTime(), extractor.getSampleFlags());
                        bookmarks.add(sampleInfo);
                    }
                } while (extractor.advance());
                extractor.unselectTrack(trackID);
                break;
            }
            extractor.release();
            return bookmarks;
        }

        private ArrayList<SeekTestParams> generateSeekTestArgs(String srcFile, String mime,
                boolean isRandom) throws IOException {
            ArrayList<SeekTestParams> testArgs = new ArrayList<>();
            if (mime == null) return null;
            if (isRandom) {
                MediaExtractor extractor = new MediaExtractor();
                extractor.setDataSource(mInpPrefix + srcFile);
                final long maxEstDuration = 4000000;
                for (int trackID = 0; trackID < extractor.getTrackCount(); trackID++) {
                    MediaFormat format = extractor.getTrackFormat(trackID);
                    if (!mime.equals(format.getString(MediaFormat.KEY_MIME))) continue;
                    extractor.selectTrack(trackID);
                    for (int i = 0; i < MAX_SEEK_POINTS; i++) {
                        long pts = (long) (mRandNum.nextDouble() * maxEstDuration);
                        for (int mode = MediaExtractor.SEEK_TO_PREVIOUS_SYNC;
                             mode <= MediaExtractor.SEEK_TO_CLOSEST_SYNC; mode++) {
                            MediaCodec.BufferInfo currInfo = new MediaCodec.BufferInfo();
                            extractor.seekTo(pts, mode);
                            currInfo.set(0, (int) extractor.getSampleSize(),
                                    extractor.getSampleTime(), extractor.getSampleFlags());
                            testArgs.add(new SeekTestParams(currInfo, pts, mode));
                        }
                    }
                    extractor.unselectTrack(trackID);
                    break;
                }
                extractor.release();
            } else {
                ArrayList<MediaCodec.BufferInfo> bookmarks = getSeekablePoints(srcFile, mime);
                if (bookmarks == null) return null;
                int size = bookmarks.size();
                int[] indices;
                if (size > MAX_SEEK_POINTS) {
                    indices = new int[MAX_SEEK_POINTS];
                    indices[0] = 0;
                    indices[MAX_SEEK_POINTS - 1] = size - 1;
                    for (int i = 1; i < MAX_SEEK_POINTS - 1; i++) {
                        indices[i] = (int) (mRandNum.nextDouble() * (MAX_SEEK_POINTS - 1) + 1);
                    }
                } else {
                    indices = new int[size];
                    for (int i = 0; i < size; i++) indices[i] = i;
                }
                // closest sync : Seek to the sync sample CLOSEST to the specified time
                // previous sync : Seek to a sync sample AT or AFTER the specified time
                // next sync : Seek to a sync sample AT or BEFORE the specified time
                for (int i : indices) {
                    MediaCodec.BufferInfo currInfo = bookmarks.get(i);
                    long pts = currInfo.presentationTimeUs;
                    testArgs.add(
                            new SeekTestParams(currInfo, pts, MediaExtractor.SEEK_TO_CLOSEST_SYNC));
                    testArgs.add(
                            new SeekTestParams(currInfo, pts, MediaExtractor.SEEK_TO_NEXT_SYNC));
                    testArgs.add(
                            new SeekTestParams(currInfo, pts,
                                    MediaExtractor.SEEK_TO_PREVIOUS_SYNC));
                    if (i > 0) {
                        MediaCodec.BufferInfo prevInfo = bookmarks.get(i - 1);
                        long ptsMinus = prevInfo.presentationTimeUs;
                        ptsMinus = pts - ((pts - ptsMinus) >> 3);
                        testArgs.add(new SeekTestParams(currInfo, ptsMinus,
                                MediaExtractor.SEEK_TO_CLOSEST_SYNC));
                        testArgs.add(new SeekTestParams(currInfo, ptsMinus,
                                MediaExtractor.SEEK_TO_NEXT_SYNC));
                        testArgs.add(new SeekTestParams(prevInfo, ptsMinus,
                                MediaExtractor.SEEK_TO_PREVIOUS_SYNC));
                    }
                    if (i < size - 1) {
                        MediaCodec.BufferInfo nextInfo = bookmarks.get(i + 1);
                        long ptsPlus = nextInfo.presentationTimeUs;
                        ptsPlus = pts + ((ptsPlus - pts) >> 3);
                        testArgs.add(new SeekTestParams(currInfo, ptsPlus,
                                MediaExtractor.SEEK_TO_CLOSEST_SYNC));
                        testArgs.add(new SeekTestParams(nextInfo, ptsPlus,
                                MediaExtractor.SEEK_TO_NEXT_SYNC));
                        testArgs.add(new SeekTestParams(currInfo, ptsPlus,
                                MediaExtractor.SEEK_TO_PREVIOUS_SYNC));
                    }
                }
            }
            return testArgs;
        }

        int checkSeekPoints(String srcFile, String mime,
                ArrayList<SeekTestParams> seekTestArgs) throws IOException {
            int errCnt = 0;
            MediaExtractor extractor = new MediaExtractor();
            extractor.setDataSource(mInpPrefix + srcFile);
            for (int trackID = 0; trackID < extractor.getTrackCount(); trackID++) {
                MediaFormat format = extractor.getTrackFormat(trackID);
                if (!format.getString(MediaFormat.KEY_MIME).equals(mime)) continue;
                extractor.selectTrack(trackID);
                MediaCodec.BufferInfo received = new MediaCodec.BufferInfo();
                for (SeekTestParams arg : seekTestArgs) {
                    extractor.seekTo(arg.mTimeStamp, arg.mMode);
                    received.set(0, (int) extractor.getSampleSize(), extractor.getSampleTime(),
                            extractor.getSampleFlags());
                    if (!isSampleInfoIdentical(arg.mExpected, received)) {
                        errCnt++;
                        if (ENABLE_LOGS) {
                            Log.d(LOG_TAG, " flags exp/got: " + arg.mExpected.flags + '/' +
                                    received.flags);
                            Log.d(LOG_TAG,
                                    " size exp/got: " + arg.mExpected.size + '/' + received.size);
                            Log.d(LOG_TAG,
                                    " ts exp/got: " + arg.mExpected.presentationTimeUs + '/' +
                                            received.presentationTimeUs);
                        }
                    }
                }
                extractor.unselectTrack(trackID);
                break;
            }
            extractor.release();
            return errCnt;
        }

        /**
         * Audio, Video codecs support a variety of file-types/container formats. For example,
         * Vorbis supports OGG, MP4, WEBM and MKV. H.263 supports 3GPP, WEBM and MKV. For every
         * mime, a list of test vectors are provided one for each container) but underlying
         * elementary stream is the same for all. The streams of a mime are extracted and
         * compared with each other for similarity.
         */
        @LargeTest
        @Test
        public void testExtract() throws IOException {
            assumeTrue(shouldRunTest(mMime));
            assertTrue(mSrcFiles.length > 1);
            assumeTrue("TODO(b/146925481)", !mMime.equals(MediaFormat.MIMETYPE_AUDIO_VORBIS));
            assumeTrue("TODO(b/146925481)", !mMime.equals(MediaFormat.MIMETYPE_AUDIO_OPUS));
            assumeTrue("TODO(b/146925481)", !mMime.equals(MediaFormat.MIMETYPE_AUDIO_MPEG));
            assumeTrue("TODO(b/146925481)", !mMime.equals(MediaFormat.MIMETYPE_AUDIO_AAC));
            MediaExtractor refExtractor = new MediaExtractor();
            refExtractor.setDataSource(mInpPrefix + mSrcFiles[0]);
            boolean isOk = true;
            for (int i = 1; i < mSrcFiles.length && isOk; i++) {
                MediaExtractor testExtractor = new MediaExtractor();
                testExtractor.setDataSource(mInpPrefix + mSrcFiles[i]);
                if (!isMediaSimilar(refExtractor, testExtractor, mMime, Integer.MAX_VALUE)) {
                    if (ENABLE_LOGS) {
                        Log.d(LOG_TAG, "Files: " + mSrcFiles[0] + ", " + mSrcFiles[i] +
                                " are different from extractor perspective");
                    }
                    if (!codecListSupp.contains(mMime)) {
                        isOk = false;
                    }
                }
                testExtractor.release();
            }
            refExtractor.release();
            assertTrue(testName.getMethodName() + " failed for mime: " + mMime, isOk);
        }

        /**
         * Tests seek functionality, verifies if we seek to most accurate point for a given
         * choice of timestamp and mode.
         */
        @LargeTest
        @Test
        @Ignore("TODO(b/146420831)")
        public void testSeek() throws IOException {
            assumeTrue(shouldRunTest(mMime));
            boolean isOk = true;
            for (String srcFile : mSrcFiles) {
                ArrayList<SeekTestParams> seekTestArgs =
                        generateSeekTestArgs(srcFile, mMime, false);
                assertTrue("Mime is null.", seekTestArgs != null);
                assertTrue("No sync samples found.", !seekTestArgs.isEmpty());
                Collections.shuffle(seekTestArgs, mRandNum);
                int seekAccErrCnt = checkSeekPoints(srcFile, mMime, seekTestArgs);
                if (seekAccErrCnt != 0) {
                    if (ENABLE_LOGS) {
                        Log.d(LOG_TAG, "For " + srcFile + " seek chose inaccurate Sync point in: " +
                                seekAccErrCnt + "/" + seekTestArgs.size());
                    }
                    if (!codecListSupp.contains(mMime)) {
                        isOk = false;
                        break;
                    }
                }
            }
            assertTrue(testName.getMethodName() + " failed for mime: " + mMime, isOk);
        }

        /**
         * Tests if we get the same content each time after a call to seekto;
         */
        @LargeTest
        @Test
        public void testSeekFlakiness() throws IOException {
            assumeTrue(shouldRunTest(mMime));
            boolean isOk = true;
            for (String srcFile : mSrcFiles) {
                ArrayList<SeekTestParams> seekTestArgs = generateSeekTestArgs(srcFile, mMime, true);
                assertTrue("Mime is null.", seekTestArgs != null);
                assertTrue("No samples found.", !seekTestArgs.isEmpty());
                Collections.shuffle(seekTestArgs, mRandNum);
                int flakyErrCnt = checkSeekPoints(srcFile, mMime, seekTestArgs);
                if (flakyErrCnt != 0) {
                    if (ENABLE_LOGS) {
                        Log.d(LOG_TAG,
                                "No. of Samples where seek showed flakiness is: " + flakyErrCnt);
                    }
                    if (!codecListSupp.contains(mMime)) {
                        isOk = false;
                        break;
                    }
                }
            }
            assertTrue(testName.getMethodName() + " failed for mime: " + mMime, isOk);
        }

        /**
         * Test if seekTo(0) yields the same content as if we had just opened the file and started
         * reading.
         */
        @SmallTest
        @Test
        public void testSeekToZero() throws IOException {
            assumeTrue(shouldRunTest(mMime));
            assumeTrue("TODO(b/146925481)", !mMime.equals(MediaFormat.MIMETYPE_AUDIO_VORBIS));
            assumeTrue("TODO(b/146925481)", !mMime.equals(MediaFormat.MIMETYPE_AUDIO_MPEG));
            assumeTrue("TODO(b/146925481)", !mMime.equals(MediaFormat.MIMETYPE_AUDIO_AAC));
            boolean isOk = true;
            for (String srcFile : mSrcFiles) {
                MediaExtractor extractor = new MediaExtractor();
                extractor.setDataSource(mInpPrefix + srcFile);
                MediaCodec.BufferInfo sampleInfoAtZero = new MediaCodec.BufferInfo();
                MediaCodec.BufferInfo currInfo = new MediaCodec.BufferInfo();
                final long randomSeekPts = 1 << 20;
                for (int trackID = 0; trackID < extractor.getTrackCount(); trackID++) {
                    MediaFormat format = extractor.getTrackFormat(trackID);
                    if (!mMime.equals(format.getString(MediaFormat.KEY_MIME))) continue;
                    extractor.selectTrack(trackID);
                    sampleInfoAtZero.set(0, (int) extractor.getSampleSize(),
                            extractor.getSampleTime(), extractor.getSampleFlags());
                    extractor.seekTo(randomSeekPts, MediaExtractor.SEEK_TO_NEXT_SYNC);
                    extractor.seekTo(0, MediaExtractor.SEEK_TO_CLOSEST_SYNC);
                    currInfo.set(0, (int) extractor.getSampleSize(),
                            extractor.getSampleTime(), extractor.getSampleFlags());
                    if (!isSampleInfoIdentical(sampleInfoAtZero, currInfo)) {
                        if (!codecListSupp.contains(mMime)) {
                            if (ENABLE_LOGS) {
                                Log.d(LOG_TAG, "seen mismatch seekTo(0, SEEK_TO_CLOSEST_SYNC)");
                                Log.d(LOG_TAG, " flags exp/got: " + sampleInfoAtZero.flags + '/' +
                                        currInfo.flags);
                                Log.d(LOG_TAG, " size exp/got: " + sampleInfoAtZero.size + '/' +
                                        currInfo.size);
                                Log.d(LOG_TAG,
                                        " ts exp/got: " + sampleInfoAtZero.presentationTimeUs +
                                                '/' + currInfo.presentationTimeUs);
                            }
                            isOk = false;
                            break;
                        }
                    }
                    extractor.seekTo(-1L, MediaExtractor.SEEK_TO_CLOSEST_SYNC);
                    currInfo.set(0, (int) extractor.getSampleSize(),
                            extractor.getSampleTime(), extractor.getSampleFlags());
                    if (!isSampleInfoIdentical(sampleInfoAtZero, currInfo)) {
                        if (!codecListSupp.contains(mMime)) {
                            if (ENABLE_LOGS) {
                                Log.d(LOG_TAG, "seen mismatch seekTo(-1, SEEK_TO_CLOSEST_SYNC)");
                                Log.d(LOG_TAG, " flags exp/got: " + sampleInfoAtZero.flags + '/' +
                                        currInfo.flags);
                                Log.d(LOG_TAG, " size exp/got: " + sampleInfoAtZero.size + '/' +
                                        currInfo.size);
                                Log.d(LOG_TAG,
                                        " ts exp/got: " + sampleInfoAtZero.presentationTimeUs +
                                                '/' + currInfo.presentationTimeUs);
                            }
                            isOk = false;
                            break;
                        }
                    }
                    extractor.unselectTrack(trackID);
                }
                extractor.release();
            }
            assertTrue(testName.getMethodName() + " failed for mime: " + mMime, isOk);
        }

        @SmallTest
        @Test
        public void testMetrics() throws IOException {
            assumeTrue(shouldRunTest(mMime));
            for (String srcFile : mSrcFiles) {
                MediaExtractor extractor = new MediaExtractor();
                extractor.setDataSource(mInpPrefix + srcFile);
                PersistableBundle bundle = extractor.getMetrics();
                int numTracks = bundle.getInt(MediaExtractor.MetricsConstants.TRACKS);
                String format = bundle.getString(MediaExtractor.MetricsConstants.FORMAT);
                String mime = bundle.getString(MediaExtractor.MetricsConstants.MIME_TYPE);
                assertTrue(numTracks == extractor.getTrackCount() && format != null &&
                        mime != null);
                extractor.release();
            }
        }

        @LargeTest
        @Test
        public void testExtractNative() {
            assumeTrue(shouldRunTest(mMime));
            assertTrue(mSrcFiles.length > 1);
            assumeTrue("TODO(b/146925481)", !mMime.equals(MediaFormat.MIMETYPE_AUDIO_VORBIS));
            assumeTrue("TODO(b/146925481)", !mMime.equals(MediaFormat.MIMETYPE_AUDIO_OPUS));
            assumeTrue("TODO(b/146925481)", !mMime.equals(MediaFormat.MIMETYPE_AUDIO_MPEG));
            assumeTrue("TODO(b/146925481)", !mMime.equals(MediaFormat.MIMETYPE_AUDIO_AAC));
            boolean isOk = true;
            for (int i = 1; i < mSrcFiles.length; i++) {
                if (!nativeTestExtract(mInpPrefix + mSrcFiles[0], mInpPrefix + mSrcFiles[i],
                        mMime)) {
                    Log.d(LOG_TAG, "Files: " + mSrcFiles[0] + ", " + mSrcFiles[i] +
                            " are different from extractor perpsective");
                    if (!codecListSupp.contains(mMime)) {
                        isOk = false;
                        break;
                    }
                }
            }
            assertTrue(testName.getMethodName() + " failed for mime: " + mMime, isOk);
        }

        @LargeTest
        @Test
        @Ignore("TODO(b/146420831)")
        public void testSeekNative() {
            assumeTrue(shouldRunTest(mMime));
            boolean isOk = true;
            for (String srcFile : mSrcFiles) {
                if (!nativeTestSeek(mInpPrefix + srcFile, mMime)) {
                    if (!codecListSupp.contains(mMime)) {
                        isOk = false;
                        break;
                    }
                }
            }
            assertTrue(testName.getMethodName() + " failed for mime: " + mMime, isOk);
        }

        @LargeTest
        @Test
        public void testSeekFlakinessNative() {
            assumeTrue(shouldRunTest(mMime));
            boolean isOk = true;
            for (String srcFile : mSrcFiles) {
                if (!nativeTestSeekFlakiness(mInpPrefix + srcFile, mMime)) {
                    if (!codecListSupp.contains(mMime)) {
                        isOk = false;
                        break;
                    }
                }
            }
            assertTrue(testName.getMethodName() + " failed for mime: " + mMime, isOk);
        }

        @SmallTest
        @Test
        public void testSeekToZeroNative() {
            assumeTrue(shouldRunTest(mMime));
            assumeTrue("TODO(b/146925481)", !mMime.equals(MediaFormat.MIMETYPE_AUDIO_VORBIS));
            assumeTrue("TODO(b/146925481)", !mMime.equals(MediaFormat.MIMETYPE_AUDIO_MPEG));
            assumeTrue("TODO(b/146925481)", !mMime.equals(MediaFormat.MIMETYPE_AUDIO_AAC));
            boolean isOk = true;
            for (String srcFile : mSrcFiles) {
                if (!nativeTestSeekToZero(mInpPrefix + srcFile, mMime)) {
                    if (!codecListSupp.contains(mMime)) {
                        isOk = false;
                        break;
                    }
                }
            }
            assertTrue(testName.getMethodName() + " failed for mime: " + mMime, isOk);
        }

        @SmallTest
        @Test
        public void testFileFormatNative() {
            assumeTrue(shouldRunTest(mMime));
            boolean isOk = true;
            for (String srcFile : mSrcFiles) {
                if (!nativeTestFileFormat(mInpPrefix + srcFile)) {
                    isOk = false;
                    break;
                }
                assertTrue(testName.getMethodName() + " failed for mime: " + mMime, isOk);
            }
        }
    }
}
