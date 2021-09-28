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

package android.uirendering.cts.testclasses;

import android.graphics.Color;
import android.graphics.Point;
import android.graphics.PorterDuff;
import android.uirendering.cts.bitmapverifiers.SamplePointVerifier;
import android.uirendering.cts.blendmode.BitmapBlendModeCanvasClient;
import android.uirendering.cts.testinfrastructure.ActivityTestBase;

import androidx.test.filters.SmallTest;
import androidx.test.runner.AndroidJUnit4;

import org.junit.Test;
import org.junit.runner.RunWith;

@SmallTest
@RunWith(AndroidJUnit4.class)
public class BitmapBlendModeTest extends ActivityTestBase {

    private static final Point[] SAMPLE_POINTS = {
            new Point(0, 0),
            new Point(TEST_WIDTH - 1, 0),
            new Point(0, TEST_HEIGHT - 1),
            new Point(TEST_WIDTH - 1, TEST_HEIGHT - 1),
            new Point (TEST_WIDTH / 2, TEST_HEIGHT / 2)
    };

    @Test
    public void testClearBlendMode() {
        // Verify that using CLEAR for rendering a bitmap has the same behavior as
        // DST_OUT. This is to verify the backward compatible behavior of using clear ends up
        // utilizing dst_out while rendering a bitmap on API levels 27 and below
        // The output image here should be a red circle with a white background even though
        // the original mask bitmap is drawn with a blue circle
        int[] colors = {Color.WHITE, Color.WHITE, Color.WHITE, Color.WHITE, Color.RED};
        BitmapBlendModeCanvasClient client = new BitmapBlendModeCanvasClient(
                PorterDuff.Mode.CLEAR, PorterDuff.Mode.DST_OVER,
                Color.BLUE, Color.RED);
        createTest().addCanvasClientWithoutUsingPicture(client, true)
                .runWithVerifier(new SamplePointVerifier(SAMPLE_POINTS, colors));
    }

    @Test
    public void testDstOutBlendMode() {
        // Verify that using DST_OUT for rendering a bitmap only clears the overlapping
        // pixels of the bitmap from the destination.
        // The output image here should be a red circle with a white background even though
        // the original mask bitmap is drawn with a blue circle
        int[] colors = {Color.WHITE, Color.WHITE, Color.WHITE, Color.WHITE, Color.RED};
        BitmapBlendModeCanvasClient client = new BitmapBlendModeCanvasClient(
                PorterDuff.Mode.DST_OUT, PorterDuff.Mode.DST_OVER,
                Color.BLUE, Color.RED);
        createTest().addCanvasClientWithoutUsingPicture(client, true)
                .runWithVerifier(new SamplePointVerifier(SAMPLE_POINTS, colors));
    }
}
