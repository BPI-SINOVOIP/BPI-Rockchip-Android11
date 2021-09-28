/*
 * Copyright (C) 2016 The Android Open Source Project
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

public class PixelColor {
    public static final int BLACK = 0xFF000000;
    public static final int RED = 0xFF0000FF;
    public static final int GREEN = 0xFF00FF00;
    public static final int BLUE = 0xFFFF0000;
    public static final int YELLOW = 0xFF00FFFF;
    public static final int MAGENTA = 0xFFFF00FF;
    public static final int WHITE = 0xFFFFFFFF;

    public static final int TRANSPARENT_RED = 0x7F0000FF;
    public static final int TRANSPARENT_BLUE = 0x7FFF0000;
    public static final int TRANSPARENT = 0x00000000;

    // Default to black
    public short mMinAlpha;
    public short mMaxAlpha;
    public short mMinRed;
    public short mMaxRed;
    public short mMinBlue;
    public short mMaxBlue;
    public short mMinGreen;
    public short mMaxGreen;

    public PixelColor(int color) {
        short alpha = (short) ((color >> 24) & 0xFF);
        short blue = (short) ((color >> 16) & 0xFF);
        short green = (short) ((color >> 8) & 0xFF);
        short red = (short) (color & 0xFF);

        mMinAlpha = (short) getMinValue(alpha);
        mMaxAlpha = (short) getMaxValue(alpha);
        mMinRed = (short) getMinValue(red);
        mMaxRed = (short) getMaxValue(red);
        mMinBlue = (short) getMinValue(blue);
        mMaxBlue = (short) getMaxValue(blue);
        mMinGreen = (short) getMinValue(green);
        mMaxGreen = (short) getMaxValue(green);
    }

    public PixelColor() {
        this(BLACK);
    }

    private int getMinValue(short color) {
        if (color - 4 > 0) {
            return color - 4;
        }
        return 0;
    }

    private int getMaxValue(short color) {
        if (color + 4 < 0xFF) {
            return color + 4;
        }
        return 0xFF;
    }

    public void addToPixelCounter(ScriptC_PixelCounter script) {
        script.set_MIN_ALPHA(mMinAlpha);
        script.set_MAX_ALPHA(mMaxAlpha);
        script.set_MIN_RED(mMinRed);
        script.set_MAX_RED(mMaxRed);
        script.set_MIN_BLUE(mMinBlue);
        script.set_MAX_BLUE(mMaxBlue);
        script.set_MIN_GREEN(mMinGreen);
        script.set_MAX_GREEN(mMaxGreen);
    }
}
