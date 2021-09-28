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

import static org.junit.Assert.assertFalse;

import android.content.res.Resources;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.graphics.BitmapShader;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.ComposeShader;
import android.graphics.Paint;
import android.graphics.Path;
import android.graphics.Picture;
import android.graphics.Point;
import android.graphics.PorterDuff;
import android.graphics.RadialGradient;
import android.graphics.Rect;
import android.graphics.RectF;
import android.graphics.Shader;
import android.uirendering.cts.R;
import android.uirendering.cts.bitmapverifiers.BitmapVerifier;
import android.uirendering.cts.bitmapverifiers.SamplePointVerifier;
import android.uirendering.cts.testinfrastructure.ActivityTestBase;

import androidx.test.InstrumentationRegistry;
import androidx.test.filters.MediumTest;
import androidx.test.runner.AndroidJUnit4;

import org.junit.Test;
import org.junit.runner.RunWith;

@MediumTest
@RunWith(AndroidJUnit4.class)
public class CanvasTests extends ActivityTestBase {

    private static final int PAINT_COLOR = 0xff00ff00;
    private static final int BITMAP_WIDTH = 10;
    private static final int BITMAP_HEIGHT = 28;

    private Paint getPaint() {
        Paint paint = new Paint();
        paint.setColor(PAINT_COLOR);
        return paint;
    }

    public Bitmap getImmutableBitmap() {
        final Resources res = InstrumentationRegistry.getTargetContext().getResources();
        BitmapFactory.Options opt = new BitmapFactory.Options();
        opt.inScaled = false; // bitmap will only be immutable if not scaled during load
        Bitmap immutableBitmap = BitmapFactory.decodeResource(res, R.drawable.sunset1, opt);
        assertFalse(immutableBitmap.isMutable());
        return immutableBitmap;
    }

    public Bitmap getMutableBitmap() {
        return Bitmap.createBitmap(BITMAP_WIDTH, BITMAP_HEIGHT, Bitmap.Config.ARGB_8888);
    }

    @Test
    public void testDrawDoubleRoundRect() {
        Point[] testPoints = {
                new Point(0, 0),
                new Point(89, 0),
                new Point(89, 89),
                new Point(0, 89),
                new Point(10, 10),
                new Point(79, 10),
                new Point(79, 79),
                new Point(10, 79)
        };
        int[] colors = {
                Color.WHITE,
                Color.WHITE,
                Color.WHITE,
                Color.WHITE,
                Color.RED,
                Color.RED,
                Color.RED,
                Color.RED
        };
        createTest()
                .addCanvasClient((canvas, width, height) -> {
                    Paint paint = new Paint();
                    paint.setColor(Color.WHITE);
                    canvas.drawRect(0, 0, width, height, paint);

                    paint.setColor(Color.RED);
                    int inset = 20;
                    RectF outer = new RectF(0, 0, width, height);
                    RectF inner = new RectF(inset,
                            inset,
                            width - inset,
                            height - inset);
                    canvas.drawDoubleRoundRect(outer, 5, 5,
                            inner, 10, 10, paint);
                })
                .runWithVerifier(new SamplePointVerifier(testPoints, colors));
    }

    @Test
    public void testDrawDoubleRoundRectWithRadii() {
        Point[] testPoints = {
                new Point(0, 0),
                new Point(89, 0),
                new Point(89, 89),
                new Point(0, 89),
                new Point(9, 7),
                new Point(81, 7),
                new Point(75, 75),
                new Point(21, 67)
        };
        int[] colors = {
                Color.RED,
                Color.WHITE,
                Color.WHITE,
                Color.WHITE,
                Color.RED,
                Color.RED,
                Color.RED,
                Color.RED
        };
        createTest()
                .addCanvasClient((canvas, width, height) -> {
                    Paint paint = new Paint();
                    paint.setColor(Color.WHITE);
                    canvas.drawRect(0, 0, width, height, paint);

                    paint.setColor(Color.RED);
                    int inset = 30;
                    RectF outer = new RectF(0, 0, width, height);
                    RectF inner = new RectF(inset,
                            inset,
                            width - inset,
                            height - inset);

                    float[] outerRadii = {
                            0.0f, 0.0f, // top left
                            5.0f, 5.0f, // top right
                            10.0f, 10.0f, // bottom right
                            20.0f, 20.0f // bottom left
                    };

                    float[] innerRadii = {
                            20.0f, 20.0f,
                            15.0f, 15.0f,
                            8.0f, 8.0f,
                            3.0f, 3.0f
                    };
                    canvas.drawDoubleRoundRect(outer, outerRadii, inner, innerRadii, paint);
                })
                .runWithVerifier(new SamplePointVerifier(testPoints, colors));
    }

    @Test(expected = IllegalArgumentException.class)
    public void testDrawHwBitmap_inSwCanvas() {
        Bitmap hwBitmap = getImmutableBitmap().copy(Bitmap.Config.HARDWARE, false);
        // we verify this specific call should IAE
        new Canvas(getMutableBitmap()).drawBitmap(hwBitmap, 0, 0, null);
    }

    @Test(expected = IllegalArgumentException.class)
    public void testDrawHwBitmap_inPictureCanvas_inSwCanvas() {
        Bitmap hwBitmap = getImmutableBitmap().copy(Bitmap.Config.HARDWARE, false);
        Picture picture = new Picture();
        Canvas pictureCanvas = picture.beginRecording(100, 100);
        pictureCanvas.drawBitmap(hwBitmap, 0, 0, null);
        // we verify this specific call should IAE
        new Canvas(getMutableBitmap()).drawPicture(picture);
    }

    @Test(expected = IllegalArgumentException.class)
    public void testDrawHwBitmap_inPictureCanvas_inPictureCanvas_inSwCanvas() {
        Bitmap hwBitmap = getImmutableBitmap().copy(Bitmap.Config.HARDWARE, false);
        Picture innerPicture = new Picture();
        Canvas pictureCanvas = innerPicture.beginRecording(100, 100);
        pictureCanvas.drawBitmap(hwBitmap, 0, 0, null);

        Picture outerPicture = new Picture();
        Canvas outerPictureCanvas = outerPicture.beginRecording(100, 100);
        outerPictureCanvas.drawPicture(innerPicture);
        // we verify this specific call should IAE
        new Canvas(getMutableBitmap()).drawPicture(outerPicture);
    }

    @Test(expected = IllegalArgumentException.class)
    public void testHwBitmapShaderInSwCanvas1() {
        Bitmap hwBitmap = getImmutableBitmap().copy(Bitmap.Config.HARDWARE, false);
        BitmapShader bitmapShader = new BitmapShader(hwBitmap, Shader.TileMode.REPEAT,
                Shader.TileMode.REPEAT);
        RadialGradient gradientShader = new RadialGradient(10, 10, 30,
                Color.BLACK, Color.CYAN, Shader.TileMode.REPEAT);
        Shader shader = new ComposeShader(gradientShader, bitmapShader, PorterDuff.Mode.OVERLAY);
        Paint p = new Paint();
        p.setShader(shader);
        new Canvas(getMutableBitmap()).drawRect(0, 0, 10, 10, p);
    }

    @Test(expected = IllegalArgumentException.class)
    public void testHwBitmapShaderInSwCanvas2() {
        Bitmap hwBitmap = getImmutableBitmap().copy(Bitmap.Config.HARDWARE, false);
        BitmapShader bitmapShader = new BitmapShader(hwBitmap, Shader.TileMode.REPEAT,
                Shader.TileMode.REPEAT);
        RadialGradient gradientShader = new RadialGradient(10, 10, 30,
                Color.BLACK, Color.CYAN, Shader.TileMode.REPEAT);
        Shader shader = new ComposeShader(bitmapShader, gradientShader, PorterDuff.Mode.OVERLAY);
        Paint p = new Paint();
        p.setShader(shader);
        new Canvas(getMutableBitmap()).drawRect(0, 0, 10, 10, p);
    }

    @Test(expected = IllegalStateException.class)
    public void testCanvasFromImmutableBitmap() {
        // Should throw out IllegalStateException when creating Canvas with an ImmutableBitmap
        new Canvas(getImmutableBitmap());
    }

    @Test(expected = RuntimeException.class)
    public void testCanvasFromRecycledBitmap() {
        // Should throw out RuntimeException when creating Canvas with a MutableBitmap which
        // is recycled
        Bitmap bitmap = getMutableBitmap();
        bitmap.recycle();
        new Canvas(bitmap);
    }

    @Test(expected = IllegalStateException.class)
    public void testSetBitmapToImmutableBitmap() {
        // Should throw out IllegalStateException when setting an ImmutableBitmap to a Canvas
        new Canvas(getMutableBitmap()).setBitmap(getImmutableBitmap());
    }

    @Test(expected = RuntimeException.class)
    public void testSetBitmapToRecycledBitmap() {
        // Should throw out RuntimeException when setting Bitmap which is recycled to a Canvas
        Bitmap bitmap = getMutableBitmap();
        bitmap.recycle();
        new Canvas(getMutableBitmap()).setBitmap(bitmap);
    }

    @Test(expected = IllegalStateException.class)
    public void testRestoreWithoutSave() {
        // Should throw out IllegalStateException because cannot restore Canvas before save
        new Canvas(getMutableBitmap()).restore();
    }

    @Test(expected = IllegalArgumentException.class)
    public void testRestoreToCountIllegalSaveCount() {
        // Should throw out IllegalArgumentException because saveCount is less than 1
        new Canvas(getMutableBitmap()).restoreToCount(0);
    }

    @Test(expected = ArrayIndexOutOfBoundsException.class)
    public void testDrawPointsInvalidOffset() {
        // Should throw out ArrayIndexOutOfBoundsException because of invalid offset
        new Canvas(getMutableBitmap()).drawPoints(new float[]{
                10.0f, 29.0f
        }, -1, 2, getPaint());
    }

    @Test(expected = ArrayIndexOutOfBoundsException.class)
    public void testDrawPointsInvalidCount() {
        // Should throw out ArrayIndexOutOfBoundsException because of invalid count
        new Canvas(getMutableBitmap()).drawPoints(new float[]{
                10.0f, 29.0f
        }, 0, 31, getPaint());
    }

    @Test(expected = ArrayIndexOutOfBoundsException.class)
    public void testDrawLinesInvalidOffset() {
        // Should throw out ArrayIndexOutOfBoundsException because of invalid offset
        new Canvas(getMutableBitmap()).drawLines(new float[]{
                0, 0, 10, 31
        }, 2, 4, new Paint());
    }

    @Test(expected = ArrayIndexOutOfBoundsException.class)
    public void testDrawLinesInvalidCount() {
        // Should throw out ArrayIndexOutOfBoundsException because of invalid count
        new Canvas(getMutableBitmap()).drawLines(new float[]{
                0, 0, 10, 31
        }, 0, 8, new Paint());
    }

    @Test(expected = NullPointerException.class)
    public void testDrawOvalNull() {
        // Should throw out NullPointerException because oval is null
        new Canvas(getMutableBitmap()).drawOval(null, getPaint());
    }

    @Test(expected = NullPointerException.class)
    public void testDrawArcNullOval() {
        // Should throw NullPointerException because oval is null
        new Canvas(getMutableBitmap()).drawArc(null, 10.0f, 29.0f,
                true, getPaint());
    }

    @Test(expected = NullPointerException.class)
    public void testDrawRoundRectNull() {
        // Should throw out NullPointerException because RoundRect is null
        new Canvas(getMutableBitmap()).drawRoundRect(null, 10.0f, 29.0f, getPaint());
    }

    @Test(expected = RuntimeException.class)
    public void testDrawBitmapAtPointRecycled() {
        Bitmap b = Bitmap.createBitmap(BITMAP_WIDTH, 29, Bitmap.Config.ARGB_8888);
        b.recycle();

        // Should throw out RuntimeException because bitmap has been recycled
        new Canvas(getMutableBitmap()).drawBitmap(b, 10.0f, 29.0f, getPaint());
    }

    @Test(expected = RuntimeException.class)
    public void testDrawBitmapSrcDstFloatRecycled() {
        Bitmap b = Bitmap.createBitmap(BITMAP_WIDTH, 29, Bitmap.Config.ARGB_8888);
        b.recycle();

        // Should throw out RuntimeException because bitmap has been recycled
        new Canvas(getMutableBitmap()).drawBitmap(b, null, new RectF(), getPaint());
    }

    @Test(expected = RuntimeException.class)
    public void testDrawBitmapSrcDstIntRecycled() {
        Bitmap b = Bitmap.createBitmap(BITMAP_WIDTH, 29, Bitmap.Config.ARGB_8888);
        b.recycle();

        // Should throw out RuntimeException because bitmap has been recycled
        new Canvas(getMutableBitmap()).drawBitmap(b, null, new Rect(), getPaint());
    }

    @Test(expected = IllegalArgumentException.class)
    public void testDrawBitmapIntsNegativeWidth() {
        // Should throw out IllegalArgumentException because width is less than 0
        new Canvas(getMutableBitmap()).drawBitmap(new int[2008], 10, 10, 10,
                10, -1, 10, true, null);
    }

    @Test(expected = IllegalArgumentException.class)
    public void testDrawBitmapIntsNegativeHeight() {
        // Should throw out IllegalArgumentException because height is less than 0
        new Canvas(getMutableBitmap()).drawBitmap(new int[2008], 10, 10, 10,
                10, 10, -1, true, null);
    }

    @Test(expected = IllegalArgumentException.class)
    public void testDrawBitmapIntsBadStride() {
        // Should throw out IllegalArgumentException because stride less than width and
        // bigger than -width
        new Canvas(getMutableBitmap()).drawBitmap(new int[2008], 10, 5, 10,
                10, 10, 10, true, null);
    }

    @Test(expected = ArrayIndexOutOfBoundsException.class)
    public void testDrawBitmapIntsNegativeOffset() {
        // Should throw out ArrayIndexOutOfBoundsException because offset less than 0
        new Canvas(getMutableBitmap()).drawBitmap(new int[2008], -1, 10, 10,
                10, 10, 10, true, null);
    }

    @Test(expected = ArrayIndexOutOfBoundsException.class)
    public void testDrawBitmapIntsBadOffset() {
        // Should throw out ArrayIndexOutOfBoundsException because sum of offset and width
        // is bigger than colors' length
        new Canvas(getMutableBitmap()).drawBitmap(new int[29], 10, 29, 10,
                10, 20, 10, true, null);
    }

    @Test(expected = IllegalArgumentException.class)
    public void testDrawBitmapFloatsNegativeWidth() {
        // Should throw out IllegalArgumentException because width is less than 0
        new Canvas(getMutableBitmap()).drawBitmap(new int[2008], 10, 10, 10.0f,
                10.0f, -1, 10, true, null);
    }

    @Test(expected = IllegalArgumentException.class)
    public void testDrawBitmapFloatsNegativeHeight() {
        // Should throw out IllegalArgumentException because height is less than 0
        new Canvas(getMutableBitmap()).drawBitmap(new int[2008], 10, 10, 10.0f,
                10.0f, 10, -1, true, null);
    }

    @Test(expected = IllegalArgumentException.class)
    public void testDrawBitmapFloatsBadStride() {
        // Should throw out IllegalArgumentException because stride less than width and
        // bigger than -width
        new Canvas(getMutableBitmap()).drawBitmap(new int[2008], 10, 5, 10.0f,
                10.0f, 10, 10, true, null);
    }

    @Test(expected = ArrayIndexOutOfBoundsException.class)
    public void testDrawBitmapFloatsNegativeOffset() {
        // Should throw out ArrayIndexOutOfBoundsException because offset less than 0
        new Canvas(getMutableBitmap()).drawBitmap(new int[2008], -1, 10, 10.0f,
                10.0f, 10, 10, true, null);
    }

    @Test(expected = ArrayIndexOutOfBoundsException.class)
    public void testDrawBitmapFloatsBadOffset() {
        // Should throw out ArrayIndexOutOfBoundsException because sum of offset and width
        // is bigger than colors' length
        new Canvas(getMutableBitmap()).drawBitmap(new int[29], 10, 29, 10.0f,
                10.0f, 20, 10, true, null);
    }

    @Test(expected = ArrayIndexOutOfBoundsException.class)
    public void testDrawBitmapMeshNegativeWidth() {
        final Bitmap b = Bitmap.createBitmap(BITMAP_WIDTH, 29, Bitmap.Config.ARGB_8888);

        // Should throw out ArrayIndexOutOfBoundsException because meshWidth less than 0
        new Canvas(getMutableBitmap()).drawBitmapMesh(b, -1, 10,
                null, 0, null, 0, null);
    }

    @Test(expected = ArrayIndexOutOfBoundsException.class)
    public void testDrawBitmapMeshNegativeHeight() {
        final Bitmap b = Bitmap.createBitmap(BITMAP_WIDTH, 29, Bitmap.Config.ARGB_8888);

        // Should throw out ArrayIndexOutOfBoundsException because meshHeight is less than 0
        new Canvas(getMutableBitmap()).drawBitmapMesh(b, 10, -1,
                null, 0, null, 0, null);
    }

    @Test(expected = ArrayIndexOutOfBoundsException.class)
    public void testDrawBitmapMeshNegativeVertOffset() {
        final Bitmap b = Bitmap.createBitmap(BITMAP_WIDTH, 29, Bitmap.Config.ARGB_8888);

        // Should throw out ArrayIndexOutOfBoundsException because vertOffset is less than 0
        new Canvas(getMutableBitmap()).drawBitmapMesh(b, 10, 10,
                null, -1, null, 0, null);
    }

    @Test(expected = ArrayIndexOutOfBoundsException.class)
    public void testDrawBitmapMeshNegativeColorOffset() {
        final Bitmap b = Bitmap.createBitmap(BITMAP_WIDTH, 29, Bitmap.Config.ARGB_8888);

        // Should throw out ArrayIndexOutOfBoundsException because colorOffset is less than 0
        new Canvas(getMutableBitmap()).drawBitmapMesh(b, 10, 10,
                null, 10, null, -1, null);
    }

    @Test(expected = ArrayIndexOutOfBoundsException.class)
    public void testDrawBitmapMeshTooFewVerts() {
        final Bitmap b = Bitmap.createBitmap(BITMAP_WIDTH, 29, Bitmap.Config.ARGB_8888);

        // Should throw out ArrayIndexOutOfBoundsException because verts' length is too short
        new Canvas(getMutableBitmap()).drawBitmapMesh(b, 10, 10,
                new float[] { 10.0f, 29.0f}, 10, null, 10, null);
    }

    @Test(expected = ArrayIndexOutOfBoundsException.class)
    public void testDrawBitmapMeshTooFewColors() {
        final Bitmap b = Bitmap.createBitmap(BITMAP_WIDTH, 29, Bitmap.Config.ARGB_8888);

        // Should throw out ArrayIndexOutOfBoundsException because colors' length is too short
        // abnormal case: colors' length is too short
        final float[] verts = new float[2008];
        new Canvas(getMutableBitmap()).drawBitmapMesh(b, 10, 10, verts,
                10, new int[] { 10, 29}, 10, null);
    }

    @Test(expected = ArrayIndexOutOfBoundsException.class)
    public void testDrawVerticesTooFewVerts() {
        final float[] verts = new float[10];
        final float[] texs = new float[10];
        final int[] colors = new int[10];
        final short[] indices = {
                0, 1, 2, 3, 4, 1
        };

        // Should throw out ArrayIndexOutOfBoundsException because sum of vertOffset and
        // vertexCount is bigger than verts' length
        new Canvas(getMutableBitmap()).drawVertices(Canvas.VertexMode.TRIANGLES, 10,
                verts, 8, texs, 0, colors, 0, indices,
                0, 4, getPaint());
    }

    @Test(expected = ArrayIndexOutOfBoundsException.class)
    public void testDrawVerticesTooFewTexs() {
        final float[] verts = new float[10];
        final float[] texs = new float[10];
        final int[] colors = new int[10];
        final short[] indices = {
                0, 1, 2, 3, 4, 1
        };

        // Should throw out ArrayIndexOutOfBoundsException because sum of texOffset and
        // vertexCount is bigger thatn texs' length
        new Canvas(getMutableBitmap()).drawVertices(Canvas.VertexMode.TRIANGLES, 10,
                verts, 0, texs, 30, colors, 0, indices,
                0, 4, getPaint());
    }

    @Test(expected = ArrayIndexOutOfBoundsException.class)
    public void testDrawVerticesTooFewColors() {
        final float[] verts = new float[10];
        final float[] texs = new float[10];
        final int[] colors = new int[10];
        final short[] indices = {
                0, 1, 2, 3, 4, 1
        };

        // Should throw out ArrayIndexOutOfBoundsException because sum of colorOffset and
        // vertexCount is bigger than colors' length
        new Canvas(getMutableBitmap()).drawVertices(Canvas.VertexMode.TRIANGLES, 10,
                verts, 0, texs, 0, colors, 30, indices,
                0, 4, getPaint());
    }

    @Test(expected = ArrayIndexOutOfBoundsException.class)
    public void testDrawVerticesTooFewIndices() {
        final float[] verts = new float[10];
        final float[] texs = new float[10];
        final int[] colors = new int[10];
        final short[] indices = {
                0, 1, 2, 3, 4, 1
        };

        // Should throw out ArrayIndexOutOfBoundsException because sum of indexOffset and
        // indexCount is bigger than indices' length
        new Canvas(getMutableBitmap()).drawVertices(Canvas.VertexMode.TRIANGLES, 10,
                verts, 0, texs, 0, colors, 0, indices,
                10, 30, getPaint());
    }

    @Test(expected = IndexOutOfBoundsException.class)
    public void testDrawArrayTextNegativeIndex() {
        final char[] text = { 'a', 'n', 'd', 'r', 'o', 'i', 'd' };

        // Should throw out IndexOutOfBoundsException because index is less than 0
        new Canvas(getMutableBitmap()).drawText(text, -1, 7, 10, 10, getPaint());
    }

    @Test(expected = IndexOutOfBoundsException.class)
    public void testDrawArrayTextNegativeCount() {
        final char[] text = { 'a', 'n', 'd', 'r', 'o', 'i', 'd' };

        // Should throw out IndexOutOfBoundsException because count is less than 0
        new Canvas(getMutableBitmap()).drawText(text, 0, -1, 10, 10, getPaint());
    }

    @Test(expected = IndexOutOfBoundsException.class)
    public void testDrawArrayTextTextLengthTooSmall() {
        final char[] text = { 'a', 'n', 'd', 'r', 'o', 'i', 'd' };

        // Should throw out IndexOutOfBoundsException because sum of index and count
        // is bigger than text's length
        new Canvas(getMutableBitmap()).drawText(text, 0, 10, 10, 10, getPaint());
    }

    @Test(expected = IndexOutOfBoundsException.class)
    public void testDrawTextTextAtPositionWithOffsetsNegativeStart() {
        // Should throw out IndexOutOfBoundsException because start is less than 0
        new Canvas(getMutableBitmap()).drawText("android", -1, 7, 10, 30,
                getPaint());
    }

    @Test(expected = IndexOutOfBoundsException.class)
    public void testDrawTextTextAtPositionWithOffsetsNegativeEnd() {
        // Should throw out IndexOutOfBoundsException because end is less than 0
        new Canvas(getMutableBitmap()).drawText("android", 0, -1, 10, 30,
                getPaint());
    }

    @Test(expected = IndexOutOfBoundsException.class)
    public void testDrawTextTextAtPositionWithOffsetsStartEndMismatch() {
        // Should throw out IndexOutOfBoundsException because start is bigger than end
        new Canvas(getMutableBitmap()).drawText("android", 3, 1, 10, 30,
                getPaint());
    }

    @Test(expected = IndexOutOfBoundsException.class)
    public void testDrawTextTextAtPositionWithOffsetsTextTooLong() {
        // Should throw out IndexOutOfBoundsException because end subtracts start should
        // bigger than text's length
        new Canvas(getMutableBitmap()).drawText("android", 0, 10, 10, 30,
                getPaint());
    }

    @Test(expected = NullPointerException.class)
    public void testDrawTextRunNullCharArray() {
        // Should throw out NullPointerException because text is null
        new Canvas(getMutableBitmap()).drawTextRun((char[]) null, 0, 0,
                0, 0, 0.0f, 0.0f, false, new Paint());
    }

    @Test(expected = NullPointerException.class)
    public void testDrawTextRunNullCharSequence() {
        // Should throw out NullPointerException because text is null
        new Canvas(getMutableBitmap()).drawTextRun((CharSequence) null, 0, 0,
                0, 0, 0.0f, 0.0f, false, new Paint());
    }

    @Test(expected = NullPointerException.class)
    public void testDrawTextRunCharArrayNullPaint() {
        // Should throw out NullPointerException because paint is null
        new Canvas(getMutableBitmap()).drawTextRun("android".toCharArray(), 0, 0,
                0, 0, 0.0f, 0.0f, false, null);
    }

    @Test(expected = NullPointerException.class)
    public void testDrawTextRunCharSequenceNullPaint() {
        // Should throw out NullPointerException because paint is null
        new Canvas(getMutableBitmap()).drawTextRun("android", 0, 0, 0,
                0, 0.0f, 0.0f, false, null);
    }

    @Test(expected = IndexOutOfBoundsException.class)
    public void testDrawTextRunNegativeIndex() {
        final String text = "android";
        final Paint paint = new Paint();

        // Should throw out IndexOutOfBoundsException because index is less than 0
        new Canvas(getMutableBitmap()).drawTextRun(text.toCharArray(), -1, text.length(),
                0, text.length(), 0.0f, 0.0f,
                false, new Paint());
    }

    @Test(expected = IndexOutOfBoundsException.class)
    public void testDrawTextRunNegativeCount() {
        final String text = "android";

        // Should throw out IndexOutOfBoundsException because count is less than 0
        new Canvas(getMutableBitmap()).drawTextRun(text.toCharArray(), 0, -1,
                0, text.length(), 0.0f, 0.0f, false,
                new Paint());
    }

    @Test(expected = IndexOutOfBoundsException.class)
    public void testDrawTextRunContestIndexTooLarge() {
        final String text = "android";

        // Should throw out IndexOutOfBoundsException because contextIndex is bigger than index
        new Canvas(getMutableBitmap()).drawTextRun(text.toCharArray(), 0, text.length(),
                1, text.length(), 0.0f, 0.0f,
                false, new Paint());
    }

    @Test(expected = IndexOutOfBoundsException.class)
    public void testDrawTextRunContestIndexTooSmall() {
        final String text = "android";

        // Should throw out IndexOutOfBoundsException because contextIndex + contextCount
        // is less than index + count
        new Canvas(getMutableBitmap()).drawTextRun(text, 0, text.length(), 0,
                text.length() - 1, 0.0f, 0.0f, false,
                new Paint());
    }

    @Test(expected = IndexOutOfBoundsException.class)
    public void testDrawTextRunIndexTooLarge() {
        final String text = "android";
        final Paint paint = new Paint();

        // Should throw out IndexOutOfBoundsException because index + count is bigger than
        // text length
        new Canvas(getMutableBitmap()).drawTextRun(text.toCharArray(), 0,
                text.length() + 1, 0, text.length() + 1,
                0.0f, 0.0f, false, new Paint());
    }

    @Test(expected = IndexOutOfBoundsException.class)
    public void testDrawTextRunNegativeContextStart() {
        final String text = "android";
        final Paint paint = new Paint();

        // Should throw out IndexOutOfBoundsException because contextStart is less than 0
        new Canvas(getMutableBitmap()).drawTextRun(text, 0, text.length(), -1,
                text.length(), 0.0f, 0.0f, false,
                new Paint());
    }

    @Test(expected = IndexOutOfBoundsException.class)
    public void testDrawTextRunStartLessThanContextStart() {
        final String text = "android";

        // Should throw out IndexOutOfBoundsException because start is less than contextStart
        new Canvas(getMutableBitmap()).drawTextRun(text, 0, text.length(), 1,
                text.length(), 0.0f, 0.0f, false,
                new Paint());
    }

    @Test(expected = IndexOutOfBoundsException.class)
    public void testDrawTextRunEndLessThanStart() {
        final String text = "android";

        // Should throw out IndexOutOfBoundsException because end is less than start
        new Canvas(getMutableBitmap()).drawTextRun(text, 1, 0, 0,
                text.length(), 0.0f, 0.0f, false, new Paint());
    }

    @Test(expected = IndexOutOfBoundsException.class)
    public void testDrawTextRunContextEndLessThanEnd() {
        final String text = "android";

        // Should throw out IndexOutOfBoundsException because contextEnd is less than end
        new Canvas(getMutableBitmap()).drawTextRun(text, 0, text.length(), 0,
                text.length() - 1, 0.0f, 0.0f, false,
                new Paint());
    }

    @Test(expected = IndexOutOfBoundsException.class)
    public void testDrawTextRunContextEndLargerThanTextLength() {
        final String text = "android";

        // Should throw out IndexOutOfBoundsException because contextEnd is bigger than
        // text length
        new Canvas(getMutableBitmap()).drawTextRun(text, 0, text.length(), 0,
                text.length() + 1, 0.0f, 0.0f, false,
                new Paint());
    }

    @Test(expected = IndexOutOfBoundsException.class)
    public void testDrawPosTextWithIndexAndCountNegativeIndex() {
        final char[] text = {
                'a', 'n', 'd', 'r', 'o', 'i', 'd'
        };
        final float[] pos = new float[]{
                0.0f, 0.0f, 1.0f, 1.0f, 2.0f, 2.0f, 3.0f, 3.0f, 4.0f, 4.0f, 5.0f, 5.0f, 6.0f, 6.0f,
                7.0f, 7.0f
        };

        // Should throw out IndexOutOfBoundsException because index is less than 0
        new Canvas(getMutableBitmap()).drawPosText(text, -1, 7, pos, getPaint());
    }

    @Test(expected = IndexOutOfBoundsException.class)
    public void testDrawPosTextWithIndexAndCountTextTooShort() {
        final char[] text = {
                'a', 'n', 'd', 'r', 'o', 'i', 'd'
        };
        final float[] pos = new float[]{
                0.0f, 0.0f, 1.0f, 1.0f, 2.0f, 2.0f, 3.0f, 3.0f, 4.0f, 4.0f, 5.0f, 5.0f, 6.0f, 6.0f,
                7.0f, 7.0f
        };

        // Should throw out IndexOutOfBoundsException because sum of index and count is
        // bigger than text's length
        new Canvas(getMutableBitmap()).drawPosText(text, 1, 10, pos, getPaint());
    }

    @Test(expected = IndexOutOfBoundsException.class)
    public void testDrawPosTextWithIndexAndCountCountTooLarge() {
        final char[] text = {
                'a', 'n', 'd', 'r', 'o', 'i', 'd'
        };

        // Should throw out IndexOutOfBoundsException because 2 times of count is
        // bigger than pos' length
        new Canvas(getMutableBitmap()).drawPosText(text, 1, 10, new float[] {
                10.0f, 30.f
        }, getPaint());
    }

    @Test(expected = ArrayIndexOutOfBoundsException.class)
    public void testDrawTextOnPathWithIndexAndCountNegativeIndex() {
        final char[] text = { 'a', 'n', 'd', 'r', 'o', 'i', 'd' };

        // Should throw out ArrayIndexOutOfBoundsException because index is smaller than 0
        new Canvas(getMutableBitmap()).drawTextOnPath(text, -1, 7, new Path(),
                10.0f, 10.0f, getPaint());
    }

    @Test(expected = ArrayIndexOutOfBoundsException.class)
    public void testDrawTextOnPathWithIndexAndCountTextTooShort() {
        final char[] text = { 'a', 'n', 'd', 'r', 'o', 'i', 'd' };

        // Should throw out ArrayIndexOutOfBoundsException because sum of index and
        // count is bigger than text's length
        new Canvas(getMutableBitmap()).drawTextOnPath(text, 0, 10, new Path(),
                10.0f, 10.0f, getPaint());
    }

    @Test(expected = IndexOutOfBoundsException.class)
    public void testDrawPosTextCountTooLarge() {
        final String text = "android";

        // Should throw out IndexOutOfBoundsException because 2 times of count is
        // bigger than pos' length
        new Canvas(getMutableBitmap()).drawPosText(text, new float[]{
                10.0f, 30.f
        }, getPaint());
    }

    @Test
    public void testAntiAliasClipping() {
        createTest()
                .addCanvasClient((canvas, width, height) -> {
                    Paint paint = new Paint();
                    Path clipPath = new Path();

                    clipPath.addCircle(
                            width / 2.0f, height / 2.0f,
                            Math.min(width, height) / 2.0f,
                            Path.Direction.CW
                    );

                    paint.setColor(Color.WHITE);
                    canvas.drawRect(0, 0, width, height, paint);

                    paint.setColor(Color.RED);

                    canvas.save();

                    canvas.clipPath(clipPath);
                    canvas.drawRect(0, 0, width, height, paint);

                    canvas.restore();

                })
                .runWithVerifier(new AntiAliasPixelCounter(Color.WHITE, Color.RED, 10));
    }

    private static class AntiAliasPixelCounter extends BitmapVerifier {

        private final int mColor1;
        private final int mColor2;
        private final int mCountThreshold;

        AntiAliasPixelCounter(int color1, int color2, int countThreshold) {
            mColor1 = color1;
            mColor2 = color2;
            mCountThreshold = countThreshold;
        }

        @Override
        public boolean verify(int[] bitmap, int offset, int stride, int width, int height) {
            int nonTargetColorCount = 0;
            for (int x = 0; x < width; x++) {
                for (int y = 0; y < height; y++) {
                    int pixelColor = bitmap[indexFromXAndY(x, y, stride, offset)];
                    if (pixelColor != mColor1 && pixelColor != mColor2) {
                        nonTargetColorCount++;
                    }
                }
            }
            return nonTargetColorCount > mCountThreshold;
        }
    }
}
