/*
 * Copyright (C) 2020 The Android Open Source Project
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
package com.google.android.exoplayer2.source;

import static com.google.common.truth.Truth.assertThat;

import androidx.test.ext.junit.runners.AndroidJUnit4;
import com.google.android.exoplayer2.C;
import com.google.android.exoplayer2.Format;
import com.google.android.exoplayer2.FormatHolder;
import com.google.android.exoplayer2.decoder.DecoderInputBuffer;
import com.google.android.exoplayer2.source.MediaSourceEventListener.EventDispatcher;
import com.google.android.exoplayer2.testutil.FakeMediaPeriod;
import com.google.android.exoplayer2.trackselection.FixedTrackSelection;
import com.google.android.exoplayer2.trackselection.TrackSelection;
import java.util.concurrent.CountDownLatch;
import org.checkerframework.checker.nullness.compatqual.NullableType;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.LooperMode;

/** Unit test for {@link MergingMediaPeriod}. */
@RunWith(AndroidJUnit4.class)
@LooperMode(LooperMode.Mode.PAUSED)
public final class MergingMediaPeriodTest {

  private static final Format childFormat11 = new Format.Builder().setId("1_1").build();
  private static final Format childFormat12 = new Format.Builder().setId("1_2").build();
  private static final Format childFormat21 = new Format.Builder().setId("2_1").build();
  private static final Format childFormat22 = new Format.Builder().setId("2_2").build();

  @Test
  public void getTrackGroups_returnsAllChildTrackGroups() throws Exception {
    MergingMediaPeriod mergingMediaPeriod =
        prepareMergingPeriod(
            new MergingPeriodDefinition(/* timeOffsetUs= */ 0, childFormat11, childFormat12),
            new MergingPeriodDefinition(/* timeOffsetUs= */ 0, childFormat21, childFormat22));

    assertThat(mergingMediaPeriod.getTrackGroups().length).isEqualTo(4);
    assertThat(mergingMediaPeriod.getTrackGroups().get(0).getFormat(0)).isEqualTo(childFormat11);
    assertThat(mergingMediaPeriod.getTrackGroups().get(1).getFormat(0)).isEqualTo(childFormat12);
    assertThat(mergingMediaPeriod.getTrackGroups().get(2).getFormat(0)).isEqualTo(childFormat21);
    assertThat(mergingMediaPeriod.getTrackGroups().get(3).getFormat(0)).isEqualTo(childFormat22);
  }

  @Test
  public void selectTracks_createsSampleStreamsFromChildPeriods() throws Exception {
    MergingMediaPeriod mergingMediaPeriod =
        prepareMergingPeriod(
            new MergingPeriodDefinition(/* timeOffsetUs= */ 0, childFormat11, childFormat12),
            new MergingPeriodDefinition(/* timeOffsetUs= */ 0, childFormat21, childFormat22));

    TrackSelection selectionForChild1 =
        new FixedTrackSelection(mergingMediaPeriod.getTrackGroups().get(1), /* track= */ 0);
    TrackSelection selectionForChild2 =
        new FixedTrackSelection(mergingMediaPeriod.getTrackGroups().get(2), /* track= */ 0);
    SampleStream[] streams = new SampleStream[4];
    mergingMediaPeriod.selectTracks(
        /* selections= */ new TrackSelection[] {null, selectionForChild1, selectionForChild2, null},
        /* mayRetainStreamFlags= */ new boolean[] {false, false, false, false},
        streams,
        /* streamResetFlags= */ new boolean[] {false, false, false, false},
        /* positionUs= */ 0);

    assertThat(streams[0]).isNull();
    assertThat(streams[3]).isNull();

    FormatHolder formatHolder = new FormatHolder();
    DecoderInputBuffer inputBuffer =
        new DecoderInputBuffer(DecoderInputBuffer.BUFFER_REPLACEMENT_MODE_NORMAL);
    assertThat(streams[1].readData(formatHolder, inputBuffer, /* formatRequired= */ true))
        .isEqualTo(C.RESULT_FORMAT_READ);
    assertThat(formatHolder.format).isEqualTo(childFormat12);

    assertThat(streams[2].readData(formatHolder, inputBuffer, /* formatRequired= */ true))
        .isEqualTo(C.RESULT_FORMAT_READ);
    assertThat(formatHolder.format).isEqualTo(childFormat21);
  }

  @Test
  public void
      selectTracks_withPeriodOffsets_selectTracksWithOffset_andCreatesSampleStreamsCorrectingOffset()
          throws Exception {
    MergingMediaPeriod mergingMediaPeriod =
        prepareMergingPeriod(
            new MergingPeriodDefinition(/* timeOffsetUs= */ 0, childFormat11, childFormat12),
            new MergingPeriodDefinition(/* timeOffsetUs= */ -3000, childFormat21, childFormat22));

    TrackSelection selectionForChild1 =
        new FixedTrackSelection(mergingMediaPeriod.getTrackGroups().get(0), /* track= */ 0);
    TrackSelection selectionForChild2 =
        new FixedTrackSelection(mergingMediaPeriod.getTrackGroups().get(2), /* track= */ 0);
    SampleStream[] streams = new SampleStream[2];
    mergingMediaPeriod.selectTracks(
        /* selections= */ new TrackSelection[] {selectionForChild1, selectionForChild2},
        /* mayRetainStreamFlags= */ new boolean[] {false, false},
        streams,
        /* streamResetFlags= */ new boolean[] {false, false},
        /* positionUs= */ 0);
    FormatHolder formatHolder = new FormatHolder();
    DecoderInputBuffer inputBuffer =
        new DecoderInputBuffer(DecoderInputBuffer.BUFFER_REPLACEMENT_MODE_NORMAL);
    streams[0].readData(formatHolder, inputBuffer, /* formatRequired= */ true);
    streams[1].readData(formatHolder, inputBuffer, /* formatRequired= */ true);

    FakeMediaPeriodWithSelectTracksPosition childMediaPeriod1 =
        (FakeMediaPeriodWithSelectTracksPosition) mergingMediaPeriod.getChildPeriod(0);
    assertThat(childMediaPeriod1.selectTracksPositionUs).isEqualTo(0);
    assertThat(streams[0].readData(formatHolder, inputBuffer, /* formatRequired= */ false))
        .isEqualTo(C.RESULT_BUFFER_READ);
    assertThat(inputBuffer.timeUs).isEqualTo(0L);

    FakeMediaPeriodWithSelectTracksPosition childMediaPeriod2 =
        (FakeMediaPeriodWithSelectTracksPosition) mergingMediaPeriod.getChildPeriod(1);
    assertThat(childMediaPeriod2.selectTracksPositionUs).isEqualTo(3000L);
    assertThat(streams[1].readData(formatHolder, inputBuffer, /* formatRequired= */ false))
        .isEqualTo(C.RESULT_BUFFER_READ);
    assertThat(inputBuffer.timeUs).isEqualTo(0L);
  }

  private MergingMediaPeriod prepareMergingPeriod(MergingPeriodDefinition... definitions)
      throws Exception {
    MediaPeriod[] mediaPeriods = new MediaPeriod[definitions.length];
    long[] timeOffsetsUs = new long[definitions.length];
    for (int i = 0; i < definitions.length; i++) {
      timeOffsetsUs[i] = definitions[i].timeOffsetUs;
      TrackGroup[] trackGroups = new TrackGroup[definitions[i].formats.length];
      for (int j = 0; j < definitions[i].formats.length; j++) {
        trackGroups[j] = new TrackGroup(definitions[i].formats[j]);
      }
      mediaPeriods[i] =
          new FakeMediaPeriodWithSelectTracksPosition(
              new TrackGroupArray(trackGroups), new EventDispatcher());
    }
    MergingMediaPeriod mergingMediaPeriod =
        new MergingMediaPeriod(
            new DefaultCompositeSequenceableLoaderFactory(), timeOffsetsUs, mediaPeriods);

    CountDownLatch prepareCountDown = new CountDownLatch(1);
    mergingMediaPeriod.prepare(
        new MediaPeriod.Callback() {
          @Override
          public void onPrepared(MediaPeriod mediaPeriod) {
            prepareCountDown.countDown();
          }

          @Override
          public void onContinueLoadingRequested(MediaPeriod source) {
            mergingMediaPeriod.continueLoading(/* positionUs= */ 0);
          }
        },
        /* positionUs= */ 0);
    prepareCountDown.await();

    return mergingMediaPeriod;
  }

  private static final class FakeMediaPeriodWithSelectTracksPosition extends FakeMediaPeriod {

    public long selectTracksPositionUs;

    public FakeMediaPeriodWithSelectTracksPosition(
        TrackGroupArray trackGroupArray, EventDispatcher eventDispatcher) {
      super(trackGroupArray, eventDispatcher);
      selectTracksPositionUs = C.TIME_UNSET;
    }

    @Override
    public long selectTracks(
        @NullableType TrackSelection[] selections,
        boolean[] mayRetainStreamFlags,
        @NullableType SampleStream[] streams,
        boolean[] streamResetFlags,
        long positionUs) {
      selectTracksPositionUs = positionUs;
      return super.selectTracks(
          selections, mayRetainStreamFlags, streams, streamResetFlags, positionUs);
    }
  }

  private static final class MergingPeriodDefinition {

    public long timeOffsetUs;
    public Format[] formats;

    public MergingPeriodDefinition(long timeOffsetUs, Format... formats) {
      this.timeOffsetUs = timeOffsetUs;
      this.formats = formats;
    }
  }
}
