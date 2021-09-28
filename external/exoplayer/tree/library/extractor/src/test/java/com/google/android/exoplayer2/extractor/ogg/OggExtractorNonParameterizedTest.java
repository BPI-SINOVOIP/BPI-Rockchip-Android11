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
package com.google.android.exoplayer2.extractor.ogg;

import static com.google.android.exoplayer2.testutil.TestUtil.getByteArray;

import androidx.test.core.app.ApplicationProvider;
import androidx.test.ext.junit.runners.AndroidJUnit4;
import com.google.android.exoplayer2.testutil.ExtractorAsserts;
import com.google.android.exoplayer2.testutil.FakeExtractorInput;
import java.io.IOException;
import org.junit.Test;
import org.junit.runner.RunWith;

/**
 * Tests for {@link OggExtractor} that test specific behaviours and don't need to be parameterized.
 *
 * <p>For parameterized tests using {@link ExtractorAsserts} see {@link
 * OggExtractorParameterizedTest}.
 */
@RunWith(AndroidJUnit4.class)
public final class OggExtractorNonParameterizedTest {

  @Test
  public void sniffVorbis() throws Exception {
    byte[] data = getByteArray(ApplicationProvider.getApplicationContext(), "ogg/vorbis_header");
    assertSniff(data, /* expectedResult= */ true);
  }

  @Test
  public void sniffFlac() throws Exception {
    byte[] data = getByteArray(ApplicationProvider.getApplicationContext(), "ogg/flac_header");
    assertSniff(data, /* expectedResult= */ true);
  }

  @Test
  public void sniffFailsOpusFile() throws Exception {
    byte[] data = getByteArray(ApplicationProvider.getApplicationContext(), "ogg/opus_header");
    assertSniff(data, /* expectedResult= */ false);
  }

  @Test
  public void sniffFailsInvalidOggHeader() throws Exception {
    byte[] data =
        getByteArray(ApplicationProvider.getApplicationContext(), "ogg/invalid_ogg_header");
    assertSniff(data, /* expectedResult= */ false);
  }

  @Test
  public void sniffInvalidHeader() throws Exception {
    byte[] data = getByteArray(ApplicationProvider.getApplicationContext(), "ogg/invalid_header");
    assertSniff(data, /* expectedResult= */ false);
  }

  @Test
  public void sniffFailsEOF() throws Exception {
    byte[] data = getByteArray(ApplicationProvider.getApplicationContext(), "ogg/eof_header");
    assertSniff(data, /* expectedResult= */ false);
  }

  private void assertSniff(byte[] data, boolean expectedResult) throws IOException {
    FakeExtractorInput input =
        new FakeExtractorInput.Builder()
            .setData(data)
            .setSimulateIOErrors(true)
            .setSimulateUnknownLength(true)
            .setSimulatePartialReads(true)
            .build();
    ExtractorAsserts.assertSniff(new OggExtractor(), input, expectedResult);
  }
}
