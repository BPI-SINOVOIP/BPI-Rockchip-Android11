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

import android.util.Log;

import java.util.Objects;

/**
 * Describes a single native or variant format available for an image, coming from a
 * BipImageProperties object.
 *
 * This is not an object by specification, per say. It abstracts all the various native and variant
 * formats available in a given set of image properties.
 *
 * This BipImageFormat can be used to choose a specific BipImageDescriptor when downloading an image
 *
 * Examples:
 *   <native encoding="JPEG" pixel="1280*1024” size="1048576"/>
 *   <variant encoding="JPEG" pixel="640*480"/>
 *   <variant encoding="JPEG" pixel="160*120"/>
 *   <variant encoding="GIF" pixel="80*60-640*480"/>
 */
public class BipImageFormat {
    private static final String TAG = "avrcpcontroller.BipImageFormat";

    public static final int FORMAT_NATIVE = 0;
    public static final int FORMAT_VARIANT = 1;

    /**
     * Create a native BipImageFormat from the given string fields
     */
    public static BipImageFormat parseNative(String encoding, String pixel, String size) {
        return new BipImageFormat(BipImageFormat.FORMAT_NATIVE, encoding, pixel, size, null, null);
    }

    /**
     * Create a variant BipImageFormat from the given string fields
     */
    public static BipImageFormat parseVariant(String encoding, String pixel, String maxSize,
            String transformation) {
        return new BipImageFormat(BipImageFormat.FORMAT_VARIANT, encoding, pixel, null, maxSize,
                transformation);
    }

    /**
     * Create a native BipImageFormat from the given parameters
     */
    public static BipImageFormat createNative(BipEncoding encoding, BipPixel pixel, int size) {
        return new BipImageFormat(BipImageFormat.FORMAT_NATIVE, encoding, pixel, size, -1, null);
    }

    /**
     * Create a variant BipImageFormat from the given parameters
     */
    public static BipImageFormat createVariant(BipEncoding encoding, BipPixel pixel, int maxSize,
            BipTransformation transformation) {
        return new BipImageFormat(BipImageFormat.FORMAT_VARIANT, encoding, pixel, -1, maxSize,
                transformation);
    }

    /**
     * The 'flavor' of this image format, from the format constants above.
     */
    private final int mFormatType;

    /**
     * The encoding method in which this image is available, required by the specification
     */
    private final BipEncoding mEncoding;

    /**
     * The pixel size or range of pixel sizes in which the image is available, required by the
     * specification
     */
    private final BipPixel mPixel;

    /**
     * The list of supported image transformation methods, any of:
     *   - 'stretch' : Image server is capable of stretching the image to fit a space
     *   - 'fill' : Image server is capable of filling the image padding data to fit a space
     *   - 'crop' : Image server is capable of cropping the image down to fit a space
     *
     * Used by the variant type only
     */
    private final BipTransformation mTransformation;

    /**
     * Size in bytes of the image.
     *
     * Used by the native type only
     */
    private final int mSize;

    /**
     * The estimated maximum size of an image after a transformation is performed.
     *
     * Used by the variant type only
     */
    private final int mMaxSize;

    private BipImageFormat(int type, BipEncoding encoding, BipPixel pixel, int size, int maxSize,
            BipTransformation transformation) {
        mFormatType = type;
        mEncoding = Objects.requireNonNull(encoding, "Encoding cannot be null");
        mPixel = Objects.requireNonNull(pixel, "Pixel cannot be null");
        mTransformation = transformation;
        mSize = size;
        mMaxSize = maxSize;
    }

    private BipImageFormat(int type, String encoding, String pixel, String size, String maxSize,
            String transformation) {
        mFormatType = type;
        mEncoding = new BipEncoding(encoding);
        mPixel = new BipPixel(pixel);
        mTransformation = new BipTransformation(transformation);
        mSize = parseInt(size);
        mMaxSize = parseInt(maxSize);
    }

    private static int parseInt(String s) {
        if (s == null) return -1;
        try {
            return Integer.parseInt(s);
        } catch (NumberFormatException e) {
            error("Failed to parse '" + s + "'");
        }
        return -1;
    }

    public int getType() {
        return mFormatType;
    }

    public BipEncoding getEncoding() {
        return mEncoding;
    }

    public BipPixel getPixel() {
        return mPixel;
    }

    public BipTransformation getTransformation() {
        return mTransformation;
    }

    public int getSize() {
        return mSize;
    }

    public int getMaxSize() {
        return mMaxSize;
    }

    @Override
    public boolean equals(Object o) {
        if (o == this) return true;
        if (!(o instanceof BipImageFormat)) return false;

        BipImageFormat f = (BipImageFormat) o;
        return f.getType() == getType()
                && f.getEncoding() == getEncoding()
                && f.getPixel() == getPixel()
                && f.getTransformation() == getTransformation()
                && f.getSize() == getSize()
                && f.getMaxSize() == getMaxSize();
    }

    @Override
    public String toString() {
        if (mEncoding == null || mEncoding.getType() == BipEncoding.UNKNOWN || mPixel == null
                || mPixel.getType() == BipPixel.TYPE_UNKNOWN) {
            error("Missing required fields [ " + (mEncoding == null ? "encoding " : "")
                    + (mPixel == null ? "pixel " : ""));
            return null;
        }

        StringBuilder sb = new StringBuilder();
        switch (mFormatType) {
            case FORMAT_NATIVE:
                sb.append("<native");
                sb.append(" encoding=\"" + mEncoding.toString() + "\"");
                sb.append(" pixel=\"" + mPixel.toString() + "\"");
                if (mSize > -1) {
                    sb.append(" size=\"" + mSize + "\"");
                }
                sb.append(" />");
                return sb.toString();
            case FORMAT_VARIANT:
                sb.append("<variant");
                sb.append(" encoding=\"" + mEncoding.toString() + "\"");
                sb.append(" pixel=\"" + mPixel.toString() + "\"");
                if (mTransformation != null && mTransformation.supportsAny()) {
                    sb.append(" transformation=\"" + mTransformation.toString() + "\"");
                }
                if (mSize > -1) {
                    sb.append(" size=\"" + mSize + "\"");
                }
                if (mMaxSize > -1) {
                    sb.append(" maxsize=\"" + mMaxSize + "\"");
                }
                sb.append(" />");
                return sb.toString();
            default:
                error("Unsupported format type '" + mFormatType + "'");
        }
        return null;
    }

    private static void error(String msg) {
        Log.e(TAG, msg);
    }
}
