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
import android.media.MediaFormat;
import android.media.MediaMuxer;

import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Ignore;
import org.junit.Rule;
import org.junit.Test;
import org.junit.experimental.runners.Enclosed;
import org.junit.rules.TestName;
import org.junit.runner.RunWith;

import java.io.File;
import java.io.FileDescriptor;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.nio.ByteBuffer;
import java.nio.charset.StandardCharsets;

import static android.system.Os.pipe;
import static org.junit.Assert.assertTrue;
import static org.junit.Assert.fail;

/**
 * Tests MediaMuxer API that are independent of MediaMuxer.OutputFormat. Constructors,
 * addTrack, start, writeSampleData, stop, release are independent of OutputFormat selected.
 * Legality of these APIs are tested in this class.
 */
@RunWith(Enclosed.class)
public class MuxerUnitTest {
    // duplicate definitions of hide fields of MediaMuxer.OutputFormat.
    private static final int MUXER_OUTPUT_LAST = MediaMuxer.OutputFormat.MUXER_OUTPUT_OGG;

    @SmallTest
    public static class TestApi {
        @Rule
        public TestName testName = new TestName();

        @Before
        public void prologue() throws IOException {
            mOutMedia = File.createTempFile(testName.getMethodName(), ".out");
            mOutLoc = mOutMedia.getAbsolutePath();
        }

        @After
        public void epilogue() {
            new File(mOutLoc).delete();
        }

        private File mOutMedia;
        private String mOutLoc;

        // Insert one frame SubRip
        static private void insertPerFrameSubtitles(MediaMuxer muxer, long presentationTimeUs,
                int trackID) {
            byte[] greeting = "hello world".getBytes(StandardCharsets.UTF_8);
            ByteBuffer metaBuff = ByteBuffer.allocate(greeting.length);
            metaBuff.put(greeting);
            MediaCodec.BufferInfo metaInfo = new MediaCodec.BufferInfo();
            metaInfo.offset = 0;
            metaInfo.size = greeting.length;
            metaInfo.presentationTimeUs = presentationTimeUs;
            metaInfo.flags = 0;
            muxer.writeSampleData(trackID, metaBuff, metaInfo);
        }

        @Test
        public void testIfNullPathIsRejected() {
            MediaMuxer muxer = null;
            try {
                String nullPath = null;
                muxer = new MediaMuxer(nullPath, MediaMuxer.OutputFormat.MUXER_OUTPUT_3GPP);
                fail("null destination path accepted by constructor");
            } catch (IllegalArgumentException e) {
                // expected
            } catch (Exception e) {
                fail(e.getMessage());
            } finally {
                if (null != muxer) muxer.release();
            }
        }

        @Test
        public void testIfNullFdIsRejected() {
            MediaMuxer muxer = null;
            try {
                FileDescriptor fd = null;
                muxer = new MediaMuxer(fd, MediaMuxer.OutputFormat.MUXER_OUTPUT_3GPP);
                fail("null fd accepted by constructor");
            } catch (IllegalArgumentException e) {
                // expected
            } catch (Exception e) {
                fail(e.getMessage());
            } finally {
                if (null != muxer) muxer.release();
            }
        }

        @Test
        public void testIfInvalidFdIsRejected() {
            MediaMuxer muxer = null;
            try {
                FileDescriptor fd = new FileDescriptor();
                muxer = new MediaMuxer(fd, MediaMuxer.OutputFormat.MUXER_OUTPUT_3GPP);
                fail("Invalid fd accepted by constructor");
            } catch (IllegalArgumentException e) {
                // expected
            } catch (Exception e) {
                fail(e.getMessage());
            } finally {
                if (null != muxer) muxer.release();
            }
        }

        @Test
        public void testIfReadOnlyFdIsRejected() {
            MediaMuxer muxer = null;
            try (FileInputStream fInp = new FileInputStream(mOutMedia)) {
                muxer = new MediaMuxer(fInp.getFD(), MediaMuxer.OutputFormat.MUXER_OUTPUT_3GPP);
                fail("fd with read-only attribute accepted by constructor");
            } catch (IOException e) {
                // expected
            } catch (Exception e) {
                fail(e.getMessage());
            } finally {
                if (null != muxer) muxer.release();
            }
        }

        @Test
        @Ignore("TODO(b/146417874)")
        public void testIfWriteOnlyFdIsRejected() {
            MediaMuxer muxer = null;
            try (FileOutputStream fOut = new FileOutputStream(mOutMedia)) {
                muxer = new MediaMuxer(fOut.getFD(), MediaMuxer.OutputFormat.MUXER_OUTPUT_WEBM);
                fail("fd with write only attribute accepted by constructor");
            } catch (Exception e) {
                // expected
            } finally {
                if (null != muxer) muxer.release();
            }
            assertTrue(mOutMedia.delete());
        }

        @Test
        @Ignore("TODO(b/146417874)")
        public void testIfNonSeekableFdIsRejected() {
            MediaMuxer muxer = null;
            try {
                FileDescriptor[] fd = pipe();
                muxer = new MediaMuxer(fd[1], MediaMuxer.OutputFormat.MUXER_OUTPUT_3GPP);
                fail("pipe, a non-seekable fd accepted by constructor");
            } catch (IOException e) {
                // expected
            } catch (Exception e) {
                fail(e.getMessage());
            } finally {
                if (null != muxer) muxer.release();
            }
        }

        @Test
        public void testIfInvalidOutputFormatIsRejected() {
            MediaMuxer muxer = null;
            try {
                muxer = new MediaMuxer(mOutLoc, MUXER_OUTPUT_LAST + 1);
                fail("Invalid Media format accepted by constructor");
            } catch (IllegalArgumentException e) {
                // expected
            } catch (Exception e) {
                fail(e.getMessage());
            } finally {
                if (null != muxer) muxer.release();
            }
        }

        @Test
        public void testIfNullMediaFormatIsRejected() throws IOException {
            MediaMuxer muxer = new MediaMuxer(mOutLoc, MediaMuxer.OutputFormat.MUXER_OUTPUT_MPEG_4);
            try {
                muxer.addTrack(null);
                fail("null media format accepted by addTrack");
            } catch (IllegalArgumentException e) {
                // expected
            } finally {
                muxer.release();
            }
        }

        @Test
        public void testIfInvalidMediaFormatIsRejected() throws IOException {
            MediaMuxer muxer = new MediaMuxer(mOutLoc, MediaMuxer.OutputFormat.MUXER_OUTPUT_MPEG_4);
            try {
                // Invalid media format - no mime key
                try {
                    muxer.addTrack(new MediaFormat());
                    fail("Invalid media format accepted by addTrack");
                } catch (IllegalArgumentException e) {
                    // expected
                }

                // metadata mime format shall start with "application/*"
                try {
                    MediaFormat format = new MediaFormat();
                    format.setString(MediaFormat.KEY_MIME, MediaFormat.MIMETYPE_TEXT_CEA_608);
                    muxer.addTrack(format);
                    fail("Invalid media format accepted by addTrack");
                } catch (IllegalArgumentException | IllegalStateException e) {
                    // Ideally check only for IllegalArgumentException.
                    // expected
                }
            } finally {
                muxer.release();
            }
        }

        @Test
        @Ignore("TODO(b/146923138)")
        public void testIfCorruptMediaFormatIsRejected() throws IOException {
            MediaMuxer muxer = new MediaMuxer(mOutLoc, MediaMuxer.OutputFormat.MUXER_OUTPUT_MPEG_4);

             /* TODO: Audio/Video formats, have certain keys required to be set. It is noticed
                that even when these keys are not set, no exceptions were raised. Do we need to
                add fixtures for those cases. */
            try {
                MediaFormat format = new MediaFormat();
                format.setString(MediaFormat.KEY_MIME, MediaFormat.MIMETYPE_AUDIO_AAC);
                format.setInteger(MediaFormat.KEY_SAMPLE_RATE, -1);
                muxer.addTrack(format);
                muxer.start();
                fail("muxer accepts media format with required key-value pairs missing");
            } catch (Exception e) {
                // expected
            } finally {
                muxer.release();
            }
        }

        @Test
        public void testIfAddTrackSucceedsAfterStart() throws IOException {
            MediaMuxer muxer = new MediaMuxer(mOutLoc, MediaMuxer.OutputFormat.MUXER_OUTPUT_MPEG_4);
            try {
                MediaFormat format = new MediaFormat();
                format.setString(MediaFormat.KEY_MIME, MediaFormat.MIMETYPE_TEXT_SUBRIP);
                muxer.addTrack(format);
                muxer.start();
                muxer.addTrack(format);
                fail("muxer.addTrack() succeeded after muxer.start()");
            } catch (IllegalStateException e) {
                // expected
            } catch (Exception e) {
                fail(e.getMessage());
            } finally {
                muxer.release();
            }
        }

        @Test
        public void testIfAddTrackSucceedsAfterWriteSampleData() throws IOException {
            MediaMuxer muxer = new MediaMuxer(mOutLoc, MediaMuxer.OutputFormat.MUXER_OUTPUT_MPEG_4);
            try {
                MediaFormat format = new MediaFormat();
                format.setString(MediaFormat.KEY_MIME, MediaFormat.MIMETYPE_TEXT_SUBRIP);
                int trackID = muxer.addTrack(format);
                muxer.start();
                insertPerFrameSubtitles(muxer, 0, trackID);
                muxer.addTrack(format);
                fail("muxer.addTrack() succeeded after muxer.writeSampleData()");
            } catch (IllegalStateException e) {
                // expected
            } catch (Exception e) {
                fail(e.getMessage());
            } finally {
                muxer.release();
            }
        }

        @Test
        public void testIfAddTrackSucceedsAfterStop() throws IOException {
            MediaMuxer muxer = new MediaMuxer(mOutLoc, MediaMuxer.OutputFormat.MUXER_OUTPUT_MPEG_4);
            try {
                MediaFormat format = new MediaFormat();
                format.setString(MediaFormat.KEY_MIME, MediaFormat.MIMETYPE_TEXT_SUBRIP);
                int trackID = muxer.addTrack(format);
                muxer.start();
                insertPerFrameSubtitles(muxer, 0, trackID);
                muxer.stop();
                muxer.addTrack(format);
                fail("muxer.addTrack() succeeded after muxer.stop()");
            } catch (IllegalStateException e) {
                // expected
            } catch (Exception e) {
                fail(e.getMessage());
            } finally {
                muxer.release();
            }
        }

        @Test
        public void testIfAddTrackSucceedsAfterRelease() throws IOException {
            MediaMuxer muxer = new MediaMuxer(mOutLoc, MediaMuxer.OutputFormat.MUXER_OUTPUT_MPEG_4);
            try {
                MediaFormat format = new MediaFormat();
                format.setString(MediaFormat.KEY_MIME, MediaFormat.MIMETYPE_TEXT_SUBRIP);
                muxer.release();
                muxer.addTrack(format);
                fail("muxer.addTrack() succeeded after muxer.release()");
            } catch (IllegalStateException e) {
                // expected
            } catch (Exception e) {
                fail(e.getMessage());
            } finally {
                muxer.release();
            }
        }

        @Test
        public void testIfMuxerStartsBeforeAddTrack() throws IOException {
            MediaMuxer muxer = new MediaMuxer(mOutLoc, MediaMuxer.OutputFormat.MUXER_OUTPUT_MPEG_4);
            MediaFormat format = new MediaFormat();
            format.setString(MediaFormat.KEY_MIME, MediaFormat.MIMETYPE_TEXT_SUBRIP);

            try {
                muxer.start();
                fail("muxer.start() succeeded before muxer.addTrack()");
            } catch (IllegalStateException e) {
                // expected
            } catch (Exception e) {
                fail(e.getMessage());
            } finally {
                muxer.release();
            }
        }

        @Test
        public void testIdempotentStart() throws IOException {
            MediaMuxer muxer = new MediaMuxer(mOutLoc, MediaMuxer.OutputFormat.MUXER_OUTPUT_MPEG_4);
            MediaFormat format = new MediaFormat();
            format.setString(MediaFormat.KEY_MIME, MediaFormat.MIMETYPE_TEXT_SUBRIP);

            try {
                muxer.addTrack(format);
                muxer.start();
                muxer.start();
                fail("muxer.start() succeeded after muxer.start()");
            } catch (IllegalStateException e) {
                // expected
            } catch (Exception e) {
                fail(e.getMessage());
            } finally {
                muxer.release();
            }
        }

        @Test
        public void testIfMuxerStartsAfterWriteSampleData() throws IOException {
            MediaMuxer muxer = new MediaMuxer(mOutLoc, MediaMuxer.OutputFormat.MUXER_OUTPUT_MPEG_4);
            MediaFormat format = new MediaFormat();
            format.setString(MediaFormat.KEY_MIME, MediaFormat.MIMETYPE_TEXT_SUBRIP);

            try {
                int trackID = muxer.addTrack(format);
                muxer.start();
                insertPerFrameSubtitles(muxer, 0, trackID);
                muxer.start();
                fail("muxer.start() succeeded after muxer.writeSampleData()");
            } catch (IllegalStateException e) {
                // expected
            } catch (Exception e) {
                fail(e.getMessage());
            } finally {
                muxer.release();
            }
        }

        @Test
        public void testIfMuxerStartsAfterStop() throws IOException {
            MediaMuxer muxer = new MediaMuxer(mOutLoc, MediaMuxer.OutputFormat.MUXER_OUTPUT_MPEG_4);
            MediaFormat format = new MediaFormat();
            format.setString(MediaFormat.KEY_MIME, MediaFormat.MIMETYPE_TEXT_SUBRIP);

            try {
                int trackID = muxer.addTrack(format);
                muxer.start();
                insertPerFrameSubtitles(muxer, 0, trackID);
                muxer.stop();
                muxer.start();
                fail("muxer.start() succeeded after muxer.stop()");
            } catch (IllegalStateException e) {
                // expected
            } catch (Exception e) {
                fail(e.getMessage());
            } finally {
                muxer.release();
            }
        }

        @Test
        public void testIfMuxerStartsAfterRelease() throws IOException {
            MediaMuxer muxer = new MediaMuxer(mOutLoc, MediaMuxer.OutputFormat.MUXER_OUTPUT_MPEG_4);

            try {
                muxer.release();
                muxer.start();
                fail("muxer.start() succeeded after muxer.release()");
            } catch (IllegalStateException e) {
                // expected
            } catch (Exception e) {
                fail(e.getMessage());
            } finally {
                muxer.release();
            }
        }

        @Test
        public void testStopOnANonStartedMuxer() throws IOException {
            MediaMuxer muxer = new MediaMuxer(mOutLoc, MediaMuxer.OutputFormat.MUXER_OUTPUT_MPEG_4);

            try {
                muxer.stop();
                fail("muxer.stop() succeeded before muxer.start()");
            } catch (IllegalStateException e) {
                // expected
            } catch (Exception e) {
                fail(e.getMessage());
            } finally {
                muxer.release();
            }
        }

        @Test
        public void testIdempotentStop() throws IOException {
            MediaMuxer muxer = new MediaMuxer(mOutLoc, MediaMuxer.OutputFormat.MUXER_OUTPUT_MPEG_4);
            MediaFormat format = new MediaFormat();
            format.setString(MediaFormat.KEY_MIME, MediaFormat.MIMETYPE_TEXT_SUBRIP);

            try {
                int trackID = muxer.addTrack(format);
                muxer.start();
                insertPerFrameSubtitles(muxer, 0, trackID);
                muxer.stop();
                muxer.stop();
                fail("muxer.stop() succeeded after muxer.stop()");
            } catch (IllegalStateException e) {
                // expected
            } catch (Exception e) {
                fail(e.getMessage());
            } finally {
                muxer.release();
            }
        }

        @Test
        public void testStopAfterRelease() throws IOException {
            MediaMuxer muxer = new MediaMuxer(mOutLoc, MediaMuxer.OutputFormat.MUXER_OUTPUT_MPEG_4);
            try {
                muxer.release();
                muxer.stop();
                fail("muxer.stop() succeeded after muxer.release()");
            } catch (IllegalStateException e) {
                // expected
            } catch (Exception e) {
                fail(e.getMessage());
            } finally {
                muxer.release();
            }
        }

        @Test
        public void testSimpleStartStopMuxer() throws IOException {
            MediaMuxer muxer = new MediaMuxer(mOutLoc, MediaMuxer.OutputFormat.MUXER_OUTPUT_MPEG_4);
            MediaFormat format = new MediaFormat();
            format.setString(MediaFormat.KEY_MIME, MediaFormat.MIMETYPE_TEXT_SUBRIP);

            try {
                muxer.addTrack(format);
                muxer.start();
                muxer.stop();
            } catch (Exception e) {
                fail(e.getMessage());
            } finally {
                muxer.release();
            }
        }

        @Test
        public void testIfWriteSampleDataRejectsInvalidTrackIndex() throws IOException {
            MediaMuxer muxer = new MediaMuxer(mOutLoc, MediaMuxer.OutputFormat.MUXER_OUTPUT_MPEG_4);
            MediaFormat format = new MediaFormat();
            format.setString(MediaFormat.KEY_MIME, MediaFormat.MIMETYPE_TEXT_SUBRIP);

            int trackID = muxer.addTrack(format);
            muxer.start();
            insertPerFrameSubtitles(muxer, 22000, trackID);
            try {
                insertPerFrameSubtitles(muxer, 0, trackID - 1);
                fail("muxer.writeSampleData() succeeded with bad argument, trackIndex");
            } catch (IllegalArgumentException e) {
                // expected
            } finally {
                muxer.release();
            }
        }

        @Test
        public void testIfWriteSampleDataRejectsNullByteBuffer() throws IOException {
            MediaMuxer muxer = new MediaMuxer(mOutLoc, MediaMuxer.OutputFormat.MUXER_OUTPUT_MPEG_4);
            MediaFormat format = new MediaFormat();
            format.setString(MediaFormat.KEY_MIME, MediaFormat.MIMETYPE_TEXT_SUBRIP);

            int trackID = muxer.addTrack(format);
            muxer.start();
            insertPerFrameSubtitles(muxer, 22000, trackID);

            MediaCodec.BufferInfo metaInfo = new MediaCodec.BufferInfo();
            metaInfo.offset = 0;
            metaInfo.size = 24;
            metaInfo.presentationTimeUs = 0;
            metaInfo.flags = 0;
            try {
                muxer.writeSampleData(trackID, null, metaInfo);
                fail("muxer.writeSampleData() succeeded with bad argument, byteBuffer = null");
            } catch (IllegalArgumentException e) {
                // expected
            } finally {
                muxer.release();
            }
        }

        @Test
        public void testIfWriteSampleDataRejectsNullBuffInfo() throws IOException {
            MediaMuxer muxer = new MediaMuxer(mOutLoc, MediaMuxer.OutputFormat.MUXER_OUTPUT_MPEG_4);
            MediaFormat format = new MediaFormat();
            format.setString(MediaFormat.KEY_MIME, MediaFormat.MIMETYPE_TEXT_SUBRIP);

            int trackID = muxer.addTrack(format);
            muxer.start();
            insertPerFrameSubtitles(muxer, 22000, trackID);

            byte[] greeting = "hello world".getBytes(StandardCharsets.UTF_8);
            ByteBuffer metaBuff = ByteBuffer.allocate(greeting.length);
            metaBuff.put(greeting);

            try {
                muxer.writeSampleData(trackID, metaBuff, null);
                fail("muxer.writeSampleData() succeeded with bad argument, byteBuffer = null");
            } catch (IllegalArgumentException e) {
                // expected
            } finally {
                muxer.release();
            }
        }

        @Test
        public void testIfWriteSampleDataRejectsInvalidBuffInfo() throws IOException {
            MediaMuxer muxer = new MediaMuxer(mOutLoc, MediaMuxer.OutputFormat.MUXER_OUTPUT_MPEG_4);
            MediaFormat format = new MediaFormat();
            format.setString(MediaFormat.KEY_MIME, MediaFormat.MIMETYPE_TEXT_SUBRIP);

            int trackID = muxer.addTrack(format);
            muxer.start();
            insertPerFrameSubtitles(muxer, 22000, trackID);

            byte[] greeting = "hello world".getBytes(StandardCharsets.UTF_8);
            ByteBuffer metaBuff = ByteBuffer.allocate(greeting.length);
            metaBuff.put(greeting);

            MediaCodec.BufferInfo metaInfo = new MediaCodec.BufferInfo();

            try {
                // invalid metaData : buffer offset < 0
                try {
                    metaInfo.offset = -1;
                    metaInfo.size = greeting.length;
                    metaInfo.presentationTimeUs = 0;
                    metaInfo.flags = 0;
                    muxer.writeSampleData(trackID, metaBuff, metaInfo);
                    fail("muxer.writeSampleData() succeeded with bad argument, bufInfo.offset < 0");
                } catch (IllegalArgumentException e) {
                    // expected
                }

                // invalid metaData : buffer size < 0
                try {
                    metaInfo.offset = 0;
                    metaInfo.size = -1;
                    metaInfo.presentationTimeUs = 0;
                    metaInfo.flags = 0;
                    muxer.writeSampleData(trackID, metaBuff, metaInfo);
                    fail("muxer.writeSampleData() succeeded with bad argument, buffInfo.size < 0");
                } catch (IllegalArgumentException e) {
                    // expected
                }

                // invalid metaData : buffer size > capacity
                try {
                    metaInfo.offset = 0;
                    metaInfo.size = greeting.length * 2;
                    metaInfo.presentationTimeUs = 0;
                    metaInfo.flags = 0;
                    muxer.writeSampleData(trackID, metaBuff, metaInfo);
                    fail("muxer.writeSampleData() succeeded with bad argument, buffInfo.size > " +
                            "byteBuffer.capacity()");
                } catch (IllegalArgumentException e) {
                    // expected
                }

                // invalid metaData : buffer offset + size > capacity
                try {
                    metaInfo.offset = 1;
                    metaInfo.size = greeting.length;
                    metaInfo.presentationTimeUs = 0;
                    metaInfo.flags = 0;
                    muxer.writeSampleData(trackID, metaBuff, metaInfo);
                    fail("muxer.writeSampleData() succeeded with bad argument, bufferInfo.offset " +
                            "+ bufferInfo.size > byteBuffer.capacity()");
                } catch (IllegalArgumentException e) {
                    // expected
                }
            } finally {
                muxer.release();
            }
        }

        @Test
        @Ignore("TODO(b/147128377)")
        public void testIfWriteSampleDataRejectsInvalidPts() throws IOException {
            MediaMuxer muxer = new MediaMuxer(mOutLoc, MediaMuxer.OutputFormat.MUXER_OUTPUT_MPEG_4);
            MediaFormat format = new MediaFormat();
            format.setString(MediaFormat.KEY_MIME, MediaFormat.MIMETYPE_TEXT_SUBRIP);

            int trackID = muxer.addTrack(format);
            muxer.start();
            insertPerFrameSubtitles(muxer, 22000, trackID);
            try {
                insertPerFrameSubtitles(muxer, -33000, trackID);
                fail("muxer.writeSampleData() succeeded with bad argument, presentationTime");
            } catch (IllegalArgumentException e) {
                // expected
            } finally {
                muxer.release();
            }
        }

        @Test
        public void testIfWriteSampleDataSucceedsBeforeStart() throws IOException {
            MediaMuxer muxer = new MediaMuxer(mOutLoc, MediaMuxer.OutputFormat.MUXER_OUTPUT_MPEG_4);
            MediaFormat format = new MediaFormat();
            format.setString(MediaFormat.KEY_MIME, MediaFormat.MIMETYPE_TEXT_SUBRIP);

            try {
                int trackID = muxer.addTrack(format);
                insertPerFrameSubtitles(muxer, 0, trackID);
                fail("muxer.WriteSampleData() succeeds before muxer.start()");
            } catch (IllegalStateException e) {
                // expected
            } catch (Exception e) {
                fail(e.getMessage());
            } finally {
                muxer.release();
            }
        }

        @Test
        public void testIfWriteSampleDataSucceedsAfterStop() throws IOException {
            MediaMuxer muxer = new MediaMuxer(mOutLoc, MediaMuxer.OutputFormat.MUXER_OUTPUT_MPEG_4);
            MediaFormat format = new MediaFormat();
            format.setString(MediaFormat.KEY_MIME, MediaFormat.MIMETYPE_TEXT_SUBRIP);

            try {
                int trackID = muxer.addTrack(format);
                muxer.start();
                insertPerFrameSubtitles(muxer, 0, trackID);
                muxer.stop();
                insertPerFrameSubtitles(muxer, 0, trackID);
                fail("muxer.WriteSampleData() succeeds after muxer.stop()");
            } catch (IllegalStateException e) {
                // expected
            } catch (Exception e) {
                fail(e.getMessage());
            } finally {
                muxer.release();
            }
        }

        @Test
        public void testIfWriteSampleDataSucceedsAfterRelease() throws IOException {
            MediaMuxer muxer = new MediaMuxer(mOutLoc, MediaMuxer.OutputFormat.MUXER_OUTPUT_MPEG_4);
            MediaFormat format = new MediaFormat();
            format.setString(MediaFormat.KEY_MIME, MediaFormat.MIMETYPE_TEXT_SUBRIP);

            try {
                int trackID = muxer.addTrack(format);
                muxer.start();
                insertPerFrameSubtitles(muxer, 0, trackID);
                muxer.release();
                insertPerFrameSubtitles(muxer, 0, trackID);
                fail("muxer.WriteSampleData() succeeds after muxer.release()");
            } catch (IllegalStateException e) {
                // expected
            } catch (Exception e) {
                fail(e.getMessage());
            } finally {
                muxer.release();
            }
        }

        @Test
        public void testIdempotentRelease() throws IOException {
            MediaMuxer muxer = new MediaMuxer(mOutLoc, MediaMuxer.OutputFormat.MUXER_OUTPUT_MPEG_4);
            try {
                muxer.release();
                muxer.release();
            } catch (Exception e) {
                fail(e.getMessage());
            }
        }
    }

    @SmallTest
    public static class TestApiNative {
        @Rule
        public TestName testName = new TestName();

        static {
            System.loadLibrary("ctsmediav2muxer_jni");
        }

        @Before
        public void prologue() throws IOException {
            File mOutMedia = File.createTempFile(testName.getMethodName(), ".out");
            mOutLoc = mOutMedia.getAbsolutePath();
        }

        @After
        public void epilogue() {
            new File(mOutLoc).delete();
        }

        private String mOutLoc;

        private native boolean nativeTestIfInvalidFdIsRejected();
        private native boolean nativeTestIfReadOnlyFdIsRejected(String outPath);
        private native boolean nativeTestIfWriteOnlyFdIsRejected(String outPath);
        private native boolean nativeTestIfNonSeekableFdIsRejected(String outPath);
        private native boolean nativeTestIfInvalidOutputFormatIsRejected(String outPath);

        private native boolean nativeTestIfInvalidMediaFormatIsRejected(String outPath);
        private native boolean nativeTestIfCorruptMediaFormatIsRejected(String outPath);
        private native boolean nativeTestIfAddTrackSucceedsAfterStart(String outPath);
        private native boolean nativeTestIfAddTrackSucceedsAfterWriteSampleData(String outPath);
        private native boolean nativeTestIfAddTrackSucceedsAfterStop(String outPath);

        private native boolean nativeTestIfMuxerStartsBeforeAddTrack(String outPath);
        private native boolean nativeTestIdempotentStart(String outPath);
        private native boolean nativeTestIfMuxerStartsAfterWriteSampleData(String outPath);
        private native boolean nativeTestIfMuxerStartsAfterStop(String outPath);

        private native boolean nativeTestStopOnANonStartedMuxer(String outPath);
        private native boolean nativeTestIdempotentStop(String outPath);
        private native boolean nativeTestSimpleStartStop(String outPath);

        private native boolean nativeTestIfWriteSampleDataRejectsInvalidTrackIndex(String outPath);
        private native boolean nativeTestIfWriteSampleDataRejectsInvalidPts(String outPath);
        private native boolean nativeTestIfWriteSampleDataSucceedsBeforeStart(String outPath);
        private native boolean nativeTestIfWriteSampleDataSucceedsAfterStop(String outPath);

        @Test
        @Ignore("TODO(b/146417874)")
        public void testIfInvalidFdIsRejected() {
            assertTrue(nativeTestIfInvalidFdIsRejected());
        }

        @Test
        @Ignore("TODO(b/146417874)")
        public void testIfReadOnlyFdIsRejected() {
            assertTrue(nativeTestIfReadOnlyFdIsRejected(mOutLoc));
        }

        @Test
        @Ignore("TODO(b/146417874)")
        public void testIfWriteOnlyFdIsRejected() {
            assertTrue(nativeTestIfWriteOnlyFdIsRejected(mOutLoc));
        }

        @Test
        @Ignore("TODO(b/146417874)")
        public void testIfNonSeekableFdIsRejected() {
            assertTrue(nativeTestIfNonSeekableFdIsRejected(mOutLoc));
        }

        @Test
        @Ignore("TODO(b/146417874)")
        public void testIfInvalidOutputFormatIsRejected() {
            assertTrue(nativeTestIfInvalidOutputFormatIsRejected(mOutLoc));
        }

        @Test
        public void testIfInvalidMediaFormatIsRejected() {
            assertTrue(nativeTestIfInvalidMediaFormatIsRejected(mOutLoc));
        }

        @Test
        @Ignore("TODO(b/146923138)")
        public void testIfCorruptMediaFormatIsRejected() {
            assertTrue(nativeTestIfCorruptMediaFormatIsRejected(mOutLoc));
        }

        @Test
        public void testIfAddTrackSucceedsAfterStart() {
            assertTrue(nativeTestIfAddTrackSucceedsAfterStart(mOutLoc));
        }

        @Test
        public void testIfAddTrackSucceedsAfterWriteSampleData() {
            assertTrue(nativeTestIfAddTrackSucceedsAfterWriteSampleData(mOutLoc));
        }

        @Test
        public void testIfAddTrackSucceedsAfterStop() {
            assertTrue(nativeTestIfAddTrackSucceedsAfterStop(mOutLoc));
        }

        @Test
        public void testIfMuxerStartsBeforeAddTrack() {
            assertTrue(nativeTestIfMuxerStartsBeforeAddTrack(mOutLoc));
        }

        @Test
        public void testIdempotentStart() {
            assertTrue(nativeTestIdempotentStart(mOutLoc));
        }

        @Test
        public void testIfMuxerStartsAfterWriteSampleData() {
            assertTrue(nativeTestIfMuxerStartsAfterWriteSampleData(mOutLoc));
        }

        @Test
        public void testIfMuxerStartsAfterStop() {
            assertTrue(nativeTestIfMuxerStartsAfterStop(mOutLoc));
        }

        @Test
        public void testStopOnANonStartedMuxer() {
            assertTrue(nativeTestStopOnANonStartedMuxer(mOutLoc));
        }

        @Test
        public void testIdempotentStop() {
            assertTrue(nativeTestIdempotentStop(mOutLoc));
        }

        @Test
        public void testSimpleStartStopMuxer() {
            assertTrue(nativeTestSimpleStartStop(mOutLoc));
        }

        @Test
        public void testIfWriteSampleDataRejectsInvalidTrackIndex() {
            assertTrue(nativeTestIfWriteSampleDataRejectsInvalidTrackIndex(mOutLoc));
        }

        @Test
        @Ignore("TODO(b/147128377)")
        public void testIfWriteSampleDataRejectsInvalidPts() {
            assertTrue(nativeTestIfWriteSampleDataRejectsInvalidPts(mOutLoc));
        }

        @Test
        public void testIfWriteSampleDataSucceedsBeforeStart() {
            assertTrue(nativeTestIfWriteSampleDataSucceedsBeforeStart(mOutLoc));
        }

        @Test
        public void testIfWriteSampleDataSucceedsAfterStop() {
            assertTrue(nativeTestIfWriteSampleDataSucceedsAfterStop(mOutLoc));
        }
    }
}
