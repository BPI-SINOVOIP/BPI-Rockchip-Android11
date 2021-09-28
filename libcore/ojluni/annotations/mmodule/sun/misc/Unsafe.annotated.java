/*
 * Copyright (c) 2000, 2013, Oracle and/or its affiliates. All rights reserved.
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

package sun.misc;

// sun.misc.Unsafe is part of the Core Platform API as platform uses protobuf and protobuf uses
// this API for fast structure updates from native code (b/144502743).
@libcore.api.CorePlatformApi
@libcore.api.Hide
@SuppressWarnings({"unchecked", "deprecation", "all"})
public final class Unsafe {

    private Unsafe() {
        throw new RuntimeException("Stub!");
    }

    @libcore.api.CorePlatformApi
    public static sun.misc.Unsafe getUnsafe() {
        throw new RuntimeException("Stub!");
    }

    @libcore.api.CorePlatformApi
    public int arrayBaseOffset(Class clazz) {
        throw new RuntimeException("Stub!");
    }

    @libcore.api.CorePlatformApi
    public int arrayIndexScale(Class clazz) {
        throw new RuntimeException("Stub!");
    }

    @libcore.api.CorePlatformApi
    public native void copyMemory(long srcAddr, long destAddr, long bytes);

    @libcore.api.CorePlatformApi
    public native boolean getBoolean(Object obj, long offset);

    @libcore.api.CorePlatformApi
    public native byte getByte(long address);

    @libcore.api.CorePlatformApi
    public native byte getByte(Object obj, long offset);

    @libcore.api.CorePlatformApi
    public native double getDouble(Object obj, long offset);

    @libcore.api.CorePlatformApi
    public native float getFloat(Object obj, long offset);

    @libcore.api.CorePlatformApi
    public native int getInt(long address);

    @libcore.api.CorePlatformApi
    public native int getInt(Object obj, long offset);

    @libcore.api.CorePlatformApi
    public native long getLong(long address);

    @libcore.api.CorePlatformApi
    public native long getLong(Object obj, long offset);

    @libcore.api.CorePlatformApi
    public native Object getObject(Object obj, long offset);

    @libcore.api.CorePlatformApi
    public long objectFieldOffset(java.lang.reflect.Field field) {
        throw new RuntimeException("Stub!");
    }

    @libcore.api.CorePlatformApi
    public native void putBoolean(Object obj, long offset, boolean newValue);

    @libcore.api.CorePlatformApi
    public native void putByte(long address, byte newValue);

    @libcore.api.CorePlatformApi
    public native void putByte(Object obj, long offset, byte newValue);

    @libcore.api.CorePlatformApi
    public native void putDouble(Object obj, long offset, double newValue);

    @libcore.api.CorePlatformApi
    public native void putFloat(Object obj, long offset, float newValue);

    @libcore.api.CorePlatformApi
    public native void putInt(long address, int newValue);

    @libcore.api.CorePlatformApi
    public native void putInt(Object obj, long offset, int newValue);

    @libcore.api.CorePlatformApi
    public native void putLong(long address, long newValue);

    @libcore.api.CorePlatformApi
    public native void putLong(Object obj, long offset, long newValue);

    @libcore.api.CorePlatformApi
    public native void putObject(Object obj, long offset, Object newValue);
}
