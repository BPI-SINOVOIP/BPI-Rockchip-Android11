/*
 * Copyright (C) 2016 The Android Open Source Project
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
package android.transition.cts;

import static com.android.compatibility.common.util.CtsMockitoUtils.within;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.mockito.Matchers.any;
import static org.mockito.Mockito.timeout;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import android.graphics.Color;
import android.graphics.Matrix;
import android.graphics.drawable.ColorDrawable;
import android.graphics.drawable.Drawable;
import android.transition.ChangeImageTransform;
import android.transition.TransitionManager;
import android.util.DisplayMetrics;
import android.util.TypedValue;
import android.view.ViewGroup;
import android.view.ViewGroup.LayoutParams;
import android.widget.ImageView;
import android.widget.ImageView.ScaleType;

import androidx.test.filters.MediumTest;
import androidx.test.runner.AndroidJUnit4;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mockito;

import java.util.ArrayList;
import java.util.List;
import java.util.Objects;

@MediumTest
@RunWith(AndroidJUnit4.class)
public class ChangeImageTransformTest extends BaseTransitionTest {
    ChangeImageTransform mChangeImageTransform;
    Matrix mStartMatrix;
    Matrix mEndMatrix;
    Drawable mImage;
    ImageView mImageView;

    @Override
    @Before
    public void setup() {
        super.setup();
        resetTransition();
        mStartMatrix = null;
        mEndMatrix = null;
        mImage = null;
        mImageView = null;
    }

    private void resetTransition() {
        mChangeImageTransform = new ChangeImageTransform();
        mChangeImageTransform.setDuration(500);
        mTransition = mChangeImageTransform;
        resetListener();
    }

    @Test
    public void testCenterToFitXY() throws Throwable {
        transformImage(ScaleType.CENTER, ScaleType.FIT_XY);
        verifyMatrixMatches(centerMatrix(), mStartMatrix);
        verifyMatrixMatches(fitXYMatrix(), mEndMatrix);
    }

    @Test
    public void testCenterCropToFitCenter() throws Throwable {
        transformImage(ScaleType.CENTER_CROP, ScaleType.FIT_CENTER);
        verifyMatrixMatches(centerCropMatrix(), mStartMatrix);
        verifyMatrixMatches(fitCenterMatrix(), mEndMatrix);
    }

    @Test
    public void testCenterInsideToFitEnd() throws Throwable {
        transformImage(ScaleType.CENTER_INSIDE, ScaleType.FIT_END);
        // CENTER_INSIDE and CENTER are the same when the image is smaller than the View
        verifyMatrixMatches(centerMatrix(), mStartMatrix);
        verifyMatrixMatches(fitEndMatrix(), mEndMatrix);
    }

    @Test
    public void testFitStartToCenter() throws Throwable {
        transformImage(ScaleType.FIT_START, ScaleType.CENTER);
        verifyMatrixMatches(fitStartMatrix(), mStartMatrix);
        verifyMatrixMatches(centerMatrix(), mEndMatrix);
    }

    @Test
    public void testNoChange() throws Throwable {
        transformImage(ScaleType.CENTER, ScaleType.CENTER);
        verifyMatrixMatches(centerMatrix(), mStartMatrix);
        verifyMatrixMatches(centerMatrix(), mEndMatrix);
    }

    @Test
    public void testNoAnimationForDrawableWithoutSize() throws Throwable {
        transformImage(ScaleType.CENTER_CROP, ScaleType.FIT_XY, new ColorDrawable(Color.WHITE),
                false, false, true);
        Matrix identityMatrix = new Matrix();
        assertEquals(identityMatrix, mStartMatrix);
        assertEquals(identityMatrix, mEndMatrix);
    }

    @Test
    public void testNullAnimatorKeepsImagePadding() throws Throwable {
        transformImage(ScaleType.FIT_XY, ScaleType.FIT_XY, new ColorDrawable(Color.WHITE),
                true, true, true);
        assertEquals(mImage.getBounds().width(), mImageView.getWidth()
                - mImageView.getPaddingLeft() - mImageView.getPaddingRight());
        assertEquals(mImage.getBounds().height(), mImageView.getHeight()
                - mImageView.getPaddingTop() - mImageView.getPaddingBottom());
    }

    private Matrix centerMatrix() {
        int imageWidth = mImage.getIntrinsicWidth();
        int imageViewWidth = mImageView.getWidth();
        float tx = Math.round((imageViewWidth - imageWidth)/2f);

        int imageHeight = mImage.getIntrinsicHeight();
        int imageViewHeight = mImageView.getHeight();
        float ty = Math.round((imageViewHeight - imageHeight)/2f);

        Matrix matrix = new Matrix();
        matrix.postTranslate(tx, ty);
        return matrix;
    }

    private Matrix fitXYMatrix() {
        int imageWidth = mImage.getIntrinsicWidth();
        int imageViewWidth = mImageView.getWidth();
        float scaleX = ((float)imageViewWidth)/imageWidth;

        int imageHeight = mImage.getIntrinsicHeight();
        int imageViewHeight = mImageView.getHeight();
        float scaleY = ((float)imageViewHeight)/imageHeight;

        Matrix matrix = new Matrix();
        matrix.postScale(scaleX, scaleY);
        return matrix;
    }

    private Matrix centerCropMatrix() {
        int imageWidth = mImage.getIntrinsicWidth();
        int imageViewWidth = mImageView.getWidth();
        float scaleX = ((float)imageViewWidth)/imageWidth;

        int imageHeight = mImage.getIntrinsicHeight();
        int imageViewHeight = mImageView.getHeight();
        float scaleY = ((float)imageViewHeight)/imageHeight;

        float maxScale = Math.max(scaleX, scaleY);

        float width = imageWidth * maxScale;
        float height = imageHeight * maxScale;
        float tx = Math.round((imageViewWidth - width) / 2f);
        float ty = Math.round((imageViewHeight - height) / 2f);

        Matrix matrix = new Matrix();
        matrix.postScale(maxScale, maxScale);
        matrix.postTranslate(tx, ty);
        return matrix;
    }

    private Matrix fitCenterMatrix() {
        int imageWidth = mImage.getIntrinsicWidth();
        int imageViewWidth = mImageView.getWidth();
        float scaleX = ((float)imageViewWidth)/imageWidth;

        int imageHeight = mImage.getIntrinsicHeight();
        int imageViewHeight = mImageView.getHeight();
        float scaleY = ((float)imageViewHeight)/imageHeight;

        float minScale = Math.min(scaleX, scaleY);

        float width = imageWidth * minScale;
        float height = imageHeight * minScale;
        float tx = (imageViewWidth - width) / 2f;
        float ty = (imageViewHeight - height) / 2f;

        Matrix matrix = new Matrix();
        matrix.postScale(minScale, minScale);
        matrix.postTranslate(tx, ty);
        return matrix;
    }

    private Matrix fitStartMatrix() {
        int imageWidth = mImage.getIntrinsicWidth();
        int imageViewWidth = mImageView.getWidth();
        float scaleX = ((float)imageViewWidth)/imageWidth;

        int imageHeight = mImage.getIntrinsicHeight();
        int imageViewHeight = mImageView.getHeight();
        float scaleY = ((float)imageViewHeight)/imageHeight;

        float minScale = Math.min(scaleX, scaleY);

        Matrix matrix = new Matrix();
        matrix.postScale(minScale, minScale);
        return matrix;
    }

    private Matrix fitEndMatrix() {
        int imageWidth = mImage.getIntrinsicWidth();
        int imageViewWidth = mImageView.getWidth();
        float scaleX = ((float)imageViewWidth)/imageWidth;

        int imageHeight = mImage.getIntrinsicHeight();
        int imageViewHeight = mImageView.getHeight();
        float scaleY = ((float)imageViewHeight)/imageHeight;

        float minScale = Math.min(scaleX, scaleY);

        float width = imageWidth * minScale;
        float height = imageHeight * minScale;
        float tx = imageViewWidth - width;
        float ty = imageViewHeight - height;

        Matrix matrix = new Matrix();
        matrix.postScale(minScale, minScale);
        matrix.postTranslate(tx, ty);
        return matrix;
    }

    private void verifyMatrixMatches(Matrix expected, Matrix matrix) {
        if (expected == null) {
            assertNull(matrix);
            return;
        }
        assertNotNull(matrix);
        float[] expectedValues = new float[9];
        expected.getValues(expectedValues);

        float[] values = new float[9];
        matrix.getValues(values);

        for (int i = 0; i < values.length; i++) {
            final float expectedValue = expectedValues[i];
            final float value = values[i];
            assertEquals("Value [" + i + "]", expectedValue, value, 0.01f);
        }
    }

    private void transformImage(final ScaleType startScale,
            final ImageView.ScaleType endScale) throws Throwable {
        transformImage(startScale, endScale, null, false, false, startScale == endScale);
    }

    private void transformImage(final ScaleType startScale,
            final ImageView.ScaleType endScale,
            final Drawable customImage,
            final boolean applyPadding,
            final boolean withChangingSize,
            final boolean noMatrixChangeExpected) throws Throwable {
        final ImageView imageView = enterImageViewScene(startScale, customImage, applyPadding);
        final List<Matrix> matrices = watchImageMatrix(imageView);

        mActivityRule.runOnUiThread(() -> {
            imageView.invalidate();
        });

        // Wait for one draw() call
        verify(matrices, within(5000)).add(any());

        mActivityRule.runOnUiThread(() -> {
            TransitionManager.beginDelayedTransition(mSceneRoot, mChangeImageTransform);
            if (withChangingSize) {
                imageView.getLayoutParams().height /= 2;
                imageView.requestLayout();
            }
            imageView.setScaleType(endScale);
            imageView.invalidate();
        });
        waitForStart();
        waitForEnd(5000);
        if (noMatrixChangeExpected) {
            verify(matrices, times(1)).add(any());
            assertEquals(1, matrices.size());
        } else {
            verify(matrices, timeout(5000).atLeast(3)).add(any());
        }
        mStartMatrix = matrices.get(0);
        mEndMatrix = matrices.get(matrices.size() - 1);
    }

    private List<Matrix> watchImageMatrix(ImageView view) throws Throwable {
        final List<Matrix> matrices = Mockito.spy(new ArrayList<>());
        mActivityRule.runOnUiThread(() -> {
            mActivity.getWindow().getDecorView().getViewTreeObserver().addOnDrawListener(() -> {
                Matrix matrix = view.getImageMatrix();
                if (matrices.isEmpty()
                        || !Objects.equals(matrix, matrices.get(matrices.size() - 1))) {
                    if (matrix == null) {
                        matrices.add(matrix);
                    } else {
                        matrices.add(new Matrix(matrix));
                    }
                }
            });
        });
        return matrices;
    }

    private ImageView enterImageViewScene(final ScaleType scaleType,
            final Drawable customImage,
            final boolean withPadding) throws Throwable {
        enterScene(R.layout.scene4);
        final ViewGroup container = mActivity.findViewById(R.id.holder);
        final ImageView[] imageViews = new ImageView[1];
        mActivityRule.runOnUiThread(() -> {
            mImageView = new ImageView(mActivity);
            if (customImage != null) {
                mImage = customImage;
            } else {
                mImage = mActivity.getDrawable(android.R.drawable.ic_media_play);
            }
            mImageView.setImageDrawable(mImage);
            mImageView.setScaleType(scaleType);
            imageViews[0] = mImageView;
            container.addView(mImageView);
            LayoutParams layoutParams = mImageView.getLayoutParams();
            DisplayMetrics metrics = mActivity.getResources().getDisplayMetrics();
            float size = TypedValue.applyDimension(TypedValue.COMPLEX_UNIT_DIP, 50, metrics);
            layoutParams.width = Math.round(size);
            layoutParams.height = Math.round(size * 2);
            mImageView.setLayoutParams(layoutParams);
            if (withPadding) {
                int padding = (int) TypedValue.applyDimension(TypedValue.COMPLEX_UNIT_DIP, 8,
                        metrics);
                mImageView.setPadding(padding, padding, padding, padding);
            }
        });
        mInstrumentation.waitForIdleSync();
        return imageViews[0];
    }
}

