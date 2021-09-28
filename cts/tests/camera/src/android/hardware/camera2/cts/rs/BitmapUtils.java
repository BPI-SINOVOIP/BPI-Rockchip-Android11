/*
 * Copyright 2015 The Android Open Source Project
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
package android.hardware.camera2.cts.rs;

import android.graphics.Bitmap;
import android.graphics.Color;
import android.renderscript.Allocation;
import android.renderscript.Element;
import android.renderscript.RenderScript;
import android.renderscript.ScriptIntrinsicHistogram;

/**
 * Utility class providing methods for various pixel-wise ARGB bitmap operations.
 */
public class BitmapUtils {
    private static final String TAG = "BitmapUtils";
    private static final int COLOR_BIT_DEPTH = 256;
    public static int NUM_CHANNELS = 4;

    /**
     * Return the histograms for each color channel (interleaved).
     *
     * @param rs a {@link RenderScript} context to use.
     * @param bmap a {@link Bitmap} to generate the histograms for.
     * @return an array containing NUM_CHANNELS * COLOR_BIT_DEPTH histogram bucket values, with
     * the color channels interleaved.
     */
    public static int[] calcHistograms(RenderScript rs, Bitmap bmap) {
        ScriptIntrinsicHistogram hist = ScriptIntrinsicHistogram.create(rs, Element.U8_4(rs));
        Allocation sums = Allocation.createSized(rs, Element.I32_4(rs), COLOR_BIT_DEPTH);

        // Setup input allocation (ARGB 8888 bitmap).
        Allocation input = Allocation.createFromBitmap(rs, bmap);

        hist.setOutput(sums);
        hist.forEach(input);
        int[] output = new int[COLOR_BIT_DEPTH * NUM_CHANNELS];
        sums.copyTo(output);
        return output;
    }

    // Some stats output from comparing two Bitmap using calcDifferenceMetric
    public static class BitmapCompareResult {
        // difference between two bitmaps using average of per-pixel differences.
        public double mDiff;
        // If the LHS Bitmap has same RGB values for all pixels
        public boolean mLhsFlat;
        // If the RHS Bitmap has same RGB values for all pixels
        public boolean mRhsFlat;
        // The R/G/B average pixel value of LHS Bitmap
        public double[] mLhsAverage = new double[3];
        // The R/G/B average pixel value of RHS Bitmap
        public double[] mRhsAverage = new double[3];
    }

    /**
     * Compare two bitmaps and also return some statistics about the two Bitmap.
     *
     * @param a first {@link android.graphics.Bitmap}.
     * @param b second {@link android.graphics.Bitmap}.
     * @return the results in a BitmapCompareResult
     */
    public static BitmapCompareResult compareBitmap(Bitmap a, Bitmap b) {
        if (a.getWidth() != b.getWidth() || a.getHeight() != b.getHeight()) {
            throw new IllegalArgumentException("Bitmap dimensions for arguments do not match a=" +
                    a.getWidth() + "x" + a.getHeight() + ", b=" + b.getWidth() + "x" +
                    b.getHeight());
        }
        // TODO: Optimize this in renderscript to avoid copy.
        int[] aPixels = new int[a.getHeight() * a.getWidth()];
        int[] bPixels = new int[aPixels.length];
        a.getPixels(aPixels, /*offset*/0, /*stride*/a.getWidth(), /*x*/0, /*y*/0, a.getWidth(),
                a.getHeight());
        b.getPixels(bPixels, /*offset*/0, /*stride*/b.getWidth(), /*x*/0, /*y*/0, b.getWidth(),
                b.getHeight());
        double diff = 0;
        double[] aSum = new double[3];
        double[] bSum = new double[3];
        int[] aFirstPix = new int[3];
        int[] bFirstPix = new int[3];
        aFirstPix[0] = Color.red(aPixels[0]);
        aFirstPix[1] = Color.green(aPixels[1]);
        aFirstPix[2] = Color.blue(aPixels[2]);
        bFirstPix[0] = Color.red(bPixels[0]);
        bFirstPix[1] = Color.green(bPixels[1]);
        bFirstPix[2] = Color.blue(bPixels[2]);
        boolean isAFlat = true, isBFlat = true;

        for (int i = 0; i < aPixels.length; i++) {
            int aPix = aPixels[i];
            int bPix = bPixels[i];
            int aR = Color.red(aPix);
            int aG = Color.green(aPix);
            int aB = Color.blue(aPix);
            int bR = Color.red(bPix);
            int bG = Color.green(bPix);
            int bB = Color.blue(bPix);
            aSum[0] += aR;
            aSum[1] += aG;
            aSum[2] += aB;
            bSum[0] += bR;
            bSum[1] += bG;
            bSum[2] += bB;

            if (isAFlat && (aR != aFirstPix[0] || aG != aFirstPix[1] || aB != aFirstPix[2])) {
                isAFlat = false;
            }

            if (isBFlat && (bR != bFirstPix[0] || bG != bFirstPix[1] || bB != bFirstPix[2])) {
                isBFlat = false;
            }

            diff += Math.abs(aR - bR); // red
            diff += Math.abs(aG - bG); // green
            diff += Math.abs(aB - bB); // blue
        }
        diff /= (aPixels.length * 3);
        BitmapCompareResult result = new BitmapCompareResult();
        result.mDiff = diff;
        result.mLhsFlat = isAFlat;
        result.mRhsFlat = isBFlat;
        for (int i = 0; i < 3; i++) {
            result.mLhsAverage[i] = aSum[i] / aPixels.length;
            result.mRhsAverage[i] = bSum[i] / bPixels.length;
        }
        return result;
    }

    /**
     * Find the difference between two bitmaps using average of per-pixel differences.
     *
     * @param a first {@link android.graphics.Bitmap}.
     * @param b second {@link android.graphics.Bitmap}.
     * @return the difference.
     */
    public static double calcDifferenceMetric(Bitmap a, Bitmap b) {
        BitmapCompareResult result = compareBitmap(a, b);
        return result.mDiff;
    }

}
