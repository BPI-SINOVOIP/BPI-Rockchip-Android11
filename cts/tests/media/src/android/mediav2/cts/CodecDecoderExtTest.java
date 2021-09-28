/*
 * Copyright (C) 2020 The Android Open Source Project
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

import android.media.MediaExtractor;
import android.media.MediaFormat;

import androidx.test.filters.LargeTest;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.Parameterized;

import java.io.IOException;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collection;
import java.util.HashSet;
import java.util.List;
import java.util.Set;

import static org.junit.Assert.assertTrue;
import static org.junit.Assert.fail;

@RunWith(Parameterized.class)
public class CodecDecoderExtTest extends CodecDecoderTestBase {
    private static final String LOG_TAG = CodecDecoderExtTest.class.getSimpleName();

    private final String mRefFile;

    public CodecDecoderExtTest(String mime, String testFile, String refFile) {
        super(mime, testFile);
        mRefFile = refFile;
    }

    @Parameterized.Parameters(name = "{index}({0})")
    public static Collection<Object[]> input() {
        final List<Object[]> exhaustiveArgsList = Arrays.asList(new Object[][]{
                {MediaFormat.MIMETYPE_VIDEO_VP9,
                        //show and no-show frames are sent as separate inputs
                        "bbb_340x280_768kbps_30fps_split_non_display_frame_vp9.webm",
                        //show and no-show frames are sent as one input
                        "bbb_340x280_768kbps_30fps_vp9.webm"},
                {MediaFormat.MIMETYPE_VIDEO_VP9,
                        //show and no-show frames are sent as separate inputs
                        "bbb_520x390_1mbps_30fps_split_non_display_frame_vp9.webm",
                        //show and no-show frames are sent as one input
                        "bbb_520x390_1mbps_30fps_vp9.webm"},
                {MediaFormat.MIMETYPE_VIDEO_MPEG2,
                        //show and no-show frames are sent as separate inputs
                        "bbb_512x288_30fps_1mbps_mpeg2_interlaced_nob_1field.ts",
                        //show and no-show frames are sent as one input
                        "bbb_512x288_30fps_1mbps_mpeg2_interlaced_nob_2fields.mp4"},
        });

        Set<String> list = new HashSet<>();
        if (isHandheld() || isTv() || isAutomotive()) {
            // sec 2.2.2, 2.3.2, 2.5.2
            list.add(MediaFormat.MIMETYPE_VIDEO_VP9);
        }
        if (isTv()) {
            // sec 2.3.2
            list.add(MediaFormat.MIMETYPE_VIDEO_MPEG2);
        }
        ArrayList<String> cddRequiredMimeList = new ArrayList<>(list);

        return prepareParamList(cddRequiredMimeList, exhaustiveArgsList, false);
    }

    /**
     * Test decodes and compares decoded output of two files.
     */
    @LargeTest
    @Test(timeout = PER_TEST_TIMEOUT_LARGE_TEST_MS)
    public void testDecodeAndValidate() throws IOException, InterruptedException {
        ArrayList<String> listOfDecoders = selectCodecs(mMime, null, null, false);
        if (listOfDecoders.isEmpty()) {
            fail("no suitable codecs found for mime: " + mMime);
        }
        final int mode = MediaExtractor.SEEK_TO_CLOSEST_SYNC;
        for (String decoder : listOfDecoders) {
            decodeToMemory(mTestFile, decoder, 0, mode, Integer.MAX_VALUE);
            OutputManager test = mOutputBuff;
            String log = String.format("codec: %s, test file: %s, ref file: %s:: ", decoder,
                    mTestFile, mRefFile);
            assertTrue(log + " unexpected error", !mAsyncHandle.hasSeenError());
            assertTrue(log + "no input sent", 0 != mInputCount);
            assertTrue(log + "output received", 0 != mOutputCount);
            if (mIsAudio) {
                assertTrue("reference output pts is not strictly increasing",
                        test.isPtsStrictlyIncreasing(mPrevOutputPts));
            } else {
                assertTrue("input pts list and output pts list are not identical",
                        test.isOutPtsListIdenticalToInpPtsList(false));
            }
            decodeToMemory(mRefFile, decoder, 0, mode, Integer.MAX_VALUE);
            OutputManager ref = mOutputBuff;
            assertTrue(log + "decoder outputs are not identical", ref.equals(test));
        }
    }
}
