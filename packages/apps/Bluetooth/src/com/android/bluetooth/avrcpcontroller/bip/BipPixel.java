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

import java.util.regex.Matcher;
import java.util.regex.Pattern;

/**
 * The pixel size or range of pixel sizes in which the image is available
 *
 * A FIXED size is represented as the following, where W is width and H is height. The domain
 * of values is [0, 65535]
 *
 *   W*H
 *
 * A RESIZABLE size that allows a modified aspect ratio is represented as the following, where
 * W_1*H_1 is the minimum width and height pair and W2*H2 is the maximum width and height pair.
 * The domain of values is [0, 65535]
 *
 *   W_1*H_1-W2*H2
 *
 * A RESIZABLE size that allows a fixed aspect ratio is represented as the following, where
 * W_1 is the minimum width and W2*H2 is the maximum width and height pair.
 * The domain of values is [0, 65535]
 *
 *   W_1**-W2*H2
 *
 * For each possible intermediate width value, the corresponding height is calculated using the
 * formula
 *
 *   H=(W*H2)/W2
 */
public class BipPixel {
    private static final String TAG = "avrcpcontroller.BipPixel";

    // The BIP specification declares this as the max size to be transferred. You can optionally
    // use this value to indicate there is no upper bound on pixel size.
    public static final int PIXEL_MAX = 65535;

    // Note that the integer values also map to the number of '*' delimiters that exist in each
    // formatted string
    public static final int TYPE_UNKNOWN = 0;
    public static final int TYPE_FIXED = 1;
    public static final int TYPE_RESIZE_MODIFIED_ASPECT_RATIO = 2;
    public static final int TYPE_RESIZE_FIXED_ASPECT_RATIO = 3;

    private final int mType;
    private final int mMinWidth;
    private final int mMinHeight;
    private final int mMaxWidth;
    private final int mMaxHeight;

    /**
     * Create a fixed size BipPixel object
     */
    public static BipPixel createFixed(int width, int height) {
        return new BipPixel(TYPE_FIXED, width, height, width, height);
    }

    /**
     * Create a resizable modifiable aspect ratio BipPixel object
     */
    public static BipPixel createResizableModified(int minWidth, int minHeight, int maxWidth,
            int maxHeight) {
        return new BipPixel(TYPE_RESIZE_MODIFIED_ASPECT_RATIO, minWidth, minHeight, maxWidth,
                maxHeight);
    }

    /**
     * Create a resizable fixed aspect ratio BipPixel object
     */
    public static BipPixel createResizableFixed(int minWidth, int maxWidth, int maxHeight) {
        int minHeight = (minWidth * maxHeight) / maxWidth;
        return new BipPixel(TYPE_RESIZE_FIXED_ASPECT_RATIO, minWidth, minHeight,
                maxWidth, maxHeight);
    }

    /**
     * Directly create a BipPixel object knowing your exact type and dimensions. Internal use only
     */
    private BipPixel(int type, int minWidth, int minHeight, int maxWidth, int maxHeight) {
        if (isDimensionInvalid(minWidth) || isDimensionInvalid(maxWidth)
                || isDimensionInvalid(minHeight) || isDimensionInvalid(maxHeight)) {
            throw new IllegalArgumentException("Dimension's must be in [0, " + PIXEL_MAX + "]");
        }

        mType = type;
        mMinWidth = minWidth;
        mMinHeight = minHeight;
        mMaxWidth = maxWidth;
        mMaxHeight = maxHeight;
    }

    /**
    * Create a BipPixel object from an Image Format pixel attribute string
     */
    public BipPixel(String pixel) {
        int type = TYPE_UNKNOWN;
        int minWidth = -1;
        int minHeight = -1;
        int maxWidth = -1;
        int maxHeight = -1;

        int typeHint = determinePixelType(pixel);
        switch (typeHint) {
            case TYPE_FIXED:
                Pattern fixed = Pattern.compile("^(\\d{1,5})\\*(\\d{1,5})$");
                Matcher m1 = fixed.matcher(pixel);
                if (m1.matches()) {
                    type = TYPE_FIXED;
                    minWidth = Integer.parseInt(m1.group(1));
                    maxWidth = Integer.parseInt(m1.group(1));
                    minHeight = Integer.parseInt(m1.group(2));
                    maxHeight = Integer.parseInt(m1.group(2));
                }
                break;
            case TYPE_RESIZE_MODIFIED_ASPECT_RATIO:
                Pattern modifiedRatio = Pattern.compile(
                        "^(\\d{1,5})\\*(\\d{1,5})-(\\d{1,5})\\*(\\d{1,5})$");
                Matcher m2 = modifiedRatio.matcher(pixel);
                if (m2.matches()) {
                    type = TYPE_RESIZE_MODIFIED_ASPECT_RATIO;
                    minWidth = Integer.parseInt(m2.group(1));
                    minHeight = Integer.parseInt(m2.group(2));
                    maxWidth = Integer.parseInt(m2.group(3));
                    maxHeight = Integer.parseInt(m2.group(4));
                }
                break;
            case TYPE_RESIZE_FIXED_ASPECT_RATIO:
                Pattern fixedRatio = Pattern.compile("^(\\d{1,5})\\*\\*-(\\d{1,5})\\*(\\d{1,5})$");
                Matcher m3 = fixedRatio.matcher(pixel);
                if (m3.matches()) {
                    type = TYPE_RESIZE_FIXED_ASPECT_RATIO;
                    minWidth = Integer.parseInt(m3.group(1));
                    maxWidth = Integer.parseInt(m3.group(2));
                    maxHeight = Integer.parseInt(m3.group(3));
                    minHeight = (minWidth * maxHeight) / maxWidth;
                }
                break;
            default:
                break;
        }
        if (type == TYPE_UNKNOWN) {
            throw new ParseException("Failed to determine type of '" + pixel + "'");
        }
        if (isDimensionInvalid(minWidth) || isDimensionInvalid(maxWidth)
                || isDimensionInvalid(minHeight) || isDimensionInvalid(maxHeight)) {
            throw new ParseException("Parsed dimensions must be in [0, " + PIXEL_MAX + "]");
        }

        mType = type;
        mMinWidth = minWidth;
        mMinHeight = minHeight;
        mMaxWidth = maxWidth;
        mMaxHeight = maxHeight;
    }

    public int getType() {
        return mType;
    }

    public int getMinWidth() {
        return mMinWidth;
    }

    public int getMaxWidth() {
        return mMaxWidth;
    }

    public int getMinHeight() {
        return mMinHeight;
    }

    public int getMaxHeight() {
        return mMaxHeight;
    }

    /**
     * Determines the type of the pixel string by counting the number of '*' delimiters in the
     * string.
     *
     * Note that the overall maximum size of any pixel string is 23 characters in length due to the
     * max size of each dimension
     *
     * @return The corresponding type we should assume the given pixel string is
     */
    private static int determinePixelType(String pixel) {
        if (pixel == null || pixel.length() > 23) return TYPE_UNKNOWN;
        int delimCount = 0;
        for (char c : pixel.toCharArray()) {
            if (c == '*') delimCount++;
        }
        return delimCount > 0 && delimCount <= 3 ? delimCount : TYPE_UNKNOWN;
    }

    protected static boolean isDimensionInvalid(int dimension) {
        return dimension < 0 || dimension > PIXEL_MAX;
    }

    @Override
    public boolean equals(Object o) {
        if (o == this) return true;
        if (!(o instanceof BipPixel)) return false;

        BipPixel p = (BipPixel) o;
        return p.getType() == getType()
                && p.getMinWidth() == getMinWidth()
                && p.getMaxWidth() == getMaxWidth()
                && p.getMinHeight() == getMinHeight()
                && p.getMaxHeight() == getMaxHeight();
    }

    @Override
    public String toString() {
        String s = null;
        switch (mType) {
            case TYPE_FIXED:
                s = mMaxWidth + "*" + mMaxHeight;
                break;
            case TYPE_RESIZE_MODIFIED_ASPECT_RATIO:
                s = mMinWidth + "*" + mMinHeight + "-" + mMaxWidth + "*" + mMaxHeight;
                break;
            case TYPE_RESIZE_FIXED_ASPECT_RATIO:
                s = mMinWidth + "**-" + mMaxWidth + "*" + mMaxHeight;
                break;
        }
        return s;
    }
}
