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
package com.google.android.exoplayer2.extractor.amr;

import com.google.android.exoplayer2.testutil.ExtractorAsserts;
import java.util.List;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.ParameterizedRobolectricTestRunner;
import org.robolectric.ParameterizedRobolectricTestRunner.Parameter;
import org.robolectric.ParameterizedRobolectricTestRunner.Parameters;

/**
 * Unit tests for {@link AmrExtractor} that use parameterization to test a range of behaviours.
 *
 * <p>For non-parameterized tests see {@link AmrExtractorSeekTest} and {@link
 * AmrExtractorNonParameterizedTest}.
 */
@RunWith(ParameterizedRobolectricTestRunner.class)
public final class AmrExtractorParameterizedTest {

  @Parameters(name = "{0}")
  public static List<Object[]> params() {
    return ExtractorAsserts.configs();
  }

  @Parameter(0)
  public ExtractorAsserts.Config assertionConfig;

  @Test
  public void extractingNarrowBandSamples() throws Exception {
    ExtractorAsserts.assertBehavior(
        createAmrExtractorFactory(/* withSeeking= */ false), "amr/sample_nb.amr", assertionConfig);
  }

  @Test
  public void extractingWideBandSamples() throws Exception {
    ExtractorAsserts.assertBehavior(
        createAmrExtractorFactory(/* withSeeking= */ false), "amr/sample_wb.amr", assertionConfig);
  }

  @Test
  public void extractingNarrowBandSamples_withSeeking() throws Exception {
    ExtractorAsserts.assertBehavior(
        createAmrExtractorFactory(/* withSeeking= */ true),
        "amr/sample_nb_cbr.amr",
        assertionConfig);
  }

  @Test
  public void extractingWideBandSamples_withSeeking() throws Exception {
    ExtractorAsserts.assertBehavior(
        createAmrExtractorFactory(/* withSeeking= */ true),
        "amr/sample_wb_cbr.amr",
        assertionConfig);
  }


  private static ExtractorAsserts.ExtractorFactory createAmrExtractorFactory(boolean withSeeking) {
    return () -> {
      if (!withSeeking) {
        return new AmrExtractor();
      } else {
        return new AmrExtractor(AmrExtractor.FLAG_ENABLE_CONSTANT_BITRATE_SEEKING);
      }
    };
  }
}
