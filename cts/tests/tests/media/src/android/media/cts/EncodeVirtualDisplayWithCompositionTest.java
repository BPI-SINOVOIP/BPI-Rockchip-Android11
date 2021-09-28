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

import android.media.MediaFormat;
import android.platform.test.annotations.Presubmit;
import android.platform.test.annotations.RequiresDevice;
import android.test.AndroidTestCase;
import android.util.Log;
import android.util.Size;

import androidx.test.filters.SmallTest;

/**
 * Tests to check if MediaCodec encoding works with composition of multiple virtual displays
 * The test also tries to destroy and create virtual displays repeatedly to
 * detect any issues. The test itself does not check the output as it is already done in other
 * tests.
 */
@SmallTest
@RequiresDevice
public class EncodeVirtualDisplayWithCompositionTest extends AndroidTestCase {
    private static final String TAG = "EncodeVirtualDisplayWithCompositionTest";
    private static final String MIME_TYPE = MediaFormat.MIMETYPE_VIDEO_AVC;

    private final EncodeVirtualDisplayWithCompositionTestImpl mImpl =
            new EncodeVirtualDisplayWithCompositionTestImpl();

    public void testVirtualDisplayRecycles() throws Exception {
        mImpl.doTestVirtualDisplayRecycles(getContext(), 3);
    }

    @Presubmit
    public void testRendering800x480Locally() throws Throwable {
        Log.i(TAG, "testRendering800x480Locally");
        if (mImpl.isConcurrentEncodingDecodingSupported(
                MIME_TYPE, 800, 480, mImpl.BITRATE_800x480)) {
            mImpl.runTestRenderingInSeparateThread(
                    getContext(), MIME_TYPE, 800, 480, false, false);
        } else {
            Log.i(TAG, "SKIPPING testRendering800x480Locally(): codec not supported");
        }
    }

    @Presubmit
    public void testRenderingMaxResolutionLocally() throws Throwable {
        Log.i(TAG, "testRenderingMaxResolutionLocally");
        Size maxRes = mImpl.checkMaxConcurrentEncodingDecodingResolution();
        if (maxRes == null) {
            Log.i(TAG, "SKIPPING testRenderingMaxResolutionLocally(): codec not supported");
        } else {
            Log.w(TAG, "Trying resolution " + maxRes);
            mImpl.runTestRenderingInSeparateThread(
                    getContext(), MIME_TYPE, maxRes.getWidth(), maxRes.getHeight(), false, false);
        }
    }

    @Presubmit
    public void testRendering800x480Remotely() throws Throwable {
        Log.i(TAG, "testRendering800x480Remotely");
        if (mImpl.isConcurrentEncodingDecodingSupported(
                MIME_TYPE, 800, 480, mImpl.BITRATE_800x480)) {
            mImpl.runTestRenderingInSeparateThread(getContext(), MIME_TYPE, 800, 480, true, false);
        } else {
            Log.i(TAG, "SKIPPING testRendering800x480Remotely(): codec not supported");
        }
    }

    @Presubmit
    public void testRenderingMaxResolutionRemotely() throws Throwable {
        Log.i(TAG, "testRenderingMaxResolutionRemotely");
        Size maxRes = mImpl.checkMaxConcurrentEncodingDecodingResolution();
        if (maxRes == null) {
            Log.i(TAG, "SKIPPING testRenderingMaxResolutionRemotely(): codec not supported");
        } else {
            Log.w(TAG, "Trying resolution " + maxRes);
            mImpl.runTestRenderingInSeparateThread(
                    getContext(), MIME_TYPE, maxRes.getWidth(), maxRes.getHeight(), true, false);
        }
    }

    @Presubmit
    public void testRendering800x480RemotelyWith3Windows() throws Throwable {
        Log.i(TAG, "testRendering800x480RemotelyWith3Windows");
        if (mImpl.isConcurrentEncodingDecodingSupported(
                MIME_TYPE, 800, 480, mImpl.BITRATE_800x480)) {
            mImpl.runTestRenderingInSeparateThread(getContext(), MIME_TYPE, 800, 480, true, true);
        } else {
            Log.i(TAG, "SKIPPING testRendering800x480RemotelyWith3Windows(): codec not supported");
        }
    }

    @Presubmit
    public void testRendering800x480LocallyWith3Windows() throws Throwable {
        Log.i(TAG, "testRendering800x480LocallyWith3Windows");
        if (mImpl.isConcurrentEncodingDecodingSupported(
                MIME_TYPE, 800, 480, mImpl.BITRATE_800x480)) {
            mImpl.runTestRenderingInSeparateThread(getContext(), MIME_TYPE, 800, 480, false, true);
        } else {
            Log.i(TAG, "SKIPPING testRendering800x480LocallyWith3Windows(): codec not supported");
        }
    }
}
