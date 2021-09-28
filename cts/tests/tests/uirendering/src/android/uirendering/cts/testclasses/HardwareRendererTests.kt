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

package android.uirendering.cts.testclasses

import android.graphics.Bitmap
import android.graphics.Color
import android.graphics.HardwareRenderer
import android.graphics.Outline
import android.graphics.Paint
import android.graphics.PixelFormat
import android.graphics.RecordingCanvas
import android.graphics.Rect
import android.graphics.RenderNode
import android.hardware.HardwareBuffer
import android.media.Image
import android.media.ImageReader
import android.os.Debug
import android.uirendering.cts.bitmapverifiers.BitmapVerifier
import android.uirendering.cts.bitmapverifiers.ColorVerifier
import android.uirendering.cts.bitmapverifiers.PerPixelBitmapVerifier
import android.uirendering.cts.bitmapverifiers.RectVerifier
import android.uirendering.cts.bitmapverifiers.RegionVerifier
import android.uirendering.cts.testinfrastructure.ActivityTestBase
import android.uirendering.cts.util.CompareUtils
import android.util.Log
import androidx.test.filters.LargeTest
import androidx.test.filters.MediumTest
import androidx.test.runner.AndroidJUnit4
import org.junit.Ignore
import org.junit.Test
import org.junit.runner.RunWith
import kotlin.test.assertEquals
import kotlin.test.assertFalse
import kotlin.test.assertNotEquals
import kotlin.test.assertNotNull
import kotlin.test.assertTrue

const val TEST_WIDTH = ActivityTestBase.TEST_WIDTH
const val TEST_HEIGHT = ActivityTestBase.TEST_HEIGHT

class CaptureResult(
    val bitmap: IntArray,
    val offset: Int,
    val stride: Int,
    val width: Int,
    val height: Int
)

data class RendererTest(
    var verifier: BitmapVerifier? = null,
    var onPrepare: (HardwareRenderer.() -> Unit)? = null,
    var onDraw: (RecordingCanvas.() -> Unit)? = null
)

private fun verify(verifier: BitmapVerifier, setup: HardwareRenderer.() -> Unit) {
    val reader = ImageReader.newInstance(
        TEST_WIDTH, TEST_HEIGHT, PixelFormat.RGBA_8888, 1,
        HardwareBuffer.USAGE_CPU_READ_OFTEN or HardwareBuffer.USAGE_GPU_COLOR_OUTPUT)
    val renderer = HardwareRenderer()
    var image: Image? = null
    try {
        renderer.setSurface(reader.surface)
        renderer.notifyFramePending()
        setup.invoke(renderer)
        val syncResult = renderer.createRenderRequest()
            .setWaitForPresent(true)
            .syncAndDraw()
        assertEquals(HardwareRenderer.SYNC_OK, syncResult)
        image = reader.acquireNextImage()
        assertNotNull(image)
        val planes = image.planes
        assertNotNull(planes)
        assertEquals(1, planes.size)
        val plane = planes[0]
        assertEquals(4, plane.pixelStride)
        assertTrue((ActivityTestBase.TEST_WIDTH * 4) <= plane.rowStride)
        val buffer = plane.buffer
        val channels = ByteArray(buffer.remaining())
        buffer.get(channels, 0, channels.size)
        val pixels = IntArray(channels.size / 4)
        var pixelIndex = 0
        var channelIndex = 0
        // Need to switch from RGBA (the pixel format on the reader) to ARGB (what bitmaps use)
        while (channelIndex < channels.size) {
            val red = channels[channelIndex++].toInt() and 0xFF
            val green = channels[channelIndex++].toInt() and 0xFF
            val blue = channels[channelIndex++].toInt() and 0xFF
            val alpha = channels[channelIndex++].toInt() and 0xFF
            pixels[pixelIndex++] = Color.argb(alpha, red, green, blue)
        }
        val result = CaptureResult(pixels, 0, plane.rowStride / plane.pixelStride,
            TEST_WIDTH, TEST_WIDTH)
        assertTrue(verifier.verify(
            result.bitmap, result.offset, result.stride, result.width, result.height))
    } finally {
        image?.close()
        renderer.destroy()
        reader.close()
    }
}

private fun rendererTest(setup: RendererTest.() -> Unit) {
    val spec = RendererTest()
    setup.invoke(spec)
    assertNotNull(spec.verifier, "Missing BitmapVerifier")
    assertNotNull(spec.onDraw, "Missing onDraw callback")
    verify(spec.verifier!!) {
        spec.onPrepare?.invoke(this)
        val content = RenderNode("content")
        content.setPosition(0, 0, TEST_WIDTH, TEST_HEIGHT)
        spec.onDraw!!.invoke(content.beginRecording())
        content.endRecording()
        setContentRoot(content)
    }
}

private fun fetchMemoryInfo(): Debug.MemoryInfo {
    Runtime.getRuntime().apply {
        gc()
        runFinalization()
        gc()
        runFinalization()
        gc()
    }
    val meminfo = Debug.MemoryInfo()
    Debug.getMemoryInfo(meminfo)
    return meminfo
}

@MediumTest
@RunWith(AndroidJUnit4::class)
class HardwareRendererTests : ActivityTestBase() {
    @Test
    fun testBasicDrawCpuConsumer() {
        val reader = ImageReader.newInstance(TEST_WIDTH, TEST_HEIGHT, PixelFormat.RGBA_8888, 1,
            HardwareBuffer.USAGE_CPU_READ_OFTEN or HardwareBuffer.USAGE_GPU_COLOR_OUTPUT)
        assertNotNull(reader)
        val renderer = HardwareRenderer()
        var image: Image? = null

        try {
            val content = RenderNode("content")
            content.setPosition(0, 0, TEST_WIDTH, TEST_HEIGHT)
            val canvas = content.beginRecording()
            canvas.drawColor(Color.BLUE)
            content.endRecording()
            renderer.setContentRoot(content)

            renderer.setSurface(reader.surface)

            val syncResult = renderer.createRenderRequest()
                .setWaitForPresent(true)
                .syncAndDraw()

            assertEquals(HardwareRenderer.SYNC_OK, syncResult)

            image = reader.acquireNextImage()
            assertNotNull(image)
            val planes = image.planes
            assertNotNull(planes)
            assertEquals(1, planes.size)
            val plane = planes[0]
            assertEquals(4, plane.pixelStride)
            assertTrue((TEST_WIDTH * 4) <= plane.rowStride)

            val buffer = plane.buffer
            val red = buffer.get()
            val green = buffer.get()
            val blue = buffer.get()
            val alpha = buffer.get()
            assertEquals(0, red, "red")
            assertEquals(0, green, "green")
            assertEquals(0xFF.toByte(), blue, "blue")
            assertEquals(0xFF.toByte(), alpha, "alpha")
        } finally {
            image?.close()
            renderer.destroy()
            reader.close()
        }
    }

    @Test
    fun testBasicDrawGpuConsumer() {
        val reader = ImageReader.newInstance(TEST_WIDTH, TEST_HEIGHT, PixelFormat.RGBA_8888, 1,
            HardwareBuffer.USAGE_GPU_SAMPLED_IMAGE or HardwareBuffer.USAGE_GPU_COLOR_OUTPUT)
        assertNotNull(reader)
        val renderer = HardwareRenderer()
        var image: Image? = null

        try {
            val content = RenderNode("content")
            content.setPosition(0, 0, TEST_WIDTH, TEST_HEIGHT)
            val canvas = content.beginRecording()
            canvas.drawColor(Color.BLUE)
            content.endRecording()
            renderer.setContentRoot(content)

            renderer.setSurface(reader.surface)

            val syncResult = renderer.createRenderRequest()
                .setWaitForPresent(true)
                .syncAndDraw()

            assertEquals(HardwareRenderer.SYNC_OK, syncResult)

            image = reader.acquireNextImage()
            assertNotNull(image)
            val buffer = image.hardwareBuffer
            assertNotNull(buffer)
            Log.d("HardwareRenderer", "buffer usage bits: " +
                    java.lang.Long.toHexString(buffer.usage))
            val bitmap = Bitmap.wrapHardwareBuffer(buffer, null)!!
                .copy(Bitmap.Config.ARGB_8888, false)

            assertEquals(TEST_WIDTH, bitmap.width)
            assertEquals(TEST_HEIGHT, bitmap.height)
            assertEquals(0xFF0000FF.toInt(), bitmap.getPixel(0, 0))
        } finally {
            image?.close()
            renderer.destroy()
            reader.close()
        }
    }

    @Test
    fun testSetOpaque() = rendererTest {
        val rect = Rect(10, 10, 30, 30)
        onPrepare = {
            assertTrue(isOpaque)
            isOpaque = false
            assertFalse(isOpaque)
        }
        onDraw = {
            val paint = Paint()
            paint.color = Color.RED
            drawRect(rect, paint)
        }
        verifier = RectVerifier(Color.TRANSPARENT, Color.RED, rect)
    }

    @Test
    fun testSetStopped() {
        val reader = ImageReader.newInstance(TEST_WIDTH, TEST_HEIGHT, PixelFormat.RGBA_8888, 1,
            HardwareBuffer.USAGE_CPU_READ_OFTEN or HardwareBuffer.USAGE_GPU_COLOR_OUTPUT)
        assertNotNull(reader)
        val renderer = HardwareRenderer()
        try {
            renderer.setSurface(reader.surface)
            assertEquals(HardwareRenderer.SYNC_OK,
                renderer.createRenderRequest().syncAndDraw())
            renderer.stop()
            assertEquals(HardwareRenderer.SYNC_CONTEXT_IS_STOPPED
                    or HardwareRenderer.SYNC_FRAME_DROPPED,
                renderer.createRenderRequest().syncAndDraw())
            reader.acquireLatestImage()?.close()
            renderer.start()
            val result = renderer.createRenderRequest().syncAndDraw()
            assertEquals(0, result and HardwareRenderer.SYNC_CONTEXT_IS_STOPPED)
        } finally {
            renderer.destroy()
            reader.close()
        }
    }

    @Test
    @Ignore // TODO: Re-enable, see b/124520175
    fun testNoSurface() {
        val reader = ImageReader.newInstance(TEST_WIDTH, TEST_HEIGHT, PixelFormat.RGBA_8888, 1,
            HardwareBuffer.USAGE_CPU_READ_OFTEN or HardwareBuffer.USAGE_GPU_COLOR_OUTPUT)
        assertNotNull(reader)
        val renderer = HardwareRenderer()
        try {
            assertEquals(
                HardwareRenderer.SYNC_LOST_SURFACE_REWARD_IF_FOUND
                        or HardwareRenderer.SYNC_FRAME_DROPPED,
                renderer.createRenderRequest().syncAndDraw())
            renderer.setSurface(reader.surface)
            assertEquals(HardwareRenderer.SYNC_OK,
                renderer.createRenderRequest().syncAndDraw())
            reader.close()
            Thread.sleep(32)
            assertEquals(HardwareRenderer.SYNC_LOST_SURFACE_REWARD_IF_FOUND
                    or HardwareRenderer.SYNC_FRAME_DROPPED,
                renderer.createRenderRequest().syncAndDraw())
        } finally {
            renderer.destroy()
            reader.close()
        }
    }

    @LargeTest
    @Test
    fun testClearContent() {
        val reader = ImageReader.newInstance(TEST_WIDTH, TEST_HEIGHT, PixelFormat.RGBA_8888, 1,
            HardwareBuffer.USAGE_CPU_READ_OFTEN or HardwareBuffer.USAGE_GPU_COLOR_OUTPUT)
        assertNotNull(reader)
        val renderer = HardwareRenderer()
        val content = RenderNode("content")
        val canvas = content.beginRecording()

        run {
            for (i in 0..5) {
                val bitmap = Bitmap.createBitmap(1024, 1024, Bitmap.Config.ARGB_8888)
                bitmap.eraseColor(Color.RED)
                canvas.drawBitmap(bitmap, 0f, 0f, Paint())
            }
        }

        content.endRecording()
        try {
            renderer.setSurface(reader.surface)
            renderer.setContentRoot(content)
            assertEquals(HardwareRenderer.SYNC_OK,
                renderer.createRenderRequest()
                    .setWaitForPresent(true)
                    .syncAndDraw())
            val infoBeforeClear = fetchMemoryInfo()
            renderer.clearContent()
            val infoAfterClear = fetchMemoryInfo()
            assertNotEquals(0, infoBeforeClear.totalPss)
            assertNotEquals(0, infoAfterClear.totalPss)

            val pssDifference = infoBeforeClear.totalPss - infoAfterClear.totalPss
            // Use a rather generous margin of error in case the only thing freed is the bitmap
            // while other memroy was allocated in the process of checking that. pss is in kB
            val minimalDifference = 5 * 1024
            assertTrue(pssDifference > minimalDifference,
                "pssDifference: $pssDifference less than expected of at least $minimalDifference")
        } finally {
            renderer.destroy()
            reader.close()
        }
    }

    @Test
    fun testDestroy() {
        val reader = ImageReader.newInstance(TEST_WIDTH, TEST_HEIGHT, PixelFormat.RGBA_8888, 1,
            HardwareBuffer.USAGE_CPU_READ_OFTEN or HardwareBuffer.USAGE_GPU_COLOR_OUTPUT)
        assertNotNull(reader)
        val renderer = HardwareRenderer()
        try {
            renderer.setSurface(reader.surface)
            assertEquals(HardwareRenderer.SYNC_OK,
                renderer.createRenderRequest()
                    .setWaitForPresent(true)
                    .syncAndDraw())
            renderer.destroy()
            assertEquals(
                HardwareRenderer.SYNC_LOST_SURFACE_REWARD_IF_FOUND
                        or HardwareRenderer.SYNC_FRAME_DROPPED,
                renderer.createRenderRequest().syncAndDraw())

            renderer.setSurface(reader.surface)
            assertEquals(HardwareRenderer.SYNC_OK,
                renderer.createRenderRequest()
                    .setWaitForPresent(true)
                    .syncAndDraw())
        } finally {
            renderer.destroy()
            reader.close()
        }
    }

    @Test
    fun testSpotShadowSetup() = rendererTest {
        val childRect = Rect(25, 25, 65, 65)
        onPrepare = {
            setLightSourceAlpha(0.0f, 1.0f)
            setLightSourceGeometry(TEST_WIDTH / 2f, 0f, 800.0f, 20.0f)
        }
        onDraw = {
            val childNode = RenderNode("shadowCaster")
            childNode.setPosition(childRect)
            val outline = Outline()
            outline.setRect(Rect(0, 0, childRect.width(), childRect.height()))
            outline.alpha = 1f
            childNode.setOutline(outline)
            val childCanvas = childNode.beginRecording()
            childCanvas.drawColor(Color.RED)
            childNode.endRecording()
            childNode.elevation = 20f

            drawColor(Color.WHITE)
            enableZ()
            drawRenderNode(childNode)
            disableZ()
        }
        verifier = RegionVerifier()
            .addVerifier(childRect, ColorVerifier(Color.RED, 10))
            .addVerifier(
                Rect(childRect.left, childRect.bottom, childRect.right, childRect.bottom + 10),
                object : PerPixelBitmapVerifier() {
                    override fun verifyPixel(x: Int, y: Int, observedColor: Int): Boolean {
                        return CompareUtils.verifyPixelGrayScale(observedColor, 1)
                    }
                })
    }

    @Test
    fun testLotsOfBuffers() {
        val colorForIndex = { i: Int ->
            Color.argb(255, 10 * i, 6 * i, 2 * i)
        }
        val testColors = IntArray(20, colorForIndex)

        val reader = ImageReader.newInstance(
                TEST_WIDTH, TEST_HEIGHT, PixelFormat.RGBA_8888, testColors.size,
                HardwareBuffer.USAGE_CPU_READ_OFTEN or HardwareBuffer.USAGE_GPU_COLOR_OUTPUT)
        assertNotNull(reader)
        val renderer = HardwareRenderer()
        val images = ArrayList<Image>()

        try {
            val content = RenderNode("content")
            content.setPosition(0, 0, TEST_WIDTH, TEST_HEIGHT)
            renderer.setContentRoot(content)
            renderer.setSurface(reader.surface)

            testColors.forEach {
                val canvas = content.beginRecording()
                canvas.drawColor(it)
                content.endRecording()

                val syncResult = renderer.createRenderRequest()
                        .setWaitForPresent(true)
                        .syncAndDraw()
                assertEquals(HardwareRenderer.SYNC_OK, syncResult)
                // TODO: Add API to avoid this
                Thread.sleep(32)
            }

            for (i in 0 until testColors.size) {
                val image = reader.acquireNextImage()
                assertNotNull(image)
                images.add(image)
            }

            assertEquals(testColors.size, images.size)

            images.forEachIndexed { index, image ->
                val planes = image.planes
                assertNotNull(planes)
                assertEquals(1, planes.size)
                val plane = planes[0]
                assertEquals(4, plane.pixelStride)
                assertTrue((TEST_WIDTH * 4) <= plane.rowStride)

                val buffer = plane.buffer
                val red = buffer.get().toInt() and 0xFF
                val green = buffer.get().toInt() and 0xFF
                val blue = buffer.get().toInt() and 0xFF
                val alpha = buffer.get().toInt() and 0xFF

                val expectedColor = colorForIndex(index)

                assertEquals(Color.red(expectedColor), red, "red")
                assertEquals(Color.green(expectedColor), green, "green")
                assertEquals(Color.blue(expectedColor), blue, "blue")
                assertEquals(255, alpha, "alpha")
            }
        } finally {
            images.forEach {
                try {
                    it.close()
                } catch (ex: Throwable) {}
            }
            images.clear()
            renderer.destroy()
            reader.close()
        }
    }
}
