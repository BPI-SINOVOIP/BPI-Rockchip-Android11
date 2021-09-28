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

import android.media.MediaCodec;
import android.media.MediaExtractor;
import android.media.MediaFormat;
import android.media.MediaMetadataRetriever;
import android.media.MediaMuxer;
import android.util.Log;

import androidx.test.filters.LargeTest;
import androidx.test.filters.SmallTest;
import androidx.test.platform.app.InstrumentationRegistry;

import org.junit.After;
import org.junit.Assume;
import org.junit.Before;
import org.junit.Test;
import org.junit.experimental.runners.Enclosed;
import org.junit.runner.RunWith;
import org.junit.runners.Parameterized;

import java.io.File;
import java.io.IOException;
import java.io.RandomAccessFile;
import java.nio.ByteBuffer;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collection;
import java.util.HashMap;
import java.util.List;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;
import static org.junit.Assert.fail;

/**
 * MuxerTestHelper breaks a media file to elements that a muxer can use to rebuild its clone.
 * While testing muxer, if the test doesn't use MediaCodecs class to generate elementary
 * stream, but uses MediaExtractor, this class will be handy
 */
class MuxerTestHelper {
    private static final String LOG_TAG = MuxerTestHelper.class.getSimpleName();
    private static final boolean ENABLE_LOGS = false;
    // Stts values within 0.1ms(100us) difference are fudged to save too
    // many stts entries in MPEG4Writer.
    static final int STTS_TOLERANCE_US = 100;
    private String mSrcPath;
    private String mMime;
    private int mTrackCount;
    private ArrayList<MediaFormat> mFormat = new ArrayList<>();
    private ByteBuffer mBuff;
    private ArrayList<ArrayList<MediaCodec.BufferInfo>> mBufferInfo;
    private HashMap<Integer, Integer> mInpIndexMap = new HashMap<>();
    private ArrayList<Integer> mTrackIdxOrder = new ArrayList<>();
    private int mFrameLimit;
    // combineMedias() uses local version of this variable
    private HashMap<Integer, Integer> mOutIndexMap = new HashMap<>();
    private boolean mRemoveCSD;

    private void splitMediaToMuxerParameters() throws IOException {
        // Set up MediaExtractor to read from the source.
        MediaExtractor extractor = new MediaExtractor();
        extractor.setDataSource(mSrcPath);

        // Set up MediaFormat
        int index = 0;
        for (int trackID = 0; trackID < extractor.getTrackCount(); trackID++) {
            extractor.selectTrack(trackID);
            MediaFormat format = extractor.getTrackFormat(trackID);
            if (mRemoveCSD) {
                for (int i = 0; ; ++i) {
                    String csdKey = "csd-" + i;
                    if (format.containsKey(csdKey)) {
                        format.removeKey(csdKey);
                    } else {
                        break;
                    }
                }
            }
            if (mMime == null) {
                mTrackCount++;
                mFormat.add(format);
                mInpIndexMap.put(trackID, index++);
            } else {
                String mime = format.getString(MediaFormat.KEY_MIME);
                if (mime != null && mime.equals(mMime)) {
                    mTrackCount++;
                    mFormat.add(format);
                    mInpIndexMap.put(trackID, index);
                    break;
                } else {
                    extractor.unselectTrack(trackID);
                }
            }
        }

        if (0 == mTrackCount) {
            extractor.release();
            throw new IllegalArgumentException("could not find usable track in file " + mSrcPath);
        }

        // Set up location for elementary stream
        File file = new File(mSrcPath);
        int bufferSize = (int) file.length();
        bufferSize = ((bufferSize + 127) >> 7) << 7;
        // Ideally, Sum of return values of extractor.readSampleData(...) should not exceed
        // source file size. But in case of Vorbis, aosp extractor appends an additional 4 bytes to
        // the data at every readSampleData() call. bufferSize <<= 1 empirically large enough to
        // hold the excess 4 bytes per read call
        bufferSize <<= 1;
        mBuff = ByteBuffer.allocate(bufferSize);

        // Set up space for bufferInfo of all samples of all tracks
        mBufferInfo = new ArrayList<>(mTrackCount);
        for (index = 0; index < mTrackCount; index++) {
            mBufferInfo.add(new ArrayList<MediaCodec.BufferInfo>());
        }

        // Let MediaExtractor do its thing
        boolean sawEOS = false;
        int frameCount = 0;
        int offset = 0;
        while (!sawEOS && frameCount < mFrameLimit) {
            int trackID;
            MediaCodec.BufferInfo bufferInfo = new MediaCodec.BufferInfo();
            bufferInfo.offset = offset;
            bufferInfo.size = extractor.readSampleData(mBuff, offset);

            if (bufferInfo.size < 0) {
                sawEOS = true;
            } else {
                bufferInfo.presentationTimeUs = extractor.getSampleTime();
                bufferInfo.flags = extractor.getSampleFlags();
                trackID = extractor.getSampleTrackIndex();
                mTrackIdxOrder.add(trackID);
                mBufferInfo.get(mInpIndexMap.get(trackID)).add(bufferInfo);
                extractor.advance();
                frameCount++;
            }
            offset += bufferInfo.size;
        }
        extractor.release();
    }

    void registerTrack(MediaMuxer muxer) {
        for (int trackID = 0; trackID < mTrackCount; trackID++) {
            int dstIndex = muxer.addTrack(mFormat.get(trackID));
            mOutIndexMap.put(trackID, dstIndex);
        }
    }

    void insertSampleData(MediaMuxer muxer) {
        // write all registered tracks in interleaved order
        int[] frameCount = new int[mTrackCount];
        for (int i = 0; i < mTrackIdxOrder.size(); i++) {
            int trackID = mTrackIdxOrder.get(i);
            int index = mInpIndexMap.get(trackID);
            MediaCodec.BufferInfo bufferInfo = mBufferInfo.get(index).get(frameCount[index]);
            muxer.writeSampleData(mOutIndexMap.get(index), mBuff, bufferInfo);
            frameCount[index]++;
            if (ENABLE_LOGS) {
                Log.v(LOG_TAG, "Track: " + index + " Timestamp: " + bufferInfo.presentationTimeUs);
            }
        }
        if (ENABLE_LOGS) {
            Log.v(LOG_TAG, "Total samples: " + mTrackIdxOrder.size());
        }
    }

    void muxMedia(MediaMuxer muxer) {
        registerTrack(muxer);
        muxer.start();
        insertSampleData(muxer);
        muxer.stop();
    }

    void combineMedias(MediaMuxer muxer, Object o, int[] repeater) {
        if (o == null || getClass() != o.getClass())
            throw new IllegalArgumentException("Invalid Object handle");
        if (null == repeater || repeater.length < 2)
            throw new IllegalArgumentException("Invalid Parameter, repeater");
        MuxerTestHelper that = (MuxerTestHelper) o;

        // add tracks
        int totalTracksToAdd = repeater[0] * this.mTrackCount + repeater[1] * that.mTrackCount;
        int[] outIndexMap = new int[totalTracksToAdd];
        MuxerTestHelper[] group = {this, that};
        for (int k = 0, idx = 0; k < group.length; k++) {
            for (int j = 0; j < repeater[k]; j++) {
                for (MediaFormat format : group[k].mFormat) {
                    outIndexMap[idx++] = muxer.addTrack(format);
                }
            }
        }

        // mux samples
        // write all registered tracks in planar order viz all samples of a track A then all
        // samples of track B, ...
        muxer.start();
        for (int k = 0, idx = 0; k < group.length; k++) {
            for (int j = 0; j < repeater[k]; j++) {
                for (int i = 0; i < group[k].mTrackCount; i++) {
                    ArrayList<MediaCodec.BufferInfo> bufInfos = group[k].mBufferInfo.get(i);
                    for (int p = 0; p < bufInfos.size(); p++) {
                        MediaCodec.BufferInfo bufInfo = bufInfos.get(p);
                        muxer.writeSampleData(outIndexMap[idx], group[k].mBuff, bufInfo);
                        if (ENABLE_LOGS) {
                            Log.v(LOG_TAG, "Track: " + outIndexMap[idx] + " Timestamp: " +
                                    bufInfo.presentationTimeUs);
                        }
                    }
                    idx++;
                }
            }
        }
        muxer.stop();
    }

    MuxerTestHelper(String srcPath, String mime, int frameLimit, boolean aRemoveCSD) throws IOException {
        mSrcPath = srcPath;
        mMime = mime;
        if (frameLimit < 0) frameLimit = Integer.MAX_VALUE;
        mFrameLimit = frameLimit;
        mRemoveCSD = aRemoveCSD;
        splitMediaToMuxerParameters();
    }

    MuxerTestHelper(String srcPath, String mime) throws IOException {
        this(srcPath, mime, -1, false);
    }

    MuxerTestHelper(String srcPath, int frameLimit) throws IOException {
        this(srcPath, null, frameLimit, false);
    }

    MuxerTestHelper(String srcPath, boolean aRemoveCSD) throws IOException {
        this(srcPath, null, -1, aRemoveCSD);
    }

    MuxerTestHelper(String srcPath) throws IOException {
        this(srcPath, null, -1, false);
    }

    int getTrackCount() {
        return mTrackCount;
    }

    // offset pts of samples from index sampleOffset till the end by tsOffset
    void offsetTimeStamp(int trackID, long tsOffset, int sampleOffset) {
        if (trackID < mTrackCount) {
            for (int i = sampleOffset; i < mBufferInfo.get(trackID).size(); i++) {
                MediaCodec.BufferInfo bufferInfo = mBufferInfo.get(trackID).get(i);
                bufferInfo.presentationTimeUs += tsOffset;
            }
        }
    }

    // returns true if 'this' stream is a subset of 'o'. That is all tracks in current media
    // stream are present in ref media stream
    boolean isSubsetOf(Object o) {
        if (this == o) return true;
        if (o == null || getClass() != o.getClass()) return false;
        MuxerTestHelper that = (MuxerTestHelper) o;
        int MAX_SAMPLE_SIZE = 4 * 1024 * 1024;
        byte[] refBuffer = new byte[MAX_SAMPLE_SIZE];
        byte[] testBuffer = new byte[MAX_SAMPLE_SIZE];
        for (int i = 0; i < mTrackCount; i++) {
            MediaFormat thisFormat = mFormat.get(i);
            String thisMime = thisFormat.getString(MediaFormat.KEY_MIME);
            int j = 0;
            for (; j < that.mTrackCount; j++) {
                MediaFormat thatFormat = that.mFormat.get(j);
                String thatMime = thatFormat.getString(MediaFormat.KEY_MIME);
                if (thisMime != null && thisMime.equals(thatMime)) {
                    if (!ExtractorTest.isCSDIdentical(thisFormat, thatFormat)) continue;
                    if (mBufferInfo.get(i).size() == that.mBufferInfo.get(j).size()) {
                        long tolerance = thisMime.startsWith("video/") ? STTS_TOLERANCE_US : 0;
                        // TODO(b/157008437) - muxed file pts is +1us of target pts
                        tolerance += 1; // rounding error
                        int k = 0;
                        for (; k < mBufferInfo.get(i).size(); k++) {
                            MediaCodec.BufferInfo thisInfo = mBufferInfo.get(i).get(k);
                            MediaCodec.BufferInfo thatInfo = that.mBufferInfo.get(j).get(k);
                            if (thisInfo.flags != thatInfo.flags) {
                                break;
                            }
                            if (thisInfo.size != thatInfo.size) {
                                break;
                            } else {
                                mBuff.position(thisInfo.offset);
                                mBuff.get(refBuffer, 0, thisInfo.size);
                                that.mBuff.position(thatInfo.offset);
                                that.mBuff.get(testBuffer, 0, thatInfo.size);
                                int count = 0;
                                for (; count < thisInfo.size; count++) {
                                    if (refBuffer[count] != testBuffer[count]) {
                                        break;
                                    }
                                }
                                if (count != thisInfo.size) break;
                            }
                            if (Math.abs(
                                    thisInfo.presentationTimeUs - thatInfo.presentationTimeUs) >
                                    tolerance) {
                                break;
                            }
                        }
                        // all samples are identical. successful match found. move to next track
                        if (k == mBufferInfo.get(i).size()) break;
                    } else {
                        if (ENABLE_LOGS) {
                            Log.d(LOG_TAG, "Mime matched but sample count different." +
                                    " Total Samples ref/test: " + mBufferInfo.get(i).size() + '/' +
                                    that.mBufferInfo.get(j).size());
                        }
                    }
                }
            }
            mBuff.position(0);
            that.mBuff.position(0);
            if (j == that.mTrackCount) {
                if (ENABLE_LOGS) {
                    Log.d(LOG_TAG, "For track: " + thisMime + " Couldn't find a match ");
                }
                return false;
            }
        }
        return true;
    }
}

@RunWith(Enclosed.class)
public class MuxerTest {
    // duplicate definitions of hide fields of MediaMuxer.OutputFormat.
    private static final int MUXER_OUTPUT_FIRST = 0;
    private static final int MUXER_OUTPUT_LAST = MediaMuxer.OutputFormat.MUXER_OUTPUT_OGG;

    private static final String MUX_SEL_KEY = "mux-sel";
    private static String selector;
    private static boolean[] muxSelector = new boolean[MUXER_OUTPUT_LAST + 1];
    private static HashMap<Integer, String> formatStringPair = new HashMap<>();

    static {
        android.os.Bundle args = InstrumentationRegistry.getArguments();
        final String defSel = "mp4;webm;3gp;ogg";
        selector = (null == args.getString(MUX_SEL_KEY)) ? defSel : args.getString(MUX_SEL_KEY);

        createFormatStringPair();
        for (int format = MUXER_OUTPUT_FIRST; format <= MUXER_OUTPUT_LAST; format++) {
            muxSelector[format] = selector.contains(formatStringPair.get(format));
        }
    }

    static private void createFormatStringPair() {
        formatStringPair.put(MediaMuxer.OutputFormat.MUXER_OUTPUT_MPEG_4, "mp4");
        formatStringPair.put(MediaMuxer.OutputFormat.MUXER_OUTPUT_WEBM, "webm");
        formatStringPair.put(MediaMuxer.OutputFormat.MUXER_OUTPUT_3GPP, "3gp");
        formatStringPair.put(MediaMuxer.OutputFormat.MUXER_OUTPUT_HEIF, "heif");
        formatStringPair.put(MediaMuxer.OutputFormat.MUXER_OUTPUT_OGG, "ogg");
    }

    static private boolean shouldRunTest(int format) {
        return muxSelector[format];
    }

    /**
     * Tests MediaMuxer API that are dependent on MediaMuxer.OutputFormat. setLocation,
     * setOrientationHint are dependent on the mime type and OutputFormat. Legality of these APIs
     * are tested in this class.
     */
    @SmallTest
    @RunWith(Parameterized.class)
    public static class TestApi {
        private int mOutFormat;
        private String mSrcFile;
        private String mInpPath;
        private String mOutPath;
        private static final float annapurnaLat = 28.59f;
        private static final float annapurnaLong = 83.82f;
        private static final float TOLERANCE = 0.0002f;
        private static final int currRotation = 180;

        static {
            System.loadLibrary("ctsmediav2muxer_jni");
        }

        @Before
        public void prologue() throws IOException {
            mInpPath = WorkDir.getMediaDirString() + mSrcFile;
            mOutPath = File.createTempFile("tmp", ".out").getAbsolutePath();
        }

        @After
        public void epilogue() {
            new File(mOutPath).delete();
        }

        @Parameterized.Parameters(name = "{index}({2})")
        public static Collection<Object[]> input() {
            return Arrays.asList(new Object[][]{
                    {MediaMuxer.OutputFormat.MUXER_OUTPUT_MPEG_4, "bbb_cif_768kbps_30fps_avc.mp4",
                            "mp4"},
                    {MediaMuxer.OutputFormat.MUXER_OUTPUT_WEBM, "bbb_cif_768kbps_30fps_vp9.mkv",
                            "webm"},
                    {MediaMuxer.OutputFormat.MUXER_OUTPUT_3GPP, "bbb_cif_768kbps_30fps_h263.mp4",
                            "3gpp"},
                    {MediaMuxer.OutputFormat.MUXER_OUTPUT_OGG, "bbb_stereo_48kHz_192kbps_opus.ogg",
                            "ogg"},
            });
        }

        public TestApi(int outFormat, String srcFile, String testName) {
            mOutFormat = outFormat;
            mSrcFile = srcFile;
        }

        private native boolean nativeTestSetLocation(int format, String srcPath, String outPath);

        private native boolean nativeTestSetOrientationHint(int format, String srcPath,
                String outPath);

        private void verifyLocationInFile(String fileName) {
            if (mOutFormat != MediaMuxer.OutputFormat.MUXER_OUTPUT_MPEG_4 &&
                    mOutFormat != MediaMuxer.OutputFormat.MUXER_OUTPUT_3GPP) return;
            MediaMetadataRetriever retriever = new MediaMetadataRetriever();
            retriever.setDataSource(fileName);

            // parsing String location and recover the location information in floats
            // Make sure the tolerance is very small - due to rounding errors.

            // Get the position of the -/+ sign in location String, which indicates
            // the beginning of the longitude.
            String loc = retriever.extractMetadata(MediaMetadataRetriever.METADATA_KEY_LOCATION);
            assertTrue(loc != null);
            int minusIndex = loc.lastIndexOf('-');
            int plusIndex = loc.lastIndexOf('+');

            assertTrue("+ or - is not found or found only at the beginning [" + loc + "]",
                    (minusIndex > 0 || plusIndex > 0));
            int index = Math.max(minusIndex, plusIndex);

            float latitude = Float.parseFloat(loc.substring(0, index - 1));
            int lastIndex = loc.lastIndexOf('/', index);
            if (lastIndex == -1) {
                lastIndex = loc.length();
            }
            float longitude = Float.parseFloat(loc.substring(index, lastIndex - 1));
            assertTrue("Incorrect latitude: " + latitude + " [" + loc + "]",
                    Math.abs(latitude - annapurnaLat) <= TOLERANCE);
            assertTrue("Incorrect longitude: " + longitude + " [" + loc + "]",
                    Math.abs(longitude - annapurnaLong) <= TOLERANCE);
            retriever.release();
        }

        private void verifyOrientation(String fileName) {
            if (mOutFormat != MediaMuxer.OutputFormat.MUXER_OUTPUT_MPEG_4 &&
                    mOutFormat != MediaMuxer.OutputFormat.MUXER_OUTPUT_3GPP) return;
            MediaMetadataRetriever retriever = new MediaMetadataRetriever();
            retriever.setDataSource(fileName);

            String testDegrees = retriever.extractMetadata(
                    MediaMetadataRetriever.METADATA_KEY_VIDEO_ROTATION);
            assertTrue(testDegrees != null);
            assertEquals("Different degrees " + currRotation + " and " + testDegrees,
                    currRotation, Integer.parseInt(testDegrees));
            retriever.release();
        }

        @Test
        public void testSetLocation() throws IOException {
            Assume.assumeTrue(shouldRunTest(mOutFormat));
            MediaMuxer muxer = new MediaMuxer(mOutPath, mOutFormat);
            try {
                boolean isGeoDataSupported = false;
                final float tooFarNorth = 90.5f;
                final float tooFarWest = -180.5f;
                final float tooFarSouth = -90.5f;
                final float tooFarEast = 180.5f;
                final float atlanticLat = 14.59f;
                final float atlanticLong = 28.67f;

                try {
                    muxer.setLocation(tooFarNorth, atlanticLong);
                    fail("setLocation succeeded with bad argument: [" + tooFarNorth + "," +
                            atlanticLong + "]");
                } catch (Exception e) {
                    // expected
                }
                try {
                    muxer.setLocation(tooFarSouth, atlanticLong);
                    fail("setLocation succeeded with bad argument: [" + tooFarSouth + "," +
                            atlanticLong + "]");
                } catch (Exception e) {
                    // expected
                }
                try {
                    muxer.setLocation(atlanticLat, tooFarWest);
                    fail("setLocation succeeded with bad argument: [" + atlanticLat + "," +
                            tooFarWest + "]");
                } catch (Exception e) {
                    // expected
                }
                try {
                    muxer.setLocation(atlanticLat, tooFarEast);
                    fail("setLocation succeeded with bad argument: [" + atlanticLat + "," +
                            tooFarEast + "]");
                } catch (Exception e) {
                    // expected
                }
                try {
                    muxer.setLocation(tooFarNorth, tooFarWest);
                    fail("setLocation succeeded with bad argument: [" + tooFarNorth + "," +
                            tooFarWest + "]");
                } catch (Exception e) {
                    // expected
                }
                try {
                    muxer.setLocation(atlanticLat, atlanticLong);
                    isGeoDataSupported = true;
                } catch (Exception e) {
                    // can happen
                }

                if (isGeoDataSupported) {
                    try {
                        muxer.setLocation(annapurnaLat, annapurnaLong);
                    } catch (IllegalArgumentException e) {
                        fail(e.getMessage());
                    }
                } else {
                    assertTrue(mOutFormat != MediaMuxer.OutputFormat.MUXER_OUTPUT_MPEG_4 &&
                            mOutFormat != MediaMuxer.OutputFormat.MUXER_OUTPUT_3GPP);
                }

                MuxerTestHelper mediaInfo = new MuxerTestHelper(mInpPath, 60);
                mediaInfo.registerTrack(muxer);
                muxer.start();
                // after start
                try {
                    muxer.setLocation(0.0f, 0.0f);
                    fail("SetLocation succeeded after muxer.start()");
                } catch (IllegalStateException e) {
                    // Exception
                }
                mediaInfo.insertSampleData(muxer);
                muxer.stop();
                // after stop
                try {
                    muxer.setLocation(annapurnaLat, annapurnaLong);
                    fail("setLocation() succeeded after muxer.stop()");
                } catch (IllegalStateException e) {
                    // expected
                }
                muxer.release();
                // after release
                try {
                    muxer.setLocation(annapurnaLat, annapurnaLong);
                    fail("setLocation() succeeded after muxer.release()");
                } catch (IllegalStateException e) {
                    // expected
                }
                verifyLocationInFile(mOutPath);
            } finally {
                muxer.release();
            }
        }

        @Test
        public void testSetOrientationHint() throws IOException {
            Assume.assumeTrue(shouldRunTest(mOutFormat));
            MediaMuxer muxer = new MediaMuxer(mOutPath, mOutFormat);
            try {
                boolean isOrientationSupported = false;
                final int[] badRotation = {360, 45, -90};
                final int oldRotation = 90;

                for (int degree : badRotation) {
                    try {
                        muxer.setOrientationHint(degree);
                        fail("setOrientationHint() succeeded with bad argument :" + degree);
                    } catch (Exception e) {
                        // expected
                    }
                }
                try {
                    muxer.setOrientationHint(oldRotation);
                    isOrientationSupported = true;
                } catch (Exception e) {
                    // can happen
                }
                if (isOrientationSupported) {
                    try {
                        muxer.setOrientationHint(currRotation);
                    } catch (IllegalArgumentException e) {
                        fail(e.getMessage());
                    }
                } else {
                    assertTrue(mOutFormat != MediaMuxer.OutputFormat.MUXER_OUTPUT_MPEG_4 &&
                            mOutFormat != MediaMuxer.OutputFormat.MUXER_OUTPUT_3GPP);
                }

                MuxerTestHelper mediaInfo = new MuxerTestHelper(mInpPath, 60);
                mediaInfo.registerTrack(muxer);
                muxer.start();
                // after start
                try {
                    muxer.setOrientationHint(0);
                    fail("setOrientationHint succeeded after muxer.start()");
                } catch (IllegalStateException e) {
                    // Exception
                }
                mediaInfo.insertSampleData(muxer);
                muxer.stop();
                // after stop
                try {
                    muxer.setOrientationHint(currRotation);
                    fail("setOrientationHint() succeeded after muxer.stop()");
                } catch (IllegalStateException e) {
                    // expected
                }
                muxer.release();
                // after release
                try {
                    muxer.setOrientationHint(currRotation);
                    fail("setOrientationHint() succeeded after muxer.release()");
                } catch (IllegalStateException e) {
                    // expected
                }
                verifyOrientation(mOutPath);
            } finally {
                muxer.release();
            }
        }

        @Test
        public void testSetLocationNative() {
            Assume.assumeTrue(shouldRunTest(mOutFormat));
            assertTrue(nativeTestSetLocation(mOutFormat, mInpPath, mOutPath));
            verifyLocationInFile(mOutPath);
        }

        @Test
        public void testSetOrientationHintNative() {
            Assume.assumeTrue(shouldRunTest(mOutFormat));
            assertTrue(nativeTestSetOrientationHint(mOutFormat, mInpPath, mOutPath));
            verifyOrientation(mOutPath);
        }
    }

    /**
     * Tests muxing multiple Video/Audio Tracks
     */
    @LargeTest
    @RunWith(Parameterized.class)
    public static class TestMultiTrack {
        private int mOutFormat;
        private String mSrcFileA;
        private String mSrcFileB;
        private String mInpPathA;
        private String mInpPathB;
        private String mRefPath;
        private String mOutPath;

        static {
            System.loadLibrary("ctsmediav2muxer_jni");
        }

        @Before
        public void prologue() throws IOException {
            mInpPathA = WorkDir.getMediaDirString() + mSrcFileA;
            mInpPathB = WorkDir.getMediaDirString() + mSrcFileB;
            mRefPath = File.createTempFile("ref", ".out").getAbsolutePath();
            mOutPath = File.createTempFile("tmp", ".out").getAbsolutePath();
        }

        @After
        public void epilogue() {
            new File(mRefPath).delete();
            new File(mOutPath).delete();
        }

        @Parameterized.Parameters(name = "{index}({3})")
        public static Collection<Object[]> input() {
            return Arrays.asList(new Object[][]{
                    // audio, video are 3 sec
                    {MediaMuxer.OutputFormat.MUXER_OUTPUT_MPEG_4, "bbb_cif_768kbps_30fps_mpeg4" +
                            ".mkv", "bbb_stereo_48kHz_192kbps_aac.mp4", "mp4"},
                    {MediaMuxer.OutputFormat.MUXER_OUTPUT_WEBM, "bbb_cif_768kbps_30fps_vp9.mkv"
                            , "bbb_stereo_48kHz_192kbps_vorbis.ogg", "webm"},
                    {MediaMuxer.OutputFormat.MUXER_OUTPUT_3GPP, "bbb_cif_768kbps_30fps_h263.mp4"
                            , "bbb_mono_16kHz_20kbps_amrwb.amr", "3gpp"},

                    // audio 3 sec, video 10 sec
                    {MediaMuxer.OutputFormat.MUXER_OUTPUT_MPEG_4, "bbb_qcif_512kbps_30fps_avc" +
                            ".mp4", "bbb_stereo_48kHz_192kbps_aac.mp4", "mp4"},
                    {MediaMuxer.OutputFormat.MUXER_OUTPUT_WEBM, "bbb_qcif_512kbps_30fps_vp9.webm"
                            , "bbb_stereo_48kHz_192kbps_vorbis.ogg", "webm"},
                    {MediaMuxer.OutputFormat.MUXER_OUTPUT_3GPP, "bbb_qcif_512kbps_30fps_h263.3gp"
                            , "bbb_mono_16kHz_20kbps_amrwb.amr", "3gpp"},

                    // audio 10 sec, video 3 sec
                    {MediaMuxer.OutputFormat.MUXER_OUTPUT_MPEG_4, "bbb_cif_768kbps_30fps_mpeg4" +
                            ".mkv", "bbb_stereo_48kHz_128kbps_aac.mp4", "mp4"},
                    {MediaMuxer.OutputFormat.MUXER_OUTPUT_WEBM, "bbb_cif_768kbps_30fps_vp9.mkv"
                            , "bbb_stereo_48kHz_128kbps_vorbis.ogg", "webm"},
                    {MediaMuxer.OutputFormat.MUXER_OUTPUT_3GPP, "bbb_cif_768kbps_30fps_h263.mp4"
                            , "bbb_mono_8kHz_8kbps_amrnb.3gp", "3gpp"},
            });
        }

        public TestMultiTrack(int outFormat, String srcFileA, String srcFileB, String testName) {
            mOutFormat = outFormat;
            mSrcFileA = srcFileA;
            mSrcFileB = srcFileB;
        }

        private native boolean nativeTestMultiTrack(int format, String fileA, String fileB,
                String fileR, String fileO);

        @Test
        public void testMultiTrack() throws IOException {
            Assume.assumeTrue(shouldRunTest(mOutFormat));
            Assume.assumeTrue("TODO(b/146423022)",
                    mOutFormat != MediaMuxer.OutputFormat.MUXER_OUTPUT_WEBM);
            // number of times to repeat {mSrcFileA, mSrcFileB} in Output
            final int[][] numTracks = {{2, 0}, {0, 2}, {1, 2}, {2, 1}};

            MuxerTestHelper mediaInfoA = new MuxerTestHelper(mInpPathA);
            MuxerTestHelper mediaInfoB = new MuxerTestHelper(mInpPathB);
            assertEquals("error! unexpected track count", 1, mediaInfoA.getTrackCount());
            assertEquals("error! unexpected track count", 1, mediaInfoB.getTrackCount());

            // prepare reference
            RandomAccessFile refFile = new RandomAccessFile(mRefPath, "rws");
            MediaMuxer muxer = new MediaMuxer(refFile.getFD(), mOutFormat);
            MuxerTestHelper refInfo = null;
            String msg = String.format("testMultiTrack: inputs: %s %s, fmt: %d ", mSrcFileA,
                    mSrcFileB, mOutFormat);
            try {
                mediaInfoA.combineMedias(muxer, mediaInfoB, new int[]{1, 1});
                refInfo = new MuxerTestHelper(mRefPath);
                if (!mediaInfoA.isSubsetOf(refInfo) || !mediaInfoB.isSubsetOf(refInfo)) {
                    fail(msg + "error ! muxing src A and src B failed");
                }
            } catch (Exception e) {
                if (mOutFormat != MediaMuxer.OutputFormat.MUXER_OUTPUT_OGG) {
                    fail(msg + "error ! muxing src A and src B failed");
                }
            } finally {
                muxer.release();
                refFile.close();
            }

            // test multi-track
            for (int[] numTrack : numTracks) {
                RandomAccessFile outFile = new RandomAccessFile(mOutPath, "rws");
                muxer = new MediaMuxer(outFile.getFD(), mOutFormat);
                try {
                    mediaInfoA.combineMedias(muxer, mediaInfoB, numTrack);
                    MuxerTestHelper outInfo = new MuxerTestHelper(mOutPath);
                    if (!outInfo.isSubsetOf(refInfo)) {
                        fail(msg + " error ! muxing src A: " + numTrack[0] + " src B: " +
                                numTrack[1] + "failed");
                    }
                } catch (Exception e) {
                    if (mOutFormat == MediaMuxer.OutputFormat.MUXER_OUTPUT_MPEG_4) {
                        fail(msg + " error ! muxing src A: " + numTrack[0] + " src B: " +
                                numTrack[1] + "failed");
                    }
                } finally {
                    muxer.release();
                    outFile.close();
                }
            }
        }

        @Test
        public void testMultiTrackNative() {
            Assume.assumeTrue(shouldRunTest(mOutFormat));
            Assume.assumeTrue("TODO(b/146423022)",
                    mOutFormat != MediaMuxer.OutputFormat.MUXER_OUTPUT_WEBM);
            assertTrue(nativeTestMultiTrack(mOutFormat, mInpPathA, mInpPathB, mRefPath, mOutPath));
        }
    }

    /**
     * Add an offset to the presentation time of samples of a track. Mux with the added offset,
     * validate by re-extracting the muxer output file and compare with original.
     */
    @LargeTest
    @RunWith(Parameterized.class)
    public static class TestOffsetPts {
        private String mSrcFile;
        private int mOutFormat;
        private int[] mOffsetIndices;
        private String mInpPath;
        private String mOutPath;

        static {
            System.loadLibrary("ctsmediav2muxer_jni");
        }

        @Before
        public void prologue() throws IOException {
            mInpPath = WorkDir.getMediaDirString() + mSrcFile;
            mOutPath = File.createTempFile("tmp", ".out").getAbsolutePath();
        }

        @After
        public void epilogue() {
            new File(mOutPath).delete();
        }

        @Parameterized.Parameters(name = "{index}({3})")
        public static Collection<Object[]> input() {
            return Arrays.asList(new Object[][]{
                    {MediaMuxer.OutputFormat.MUXER_OUTPUT_MPEG_4,
                            "bbb_cif_768kbps_30fps_hevc_stereo_48kHz_192kbps_aac.mp4",
                            new int[]{0}, "mp4"},
                    {MediaMuxer.OutputFormat.MUXER_OUTPUT_WEBM,
                            "bbb_cif_768kbps_30fps_vp8_stereo_48kHz_192kbps_vorbis.webm",
                            new int[]{0}, "webm"},
                    {MediaMuxer.OutputFormat.MUXER_OUTPUT_3GPP,
                            "bbb_cif_768kbps_30fps_mpeg4_mono_16kHz_20kbps_amrwb.3gp",
                            new int[]{0}, "3gpp"},
                    {MediaMuxer.OutputFormat.MUXER_OUTPUT_OGG, "bbb_stereo_48kHz_192kbps_opus.ogg",
                            new int[]{10}, "ogg"},
                    {MediaMuxer.OutputFormat.MUXER_OUTPUT_MPEG_4, "bbb_cif_768kbps_30fps_avc.mp4",
                            new int[]{6, 50, 77}, "mp4"},
                    {MediaMuxer.OutputFormat.MUXER_OUTPUT_WEBM, "bbb_cif_768kbps_30fps_vp9.mkv",
                            new int[]{8, 44}, "webm"},
            });
        }

        public TestOffsetPts(int outFormat, String file, int[] offsetIndices, String testName) {
            mOutFormat = outFormat;
            mSrcFile = file;
            mOffsetIndices = offsetIndices;
        }

        private native boolean nativeTestOffsetPts(int format, String srcFile, String dstFile,
                int[] offsetIndices);

        @Test
        public void testOffsetPresentationTime() throws IOException {
            final int OFFSET_TS = 111000;
            Assume.assumeTrue(shouldRunTest(mOutFormat));
            Assume.assumeTrue("TODO(b/148978457)",
                    mOutFormat != MediaMuxer.OutputFormat.MUXER_OUTPUT_MPEG_4);
            Assume.assumeTrue("TODO(b/148978457)",
                    mOutFormat != MediaMuxer.OutputFormat.MUXER_OUTPUT_3GPP);
            Assume.assumeTrue("TODO(b/146423022)",
                    mOutFormat != MediaMuxer.OutputFormat.MUXER_OUTPUT_WEBM);
            Assume.assumeTrue("TODO(b/146421018)",
                    mOutFormat != MediaMuxer.OutputFormat.MUXER_OUTPUT_OGG);
            assertTrue(OFFSET_TS > MuxerTestHelper.STTS_TOLERANCE_US);
            MuxerTestHelper mediaInfo = new MuxerTestHelper(mInpPath);
            for (int trackID = 0; trackID < mediaInfo.getTrackCount(); trackID++) {
                for (int i = 0; i < mOffsetIndices.length; i++) {
                    mediaInfo.offsetTimeStamp(trackID, OFFSET_TS, mOffsetIndices[i]);
                }
                MediaMuxer muxer = new MediaMuxer(mOutPath, mOutFormat);
                mediaInfo.muxMedia(muxer);
                muxer.release();
                MuxerTestHelper outInfo = new MuxerTestHelper(mOutPath);
                if (!outInfo.isSubsetOf(mediaInfo)) {
                    String msg = String.format(
                            "testOffsetPresentationTime: inp: %s, fmt: %d, trackID %d", mSrcFile,
                            mOutFormat, trackID);
                    fail(msg + "error! output != input");
                }
                for (int i = mOffsetIndices.length - 1; i >= 0; i--) {
                    mediaInfo.offsetTimeStamp(trackID, -OFFSET_TS, mOffsetIndices[i]);
                }
            }
        }

        @Test
        public void testOffsetPresentationTimeNative() {
            Assume.assumeTrue(shouldRunTest(mOutFormat));
            Assume.assumeTrue("TODO(b/148978457)",
                    mOutFormat != MediaMuxer.OutputFormat.MUXER_OUTPUT_MPEG_4);
            Assume.assumeTrue("TODO(b/148978457)",
                    mOutFormat != MediaMuxer.OutputFormat.MUXER_OUTPUT_3GPP);
            Assume.assumeTrue("TODO(b/146423022)",
                    mOutFormat != MediaMuxer.OutputFormat.MUXER_OUTPUT_WEBM);
            Assume.assumeTrue("TODO(b/146421018)",
                    mOutFormat != MediaMuxer.OutputFormat.MUXER_OUTPUT_OGG);
            assertTrue(nativeTestOffsetPts(mOutFormat, mInpPath, mOutPath, mOffsetIndices));
        }
    }

    /**
     * Audio, Video Codecs support a variety of file-types/container formats. For example,
     * AAC-LC supports MPEG4, 3GPP. Vorbis supports OGG and WEBM. H.263 supports 3GPP and WEBM.
     * This test takes the output of a codec and muxes it in to all possible container formats.
     * The results are checked for inconsistencies with the requirements of CDD.
     */
    @LargeTest
    @RunWith(Parameterized.class)
    public static class TestSimpleMux {
        private final List<String> codecListforTypeMp4 =
                Arrays.asList(MediaFormat.MIMETYPE_VIDEO_MPEG4, MediaFormat.MIMETYPE_VIDEO_H263,
                        MediaFormat.MIMETYPE_VIDEO_AVC, MediaFormat.MIMETYPE_VIDEO_HEVC,
                        MediaFormat.MIMETYPE_AUDIO_AAC);
        private final List<String> codecListforTypeWebm =
                Arrays.asList(MediaFormat.MIMETYPE_VIDEO_VP8, MediaFormat.MIMETYPE_VIDEO_VP9,
                        MediaFormat.MIMETYPE_AUDIO_VORBIS, MediaFormat.MIMETYPE_AUDIO_OPUS);
        private final List<String> codecListforType3gp =
                Arrays.asList(MediaFormat.MIMETYPE_VIDEO_MPEG4, MediaFormat.MIMETYPE_VIDEO_H263,
                        MediaFormat.MIMETYPE_VIDEO_AVC, MediaFormat.MIMETYPE_AUDIO_AAC,
                        MediaFormat.MIMETYPE_AUDIO_AMR_NB, MediaFormat.MIMETYPE_AUDIO_AMR_WB);
        private final List<String> codecListforTypeOgg =
                Arrays.asList(MediaFormat.MIMETYPE_AUDIO_OPUS);
        private String mMime;
        private String mSrcFile;
        private String mInpPath;
        private String mOutPath;

        static {
            System.loadLibrary("ctsmediav2muxer_jni");
        }

        public TestSimpleMux(String mime, String srcFile) {
            mMime = mime;
            mSrcFile = srcFile;
        }

        @Before
        public void prologue() throws IOException {
            mInpPath = WorkDir.getMediaDirString() + mSrcFile;
            mOutPath = File.createTempFile("tmp", ".out").getAbsolutePath();
        }

        @After
        public void epilogue() {
            new File(mOutPath).delete();
        }

        private boolean isCodecContainerPairValid(int format) {
            boolean result = false;
            if (format == MediaMuxer.OutputFormat.MUXER_OUTPUT_MPEG_4)
                result = codecListforTypeMp4.contains(mMime) || mMime.startsWith("application/");
            else if (format == MediaMuxer.OutputFormat.MUXER_OUTPUT_WEBM) {
                return codecListforTypeWebm.contains(mMime);
            } else if (format == MediaMuxer.OutputFormat.MUXER_OUTPUT_3GPP) {
                result = codecListforType3gp.contains(mMime);
            } else if (format == MediaMuxer.OutputFormat.MUXER_OUTPUT_OGG) {
                result = codecListforTypeOgg.contains(mMime);
            }
            return result;
        }

        private boolean doesCodecRequireCSD(String aMime) {
            return (aMime == MediaFormat.MIMETYPE_VIDEO_AVC ||
                    aMime == MediaFormat.MIMETYPE_VIDEO_HEVC ||
                    aMime == MediaFormat.MIMETYPE_VIDEO_MPEG4 ||
                    aMime == MediaFormat.MIMETYPE_AUDIO_AAC);

        }

        private native boolean nativeTestSimpleMux(String srcPath, String outPath, String mime,
                String selector);

        @Parameterized.Parameters(name = "{index}({0})")
        public static Collection<Object[]> input() {
            return Arrays.asList(new Object[][]{
                    // Video Codecs
                    {MediaFormat.MIMETYPE_VIDEO_H263,
                            "bbb_cif_768kbps_30fps_h263_mono_8kHz_12kbps_amrnb.3gp"},
                    {MediaFormat.MIMETYPE_VIDEO_AVC,
                            "bbb_cif_768kbps_30fps_avc_stereo_48kHz_192kbps_vorbis.mp4"},
                    {MediaFormat.MIMETYPE_VIDEO_HEVC,
                            "bbb_cif_768kbps_30fps_hevc_stereo_48kHz_192kbps_opus.mp4"},
                    {MediaFormat.MIMETYPE_VIDEO_MPEG4,
                            "bbb_cif_768kbps_30fps_mpeg4_mono_16kHz_20kbps_amrwb.3gp"},
                    {MediaFormat.MIMETYPE_VIDEO_VP8,
                            "bbb_cif_768kbps_30fps_vp8_stereo_48kHz_192kbps_vorbis.webm"},
                    {MediaFormat.MIMETYPE_VIDEO_VP9,
                            "bbb_cif_768kbps_30fps_vp9_stereo_48kHz_192kbps_opus.webm"},
                    // Audio Codecs
                    {MediaFormat.MIMETYPE_AUDIO_AAC,
                            "bbb_stereo_48kHz_128kbps_aac.mp4"},
                    {MediaFormat.MIMETYPE_AUDIO_AMR_NB,
                            "bbb_cif_768kbps_30fps_h263_mono_8kHz_12kbps_amrnb.3gp"},
                    {MediaFormat.MIMETYPE_AUDIO_AMR_WB,
                            "bbb_cif_768kbps_30fps_mpeg4_mono_16kHz_20kbps_amrwb.3gp"},
                    {MediaFormat.MIMETYPE_AUDIO_OPUS,
                            "bbb_cif_768kbps_30fps_vp9_stereo_48kHz_192kbps_opus.webm"},
                    {MediaFormat.MIMETYPE_AUDIO_VORBIS,
                            "bbb_cif_768kbps_30fps_vp8_stereo_48kHz_192kbps_vorbis.webm"},
                    // Metadata
                    {"application/gyro",
                            "video_176x144_3gp_h263_300kbps_25fps_aac_stereo_128kbps_11025hz_metadata_gyro_non_compliant.3gp"},
                    {"application/gyro",
                            "video_176x144_3gp_h263_300kbps_25fps_aac_stereo_128kbps_11025hz_metadata_gyro_compliant.3gp"},
            });
        }

        @Test
        public void testSimpleMux() throws IOException {
            Assume.assumeTrue("TODO(b/146421018)",
                    !mMime.equals(MediaFormat.MIMETYPE_AUDIO_OPUS));
            Assume.assumeTrue("TODO(b/146923287)",
                    !mMime.equals(MediaFormat.MIMETYPE_AUDIO_VORBIS));
            MuxerTestHelper mediaInfo = new MuxerTestHelper(mInpPath, mMime);
            assertEquals("error! unexpected track count", 1, mediaInfo.getTrackCount());
            for (int format = MUXER_OUTPUT_FIRST; format <= MUXER_OUTPUT_LAST; format++) {
                if (!shouldRunTest(format)) continue;
                // TODO(b/146923551)
                if (format == MediaMuxer.OutputFormat.MUXER_OUTPUT_WEBM) continue;
                String msg = String.format("testSimpleMux: inp: %s, mime: %s, fmt: %d ", mSrcFile,
                        mMime, format);
                MediaMuxer muxer = new MediaMuxer(mOutPath, format);
                try {
                    mediaInfo.muxMedia(muxer);
                    MuxerTestHelper outInfo = new MuxerTestHelper(mOutPath);
                    if (!mediaInfo.isSubsetOf(outInfo)) {
                        fail(msg + "error! output != clone(input)");
                    }
                } catch (Exception e) {
                    if (isCodecContainerPairValid(format)) {
                        fail(msg + "error! incompatible mime and output format");
                    }
                } finally {
                    muxer.release();
                }
            }
        }

        @Test
        public void testSimpleMuxNative() {
            Assume.assumeTrue("TODO(b/146421018)",
                    !mMime.equals(MediaFormat.MIMETYPE_AUDIO_OPUS));
            Assume.assumeTrue("TODO(b/146923287)",
                    !mMime.equals(MediaFormat.MIMETYPE_AUDIO_VORBIS));
            assertTrue(nativeTestSimpleMux(mInpPath, mOutPath, mMime, selector));
        }

        /* Does MediaMuxer throw IllegalStateException on missing codec specific data when required.
         * Check if relevant exception is thrown for AAC, AVC, HEVC, and MPEG4
         * codecs that require CSD in MediaMuxer.OutputFormat.MUXER_OUTPUT_MPEG_4.
         * TODO(b/156767190): Need to evaluate what all codecs need CSD and also what all formats
         * can contain these codecs, and add test cases accordingly.
         * TODO(b/156767190): Add similar tests in the native side/NDK as well.
         * TODO(b/156767190): Make a separate class, like TestNoCSDMux, instead of being part of
         * TestSimpleMux?
         */
        @Test
        public void testNoCSDMux() throws IOException {
            Assume.assumeTrue(doesCodecRequireCSD(mMime));
            MuxerTestHelper mediaInfo = new MuxerTestHelper(mInpPath, true);
            for (int format = MUXER_OUTPUT_FIRST; format <= MUXER_OUTPUT_LAST; format++) {
                // TODO(b/156767190)
                if(format != MediaMuxer.OutputFormat.MUXER_OUTPUT_MPEG_4) continue;
                MediaMuxer muxer = new MediaMuxer(mOutPath, format);
                Exception expected = null;
                String msg = String.format("testNoCSDMux: inp: %s, mime %s, fmt: %s", mSrcFile,
                                            mMime, formatStringPair.get(format));
                try {
                    mediaInfo.muxMedia(muxer);
                } catch (IllegalStateException e) {
                    expected = e;
                } catch (Exception e) {
                    fail(msg + ", unexpected exception:" + e.getMessage());
                } finally {
                    assertNotNull(msg, expected);
                    muxer.release();
                }
            }
        }
    }

    @LargeTest
    @RunWith(Parameterized.class)
    public static class TestAddEmptyTracks {
        private final List<String> mimeListforTypeMp4 =
                Arrays.asList(MediaFormat.MIMETYPE_VIDEO_MPEG4, MediaFormat.MIMETYPE_VIDEO_H263,
                        MediaFormat.MIMETYPE_VIDEO_AVC, MediaFormat.MIMETYPE_VIDEO_HEVC,
                        MediaFormat.MIMETYPE_AUDIO_AAC, MediaFormat.MIMETYPE_IMAGE_ANDROID_HEIC,
                        MediaFormat.MIMETYPE_TEXT_SUBRIP);
        private final List<String> mimeListforTypeWebm =
                Arrays.asList(MediaFormat.MIMETYPE_VIDEO_VP8, MediaFormat.MIMETYPE_VIDEO_VP9,
                        MediaFormat.MIMETYPE_AUDIO_VORBIS, MediaFormat.MIMETYPE_AUDIO_OPUS);
        private final List<String> mimeListforType3gp =
                Arrays.asList(MediaFormat.MIMETYPE_VIDEO_MPEG4, MediaFormat.MIMETYPE_VIDEO_H263,
                        MediaFormat.MIMETYPE_VIDEO_AVC, MediaFormat.MIMETYPE_AUDIO_AAC,
                        MediaFormat.MIMETYPE_AUDIO_AMR_NB, MediaFormat.MIMETYPE_AUDIO_AMR_WB);
        private final List<String> mimeListforTypeOgg =
                Arrays.asList(MediaFormat.MIMETYPE_AUDIO_OPUS);
        private String mMime;
        private String mOutPath;

        public TestAddEmptyTracks(String mime) {
            mMime = mime;
        }

        @Before
        public void prologue() throws IOException {
            mOutPath = File.createTempFile("tmp", ".out").getAbsolutePath();
        }

        @After
        public void epilogue() {
            new File(mOutPath).delete();
        }

        private boolean isMimeContainerPairValid(int format) {
            boolean result = false;
            if (format == MediaMuxer.OutputFormat.MUXER_OUTPUT_MPEG_4)
                result = mimeListforTypeMp4.contains(mMime);
            else if (format == MediaMuxer.OutputFormat.MUXER_OUTPUT_WEBM) {
                return mimeListforTypeWebm.contains(mMime);
            } else if (format == MediaMuxer.OutputFormat.MUXER_OUTPUT_3GPP) {
                result = mimeListforType3gp.contains(mMime);
            } else if (format == MediaMuxer.OutputFormat.MUXER_OUTPUT_OGG) {
                result = mimeListforTypeOgg.contains(mMime);
            }
            return result;
        }

        @Parameterized.Parameters(name = "{index}({0})")
        public static Collection<Object[]> input() {
            return Arrays.asList(new Object[][]{
                    // Video
                    {MediaFormat.MIMETYPE_VIDEO_H263},
                    {MediaFormat.MIMETYPE_VIDEO_AVC},
                    {MediaFormat.MIMETYPE_VIDEO_HEVC},
                    {MediaFormat.MIMETYPE_VIDEO_MPEG4},
                    {MediaFormat.MIMETYPE_VIDEO_VP8},
                    {MediaFormat.MIMETYPE_VIDEO_VP9},
                    // Audio
                    {MediaFormat.MIMETYPE_AUDIO_AAC},
                    {MediaFormat.MIMETYPE_AUDIO_AMR_NB},
                    {MediaFormat.MIMETYPE_AUDIO_AMR_WB},
                    {MediaFormat.MIMETYPE_AUDIO_OPUS},
                    {MediaFormat.MIMETYPE_AUDIO_VORBIS},
                    // Metadata
                    {MediaFormat.MIMETYPE_TEXT_SUBRIP},
                    // Image
                    {MediaFormat.MIMETYPE_IMAGE_ANDROID_HEIC}
            });
        }

        @Test
        public void testEmptyVideoTrack() {
            for (int format = MUXER_OUTPUT_FIRST; format <= MUXER_OUTPUT_LAST; ++format) {
                if (!mMime.startsWith("video/")) continue;
                if (!isMimeContainerPairValid(format)) continue;
                if (format != MediaMuxer.OutputFormat.MUXER_OUTPUT_MPEG_4) continue;
                try {
                    MediaMuxer mediaMuxer = new MediaMuxer(mOutPath, format);
                    MediaFormat mediaFormat = new MediaFormat();
                    mediaFormat.setString(MediaFormat.KEY_MIME, mMime);
                    mediaFormat.setInteger(MediaFormat.KEY_HEIGHT, 480);
                    mediaFormat.setInteger(MediaFormat.KEY_WIDTH, 640);
                    mediaMuxer.addTrack(mediaFormat);
                    mediaMuxer.start();
                    mediaMuxer.stop();
                    mediaMuxer.release();
                } catch (Exception e) {
                    fail("testEmptyVideoTrack : unexpected exception : " + e.getMessage());
                }
            }
        }

        @Test
        public void testEmptyAudioTrack() {
            for (int format = MUXER_OUTPUT_FIRST; format <= MUXER_OUTPUT_LAST; ++format) {
                if (!mMime.startsWith("audio/")) continue;
                if (!isMimeContainerPairValid(format)) continue;
                if (format != MediaMuxer.OutputFormat.MUXER_OUTPUT_MPEG_4) continue;
                try {
                    MediaMuxer mediaMuxer = new MediaMuxer(mOutPath, format);
                    MediaFormat mediaFormat = new MediaFormat();
                    mediaFormat.setString(MediaFormat.KEY_MIME, mMime);
                    mediaFormat.setInteger(MediaFormat.KEY_SAMPLE_RATE, 12000);
                    mediaFormat.setInteger(MediaFormat.KEY_CHANNEL_COUNT, 2);
                    mediaMuxer.addTrack(mediaFormat);
                    mediaMuxer.start();
                    mediaMuxer.stop();
                    mediaMuxer.release();
                } catch (Exception e) {
                    fail("testEmptyAudioTrack : unexpected exception : " + e.getMessage());
                }
            }
        }

        @Test
        public void testEmptyMetaDataTrack() {
            for (int format = MUXER_OUTPUT_FIRST; format <= MUXER_OUTPUT_LAST; ++format) {
                if (!mMime.startsWith("application/")) continue;
                if (!isMimeContainerPairValid(format)) continue;
                try {
                    MediaMuxer mediaMuxer = new MediaMuxer(mOutPath, format);
                    MediaFormat mediaFormat = new MediaFormat();
                    mediaFormat.setString(MediaFormat.KEY_MIME, mMime);
                    mediaMuxer.addTrack(mediaFormat);
                    mediaMuxer.start();
                    mediaMuxer.stop();
                    mediaMuxer.release();
                } catch (Exception e) {
                    fail("testEmptyMetaDataTrack : unexpected exception : " + e.getMessage());
                }
            }
        }

        @Test
        public void testEmptyImageTrack() {
            for (int format = MUXER_OUTPUT_FIRST; format <= MUXER_OUTPUT_LAST; ++format) {
                if (!mMime.startsWith("image/")) continue;
                if (!isMimeContainerPairValid(format)) continue;
                try {
                    MediaMuxer mediaMuxer = new MediaMuxer(mOutPath, format);
                    MediaFormat mediaFormat = new MediaFormat();
                    mediaFormat.setString(MediaFormat.KEY_MIME, mMime);
                    mediaFormat.setInteger(MediaFormat.KEY_HEIGHT, 480);
                    mediaFormat.setInteger(MediaFormat.KEY_WIDTH, 640);
                    mediaMuxer.addTrack(mediaFormat);
                    mediaMuxer.start();
                    mediaMuxer.stop();
                    mediaMuxer.release();
                } catch (Exception e) {
                    fail("testEmptyImageTrack : unexpected exception : " + e.getMessage());
                }
            }
        }
    }
}
