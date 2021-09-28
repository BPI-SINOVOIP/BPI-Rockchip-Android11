/*
 * Copyright (C) 2018 The Android Open Source Project
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

import static org.junit.Assert.assertEquals;

import android.graphics.Bitmap;
import android.graphics.BlendMode;
import android.graphics.BlendModeColorFilter;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Paint;
import android.graphics.Point;
import android.uirendering.cts.bitmapverifiers.SamplePointVerifier;
import android.uirendering.cts.testinfrastructure.ActivityTestBase;
import android.uirendering.cts.testinfrastructure.CanvasClient;

import androidx.test.filters.MediumTest;
import androidx.test.runner.AndroidJUnit4;

import org.junit.Test;
import org.junit.runner.RunWith;

@MediumTest
@RunWith(AndroidJUnit4.class)
public class BlendModeColorFilterTest extends ActivityTestBase {
    private static final int TOLERANCE = 5;

    private static final int LEFT_X = TEST_WIDTH / 4;
    private static final int RIGHT_X = TEST_WIDTH * 3 / 4;
    private static final int TOP_Y = TEST_HEIGHT / 4;
    private static final int BOTTOM_Y = TEST_HEIGHT * 3 / 4;

    private static final int FILTER_COLOR = Color.argb(0x80, 0, 0xFF, 0);

    private static final Point[] SAMPLE_POINTS = {
            new Point(LEFT_X, TOP_Y),
            new Point(LEFT_X, BOTTOM_Y),
            new Point(RIGHT_X, BOTTOM_Y)
    };

    private static class BlendModeColorFilterClient implements CanvasClient {

        private final Bitmap mB1;
        private final Bitmap mB2;
        private final BlendMode mMode;
        private final int mFilterColor;

        BlendModeColorFilterClient(int filterColor, BlendMode mode) {
            final Bitmap b1 = Bitmap.createBitmap(TEST_WIDTH / 2,
                            TEST_HEIGHT, Bitmap.Config.ARGB_8888);
            b1.eraseColor(Color.RED);
            final Bitmap b2 = Bitmap.createBitmap(TEST_WIDTH,
                            TEST_HEIGHT / 2, Bitmap.Config.ARGB_8888);
            b2.eraseColor(Color.BLUE);
            mB1 = b1;
            mB2 = b2;
            mMode = mode;
            mFilterColor = filterColor;
        }

        @Override
        public void draw(Canvas canvas, int width, int height) {
            canvas.drawColor(Color.WHITE);

            BlendModeColorFilter filter = new BlendModeColorFilter(mFilterColor, mMode);
            Paint p = new Paint();
            canvas.drawBitmap(mB1, 0, 0, p);
            p.setColorFilter(filter);
            canvas.drawBitmap(mB2, 0, height / 2, p);
        }
    }

    private void testBlendModeColorFilter(int filterColor, BlendMode mode, int[] colors) {
        createTest().addCanvasClient(new BlendModeColorFilterClient(filterColor, mode))
                .runWithVerifier(new SamplePointVerifier(SAMPLE_POINTS, colors));
    }

    @Test
    public void testBlendModeColorFilter_SRC() {
        testBlendModeColorFilter(FILTER_COLOR, BlendMode.SRC,
                new int[]{Color.RED, 0xFF7F8000, 0xFF7FFF7f});
    }

    @Test
    public void testBlendModeColorFilter_DST() {
        testBlendModeColorFilter(FILTER_COLOR, BlendMode.DST,
                new int[]{Color.RED, Color.BLUE, Color.BLUE});
    }

    @Test
    public void testBlendModeColorFilter_SCREEN() {
        testBlendModeColorFilter(Color.GREEN, BlendMode.SCREEN,
                new int[]{Color.RED, Color.CYAN, Color.CYAN});
    }

    @Test
    public void testBlendModeColorFilterGetMode() {
        BlendModeColorFilter filter = new BlendModeColorFilter(Color.CYAN, BlendMode.SOFT_LIGHT);
        assertEquals(BlendMode.SOFT_LIGHT, filter.getMode());
    }

    @Test
    public void testBlendModeColorFilterGetColor() {
        BlendModeColorFilter filter = new BlendModeColorFilter(Color.MAGENTA, BlendMode.HARD_LIGHT);
        assertEquals(Color.MAGENTA, filter.getColor());
    }
}
