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

import java.io.File
import java.io.FileDescriptor
import java.net.InetAddress
import java.security.Permission
import kotlin.concurrent.getOrSet

/**
 * Detect access to files that not explicitly specified in the command line.
 *
 * This class detects reads on both files and directories. Directory accesses are logged by
 * [Driver], which only logs it, but doesn't consider it an error.
 *
 * We do not prevent reads on directories that are not explicitly listed in the command line because
 * metalava (or JVM, really) seems to read some system directories such as /usr/, etc., but this
 * behavior may be JVM dependent so we do not want to have to explicitly whitelist them.
 * (Because, otherwise, when we update JVM, it may access different directories and we end up
 * having to update the implicit whitelist.) As long as we don't read files, reading directories
 * shouldn't (normally) affect the result, so we simply allow any directory reads.
 */
internal object FileReadSandbox {
    private var installed = false

    private var previousSecurityManager: SecurityManager? = null
    private val mySecurityManager = MySecurityManager()

    /**
     * Set of paths that are allowed to access. This is used with an exact match.
     */
    private var allowedPaths = mutableSetOf<String>()

    /**
     * Set of path prefixes that are allowed to access.
     */
    private var allowedPathPrefixes = mutableSetOf<String>()

    interface Listener {
        /** Called when a violation is detected with the absolute path. */
        fun onViolation(absolutePath: String, isDirectory: Boolean)
    }

    private var listener: Listener? = null

    init {
        initialize()
    }

    private fun initialize() {
        allowedPaths.clear()
        allowedPathPrefixes.clear()
        listener = null

        // Allow certain directories by default.
        // The sandbox detects all file accessed done by JVM implicitly too (e.g. JVM seems to
        // access /dev/urandom on linux), so we need to allow them.
        // At least on linux, there doesn't seem to be any access to files under /proc/ or /sys/,
        // but they should be allowed anyway.
        listOf("/dev", "/proc", "/sys").forEach {
            allowAccess(File(it))
        }
        // Allow access to the directory containing the metalava's jar itself.
        // (so even if metalava loads other jar files in the same directory, that wouldn't be a
        // violation.)
        FileReadSandbox::class.java.protectionDomain?.codeSource?.location?.toURI()?.path?.let {
            allowAccess(File(it).parentFile)
        }

        // Allow access to $JAVA_HOME.
        // We also allow $ANDROID_JAVA_HOME, which is used in the Android platform build.
        // (which is normally $ANDROID_BUILD_TOP + "/prebuilts/jdk/jdk11/linux-x86" as of writing.)
        listOf(
            "JAVA_HOME",
            "ANDROID_JAVA_HOME"
        ).forEach {
            System.getenv(it)?.let {
                allowAccess(File(it))
            }
        }
        // JVM seems to use ~/.cache/
        System.getenv("HOME")?.let {
            allowAccess(File("$it/.cache"))
        }
    }

    /** Activate the sandbox. */
    fun activate(listener: Listener) {
        if (installed) {
            throw IllegalStateException("Already activated")
        }
        previousSecurityManager = System.getSecurityManager()
        System.setSecurityManager(mySecurityManager)
        installed = true
        this.listener = listener
    }

    /** Deactivate the sandbox. This also resets [violationCount]. */
    fun deactivate() {
        if (!installed) {
            throw IllegalStateException("Not activated")
        }
        System.setSecurityManager(previousSecurityManager)
        previousSecurityManager = null

        installed = false
    }

    /** Reset all the state and re-initialize the sandbox. Only callable when not activated. */
    fun reset() {
        if (installed) {
            throw IllegalStateException("Can't reset. Already activated")
        }
        initialize()
    }

    /** Allows access to files or directories. */
    fun allowAccess(files: List<File>): List<File> {
        files.forEach { allowAccess(it) }
        return files
    }

    /** Allows access to a file or directory. */
    fun <T : File?> allowAccess(file: T): T {
        if (file == null) {
            return file
        }
        val path = file.absolutePath
        val added = allowedPaths.add(path)
        if (file.isDirectory) {
            // Even if it had already been added to allowedPaths (i.e. added == false),
            // the directory might have just been created, so we still do it.
            allowedPathPrefixes.add("$path/")
        }
        if (!added) {
            // Already added; bail early.
            return file
        }

        // Whitelist all parent directories. But don't allow prefix accesses (== access to the
        // directory itself is okay, but don't grant access to any files/directories under it).
        var parent = file.parentFile
        while (true) {
            if (parent == null) {
                break
            }
            @Suppress("NAME_SHADOWING")
            val path = parent.absolutePath
            if (!allowedPaths.add(path)) {
                break // already added.
            }
            if (path == "/") {
                break
            }
            parent = parent.parentFile
        }
        return file
    }

    fun isAccessAllowed(file: File): Boolean {
        if (!file.exists()) {
            return true
        }
        val absPath = file.absolutePath

        if (allowedPaths.contains(absPath)) {
            return true
        }
        allowedPathPrefixes.forEach {
            if (absPath.startsWith(it)) {
                return true
            }
        }
        return false
    }

    fun isDirectory(file: File): Boolean {
        try {
            temporaryExempt.set(true)
            return file.isDirectory()
        } finally {
            temporaryExempt.set(false)
        }
    }

    fun isFile(file: File): Boolean {
        try {
            temporaryExempt.set(true)
            return file.isFile()
        } finally {
            temporaryExempt.set(false)
        }
    }

    /** Used to skip all checks on any filesystem access made within the [check] method. */
    private val temporaryExempt = ThreadLocal<Boolean>()

    private fun check(path: String) {
        if (temporaryExempt.getOrSet { false }) {
            return
        }

        try {
            temporaryExempt.set(true)

            val file = File(path)

            if (!isAccessAllowed(file)) {
                listener?.onViolation(file.absolutePath, file.isDirectory)
            }
        } finally {
            temporaryExempt.set(false)
        }
    }

    /**
     * Reading files that are created by metalava should be allowed, so we detect file writes to
     * new files, and whitelist it.
     */
    private fun writeDetected(origPath: String?) {
        if (temporaryExempt.getOrSet { false }) {
            return
        }

        try {
            temporaryExempt.set(true)

            val file = File(origPath)
            if (file.exists()) {
                return // Ignore writes to an existing file / directory.
            }
            allowedPaths.add(file.absolutePath)
        } finally {
            temporaryExempt.set(false)
        }
    }

    private class MySecurityManager : SecurityManager() {
        override fun checkRead(file: String) {
            check(file)
        }

        override fun checkRead(file: String, context: Any?) {
            check(file)
        }
        override fun checkRead(p0: FileDescriptor?) {
        }

        override fun checkDelete(p0: String?) {
        }

        override fun checkPropertiesAccess() {
        }

        override fun checkAccess(p0: Thread?) {
        }

        override fun checkAccess(p0: ThreadGroup?) {
        }

        override fun checkExec(p0: String?) {
        }

        override fun checkListen(p0: Int) {
        }

        override fun checkExit(p0: Int) {
        }

        override fun checkLink(p0: String?) {
        }

        override fun checkPropertyAccess(p0: String?) {
        }

        override fun checkPackageDefinition(p0: String?) {
        }

        override fun checkMulticast(p0: InetAddress?) {
        }

        override fun checkMulticast(p0: InetAddress?, p1: Byte) {
        }

        override fun checkPermission(p0: Permission?) {
        }

        override fun checkPermission(p0: Permission?, p1: Any?) {
        }

        override fun checkPackageAccess(p0: String?) {
        }

        override fun checkAccept(p0: String?, p1: Int) {
        }

        override fun checkSecurityAccess(p0: String?) {
        }

        override fun checkWrite(p0: FileDescriptor?) {
        }

        override fun checkWrite(file: String?) {
            writeDetected(file)
        }

        override fun checkPrintJobAccess() {
        }

        override fun checkCreateClassLoader() {
        }

        override fun checkConnect(p0: String?, p1: Int) {
        }

        override fun checkConnect(p0: String?, p1: Int, p2: Any?) {
        }

        override fun checkSetFactory() {
        }
    }
}
