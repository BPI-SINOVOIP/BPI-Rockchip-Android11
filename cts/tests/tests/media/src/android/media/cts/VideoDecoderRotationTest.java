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

package android.media.cts;

import android.media.MediaCodecInfo;
import android.media.MediaCodecList;
import android.media.MediaFormat;
import android.platform.test.annotations.RequiresDevice;
import android.util.Log;
import android.util.Size;

import androidx.test.filters.SmallTest;
import androidx.test.platform.app.InstrumentationRegistry;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collection;
import java.util.List;

import org.junit.runner.RunWith;
import org.junit.runners.Parameterized;
import org.junit.runners.Parameterized.Parameter;
import org.junit.runners.Parameterized.Parameters;
import org.junit.Test;

/**
 * Tests to check if MediaCodec decoding works with rotation.
 */
@SmallTest
@RequiresDevice
@RunWith(Parameterized.class)
public class VideoDecoderRotationTest {
    private static final String TAG = "VideoDecoderRotationTest";

    private final EncodeVirtualDisplayWithCompositionTestImpl mImpl =
            new EncodeVirtualDisplayWithCompositionTestImpl();

    @Parameter(0)
    public String mDecoderName;
    @Parameter(1)
    public String mMimeType;
    @Parameter(2)
    public Integer mDegrees;

    private static final List<String> SUPPORTED_TYPES = Arrays.asList(
            MediaFormat.MIMETYPE_VIDEO_AVC,
            MediaFormat.MIMETYPE_VIDEO_HEVC,
            MediaFormat.MIMETYPE_VIDEO_VP8,
            MediaFormat.MIMETYPE_VIDEO_VP9,
            MediaFormat.MIMETYPE_VIDEO_AV1);

    @Parameters(name = "{0}:{1}:{2}")
    public static Collection<Object[]> data() {
        final List<Object[]> testParams = new ArrayList<>();
        MediaCodecList mcl = new MediaCodecList(MediaCodecList.REGULAR_CODECS);
        for (MediaCodecInfo info : mcl.getCodecInfos()) {
            if (info.isAlias() || info.isEncoder()) {
                continue;
            }
            for (String type : info.getSupportedTypes()) {
                if (!SUPPORTED_TYPES.contains(type)) {
                    continue;
                }
                testParams.add(new Object[] { info.getName(), type, Integer.valueOf(90) });
                testParams.add(new Object[] { info.getName(), type, Integer.valueOf(180) });
                testParams.add(new Object[] { info.getName(), type, Integer.valueOf(270) });
                testParams.add(new Object[] { info.getName(), type, Integer.valueOf(360) });
            }
        }
        return testParams;
    }

    @Test
    public void testRendering800x480Rotated() throws Throwable {
        if (mImpl.isConcurrentEncodingDecodingSupported(
                mMimeType, 800, 480, mImpl.BITRATE_800x480, mDecoderName)) {
            mImpl.runTestRenderingInSeparateThread(
                    InstrumentationRegistry.getInstrumentation().getContext(),
                    mMimeType, 800, 480, false, false, mDegrees, mDecoderName);
        } else {
            Log.i(TAG, "SKIPPING testRendering800x480Rotated" + mDegrees + ":codec (" +
                    mDecoderName + ":" + mMimeType + ") not supported");
        }
    }
}
