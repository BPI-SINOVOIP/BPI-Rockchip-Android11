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
package com.google.android.exoplayer2.testutil;

import static com.google.common.truth.Truth.assertThat;

import android.net.Uri;
import android.os.Handler;
import android.os.SystemClock;
import androidx.annotation.Nullable;
import com.google.android.exoplayer2.C;
import com.google.android.exoplayer2.Format;
import com.google.android.exoplayer2.Timeline;
import com.google.android.exoplayer2.Timeline.Period;
import com.google.android.exoplayer2.source.BaseMediaSource;
import com.google.android.exoplayer2.source.ForwardingTimeline;
import com.google.android.exoplayer2.source.LoadEventInfo;
import com.google.android.exoplayer2.source.MediaLoadData;
import com.google.android.exoplayer2.source.MediaPeriod;
import com.google.android.exoplayer2.source.MediaSource;
import com.google.android.exoplayer2.source.MediaSourceEventListener.EventDispatcher;
import com.google.android.exoplayer2.source.TrackGroup;
import com.google.android.exoplayer2.source.TrackGroupArray;
import com.google.android.exoplayer2.upstream.Allocator;
import com.google.android.exoplayer2.upstream.DataSpec;
import com.google.android.exoplayer2.upstream.TransferListener;
import com.google.android.exoplayer2.util.Assertions;
import com.google.android.exoplayer2.util.Util;
import java.io.IOException;
import java.util.ArrayList;
import java.util.Collections;
import java.util.List;
import org.checkerframework.checker.nullness.qual.MonotonicNonNull;

/**
 * Fake {@link MediaSource} that provides a given timeline. Creating the period will return a {@link
 * FakeMediaPeriod} with a {@link TrackGroupArray} using the given {@link Format}s.
 */
public class FakeMediaSource extends BaseMediaSource {

  /** A forwarding timeline to provide an initial timeline for fake multi window sources. */
  public static class InitialTimeline extends ForwardingTimeline {

    public InitialTimeline(Timeline timeline) {
      super(timeline);
    }

    @Override
    public Window getWindow(int windowIndex, Window window, long defaultPositionProjectionUs) {
      Window childWindow = timeline.getWindow(windowIndex, window, defaultPositionProjectionUs);
      childWindow.isDynamic = true;
      childWindow.isSeekable = false;
      return childWindow;
    }
  }

  private static final DataSpec FAKE_DATA_SPEC = new DataSpec(Uri.parse("http://manifest.uri"));
  private static final int MANIFEST_LOAD_BYTES = 100;

  private final TrackGroupArray trackGroupArray;
  private final ArrayList<FakeMediaPeriod> activeMediaPeriods;
  private final ArrayList<MediaPeriodId> createdMediaPeriods;

  private @MonotonicNonNull Timeline timeline;
  private boolean preparedSource;
  private boolean releasedSource;
  @Nullable private Handler sourceInfoRefreshHandler;
  @Nullable private TransferListener transferListener;

  /**
   * Creates a {@link FakeMediaSource}. This media source creates {@link FakeMediaPeriod}s with a
   * {@link TrackGroupArray} using the given {@link Format}s. The provided {@link Timeline} may be
   * null to prevent an immediate source info refresh message when preparing the media source. It
   * can be manually set later using {@link #setNewSourceInfo(Timeline)}.
   */
  public FakeMediaSource(@Nullable Timeline timeline, Format... formats) {
    this(timeline, buildTrackGroupArray(formats));
  }

  /**
   * Creates a {@link FakeMediaSource}. This media source creates {@link FakeMediaPeriod}s with the
   * given {@link TrackGroupArray}. The provided {@link Timeline} may be null to prevent an
   * immediate source info refresh message when preparing the media source. It can be manually set
   * later using {@link #setNewSourceInfo(Timeline)}.
   */
  public FakeMediaSource(@Nullable Timeline timeline, TrackGroupArray trackGroupArray) {
    if (timeline != null) {
      this.timeline = timeline;
    }
    this.trackGroupArray = trackGroupArray;
    this.activeMediaPeriods = new ArrayList<>();
    this.createdMediaPeriods = new ArrayList<>();
  }

  @Nullable
  protected Timeline getTimeline() {
    return timeline;
  }

  @Override
  @Nullable
  public Object getTag() {
    if (timeline == null || timeline.isEmpty()) {
      return null;
    }
    return timeline.getWindow(0, new Timeline.Window()).tag;
  }

  @Override
  @Nullable
  public Timeline getInitialTimeline() {
    return timeline == null || timeline == Timeline.EMPTY || timeline.getWindowCount() == 1
        ? null
        : new InitialTimeline(timeline);
  }

  @Override
  public boolean isSingleWindow() {
    return timeline == null || timeline == Timeline.EMPTY || timeline.getWindowCount() == 1;
  }

  @Override
  public synchronized void prepareSourceInternal(@Nullable TransferListener mediaTransferListener) {
    assertThat(preparedSource).isFalse();
    transferListener = mediaTransferListener;
    preparedSource = true;
    releasedSource = false;
    sourceInfoRefreshHandler = Util.createHandler();
    if (timeline != null) {
      finishSourcePreparation();
    }
  }

  @Override
  public void maybeThrowSourceInfoRefreshError() throws IOException {
    assertThat(preparedSource).isTrue();
  }

  @Override
  public MediaPeriod createPeriod(MediaPeriodId id, Allocator allocator, long startPositionUs) {
    assertThat(preparedSource).isTrue();
    assertThat(releasedSource).isFalse();
    int periodIndex = Util.castNonNull(timeline).getIndexOfPeriod(id.periodUid);
    Assertions.checkArgument(periodIndex != C.INDEX_UNSET);
    Period period = timeline.getPeriod(periodIndex, new Period());
    EventDispatcher eventDispatcher =
        createEventDispatcher(period.windowIndex, id, period.getPositionInWindowMs());
    FakeMediaPeriod mediaPeriod =
        createFakeMediaPeriod(id, trackGroupArray, allocator, eventDispatcher, transferListener);
    activeMediaPeriods.add(mediaPeriod);
    createdMediaPeriods.add(id);
    return mediaPeriod;
  }

  @Override
  public void releasePeriod(MediaPeriod mediaPeriod) {
    assertThat(preparedSource).isTrue();
    assertThat(releasedSource).isFalse();
    FakeMediaPeriod fakeMediaPeriod = (FakeMediaPeriod) mediaPeriod;
    assertThat(activeMediaPeriods.remove(fakeMediaPeriod)).isTrue();
    fakeMediaPeriod.release();
  }

  @Override
  protected void releaseSourceInternal() {
    assertThat(preparedSource).isTrue();
    assertThat(releasedSource).isFalse();
    assertThat(activeMediaPeriods.isEmpty()).isTrue();
    releasedSource = true;
    preparedSource = false;
    Util.castNonNull(sourceInfoRefreshHandler).removeCallbacksAndMessages(null);
    sourceInfoRefreshHandler = null;
  }

  /**
   * Sets a new timeline. If the source is already prepared, this triggers a source info refresh
   * message being sent to the listener.
   */
  public synchronized void setNewSourceInfo(final Timeline newTimeline) {
    if (sourceInfoRefreshHandler != null) {
      sourceInfoRefreshHandler.post(
          () -> {
            assertThat(releasedSource).isFalse();
            assertThat(preparedSource).isTrue();
            timeline = newTimeline;
            finishSourcePreparation();
          });
    } else {
      timeline = newTimeline;
    }
  }

  /** Returns whether the source is currently prepared. */
  public boolean isPrepared() {
    return preparedSource;
  }

  /**
   * Assert that the source and all periods have been released.
   */
  public void assertReleased() {
    assertThat(releasedSource || !preparedSource).isTrue();
  }

  /**
   * Assert that a media period for the given id has been created.
   */
  public void assertMediaPeriodCreated(MediaPeriodId mediaPeriodId) {
    assertThat(createdMediaPeriods).contains(mediaPeriodId);
  }

  /** Returns a list of {@link MediaPeriodId}s, with one element for each created media period. */
  public List<MediaPeriodId> getCreatedMediaPeriods() {
    return createdMediaPeriods;
  }

  /**
   * Creates a {@link FakeMediaPeriod} for this media source.
   *
   * @param id The identifier of the period.
   * @param trackGroupArray The {@link TrackGroupArray} supported by the media period.
   * @param allocator An {@link Allocator} from which to obtain media buffer allocations.
   * @param eventDispatcher An {@link EventDispatcher} to dispatch media source events.
   * @param transferListener The transfer listener which should be informed of any data transfers.
   *     May be null if no listener is available.
   * @return A new {@link FakeMediaPeriod}.
   */
  protected FakeMediaPeriod createFakeMediaPeriod(
      MediaPeriodId id,
      TrackGroupArray trackGroupArray,
      Allocator allocator,
      EventDispatcher eventDispatcher,
      @Nullable TransferListener transferListener) {
    return new FakeMediaPeriod(trackGroupArray, eventDispatcher);
  }

  private void finishSourcePreparation() {
    refreshSourceInfo(Assertions.checkStateNotNull(timeline));
    if (!timeline.isEmpty()) {
      MediaLoadData mediaLoadData =
          new MediaLoadData(
              C.DATA_TYPE_MANIFEST,
              C.TRACK_TYPE_UNKNOWN,
              /* trackFormat= */ null,
              C.SELECTION_REASON_UNKNOWN,
              /* trackSelectionData= */ null,
              /* mediaStartTimeMs= */ C.TIME_UNSET,
              /* mediaEndTimeMs = */ C.TIME_UNSET);
      long elapsedRealTimeMs = SystemClock.elapsedRealtime();
      EventDispatcher eventDispatcher = createEventDispatcher(/* mediaPeriodId= */ null);
      eventDispatcher.loadStarted(
          new LoadEventInfo(
              FAKE_DATA_SPEC,
              FAKE_DATA_SPEC.uri,
              /* responseHeaders= */ Collections.emptyMap(),
              elapsedRealTimeMs,
              /* loadDurationMs= */ 0,
              /* bytesLoaded= */ 0),
          mediaLoadData);
      eventDispatcher.loadCompleted(
          new LoadEventInfo(
              FAKE_DATA_SPEC,
              FAKE_DATA_SPEC.uri,
              /* responseHeaders= */ Collections.emptyMap(),
              elapsedRealTimeMs,
              /* loadDurationMs= */ 0,
              /* bytesLoaded= */ MANIFEST_LOAD_BYTES),
          mediaLoadData);
    }
  }

  private static TrackGroupArray buildTrackGroupArray(Format... formats) {
    TrackGroup[] trackGroups = new TrackGroup[formats.length];
    for (int i = 0; i < formats.length; i++) {
      trackGroups[i] = new TrackGroup(formats[i]);
    }
    return new TrackGroupArray(trackGroups);
  }
}
