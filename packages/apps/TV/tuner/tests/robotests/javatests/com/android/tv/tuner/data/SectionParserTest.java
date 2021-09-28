/*
 * Copyright (C) 2017 The Android Open Source Project
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

package com.android.tv.tuner.data;

import static com.google.common.truth.Truth.assertThat;
import static com.google.common.truth.Truth.assertWithMessage;

import com.android.tv.testing.constants.ConfigConstants;
import com.android.tv.tuner.data.PsipData.ContentAdvisoryDescriptor;
import com.android.tv.tuner.data.PsipData.RatingRegion;
import com.android.tv.tuner.data.PsipData.RegionalRating;
import com.android.tv.tuner.data.PsipData.TsDescriptor;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.RobolectricTestRunner;
import org.robolectric.annotation.Config;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collections;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

/** Tests for {@link com.android.tv.tuner.data.SectionParser}. */
@RunWith(RobolectricTestRunner.class)
@Config(sdk = ConfigConstants.SDK)
public class SectionParserTest {
    private static final Map<String, String> US_RATING_MAP = new HashMap<>();
    private static final int RATING_REGION_US = 1;

    static {
        // These mappings are from table 3 of ANSI-CEA-766-D
        US_RATING_MAP.put("1 0 0 0 0 0 0 X", ""); // TV-None
        US_RATING_MAP.put("0 0 0 0 0 1 0 X", "com.android.tv/US_TV/US_TV_Y");
        US_RATING_MAP.put("0 0 0 0 0 2 0 X", "com.android.tv/US_TV/US_TV_Y7");
        US_RATING_MAP.put("0 0 0 0 0 2 1 X", "com.android.tv/US_TV/US_TV_Y7/US_TV_FV");
        US_RATING_MAP.put("2 0 0 0 0 0 0 X", "com.android.tv/US_TV/US_TV_G");
        US_RATING_MAP.put("3 0 0 0 0 0 0 X", "com.android.tv/US_TV/US_TV_PG");
        US_RATING_MAP.put("3 1 0 0 0 0 0 X", "com.android.tv/US_TV/US_TV_PG/US_TV_D");
        US_RATING_MAP.put("3 0 1 0 0 0 0 X", "com.android.tv/US_TV/US_TV_PG/US_TV_L");
        US_RATING_MAP.put("3 0 0 1 0 0 0 X", "com.android.tv/US_TV/US_TV_PG/US_TV_S");
        US_RATING_MAP.put("3 0 0 0 1 0 0 X", "com.android.tv/US_TV/US_TV_PG/US_TV_V");
        US_RATING_MAP.put("3 1 1 0 0 0 0 X", "com.android.tv/US_TV/US_TV_PG/US_TV_D/US_TV_L");
        US_RATING_MAP.put("3 1 0 1 0 0 0 X", "com.android.tv/US_TV/US_TV_PG/US_TV_D/US_TV_S");
        US_RATING_MAP.put("3 1 0 0 1 0 0 X", "com.android.tv/US_TV/US_TV_PG/US_TV_D/US_TV_V");
        US_RATING_MAP.put("3 0 1 1 0 0 0 X", "com.android.tv/US_TV/US_TV_PG/US_TV_L/US_TV_S");
        US_RATING_MAP.put("3 0 1 0 1 0 0 X", "com.android.tv/US_TV/US_TV_PG/US_TV_L/US_TV_V");
        US_RATING_MAP.put("3 0 0 1 1 0 0 X", "com.android.tv/US_TV/US_TV_PG/US_TV_S/US_TV_V");
        US_RATING_MAP.put(
                "3 1 1 1 0 0 0 X", "com.android.tv/US_TV/US_TV_PG/US_TV_D/US_TV_L/US_TV_S");
        US_RATING_MAP.put(
                "3 1 1 0 1 0 0 X", "com.android.tv/US_TV/US_TV_PG/US_TV_D/US_TV_L/US_TV_V");
        US_RATING_MAP.put(
                "3 1 0 1 1 0 0 X", "com.android.tv/US_TV/US_TV_PG/US_TV_D/US_TV_S/US_TV_V");
        US_RATING_MAP.put(
                "3 0 1 1 1 0 0 X", "com.android.tv/US_TV/US_TV_PG/US_TV_L/US_TV_S/US_TV_V");
        US_RATING_MAP.put(
                "3 1 1 1 1 0 0 X", "com.android.tv/US_TV/US_TV_PG/US_TV_D/US_TV_L/US_TV_S/US_TV_V");
        US_RATING_MAP.put("4 0 0 0 0 0 0 X", "com.android.tv/US_TV/US_TV_14");
        US_RATING_MAP.put("4 1 0 0 0 0 0 X", "com.android.tv/US_TV/US_TV_14/US_TV_D");
        US_RATING_MAP.put("4 0 1 0 0 0 0 X", "com.android.tv/US_TV/US_TV_14/US_TV_L");
        US_RATING_MAP.put("4 0 0 1 0 0 0 X", "com.android.tv/US_TV/US_TV_14/US_TV_S");
        US_RATING_MAP.put("4 0 0 0 1 0 0 X", "com.android.tv/US_TV/US_TV_14/US_TV_V");
        US_RATING_MAP.put("4 1 1 0 0 0 0 X", "com.android.tv/US_TV/US_TV_14/US_TV_D/US_TV_L");
        US_RATING_MAP.put("4 1 0 1 0 0 0 X", "com.android.tv/US_TV/US_TV_14/US_TV_D/US_TV_S");
        US_RATING_MAP.put("4 1 0 0 1 0 0 X", "com.android.tv/US_TV/US_TV_14/US_TV_D/US_TV_V");
        US_RATING_MAP.put("4 0 1 1 0 0 0 X", "com.android.tv/US_TV/US_TV_14/US_TV_L/US_TV_S");
        US_RATING_MAP.put("4 0 1 0 1 0 0 X", "com.android.tv/US_TV/US_TV_14/US_TV_L/US_TV_V");
        US_RATING_MAP.put("4 0 0 1 1 0 0 X", "com.android.tv/US_TV/US_TV_14/US_TV_S/US_TV_V");
        US_RATING_MAP.put(
                "4 1 1 1 0 0 0 X", "com.android.tv/US_TV/US_TV_14/US_TV_D/US_TV_L/US_TV_S");
        US_RATING_MAP.put(
                "4 1 1 0 1 0 0 X", "com.android.tv/US_TV/US_TV_14/US_TV_D/US_TV_L/US_TV_V");
        US_RATING_MAP.put(
                "4 1 0 1 1 0 0 X", "com.android.tv/US_TV/US_TV_14/US_TV_D/US_TV_S/US_TV_V");
        US_RATING_MAP.put(
                "4 0 1 1 1 0 0 X", "com.android.tv/US_TV/US_TV_14/US_TV_L/US_TV_S/US_TV_V");
        US_RATING_MAP.put(
                "4 1 1 1 1 0 0 X", "com.android.tv/US_TV/US_TV_14/US_TV_D/US_TV_L/US_TV_S/US_TV_V");
        US_RATING_MAP.put("5 0 0 0 0 0 0 X", "com.android.tv/US_TV/US_TV_MA");
        US_RATING_MAP.put("5 0 1 0 0 0 0 X", "com.android.tv/US_TV/US_TV_MA/US_TV_L");
        US_RATING_MAP.put("5 0 0 1 0 0 0 X", "com.android.tv/US_TV/US_TV_MA/US_TV_S");
        US_RATING_MAP.put("5 0 0 0 1 0 0 X", "com.android.tv/US_TV/US_TV_MA/US_TV_V");
        US_RATING_MAP.put("5 0 1 1 0 0 0 X", "com.android.tv/US_TV/US_TV_MA/US_TV_L/US_TV_S");
        US_RATING_MAP.put("5 0 1 0 1 0 0 X", "com.android.tv/US_TV/US_TV_MA/US_TV_L/US_TV_V");
        US_RATING_MAP.put("5 0 0 1 1 0 0 X", "com.android.tv/US_TV/US_TV_MA/US_TV_S/US_TV_V");
        US_RATING_MAP.put(
                "5 0 1 1 1 0 0 X", "com.android.tv/US_TV/US_TV_MA/US_TV_L/US_TV_S/US_TV_V");
        US_RATING_MAP.put("X X X X X X X 1", ""); // MPAA-N/A
        US_RATING_MAP.put("X X X X X X X 2", "com.android.tv/US_MV/US_MV_G");
        US_RATING_MAP.put("X X X X X X X 3", "com.android.tv/US_MV/US_MV_PG");
        US_RATING_MAP.put("X X X X X X X 4", "com.android.tv/US_MV/US_MV_PG13");
        US_RATING_MAP.put("X X X X X X X 5", "com.android.tv/US_MV/US_MV_R");
        US_RATING_MAP.put("X X X X X X X 6", "com.android.tv/US_MV/US_MV_NC17");
        // MPAA-X was replaced by NC17
        US_RATING_MAP.put("X X X X X X X 7", "com.android.tv/US_MV/US_MV_NC17");
        US_RATING_MAP.put("X X X X X X X 8", ""); // MPAA - Not Rated
    }

    @Test
    public void testGenerateContentRating_emptyInput() {
        assertThat(SectionParser.generateContentRating(new ArrayList<TsDescriptor>())).isEmpty();
    }

    @Test
    public void testGenerateContentRating_validInputs() {
        for (Map.Entry<String, String> entry : US_RATING_MAP.entrySet()) {
            RatingRegion ratingRegion = createRatingRegionForTest(entry.getKey(), RATING_REGION_US);
            ContentAdvisoryDescriptor descriptor = createDescriptorForTest(ratingRegion);
            assertWithMessage("key = " + entry.getKey())
                    .that(
                            SectionParser.generateContentRating(
                                    Collections.singletonList((TsDescriptor) descriptor)))
                    .isEqualTo(entry.getValue());
        }
    }

    @Test
    public void testGenerateContentRating_invalidInput() {
        // Invalid because the value of the first dimension is lost.
        RatingRegion ratingRegion = createRatingRegionForTest("X 1 0 0 0 0 0 X", RATING_REGION_US);
        ContentAdvisoryDescriptor descriptor = createDescriptorForTest(ratingRegion);
        assertThat(
                        SectionParser.generateContentRating(
                                Collections.singletonList((TsDescriptor) descriptor)))
                .isEmpty();
    }

    @Test
    public void testGenerateContentRating_multipleRatings() {
        // TV-MA
        RatingRegion ratingRegionTv =
                createRatingRegionForTest("5 0 0 0 0 0 0 X", RATING_REGION_US);
        // MPAA-R
        RatingRegion ratingRegionMv =
                createRatingRegionForTest("X X X X X X X 5", RATING_REGION_US);
        ContentAdvisoryDescriptor descriptorTv = createDescriptorForTest(ratingRegionTv);
        ContentAdvisoryDescriptor descriptorMv = createDescriptorForTest(ratingRegionMv);
        assertThat(
                        SectionParser.generateContentRating(
                                Arrays.<TsDescriptor>asList(descriptorTv, descriptorMv)))
                .isEqualTo("com.android.tv/US_MV/US_MV_R,com.android.tv/US_TV/US_TV_MA");
    }

    private static RatingRegion createRatingRegionForTest(String values, int region) {
        String[] valueArray = values.split(" ");
        List<RegionalRating> regionalRatings = new ArrayList<>();
        for (int i = 0; i < valueArray.length; i++) {
            try {
                int value = Integer.valueOf(valueArray[i]);
                if (value != 0) {
                    // value 0 means the dimension should be omitted from the descriptor
                    regionalRatings.add(new RegionalRating(i, value));
                }
            } catch (NumberFormatException e) {
                // do nothing
            }
        }
        return new RatingRegion(region, "", regionalRatings);
    }

    private static ContentAdvisoryDescriptor createDescriptorForTest(RatingRegion... regions) {
        return new ContentAdvisoryDescriptor(Arrays.asList(regions));
    }
}
