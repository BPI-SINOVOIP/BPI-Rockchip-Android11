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

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertTrue;
import static org.mockito.Matchers.any;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;

import android.graphics.PointF;
import android.transition.Explode;
import android.transition.TransitionManager;
import android.view.View;

import androidx.test.filters.MediumTest;
import androidx.test.runner.AndroidJUnit4;

import com.android.compatibility.common.util.PollingCheck;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import java.util.List;

@MediumTest
@RunWith(AndroidJUnit4.class)
public class ExplodeTest extends BaseTransitionTest {
    Explode mExplode;

    @Override
    @Before
    public void setup() {
        super.setup();
        resetTransition();
    }

    private void resetTransition() {
        mExplode = new Explode();
        mExplode.setDuration(1000);
        mTransition = mExplode;
        resetListener();
    }

    @Test
    public void testExplode() throws Throwable {
        enterScene(R.layout.scene10);
        final View redSquare = mActivity.findViewById(R.id.redSquare);
        final View greenSquare = mActivity.findViewById(R.id.greenSquare);
        final View blueSquare = mActivity.findViewById(R.id.blueSquare);
        final View yellowSquare = mActivity.findViewById(R.id.yellowSquare);

        final List<PointF> redPoints = captureTranslations(redSquare);
        final List<PointF> greenPoints = captureTranslations(greenSquare);
        final List<PointF> bluePoints = captureTranslations(blueSquare);
        final List<PointF> yellowPoints = captureTranslations(yellowSquare);

        mActivityRule.runOnUiThread(() -> {
            TransitionManager.beginDelayedTransition(mSceneRoot, mTransition);
            redSquare.setVisibility(View.INVISIBLE);
            greenSquare.setVisibility(View.INVISIBLE);
            blueSquare.setVisibility(View.INVISIBLE);
            yellowSquare.setVisibility(View.INVISIBLE);
        });
        waitForStart();
        verify(mListener, never()).onTransitionEnd(any());
        assertEquals(View.VISIBLE, redSquare.getVisibility());
        assertEquals(View.VISIBLE, greenSquare.getVisibility());
        assertEquals(View.VISIBLE, blueSquare.getVisibility());
        assertEquals(View.VISIBLE, yellowSquare.getVisibility());

        waitForEnd(5000);
        verifyMovement(redPoints, false, false, true);
        verifyMovement(greenPoints, true, false, true);
        verifyMovement(bluePoints, true, true, true);
        verifyMovement(yellowPoints, false, true, true);

        verifyNoTranslation(redSquare);
        verifyNoTranslation(greenSquare);
        verifyNoTranslation(blueSquare);
        verifyNoTranslation(yellowSquare);
        assertEquals(View.INVISIBLE, redSquare.getVisibility());
        assertEquals(View.INVISIBLE, greenSquare.getVisibility());
        assertEquals(View.INVISIBLE, blueSquare.getVisibility());
        assertEquals(View.INVISIBLE, yellowSquare.getVisibility());
    }

    @Test
    public void testImplode() throws Throwable {
        enterScene(R.layout.scene10);
        final View redSquare = mActivity.findViewById(R.id.redSquare);
        final View greenSquare = mActivity.findViewById(R.id.greenSquare);
        final View blueSquare = mActivity.findViewById(R.id.blueSquare);
        final View yellowSquare = mActivity.findViewById(R.id.yellowSquare);

        final List<PointF> redPoints = captureTranslations(redSquare);
        final List<PointF> greenPoints = captureTranslations(greenSquare);
        final List<PointF> bluePoints = captureTranslations(blueSquare);
        final List<PointF> yellowPoints = captureTranslations(yellowSquare);

        mActivityRule.runOnUiThread(() -> {
            redSquare.setVisibility(View.INVISIBLE);
            greenSquare.setVisibility(View.INVISIBLE);
            blueSquare.setVisibility(View.INVISIBLE);
            yellowSquare.setVisibility(View.INVISIBLE);
        });
        mInstrumentation.waitForIdleSync();

        mActivityRule.runOnUiThread(() -> {
            TransitionManager.beginDelayedTransition(mSceneRoot, mTransition);
            redSquare.setVisibility(View.VISIBLE);
            greenSquare.setVisibility(View.VISIBLE);
            blueSquare.setVisibility(View.VISIBLE);
            yellowSquare.setVisibility(View.VISIBLE);
        });
        waitForStart();

        assertEquals(View.VISIBLE, redSquare.getVisibility());
        assertEquals(View.VISIBLE, greenSquare.getVisibility());
        assertEquals(View.VISIBLE, blueSquare.getVisibility());
        assertEquals(View.VISIBLE, yellowSquare.getVisibility());

        waitForEnd(5000);
        verifyMovement(redPoints, true, true, false);
        verifyMovement(greenPoints, false, true, false);
        verifyMovement(bluePoints, false, false, false);
        verifyMovement(yellowPoints, true, false, false);

        verifyNoTranslation(redSquare);
        verifyNoTranslation(greenSquare);
        verifyNoTranslation(blueSquare);
        verifyNoTranslation(yellowSquare);
        assertEquals(View.VISIBLE, redSquare.getVisibility());
        assertEquals(View.VISIBLE, greenSquare.getVisibility());
        assertEquals(View.VISIBLE, blueSquare.getVisibility());
        assertEquals(View.VISIBLE, yellowSquare.getVisibility());
    }

    private void verifyMovement(List<PointF> points, boolean moveRight, boolean moveDown,
            boolean explode) {
        int numPoints = points.size();
        assertTrue(numPoints > 3);

        // skip the first point -- it is the value before the change
        PointF firstPoint = points.get(1);

        // Skip the last point -- it may be the settled value after the change
        PointF lastPoint = points.get(numPoints - 2);

        assertNotEquals(lastPoint.x, firstPoint.x);
        assertNotEquals(lastPoint.y, firstPoint.y);
        assertEquals(moveRight, firstPoint.x < lastPoint.x);
        assertEquals(moveDown, firstPoint.y < lastPoint.y);

        assertEquals(explode, Math.abs(firstPoint.x) < Math.abs(lastPoint.x));
        assertEquals(explode, Math.abs(firstPoint.y) < Math.abs(lastPoint.y));
    }

    private void waitForMovement(View view, float startX, float startY) {
        PollingCheck.waitFor(5000, () -> hasMoved(view, startX, startY));
    }

    private boolean hasMoved(View view, float x, float y) {
        return Math.abs(view.getTranslationX() - x) > 2f
                || Math.abs(view.getTranslationY() - y) > 2f;
    }

    private void verifyNoTranslation(View view) {
        assertEquals(0f, view.getTranslationX(), 0.0f);
        assertEquals(0f, view.getTranslationY(), 0.0f);
    }
}

