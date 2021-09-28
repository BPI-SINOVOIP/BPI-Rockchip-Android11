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
package android.view.cts.surfacevalidator;

import android.graphics.Rect;
import android.media.Image;
import android.os.Trace;

import java.nio.ByteBuffer;
import java.util.ArrayList;


public class RectChecker extends PixelChecker {
    private ArrayList<Target> mTargets = new ArrayList<Target>();

    private static final int PIXEL_STRIDE = 4;
    
    public static class Target {
        PixelColor mPixelColor;
        Rect mTargetRect;
        
        public Target(Rect r, int p) {
            mPixelColor = new PixelColor(p);
            mTargetRect = r;
        }
    };

    public RectChecker(Target... targets) {
        for (Target t : targets) {
            mTargets.add(t);
        }
    }

    public RectChecker(Rect r, int p) {
        this(new Target(r, p));
    }

    public boolean validatePlane(Image.Plane plane, Rect boundsToCheck,
            int width, int height) {
        for (Target t : mTargets) {
            if (validatePlaneForTarget(t, plane, boundsToCheck, width, height) == false) {
                return false;
            }
        }
        return true;
    }

    public boolean validatePlaneForTarget(Target t, Image.Plane plane, Rect boundsToCheck,
            int width, int height) {
        int rowStride = plane.getRowStride();
        ByteBuffer buffer = plane.getBuffer();

        Trace.beginSection("check");

        int startY = boundsToCheck.top + t.mTargetRect.top;
        int endY = t.mTargetRect.bottom;
        int startX = (boundsToCheck.left + t.mTargetRect.left) * PIXEL_STRIDE;
        int bytesWidth = t.mTargetRect.width() * PIXEL_STRIDE;

        final short maxAlpha = t.mPixelColor.mMaxAlpha;
        final short minAlpha = t.mPixelColor.mMinAlpha;
        final short maxRed = t.mPixelColor.mMaxRed;
        final short minRed = t.mPixelColor.mMinRed;
        final short maxGreen = t.mPixelColor.mMaxGreen;
        final short minGreen = t.mPixelColor.mMinGreen;
        final short maxBlue = t.mPixelColor.mMaxBlue;
        final short minBlue = t.mPixelColor.mMinBlue;

        byte[] scanline = new byte[bytesWidth];
        for (int row = startY; row < endY; row++) {
            buffer.position(rowStride * row + startX);
            buffer.get(scanline, 0, scanline.length);
            for (int i = 0; i < bytesWidth; i += PIXEL_STRIDE) {
                // Format is RGBA_8888 not ARGB_8888
                final int red = scanline[i + 0] & 0xFF;
                final int green = scanline[i + 1] & 0xFF;
                final int blue = scanline[i + 2] & 0xFF;
                final int alpha = scanline[i + 3] & 0xFF;

                if (alpha <= maxAlpha
                        && alpha >= minAlpha
                        && red <= maxRed
                        && red >= minRed
                        && green <= maxGreen
                        && green >= minGreen
                        && blue <= maxBlue
                        && blue >= minBlue) {
                    continue;
                } else {
                    return false;
                }
            }
        }
        Trace.endSection();

        return true;
    }

    public String getLastError() {
        return "(couldn't find target Rect)";
    }

    public boolean checkPixels(int matchingPixelCount, int width, int height) {
        return false;
    }
}
