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
package com.google.android.exoplayer2;

import static com.google.common.truth.Truth.assertThat;

import androidx.test.ext.junit.runners.AndroidJUnit4;
import com.google.android.exoplayer2.testutil.FakeTimeline;
import com.google.android.exoplayer2.testutil.FakeTimeline.TimelineWindowDefinition;
import com.google.android.exoplayer2.testutil.TimelineAsserts;
import org.junit.Test;
import org.junit.runner.RunWith;

/** Unit test for {@link Timeline}. */
@RunWith(AndroidJUnit4.class)
public class TimelineTest {

  @Test
  public void emptyTimeline() {
    TimelineAsserts.assertEmpty(Timeline.EMPTY);
  }

  @Test
  public void singlePeriodTimeline() {
    Timeline timeline = new FakeTimeline(new TimelineWindowDefinition(1, 111));
    TimelineAsserts.assertWindowTags(timeline, 111);
    TimelineAsserts.assertPeriodCounts(timeline, 1);
    TimelineAsserts.assertPreviousWindowIndices(
        timeline, Player.REPEAT_MODE_OFF, false, C.INDEX_UNSET);
    TimelineAsserts.assertPreviousWindowIndices(timeline, Player.REPEAT_MODE_ONE, false, 0);
    TimelineAsserts.assertPreviousWindowIndices(timeline, Player.REPEAT_MODE_ALL, false, 0);
    TimelineAsserts.assertNextWindowIndices(timeline, Player.REPEAT_MODE_OFF, false, C.INDEX_UNSET);
    TimelineAsserts.assertNextWindowIndices(timeline, Player.REPEAT_MODE_ONE, false, 0);
    TimelineAsserts.assertNextWindowIndices(timeline, Player.REPEAT_MODE_ALL, false, 0);
  }

  @Test
  public void multiPeriodTimeline() {
    Timeline timeline = new FakeTimeline(new TimelineWindowDefinition(5, 111));
    TimelineAsserts.assertWindowTags(timeline, 111);
    TimelineAsserts.assertPeriodCounts(timeline, 5);
    TimelineAsserts.assertPreviousWindowIndices(
        timeline, Player.REPEAT_MODE_OFF, false, C.INDEX_UNSET);
    TimelineAsserts.assertPreviousWindowIndices(timeline, Player.REPEAT_MODE_ONE, false, 0);
    TimelineAsserts.assertPreviousWindowIndices(timeline, Player.REPEAT_MODE_ALL, false, 0);
    TimelineAsserts.assertNextWindowIndices(timeline, Player.REPEAT_MODE_OFF, false, C.INDEX_UNSET);
    TimelineAsserts.assertNextWindowIndices(timeline, Player.REPEAT_MODE_ONE, false, 0);
    TimelineAsserts.assertNextWindowIndices(timeline, Player.REPEAT_MODE_ALL, false, 0);
  }

  @Test
  public void windowEquals() {
    Timeline.Window window = new Timeline.Window();
    assertThat(window).isEqualTo(new Timeline.Window());

    Timeline.Window otherWindow = new Timeline.Window();
    otherWindow.tag = new Object();
    assertThat(window).isNotEqualTo(otherWindow);

    otherWindow = new Timeline.Window();
    otherWindow.manifest = new Object();
    assertThat(window).isNotEqualTo(otherWindow);

    otherWindow = new Timeline.Window();
    otherWindow.presentationStartTimeMs = C.TIME_UNSET;
    assertThat(window).isNotEqualTo(otherWindow);

    otherWindow = new Timeline.Window();
    otherWindow.windowStartTimeMs = C.TIME_UNSET;
    assertThat(window).isNotEqualTo(otherWindow);

    otherWindow = new Timeline.Window();
    otherWindow.isSeekable = true;
    assertThat(window).isNotEqualTo(otherWindow);

    otherWindow = new Timeline.Window();
    otherWindow.isDynamic = true;
    assertThat(window).isNotEqualTo(otherWindow);

    otherWindow = new Timeline.Window();
    otherWindow.isLive = true;
    assertThat(window).isNotEqualTo(otherWindow);

    otherWindow = new Timeline.Window();
    otherWindow.isPlaceholder = true;
    assertThat(window).isNotEqualTo(otherWindow);

    otherWindow = new Timeline.Window();
    otherWindow.defaultPositionUs = C.TIME_UNSET;
    assertThat(window).isNotEqualTo(otherWindow);

    otherWindow = new Timeline.Window();
    otherWindow.durationUs = C.TIME_UNSET;
    assertThat(window).isNotEqualTo(otherWindow);

    otherWindow = new Timeline.Window();
    otherWindow.firstPeriodIndex = 1;
    assertThat(window).isNotEqualTo(otherWindow);

    otherWindow = new Timeline.Window();
    otherWindow.lastPeriodIndex = 1;
    assertThat(window).isNotEqualTo(otherWindow);

    otherWindow = new Timeline.Window();
    otherWindow.positionInFirstPeriodUs = C.TIME_UNSET;
    assertThat(window).isNotEqualTo(otherWindow);

    window.uid = new Object();
    window.tag = new Object();
    window.manifest = new Object();
    window.presentationStartTimeMs = C.TIME_UNSET;
    window.windowStartTimeMs = C.TIME_UNSET;
    window.isSeekable = true;
    window.isDynamic = true;
    window.isLive = true;
    window.defaultPositionUs = C.TIME_UNSET;
    window.durationUs = C.TIME_UNSET;
    window.firstPeriodIndex = 1;
    window.lastPeriodIndex = 1;
    window.positionInFirstPeriodUs = C.TIME_UNSET;
    otherWindow =
        otherWindow.set(
            window.uid,
            window.tag,
            window.manifest,
            window.presentationStartTimeMs,
            window.windowStartTimeMs,
            window.elapsedRealtimeEpochOffsetMs,
            window.isSeekable,
            window.isDynamic,
            window.isLive,
            window.defaultPositionUs,
            window.durationUs,
            window.firstPeriodIndex,
            window.lastPeriodIndex,
            window.positionInFirstPeriodUs);
    assertThat(window).isEqualTo(otherWindow);
  }

  @Test
  public void windowHashCode() {
    Timeline.Window window = new Timeline.Window();
    Timeline.Window otherWindow = new Timeline.Window();
    assertThat(window.hashCode()).isEqualTo(otherWindow.hashCode());

    window.tag = new Object();
    assertThat(window.hashCode()).isNotEqualTo(otherWindow.hashCode());
    otherWindow.tag = window.tag;
    assertThat(window.hashCode()).isEqualTo(otherWindow.hashCode());
  }

  @Test
  public void periodEquals() {
    Timeline.Period period = new Timeline.Period();
    assertThat(period).isEqualTo(new Timeline.Period());

    Timeline.Period otherPeriod = new Timeline.Period();
    otherPeriod.id = new Object();
    assertThat(period).isNotEqualTo(otherPeriod);

    otherPeriod = new Timeline.Period();
    otherPeriod.uid = new Object();
    assertThat(period).isNotEqualTo(otherPeriod);

    otherPeriod = new Timeline.Period();
    otherPeriod.windowIndex = 12;
    assertThat(period).isNotEqualTo(otherPeriod);

    otherPeriod = new Timeline.Period();
    otherPeriod.durationUs = 11L;
    assertThat(period).isNotEqualTo(otherPeriod);

    otherPeriod = new Timeline.Period();
    period.id = new Object();
    period.uid = new Object();
    period.windowIndex = 1;
    period.durationUs = 123L;
    otherPeriod =
        otherPeriod.set(
            period.id,
            period.uid,
            period.windowIndex,
            period.durationUs,
            /* positionInWindowUs= */ 0);
    assertThat(period).isEqualTo(otherPeriod);
  }

  @Test
  public void periodHashCode() {
    Timeline.Period period = new Timeline.Period();
    Timeline.Period otherPeriod = new Timeline.Period();
    assertThat(period.hashCode()).isEqualTo(otherPeriod.hashCode());

    period.windowIndex = 12;
    assertThat(period.hashCode()).isNotEqualTo(otherPeriod.hashCode());
    otherPeriod.windowIndex = period.windowIndex;
    assertThat(period.hashCode()).isEqualTo(otherPeriod.hashCode());
  }
}
