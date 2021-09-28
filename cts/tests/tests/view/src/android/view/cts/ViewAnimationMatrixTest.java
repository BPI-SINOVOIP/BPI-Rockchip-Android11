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

package android.view.cts;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;

import android.graphics.Bitmap;
import android.graphics.Color;
import android.graphics.Matrix;
import android.graphics.Rect;
import android.os.Handler;
import android.os.Looper;
import android.view.PixelCopy;
import android.view.View;
import android.view.ViewGroup;

import androidx.test.InstrumentationRegistry;
import androidx.test.annotation.UiThreadTest;
import androidx.test.filters.SmallTest;
import androidx.test.rule.ActivityTestRule;
import androidx.test.runner.AndroidJUnit4;

import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;

@SmallTest
@RunWith(AndroidJUnit4.class)
public class ViewAnimationMatrixTest {

    @Rule
    public ActivityTestRule<ViewAnimationMatrixActivity> mRule =
            new ActivityTestRule<>(ViewAnimationMatrixActivity.class, false, false);

    @UiThreadTest
    @Test
    public void testAnimationMatrixGetter() {
        View view = new View(InstrumentationRegistry.getTargetContext());
        Matrix matrix = new Matrix();
        matrix.setTranslate(34, 65);
        matrix.setRotate(45);
        view.setAnimationMatrix(matrix);

        assertEquals(matrix, view.getAnimationMatrix());
    }

    @Test
    public void testAnimationMatrixAppliedDuringDrawing() throws Throwable {
        ViewAnimationMatrixActivity activity = mRule.launchActivity(null);
        final View view = activity.mView;
        final View root = activity.mRoot;

        // view has some offset and rotation originally
        mRule.runOnUiThread(() -> view.setAnimationMatrix(moveToTopLeftCorner(view)));
        // now it should be in the top left corner of the parent

        waitForDraw(root);
        Bitmap bitmap = captureView(root, view.getWidth(), view.getHeight());

        assertAllPixelsAre(Color.BLACK, bitmap);
    }

    @Test
    public void testAnimationMatrixClearedWithPassingNull() throws Throwable {
        ViewAnimationMatrixActivity activity = mRule.launchActivity(null);
        final View view = activity.mView;
        final View root = activity.mRoot;

        mRule.runOnUiThread(() -> view.setAnimationMatrix(moveToTopLeftCorner(view)));
        mRule.runOnUiThread(() -> view.setAnimationMatrix(null));

        waitForDraw(root);
        Bitmap bitmap = captureView(root, view.getWidth(), view.getHeight());

        // view should again be drawn with the original offset, so our target rect is empty
        assertAllPixelsAre(Color.WHITE, bitmap);
    }

    private Matrix moveToTopLeftCorner(View view) {
        final ViewGroup.MarginLayoutParams lp =
                (ViewGroup.MarginLayoutParams) view.getLayoutParams();
        Matrix matrix = new Matrix();
        matrix.setRotate(-view.getRotation(), lp.width / 2f, lp.height / 2f);
        matrix.postTranslate(-lp.leftMargin, 0);
        return matrix;
    }

    private Bitmap captureView(final View view, int width, int height) throws Throwable {
        final Bitmap dest = Bitmap.createBitmap(width, height, Bitmap.Config.ARGB_8888);
        final CountDownLatch latch = new CountDownLatch(1);
        int[] offset = new int[2];
        view.getLocationInWindow(offset);
        Rect srcRect = new Rect(0, 0, width, height);
        srcRect.offset(offset[0], offset[1]);
        PixelCopy.OnPixelCopyFinishedListener onCopyFinished =
                copyResult -> {
                    assertEquals(PixelCopy.SUCCESS, copyResult);
                    latch.countDown();
                };
        PixelCopy.request(mRule.getActivity().getWindow(), srcRect, dest, onCopyFinished,
                new Handler(Looper.getMainLooper()));
        assertTrue(latch.await(1, TimeUnit.SECONDS));
        return dest;
    }

    private void waitForDraw(final View view) throws Throwable {
        final CountDownLatch latch = new CountDownLatch(1);
        mRule.runOnUiThread(() -> {
            view.getViewTreeObserver().registerFrameCommitCallback(latch::countDown);
            view.invalidate();
        });
        assertTrue(latch.await(1, TimeUnit.SECONDS));
    }

    private void assertAllPixelsAre(int color, Bitmap bitmap) {
        // skipping a few border pixels in case of antialiazing
        for (int i = 2; i < bitmap.getWidth() - 2; i++) {
            for (int j = 2; j < bitmap.getHeight() - 2; j++) {
                assertEquals(color, bitmap.getPixel(i, j));
            }
        }
    }

}
