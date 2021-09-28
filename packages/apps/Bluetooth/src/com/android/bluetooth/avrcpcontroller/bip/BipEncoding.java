/*
 * Copyright (C) 2019 The Android Open Source Project
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

package com.android.bluetooth.avrcpcontroller;

import android.util.SparseArray;

import java.util.HashMap;

/**
 * Represents an encoding method in which a BIP image is available.
 *
 * The encodings supported by this profile include:
 *   - JPEG
 *   - GIF
 *   - WBMP
 *   - PNG
 *   - JPEG2000
 *   - BMP
 *   - USR-xxx
 *
 * The tag USR-xxx is used to represent proprietary encodings. The tag shall begin with the string
 * “USR-” but the implementer assigns the characters of the second half of the string. This tag can
 * be used by a manufacturer to enable its devices to exchange images under a proprietary encoding.
 *
 * Example proprietary encoding:
 *
 *   - USR-NOKIA-FORMAT1
 */
public class BipEncoding {
    public static final int JPEG = 0;
    public static final int PNG = 1;
    public static final int BMP = 2;
    public static final int GIF = 3;
    public static final int JPEG2000 = 4;
    public static final int WBMP = 5;
    public static final int USR_XXX = 6;
    public static final int UNKNOWN = 7; // i.e 'not assigned' or 'not assigned anything valid'

    private static final HashMap sEncodingNamesToIds = new HashMap<String, Integer>();
    static {
        sEncodingNamesToIds.put("JPEG", JPEG);
        sEncodingNamesToIds.put("GIF", GIF);
        sEncodingNamesToIds.put("WBMP", WBMP);
        sEncodingNamesToIds.put("PNG", PNG);
        sEncodingNamesToIds.put("JPEG2000", JPEG2000);
        sEncodingNamesToIds.put("BMP", BMP);
    }

    private static final SparseArray sIdsToEncodingNames = new SparseArray<String>();
    static {
        sIdsToEncodingNames.put(JPEG, "JPEG");
        sIdsToEncodingNames.put(GIF, "GIF");
        sIdsToEncodingNames.put(WBMP, "WBMP");
        sIdsToEncodingNames.put(PNG, "PNG");
        sIdsToEncodingNames.put(JPEG2000, "JPEG2000");
        sIdsToEncodingNames.put(BMP, "BMP");
        sIdsToEncodingNames.put(UNKNOWN, "UNKNOWN");
    }

    /**
     * The integer ID of the type that this encoding is
     */
    private final int mType;

    /**
     * If an encoding is type USR_XXX then it has an extension that defines the encoding
     */
    private final String mProprietaryEncodingId;

    /**
     * Create an encoding object based on a AVRCP specification defined string of the encoding name
     *
     * @param encoding The encoding name
     */
    public BipEncoding(String encoding) {
        if (encoding == null) {
            throw new ParseException("Encoding input invalid");
        }
        encoding = encoding.trim();
        mType = determineEncoding(encoding.toUpperCase());

        String proprietaryEncodingId = null;
        if (mType == USR_XXX) {
            proprietaryEncodingId = encoding.substring(4).toUpperCase();
        }
        mProprietaryEncodingId = proprietaryEncodingId;

        // If we don't have a type by now, we've failed to parse the encoding
        if (mType == UNKNOWN) {
            throw new ParseException("Failed to determine type of '" + encoding + "'");
        }
    }

    /**
     * Create an encoding object based on one of the constants for the available formats
     *
     * @param encoding A constant representing an available encoding
     * @param proprietaryId A string representing the Id of a propreitary encoding. Only used if the
     *                      encoding type is BipEncoding.USR_XXX
     */
    public BipEncoding(int encoding, String proprietaryId) {
        if (encoding < 0 || encoding > USR_XXX) {
            throw new IllegalArgumentException("Received invalid encoding type '" + encoding + "'");
        }
        mType = encoding;

        String proprietaryEncodingId = null;
        if (mType == USR_XXX) {
            if (proprietaryId == null) {
                throw new IllegalArgumentException("Received invalid user defined encoding id '"
                        + proprietaryId + "'");
            }
            proprietaryEncodingId = proprietaryId.toUpperCase();
        }
        mProprietaryEncodingId = proprietaryEncodingId;
    }

    public BipEncoding(int encoding) {
        this(encoding, null);
    }

    /**
     * Returns the encoding type
     *
     * @return Integer type ID of the encoding
     */
    public int getType() {
        return mType;
    }

    /**
     * Returns the ID portion of an encoding if it's a proprietary encoding
     *
     * @return String ID of a proprietary encoding, or null if the encoding is not proprietary
     */
    public String getProprietaryEncodingId() {
        return mProprietaryEncodingId;
    }

    /**
     * Determines if an encoding is supported by Android's Graphics Framework
     *
     * Android's Bitmap/BitmapFactory can handle BMP, GIF, JPEG, PNG, WebP, and HEIF formats.
     *
     * @return True if the encoding is supported, False otherwise.
     */
    public boolean isAndroidSupported() {
        return mType == BipEncoding.JPEG || mType == BipEncoding.PNG || mType == BipEncoding.BMP
                || mType == BipEncoding.GIF;
    }

    /**
     * Determine the encoding type based on an input string
     */
    private static int determineEncoding(String encoding) {
        Integer type = (Integer) sEncodingNamesToIds.get(encoding);
        if (type != null) return type.intValue();
        if (encoding != null && encoding.length() >= 4 && encoding.substring(0, 4).equals("USR-")) {
            return USR_XXX;
        }
        return UNKNOWN;
    }

    @Override
    public boolean equals(Object o) {
        if (o == this) return true;
        if (!(o instanceof BipEncoding)) return false;

        BipEncoding e = (BipEncoding) o;
        return e.getType() == getType()
                && e.getProprietaryEncodingId() == getProprietaryEncodingId();
    }

    @Override
    public String toString() {
        if (mType == USR_XXX) return "USR-" + mProprietaryEncodingId;
        String encoding = (String) sIdsToEncodingNames.get(mType);
        return encoding;
    }
}
