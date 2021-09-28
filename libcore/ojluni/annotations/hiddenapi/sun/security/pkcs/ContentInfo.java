/*
 * Copyright (c) 1996, 2011, Oracle and/or its affiliates. All rights reserved.
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

package sun.security.pkcs;

import java.io.*;
import sun.security.util.*;

@SuppressWarnings({"unchecked", "deprecation", "all"})
public class ContentInfo {

    @android.compat.annotation.UnsupportedAppUsage
    public ContentInfo(
            sun.security.util.ObjectIdentifier contentType, sun.security.util.DerValue content) {
        throw new RuntimeException("Stub!");
    }

    @android.compat.annotation.UnsupportedAppUsage
    public ContentInfo(byte[] bytes) {
        throw new RuntimeException("Stub!");
    }

    public ContentInfo(sun.security.util.DerInputStream derin)
            throws java.io.IOException, sun.security.pkcs.ParsingException {
        throw new RuntimeException("Stub!");
    }

    public ContentInfo(sun.security.util.DerInputStream derin, boolean oldStyle)
            throws java.io.IOException, sun.security.pkcs.ParsingException {
        throw new RuntimeException("Stub!");
    }

    public sun.security.util.DerValue getContent() {
        throw new RuntimeException("Stub!");
    }

    public sun.security.util.ObjectIdentifier getContentType() {
        throw new RuntimeException("Stub!");
    }

    @android.compat.annotation.UnsupportedAppUsage
    public byte[] getData() throws java.io.IOException {
        throw new RuntimeException("Stub!");
    }

    @android.compat.annotation.UnsupportedAppUsage
    public void encode(sun.security.util.DerOutputStream out) throws java.io.IOException {
        throw new RuntimeException("Stub!");
    }

    public byte[] getContentBytes() throws java.io.IOException {
        throw new RuntimeException("Stub!");
    }

    public java.lang.String toString() {
        throw new RuntimeException("Stub!");
    }

    @android.compat.annotation.UnsupportedAppUsage
    public static sun.security.util.ObjectIdentifier DATA_OID;

    public static sun.security.util.ObjectIdentifier DIGESTED_DATA_OID;

    public static sun.security.util.ObjectIdentifier ENCRYPTED_DATA_OID;

    public static sun.security.util.ObjectIdentifier ENVELOPED_DATA_OID;

    public static sun.security.util.ObjectIdentifier NETSCAPE_CERT_SEQUENCE_OID;

    private static final int[] OLD_DATA;

    static {
        OLD_DATA = new int[0];
    }

    public static sun.security.util.ObjectIdentifier OLD_DATA_OID;

    private static final int[] OLD_SDATA;

    static {
        OLD_SDATA = new int[0];
    }

    public static sun.security.util.ObjectIdentifier OLD_SIGNED_DATA_OID;

    public static sun.security.util.ObjectIdentifier PKCS7_OID;

    public static sun.security.util.ObjectIdentifier SIGNED_AND_ENVELOPED_DATA_OID;

    public static sun.security.util.ObjectIdentifier SIGNED_DATA_OID;

    public static sun.security.util.ObjectIdentifier TIMESTAMP_TOKEN_INFO_OID;

    sun.security.util.DerValue content;

    sun.security.util.ObjectIdentifier contentType;

    private static int[] crdata;

    private static int[] data;

    private static int[] ddata;

    private static int[] edata;

    private static int[] nsdata;

    private static int[] pkcs7;

    private static int[] sdata;

    private static int[] sedata;

    private static int[] tstInfo;
}
