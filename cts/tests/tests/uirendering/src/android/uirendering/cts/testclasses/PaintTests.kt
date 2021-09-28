/*
 * Copyright 2020 The Android Open Source Project
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

package android.uirendering.cts.testclasses

import android.graphics.Bitmap
import android.graphics.BlurMaskFilter
import android.graphics.Canvas
import android.graphics.Color
import android.graphics.LinearGradient
import android.graphics.Paint
import android.graphics.Path
import android.graphics.Rect
import android.graphics.RectF
import android.graphics.Shader
import android.uirendering.cts.bitmapcomparers.ExactComparer
import android.uirendering.cts.bitmapverifiers.GoldenImageVerifier
import android.uirendering.cts.testinfrastructure.ActivityTestBase
import androidx.test.runner.AndroidJUnit4
import org.junit.Test
import org.junit.runner.RunWith
import kotlin.math.sin
import kotlin.test.assertTrue

@RunWith(AndroidJUnit4::class)
class PaintTests : ActivityTestBase() {
    /**
     * Create a Bitmap to exercise or imitate setShadowLayer.
     *
     * @param drawCall Function to draw something to a Canvas. Must use the Paint.
     * @param reference If true, this is the expectation to compare against. It will imitate
     *                  setShadowLayer by first calling |drawCall| with a blur and an offset,
     *                  then calling |drawCall| again with a normal Paint.
     *                  If false, this is the test case. It will call setShadowLayer and call
     *                  |drawCall| once.
     * @return Bitmap Bitmap containing the reference or test image.
     */
    private fun createShadowBitmap(drawCall: (Paint, Canvas) -> Unit, reference: Boolean): Bitmap {
        val bitmap = Bitmap.createBitmap(1000, 1000, Bitmap.Config.ARGB_8888)
        val canvas = Canvas(bitmap)

        val radius = 3f
        val dx = 5f
        val dy = 5f
        val shadowColor = Color.RED
        val paint = Paint()
        if (reference) {
            val saveCount = canvas.save()
            canvas.translate(dx, dy)
            val blurPaint = Paint(paint)
            blurPaint.maskFilter = BlurMaskFilter(radius, BlurMaskFilter.Blur.NORMAL)
            blurPaint.color = shadowColor
            drawCall(blurPaint, canvas)
            canvas.restoreToCount(saveCount)
        } else {
            paint.setShadowLayer(radius, dx, dy, shadowColor)
        }
        drawCall(paint, canvas)
        return bitmap
    }

    private fun testDrawShadow(drawCall: (Paint, Canvas) -> Unit) {
        val reference = createShadowBitmap(drawCall, true)
        val test = createShadowBitmap(drawCall, false)
        val verifier = GoldenImageVerifier(reference, ExactComparer())
        assertTrue(verifier.verify(test))
    }

    @Test
    fun testDrawTextShadow() = testDrawShadow() {
        paint, canvas ->
        paint.textSize = 100f
        canvas.drawText("shadow text", 0f, paint.textSize, paint)
    }

    @Test
    fun testDrawTextOnPathShadow() = testDrawShadow() {
        paint, canvas ->
        paint.textSize = 100f
        val path = Path()
        path.addCircle(500f, 500f, 300f, Path.Direction.CW)
        canvas.drawTextOnPath("shadow text on path", path, 0f, 0f, paint)
    }

    @Test
    fun testDrawArcShadow() = testDrawShadow() {
        paint, canvas ->
        canvas.drawArc(RectF(100f, 100f, 900f, 900f), 0f, 120f, true, paint)
    }

    @Test
    fun testDrawCircleShadow() = testDrawShadow() {
        paint, canvas ->
        canvas.drawCircle(500f, 500f, 400f, paint)
    }

    @Test
    fun testDoubleRoundRectShadow() = testDrawShadow() {
        paint, canvas ->
        val radii = FloatArray(8) { 5f }
        canvas.drawDoubleRoundRect(RectF(100f, 100f, 900f, 900f), radii,
                RectF(200f, 200f, 800f, 800f), radii, paint)
    }

    @Test
    fun testDrawLineShadow() = testDrawShadow() {
        paint, canvas ->
        canvas.drawLine(1000f, 0f, 0f, 1000f, paint)
    }

    @Test
    fun testDrawLinesShadow() = testDrawShadow() {
        paint, canvas ->
        val points = floatArrayOf(
                100f, 0f, 100f, 1000f,
                500f, 250f, 500f, 750f,
                900f, 450f, 900f, 550f
        )
        canvas.drawLines(points, paint)
    }

    @Test
    fun testDrawOvalShadow() = testDrawShadow() {
        paint, canvas ->
        canvas.drawOval(RectF(100f, 100f, 900f, 900f), paint)
    }

    @Test
    fun testDrawPathShadow() = testDrawShadow() {
        paint, canvas ->
        val path = Path()
        path.moveTo(100f, 100f)
        path.lineTo(900f, 100f)
        path.lineTo(500f, 900f)
        path.close()
        canvas.drawPath(path, paint)
    }

    @Test
    fun testDrawPointsShadow() = testDrawShadow() {
        paint, canvas ->
        paint.strokeWidth = 5f
        paint.strokeCap = Paint.Cap.ROUND
        val points = FloatArray(2000) {
            if (it % 2 == 0) (it / 2).toFloat()
            else 500f * sin((it / 2).toDouble()).toFloat() + 500f
        }
        canvas.drawPoints(points, paint)
    }

    @Test
    fun testDrawRectShadow() = testDrawShadow() {
        paint, canvas ->
        paint.style = Paint.Style.STROKE
        canvas.drawRect(Rect(300, 100, 700, 900), paint)
    }

    @Test
    fun testDrawRoundRectShadow() = testDrawShadow() {
        paint, canvas ->
        canvas.drawRoundRect(100f, 300f, 900f, 700f, 20f, 20f, paint)
    }

    @Test
    fun testDrawVerticesShadow() = testDrawShadow() {
        paint, canvas ->
        val vertices = floatArrayOf(
                100f, 100f,
                900f, 100f,
                500f, 900f
        )
        val colors = intArrayOf(Color.BLACK, Color.BLACK, Color.BLACK)
        canvas.drawVertices(Canvas.VertexMode.TRIANGLE_FAN, vertices.size, vertices, 0,
                null, 0, colors, 0, null, 0, 0, paint)
    }

    @Test
    fun testDrawBitmapShadow() = testDrawShadow() {
        paint, canvas ->
        val bitmap = Bitmap.createBitmap(400, 400, Bitmap.Config.ARGB_8888)
        val bitmapPaint = Paint()
        val colors = intArrayOf(Color.BLUE, Color.GREEN, Color.YELLOW)
        bitmapPaint.shader = LinearGradient(0f, 0f, 400f, 400f, colors, null,
                Shader.TileMode.CLAMP)
        val bitmapCanvas = Canvas(bitmap)
        bitmapCanvas.drawPaint(bitmapPaint)
        bitmapPaint.shader = null
        bitmapPaint.style = Paint.Style.STROKE
        bitmapPaint.strokeWidth = 2f
        bitmapCanvas.drawRect(0f, 0f, 400f, 400f, bitmapPaint)

        canvas.drawBitmap(bitmap, 100f, 100f, paint)
    }
}
