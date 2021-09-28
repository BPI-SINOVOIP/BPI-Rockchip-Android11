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

package com.android.tools.metalava

import org.junit.Rule
import org.junit.Test
import org.junit.rules.TemporaryFolder
import java.io.File
import kotlin.test.assertEquals
import kotlin.test.assertTrue

class FileReadSandboxTest {
    @get:Rule
    var temporaryFolder = TemporaryFolder()

    @Test
    fun `Test sandbox`() {
        // Create target files.
        val root = temporaryFolder.newFolder()

        val goodFile = File(root, "goodFile").apply { createNewFile() }
        val badFile = File(root, "badFile").apply { createNewFile() }

        val goodDir = File(root, "goodDir").apply { mkdirs() }
        val goodDirFile = File(goodDir, "file").apply { createNewFile() }

        val badDir = File(root, "badDir").apply { mkdirs() }
        val badDirFile = File(badDir, "file").apply { createNewFile() }

        val subDir = File(root, "subdir").apply { mkdirs() }
        val subSubDir = File(subDir, "subdir").apply { mkdirs() }
        val subSubDir2 = File(subDir, "subdir2").apply { mkdirs() }
        val subSubDirGoodFile = File(subSubDir, "good").apply { createNewFile() }
        val subSubDirBadFile = File(subSubDir, "bad").apply { createNewFile() }

        // Structure:
        //   - *  Explicitly allowed
        //   - ** Implicitly allowed (parent dirctories of whitelisted paths)
        //   - @  Not allowed
        // root        **
        //  |-goodFile *
        //  |
        //  |-badFile  @
        //  |
        //  |-goodDir  *
        //  |  |-goodDirFile **
        //  |
        //  |-badDir         @
        //  |  |-badDirFile  @
        //  |
        //  |-subDir **
        //     |-subSubDir **
        //     |  |-subSubDirGoodFile *
        //     |  |-subSubDirBadFile  @
        //     |-subSubDir2           @

        // Set up the sandbox.
        FileReadSandbox.allowAccess(goodFile)
        FileReadSandbox.allowAccess(goodDir)
        FileReadSandbox.allowAccess(subSubDirGoodFile)

        var allowedSet = mutableSetOf(root, goodFile, goodDir, goodDirFile, subDir, subSubDir, subSubDirGoodFile).map { it.absolutePath }
        var emptySet = setOf<String>()

        var violations = mutableSetOf<String>()

        // Make sure whitelisted files are not in the violation list.
        fun assertWhitelistedFilesNotReported() {
            assertEquals(emptySet, violations.intersect(allowedSet))
        }

        // Assert given files are reported as violations.
        fun assertViolationReported(vararg files: File) {
            val fileSet = files.map { it.absolutePath }.toSet()
            assertEquals(fileSet, violations.intersect(fileSet))
        }

        // Assert given files are *not* reported as violations.
        fun assertViolationNotReported(vararg files: File) {
            val fileSet = files.map { it.absolutePath }.toSet()
            assertEquals(emptySet, violations.intersect(fileSet))
        }

        val listener = object : FileReadSandbox.Listener {
            override fun onViolation(absolutePath: String, isDirectory: Boolean) {
                violations.add(absolutePath)
            }
        }

        // Activate the sandbox.
        FileReadSandbox.activate(listener)
        try {
            // Access "good" files, which should be allowed.
            goodFile.readBytes()
            subSubDirGoodFile.readBytes()

            // If a file is created by metalava, then it can be read.
            val newFile1 = File(root, "newFile1").apply { createNewFile() }
            newFile1.readBytes()

            assertWhitelistedFilesNotReported()
            assertViolationNotReported(badFile, badDir, badDirFile, subSubDirBadFile)

            // Access "bad" files.
            badFile.readBytes()
            badDirFile.readBytes()

            assertWhitelistedFilesNotReported()
            assertViolationReported(badFile, badDirFile)
            assertViolationNotReported(subSubDirBadFile)

            // Clear the violations, and access badDir.
            violations.clear()
            badDir.listFiles()
            root.listFiles()

            assertWhitelistedFilesNotReported()
            assertViolationReported(badDir)
            assertViolationNotReported(badFile, badDirFile, subSubDirBadFile)

            // Check "implicitly allowed access" directories.
            violations.clear()
            subDir.listFiles() // Implicitly allowed.
            subSubDir.listFiles() // Implicitly allowed.
            subSubDir2.listFiles() // *Not* implicitly allowed.
            subSubDirGoodFile.readBytes()
            subSubDirBadFile.readBytes() // *Not* allowed.
            root.listFiles()

            assertWhitelistedFilesNotReported()

            assertViolationReported(subSubDir2, subSubDirBadFile) // These are not allowed to read.
        } finally {
            // Deactivate the sandbox.
            FileReadSandbox.deactivate()
            FileReadSandbox.reset()
        }
        violations.clear()

        // This shouldn't trigger the listener.
        badFile.readBytes()

        assertTrue(violations.isEmpty())
    }
}
