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
package com.google.android.exoplayer2;

import static com.google.common.truth.Truth.assertThat;
import static org.junit.Assert.assertNull;
import static org.mockito.Mockito.mock;
import static org.robolectric.annotation.LooperMode.Mode.LEGACY;

import android.net.Uri;
import androidx.test.ext.junit.runners.AndroidJUnit4;
import com.google.android.exoplayer2.source.MediaSource.MediaPeriodId;
import com.google.android.exoplayer2.source.ShuffleOrder;
import com.google.android.exoplayer2.source.SinglePeriodTimeline;
import com.google.android.exoplayer2.source.ads.AdPlaybackState;
import com.google.android.exoplayer2.source.ads.SinglePeriodAdTimeline;
import com.google.android.exoplayer2.testutil.FakeMediaSource;
import com.google.android.exoplayer2.testutil.FakeTimeline;
import com.google.android.exoplayer2.testutil.FakeTimeline.TimelineWindowDefinition;
import com.google.android.exoplayer2.trackselection.TrackSelection;
import com.google.android.exoplayer2.trackselection.TrackSelector;
import com.google.android.exoplayer2.trackselection.TrackSelectorResult;
import com.google.android.exoplayer2.upstream.Allocator;
import java.util.Collections;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.LooperMode;

/** Unit tests for {@link MediaPeriodQueue}. */
@LooperMode(LEGACY)
@RunWith(AndroidJUnit4.class)
public final class MediaPeriodQueueTest {

  private static final long CONTENT_DURATION_US = 30 * C.MICROS_PER_SECOND;
  private static final long AD_DURATION_US = 10 * C.MICROS_PER_SECOND;
  private static final long FIRST_AD_START_TIME_US = 10 * C.MICROS_PER_SECOND;
  private static final long SECOND_AD_START_TIME_US = 20 * C.MICROS_PER_SECOND;

  private static final Timeline CONTENT_TIMELINE =
      new SinglePeriodTimeline(
          CONTENT_DURATION_US, /* isSeekable= */ true, /* isDynamic= */ false, /* isLive= */ false);
  private static final Uri AD_URI = Uri.EMPTY;

  private MediaPeriodQueue mediaPeriodQueue;
  private AdPlaybackState adPlaybackState;
  private Object firstPeriodUid;

  private PlaybackInfo playbackInfo;
  private RendererCapabilities[] rendererCapabilities;
  private TrackSelector trackSelector;
  private Allocator allocator;
  private MediaSourceList mediaSourceList;
  private FakeMediaSource fakeMediaSource;
  private MediaSourceList.MediaSourceHolder mediaSourceHolder;

  @Before
  public void setUp() {
    mediaPeriodQueue = new MediaPeriodQueue();
    mediaSourceList = mock(MediaSourceList.class);
    rendererCapabilities = new RendererCapabilities[0];
    trackSelector = mock(TrackSelector.class);
    allocator = mock(Allocator.class);
  }

  @Test
  public void getNextMediaPeriodInfo_withoutAds_returnsLastMediaPeriodInfo() {
    setupAdTimeline(/* no ad groups */ );
    assertGetNextMediaPeriodInfoReturnsContentMediaPeriod(
        /* periodUid= */ firstPeriodUid,
        /* startPositionUs= */ 0,
        /* requestedContentPositionUs= */ C.TIME_UNSET,
        /* endPositionUs= */ C.TIME_UNSET,
        /* durationUs= */ CONTENT_DURATION_US,
        /* isLastInPeriod= */ true,
        /* isLastInWindow= */ true,
        /* nextAdGroupIndex= */ C.INDEX_UNSET);
  }

  @Test
  public void getNextMediaPeriodInfo_withPrerollAd_returnsCorrectMediaPeriodInfos() {
    setupAdTimeline(/* adGroupTimesUs...= */ 0);
    setAdGroupLoaded(/* adGroupIndex= */ 0);
    assertNextMediaPeriodInfoIsAd(/* adGroupIndex= */ 0, /* contentPositionUs= */ C.TIME_UNSET);
    advance();
    assertGetNextMediaPeriodInfoReturnsContentMediaPeriod(
        /* periodUid= */ firstPeriodUid,
        /* startPositionUs= */ 0,
        /* requestedContentPositionUs= */ C.TIME_UNSET,
        /* endPositionUs= */ C.TIME_UNSET,
        /* durationUs= */ CONTENT_DURATION_US,
        /* isLastInPeriod= */ true,
        /* isLastInWindow= */ true,
        /* nextAdGroupIndex= */ C.INDEX_UNSET);
  }

  @Test
  public void getNextMediaPeriodInfo_withMidrollAds_returnsCorrectMediaPeriodInfos() {
    setupAdTimeline(/* adGroupTimesUs...= */ FIRST_AD_START_TIME_US, SECOND_AD_START_TIME_US);
    assertGetNextMediaPeriodInfoReturnsContentMediaPeriod(
        /* periodUid= */ firstPeriodUid,
        /* startPositionUs= */ 0,
        /* requestedContentPositionUs= */ C.TIME_UNSET,
        /* endPositionUs= */ FIRST_AD_START_TIME_US,
        /* durationUs= */ FIRST_AD_START_TIME_US,
        /* isLastInPeriod= */ false,
        /* isLastInWindow= */ false,
        /* nextAdGroupIndex= */ 0);
    // The next media period info should be null as we haven't loaded the ad yet.
    advance();
    assertNull(getNextMediaPeriodInfo());
    setAdGroupLoaded(/* adGroupIndex= */ 0);
    assertNextMediaPeriodInfoIsAd(
        /* adGroupIndex= */ 0, /* contentPositionUs= */ FIRST_AD_START_TIME_US);
    advance();
    assertGetNextMediaPeriodInfoReturnsContentMediaPeriod(
        /* periodUid= */ firstPeriodUid,
        /* startPositionUs= */ FIRST_AD_START_TIME_US,
        /* requestedContentPositionUs= */ FIRST_AD_START_TIME_US,
        /* endPositionUs= */ SECOND_AD_START_TIME_US,
        /* durationUs= */ SECOND_AD_START_TIME_US,
        /* isLastInPeriod= */ false,
        /* isLastInWindow= */ false,
        /* nextAdGroupIndex= */ 1);
    advance();
    setAdGroupLoaded(/* adGroupIndex= */ 1);
    assertNextMediaPeriodInfoIsAd(
        /* adGroupIndex= */ 1, /* contentPositionUs= */ SECOND_AD_START_TIME_US);
    advance();
    assertGetNextMediaPeriodInfoReturnsContentMediaPeriod(
        /* periodUid= */ firstPeriodUid,
        /* startPositionUs= */ SECOND_AD_START_TIME_US,
        /* requestedContentPositionUs= */ SECOND_AD_START_TIME_US,
        /* endPositionUs= */ C.TIME_UNSET,
        /* durationUs= */ CONTENT_DURATION_US,
        /* isLastInPeriod= */ true,
        /* isLastInWindow= */ true,
        /* nextAdGroupIndex= */ C.INDEX_UNSET);
  }

  @Test
  public void getNextMediaPeriodInfo_withMidrollAndPostroll_returnsCorrectMediaPeriodInfos() {
    setupAdTimeline(/* adGroupTimesUs...= */ FIRST_AD_START_TIME_US, C.TIME_END_OF_SOURCE);
    assertGetNextMediaPeriodInfoReturnsContentMediaPeriod(
        /* periodUid= */ firstPeriodUid,
        /* startPositionUs= */ 0,
        /* requestedContentPositionUs= */ C.TIME_UNSET,
        /* endPositionUs= */ FIRST_AD_START_TIME_US,
        /* durationUs= */ FIRST_AD_START_TIME_US,
        /* isLastInPeriod= */ false,
        /* isLastInWindow= */ false,
        /* nextAdGroupIndex= */ 0);
    advance();
    setAdGroupLoaded(/* adGroupIndex= */ 0);
    assertNextMediaPeriodInfoIsAd(
        /* adGroupIndex= */ 0, /* contentPositionUs= */ FIRST_AD_START_TIME_US);
    advance();
    assertGetNextMediaPeriodInfoReturnsContentMediaPeriod(
        /* periodUid= */ firstPeriodUid,
        /* startPositionUs= */ FIRST_AD_START_TIME_US,
        /* requestedContentPositionUs= */ FIRST_AD_START_TIME_US,
        /* endPositionUs= */ C.TIME_END_OF_SOURCE,
        /* durationUs= */ CONTENT_DURATION_US,
        /* isLastInPeriod= */ false,
        /* isLastInWindow= */ false,
        /* nextAdGroupIndex= */ 1);
    advance();
    setAdGroupLoaded(/* adGroupIndex= */ 1);
    assertNextMediaPeriodInfoIsAd(
        /* adGroupIndex= */ 1, /* contentPositionUs= */ CONTENT_DURATION_US);
    advance();
    assertGetNextMediaPeriodInfoReturnsContentMediaPeriod(
        /* periodUid= */ firstPeriodUid,
        /* startPositionUs= */ CONTENT_DURATION_US - 1,
        /* requestedContentPositionUs= */ CONTENT_DURATION_US,
        /* endPositionUs= */ C.TIME_UNSET,
        /* durationUs= */ CONTENT_DURATION_US,
        /* isLastInPeriod= */ true,
        /* isLastInWindow= */ true,
        /* nextAdGroupIndex= */ C.INDEX_UNSET);
  }

  @Test
  public void getNextMediaPeriodInfo_withPostrollLoadError_returnsEmptyFinalMediaPeriodInfo() {
    setupAdTimeline(/* adGroupTimesUs...= */ C.TIME_END_OF_SOURCE);
    assertGetNextMediaPeriodInfoReturnsContentMediaPeriod(
        /* periodUid= */ firstPeriodUid,
        /* startPositionUs= */ 0,
        /* requestedContentPositionUs= */ C.TIME_UNSET,
        /* endPositionUs= */ C.TIME_END_OF_SOURCE,
        /* durationUs= */ CONTENT_DURATION_US,
        /* isLastInPeriod= */ false,
        /* isLastInWindow= */ false,
        /* nextAdGroupIndex= */ 0);
    advance();
    setAdGroupFailedToLoad(/* adGroupIndex= */ 0);
    assertGetNextMediaPeriodInfoReturnsContentMediaPeriod(
        /* periodUid= */ firstPeriodUid,
        /* startPositionUs= */ CONTENT_DURATION_US - 1,
        /* requestedContentPositionUs= */ CONTENT_DURATION_US,
        /* endPositionUs= */ C.TIME_UNSET,
        /* durationUs= */ CONTENT_DURATION_US,
        /* isLastInPeriod= */ true,
        /* isLastInWindow= */ true,
        /* nextAdGroupIndex= */ C.INDEX_UNSET);
  }

  @Test
  public void getNextMediaPeriodInfo_inMultiPeriodWindow_returnsCorrectMediaPeriodInfos() {
    setupTimeline(
        new FakeTimeline(
            new TimelineWindowDefinition(
                /* periodCount= */ 2,
                /* id= */ new Object(),
                /* isSeekable= */ false,
                /* isDynamic= */ false,
                /* durationUs= */ 2 * CONTENT_DURATION_US)));

    assertGetNextMediaPeriodInfoReturnsContentMediaPeriod(
        /* periodUid= */ playbackInfo.timeline.getUidOfPeriod(/* periodIndex= */ 0),
        /* startPositionUs= */ 0,
        /* requestedContentPositionUs= */ C.TIME_UNSET,
        /* endPositionUs= */ C.TIME_UNSET,
        /* durationUs= */ CONTENT_DURATION_US
            + TimelineWindowDefinition.DEFAULT_WINDOW_OFFSET_IN_FIRST_PERIOD_US,
        /* isLastInPeriod= */ true,
        /* isLastInWindow= */ false,
        /* nextAdGroupIndex= */ C.INDEX_UNSET);
    advance();
    assertGetNextMediaPeriodInfoReturnsContentMediaPeriod(
        /* periodUid= */ playbackInfo.timeline.getUidOfPeriod(/* periodIndex= */ 1),
        /* startPositionUs= */ 0,
        /* requestedContentPositionUs= */ 0,
        /* endPositionUs= */ C.TIME_UNSET,
        /* durationUs= */ CONTENT_DURATION_US,
        /* isLastInPeriod= */ true,
        /* isLastInWindow= */ true,
        /* nextAdGroupIndex= */ C.INDEX_UNSET);
  }

  @Test
  public void
      updateQueuedPeriods_withDurationChangeAfterReadingPeriod_handlesChangeAndRemovesPeriodsAfterChangedPeriod() {
    setupAdTimeline(/* adGroupTimesUs...= */ FIRST_AD_START_TIME_US, SECOND_AD_START_TIME_US);
    setAdGroupLoaded(/* adGroupIndex= */ 0);
    setAdGroupLoaded(/* adGroupIndex= */ 1);
    enqueueNext(); // Content before first ad.
    advancePlaying();
    enqueueNext(); // First ad.
    enqueueNext(); // Content between ads.
    enqueueNext(); // Second ad.

    // Change position of second ad (= change duration of content between ads).
    updateAdPlaybackStateAndTimeline(
        /* adGroupTimesUs...= */ FIRST_AD_START_TIME_US, SECOND_AD_START_TIME_US + 1);
    setAdGroupLoaded(/* adGroupIndex= */ 0);
    setAdGroupLoaded(/* adGroupIndex= */ 1);
    boolean changeHandled =
        mediaPeriodQueue.updateQueuedPeriods(
            playbackInfo.timeline, /* rendererPositionUs= */ 0, /* maxRendererReadPositionUs= */ 0);

    assertThat(changeHandled).isTrue();
    assertThat(getQueueLength()).isEqualTo(3);
  }

  @Test
  public void
      updateQueuedPeriods_withDurationChangeBeforeReadingPeriod_doesntHandleChangeAndRemovesPeriodsAfterChangedPeriod() {
    setupAdTimeline(/* adGroupTimesUs...= */ FIRST_AD_START_TIME_US, SECOND_AD_START_TIME_US);
    setAdGroupLoaded(/* adGroupIndex= */ 0);
    setAdGroupLoaded(/* adGroupIndex= */ 1);
    enqueueNext(); // Content before first ad.
    advancePlaying();
    enqueueNext(); // First ad.
    enqueueNext(); // Content between ads.
    enqueueNext(); // Second ad.
    advanceReading(); // Reading first ad.

    // Change position of first ad (= change duration of content before first ad).
    updateAdPlaybackStateAndTimeline(
        /* adGroupTimesUs...= */ FIRST_AD_START_TIME_US + 1, SECOND_AD_START_TIME_US);
    setAdGroupLoaded(/* adGroupIndex= */ 0);
    setAdGroupLoaded(/* adGroupIndex= */ 1);
    boolean changeHandled =
        mediaPeriodQueue.updateQueuedPeriods(
            playbackInfo.timeline,
            /* rendererPositionUs= */ 0,
            /* maxRendererReadPositionUs= */ FIRST_AD_START_TIME_US);

    assertThat(changeHandled).isFalse();
    assertThat(getQueueLength()).isEqualTo(1);
  }

  @Test
  public void
      updateQueuedPeriods_withDurationChangeInReadingPeriodAfterReadingPosition_handlesChangeAndRemovesPeriodsAfterChangedPeriod() {
    setupAdTimeline(/* adGroupTimesUs...= */ FIRST_AD_START_TIME_US, SECOND_AD_START_TIME_US);
    setAdGroupLoaded(/* adGroupIndex= */ 0);
    setAdGroupLoaded(/* adGroupIndex= */ 1);
    enqueueNext(); // Content before first ad.
    advancePlaying();
    enqueueNext(); // First ad.
    enqueueNext(); // Content between ads.
    enqueueNext(); // Second ad.
    advanceReading(); // Reading first ad.
    advanceReading(); // Reading content between ads.

    // Change position of second ad (= change duration of content between ads).
    updateAdPlaybackStateAndTimeline(
        /* adGroupTimesUs...= */ FIRST_AD_START_TIME_US, SECOND_AD_START_TIME_US - 1000);
    setAdGroupLoaded(/* adGroupIndex= */ 0);
    setAdGroupLoaded(/* adGroupIndex= */ 1);
    long readingPositionAtStartOfContentBetweenAds = FIRST_AD_START_TIME_US + AD_DURATION_US;
    boolean changeHandled =
        mediaPeriodQueue.updateQueuedPeriods(
            playbackInfo.timeline,
            /* rendererPositionUs= */ 0,
            /* maxRendererReadPositionUs= */ readingPositionAtStartOfContentBetweenAds);

    assertThat(changeHandled).isTrue();
    assertThat(getQueueLength()).isEqualTo(3);
  }

  @Test
  public void
      updateQueuedPeriods_withDurationChangeInReadingPeriodBeforeReadingPosition_doesntHandleChangeAndRemovesPeriodsAfterChangedPeriod() {
    setupAdTimeline(/* adGroupTimesUs...= */ FIRST_AD_START_TIME_US, SECOND_AD_START_TIME_US);
    setAdGroupLoaded(/* adGroupIndex= */ 0);
    setAdGroupLoaded(/* adGroupIndex= */ 1);
    enqueueNext(); // Content before first ad.
    advancePlaying();
    enqueueNext(); // First ad.
    enqueueNext(); // Content between ads.
    enqueueNext(); // Second ad.
    advanceReading(); // Reading first ad.
    advanceReading(); // Reading content between ads.

    // Change position of second ad (= change duration of content between ads).
    updateAdPlaybackStateAndTimeline(
        /* adGroupTimesUs...= */ FIRST_AD_START_TIME_US, SECOND_AD_START_TIME_US - 1000);
    setAdGroupLoaded(/* adGroupIndex= */ 0);
    setAdGroupLoaded(/* adGroupIndex= */ 1);
    long readingPositionAtEndOfContentBetweenAds = SECOND_AD_START_TIME_US + AD_DURATION_US;
    boolean changeHandled =
        mediaPeriodQueue.updateQueuedPeriods(
            playbackInfo.timeline,
            /* rendererPositionUs= */ 0,
            /* maxRendererReadPositionUs= */ readingPositionAtEndOfContentBetweenAds);

    assertThat(changeHandled).isFalse();
    assertThat(getQueueLength()).isEqualTo(3);
  }

  @Test
  public void
      updateQueuedPeriods_withDurationChangeInReadingPeriodReadToEnd_doesntHandleChangeAndRemovesPeriodsAfterChangedPeriod() {
    setupAdTimeline(/* adGroupTimesUs...= */ FIRST_AD_START_TIME_US, SECOND_AD_START_TIME_US);
    setAdGroupLoaded(/* adGroupIndex= */ 0);
    setAdGroupLoaded(/* adGroupIndex= */ 1);
    enqueueNext(); // Content before first ad.
    advancePlaying();
    enqueueNext(); // First ad.
    enqueueNext(); // Content between ads.
    enqueueNext(); // Second ad.
    advanceReading(); // Reading first ad.
    advanceReading(); // Reading content between ads.

    // Change position of second ad (= change duration of content between ads).
    updateAdPlaybackStateAndTimeline(
        /* adGroupTimesUs...= */ FIRST_AD_START_TIME_US, SECOND_AD_START_TIME_US - 1000);
    setAdGroupLoaded(/* adGroupIndex= */ 0);
    setAdGroupLoaded(/* adGroupIndex= */ 1);
    boolean changeHandled =
        mediaPeriodQueue.updateQueuedPeriods(
            playbackInfo.timeline,
            /* rendererPositionUs= */ 0,
            /* maxRendererReadPositionUs= */ C.TIME_END_OF_SOURCE);

    assertThat(changeHandled).isFalse();
    assertThat(getQueueLength()).isEqualTo(3);
  }

  private void setupAdTimeline(long... adGroupTimesUs) {
    adPlaybackState =
        new AdPlaybackState(adGroupTimesUs).withContentDurationUs(CONTENT_DURATION_US);
    SinglePeriodAdTimeline adTimeline =
        new SinglePeriodAdTimeline(CONTENT_TIMELINE, adPlaybackState);
    setupTimeline(adTimeline);
  }

  private void setupTimeline(Timeline timeline) {
    fakeMediaSource = new FakeMediaSource(timeline);
    mediaSourceHolder = new MediaSourceList.MediaSourceHolder(fakeMediaSource, false);
    mediaSourceHolder.mediaSource.prepareSourceInternal(/* mediaTransferListener */ null);

    Timeline playlistTimeline = createPlaylistTimeline();
    firstPeriodUid = playlistTimeline.getUidOfPeriod(/* periodIndex= */ 0);

    playbackInfo =
        new PlaybackInfo(
            playlistTimeline,
            mediaPeriodQueue.resolveMediaPeriodIdForAds(
                playlistTimeline, firstPeriodUid, /* positionUs= */ 0),
            /* requestedContentPositionUs= */ C.TIME_UNSET,
            Player.STATE_READY,
            /* playbackError= */ null,
            /* isLoading= */ false,
            /* trackGroups= */ null,
            /* trackSelectorResult= */ null,
            /* loadingMediaPeriodId= */ null,
            /* playWhenReady= */ false,
            Player.PLAYBACK_SUPPRESSION_REASON_NONE,
            /* bufferedPositionUs= */ 0,
            /* totalBufferedDurationUs= */ 0,
            /* positionUs= */ 0);
  }

  private void updateAdPlaybackStateAndTimeline(long... adGroupTimesUs) {
    adPlaybackState =
        new AdPlaybackState(adGroupTimesUs).withContentDurationUs(CONTENT_DURATION_US);
    updateTimeline();
  }

  private void updateTimeline() {
    SinglePeriodAdTimeline adTimeline =
        new SinglePeriodAdTimeline(CONTENT_TIMELINE, adPlaybackState);
    fakeMediaSource.setNewSourceInfo(adTimeline);
    playbackInfo = playbackInfo.copyWithTimeline(createPlaylistTimeline());
  }

  private MediaSourceList.PlaylistTimeline createPlaylistTimeline() {
    return new MediaSourceList.PlaylistTimeline(
        Collections.singleton(mediaSourceHolder),
        new ShuffleOrder.DefaultShuffleOrder(/* length= */ 1));
  }

  private void advance() {
    enqueueNext();
    if (mediaPeriodQueue.getLoadingPeriod() != mediaPeriodQueue.getPlayingPeriod()) {
      advancePlaying();
    }
  }

  private void advancePlaying() {
    mediaPeriodQueue.advancePlayingPeriod();
  }

  private void advanceReading() {
    mediaPeriodQueue.advanceReadingPeriod();
  }

  private void enqueueNext() {
    mediaPeriodQueue.enqueueNextMediaPeriodHolder(
        rendererCapabilities,
        trackSelector,
        allocator,
        mediaSourceList,
        getNextMediaPeriodInfo(),
        new TrackSelectorResult(
            new RendererConfiguration[0], new TrackSelection[0], /* info= */ null));
  }

  private MediaPeriodInfo getNextMediaPeriodInfo() {
    return mediaPeriodQueue.getNextMediaPeriodInfo(/* rendererPositionUs= */ 0, playbackInfo);
  }

  private void setAdGroupLoaded(int adGroupIndex) {
    long[][] newDurations = new long[adPlaybackState.adGroupCount][];
    for (int i = 0; i < adPlaybackState.adGroupCount; i++) {
      newDurations[i] =
          i == adGroupIndex ? new long[] {AD_DURATION_US} : adPlaybackState.adGroups[i].durationsUs;
    }
    adPlaybackState =
        adPlaybackState
            .withAdCount(adGroupIndex, /* adCount= */ 1)
            .withAdUri(adGroupIndex, /* adIndexInAdGroup= */ 0, AD_URI)
            .withAdDurationsUs(newDurations);
    updateTimeline();
  }

  private void setAdGroupFailedToLoad(int adGroupIndex) {
    adPlaybackState =
        adPlaybackState
            .withAdCount(adGroupIndex, /* adCount= */ 1)
            .withAdLoadError(adGroupIndex, /* adIndexInAdGroup= */ 0);
    updateTimeline();
  }

  private void assertGetNextMediaPeriodInfoReturnsContentMediaPeriod(
      Object periodUid,
      long startPositionUs,
      long requestedContentPositionUs,
      long endPositionUs,
      long durationUs,
      boolean isLastInPeriod,
      boolean isLastInWindow,
      int nextAdGroupIndex) {
    assertThat(getNextMediaPeriodInfo())
        .isEqualTo(
            new MediaPeriodInfo(
                new MediaPeriodId(periodUid, /* windowSequenceNumber= */ 0, nextAdGroupIndex),
                startPositionUs,
                requestedContentPositionUs,
                endPositionUs,
                durationUs,
                isLastInPeriod,
                isLastInWindow,
                /* isFinal= */ isLastInWindow));
  }

  private void assertNextMediaPeriodInfoIsAd(int adGroupIndex, long contentPositionUs) {
    assertThat(getNextMediaPeriodInfo())
        .isEqualTo(
            new MediaPeriodInfo(
                new MediaPeriodId(
                    firstPeriodUid,
                    adGroupIndex,
                    /* adIndexInAdGroup= */ 0,
                    /* windowSequenceNumber= */ 0),
                /* startPositionUs= */ 0,
                contentPositionUs,
                /* endPositionUs= */ C.TIME_UNSET,
                /* durationUs= */ AD_DURATION_US,
                /* isLastInTimelinePeriod= */ false,
                /* isLastInTimelineWindow= */ false,
                /* isFinal= */ false));
  }

  private int getQueueLength() {
    int length = 0;
    MediaPeriodHolder periodHolder = mediaPeriodQueue.getPlayingPeriod();
    while (periodHolder != null) {
      length++;
      periodHolder = periodHolder.getNext();
    }
    return length;
  }
}
