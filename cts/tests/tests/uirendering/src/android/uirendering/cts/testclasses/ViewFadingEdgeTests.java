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

import android.content.Context;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Point;
import android.uirendering.cts.R;
import android.uirendering.cts.bitmapverifiers.SamplePointVerifier;
import android.uirendering.cts.testinfrastructure.ActivityTestBase;
import android.uirendering.cts.testinfrastructure.ViewInitializer;
import android.view.View;
import android.widget.FrameLayout;

import androidx.test.filters.MediumTest;
import androidx.test.runner.AndroidJUnit4;

import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;

@MediumTest
@RunWith(AndroidJUnit4.class)
public class ViewFadingEdgeTests extends ActivityTestBase {
    class FadingView extends View {
        public boolean mEnableFadingLeftEdge = false;
        public boolean mEnableFadingRightEdge = false;
        public boolean mEnableFadingTopEdge = false;
        public boolean mEnableFadingBottomEdge = false;

        FadingView(Context context) {
            super(context);
        }

        @Override
        protected void onDraw(Canvas canvas) {
            canvas.drawColor(Color.RED);
        }

        @Override
        protected float getLeftFadingEdgeStrength() {
            return mEnableFadingLeftEdge ? 1.0f : 0.0f;
        }

        @Override
        protected float getRightFadingEdgeStrength() {
            return mEnableFadingRightEdge ? 1.0f : 0.0f;
        }

        @Override
        protected float getTopFadingEdgeStrength() {
            return mEnableFadingTopEdge ? 1.0f : 0.0f;
        }

        @Override
        protected float getBottomFadingEdgeStrength() {
            return mEnableFadingBottomEdge ? 1.0f : 0.0f;
        }
    }

    private FadingView attachFadingView(View parentView) {
        final FadingView child = new FadingView(parentView.getContext());
        child.setLayoutParams(new FrameLayout.LayoutParams(
                FrameLayout.LayoutParams.MATCH_PARENT,
                FrameLayout.LayoutParams.MATCH_PARENT));

        FrameLayout root = (FrameLayout) parentView.findViewById(R.id.frame_layout);
        root.addView(child);
        return child;
    }

    @Test
    public void testCreateLeftFade() {
        createTest()
                .addLayout(R.layout.frame_layout, (ViewInitializer) view -> {
                    final FadingView child = attachFadingView(view);
                    child.mEnableFadingLeftEdge = true;
                    child.setFadingEdgeLength(TEST_WIDTH / 2);
                    child.setHorizontalFadingEdgeEnabled(true);
                    Assert.assertEquals(0, child.getSolidColor());
                })
                .runWithVerifier(new SamplePointVerifier(new Point[] {
                        new Point(0, 0),
                        new Point(TEST_WIDTH / 4, 0),
                        new Point(TEST_WIDTH / 2, 0),
                        new Point(TEST_WIDTH - 1, 0)
                }, new int[] {
                        Color.WHITE,
                        0xFFFF8080, // gradient halfway between red and white
                        Color.RED,
                        Color.RED,

                }, 20)); // Tolerance set to account for interpolation
    }

    @Test
    public void testCreateRightFade() {
        createTest()
                .addLayout(R.layout.frame_layout, (ViewInitializer) view -> {
                    final FadingView child = attachFadingView(view);
                    child.mEnableFadingRightEdge = true;
                    child.setFadingEdgeLength(TEST_WIDTH / 2);
                    child.setHorizontalFadingEdgeEnabled(true);
                    Assert.assertEquals(0, child.getSolidColor());
                })
                .runWithVerifier(new SamplePointVerifier(new Point[] {
                        new Point(0, 0),
                        new Point(TEST_WIDTH / 2, 0),
                        new Point((TEST_WIDTH / 4) * 3, 0),
                        new Point(TEST_WIDTH - 1, 0)
                }, new int[] {
                        Color.RED,
                        Color.RED,
                        0xFFFF8080, // gradient halfway between red and white
                        Color.WHITE
                }, 20)); // Tolerance set to account for interpolation
    }

    @Test
    public void testCreateTopFade() {
        createTest()
                .addLayout(R.layout.frame_layout, (ViewInitializer) view -> {
                    final FadingView child = attachFadingView(view);
                    child.mEnableFadingTopEdge = true;
                    child.setFadingEdgeLength(TEST_HEIGHT / 2);
                    child.setVerticalFadingEdgeEnabled(true);
                    Assert.assertEquals(0, child.getSolidColor());
                })
                .runWithVerifier(new SamplePointVerifier(new Point[] {
                        new Point(0, 0),
                        new Point(0, TEST_HEIGHT / 4),
                        new Point(0, TEST_HEIGHT / 2),
                        new Point(0, TEST_HEIGHT - 1)
                }, new int[] {
                        Color.WHITE,
                        0xFFFF8080, // gradient halfway between red and white
                        Color.RED,
                        Color.RED,
                }, 20)); // Tolerance set to account for interpolation
    }

    @Test
    public void testCreateBottomFade() {
        createTest()
                .addLayout(R.layout.frame_layout, (ViewInitializer) view -> {
                    final FadingView child = attachFadingView(view);
                    child.mEnableFadingBottomEdge = true;
                    child.setFadingEdgeLength(TEST_HEIGHT / 2);
                    child.setVerticalFadingEdgeEnabled(true);
                    Assert.assertEquals(0, child.getSolidColor());
                })
                .runWithVerifier(new SamplePointVerifier(new Point[] {
                        new Point(0, 0),
                        new Point(0, TEST_HEIGHT / 2),
                        new Point(0, (TEST_HEIGHT / 4) * 3),
                        new Point(0, TEST_HEIGHT - 1)
                }, new int[] {
                        Color.RED,
                        Color.RED,
                        0xFFFF8080, // gradient halfway between red and white
                        Color.WHITE
                }, 20)); // Tolerance set to account for interpolation
    }

    @Test
    public void testCreateLeftAndRightFade() {
        createTest()
                .addLayout(R.layout.frame_layout, (ViewInitializer) view -> {
                    final FadingView child = attachFadingView(view);
                    child.mEnableFadingLeftEdge = true;
                    child.mEnableFadingRightEdge = true;
                    child.setFadingEdgeLength(TEST_WIDTH / 2);
                    child.setHorizontalFadingEdgeEnabled(true);
                    Assert.assertEquals(0, child.getSolidColor());
                })
                .runWithVerifier(new SamplePointVerifier(new Point[] {
                        new Point(0, 0),
                        new Point(TEST_WIDTH / 4, 0),
                        new Point(TEST_WIDTH / 2, 0),
                        new Point((TEST_WIDTH / 4) * 3, 0),
                        new Point(TEST_WIDTH - 1, 0)
                }, new int[] {
                        Color.WHITE,
                        0xFFFF8080, // gradient halfway between red and white
                        Color.RED,
                        0xFFFF8080, // gradient halfway between red and white
                        Color.WHITE
                }, 20)); // Tolerance set to account for interpolation
    }
}
