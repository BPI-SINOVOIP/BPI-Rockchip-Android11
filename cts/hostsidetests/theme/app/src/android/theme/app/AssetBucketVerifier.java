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
 * limitations under the License
 */

package android.theme.app;

import android.content.Context;

import java.io.IOException;
import java.util.ArrayList;
import java.util.List;

class AssetBucketVerifier {
    /** Asset file to verify. */
    private static final String ASSET_NAME = "ic_star_black_16dp.png";

    /** Densities at which {@link #ASSET_NAME} may be defined. */
    private static final int[] DENSITIES_DPI = new int[] {
            160, // mdpi
            240, // hdpi
            320, // xhdpi
            480, // xxhdpi
            640, // xxxhdpi
    };

    /** Bucket names corresponding to {@link #DENSITIES_DPI} entries. */
    private static final String[] DENSITIES_NAME = new String[] {
            "mdpi",
            "hdpi",
            "xhdpi",
            "xxhdpi",
            "xxxhdpi"
    };

    static class Result {
        String expectedAtDensity;
        List<String> foundAtDensity;
    }

    static Result verifyAssetBucket(Context context) {
        List<String> foundAtDensity = new ArrayList<>();
        String expectedAtDensity = null;

        int deviceDensityDpi = context.getResources().getConfiguration().densityDpi;
        for (int i = 0; i < DENSITIES_DPI.length; i++) {
            // Find the matching or next-highest density bucket.
            if (expectedAtDensity == null && DENSITIES_DPI[i] >= deviceDensityDpi) {
                expectedAtDensity = DENSITIES_NAME[i];
            }

            // Try to load and close the asset from the current density.
            try {
                context.getAssets().openNonAssetFd(1,
                        "res/drawable-" + DENSITIES_NAME[i] + "-v4/" + ASSET_NAME).close();
                foundAtDensity.add(DENSITIES_NAME[i]);
            } catch (IOException e) {
                e.printStackTrace();
            }
        }

        if (expectedAtDensity == null) {
            expectedAtDensity = DENSITIES_NAME[DENSITIES_NAME.length - 1];
        }

        Result result = new Result();
        result.expectedAtDensity = expectedAtDensity;
        result.foundAtDensity = foundAtDensity;
        return result;
    }
}
