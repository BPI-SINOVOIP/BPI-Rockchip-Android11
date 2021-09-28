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
package com.android.devicehealthchecks;

import android.media.MediaCodec;
import android.media.MediaCodecInfo;
import android.media.MediaCodecList;

import org.junit.Assert;
import org.junit.Test;

/*
 * Tests used for basic media validation after the device boot is completed.
 * This test is used for global presubmit.
 */
// TODO: @GlobalPresubmit
public class MediaBootCheck {
    /*
     * Test if codecs are listed and instantiable.
     */
    @Test
    public void checkCodecList() throws Exception {
        /*
         * This is a simple test that checks the following:
         * 1) MediaCodecList is not empty
         * 2) The codecs listed can be created
         */
        MediaCodecList list = new MediaCodecList(MediaCodecList.ALL_CODECS);
        MediaCodecInfo[] infos = list.getCodecInfos();
        Assert.assertTrue("MediaCodecList should not be empty", infos.length > 0);
        for (MediaCodecInfo info : infos) {
            MediaCodec codec = null;
            try {
                codec = MediaCodec.createByCodecName(info.getName());
                // Basic checks.
                Assert.assertEquals(codec.getName(), info.getName());
                Assert.assertEquals(codec.getCanonicalName(), info.getCanonicalName());
            } finally {
                if (codec != null) {
                    codec.release();
                }
            }
        }
    }
}
