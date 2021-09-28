/*
 * Copyright (C) 2014 The Android Open Source Project
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

import android.content.res.AssetFileDescriptor;
import android.content.res.Resources;
import android.media.MediaCodec;
import android.media.MediaCodec.BufferInfo;
import android.media.MediaExtractor;
import android.media.MediaFormat;
import android.media.MediaPlayer;
import android.media.cts.R;
import android.media.cts.TestUtils.Monitor;
import android.net.Uri;
import android.os.ParcelFileDescriptor;
import android.platform.test.annotations.AppModeFull;
import android.platform.test.annotations.Presubmit;
import android.platform.test.annotations.RequiresDevice;
import android.util.Log;
import android.view.Surface;
import android.webkit.cts.CtsTestServer;

import androidx.test.filters.SmallTest;

import com.android.compatibility.common.util.MediaUtils;

import org.apache.http.Header;
import org.apache.http.HttpRequest;
import org.apache.http.impl.DefaultHttpServerConnection;
import org.apache.http.impl.io.SocketOutputBuffer;
import org.apache.http.io.SessionOutputBuffer;
import org.apache.http.params.HttpParams;
import org.apache.http.util.CharArrayBuffer;

import java.io.File;
import java.io.FileDescriptor;
import java.io.IOException;
import java.net.Socket;
import java.nio.ByteBuffer;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.HashMap;
import java.util.Map;
import java.util.Set;
import java.util.UUID;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;
import java.util.zip.Adler32;

@SmallTest
@RequiresDevice
@AppModeFull(reason = "TODO: evaluate and port to instant")
public class NativeDecoderTest extends MediaPlayerTestBase {
    private static final String TAG = "DecoderTest";

    private static final int RESET_MODE_NONE = 0;
    private static final int RESET_MODE_RECONFIGURE = 1;
    private static final int RESET_MODE_FLUSH = 2;
    private static final int RESET_MODE_EOS_FLUSH = 3;

    private static final String[] CSD_KEYS = new String[] { "csd-0", "csd-1" };

    private static final int CONFIG_MODE_NONE = 0;
    private static final int CONFIG_MODE_QUEUE = 1;

    private static Resources mResources;
    short[] mMasterBuffer;

    /** Load jni on initialization */
    static {
        Log.i("@@@", "before loadlibrary");
        System.loadLibrary("ctsmediacodec_jni");
        Log.i("@@@", "after loadlibrary");
    }

    @Override
    protected void setUp() throws Exception {
        super.setUp();
        mResources = mContext.getResources();

    }

    // check that native extractor behavior matches java extractor

    private void compareArrays(String message, int[] a1, int[] a2) {
        if (a1 == a2) {
            return;
        }

        assertNotNull(message + ": array 1 is null", a1);
        assertNotNull(message + ": array 2 is null", a2);

        assertEquals(message + ": arraylengths differ", a1.length, a2.length);
        int length = a1.length;

        for (int i = 0; i < length; i++)
            if (a1[i] != a2[i]) {
                Log.i("@@@@", Arrays.toString(a1));
                Log.i("@@@@", Arrays.toString(a2));
                fail(message + ": at index " + i);
            }
    }

    public void testExtractor() throws Exception {
        testExtractor(R.raw.sinesweepogg);
        testExtractor(R.raw.sinesweepoggmkv);
        testExtractor(R.raw.sinesweepoggmp4);
        testExtractor(R.raw.sinesweepmp3lame);
        testExtractor(R.raw.sinesweepmp3smpb);
        testExtractor(R.raw.sinesweepopus);
        testExtractor(R.raw.sinesweepopusmp4);
        testExtractor(R.raw.sinesweepm4a);
        testExtractor(R.raw.sinesweepflacmkv);
        testExtractor(R.raw.sinesweepflac);
        testExtractor(R.raw.sinesweepflacmp4);
        testExtractor(R.raw.sinesweepwav);

        testExtractor(R.raw.video_1280x720_mp4_h264_1000kbps_25fps_aac_stereo_128kbps_44100hz);
        testExtractor(R.raw.bbb_s3_1280x720_webm_vp8_8mbps_60fps_opus_6ch_384kbps_48000hz);
        testExtractor(R.raw.bbb_s4_1280x720_webm_vp9_0p31_4mbps_30fps_opus_stereo_128kbps_48000hz);
        testExtractor(R.raw.video_1280x720_webm_av1_2000kbps_30fps_vorbis_stereo_128kbps_48000hz);
        testExtractor(R.raw.video_176x144_3gp_h263_300kbps_12fps_aac_mono_24kbps_11025hz);
        testExtractor(R.raw.video_480x360_mp4_mpeg2_1500kbps_30fps_aac_stereo_128kbps_48000hz);
        testExtractor(R.raw.video_480x360_mp4_mpeg4_860kbps_25fps_aac_stereo_128kbps_44100hz);

        CtsTestServer foo = new CtsTestServer(mContext);
        testExtractor(foo.getAssetUrl("noiseandchirps.ogg"));
        testExtractor(foo.getAssetUrl("ringer.mp3"));
        testExtractor(foo.getRedirectingAssetUrl("ringer.mp3"));

        String[] keys = new String[] {"header0", "header1"};
        String[] values = new String[] {"value0", "value1"};
        testExtractor(foo.getAssetUrl("noiseandchirps.ogg"), keys, values);
        HttpRequest req = foo.getLastRequest("noiseandchirps.ogg");
        for (int i = 0; i < keys.length; i++) {
            String key = keys[i];
            String value = values[i];
            Header[] header = req.getHeaders(key);
            assertTrue("expecting " + key + ":" + value + ", saw " + Arrays.toString(header),
                    header.length == 1 && header[0].getValue().equals(value));
        }

        String[] emptyArray = new String[0];
        testExtractor(foo.getAssetUrl("noiseandchirps.ogg"), emptyArray, emptyArray);
    }

    private void testExtractor(String path) throws Exception {
        testExtractor(path, null, null);
    }

    /**
     * |keys| and |values| should be arrays of the same length.
     *
     * If keys or values is null, test {@link MediaExtractor#setDataSource(String)}
     * and NDK counter part, i.e. set data source without headers.
     *
     * If keys or values is zero length, test {@link MediaExtractor#setDataSource(String, Map))}
     * and NDK counter part with null headers.
     *
     */
    private void testExtractor(String path, String[] keys, String[] values) throws Exception {
        int[] jsizes = getSampleSizes(path, keys, values);
        int[] nsizes = getSampleSizesNativePath(path, keys, values, /* testNativeSource = */ false);
        int[] nsizes2 = getSampleSizesNativePath(path, keys, values, /* testNativeSource = */ true);

        compareArrays("different samplesizes", jsizes, nsizes);
        compareArrays("different samplesizes native source", jsizes, nsizes2);
    }

    private void testExtractor(int res) throws Exception {
        AssetFileDescriptor fd = mResources.openRawResourceFd(res);

        int[] jsizes = getSampleSizes(
                fd.getFileDescriptor(), fd.getStartOffset(), fd.getLength());
        int[] nsizes = getSampleSizesNative(
                fd.getParcelFileDescriptor().getFd(), fd.getStartOffset(), fd.getLength());

        fd.close();
        compareArrays("different samples", jsizes, nsizes);
    }

    private static int[] getSampleSizes(String path, String[] keys, String[] values) throws IOException {
        MediaExtractor ex = new MediaExtractor();
        if (keys == null || values == null) {
            ex.setDataSource(path);
        } else {
            Map<String, String> headers = null;
            int numheaders = Math.min(keys.length, values.length);
            for (int i = 0; i < numheaders; i++) {
                if (headers == null) {
                    headers = new HashMap<>();
                }
                String key = keys[i];
                String value = values[i];
                headers.put(key, value);
            }
            ex.setDataSource(path, headers);
        }

        return getSampleSizes(ex);
    }

    private static int[] getSampleSizes(FileDescriptor fd, long offset, long size)
            throws IOException {
        MediaExtractor ex = new MediaExtractor();
        ex.setDataSource(fd, offset, size);
        return getSampleSizes(ex);
    }

    private static int[] getSampleSizes(MediaExtractor ex) {
        ArrayList<Integer> foo = new ArrayList<Integer>();
        ByteBuffer buf = ByteBuffer.allocate(1024*1024);
        int numtracks = ex.getTrackCount();
        assertTrue("no tracks", numtracks > 0);
        foo.add(numtracks);
        for (int i = 0; i < numtracks; i++) {
            MediaFormat format = ex.getTrackFormat(i);
            String mime = format.getString(MediaFormat.KEY_MIME);
            if (mime.startsWith("audio/")) {
                foo.add(0);
                foo.add(format.getInteger(MediaFormat.KEY_SAMPLE_RATE));
                foo.add(format.getInteger(MediaFormat.KEY_CHANNEL_COUNT));
                foo.add((int)format.getLong(MediaFormat.KEY_DURATION));
            } else if (mime.startsWith("video/")) {
                foo.add(1);
                foo.add(format.getInteger(MediaFormat.KEY_WIDTH));
                foo.add(format.getInteger(MediaFormat.KEY_HEIGHT));
                foo.add((int)format.getLong(MediaFormat.KEY_DURATION));
            } else {
                fail("unexpected mime type: " + mime);
            }
            ex.selectTrack(i);
        }
        while(true) {
            int n = ex.readSampleData(buf, 0);
            if (n < 0) {
                break;
            }
            foo.add(n);
            foo.add(ex.getSampleTrackIndex());
            foo.add(ex.getSampleFlags());
            foo.add((int)ex.getSampleTime()); // just the low bits should be OK
            byte foobar[] = new byte[n];
            buf.get(foobar, 0, n);
            foo.add((int)adler32(foobar));
            ex.advance();
        }

        int [] ret = new int[foo.size()];
        for (int i = 0; i < ret.length; i++) {
            ret[i] = foo.get(i);
        }
        return ret;
    }

    private static native int[] getSampleSizesNative(int fd, long offset, long size);
    private static native int[] getSampleSizesNativePath(
            String path, String[] keys, String[] values, boolean testNativeSource);

    @Presubmit
    public void testExtractorFileDurationNative() throws Exception {
        int res = R.raw.video_1280x720_mp4_h264_1000kbps_25fps_aac_stereo_128kbps_44100hz;
        testExtractorFileDurationNative(res);
    }

    private void testExtractorFileDurationNative(int res) throws Exception {

        AssetFileDescriptor fd = mResources.openRawResourceFd(res);
        long durationUs = getExtractorFileDurationNative(
                fd.getParcelFileDescriptor().getFd(), fd.getStartOffset(), fd.getLength());

        MediaExtractor ex = new MediaExtractor();
        ex.setDataSource(fd.getFileDescriptor(), fd.getStartOffset(), fd.getLength());

        int numtracks = ex.getTrackCount();
        long aDurationUs = -1, vDurationUs = -1;
        for (int i = 0; i < numtracks; i++) {
            MediaFormat format = ex.getTrackFormat(i);
            String mime = format.getString(MediaFormat.KEY_MIME);
            if (mime.startsWith("audio/")) {
                aDurationUs = format.getLong(MediaFormat.KEY_DURATION);
            } else if (mime.startsWith("video/")) {
                vDurationUs = format.getLong(MediaFormat.KEY_DURATION);
            }
        }

        assertTrue("duration inconsistency",
                durationUs < 0 || durationUs >= aDurationUs && durationUs >= vDurationUs);

    }

    private static native long getExtractorFileDurationNative(int fd, long offset, long size);

    @Presubmit
    public void testExtractorCachedDurationNative() throws Exception {
        CtsTestServer foo = new CtsTestServer(mContext);
        String url = foo.getAssetUrl("ringer.mp3");
        long cachedDurationUs = getExtractorCachedDurationNative(url, /* testNativeSource = */ false);
        assertTrue("cached duration negative", cachedDurationUs >= 0);
        cachedDurationUs = getExtractorCachedDurationNative(url, /* testNativeSource = */ true);
        assertTrue("cached duration negative native source", cachedDurationUs >= 0);
    }

    private static native long getExtractorCachedDurationNative(String uri, boolean testNativeSource);

    public void testDecoder() throws Exception {
        int testsRun =
            testDecoder(R.raw.sinesweepogg) +
            testDecoder(R.raw.sinesweepoggmkv) +
            testDecoder(R.raw.sinesweepoggmp4) +
            testDecoder(R.raw.sinesweepmp3lame) +
            testDecoder(R.raw.sinesweepmp3smpb) +
            testDecoder(R.raw.sinesweepopus) +
            testDecoder(R.raw.sinesweepopusmp4) +
            testDecoder(R.raw.sinesweepm4a) +
            testDecoder(R.raw.sinesweepflacmkv) +
            testDecoder(R.raw.sinesweepflac) +
            testDecoder(R.raw.sinesweepflacmp4) +
            testDecoder(R.raw.sinesweepwav) +

            testDecoder(R.raw.video_1280x720_mp4_h264_1000kbps_25fps_aac_stereo_128kbps_44100hz) +
            testDecoder(R.raw.bbb_s1_640x360_webm_vp8_2mbps_30fps_vorbis_5ch_320kbps_48000hz) +
            testDecoder(R.raw.bbb_s1_640x360_webm_vp9_0p21_1600kbps_30fps_vorbis_stereo_128kbps_48000hz) +
            testDecoder(R.raw.video_176x144_3gp_h263_300kbps_12fps_aac_mono_24kbps_11025hz) +
            testDecoder(R.raw.video_480x360_mp4_mpeg2_1500kbps_30fps_aac_stereo_128kbps_48000hz);
            testDecoder(R.raw.video_480x360_mp4_mpeg4_860kbps_25fps_aac_stereo_128kbps_44100hz);
        if (testsRun == 0) {
            MediaUtils.skipTest("no decoders found");
        }
    }

    public void testDataSource() throws Exception {
        int testsRun = testDecoder(R.raw.video_176x144_3gp_h263_300kbps_12fps_aac_mono_24kbps_11025hz,
                /* wrapFd */ true, /* useCallback */ false);
        if (testsRun == 0) {
            MediaUtils.skipTest("no decoders found");
        }
    }

    public void testDataSourceAudioOnly() throws Exception {
        int testsRun = testDecoder(
                R.raw.loudsoftmp3,
                /* wrapFd */ true, /* useCallback */ false) +
                testDecoder(
                        R.raw.loudsoftaac,
                        /* wrapFd */ false, /* useCallback */ false);
        if (testsRun == 0) {
            MediaUtils.skipTest("no decoders found");
        }
    }

    public void testDataSourceWithCallback() throws Exception {
        int testsRun = testDecoder(R.raw.video_176x144_3gp_h263_300kbps_12fps_aac_mono_24kbps_11025hz,
                /* wrapFd */ true, /* useCallback */ true);
        if (testsRun == 0) {
            MediaUtils.skipTest("no decoders found");
        }
    }

    private int testDecoder(int res) throws Exception {
        return testDecoder(res, /* wrapFd */ false, /* useCallback */ false);
    }

    private int testDecoder(int res, boolean wrapFd, boolean useCallback) throws Exception {
        if (!MediaUtils.hasCodecsForResource(mContext, res)) {
            return 0; // skip
        }

        AssetFileDescriptor fd = mResources.openRawResourceFd(res);

        int[] jdata1 = getDecodedData(
                fd.getFileDescriptor(), fd.getStartOffset(), fd.getLength());
        int[] jdata2 = getDecodedData(
                fd.getFileDescriptor(), fd.getStartOffset(), fd.getLength());
        int[] ndata1 = getDecodedDataNative(
                fd.getParcelFileDescriptor().getFd(), fd.getStartOffset(), fd.getLength(), wrapFd,
                useCallback);
        int[] ndata2 = getDecodedDataNative(
                fd.getParcelFileDescriptor().getFd(), fd.getStartOffset(), fd.getLength(), wrapFd,
                useCallback);

        fd.close();
        compareArrays("inconsistent java decoder", jdata1, jdata2);
        compareArrays("inconsistent native decoder", ndata1, ndata2);
        compareArrays("different decoded data", jdata1, ndata1);
        return 1;
    }

    private static int[] getDecodedData(FileDescriptor fd, long offset, long size)
            throws IOException {
        MediaExtractor ex = new MediaExtractor();
        ex.setDataSource(fd, offset, size);
        return getDecodedData(ex);
    }
    private static int[] getDecodedData(MediaExtractor ex) throws IOException {
        int numtracks = ex.getTrackCount();
        assertTrue("no tracks", numtracks > 0);
        ArrayList<Integer>[] trackdata = new ArrayList[numtracks];
        MediaCodec[] codec = new MediaCodec[numtracks];
        MediaFormat[] format = new MediaFormat[numtracks];
        ByteBuffer[][] inbuffers = new ByteBuffer[numtracks][];
        ByteBuffer[][] outbuffers = new ByteBuffer[numtracks][];
        for (int i = 0; i < numtracks; i++) {
            format[i] = ex.getTrackFormat(i);
            String mime = format[i].getString(MediaFormat.KEY_MIME);
            if (mime.startsWith("audio/") || mime.startsWith("video/")) {
                codec[i] = MediaCodec.createDecoderByType(mime);
                codec[i].configure(format[i], null, null, 0);
                codec[i].start();
                inbuffers[i] = codec[i].getInputBuffers();
                outbuffers[i] = codec[i].getOutputBuffers();
                trackdata[i] = new ArrayList<Integer>();
            } else {
                fail("unexpected mime type: " + mime);
            }
            ex.selectTrack(i);
        }

        boolean[] sawInputEOS = new boolean[numtracks];
        boolean[] sawOutputEOS = new boolean[numtracks];
        int eosCount = 0;
        BufferInfo info = new BufferInfo();
        while(eosCount < numtracks) {
            int t = ex.getSampleTrackIndex();
            if (t >= 0) {
                assertFalse("saw input EOS twice", sawInputEOS[t]);
                int bufidx = codec[t].dequeueInputBuffer(5000);
                if (bufidx >= 0) {
                    Log.i("@@@@", "track " + t + " buffer " + bufidx);
                    ByteBuffer buf = inbuffers[t][bufidx];
                    int sampleSize = ex.readSampleData(buf, 0);
                    Log.i("@@@@", "read " + sampleSize + " @ " + ex.getSampleTime());
                    if (sampleSize < 0) {
                        sampleSize = 0;
                        sawInputEOS[t] = true;
                        Log.i("@@@@", "EOS");
                        //break;
                    }
                    long presentationTimeUs = ex.getSampleTime();

                    codec[t].queueInputBuffer(bufidx, 0, sampleSize, presentationTimeUs,
                            sawInputEOS[t] ? MediaCodec.BUFFER_FLAG_END_OF_STREAM : 0);
                    ex.advance();
                }
            } else {
                Log.i("@@@@", "no more input samples");
                for (int tt = 0; tt < codec.length; tt++) {
                    if (!sawInputEOS[tt]) {
                        // we ran out of samples without ever signaling EOS to the codec,
                        // so do that now
                        int bufidx = codec[tt].dequeueInputBuffer(5000);
                        if (bufidx >= 0) {
                            codec[tt].queueInputBuffer(bufidx, 0, 0, 0,
                                    MediaCodec.BUFFER_FLAG_END_OF_STREAM);
                            sawInputEOS[tt] = true;
                        }
                    }
                }
            }

            // see if any of the codecs have data available
            for (int tt = 0; tt < codec.length; tt++) {
                if (!sawOutputEOS[tt]) {
                    int status = codec[tt].dequeueOutputBuffer(info, 1);
                    if (status >= 0) {
                        if ((info.flags & MediaCodec.BUFFER_FLAG_END_OF_STREAM) != 0) {
                            Log.i("@@@@", "EOS on track " + tt);
                            sawOutputEOS[tt] = true;
                            eosCount++;
                        }
                        Log.i("@@@@", "got decoded buffer for track " + tt + ", size " + info.size);
                        if (info.size > 0) {
                            addSampleData(trackdata[tt], outbuffers[tt][status], info.size, format[tt]);
                        }
                        codec[tt].releaseOutputBuffer(status, false);
                    } else if (status == MediaCodec.INFO_OUTPUT_BUFFERS_CHANGED) {
                        Log.i("@@@@", "output buffers changed for track " + tt);
                        outbuffers[tt] = codec[tt].getOutputBuffers();
                    } else if (status == MediaCodec.INFO_OUTPUT_FORMAT_CHANGED) {
                        format[tt] = codec[tt].getOutputFormat();
                        Log.i("@@@@", "format changed for track " + t + ": " + format[tt].toString());
                    } else if (status == MediaCodec.INFO_TRY_AGAIN_LATER) {
                        Log.i("@@@@", "no buffer right now for track " + tt);
                    } else {
                        Log.i("@@@@", "unexpected info code for track " + tt + ": " + status);
                    }
                } else {
                    Log.i("@@@@", "already at EOS on track " + tt);
                }
            }
        }

        int totalsize = 0;
        for (int i = 0; i < numtracks; i++) {
            totalsize += trackdata[i].size();
        }
        int[] trackbytes = new int[totalsize];
        int idx = 0;
        for (int i = 0; i < numtracks; i++) {
            ArrayList<Integer> src = trackdata[i];
            int tracksize = src.size();
            for (int j = 0; j < tracksize; j++) {
                trackbytes[idx++] = src.get(j);
            }
        }

        for (int i = 0; i < codec.length; i++) {
            codec[i].release();
        }

        return trackbytes;
    }

    static void addSampleData(ArrayList<Integer> dst,
            ByteBuffer buf, int size, MediaFormat format) throws IOException{

        Log.i("@@@", "addsample " + dst.size() + "/" + size);
        int width = format.getInteger(MediaFormat.KEY_WIDTH, size);
        int stride = format.getInteger(MediaFormat.KEY_STRIDE, width);
        int height = format.getInteger(MediaFormat.KEY_HEIGHT, 1);
        byte[] bb = new byte[width * height];
        int offset = buf.position();
        for (int i = 0; i < height; i++) {
            buf.position(i * stride + offset);
            buf.get(bb, i * width, width);
        }
        // bb is filled with data
        long sum = adler32(bb);
        dst.add( (int) (sum & 0xffffffff));
    }

    private final static Adler32 checksummer = new Adler32(); 
    // simple checksum computed over every decoded buffer
    static int adler32(byte[] input) {
        checksummer.reset();
        checksummer.update(input);
        int ret = (int) checksummer.getValue();
        Log.i("@@@", "adler " + input.length + "/" + ret);
        return ret;
    }

    private static native int[] getDecodedDataNative(int fd, long offset, long size, boolean wrapFd,
            boolean useCallback)
            throws IOException;

    public void testVideoPlayback() throws Exception {
        int testsRun =
            testVideoPlayback(
                    R.raw.video_1280x720_mp4_h264_1000kbps_25fps_aac_stereo_128kbps_44100hz) +
            testVideoPlayback(
                    R.raw.bbb_s1_640x360_webm_vp8_2mbps_30fps_vorbis_5ch_320kbps_48000hz) +
            testVideoPlayback(
                    R.raw.bbb_s1_640x360_webm_vp9_0p21_1600kbps_30fps_vorbis_stereo_128kbps_48000hz) +
            testVideoPlayback(
                    R.raw.video_640x360_webm_av1_470kbps_30fps_vorbis_stereo_128kbps_48000hz) +
            testVideoPlayback(
                    R.raw.video_176x144_3gp_h263_300kbps_12fps_aac_mono_24kbps_11025hz) +
            testVideoPlayback(
                    R.raw.video_176x144_mp4_mpeg2_105kbps_25fps_aac_stereo_128kbps_44100hz) +
            testVideoPlayback(
                    R.raw.video_480x360_mp4_mpeg4_860kbps_25fps_aac_stereo_128kbps_44100hz);
        if (testsRun == 0) {
            MediaUtils.skipTest("no decoders found");
        }
    }

    private int testVideoPlayback(int res) throws Exception {
        if (!MediaUtils.checkCodecsForResource(mContext, res)) {
            return 0; // skip
        }

        AssetFileDescriptor fd = mResources.openRawResourceFd(res);

        boolean ret = testPlaybackNative(mActivity.getSurfaceHolder().getSurface(),
                fd.getParcelFileDescriptor().getFd(), fd.getStartOffset(), fd.getLength());
        assertTrue("native playback error", ret);
        return 1;
    }

    private static native boolean testPlaybackNative(Surface surface,
            int fd, long startOffset, long length);

    @Presubmit
    public void testMuxerAvc() throws Exception {
        // IMPORTANT: this file must not have B-frames
        testMuxer(R.raw.video_1280x720_mp4_h264_1000kbps_25fps_aac_stereo_128kbps_44100hz, false);
    }

    public void testMuxerH263() throws Exception {
        // IMPORTANT: this file must not have B-frames
        testMuxer(R.raw.video_176x144_3gp_h263_300kbps_25fps_aac_stereo_128kbps_11025hz, false);
    }

    public void testMuxerHevc() throws Exception {
        // IMPORTANT: this file must not have B-frames
        testMuxer(R.raw.video_640x360_mp4_hevc_450kbps_no_b, false);
    }

    public void testMuxerVp8() throws Exception {
        testMuxer(R.raw.bbb_s1_640x360_webm_vp8_2mbps_30fps_vorbis_5ch_320kbps_48000hz, true);
    }

    public void testMuxerVp9() throws Exception {
        testMuxer(
                R.raw.video_1280x720_webm_vp9_csd_309kbps_25fps_vorbis_stereo_128kbps_48000hz,
                true);
    }

    public void testMuxerVp9NoCsd() throws Exception {
        testMuxer(
                R.raw.bbb_s1_640x360_webm_vp9_0p21_1600kbps_30fps_vorbis_stereo_128kbps_48000hz,
                true);
    }

    public void testMuxerVp9Hdr() throws Exception {
        testMuxer(R.raw.video_256x144_webm_vp9_hdr_83kbps_24fps, true);
    }

    // We do not support MPEG-2 muxing as of yet
    public void SKIP_testMuxerMpeg2() throws Exception {
        // IMPORTANT: this file must not have B-frames
        testMuxer(R.raw.video_176x144_mp4_mpeg2_105kbps_25fps_aac_stereo_128kbps_44100hz, false);
    }

    public void testMuxerMpeg4() throws Exception {
        // IMPORTANT: this file must not have B-frames
        testMuxer(R.raw.video_176x144_mp4_mpeg4_300kbps_25fps_aac_stereo_128kbps_44100hz, false);
    }

    private void testMuxer(int res, boolean webm) throws Exception {
        if (!MediaUtils.checkCodecsForResource(mContext, res)) {
            return; // skip
        }

        AssetFileDescriptor infd = mResources.openRawResourceFd(res);

        File base = mContext.getExternalFilesDir(null);
        String tmpFile = base.getPath() + "/tmp.dat";
        Log.i("@@@", "using tmp file " + tmpFile);
        new File(tmpFile).delete();
        ParcelFileDescriptor out = ParcelFileDescriptor.open(new File(tmpFile),
                ParcelFileDescriptor.MODE_READ_WRITE | ParcelFileDescriptor.MODE_CREATE);

        assertTrue("muxer failed", testMuxerNative(
                infd.getParcelFileDescriptor().getFd(), infd.getStartOffset(), infd.getLength(),
                out.getFd(), webm));

        // compare the original with the remuxed
        MediaExtractor org = new MediaExtractor();
        org.setDataSource(infd.getFileDescriptor(),
                infd.getStartOffset(), infd.getLength());

        MediaExtractor remux = new MediaExtractor();
        remux.setDataSource(out.getFileDescriptor());

        assertEquals("mismatched numer of tracks", org.getTrackCount(), remux.getTrackCount());
        // allow duration mismatch for webm files as ffmpeg does not consider the duration of the
        // last frame while libwebm (and our framework) does.
        final long maxDurationDiffUs = webm ? 50000 : 0; // 50ms for webm
        for (int i = 0; i < org.getTrackCount(); i++) {
            MediaFormat format1 = org.getTrackFormat(i);
            MediaFormat format2 = remux.getTrackFormat(i);
            Log.i("@@@", "org: " + format1);
            Log.i("@@@", "remux: " + format2);
            assertTrue("different formats", compareFormats(format1, format2, maxDurationDiffUs));
        }

        org.release();
        remux.release();

        MediaPlayer player1 = MediaPlayer.create(mContext, res);
        MediaPlayer player2 = MediaPlayer.create(mContext, Uri.parse("file://" + tmpFile));
        assertEquals("duration is different",
                     player1.getDuration(), player2.getDuration(), maxDurationDiffUs * 0.001);
        player1.release();
        player2.release();
        new File(tmpFile).delete();
    }

    private String hexString(ByteBuffer buf) {
        if (buf == null) {
            return "(null)";
        }
        final char digits[] =
            { '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'a', 'b', 'c', 'd', 'e', 'f' };

        StringBuilder hex = new StringBuilder();
        for (int i = buf.position(); i < buf.limit(); ++i) {
            byte c = buf.get(i);
            hex.append(digits[(c >> 4) & 0xf]);
            hex.append(digits[c & 0xf]);
        }
        return hex.toString();
    }

    /** returns: null if key is in neither formats, true if they match and false otherwise */
    private Boolean compareByteBufferInFormats(MediaFormat f1, MediaFormat f2, String key) {
        ByteBuffer bufF1 = f1.containsKey(key) ? f1.getByteBuffer(key) : null;
        ByteBuffer bufF2 = f2.containsKey(key) ? f2.getByteBuffer(key) : null;
        if (bufF1 == null && bufF2 == null) {
            return null;
        }
        if (bufF1 == null || !bufF1.equals(bufF2)) {
            Log.i("@@@", "org " + key + ": " + hexString(bufF1));
            Log.i("@@@", "rmx " + key + ": " + hexString(bufF2));
            return false;
        }
        return true;
    }

    private boolean compareFormats(MediaFormat f1, MediaFormat f2, long maxDurationDiffUs) {
        final String KEY_DURATION = MediaFormat.KEY_DURATION;

        // allow some difference in durations
        if (maxDurationDiffUs > 0
                && f1.containsKey(KEY_DURATION) && f2.containsKey(KEY_DURATION)
                && Math.abs(f1.getLong(KEY_DURATION)
                        - f2.getLong(KEY_DURATION)) <= maxDurationDiffUs) {
            f2.setLong(KEY_DURATION, f1.getLong(KEY_DURATION));
        }

        // verify hdr-static-info
        if (Boolean.FALSE.equals(compareByteBufferInFormats(f1, f2, "hdr-static-info"))) {
            return false;
        }

        // verify CSDs
        for (int i = 0;; ++i) {
            String key = "csd-" + i;
            Boolean match = compareByteBufferInFormats(f1, f2, key);
            if (match == null) {
                break;
            } else if (match == false) {
                return false;
            }
        }

        // there's no good way to compare two MediaFormats, so compare their string
        // representation
        return f1.toString().equals(f2.toString());
    }

    private static native boolean testMuxerNative(int in, long inoffset, long insize,
            int out, boolean webm);

    @Presubmit
    public void testFormat() throws Exception {
        assertTrue("media format fail, see log for details", testFormatNative());
    }

    private static native boolean testFormatNative();

    @Presubmit
    public void testPssh() throws Exception {
        testPssh(R.raw.psshtest);
    }

    private void testPssh(int res) throws Exception {
        AssetFileDescriptor fd = mResources.openRawResourceFd(res);

        MediaExtractor ex = new MediaExtractor();
        ex.setDataSource(fd.getParcelFileDescriptor().getFileDescriptor(),
                fd.getStartOffset(), fd.getLength());
        testPssh(ex);
        ex.release();

        boolean ret = testPsshNative(
                fd.getParcelFileDescriptor().getFd(), fd.getStartOffset(), fd.getLength());
        assertTrue("native pssh error", ret);
    }

    private static void testPssh(MediaExtractor ex) {
        Map<UUID, byte[]> map = ex.getPsshInfo();
        Set<UUID> keys = map.keySet();
        for (UUID uuid: keys) {
            Log.i("@@@", "uuid: " + uuid + ", data size " +
                    map.get(uuid).length);
        }
    }

    private static native boolean testPsshNative(int fd, long offset, long size);

    public void testCryptoInfo() throws Exception {
        assertTrue("native cryptoinfo failed, see log for details", testCryptoInfoNative());
    }

    private static native boolean testCryptoInfoNative();

    @Presubmit
    public void testMediaFormat() throws Exception {
        assertTrue("native mediaformat failed, see log for details", testMediaFormatNative());
    }

    private static native boolean testMediaFormatNative();

    @Presubmit
    public void testAMediaDataSourceClose() throws Throwable {

        final CtsTestServer slowServer = new SlowCtsTestServer();
        final String url = slowServer.getAssetUrl("noiseandchirps.ogg");
        final long ds = createAMediaDataSource(url);
        final long ex = createAMediaExtractor();

        try {
            setAMediaExtractorDataSourceAndFailIfAnr(ex, ds);
        } finally {
            slowServer.shutdown();
            deleteAMediaExtractor(ex);
            deleteAMediaDataSource(ds);
        }

    }

    private void setAMediaExtractorDataSourceAndFailIfAnr(final long ex, final long ds)
            throws Throwable {
        final Monitor setAMediaExtractorDataSourceDone = new Monitor();
        final int HEAD_START_MILLIS = 1000;
        final int ANR_TIMEOUT_MILLIS = 2500;
        final int JOIN_TIMEOUT_MILLIS = 1500;

        Thread setAMediaExtractorDataSourceThread = new Thread() {
            public void run() {
                setAMediaExtractorDataSource(ex, ds);
                setAMediaExtractorDataSourceDone.signal();
            }
        };

        try {
            setAMediaExtractorDataSourceThread.start();
            Thread.sleep(HEAD_START_MILLIS);
            closeAMediaDataSource(ds);
            boolean closed = setAMediaExtractorDataSourceDone.waitForSignal(ANR_TIMEOUT_MILLIS);
            assertTrue("close took longer than " + ANR_TIMEOUT_MILLIS, closed);
        } finally {
            setAMediaExtractorDataSourceThread.join(JOIN_TIMEOUT_MILLIS);
        }

    }

    private class SlowCtsTestServer extends CtsTestServer {

        private static final int SERVER_DELAY_MILLIS = 5000;
        private final CountDownLatch mDisconnected = new CountDownLatch(1);

        SlowCtsTestServer() throws Exception {
            super(mContext);
        }

        @Override
        protected DefaultHttpServerConnection createHttpServerConnection() {
            return new SlowHttpServerConnection(mDisconnected, SERVER_DELAY_MILLIS);
        }

        @Override
        public void shutdown() {
            mDisconnected.countDown();
            super.shutdown();
        }
    }

    private static class SlowHttpServerConnection extends DefaultHttpServerConnection {

        private final CountDownLatch mDisconnected;
        private final int mDelayMillis;

        public SlowHttpServerConnection(CountDownLatch disconnected, int delayMillis) {
            mDisconnected = disconnected;
            mDelayMillis = delayMillis;
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
                public void write(byte[] b) throws IOException {
                    write(b, 0, b.length);
                }

                @Override
                public void write(byte[] b, int off, int len) throws IOException {
                    while (len-- > 0) {
                        write(b[off++]);
                    }
                }

                @Override
                public void writeLine(String s) throws IOException {
                    delay();
                    super.writeLine(s);
                }

                @Override
                public void writeLine(CharArrayBuffer buffer) throws IOException {
                    delay();
                    super.writeLine(buffer);
                }

                @Override
                public void write(int b) throws IOException {
                    delay();
                    super.write(b);
                }

                private void delay() throws IOException {
                    try {
                        mDisconnected.await(mDelayMillis, TimeUnit.MILLISECONDS);
                    } catch (InterruptedException e) {
                        // Ignored
                    }
                }

            };
        }
    }

    private static native long createAMediaExtractor();
    private static native long createAMediaDataSource(String url);
    private static native int  setAMediaExtractorDataSource(long ex, long ds);
    private static native void closeAMediaDataSource(long ds);
    private static native void deleteAMediaExtractor(long ex);
    private static native void deleteAMediaDataSource(long ds);

}

