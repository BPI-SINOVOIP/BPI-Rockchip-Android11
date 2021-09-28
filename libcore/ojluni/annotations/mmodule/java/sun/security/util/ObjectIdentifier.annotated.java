/*
 * Copyright (c) 1996, 2006, Oracle and/or its affiliates. All rights reserved.
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


package sun.security.util;

import java.io.*;
import java.math.BigInteger;

@libcore.api.CorePlatformApi
@libcore.api.Hide
@SuppressWarnings({"unchecked", "deprecation", "all"})
public final class ObjectIdentifier implements java.io.Serializable {

@libcore.api.CorePlatformApi
public ObjectIdentifier(java.lang.String oid) throws java.io.IOException { throw new RuntimeException("Stub!"); }

public ObjectIdentifier(int[] values) throws java.io.IOException { throw new RuntimeException("Stub!"); }

public ObjectIdentifier(sun.security.util.DerInputStream in) throws java.io.IOException { throw new RuntimeException("Stub!"); }

public static sun.security.util.ObjectIdentifier newInternal(int[] values) { throw new RuntimeException("Stub!"); }

@Deprecated
public boolean equals(sun.security.util.ObjectIdentifier other) { throw new RuntimeException("Stub!"); }

public boolean equals(java.lang.Object obj) { throw new RuntimeException("Stub!"); }

public int hashCode() { throw new RuntimeException("Stub!"); }

public int[] toIntArray() { throw new RuntimeException("Stub!"); }

public java.lang.String toString() { throw new RuntimeException("Stub!"); }
}

