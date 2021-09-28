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
import static org.junit.Assert.assertTrue;
import static org.mockito.Matchers.any;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.verify;

import android.graphics.PointF;
import android.transition.Slide;
import android.transition.Transition;
import android.transition.TransitionManager;
import android.view.Gravity;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewTreeObserver;

import androidx.test.filters.MediumTest;
import androidx.test.runner.AndroidJUnit4;

import org.junit.Test;
import org.junit.runner.RunWith;

import java.util.List;

@MediumTest
@RunWith(AndroidJUnit4.class)
public class SlideEdgeTest extends BaseTransitionTest  {
    private static final Object[][] sSlideEdgeArray = {
            { Gravity.START, "START" },
            { Gravity.END, "END" },
            { Gravity.LEFT, "LEFT" },
            { Gravity.TOP, "TOP" },
            { Gravity.RIGHT, "RIGHT" },
            { Gravity.BOTTOM, "BOTTOM" },
    };

    @Test
    public void testSetSide() throws Throwable {
        for (int i = 0; i < sSlideEdgeArray.length; i++) {
            int slideEdge = (Integer) (sSlideEdgeArray[i][0]);
            String edgeName = (String) (sSlideEdgeArray[i][1]);
            Slide slide = new Slide(slideEdge);
            assertEquals("Edge not set properly in constructor " + edgeName,
                    slideEdge, slide.getSlideEdge());

            slide = new Slide();
            slide.setSlideEdge(slideEdge);
            assertEquals("Edge not set properly with setter " + edgeName,
                    slideEdge, slide.getSlideEdge());
        }
    }

    @Test
    public void testSlideOut() throws Throwable {
        for (int i = 0; i < sSlideEdgeArray.length; i++) {
            final int slideEdge = (Integer) (sSlideEdgeArray[i][0]);
            final Slide slide = new Slide(slideEdge);
            slide.setDuration(1000);
            final Transition.TransitionListener listener =
                    mock(Transition.TransitionListener.class);
            slide.addListener(listener);

            mActivityRule.runOnUiThread(() -> mActivity.setContentView(R.layout.scene1));
            mInstrumentation.waitForIdleSync();

            final View redSquare = mActivity.findViewById(R.id.redSquare);
            final View greenSquare = mActivity.findViewById(R.id.greenSquare);
            final View hello = mActivity.findViewById(R.id.hello);
            final ViewGroup sceneRoot = (ViewGroup) mActivity.findViewById(R.id.holder);
            final List<PointF> redPoints = captureTranslations(redSquare);
            final List<PointF> greenPoints = captureTranslations(greenSquare);
            final List<PointF> helloPoints = captureTranslations(hello);

            mActivityRule.runOnUiThread(() -> {
                TransitionManager.beginDelayedTransition(sceneRoot, slide);
                redSquare.setVisibility(View.INVISIBLE);
                greenSquare.setVisibility(View.INVISIBLE);
                hello.setVisibility(View.INVISIBLE);
                hello.getViewTreeObserver().addOnPreDrawListener(
                        new ViewTreeObserver.OnPreDrawListener() {
                            @Override
                            public boolean onPreDraw() {
                                assertEquals(View.VISIBLE, redSquare.getVisibility());
                                assertEquals(View.VISIBLE, greenSquare.getVisibility());
                                assertEquals(View.VISIBLE, hello.getVisibility());

                                hello.getViewTreeObserver().removeOnPreDrawListener(this);
                                return true;
                            }
                        });
            });
            verify(listener, within(1000)).onTransitionStart(any());
            verify(listener, within(2000)).onTransitionEnd(any());

            verifyMovement(redPoints, slideEdge, false);
            verifyMovement(greenPoints, slideEdge, false);
            verifyMovement(helloPoints, slideEdge, false);
            mInstrumentation.waitForIdleSync();

            verifyNoTranslation(redSquare);
            verifyNoTranslation(greenSquare);
            verifyNoTranslation(hello);
            assertEquals(View.INVISIBLE, redSquare.getVisibility());
            assertEquals(View.INVISIBLE, greenSquare.getVisibility());
            assertEquals(View.INVISIBLE, hello.getVisibility());
        }
    }

    @Test
    public void testSlideIn() throws Throwable {
        for (int i = 0; i < sSlideEdgeArray.length; i++) {
            final int slideEdge = (Integer) (sSlideEdgeArray[i][0]);
            final Slide slide = new Slide(slideEdge);
            slide.setDuration(1000);
            final Transition.TransitionListener listener =
                    mock(Transition.TransitionListener.class);
            slide.addListener(listener);

            mActivityRule.runOnUiThread(() -> mActivity.setContentView(R.layout.scene1));
            mInstrumentation.waitForIdleSync();

            final View redSquare = mActivity.findViewById(R.id.redSquare);
            final View greenSquare = mActivity.findViewById(R.id.greenSquare);
            final View hello = mActivity.findViewById(R.id.hello);
            final ViewGroup sceneRoot = (ViewGroup) mActivity.findViewById(R.id.holder);

            final List<PointF> redPoints = captureTranslations(redSquare);
            final List<PointF> greenPoints = captureTranslations(greenSquare);
            final List<PointF> helloPoints = captureTranslations(hello);

            mActivityRule.runOnUiThread(() -> {
                redSquare.setVisibility(View.INVISIBLE);
                greenSquare.setVisibility(View.INVISIBLE);
                hello.setVisibility(View.INVISIBLE);
            });
            mInstrumentation.waitForIdleSync();

            // now slide in
            mActivityRule.runOnUiThread(() -> {
                TransitionManager.beginDelayedTransition(sceneRoot, slide);
                redSquare.setVisibility(View.VISIBLE);
                greenSquare.setVisibility(View.VISIBLE);
                hello.setVisibility(View.VISIBLE);
                hello.getViewTreeObserver().addOnPreDrawListener(
                        new ViewTreeObserver.OnPreDrawListener() {
                            @Override
                            public boolean onPreDraw() {
                                assertEquals(View.VISIBLE, redSquare.getVisibility());
                                assertEquals(View.VISIBLE, greenSquare.getVisibility());
                                assertEquals(View.VISIBLE, hello.getVisibility());

                                hello.getViewTreeObserver().removeOnPreDrawListener(this);
                                return true;
                            }
                        });
            });
            verify(listener, within(1000)).onTransitionStart(any());
            verify(listener, within(2000)).onTransitionEnd(any());

            verifyMovement(redPoints, slideEdge, true);
            verifyMovement(greenPoints, slideEdge, true);
            verifyMovement(helloPoints, slideEdge, true);
            mInstrumentation.waitForIdleSync();

            verifyNoTranslation(redSquare);
            verifyNoTranslation(greenSquare);
            verifyNoTranslation(hello);
            assertEquals(View.VISIBLE, redSquare.getVisibility());
            assertEquals(View.VISIBLE, greenSquare.getVisibility());
            assertEquals(View.VISIBLE, hello.getVisibility());
        }
    }

    private void verifyMovement(List<PointF> points, int slideEdge, boolean in) {
        int numPoints = points.size();
        assertTrue(numPoints > 4);

        // skip the first point -- it is the value before the change
        PointF firstPoint = points.get(1);

        PointF midPoint = points.get(numPoints / 2);

        // Skip the last point -- it may be the settled value after the change
        PointF lastPoint = points.get(numPoints - 2);

        // Check nothing hapens in off-axis motion
        switch (slideEdge) {
            case Gravity.LEFT:
            case Gravity.START:
            case Gravity.RIGHT:
            case Gravity.END:
                assertEquals(0f, lastPoint.y, 0.01f);
                assertEquals(0f, midPoint.y, 0.01f);
                assertEquals(0f, firstPoint.y, 0.01f);
                break;
            case Gravity.TOP:
            case Gravity.BOTTOM:
                assertEquals(0f, lastPoint.x, 0.01f);
                assertEquals(0f, midPoint.x, 0.01f);
                assertEquals(0f, firstPoint.x, 0.01f);
                break;
        }

        float startCoord;
        float endCoord;
        float midCoord;
        boolean moveGreater;
        if (slideEdge == Gravity.TOP || slideEdge == Gravity.BOTTOM) {
            midCoord = midPoint.y;
            startCoord = firstPoint.y;
            endCoord = lastPoint.y;
            moveGreater = in == (slideEdge == Gravity.TOP);
        } else {
            midCoord = midPoint.x;
            startCoord = firstPoint.x;
            endCoord = lastPoint.x;
            moveGreater = in == (slideEdge == Gravity.START || slideEdge == Gravity.LEFT);
        }
        assertEquals(moveGreater, endCoord > midCoord);
        assertEquals(moveGreater, midCoord > startCoord);
        assertEquals(in, Math.abs(endCoord) < Math.abs(startCoord));
    }

    private void verifyNoTranslation(View view) {
        assertEquals(0f, view.getTranslationX(), 0.01f);
        assertEquals(0f, view.getTranslationY(), 0.01f);
    }
}

