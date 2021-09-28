/*
 * Copyright (c) 1997, 2012, Oracle and/or its affiliates. All rights reserved.
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

package sun.security.x509;

import java.util.*;
import sun.security.util.*;

@SuppressWarnings({"unchecked", "deprecation", "all"})
public class CertificateExtensions
        implements sun.security.x509.CertAttrSet<sun.security.x509.Extension> {

    @android.compat.annotation.UnsupportedAppUsage
    public CertificateExtensions() {
        throw new RuntimeException("Stub!");
    }

    @android.compat.annotation.UnsupportedAppUsage
    public CertificateExtensions(sun.security.util.DerInputStream in) throws java.io.IOException {
        throw new RuntimeException("Stub!");
    }

    private void init(sun.security.util.DerInputStream in) throws java.io.IOException {
        throw new RuntimeException("Stub!");
    }

    private void parseExtension(sun.security.x509.Extension ext) throws java.io.IOException {
        throw new RuntimeException("Stub!");
    }

    public void encode(java.io.OutputStream out)
            throws java.security.cert.CertificateException, java.io.IOException {
        throw new RuntimeException("Stub!");
    }

    @android.compat.annotation.UnsupportedAppUsage
    public void encode(java.io.OutputStream out, boolean isCertReq)
            throws java.security.cert.CertificateException, java.io.IOException {
        throw new RuntimeException("Stub!");
    }

    @android.compat.annotation.UnsupportedAppUsage
    public void set(java.lang.String name, java.lang.Object obj) throws java.io.IOException {
        throw new RuntimeException("Stub!");
    }

    @android.compat.annotation.UnsupportedAppUsage
    public sun.security.x509.Extension get(java.lang.String name) throws java.io.IOException {
        throw new RuntimeException("Stub!");
    }

    sun.security.x509.Extension getExtension(java.lang.String name) {
        throw new RuntimeException("Stub!");
    }

    public void delete(java.lang.String name) throws java.io.IOException {
        throw new RuntimeException("Stub!");
    }

    public java.lang.String getNameByOid(sun.security.util.ObjectIdentifier oid)
            throws java.io.IOException {
        throw new RuntimeException("Stub!");
    }

    public java.util.Enumeration<sun.security.x509.Extension> getElements() {
        throw new RuntimeException("Stub!");
    }

    public java.util.Collection<sun.security.x509.Extension> getAllExtensions() {
        throw new RuntimeException("Stub!");
    }

    public java.util.Map<java.lang.String, sun.security.x509.Extension> getUnparseableExtensions() {
        throw new RuntimeException("Stub!");
    }

    public java.lang.String getName() {
        throw new RuntimeException("Stub!");
    }

    public boolean hasUnsupportedCriticalExtension() {
        throw new RuntimeException("Stub!");
    }

    public boolean equals(java.lang.Object other) {
        throw new RuntimeException("Stub!");
    }

    public int hashCode() {
        throw new RuntimeException("Stub!");
    }

    public java.lang.String toString() {
        throw new RuntimeException("Stub!");
    }

    public static final java.lang.String IDENT = "x509.info.extensions";

    public static final java.lang.String NAME = "extensions";

    private static java.lang.Class[] PARAMS;

    private static final sun.security.util.Debug debug;

    static {
        debug = null;
    }

    private java.util.Map<java.lang.String, sun.security.x509.Extension> map;

    private java.util.Map<java.lang.String, sun.security.x509.Extension> unparseableExtensions;

    private boolean unsupportedCritExt = false;
}
