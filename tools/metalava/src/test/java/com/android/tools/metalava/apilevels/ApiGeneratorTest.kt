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

package com.android.tools.metalava.apilevels

import com.android.sdklib.repository.AndroidSdkHandler
import com.android.tools.lint.LintCliClient
import com.android.tools.lint.checks.ApiLookup
import com.android.tools.metalava.ARG_ANDROID_JAR_PATTERN
import com.android.tools.metalava.ARG_CURRENT_CODENAME
import com.android.tools.metalava.ARG_CURRENT_VERSION
import com.android.tools.metalava.ARG_GENERATE_API_LEVELS
import com.android.tools.metalava.DriverTest
import com.android.utils.XmlUtils
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertTrue
import org.junit.Test
import java.io.File
import kotlin.text.Charsets.UTF_8

class ApiGeneratorTest : DriverTest() {
    @Test
    fun `Extract API levels`() {
        var oldSdkJars = File("prebuilts/tools/common/api-versions")
        if (!oldSdkJars.isDirectory) {
            oldSdkJars = File("../../prebuilts/tools/common/api-versions")
            if (!oldSdkJars.isDirectory) {
                println("Ignoring ${ApiGeneratorTest::class.java}: prebuilts not found - is \$PWD set to an Android source tree?")
                return
            }
        }

        var platformJars = File("prebuilts/sdk")
        if (!platformJars.isDirectory) {
            platformJars = File("../../prebuilts/sdk")
            if (!platformJars.isDirectory) {
                println("Ignoring ${ApiGeneratorTest::class.java}: prebuilts not found: $platformJars")
                return
            }
        }
        val output = File.createTempFile("api-info", "xml")
        output.deleteOnExit()
        val outputPath = output.path

        check(
            extraArguments = arrayOf(
                ARG_GENERATE_API_LEVELS,
                outputPath,
                ARG_ANDROID_JAR_PATTERN,
                "${oldSdkJars.path}/android-%/android.jar",
                ARG_ANDROID_JAR_PATTERN,
                "${platformJars.path}/%/public/android.jar",
                ARG_CURRENT_CODENAME,
                "Z",
                ARG_CURRENT_VERSION,
                "35" // not real api level of Z
            ),
            sourceFiles = arrayOf(
                java(
                    """
                    package android.pkg;
                    public class MyTest {
                    }
                    """
                )
            )
        )

        assertTrue(output.isFile)

        val xml = output.readText(UTF_8)
        assertTrue(xml.contains("<class name=\"android/Manifest\$permission\" since=\"1\">"))
        assertTrue(xml.contains("<field name=\"BIND_CARRIER_MESSAGING_SERVICE\" since=\"22\" deprecated=\"23\"/>"))
        assertTrue(xml.contains("<class name=\"android/pkg/MyTest\" since=\"36\""))
        assertFalse(xml.contains("<implements name=\"java/lang/annotation/Annotation\" removed=\""))
        assertFalse(xml.contains("<extends name=\"java/lang/Enum\" removed=\""))
        assertFalse(xml.contains("<method name=\"append(C)Ljava/lang/AbstractStringBuilder;\""))

        // Also make sure package private super classes are pruned
        assertFalse(xml.contains("<extends name=\"android/icu/util/CECalendar\""))

        val document = XmlUtils.parseDocumentSilently(xml, false)
        assertNotNull(document)

        // Make sure we can process it via ApiLookup as well
        @Suppress("DEPRECATION") // still using older lint-api when building with soong
        val client = object : LintCliClient() {
            override fun findResource(relativePath: String): File? {
                if (relativePath == ApiLookup.XML_FILE_PATH) {
                    return output
                }
                return super.findResource(relativePath)
            }

            override fun getSdk(): AndroidSdkHandler? {
                return null
            }

            override fun getCacheDir(name: String?, create: Boolean): File? {
                return temporaryFolder.newFolder()
            }
        }

        val apiLookup = ApiLookup.get(client)
        apiLookup.getClassVersion("android.v")
        assertEquals(5, apiLookup.getFieldVersion("android.Manifest\$permission", "AUTHENTICATE_ACCOUNTS"))

        val methodVersion = apiLookup.getMethodVersion("android/icu/util/CopticCalendar", "computeTime", "()")
        assertEquals(24, methodVersion)
    }
}
