/*
 * Copyright 2020 The Android Open Source Project
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
package android.graphics.cts;

import static org.junit.Assert.assertNotNull;

import android.graphics.ColorSpace;
import android.graphics.ColorSpace.Named;

/**
 *  Helper class for ADataSpace.
 */
final class DataSpace {
    private DataSpace() {
    }

    // These match the values in data_space.h
    public static final int ADATASPACE_UNKNOWN = 0;
    public static final int ADATASPACE_SRGB = 142671872;
    public static final int ADATASPACE_SCRGB = 411107328;
    public static final int ADATASPACE_SRGB_LINEAR = 138477568;
    public static final int ADATASPACE_SCRGB_LINEAR = 406913024;
    public static final int ADATASPACE_DISPLAY_P3 = 143261696;
    public static final int ADATASPACE_BT2020 = 147193856;
    public static final int ADATASPACE_ADOBE_RGB = 151715840;
    public static final int ADATASPACE_DCI_P3 = 155844608;
    public static final int ADATASPACE_BT709 = 281083904;

    public static int fromColorSpace(ColorSpace cs) {
        assertNotNull(cs);
        if (cs == ColorSpace.get(Named.DISPLAY_P3)) {
            return ADATASPACE_DISPLAY_P3;
        }
        if (cs == ColorSpace.get(Named.BT2020)) {
            return ADATASPACE_BT2020;
        }
        if (cs == ColorSpace.get(Named.ADOBE_RGB)) {
            return ADATASPACE_ADOBE_RGB;
        }
        if (cs == ColorSpace.get(Named.BT709)) {
            return ADATASPACE_BT709;
        }
        if (cs == ColorSpace.get(Named.DCI_P3)) {
            return ADATASPACE_DCI_P3;
        }

        if (cs == ColorSpace.get(Named.SRGB)) {
            return ADATASPACE_SRGB;
        }
        if (cs == ColorSpace.get(Named.EXTENDED_SRGB)) {
            return ADATASPACE_SCRGB;
        }

        if (cs == ColorSpace.get(Named.LINEAR_SRGB)) {
            return ADATASPACE_SRGB_LINEAR;
        }
        if (cs == ColorSpace.get(Named.LINEAR_EXTENDED_SRGB)) {
            return ADATASPACE_SCRGB_LINEAR;
        }

        return ADATASPACE_UNKNOWN;
    }
}
