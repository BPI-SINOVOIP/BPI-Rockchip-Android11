/*
 * Copyright (C) 2013 The Android Open Source Project
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

import android.media.cts.R;

import android.content.Context;
import android.content.res.AssetFileDescriptor;
import android.content.res.Resources;
import android.media.MediaCodec.BufferInfo;
import android.media.MediaExtractor;
import android.media.MediaFormat;
import android.media.MediaMetadataRetriever;
import android.media.MediaMuxer;
import android.os.Build;
import android.os.ParcelFileDescriptor;
import android.platform.test.annotations.AppModeFull;
import android.test.AndroidTestCase;
import android.util.Log;

import com.android.compatibility.common.util.MediaUtils;

import java.io.File;
import java.io.IOException;
import java.io.RandomAccessFile;
import java.nio.ByteBuffer;
import java.util.HashMap;
import java.util.HashSet;
import java.util.Vector;
import java.util.stream.IntStream;

@AppModeFull(reason = "No interaction with system server")
public class MediaMuxerTest extends AndroidTestCase {
    private static final String TAG = "MediaMuxerTest";
    private static final boolean VERBOSE = false;
    private static final int MAX_SAMPLE_SIZE = 256 * 1024;
    private static final float LATITUDE = 0.0000f;
    private static final float LONGITUDE  = -180.0f;
    private static final float BAD_LATITUDE = 91.0f;
    private static final float BAD_LONGITUDE = -181.0f;
    private static final float TOLERANCE = 0.0002f;
    private static final long OFFSET_TIME_US = 29 * 60 * 1000000L; // 29 minutes
    private Resources mResources;
    private boolean mAndroid11 = Build.VERSION.SDK_INT >= Build.VERSION_CODES.R;

    @Override
    public void setContext(Context context) {
        super.setContext(context);
        mResources = context.getResources();
    }

    /**
     * Test: make sure the muxer handles both video and audio tracks correctly.
     */
    public void testVideoAudio() throws Exception {
        int source = R.raw.video_176x144_3gp_h263_300kbps_25fps_aac_stereo_128kbps_11025hz;
        String outputFilePath = File.createTempFile("MediaMuxerTest_testAudioVideo", ".mp4")
                .getAbsolutePath();
        cloneAndVerify(source, outputFilePath, 2, 90, MediaMuxer.OutputFormat.MUXER_OUTPUT_MPEG_4);
    }

    public void testDualVideoTrack() throws Exception {
        int source = R.raw.video_176x144_h264_408kbps_30fps_352x288_h264_122kbps_30fps;
        String outputFilePath = File.createTempFile("MediaMuxerTest_testDualVideo", ".mp4")
                .getAbsolutePath();
        cloneAndVerify(source, outputFilePath, 2, 90, MediaMuxer.OutputFormat.MUXER_OUTPUT_MPEG_4);
    }

    public void testDualAudioTrack() throws Exception {
        if (!MediaUtils.check(mAndroid11, "test needs Android 11")) return;

        int source = R.raw.audio_aac_mono_70kbs_44100hz_aac_mono_70kbs_44100hz;
        String outputFilePath = File.createTempFile("MediaMuxerTest_testDualAudio", ".mp4")
                .getAbsolutePath();
        cloneAndVerify(source, outputFilePath, 2, 90, MediaMuxer.OutputFormat.MUXER_OUTPUT_MPEG_4);
    }

    public void testDualVideoAndAudioTrack() throws Exception {
        if (!MediaUtils.check(mAndroid11, "test needs Android 11")) return;

        int source = R.raw.video_h264_30fps_video_h264_30fps_aac_44100hz_aac_44100hz;
        String outputFilePath = File.createTempFile("MediaMuxerTest_testDualVideoAudio", ".mp4")
                .getAbsolutePath();
        cloneAndVerify(source, outputFilePath, 4, 90, MediaMuxer.OutputFormat.MUXER_OUTPUT_MPEG_4);
    }

    /**
     * Test: make sure the muxer handles video, audio and non standard compliant metadata tracks
     * that generated before API29 correctly. This test will use extractor to extract the video
     * track, audio and the non standard compliant metadata track from the source file, then
     * remuxes them into a new file with standard compliant metadata track. Finally, it will check
     * to make sure the new file's metadata track matches the source file's metadata track for the
     * mime format and data payload.
     */
    public void testVideoAudioMedatadataWithNonCompliantMetadataTrack() throws Exception {
        int source =
                R.raw.video_176x144_3gp_h263_300kbps_25fps_aac_stereo_128kbps_11025hz_metadata_gyro_non_compliant;
        String outputFilePath = File.createTempFile("MediaMuxerTest_testAudioVideoMetadata", ".mp4")
                .getAbsolutePath();
        cloneAndVerify(source, outputFilePath, 3, 90, MediaMuxer.OutputFormat.MUXER_OUTPUT_MPEG_4);
    }

    /**
     * Test: make sure the muxer handles video, audio and standard compliant metadata tracks that
     * generated from API29 correctly. This test will use extractor to extract the video track,
     * audio and the standard compliant metadata track from the source file, then remuxes them
     * into a new file with standard compliant metadata track. Finally, it will check to make sure
     * the new file's metadata track matches the source file's metadata track for the mime format
     * and data payload.
     */
     public void testVideoAudioMedatadataWithCompliantMetadataTrack() throws Exception {
        int source =
                R.raw.video_176x144_3gp_h263_300kbps_25fps_aac_stereo_128kbps_11025hz_metadata_gyro_compliant;
        String outputFilePath = File.createTempFile("MediaMuxerTest_testAudioVideoMetadata", ".mp4")
                .getAbsolutePath();
        cloneAndVerify(source, outputFilePath, 3, 90, MediaMuxer.OutputFormat.MUXER_OUTPUT_MPEG_4);
    }

    /**
     * Test: make sure the muxer handles audio track only file correctly.
     */
    public void testAudioOnly() throws Exception {
        int source = R.raw.sinesweepm4a;
        String outputFilePath = File.createTempFile("MediaMuxerTest_testAudioOnly", ".mp4")
                .getAbsolutePath();
        cloneAndVerify(source, outputFilePath, 1, -1, MediaMuxer.OutputFormat.MUXER_OUTPUT_MPEG_4);
    }

    /**
     * Test: make sure the muxer handles video track only file correctly.
     */
    public void testVideoOnly() throws Exception {
        int source = R.raw.video_only_176x144_3gp_h263_25fps;
        String outputFilePath = File.createTempFile("MediaMuxerTest_videoOnly", ".mp4")
                .getAbsolutePath();
        cloneAndVerify(source, outputFilePath, 1, 180, MediaMuxer.OutputFormat.MUXER_OUTPUT_MPEG_4);
    }

    public void testWebmOutput() throws Exception {
        int source = R.raw.video_480x360_webm_vp9_333kbps_25fps_vorbis_stereo_128kbps_48000hz;
        String outputFilePath = File.createTempFile("testWebmOutput", ".webm")
                .getAbsolutePath();
        cloneAndVerify(source, outputFilePath, 2, 90, MediaMuxer.OutputFormat.MUXER_OUTPUT_WEBM);
    }

    public void testThreegppOutput() throws Exception {
        int source = R.raw.video_176x144_3gp_h263_300kbps_12fps_aac_stereo_128kbps_22050hz;
        String outputFilePath = File.createTempFile("testThreegppOutput", ".3gp")
                .getAbsolutePath();
        cloneAndVerify(source, outputFilePath, 2, 90, MediaMuxer.OutputFormat.MUXER_OUTPUT_3GPP);
    }

    /**
     * Tests: make sure the muxer handles exceptions correctly.
     * <br> Throws exception b/c start() is not called.
     * <br> Throws exception b/c 2 video tracks were added.
     * <br> Throws exception b/c 2 audio tracks were added.
     * <br> Throws exception b/c 3 tracks were added.
     * <br> Throws exception b/c no tracks was added.
     * <br> Throws exception b/c a wrong format.
     */
    public void testIllegalStateExceptions() throws IOException {
        String outputFilePath = File.createTempFile("MediaMuxerTest_testISEs", ".mp4")
                .getAbsolutePath();
        MediaMuxer muxer;

        // Throws exception b/c start() is not called.
        muxer = new MediaMuxer(outputFilePath, MediaMuxer.OutputFormat.MUXER_OUTPUT_MPEG_4);
        muxer.addTrack(MediaFormat.createVideoFormat(MediaFormat.MIMETYPE_VIDEO_AVC, 480, 320));

        try {
            muxer.stop();
            fail("should throw IllegalStateException.");
        } catch (IllegalStateException e) {
            // expected
        } finally {
            muxer.release();
        }

        // Should not throw exception when 2 video tracks were added.
        muxer = new MediaMuxer(outputFilePath, MediaMuxer.OutputFormat.MUXER_OUTPUT_MPEG_4);
        muxer.addTrack(MediaFormat.createVideoFormat(MediaFormat.MIMETYPE_VIDEO_AVC, 480, 320));

        try {
            muxer.addTrack(MediaFormat.createVideoFormat(MediaFormat.MIMETYPE_VIDEO_AVC, 480, 320));
        } catch (IllegalStateException e) {
            fail("should not throw IllegalStateException.");
        } finally {
            muxer.release();
        }

        // Should not throw exception when 2 audio tracks were added.
        muxer = new MediaMuxer(outputFilePath, MediaMuxer.OutputFormat.MUXER_OUTPUT_MPEG_4);
        muxer.addTrack(MediaFormat.createAudioFormat(MediaFormat.MIMETYPE_AUDIO_AAC, 48000, 1));
        try {
            muxer.addTrack(MediaFormat.createAudioFormat(MediaFormat.MIMETYPE_AUDIO_AAC, 48000, 1));
        } catch (IllegalStateException e) {
            fail("should not throw IllegalStateException.");
        } finally {
            muxer.release();
        }

        // Should not throw exception when 3 tracks were added.
        muxer = new MediaMuxer(outputFilePath, MediaMuxer.OutputFormat.MUXER_OUTPUT_MPEG_4);
        muxer.addTrack(MediaFormat.createVideoFormat(MediaFormat.MIMETYPE_VIDEO_AVC, 480, 320));
        muxer.addTrack(MediaFormat.createAudioFormat(MediaFormat.MIMETYPE_AUDIO_AAC, 48000, 1));
        try {
            muxer.addTrack(MediaFormat.createVideoFormat(MediaFormat.MIMETYPE_VIDEO_AVC, 480, 320));
        } catch (IllegalStateException e) {
            fail("should not throw IllegalStateException.");
        } finally {
            muxer.release();
        }

        // Throws exception b/c no tracks was added.
        muxer = new MediaMuxer(outputFilePath, MediaMuxer.OutputFormat.MUXER_OUTPUT_MPEG_4);
        try {
            muxer.start();
            fail("should throw IllegalStateException.");
        } catch (IllegalStateException e) {
            // expected
        } finally {
            muxer.release();
        }

        // Throws exception b/c a wrong format.
        muxer = new MediaMuxer(outputFilePath, MediaMuxer.OutputFormat.MUXER_OUTPUT_MPEG_4);
        try {
            muxer.addTrack(MediaFormat.createVideoFormat("vidoe/mp4", 480, 320));
            fail("should throw IllegalStateException.");
        } catch (IllegalStateException e) {
            // expected
        } finally {
            muxer.release();
        }

        // Test FileDescriptor Constructor expect sucess.
        RandomAccessFile file = null;
        try {
            file = new RandomAccessFile(outputFilePath, "rws");
            muxer = new MediaMuxer(file.getFD(), MediaMuxer.OutputFormat.MUXER_OUTPUT_MPEG_4);
            muxer.addTrack(MediaFormat.createVideoFormat(MediaFormat.MIMETYPE_VIDEO_AVC, 480, 320));
        } finally {
            file.close();
            muxer.release();
        }

        // Test FileDescriptor Constructor expect exception with read only mode.
        RandomAccessFile file2 = null;
        try {
            file2 = new RandomAccessFile(outputFilePath, "r");
            muxer = new MediaMuxer(file2.getFD(), MediaMuxer.OutputFormat.MUXER_OUTPUT_MPEG_4);
            fail("should throw IOException.");
        } catch (IOException e) {
            // expected
        } finally {
            file2.close();
            // No need to release the muxer.
        }

        // Test FileDescriptor Constructor expect NO exception with write only mode.
        ParcelFileDescriptor out = null;
        try {
            out = ParcelFileDescriptor.open(new File(outputFilePath),
                    ParcelFileDescriptor.MODE_WRITE_ONLY | ParcelFileDescriptor.MODE_CREATE);
            muxer = new MediaMuxer(out.getFileDescriptor(), MediaMuxer.OutputFormat.MUXER_OUTPUT_MPEG_4);
        } catch (IllegalArgumentException e) {
            fail("should not throw IllegalArgumentException.");
        } catch (IOException e) {
            fail("should not throw IOException.");
        } finally {
            out.close();
            muxer.release();
        }

        new File(outputFilePath).delete();
    }

    /**
     * Test: makes sure if audio and video muxing using MPEG4Writer works well when there are frame
     * drops as in b/63590381 and b/64949961 while B Frames encoding is enabled.
     */
    public void testSimulateAudioBVideoFramesDropIssues() throws Exception {
        int sourceId = R.raw.video_h264_main_b_frames;
        String outputFilePath = File.createTempFile(
            "MediaMuxerTest_testSimulateAudioBVideoFramesDropIssues", ".mp4").getAbsolutePath();
        try {
            simulateVideoFramesDropIssuesAndMux(sourceId, outputFilePath, 2 /* track index */,
                MediaMuxer.OutputFormat.MUXER_OUTPUT_MPEG_4);
            verifyAFewSamplesTimestamp(sourceId, outputFilePath);
            verifySamplesMatch(sourceId, outputFilePath, 66667 /* sample around 0 sec */, 0);
            verifySamplesMatch(
                    sourceId, outputFilePath, 8033333 /*  sample around 8 sec */, OFFSET_TIME_US);
        } finally {
            new File(outputFilePath).delete();
        }
    }

    /**
     * Test: makes sure if video only muxing using MPEG4Writer works well when there are B Frames.
     */
    public void testAllTimestampsBVideoOnly() throws Exception {
        int sourceId = R.raw.video_480x360_mp4_h264_bframes_495kbps_30fps_editlist;
        String outputFilePath = File.createTempFile("MediaMuxerTest_testAllTimestampsBVideoOnly",
            ".mp4").getAbsolutePath();
        try {
            // No samples to drop in this case.
            // No start offsets for any track.
            cloneMediaWithSamplesDropAndStartOffsets(sourceId, outputFilePath,
                MediaMuxer.OutputFormat.MUXER_OUTPUT_MPEG_4, null, null);
            verifyTSWithSamplesDropAndStartOffset(
                    sourceId, true /* has B frames */, outputFilePath, null, null);
        } finally {
            new File(outputFilePath).delete();
        }
    }

    /**
     * Test: makes sure muxing works well when video with B Frames are muxed using MPEG4Writer
     * and a few frames drop.
     */
    public void testTimestampsBVideoOnlyFramesDropOnce() throws Exception {
        int sourceId = R.raw.video_480x360_mp4_h264_bframes_495kbps_30fps_editlist;
        String outputFilePath = File.createTempFile(
            "MediaMuxerTest_testTimestampsBVideoOnlyFramesDropOnce", ".mp4").getAbsolutePath();
        try {
            HashSet<Integer> samplesDropSet = new HashSet<Integer>();
            // Drop frames from sample index 56 to 76, I frame at 56.
            IntStream.rangeClosed(56, 76).forEach(samplesDropSet::add);
            // No start offsets for any track.
            cloneMediaWithSamplesDropAndStartOffsets(sourceId, outputFilePath,
                MediaMuxer.OutputFormat.MUXER_OUTPUT_MPEG_4, samplesDropSet, null);
            verifyTSWithSamplesDropAndStartOffset(
                    sourceId, true /* has B frames */, outputFilePath, samplesDropSet, null);
        } finally {
            new File(outputFilePath).delete();
        }
    }

    /**
     * Test: makes sure if video muxing while framedrops occurs twice using MPEG4Writer
     * works with B Frames.
     */
    public void testTimestampsBVideoOnlyFramesDropTwice() throws Exception {
        int sourceId = R.raw.video_480x360_mp4_h264_bframes_495kbps_30fps_editlist;
        String outputFilePath = File.createTempFile(
            "MediaMuxerTest_testTimestampsBVideoOnlyFramesDropTwice", ".mp4").getAbsolutePath();
        try {
            HashSet<Integer> samplesDropSet = new HashSet<Integer>();
            // Drop frames with sample index 57 to 67, P frame at 57.
            IntStream.rangeClosed(57, 67).forEach(samplesDropSet::add);
            // Drop frames with sample index 173 to 200, B frame at 173.
            IntStream.rangeClosed(173, 200).forEach(samplesDropSet::add);
            // No start offsets for any track.
            cloneMediaWithSamplesDropAndStartOffsets(sourceId, outputFilePath,
                MediaMuxer.OutputFormat.MUXER_OUTPUT_MPEG_4, samplesDropSet, null);
            verifyTSWithSamplesDropAndStartOffset(
                    sourceId, true /* has B frames */, outputFilePath, samplesDropSet, null);
        } finally {
            new File(outputFilePath).delete();
        }
    }

    /**
     * Test: makes sure if audio/video muxing while framedrops once using MPEG4Writer
     * works with B Frames.
     */
    public void testTimestampsAudioBVideoFramesDropOnce() throws Exception {
        int sourceId = R.raw.video_h264_main_b_frames;
        String outputFilePath = File.createTempFile(
            "MediaMuxerTest_testTimestampsAudioBVideoFramesDropOnce", ".mp4").getAbsolutePath();
        try {
            HashSet<Integer> samplesDropSet = new HashSet<Integer>();
            // Drop frames from sample index 56 to 76, I frame at 56.
            IntStream.rangeClosed(56, 76).forEach(samplesDropSet::add);
            // No start offsets for any track.
            cloneMediaWithSamplesDropAndStartOffsets(sourceId, outputFilePath,
                MediaMuxer.OutputFormat.MUXER_OUTPUT_MPEG_4, samplesDropSet, null);
            verifyTSWithSamplesDropAndStartOffset(
                    sourceId, true /* has B frames */, outputFilePath, samplesDropSet, null);
        } finally {
            new File(outputFilePath).delete();
        }
    }

    /**
     * Test: makes sure if audio/video muxing while framedrops twice using MPEG4Writer
     * works with B Frames.
     */
    public void testTimestampsAudioBVideoFramesDropTwice() throws Exception {
        int sourceId = R.raw.video_h264_main_b_frames;
        String outputFilePath = File.createTempFile(
            "MediaMuxerTest_testTimestampsAudioBVideoFramesDropTwice", ".mp4").getAbsolutePath();
        try {
            HashSet<Integer> samplesDropSet = new HashSet<Integer>();
            // Drop frames with sample index 57 to 67, P frame at 57.
            IntStream.rangeClosed(57, 67).forEach(samplesDropSet::add);
            // Drop frames with sample index 173 to 200, B frame at 173.
            IntStream.rangeClosed(173, 200).forEach(samplesDropSet::add);
            // No start offsets for any track.
            cloneMediaWithSamplesDropAndStartOffsets(sourceId, outputFilePath,
                MediaMuxer.OutputFormat.MUXER_OUTPUT_MPEG_4, samplesDropSet, null);
            verifyTSWithSamplesDropAndStartOffset(
                    sourceId, true /* has B frames */, outputFilePath, samplesDropSet, null);
        } finally {
            new File(outputFilePath).delete();
        }
    }

    /**
     * Test: makes sure if audio/video muxing using MPEG4Writer works with B Frames
     * when video frames start later than audio.
     */
    public void testTimestampsAudioBVideoStartOffsetVideo() throws Exception {
        Vector<Integer> startOffsetUsVect = new Vector<Integer>();
        // Video starts at 400000us.
        startOffsetUsVect.add(400000);
        // Audio starts at 0us.
        startOffsetUsVect.add(0);
        checkTimestampsAudioBVideoDiffStartOffsets(startOffsetUsVect);
    }

    /**
     * Test: makes sure if audio/video muxing using MPEG4Writer works with B Frames
     * when video and audio samples start after zero, video later than audio.
     */
    public void testTimestampsAudioBVideoStartOffsetVideoAudio() throws Exception {
        Vector<Integer> startOffsetUsVect = new Vector<Integer>();
        // Video starts at 400000us.
        startOffsetUsVect.add(400000);
        // Audio starts at 200000us.
        startOffsetUsVect.add(200000);
        checkTimestampsAudioBVideoDiffStartOffsets(startOffsetUsVect);
    }

    /**
     * Test: makes sure if audio/video muxing using MPEG4Writer works with B Frames
     * when video and audio samples start after zero, audio later than video.
     */
    public void testTimestampsAudioBVideoStartOffsetAudioVideo() throws Exception {
        if (!MediaUtils.check(mAndroid11, "test needs Android 11")) return;

        Vector<Integer> startOffsetUsVect = new Vector<Integer>();
        // Video starts at 200000us.
        startOffsetUsVect.add(200000);
        // Audio starts at 400000us.
        startOffsetUsVect.add(400000);
        checkTimestampsAudioBVideoDiffStartOffsets(startOffsetUsVect);
    }

    /**
     * Test: makes sure if audio/video muxing using MPEG4Writer works with B Frames
     * when video starts after zero and audio starts before zero.
     */
    public void testTimestampsAudioBVideoStartOffsetNegativeAudioVideo() throws Exception {
        if (!MediaUtils.check(mAndroid11, "test needs Android 11")) return;

        Vector<Integer> startOffsetUsVect = new Vector<Integer>();
        // Video starts at 200000us.
        startOffsetUsVect.add(200000);
        // Audio starts at -23220us, multiple of duration of one frame (1024/44100hz)
        startOffsetUsVect.add(-23220);
        checkTimestampsAudioBVideoDiffStartOffsets(startOffsetUsVect);
    }

    /**
     * Test: makes sure if audio/video muxing using MPEG4Writer works with B Frames when audio
     * samples start later than video.
     */
    public void testTimestampsAudioBVideoStartOffsetAudio() throws Exception {
        if (!MediaUtils.check(mAndroid11, "test needs Android 11")) return;

        Vector<Integer> startOffsetUsVect = new Vector<Integer>();
        // Video starts at 0us.
        startOffsetUsVect.add(0);
        // Audio starts at 400000us.
        startOffsetUsVect.add(400000);
        checkTimestampsAudioBVideoDiffStartOffsets(startOffsetUsVect);
    }

    /**
     * Test: make sure if audio/video muxing works good with different start offsets for
     * audio and video, audio later than video at 0us.
     */
    public void testTimestampsStartOffsetAudio() throws Exception {
        if (!MediaUtils.check(mAndroid11, "test needs Android 11")) return;

        Vector<Integer> startOffsetUsVect = new Vector<Integer>();
        // Video starts at 0us.
        startOffsetUsVect.add(0);
        // Audio starts at 500000us.
        startOffsetUsVect.add(500000);
        checkTimestampsWithStartOffsets(startOffsetUsVect);
    }

    /**
     * Test: make sure if audio/video muxing works good with different start offsets for
     * audio and video, video later than audio at 0us.
     */
    public void testTimestampsStartOffsetVideo() throws Exception {
        if (!MediaUtils.check(mAndroid11, "test needs Android 11")) return;

        Vector<Integer> startOffsetUsVect = new Vector<Integer>();
        // Video starts at 500000us.
        startOffsetUsVect.add(500000);
        // Audio starts at 0us.
        startOffsetUsVect.add(0);
        checkTimestampsWithStartOffsets(startOffsetUsVect);
    }

    /**
     * Test: make sure if audio/video muxing works good with different start offsets for
     * audio and video, audio later than video, positive offsets for both.
     */
    public void testTimestampsStartOffsetVideoAudio() throws Exception {
        if (!MediaUtils.check(mAndroid11, "test needs Android 11")) return;

        Vector<Integer> startOffsetUsVect = new Vector<Integer>();
        // Video starts at 250000us.
        startOffsetUsVect.add(250000);
        // Audio starts at 500000us.
        startOffsetUsVect.add(500000);
        checkTimestampsWithStartOffsets(startOffsetUsVect);
    }

    /**
     * Test: make sure if audio/video muxing works good with different start offsets for
     * audio and video, video later than audio, positive offets for both.
     */
    public void testTimestampsStartOffsetAudioVideo() throws Exception {
        if (!MediaUtils.check(mAndroid11, "test needs Android 11")) return;

        Vector<Integer> startOffsetUsVect = new Vector<Integer>();
        // Video starts at 500000us.
        startOffsetUsVect.add(500000);
        // Audio starts at 250000us.
        startOffsetUsVect.add(250000);
        checkTimestampsWithStartOffsets(startOffsetUsVect);
    }

    /**
     * Test: make sure if audio/video muxing works good with different start offsets for
     * audio and video, video later than audio, audio before zero.
     */
    public void testTimestampsStartOffsetNegativeAudioVideo() throws Exception {
        if (!MediaUtils.check(mAndroid11, "test needs Android 11")) return;

        Vector<Integer> startOffsetUsVect = new Vector<Integer>();
        // Video starts at 50000us.
        startOffsetUsVect.add(50000);
        // Audio starts at -23220us, multiple of duration of one frame (1024/44100hz)
        startOffsetUsVect.add(-23220);
        checkTimestampsWithStartOffsets(startOffsetUsVect);
    }

    /**
     * Test: makes sure if audio/video muxing using MPEG4Writer works with B Frames
     * when video and audio samples start after different times.
     */
    private void checkTimestampsAudioBVideoDiffStartOffsets(Vector<Integer> startOffsetUs)
            throws Exception {
        MPEG4CheckTimestampsAudioBVideoDiffStartOffsets(startOffsetUs);
        // TODO: uncomment webm testing once bugs related to timestamps in webmwriter are fixed.
        // WebMCheckTimestampsAudioBVideoDiffStartOffsets(startOffsetUsVect);
    }

    private void MPEG4CheckTimestampsAudioBVideoDiffStartOffsets(Vector<Integer> startOffsetUs)
            throws Exception {
        if (VERBOSE) {
            Log.v(TAG, "MPEG4CheckTimestampsAudioBVideoDiffStartOffsets");
        }
        int sourceId = R.raw.video_h264_main_b_frames;
        String outputFilePath = File.createTempFile(
            "MediaMuxerTest_testTimestampsAudioBVideoDiffStartOffsets", ".mp4").getAbsolutePath();
        try {
            cloneMediaWithSamplesDropAndStartOffsets(sourceId, outputFilePath,
                MediaMuxer.OutputFormat.MUXER_OUTPUT_MPEG_4, null, startOffsetUs);
            verifyTSWithSamplesDropAndStartOffset(
                    sourceId, true /* has B frames */, outputFilePath, null, startOffsetUs);
        } finally {
            new File(outputFilePath).delete();
        }
    }

    /*
     * Check if timestamps are written consistently across all formats supported by MediaMuxer.
     */
    private void checkTimestampsWithStartOffsets(Vector<Integer> startOffsetUsVect)
            throws Exception {
        MPEG4CheckTimestampsWithStartOffsets(startOffsetUsVect);
        // TODO: uncomment webm testing once bugs related to timestamps in webmwriter are fixed.
        // WebMCheckTimestampsWithStartOffsets(startOffsetUsVect);
        // TODO: need to add other formats, OGG, AAC, AMR
    }

    /**
     * Make sure if audio/video muxing using MPEG4Writer works good with different start
     * offsets for audio and video.
     */
    private void MPEG4CheckTimestampsWithStartOffsets(Vector<Integer> startOffsetUsVect)
            throws Exception {
        if (VERBOSE) {
            Log.v(TAG, "MPEG4CheckTimestampsWithStartOffsets");
        }
        int sourceId = R.raw.video_480x360_mp4_h264_500kbps_30fps_aac_stereo_128kbps_44100hz;
        String outputFilePath =
            File.createTempFile("MediaMuxerTest_MPEG4CheckTimestampsWithStartOffsets", ".mp4")
                .getAbsolutePath();
        try {
            cloneMediaWithSamplesDropAndStartOffsets(sourceId, outputFilePath,
                    MediaMuxer.OutputFormat.MUXER_OUTPUT_MPEG_4, null, startOffsetUsVect);
            verifyTSWithSamplesDropAndStartOffset(
                    sourceId, false /* no B frames */, outputFilePath, null, startOffsetUsVect);
        } finally {
            new File(outputFilePath).delete();
        }
    }

    /**
     * Make sure if audio/video muxing using WebMWriter works good with different start
     * offsets for audio and video.
     */
    private void WebMCheckTimestampsWithStartOffsets(Vector<Integer> startOffsetUsVect)
            throws Exception {
        if (VERBOSE) {
            Log.v(TAG, "WebMCheckTimestampsWithStartOffsets");
        }
        int sourceId = R.raw.video_480x360_webm_vp9_333kbps_25fps_vorbis_stereo_128kbps_48000hz;
        String outputFilePath =
            File.createTempFile("MediaMuxerTest_WebMCheckTimestampsWithStartOffsets", ".webm")
                .getAbsolutePath();
        try {
            cloneMediaWithSamplesDropAndStartOffsets(sourceId, outputFilePath,
                    MediaMuxer.OutputFormat.MUXER_OUTPUT_WEBM, null, startOffsetUsVect);
            verifyTSWithSamplesDropAndStartOffset(
                    sourceId, false /* no B frames */, outputFilePath, null, startOffsetUsVect);
        } finally {
            new File(outputFilePath).delete();
        }
    }

    /**
     * Clones a media file and then compares against the source file to make
     * sure they match.
     */
    private void cloneAndVerify(int srcMedia, String outputMediaFile,
            int expectedTrackCount, int degrees, int fmt) throws IOException {
        try {
            cloneMediaUsingMuxer(srcMedia, outputMediaFile, expectedTrackCount,
                    degrees, fmt);
            if (fmt == MediaMuxer.OutputFormat.MUXER_OUTPUT_MPEG_4 ||
                    fmt == MediaMuxer.OutputFormat.MUXER_OUTPUT_3GPP) {
                verifyAttributesMatch(srcMedia, outputMediaFile, degrees);
                verifyLocationInFile(outputMediaFile);
            }
            // Verify timestamp of all samples.
            verifyTSWithSamplesDropAndStartOffset(
                    srcMedia, false /* no B frames */,outputMediaFile, null, null);
        } finally {
            new File(outputMediaFile).delete();
        }
    }

    /**
     * Using the MediaMuxer to clone a media file.
     */
    private void cloneMediaUsingMuxer(int srcMedia, String dstMediaPath,
            int expectedTrackCount, int degrees, int fmt)
            throws IOException {
        // Set up MediaExtractor to read from the source.
        AssetFileDescriptor srcFd = mResources.openRawResourceFd(srcMedia);
        MediaExtractor extractor = new MediaExtractor();
        extractor.setDataSource(srcFd.getFileDescriptor(), srcFd.getStartOffset(),
                srcFd.getLength());

        int trackCount = extractor.getTrackCount();
        assertEquals("wrong number of tracks", expectedTrackCount, trackCount);

        // Set up MediaMuxer for the destination.
        MediaMuxer muxer;
        muxer = new MediaMuxer(dstMediaPath, fmt);

        // Set up the tracks.
        HashMap<Integer, Integer> indexMap = new HashMap<Integer, Integer>(trackCount);
        for (int i = 0; i < trackCount; i++) {
            extractor.selectTrack(i);
            MediaFormat format = extractor.getTrackFormat(i);
            int dstIndex = muxer.addTrack(format);
            indexMap.put(i, dstIndex);
        }

        // Copy the samples from MediaExtractor to MediaMuxer.
        boolean sawEOS = false;
        int bufferSize = MAX_SAMPLE_SIZE;
        int frameCount = 0;
        int offset = 100;

        ByteBuffer dstBuf = ByteBuffer.allocate(bufferSize);
        BufferInfo bufferInfo = new BufferInfo();

        if (degrees >= 0) {
            muxer.setOrientationHint(degrees);
        }

        if (fmt == MediaMuxer.OutputFormat.MUXER_OUTPUT_MPEG_4 ||
            fmt == MediaMuxer.OutputFormat.MUXER_OUTPUT_3GPP) {
            // Test setLocation out of bound cases
            try {
                muxer.setLocation(BAD_LATITUDE, LONGITUDE);
                fail("setLocation succeeded with bad argument: [" + BAD_LATITUDE + "," + LONGITUDE
                    + "]");
            } catch (IllegalArgumentException e) {
                // Expected
            }
            try {
                muxer.setLocation(LATITUDE, BAD_LONGITUDE);
                fail("setLocation succeeded with bad argument: [" + LATITUDE + "," + BAD_LONGITUDE
                    + "]");
            } catch (IllegalArgumentException e) {
                // Expected
            }

            muxer.setLocation(LATITUDE, LONGITUDE);
        }

        muxer.start();
        while (!sawEOS) {
            bufferInfo.offset = offset;
            bufferInfo.size = extractor.readSampleData(dstBuf, offset);

            if (bufferInfo.size < 0) {
                if (VERBOSE) {
                    Log.d(TAG, "saw input EOS.");
                }
                sawEOS = true;
                bufferInfo.size = 0;
            } else {
                bufferInfo.presentationTimeUs = extractor.getSampleTime();
                bufferInfo.flags = extractor.getSampleFlags();
                int trackIndex = extractor.getSampleTrackIndex();

                muxer.writeSampleData(indexMap.get(trackIndex), dstBuf,
                        bufferInfo);
                extractor.advance();

                frameCount++;
                if (VERBOSE) {
                    Log.d(TAG, "Frame (" + frameCount + ") " +
                            "PresentationTimeUs:" + bufferInfo.presentationTimeUs +
                            " Flags:" + bufferInfo.flags +
                            " TrackIndex:" + trackIndex +
                            " Size(KB) " + bufferInfo.size / 1024);
                }
            }
        }

        muxer.stop();
        muxer.release();
        extractor.release();
        srcFd.close();
        return;
    }

    /**
     * Compares some attributes using MediaMetadataRetriever to make sure the
     * cloned media file matches the source file.
     */
    private void verifyAttributesMatch(int srcMedia, String testMediaPath,
            int degrees) throws IOException {
        AssetFileDescriptor testFd = mResources.openRawResourceFd(srcMedia);

        MediaMetadataRetriever retrieverSrc = new MediaMetadataRetriever();
        retrieverSrc.setDataSource(testFd.getFileDescriptor(),
                testFd.getStartOffset(), testFd.getLength());

        MediaMetadataRetriever retrieverTest = new MediaMetadataRetriever();
        retrieverTest.setDataSource(testMediaPath);

        String testDegrees = retrieverTest.extractMetadata(
                MediaMetadataRetriever.METADATA_KEY_VIDEO_ROTATION);
        if (testDegrees != null) {
            assertEquals("Different degrees", degrees,
                    Integer.parseInt(testDegrees));
        }

        String heightSrc = retrieverSrc.extractMetadata(
                MediaMetadataRetriever.METADATA_KEY_VIDEO_HEIGHT);
        String heightTest = retrieverTest.extractMetadata(
                MediaMetadataRetriever.METADATA_KEY_VIDEO_HEIGHT);
        assertEquals("Different height", heightSrc,
                heightTest);

        String widthSrc = retrieverSrc.extractMetadata(
                MediaMetadataRetriever.METADATA_KEY_VIDEO_WIDTH);
        String widthTest = retrieverTest.extractMetadata(
                MediaMetadataRetriever.METADATA_KEY_VIDEO_WIDTH);
        assertEquals("Different width", widthSrc,
                widthTest);

        //TODO: need to check each individual track's duration also.
        String durationSrc = retrieverSrc.extractMetadata(
                MediaMetadataRetriever.METADATA_KEY_DURATION);
        String durationTest = retrieverTest.extractMetadata(
                MediaMetadataRetriever.METADATA_KEY_DURATION);
        assertEquals("Different duration", durationSrc,
                durationTest);

        retrieverSrc.release();
        retrieverTest.release();
        testFd.close();
    }

    private void verifyLocationInFile(String fileName) {
        MediaMetadataRetriever retriever = new MediaMetadataRetriever();
        retriever.setDataSource(fileName);
        String location = retriever.extractMetadata(MediaMetadataRetriever.METADATA_KEY_LOCATION);
        assertNotNull("No location information found in file " + fileName, location);


        // parsing String location and recover the location information in floats
        // Make sure the tolerance is very small - due to rounding errors.

        // Trim the trailing slash, if any.
        int lastIndex = location.lastIndexOf('/');
        if (lastIndex != -1) {
            location = location.substring(0, lastIndex);
        }

        // Get the position of the -/+ sign in location String, which indicates
        // the beginning of the longitude.
        int minusIndex = location.lastIndexOf('-');
        int plusIndex = location.lastIndexOf('+');

        assertTrue("+ or - is not found or found only at the beginning [" + location + "]",
                (minusIndex > 0 || plusIndex > 0));
        int index = Math.max(minusIndex, plusIndex);

        float latitude = Float.parseFloat(location.substring(0, index));
        float longitude = Float.parseFloat(location.substring(index));
        assertTrue("Incorrect latitude: " + latitude + " [" + location + "]",
                Math.abs(latitude - LATITUDE) <= TOLERANCE);
        assertTrue("Incorrect longitude: " + longitude + " [" + location + "]",
                Math.abs(longitude - LONGITUDE) <= TOLERANCE);
        retriever.release();
    }

    /**
     * Uses 2 MediaExtractor, seeking to the same position, reads the sample and
     * makes sure the samples match.
     */
    private void verifySamplesMatch(int srcMedia, String testMediaPath, int seekToUs,
            long offsetTimeUs) throws IOException {
        AssetFileDescriptor testFd = mResources.openRawResourceFd(srcMedia);
        MediaExtractor extractorSrc = new MediaExtractor();
        extractorSrc.setDataSource(testFd.getFileDescriptor(),
                testFd.getStartOffset(), testFd.getLength());
        int trackCount = extractorSrc.getTrackCount();
        final int videoTrackIndex = 0;

        MediaExtractor extractorTest = new MediaExtractor();
        extractorTest.setDataSource(testMediaPath);

        assertEquals("wrong number of tracks", trackCount,
                extractorTest.getTrackCount());

        // Make sure the format is the same and select them
        for (int i = 0; i < trackCount; i++) {
            MediaFormat formatSrc = extractorSrc.getTrackFormat(i);
            MediaFormat formatTest = extractorTest.getTrackFormat(i);

            String mimeIn = formatSrc.getString(MediaFormat.KEY_MIME);
            String mimeOut = formatTest.getString(MediaFormat.KEY_MIME);
            if (!(mimeIn.equals(mimeOut))) {
                fail("format didn't match on track No." + i +
                        formatSrc.toString() + "\n" + formatTest.toString());
            }
            extractorSrc.selectTrack(videoTrackIndex);
            extractorTest.selectTrack(videoTrackIndex);

            // Pick a time and try to compare the frame.
            extractorSrc.seekTo(seekToUs, MediaExtractor.SEEK_TO_CLOSEST_SYNC);
            extractorTest.seekTo(seekToUs + offsetTimeUs, MediaExtractor.SEEK_TO_CLOSEST_SYNC);

            int bufferSize = MAX_SAMPLE_SIZE;
            ByteBuffer byteBufSrc = ByteBuffer.allocate(bufferSize);
            ByteBuffer byteBufTest = ByteBuffer.allocate(bufferSize);

            int srcBufSize = extractorSrc.readSampleData(byteBufSrc, 0);
            int testBufSize = extractorTest.readSampleData(byteBufTest, 0);

            if (!(byteBufSrc.equals(byteBufTest))) {
                if (VERBOSE) {
                    Log.d(TAG,
                            "srcTrackIndex:" + extractorSrc.getSampleTrackIndex()
                                    + "  testTrackIndex:" + extractorTest.getSampleTrackIndex());
                    Log.d(TAG,
                            "srcTSus:" + extractorSrc.getSampleTime()
                                    + " testTSus:" + extractorTest.getSampleTime());
                    Log.d(TAG, "srcBufSize:" + srcBufSize + "testBufSize:" + testBufSize);
                }
                fail("byteBuffer didn't match");
            }
            extractorSrc.unselectTrack(i);
            extractorTest.unselectTrack(i);
        }
        extractorSrc.release();
        extractorTest.release();
        testFd.close();
    }

    /**
     * Using MediaMuxer and MediaExtractor to mux a media file from another file while skipping
     * some video frames as in the issues b/63590381 and b/64949961.
     */
    private void simulateVideoFramesDropIssuesAndMux(int srcMedia, String dstMediaPath,
            int expectedTrackCount, int fmt) throws IOException {
        // Set up MediaExtractor to read from the source.
        AssetFileDescriptor srcFd = mResources.openRawResourceFd(srcMedia);
        MediaExtractor extractor = new MediaExtractor();
        extractor.setDataSource(srcFd.getFileDescriptor(), srcFd.getStartOffset(),
            srcFd.getLength());

        int trackCount = extractor.getTrackCount();
        assertEquals("wrong number of tracks", expectedTrackCount, trackCount);

        // Set up MediaMuxer for the destination.
        MediaMuxer muxer;
        muxer = new MediaMuxer(dstMediaPath, fmt);

        // Set up the tracks.
        HashMap<Integer, Integer> indexMap = new HashMap<Integer, Integer>(trackCount);

        for (int i = 0; i < trackCount; i++) {
            extractor.selectTrack(i);
            MediaFormat format = extractor.getTrackFormat(i);
            int dstIndex = muxer.addTrack(format);
            indexMap.put(i, dstIndex);
        }

        // Copy the samples from MediaExtractor to MediaMuxer.
        boolean sawEOS = false;
        int bufferSize = MAX_SAMPLE_SIZE;
        int sampleCount = 0;
        int offset = 0;
        int videoSampleCount = 0;
        // Counting frame index values starting from 1
        final int muxAllTypeVideoFramesUntilIndex = 136; // I/P/B frames passed as it is until this
        final int muxAllTypeVideoFramesFromIndex = 171; // I/P/B frames passed as it is from this
        final int pFrameBeforeARandomBframeIndex = 137;
        final int bFrameAfterPFrameIndex = pFrameBeforeARandomBframeIndex+1;

        ByteBuffer dstBuf = ByteBuffer.allocate(bufferSize);
        BufferInfo bufferInfo = new BufferInfo();

        muxer.start();
        while (!sawEOS) {
            bufferInfo.offset = 0;
            bufferInfo.size = extractor.readSampleData(dstBuf, offset);
            if (bufferInfo.size < 0) {
                if (VERBOSE) {
                    Log.d(TAG, "saw input EOS.");
                }
                sawEOS = true;
                bufferInfo.size = 0;
            } else {
                bufferInfo.presentationTimeUs = extractor.getSampleTime();
                bufferInfo.flags = extractor.getSampleFlags();
                int trackIndex = extractor.getSampleTrackIndex();
                // Video track at index 0, skip some video frames while muxing.
                if (trackIndex == 0) {
                    ++videoSampleCount;
                    if (VERBOSE) {
                        Log.v(TAG, "videoSampleCount : " + videoSampleCount);
                    }
                    if (videoSampleCount <= muxAllTypeVideoFramesUntilIndex
                            || videoSampleCount == bFrameAfterPFrameIndex) {
                        // Write frame as it is.
                        muxer.writeSampleData(indexMap.get(trackIndex), dstBuf, bufferInfo);
                    } else if (videoSampleCount == pFrameBeforeARandomBframeIndex
                            || videoSampleCount >= muxAllTypeVideoFramesFromIndex) {
                        // Adjust time stamp for this P frame to a few frames later, say ~5seconds
                        bufferInfo.presentationTimeUs += OFFSET_TIME_US;
                        muxer.writeSampleData(indexMap.get(trackIndex), dstBuf, bufferInfo);
                    } else {
                        // Skip frames after bFrameAfterPFrameIndex
                        // and before muxAllTypeVideoFramesFromIndex.
                        if (VERBOSE) {
                            Log.i(TAG, "skipped this frame");
                        }
                    }
                } else {
                    // write audio data as it is continuously
                    muxer.writeSampleData(indexMap.get(trackIndex), dstBuf, bufferInfo);
                }
                extractor.advance();
                sampleCount++;
                if (VERBOSE) {
                    Log.d(TAG, "Frame (" + sampleCount + ") " +
                            "PresentationTimeUs:" + bufferInfo.presentationTimeUs +
                            " Flags:" + bufferInfo.flags +
                            " TrackIndex:" + trackIndex +
                            " Size(bytes) " + bufferInfo.size );
                }
            }
        }

        muxer.stop();
        muxer.release();
        extractor.release();
        srcFd.close();

        return;
    }

    /**
     * Uses two MediaExtractor's and checks whether timestamps of first few and another few
     *  from last sync frame matches
     */
    private void verifyAFewSamplesTimestamp(int srcMediaId, String testMediaPath)
            throws IOException {
        final int numFramesTSCheck = 10; // Num frames to be checked for its timestamps

        AssetFileDescriptor srcFd = mResources.openRawResourceFd(srcMediaId);
        MediaExtractor extractorSrc = new MediaExtractor();
        extractorSrc.setDataSource(srcFd.getFileDescriptor(),
            srcFd.getStartOffset(), srcFd.getLength());
        MediaExtractor extractorTest = new MediaExtractor();
        extractorTest.setDataSource(testMediaPath);

        int trackCount = extractorSrc.getTrackCount();
        for (int i = 0; i < trackCount; i++) {
            MediaFormat format = extractorSrc.getTrackFormat(i);
            extractorSrc.selectTrack(i);
            extractorTest.selectTrack(i);
            if (format.getString(MediaFormat.KEY_MIME).startsWith("video/")) {
                // Check time stamps for numFramesTSCheck frames from 33333us.
                checkNumFramesTimestamp(33333, 0, numFramesTSCheck, extractorSrc, extractorTest);
                // Check time stamps for numFramesTSCheck frames from 9333333 -
                // sync frame after framedrops at index 172 of video track.
                checkNumFramesTimestamp(
                        9333333, OFFSET_TIME_US, numFramesTSCheck, extractorSrc, extractorTest);
            } else if (format.getString(MediaFormat.KEY_MIME).startsWith("audio/")) {
                // Check timestamps for all audio frames. Test file has 427 audio frames.
                checkNumFramesTimestamp(0, 0, 427, extractorSrc, extractorTest);
            }
            extractorSrc.unselectTrack(i);
            extractorTest.unselectTrack(i);
        }

        extractorSrc.release();
        extractorTest.release();
        srcFd.close();
    }

    private void checkNumFramesTimestamp(long seekTimeUs, long offsetTimeUs, int numFrames,
            MediaExtractor extractorSrc, MediaExtractor extractorTest) {
        long srcSampleTimeUs = -1;
        long testSampleTimeUs = -1;
        extractorSrc.seekTo(seekTimeUs, MediaExtractor.SEEK_TO_CLOSEST_SYNC);
        extractorTest.seekTo(seekTimeUs + offsetTimeUs, MediaExtractor.SEEK_TO_CLOSEST_SYNC);
        while (numFrames-- > 0 ) {
            srcSampleTimeUs = extractorSrc.getSampleTime();
            testSampleTimeUs = extractorTest.getSampleTime();
            if (srcSampleTimeUs == -1 || testSampleTimeUs == -1) {
                fail("either of tracks reached end of stream");
            }
            if ((srcSampleTimeUs + offsetTimeUs) != testSampleTimeUs) {
                if (VERBOSE) {
                    Log.d(TAG, "srcTrackIndex:" + extractorSrc.getSampleTrackIndex() +
                        "  testTrackIndex:" + extractorTest.getSampleTrackIndex());
                    Log.d(TAG, "srcTSus:" + srcSampleTimeUs + " testTSus:" + testSampleTimeUs);
                }
                fail("timestamps didn't match");
            }
            extractorSrc.advance();
            extractorTest.advance();
        }
    }

    /**
     * Using MediaMuxer and MediaExtractor to mux a media file from another file while skipping
     * 0 or more video frames and desired start offsets for each track.
     * startOffsetUsVect : order of tracks is the same as in the input file
     */
    private void cloneMediaWithSamplesDropAndStartOffsets(int srcMedia, String dstMediaPath,
            int fmt, HashSet<Integer> samplesDropSet, Vector<Integer> startOffsetUsVect)
            throws IOException {
        // Set up MediaExtractor to read from the source.
        AssetFileDescriptor srcFd = mResources.openRawResourceFd(srcMedia);
        MediaExtractor extractor = new MediaExtractor();
        extractor.setDataSource(srcFd.getFileDescriptor(), srcFd.getStartOffset(),
            srcFd.getLength());

        int trackCount = extractor.getTrackCount();

        // Set up MediaMuxer for the destination.
        MediaMuxer muxer;
        muxer = new MediaMuxer(dstMediaPath, fmt);

        // Set up the tracks.
        HashMap<Integer, Integer> indexMap = new HashMap<Integer, Integer>(trackCount);

        int videoTrackIndex = 100;
        int videoStartOffsetUs = 0;
        int audioTrackIndex = 100;
        int audioStartOffsetUs = 0;
        for (int i = 0; i < trackCount; i++) {
            extractor.selectTrack(i);
            MediaFormat format = extractor.getTrackFormat(i);
            int dstIndex = muxer.addTrack(format);
            indexMap.put(i, dstIndex);
            if (format.getString(MediaFormat.KEY_MIME).startsWith("video/")) {
                videoTrackIndex = i;
                // Make sure there's an entry for video track.
                if (startOffsetUsVect != null && (videoTrackIndex < startOffsetUsVect.size())) {
                    videoStartOffsetUs = startOffsetUsVect.get(videoTrackIndex);
                }
            }
            if (format.getString(MediaFormat.KEY_MIME).startsWith("audio/")) {
                audioTrackIndex = i;
                // Make sure there's an entry for audio track.
                if (startOffsetUsVect != null && (audioTrackIndex < startOffsetUsVect.size())) {
                    audioStartOffsetUs = startOffsetUsVect.get(audioTrackIndex);
                }
            }
        }

        // Copy the samples from MediaExtractor to MediaMuxer.
        boolean sawEOS = false;
        int bufferSize = MAX_SAMPLE_SIZE;
        int sampleCount = 0;
        int offset = 0;
        int videoSampleCount = 0;

        ByteBuffer dstBuf = ByteBuffer.allocate(bufferSize);
        BufferInfo bufferInfo = new BufferInfo();

        muxer.start();
        while (!sawEOS) {
            bufferInfo.offset = 0;
            bufferInfo.size = extractor.readSampleData(dstBuf, offset);
            if (bufferInfo.size < 0) {
                if (VERBOSE) {
                    Log.d(TAG, "saw input EOS.");
                }
                sawEOS = true;
                bufferInfo.size = 0;
            } else {
                bufferInfo.presentationTimeUs = extractor.getSampleTime();
                bufferInfo.flags = extractor.getSampleFlags();
                int trackIndex = extractor.getSampleTrackIndex();
                if (VERBOSE) {
                    Log.v(TAG, "TrackIndex:" + trackIndex + " PresentationTimeUs:" +
                                bufferInfo.presentationTimeUs + " Flags:" + bufferInfo.flags +
                                " Size(bytes)" + bufferInfo.size);
                }
                if (trackIndex == videoTrackIndex) {
                    ++videoSampleCount;
                    if (VERBOSE) {
                        Log.v(TAG, "videoSampleCount : " + videoSampleCount);
                    }
                    if (samplesDropSet == null || (!samplesDropSet.contains(videoSampleCount))) {
                        // Write video frame with start offset adjustment.
                        bufferInfo.presentationTimeUs += videoStartOffsetUs;
                        muxer.writeSampleData(indexMap.get(trackIndex), dstBuf, bufferInfo);
                    }
                    else {
                        if (VERBOSE) {
                            Log.v(TAG, "skipped this frame");
                        }
                    }
                } else {
                    // write audio sample with start offset adjustment.
                    bufferInfo.presentationTimeUs += audioStartOffsetUs;
                    muxer.writeSampleData(indexMap.get(trackIndex), dstBuf, bufferInfo);
                }
                extractor.advance();
                sampleCount++;
                if (VERBOSE) {
                    Log.i(TAG, "Sample (" + sampleCount + ")" +
                            " TrackIndex:" + trackIndex +
                            " PresentationTimeUs:" + bufferInfo.presentationTimeUs +
                            " Flags:" + bufferInfo.flags +
                            " Size(bytes)" + bufferInfo.size );
                }
            }
        }

        muxer.stop();
        muxer.release();
        extractor.release();
        srcFd.close();

        return;
    }

    /*
     * Uses MediaExtractors and checks whether timestamps of all samples except in samplesDropSet
     *  and with start offsets adjustments for each track match.
     */
    private void verifyTSWithSamplesDropAndStartOffset(int srcMediaId, boolean hasBframes,
            String testMediaPath, HashSet<Integer> samplesDropSet,
            Vector<Integer> startOffsetUsVect) throws IOException {
        AssetFileDescriptor srcFd = mResources.openRawResourceFd(srcMediaId);
        MediaExtractor extractorSrc = new MediaExtractor();
        extractorSrc.setDataSource(srcFd.getFileDescriptor(),
            srcFd.getStartOffset(), srcFd.getLength());
        MediaExtractor extractorTest = new MediaExtractor();
        extractorTest.setDataSource(testMediaPath);

        int videoTrackIndex = -1;
        int videoStartOffsetUs = 0;
        int minStartOffsetUs = Integer.MAX_VALUE;
        int trackCount = extractorSrc.getTrackCount();

        /*
         * When all track's start offsets are positive, MPEG4Writer makes the start timestamp of the
         * earliest track as zero and adjusts all other tracks' timestamp accordingly.
         */
        // TODO: need to confirm if the above logic holds good with all others writers we support.
        if (startOffsetUsVect != null) {
            for (int startOffsetUs : startOffsetUsVect) {
                minStartOffsetUs = Math.min(startOffsetUs, minStartOffsetUs);
            }
        } else {
            minStartOffsetUs = 0;
        }

        if (minStartOffsetUs < 0) {
            /*
             * Atleast one of the start offsets were negative. We have some test cases with negative
             * offsets for audio, minStartOffset has to be reset as Writer won't adjust any of the
             * track's timestamps.
             */
            minStartOffsetUs = 0;
        }

        // Select video track.
        for (int i = 0; i < trackCount; i++) {
            MediaFormat format = extractorSrc.getTrackFormat(i);
            if (format.getString(MediaFormat.KEY_MIME).startsWith("video/")) {
                videoTrackIndex = i;
                if (startOffsetUsVect != null && videoTrackIndex < startOffsetUsVect.size()) {
                    videoStartOffsetUs = startOffsetUsVect.get(videoTrackIndex);
                }
                extractorSrc.selectTrack(videoTrackIndex);
                extractorTest.selectTrack(videoTrackIndex);
                checkVideoSamplesTimeStamps(extractorSrc, hasBframes, extractorTest, samplesDropSet,
                    videoStartOffsetUs - minStartOffsetUs);
                extractorSrc.unselectTrack(videoTrackIndex);
                extractorTest.unselectTrack(videoTrackIndex);
            }
        }

        int audioTrackIndex = -1;
        int audioSampleCount = 0;
        int audioStartOffsetUs = 0;
        //select audio track
        for (int i = 0; i < trackCount; i++) {
            MediaFormat format = extractorSrc.getTrackFormat(i);
            if (format.getString(MediaFormat.KEY_MIME).startsWith("audio/")) {
                audioTrackIndex = i;
                if (startOffsetUsVect != null && audioTrackIndex < startOffsetUsVect.size()) {
                    audioStartOffsetUs = startOffsetUsVect.get(audioTrackIndex);
                }
                extractorSrc.selectTrack(audioTrackIndex);
                extractorTest.selectTrack(audioTrackIndex);
                checkAudioSamplesTimestamps(
                        extractorSrc, extractorTest, audioStartOffsetUs - minStartOffsetUs);
            }
        }

        extractorSrc.release();
        extractorTest.release();
        srcFd.close();
    }

    // Check timestamps of all video samples.
    private void checkVideoSamplesTimeStamps(MediaExtractor extractorSrc, boolean hasBFrames,
            MediaExtractor extractorTest, HashSet<Integer> samplesDropSet, int videoStartOffsetUs) {
        long srcSampleTimeUs = -1;
        long testSampleTimeUs = -1;
        boolean srcAdvance = false;
        boolean testAdvance = false;
        int videoSampleCount = 0;

        extractorSrc.seekTo(0, MediaExtractor.SEEK_TO_CLOSEST_SYNC);
        extractorTest.seekTo(0, MediaExtractor.SEEK_TO_CLOSEST_SYNC);

        if (VERBOSE) {
            Log.v(TAG, "srcTrackIndex:" + extractorSrc.getSampleTrackIndex() +
                        "  testTrackIndex:" + extractorTest.getSampleTrackIndex());
            Log.v(TAG, "videoStartOffsetUs:" + videoStartOffsetUs);
        }

        do {
            ++videoSampleCount;
            srcSampleTimeUs = extractorSrc.getSampleTime();
            testSampleTimeUs = extractorTest.getSampleTime();
            if (VERBOSE) {
                Log.v(TAG, "videoSampleCount:" + videoSampleCount);
                Log.i(TAG, "srcTSus:" + srcSampleTimeUs + " testTSus:" + testSampleTimeUs);
            }
            if (samplesDropSet == null || !samplesDropSet.contains(videoSampleCount)) {
                if (srcSampleTimeUs == -1 || testSampleTimeUs == -1) {
                    if (VERBOSE) {
                        Log.v(TAG, "srcUs:" + srcSampleTimeUs + "testUs:" + testSampleTimeUs);
                    }
                    fail("either source or test track reached end of stream");
                }
                /* Stts values within 0.1ms(100us) difference are fudged to save too many
                 * stts entries in MPEG4Writer.
                 */
                else if (Math.abs(srcSampleTimeUs + videoStartOffsetUs - testSampleTimeUs) > 100) {
                    if (VERBOSE) {
                        Log.v(TAG, "Fail:video timestamps didn't match");
                        Log.v(TAG,
                            "srcTrackIndex:" + extractorSrc.getSampleTrackIndex()
                                + "  testTrackIndex:" + extractorTest.getSampleTrackIndex());
                        Log.v(TAG, "srcTSus:" + srcSampleTimeUs + " testTSus:" + testSampleTimeUs);
                  }
                    fail("video timestamps didn't match");
                }
                testAdvance = extractorTest.advance();
            }
            srcAdvance = extractorSrc.advance();
        } while (srcAdvance && testAdvance);
        if (srcAdvance != testAdvance) {
            if (VERBOSE) {
                Log.v(TAG, "videoSampleCount:" + videoSampleCount);
            }
            fail("either video track has not reached its last sample");
        }
    }

    private void checkAudioSamplesTimestamps(MediaExtractor extractorSrc,
                MediaExtractor extractorTest, int audioStartOffsetUs) {
        long srcSampleTimeUs = -1;
        long testSampleTimeUs = -1;
        boolean srcAdvance = false;
        boolean testAdvance = false;
        int audioSampleCount = 0;

        extractorSrc.seekTo(0, MediaExtractor.SEEK_TO_CLOSEST_SYNC);
        if (audioStartOffsetUs >= 0) {
            // Added edit list support for maintaining only the diff in start offsets of tracks.
            // TODO: Remove this once we add support for preserving absolute timestamps as well.
            extractorTest.seekTo(0, MediaExtractor.SEEK_TO_CLOSEST_SYNC);
        } else {
            extractorTest.seekTo(audioStartOffsetUs, MediaExtractor.SEEK_TO_CLOSEST_SYNC);
        }
        if (VERBOSE) {
            Log.v(TAG, "audioStartOffsetUs:" + audioStartOffsetUs);
            Log.v(TAG, "srcTrackIndex:" + extractorSrc.getSampleTrackIndex() +
                        "  testTrackIndex:" + extractorTest.getSampleTrackIndex());
        }
        // Check timestamps of all audio samples.
        do {
            ++audioSampleCount;
            srcSampleTimeUs = extractorSrc.getSampleTime();
            testSampleTimeUs = extractorTest.getSampleTime();
            if (VERBOSE) {
                Log.v(TAG, "audioSampleCount:" + audioSampleCount);
                Log.v(TAG, "srcTSus:" + srcSampleTimeUs + " testTSus:" + testSampleTimeUs);
            }

            if (srcSampleTimeUs == -1 || testSampleTimeUs == -1) {
                if (VERBOSE) {
                    Log.v(TAG, "srcTSus:" + srcSampleTimeUs + " testTSus:" + testSampleTimeUs);
                }
                fail("either source or test track reached end of stream");
            }
            // > 1us to ignore any round off errors.
            else if (Math.abs(srcSampleTimeUs + audioStartOffsetUs - testSampleTimeUs) > 1) {
                fail("audio timestamps didn't match");
            }
            testAdvance = extractorTest.advance();
            srcAdvance = extractorSrc.advance();
        } while (srcAdvance && testAdvance);
        if (srcAdvance != testAdvance) {
            fail("either audio track has not reached its last sample");
        }
    }
}

