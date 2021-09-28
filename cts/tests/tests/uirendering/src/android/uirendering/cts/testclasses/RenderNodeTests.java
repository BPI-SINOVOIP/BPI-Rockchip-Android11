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
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertTrue;

import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.ColorMatrix;
import android.graphics.ColorMatrixColorFilter;
import android.graphics.Paint;
import android.graphics.RecordingCanvas;
import android.graphics.Rect;
import android.graphics.RenderNode;
import android.uirendering.cts.bitmapverifiers.RectVerifier;
import android.uirendering.cts.testinfrastructure.ActivityTestBase;

import androidx.test.filters.MediumTest;
import androidx.test.runner.AndroidJUnit4;

import org.junit.Test;
import org.junit.runner.RunWith;

import java.util.HashSet;
import java.util.Set;

@MediumTest
@RunWith(AndroidJUnit4.class)
public class RenderNodeTests extends ActivityTestBase {

    @Test
    public void testDefaults() {
        final RenderNode renderNode = new RenderNode(null);
        assertEquals(0, renderNode.getLeft());
        assertEquals(0, renderNode.getRight());
        assertEquals(0, renderNode.getTop());
        assertEquals(0, renderNode.getBottom());
        assertEquals(0, renderNode.getWidth());
        assertEquals(0, renderNode.getHeight());

        assertEquals(0, renderNode.getTranslationX(), 0.01f);
        assertEquals(0, renderNode.getTranslationY(), 0.01f);
        assertEquals(0, renderNode.getTranslationZ(), 0.01f);
        assertEquals(0, renderNode.getElevation(), 0.01f);

        assertEquals(0, renderNode.getRotationX(), 0.01f);
        assertEquals(0, renderNode.getRotationY(), 0.01f);
        assertEquals(0, renderNode.getRotationZ(), 0.01f);

        assertEquals(1, renderNode.getScaleX(), 0.01f);
        assertEquals(1, renderNode.getScaleY(), 0.01f);

        assertEquals(1, renderNode.getAlpha(), 0.01f);

        assertEquals(0, renderNode.getPivotX(), 0.01f);
        assertEquals(0, renderNode.getPivotY(), 0.01f);

        assertEquals(Color.BLACK, renderNode.getAmbientShadowColor());
        assertEquals(Color.BLACK, renderNode.getSpotShadowColor());

        assertEquals(8, renderNode.getCameraDistance(), 0.01f);

        assertTrue(renderNode.isForceDarkAllowed());
        assertTrue(renderNode.hasIdentityMatrix());
        assertTrue(renderNode.getClipToBounds());
        assertFalse(renderNode.getClipToOutline());
        assertFalse(renderNode.isPivotExplicitlySet());
        assertFalse(renderNode.hasDisplayList());
        assertFalse(renderNode.hasOverlappingRendering());
        assertFalse(renderNode.hasShadow());
        assertFalse(renderNode.getUseCompositingLayer());
    }

    @Test
    public void testBasicDraw() {
        final Rect rect = new Rect(10, 10, 80, 80);

        final RenderNode renderNode = new RenderNode("Blue rect");
        renderNode.setPosition(rect.left, rect.top, rect.right, rect.bottom);
        assertEquals(rect.left, renderNode.getLeft());
        assertEquals(rect.top, renderNode.getTop());
        assertEquals(rect.right, renderNode.getRight());
        assertEquals(rect.bottom, renderNode.getBottom());
        renderNode.setClipToBounds(true);

        {
            Canvas canvas = renderNode.beginRecording();
            assertEquals(rect.width(), canvas.getWidth());
            assertEquals(rect.height(), canvas.getHeight());
            assertTrue(canvas.isHardwareAccelerated());
            canvas.drawColor(Color.BLUE);
            renderNode.endRecording();
        }

        assertTrue(renderNode.hasDisplayList());
        assertTrue(renderNode.hasIdentityMatrix());

        createTest()
                .addCanvasClientWithoutUsingPicture((canvas, width, height) -> {
                    canvas.drawRenderNode(renderNode);
                }, true)
                .runWithVerifier(new RectVerifier(Color.WHITE, Color.BLUE, rect));
    }

    @Test
    public void testAlphaOverlappingRendering() {
        final Rect rect = new Rect(10, 10, 80, 80);
        final RenderNode renderNode = new RenderNode(null);
        renderNode.setPosition(rect.left, rect.top, rect.right, rect.bottom);
        renderNode.setHasOverlappingRendering(true);
        assertTrue(renderNode.hasOverlappingRendering());
        {
            Canvas canvas = renderNode.beginRecording();
            canvas.drawColor(Color.RED);
            canvas.drawColor(Color.BLUE);
            renderNode.endRecording();
        }
        renderNode.setAlpha(.5f);
        createTest()
              .addCanvasClientWithoutUsingPicture((canvas, width, height) -> {
                  canvas.drawRenderNode(renderNode);
              }, true)
              .runWithVerifier(new RectVerifier(Color.WHITE, 0xFF8080FF, rect));
    }

    @Test
    public void testAlphaNonOverlappingRendering() {
        final Rect rect = new Rect(10, 10, 80, 80);
        final RenderNode renderNode = new RenderNode(null);
        renderNode.setPosition(rect.left, rect.top, rect.right, rect.bottom);
        renderNode.setHasOverlappingRendering(false);
        assertFalse(renderNode.hasOverlappingRendering());
        {
            Canvas canvas = renderNode.beginRecording();
            canvas.drawColor(Color.RED);
            canvas.drawColor(Color.BLUE);
            renderNode.endRecording();
        }
        renderNode.setAlpha(.5f);
        createTest()
                .addCanvasClientWithoutUsingPicture((canvas, width, height) -> {
                    canvas.drawRenderNode(renderNode);
                }, true)
                .runWithVerifier(new RectVerifier(Color.WHITE, 0xFF8040BF, rect));
    }

    @Test
    public void testUseCompositingLayer() {
        final Rect rect = new Rect(10, 10, 80, 80);
        final RenderNode renderNode = new RenderNode(null);
        renderNode.setPosition(rect.left, rect.top, rect.right, rect.bottom);
        {
            Canvas canvas = renderNode.beginRecording();
            canvas.drawColor(0xFF0000AF);
            renderNode.endRecording();
        }
        // Construct & apply a Y'UV lightness invert color matrix to the layer paint
        Paint paint = new Paint();
        ColorMatrix matrix = new ColorMatrix();
        ColorMatrix tmp = new ColorMatrix();
        matrix.setRGB2YUV();
        tmp.set(new float[] {
                -1f, 0f, 0f, 0f, 255f,
                0f, 1f, 0f, 0f, 0f,
                0f, 0f, 1f, 0f, 0f,
                0f, 0f, 0f, 1f, 0f,
        });
        matrix.postConcat(tmp);
        tmp.setYUV2RGB();
        matrix.postConcat(tmp);
        paint.setColorFilter(new ColorMatrixColorFilter(matrix));
        renderNode.setUseCompositingLayer(true, paint);
        createTest()
                .addCanvasClientWithoutUsingPicture((canvas, width, height) -> {
                    canvas.drawRenderNode(renderNode);
                }, true)
                .runWithVerifier(new RectVerifier(Color.WHITE, 0xFFD7D7FF, rect));
    }

    @Test
    public void testComputeApproximateMemoryUsage() {
        final RenderNode renderNode = new RenderNode("sizeTest");
        assertTrue(renderNode.computeApproximateMemoryUsage() > 500);
        assertTrue(renderNode.computeApproximateMemoryUsage() < 1500);
        long sizeBefore = renderNode.computeApproximateMemoryUsage();
        {
            Canvas canvas = renderNode.beginRecording();
            assertTrue(canvas.isHardwareAccelerated());
            canvas.drawColor(Color.BLUE);
            renderNode.endRecording();
        }
        long sizeAfter = renderNode.computeApproximateMemoryUsage();
        assertTrue(sizeAfter > sizeBefore);
        renderNode.discardDisplayList();
        assertEquals(sizeBefore, renderNode.computeApproximateMemoryUsage());
    }

    @Test
    public void testTranslationGetSet() {
        final RenderNode renderNode = new RenderNode("translation");

        assertTrue(renderNode.hasIdentityMatrix());

        assertFalse(renderNode.setTranslationX(0.0f));
        assertFalse(renderNode.setTranslationY(0.0f));
        assertFalse(renderNode.setTranslationZ(0.0f));

        assertTrue(renderNode.hasIdentityMatrix());

        assertTrue(renderNode.setTranslationX(1.0f));
        assertEquals(1.0f, renderNode.getTranslationX(), 0.0f);
        assertTrue(renderNode.setTranslationY(1.0f));
        assertEquals(1.0f, renderNode.getTranslationY(), 0.0f);
        assertTrue(renderNode.setTranslationZ(1.0f));
        assertEquals(1.0f, renderNode.getTranslationZ(), 0.0f);

        assertFalse(renderNode.hasIdentityMatrix());

        assertTrue(renderNode.setTranslationX(0.0f));
        assertTrue(renderNode.setTranslationY(0.0f));
        assertTrue(renderNode.setTranslationZ(0.0f));

        assertTrue(renderNode.hasIdentityMatrix());
    }

    @Test
    public void testAlphaGetSet() {
        final RenderNode renderNode = new RenderNode("alpha");

        assertFalse(renderNode.setAlpha(1.0f));
        assertTrue(renderNode.setAlpha(.5f));
        assertEquals(.5f, renderNode.getAlpha(), 0.0001f);
        assertTrue(renderNode.setAlpha(1.0f));
    }

    @Test
    public void testRotationGetSet() {
        final RenderNode renderNode = new RenderNode("rotation");

        assertFalse(renderNode.setRotationX(0.0f));
        assertFalse(renderNode.setRotationY(0.0f));
        assertFalse(renderNode.setRotationZ(0.0f));
        assertTrue(renderNode.hasIdentityMatrix());

        assertTrue(renderNode.setRotationX(1.0f));
        assertEquals(1.0f, renderNode.getRotationX(), 0.0f);
        assertTrue(renderNode.setRotationY(1.0f));
        assertEquals(1.0f, renderNode.getRotationY(), 0.0f);
        assertTrue(renderNode.setRotationZ(1.0f));
        assertEquals(1.0f, renderNode.getRotationZ(), 0.0f);
        assertFalse(renderNode.hasIdentityMatrix());

        assertTrue(renderNode.setRotationX(0.0f));
        assertTrue(renderNode.setRotationY(0.0f));
        assertTrue(renderNode.setRotationZ(0.0f));
        assertTrue(renderNode.hasIdentityMatrix());
    }

    @Test
    public void testScaleGetSet() {
        final RenderNode renderNode = new RenderNode("scale");

        assertFalse(renderNode.setScaleX(1.0f));
        assertFalse(renderNode.setScaleY(1.0f));

        assertTrue(renderNode.setScaleX(2.0f));
        assertEquals(2.0f, renderNode.getScaleX(), 0.0f);
        assertTrue(renderNode.setScaleY(2.0f));
        assertEquals(2.0f, renderNode.getScaleY(), 0.0f);

        assertTrue(renderNode.setScaleX(1.0f));
        assertTrue(renderNode.setScaleY(1.0f));
    }

    @Test
    public void testStartEndRecordingEmpty() {
        final RenderNode renderNode = new RenderNode(null);
        assertEquals(0, renderNode.getWidth());
        assertEquals(0, renderNode.getHeight());
        RecordingCanvas canvas = renderNode.beginRecording();
        assertTrue(canvas.isHardwareAccelerated());
        assertEquals(0, canvas.getWidth());
        assertEquals(0, canvas.getHeight());
        renderNode.endRecording();
    }

    @Test
    public void testStartEndRecordingWithBounds() {
        final RenderNode renderNode = new RenderNode(null);
        renderNode.setPosition(10, 20, 30, 50);
        assertEquals(20, renderNode.getWidth());
        assertEquals(30, renderNode.getHeight());
        RecordingCanvas canvas = renderNode.beginRecording();
        assertTrue(canvas.isHardwareAccelerated());
        assertEquals(20, canvas.getWidth());
        assertEquals(30, canvas.getHeight());
        renderNode.endRecording();
    }

    @Test
    public void testStartEndRecordingEmptyWithSize() {
        final RenderNode renderNode = new RenderNode(null);
        assertEquals(0, renderNode.getWidth());
        assertEquals(0, renderNode.getHeight());
        RecordingCanvas canvas = renderNode.beginRecording(5, 10);
        assertTrue(canvas.isHardwareAccelerated());
        assertEquals(5, canvas.getWidth());
        assertEquals(10, canvas.getHeight());
        renderNode.endRecording();
    }

    @Test
    public void testStartEndRecordingWithBoundsWithSize() {
        final RenderNode renderNode = new RenderNode(null);
        renderNode.setPosition(10, 20, 30, 50);
        assertEquals(20, renderNode.getWidth());
        assertEquals(30, renderNode.getHeight());
        RecordingCanvas canvas = renderNode.beginRecording(5, 10);
        assertTrue(canvas.isHardwareAccelerated());
        assertEquals(5, canvas.getWidth());
        assertEquals(10, canvas.getHeight());
        renderNode.endRecording();
    }

    @Test
    public void testGetUniqueId() {
        final RenderNode r1 = new RenderNode(null);
        final RenderNode r2 = new RenderNode(null);
        assertNotEquals(r1.getUniqueId(), r2.getUniqueId());
        final Set<Long> usedIds = new HashSet<>();
        assertTrue(usedIds.add(r1.getUniqueId()));
        assertTrue(usedIds.add(r2.getUniqueId()));
        for (int i = 0; i < 100; i++) {
            assertTrue(usedIds.add(new RenderNode(null).getUniqueId()));
        }
    }

    @Test(expected = IllegalArgumentException.class)
    public void testInvalidCameraDistance() {
        final RenderNode renderNode = new RenderNode(null);
        renderNode.setCameraDistance(-1f);
    }

    @Test
    public void testCameraDistanceSetGet() {
        final RenderNode renderNode = new RenderNode(null);
        renderNode.setCameraDistance(100f);
        assertEquals(100f, renderNode.getCameraDistance(), 0.0f);
    }
}
