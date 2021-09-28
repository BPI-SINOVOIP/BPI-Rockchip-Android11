/*
 * Copyright (C) 2014 The Android Open Source Project
 * Copyright (c) 1994, 2013, Oracle and/or its affiliates. All rights reserved.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 * This code is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 only, as
 * published by the Free Software Foundation.  Oracle designates this
 * particular file as subject to the "Classpath" exception as provided
 * by Oracle in the LICENSE file that accompanied this code.
 *
 * This code is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * version 2 for more details (a copy is included in the LICENSE file that
 * accompanied this code).
 *
 * You should have received a copy of the GNU General Public License version
 * 2 along with this work; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * Please contact Oracle, 500 Oracle Parkway, Redwood Shores, CA 94065 USA
 * or visit www.oracle.com if you need additional information or have any
 * questions.
 */

package java.io;

import android.compat.annotation.UnsupportedAppUsage;

@SuppressWarnings({"unchecked", "deprecation", "all"})
public class FileInputStream extends java.io.InputStream {

    public FileInputStream(java.lang.String name) throws java.io.FileNotFoundException {
        throw new RuntimeException("Stub!");
    }

    public FileInputStream(java.io.File file) throws java.io.FileNotFoundException {
        throw new RuntimeException("Stub!");
    }

    public FileInputStream(java.io.FileDescriptor fdObj) {
        throw new RuntimeException("Stub!");
    }

    public FileInputStream(java.io.FileDescriptor fdObj, boolean isFdOwner) {
        throw new RuntimeException("Stub!");
    }

    private native void open0(java.lang.String name) throws java.io.FileNotFoundException;

    private void open(java.lang.String name) throws java.io.FileNotFoundException {
        throw new RuntimeException("Stub!");
    }

    public int read() throws java.io.IOException {
        throw new RuntimeException("Stub!");
    }

    public int read(byte[] b) throws java.io.IOException {
        throw new RuntimeException("Stub!");
    }

    public int read(byte[] b, int off, int len) throws java.io.IOException {
        throw new RuntimeException("Stub!");
    }

    public long skip(long n) throws java.io.IOException {
        throw new RuntimeException("Stub!");
    }

    private native long skip0(long n)
            throws java.io.FileInputStream.UseManualSkipException, java.io.IOException;

    public int available() throws java.io.IOException {
        throw new RuntimeException("Stub!");
    }

    private native int available0() throws java.io.IOException;

    public void close() throws java.io.IOException {
        throw new RuntimeException("Stub!");
    }

    public final java.io.FileDescriptor getFD() throws java.io.IOException {
        throw new RuntimeException("Stub!");
    }

    public java.nio.channels.FileChannel getChannel() {
        throw new RuntimeException("Stub!");
    }

    protected void finalize() throws java.io.IOException {
        throw new RuntimeException("Stub!");
    }

    private java.nio.channels.FileChannel channel;

    private final java.lang.Object closeLock;

    {
        closeLock = null;
    }

    private volatile boolean closed = false;

    @UnsupportedAppUsage
    private final java.io.FileDescriptor fd;

    {
        fd = null;
    }

    private final dalvik.system.CloseGuard guard;

    {
        guard = null;
    }

    private final boolean isFdOwner;

    {
        isFdOwner = false;
    }

    private final java.lang.String path;

    {
        path = null;
    }

    private final libcore.io.IoTracker tracker;

    {
        tracker = null;
    }

    @SuppressWarnings({"unchecked", "deprecation", "all"})
    private static class UseManualSkipException extends java.lang.Exception {

        private UseManualSkipException() {
            throw new RuntimeException("Stub!");
        }
    }
}
