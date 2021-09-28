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
 * limitations under the License.
 */
package com.google.android.exoplayer2.metadata.flac;

import static com.google.common.truth.Truth.assertThat;

import android.os.Parcel;
import androidx.test.ext.junit.runners.AndroidJUnit4;
import org.junit.Test;
import org.junit.runner.RunWith;

/** Test for {@link VorbisComment}. */
@RunWith(AndroidJUnit4.class)
public final class VorbisCommentTest {

  @Test
  public void parcelable() {
    VorbisComment vorbisCommentFrameToParcel = new VorbisComment("key", "value");

    Parcel parcel = Parcel.obtain();
    vorbisCommentFrameToParcel.writeToParcel(parcel, 0);
    parcel.setDataPosition(0);

    VorbisComment vorbisCommentFrameFromParcel = VorbisComment.CREATOR.createFromParcel(parcel);
    assertThat(vorbisCommentFrameFromParcel).isEqualTo(vorbisCommentFrameToParcel);

    parcel.recycle();
  }
}
