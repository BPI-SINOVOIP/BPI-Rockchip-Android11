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
package com.google.android.exoplayer2.source;

import androidx.annotation.Nullable;
import com.google.android.exoplayer2.C;
import com.google.android.exoplayer2.Format;

/** Descriptor for data being loaded or selected by a media source. */
public final class MediaLoadData {

  /** One of the {@link C} {@code DATA_TYPE_*} constants defining the type of data. */
  public final int dataType;
  /**
   * One of the {@link C} {@code TRACK_TYPE_*} constants if the data corresponds to media of a
   * specific type. {@link C#TRACK_TYPE_UNKNOWN} otherwise.
   */
  public final int trackType;
  /**
   * The format of the track to which the data belongs. Null if the data does not belong to a
   * specific track.
   */
  @Nullable public final Format trackFormat;
  /**
   * One of the {@link C} {@code SELECTION_REASON_*} constants if the data belongs to a track.
   * {@link C#SELECTION_REASON_UNKNOWN} otherwise.
   */
  public final int trackSelectionReason;
  /**
   * Optional data associated with the selection of the track to which the data belongs. Null if the
   * data does not belong to a track.
   */
  @Nullable public final Object trackSelectionData;
  /**
   * The start time of the media, or {@link C#TIME_UNSET} if the data does not belong to a specific
   * media period.
   */
  public final long mediaStartTimeMs;
  /**
   * The end time of the media, or {@link C#TIME_UNSET} if the data does not belong to a specific
   * media period or the end time is unknown.
   */
  public final long mediaEndTimeMs;

  /**
   * Creates media load data.
   *
   * @param dataType One of the {@link C} {@code DATA_TYPE_*} constants defining the type of data.
   * @param trackType One of the {@link C} {@code TRACK_TYPE_*} constants if the data corresponds to
   *     media of a specific type. {@link C#TRACK_TYPE_UNKNOWN} otherwise.
   * @param trackFormat The format of the track to which the data belongs. Null if the data does not
   *     belong to a track.
   * @param trackSelectionReason One of the {@link C} {@code SELECTION_REASON_*} constants if the
   *     data belongs to a track. {@link C#SELECTION_REASON_UNKNOWN} otherwise.
   * @param trackSelectionData Optional data associated with the selection of the track to which the
   *     data belongs. Null if the data does not belong to a track.
   * @param mediaStartTimeMs The start time of the media, or {@link C#TIME_UNSET} if the data does
   *     not belong to a specific media period.
   * @param mediaEndTimeMs The end time of the media, or {@link C#TIME_UNSET} if the data does not
   *     belong to a specific media period or the end time is unknown.
   */
  public MediaLoadData(
      int dataType,
      int trackType,
      @Nullable Format trackFormat,
      int trackSelectionReason,
      @Nullable Object trackSelectionData,
      long mediaStartTimeMs,
      long mediaEndTimeMs) {
    this.dataType = dataType;
    this.trackType = trackType;
    this.trackFormat = trackFormat;
    this.trackSelectionReason = trackSelectionReason;
    this.trackSelectionData = trackSelectionData;
    this.mediaStartTimeMs = mediaStartTimeMs;
    this.mediaEndTimeMs = mediaEndTimeMs;
  }
}
