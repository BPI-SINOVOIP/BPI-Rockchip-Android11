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
package com.android.se.security.gpac;

import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.util.Arrays;

/**
 * PKG-REF-DO: The PKG-REF-DO is used for retrieving and storing the corresponding access rules
 * for a device application (which is identified by the package name) from and to
 * the ARA
 */
public class PKG_REF_DO extends BerTlv {

    public static final int TAG = 0xCA;

    private String mPackageName = null;

    public PKG_REF_DO(byte[] rawData, int valueIndex, int valueLength) {
        super(rawData, TAG, valueIndex, valueLength);
    }

    public PKG_REF_DO(byte[] pkg) {
        super(pkg, TAG, 0, (pkg == null ? 0 : pkg.length));
        if (pkg != null) {
            mPackageName = new String(pkg);
        }
    }

    public PKG_REF_DO() {
        super(null, TAG, 0, 0);
    }

    /**
     * Comapares two PKG_REF_DO objects and returns true if they are equal
     */
    public static boolean equals(PKG_REF_DO obj1, PKG_REF_DO obj2) {
        if (obj1 == null) {
            return (obj2 == null) ? true : false;
        }
        return obj1.equals(obj2);
    }

    public String getPackageName() {
        return mPackageName;
    }

    @Override
    public String toString() {
        return new String("PKG_REF_DO: " + mPackageName);
    }

    /**
     * Tags: CA Length: max length is 128 bytes
     *
     * <p>Value: pkg: identifies ASCII encoded package name
     *
     */
    @Override
    public void interpret() throws ParserException {
        byte[] data = getRawData();
        int index = getValueIndex();

        // sanity checks
        if (getValueLength() > 128) {
            throw new ParserException("Invalid value length for PKG-REF-DO!");
        }

        if (index + getValueLength() > data.length) {
            throw new ParserException("Not enough data for PKG-REF-DO!");
        }

        byte[] pkg = new byte[getValueLength()];
        System.arraycopy(data, index, pkg, 0, getValueLength());
        mPackageName = new String(pkg);
    }

    /**
     * Tags: CA Length: max length is 128 bytes
     *
     * <p>Value: pkg: identifies ASCII encoded package name
     *
     */
    @Override
    public void build(ByteArrayOutputStream stream) throws DO_Exception {
        byte[] pkg = mPackageName.getBytes();
        // sanity checks
        if (pkg.length > 128) {
            throw new DO_Exception("Invalid value length for PKG-REF-DO!");
        }

        stream.write(getTag());

        try {
            stream.write(pkg.length);
            stream.write(pkg);
        } catch (IOException ioe) {
            throw new DO_Exception("PKG could not be written!");
        }
    }

    @Override
    public boolean equals(Object obj) {
        if (obj instanceof PKG_REF_DO) {
            PKG_REF_DO pkg_ref_do = (PKG_REF_DO) obj;
            if (getTag() == pkg_ref_do.getTag()) {
                if (mPackageName != null) {
                    return mPackageName.equals(pkg_ref_do.mPackageName);
                } else {
                    return (pkg_ref_do.mPackageName == null);
                }
            }
        }
        return false;
    }

    @Override
    public int hashCode() {
        ByteArrayOutputStream stream = new ByteArrayOutputStream();
        try {
            this.build(stream);
        } catch (DO_Exception e) {
            return 1;
        }
        byte[] data = stream.toByteArray();
        int hash = Arrays.hashCode(data);
        return hash;
    }
}
