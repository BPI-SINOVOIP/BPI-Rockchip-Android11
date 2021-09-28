/*
 * Copyright (C) 2020 The Android Open Source Project
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

package com.android.captiveportallogin

import android.app.Activity
import android.app.KeyguardManager
import android.content.Intent
import android.net.Network
import android.net.Uri
import android.os.Bundle
import android.os.Parcel
import android.os.Parcelable
import android.widget.TextView
import androidx.core.content.FileProvider
import androidx.test.core.app.ActivityScenario
import androidx.test.ext.junit.runners.AndroidJUnit4
import androidx.test.filters.SmallTest
import androidx.test.platform.app.InstrumentationRegistry.getInstrumentation
import androidx.test.uiautomator.By
import androidx.test.uiautomator.UiDevice
import androidx.test.uiautomator.Until
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.doReturn
import org.mockito.Mockito.mock
import org.mockito.Mockito.timeout
import org.mockito.Mockito.verify
import java.io.ByteArrayInputStream
import java.io.File
import java.io.FileInputStream
import java.io.IOException
import java.io.InputStream
import java.io.InputStreamReader
import java.net.HttpURLConnection
import java.net.URL
import java.net.URLConnection
import java.nio.charset.StandardCharsets
import java.text.NumberFormat
import java.util.concurrent.SynchronousQueue
import java.util.concurrent.TimeUnit.MILLISECONDS
import kotlin.math.min
import kotlin.test.assertEquals
import kotlin.test.assertFalse
import kotlin.test.assertNotEquals
import kotlin.test.assertNotNull
import kotlin.test.assertTrue
import kotlin.test.fail

private val TEST_FILESIZE = 1_000_000 // 1MB
private val TEST_USERAGENT = "Test UserAgent"
private val TEST_URL = "https://test.download.example.com/myfile"
private val NOTIFICATION_SHADE_TYPE = "com.android.systemui:id/notification_stack_scroller"

private val TEST_TIMEOUT_MS = 10_000L

@RunWith(AndroidJUnit4::class)
@SmallTest
class DownloadServiceTest {
    private val connection = mock(HttpURLConnection::class.java)

    private val context by lazy { getInstrumentation().context }
    private val resources by lazy { context.resources }
    private val device by lazy { UiDevice.getInstance(getInstrumentation()) }

    // Test network that can be parceled in intents while mocking the connection
    class TestNetwork : Network(43) {
        companion object {
            // Subclasses of parcelable classes need to define a CREATOR field of their own (which
            // hides the one of the parent class), otherwise the CREATOR field of the parent class
            // would be used when unparceling and createFromParcel would return an instance of the
            // parent class.
            @JvmField
            val CREATOR = object : Parcelable.Creator<TestNetwork> {
                override fun createFromParcel(source: Parcel?) = TestNetwork()
                override fun newArray(size: Int) = emptyArray<TestNetwork>()
            }

            /**
             * Test [URLConnection] to be returned by all [TestNetwork] instances when
             * [openConnection] is called.
             *
             * This can be set to a mock connection, and is a static to allow [TestNetwork]s to be
             * parceled and unparceled without losing their mock configuration.
             */
            internal var sTestConnection: HttpURLConnection? = null
        }

        override fun openConnection(url: URL?): URLConnection {
            return sTestConnection ?: throw IOException("Mock URLConnection not initialized")
        }
    }

    /**
     * A test InputStream returning generated data.
     *
     * Reading this stream is not thread-safe: it should only be read by one thread at a time.
     */
    private class TestInputStream(private var available: Int = 0) : InputStream() {
        // position / available are only accessed in the reader thread
        private var position = 0

        private val nextAvailableQueue = SynchronousQueue<Int>()

        /**
         * Set how many bytes are available now without blocking.
         *
         * This is to be set on a thread controlling the amount of data that is available, while
         * a reader thread may be trying to read the data.
         *
         * The reader thread will block until this value is increased, and if the reader is not yet
         * waiting for the data to be made available, this method will block until it is.
         */
        fun setAvailable(newAvailable: Int) {
            assertTrue(nextAvailableQueue.offer(newAvailable.coerceIn(0, TEST_FILESIZE),
                    TEST_TIMEOUT_MS, MILLISECONDS),
                    "Timed out waiting for TestInputStream to be read")
        }

        override fun read(): Int {
            throw NotImplementedError("read() should be unused")
        }

        /**
         * Attempt to read [len] bytes at offset [off].
         *
         * This will block until some data is available if no data currently is (so this method
         * never returns 0 if [len] > 0).
         */
        override fun read(b: ByteArray, off: Int, len: Int): Int {
            if (position >= TEST_FILESIZE) return -1 // End of stream

            while (available <= position) {
                available = nextAvailableQueue.take()
            }

            // Read the requested bytes (but not more than available).
            val remaining = available - position
            val readLen = min(len, remaining)
            for (i in 0 until readLen) {
                b[off + i] = (position % 256).toByte()
                position++
            }

            return readLen
        }
    }

    @Before
    fun setUp() {
        TestNetwork.sTestConnection = connection

        doReturn(200).`when`(connection).responseCode
        doReturn(TEST_FILESIZE.toLong()).`when`(connection).contentLengthLong

        ActivityScenario.launch(RequestDismissKeyguardActivity::class.java)
    }

    /**
     * Create a temporary, empty file that can be used to read/write data for testing.
     */
    private fun createTestFile(extension: String = ".png"): File {
        // temp/ is as exported in file_paths.xml, so that the file can be shared externally
        // (in the download success notification)
        val testFilePath = File(context.getCacheDir(), "temp")
        testFilePath.mkdir()
        return File.createTempFile("test", extension, testFilePath)
    }

    private fun makeDownloadIntent(testFile: File) = DownloadService.makeDownloadIntent(
            context,
            TestNetwork(),
            TEST_USERAGENT,
            TEST_URL,
            testFile.name,
            makeFileUri(testFile))

    /**
     * Make a file URI based on a file on disk, using a [FileProvider] that is registered for the
     * test app.
     */
    private fun makeFileUri(testFile: File) = FileProvider.getUriForFile(
            context,
            // File provider registered in the test manifest
            "com.android.captiveportallogin.tests.fileprovider",
            testFile)

    @Test
    fun testDownloadFile() {
        val inputStream1 = TestInputStream()
        doReturn(inputStream1).`when`(connection).inputStream

        val testFile1 = createTestFile()
        val testFile2 = createTestFile()
        assertNotEquals(testFile1.name, testFile2.name)
        val downloadIntent1 = makeDownloadIntent(testFile1)
        val downloadIntent2 = makeDownloadIntent(testFile2)
        openNotificationShade()

        // Queue both downloads immediately: they should be started in order
        context.startForegroundService(downloadIntent1)
        context.startForegroundService(downloadIntent2)

        verify(connection, timeout(TEST_TIMEOUT_MS)).inputStream
        val dlText1 = resources.getString(R.string.downloading_paramfile, testFile1.name)

        assertTrue(device.wait(Until.hasObject(
                By.res(NOTIFICATION_SHADE_TYPE).hasDescendant(By.text(dlText1))), TEST_TIMEOUT_MS))

        // Allow download to progress to 1%
        assertEquals(0, TEST_FILESIZE % 100)
        assertTrue(TEST_FILESIZE / 100 > 0)
        inputStream1.setAvailable(TEST_FILESIZE / 100)

        // 1% progress should be shown in the notification
        assertTrue(device.wait(Until.hasObject(By.res(NOTIFICATION_SHADE_TYPE).hasDescendant(
                By.text(NumberFormat.getPercentInstance().format(.01f)))), TEST_TIMEOUT_MS))

        // Setup the connection for the next download with indeterminate progress
        val inputStream2 = TestInputStream()
        doReturn(inputStream2).`when`(connection).inputStream
        doReturn(-1L).`when`(connection).contentLengthLong

        // Allow the first download to finish
        inputStream1.setAvailable(TEST_FILESIZE)
        verify(connection, timeout(TEST_TIMEOUT_MS)).disconnect()

        FileInputStream(testFile1).use {
            assertSameContents(it, TestInputStream(TEST_FILESIZE))
        }

        testFile1.delete()

        // The second download should have started: make some data available
        inputStream2.setAvailable(TEST_FILESIZE / 100)

        // A notification should be shown for the second download with indeterminate progress
        val dlText2 = resources.getString(R.string.downloading_paramfile, testFile2.name)
        assertTrue(device.wait(Until.hasObject(
                By.res(NOTIFICATION_SHADE_TYPE).hasDescendant(By.text(dlText2))), TEST_TIMEOUT_MS))

        // Allow the second download to finish
        inputStream2.setAvailable(TEST_FILESIZE)
        verify(connection, timeout(TEST_TIMEOUT_MS).times(2)).disconnect()

        FileInputStream(testFile2).use {
            assertSameContents(it, TestInputStream(TEST_FILESIZE))
        }

        testFile2.delete()
    }

    @Test
    fun testTapDoneNotification() {
        val fileContents = "Test file contents"
        val bis = ByteArrayInputStream(fileContents.toByteArray(StandardCharsets.UTF_8))
        doReturn(bis).`when`(connection).inputStream

        // .testtxtfile extension is handled by OpenTextFileActivity in the test package
        val testFile = createTestFile(extension = ".testtxtfile")
        val downloadIntent = makeDownloadIntent(testFile)
        openNotificationShade()

        context.startForegroundService(downloadIntent)

        val doneText = resources.getString(R.string.download_completed)
        val note = device.wait(Until.findObject(By.text(doneText)), TEST_TIMEOUT_MS)
        assertNotNull(note, "Notification with text \"$doneText\" not found")

        note.click()

        // OpenTextFileActivity opens the file and shows contents
        assertTrue(device.wait(Until.hasObject(By.text(fileContents)), TEST_TIMEOUT_MS))
    }

    private fun openNotificationShade() {
        device.wakeUp()
        device.openNotification()
        assertTrue(device.wait(Until.hasObject(By.res(NOTIFICATION_SHADE_TYPE)), TEST_TIMEOUT_MS))
    }

    /**
     * Verify that two [InputStream] have the same content by reading them until the end of stream.
     */
    private fun assertSameContents(s1: InputStream, s2: InputStream) {
        val buffer1 = ByteArray(1000)
        val buffer2 = ByteArray(1000)
        while (true) {
            // Read one chunk from s1
            val read1 = s1.read(buffer1, 0, buffer1.size)
            if (read1 < 0) break

            // Read a chunk of the same size from s2
            var read2 = 0
            while (read2 < read1) {
                s2.read(buffer2, read2, read1 - read2).also {
                    assertFalse(it < 0, "Stream 2 is shorter than stream 1")
                    read2 += it
                }
            }
            assertEquals(buffer1.take(read1), buffer2.take(read1))
        }
        assertEquals(-1, s2.read(buffer2, 0, 1), "Stream 2 is longer than stream 1")
    }

    /**
     * [KeyguardManager.requestDismissKeyguard] requires an activity: this activity allows the test
     * to dismiss the keyguard by just being started.
     */
    class RequestDismissKeyguardActivity : Activity() {
        override fun onCreate(savedInstanceState: Bundle?) {
            super.onCreate(savedInstanceState)
            getSystemService(KeyguardManager::class.java).requestDismissKeyguard(this, null)
        }
    }

    /**
     * Activity that reads a file specified as [Uri] in its start [Intent], and displays the file
     * contents on screen by reading the file as UTF-8 text.
     *
     * The activity is registered in the manifest as a receiver for VIEW intents with a
     * ".testtxtfile" URI.
     */
    class OpenTextFileActivity : Activity() {
        override fun onCreate(savedInstanceState: Bundle?) {
            super.onCreate(savedInstanceState)

            val testFile = intent.data ?: fail("This activity expects a file")
            val fileStream = contentResolver.openInputStream(testFile)
                    ?: fail("Could not open file InputStream")
            val contents = InputStreamReader(fileStream, StandardCharsets.UTF_8).use {
                it.readText()
            }

            val view = TextView(this)
            view.text = contents
            setContentView(view)
        }
    }
}