/*
 * Copyright (C) 2019, The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package android.aidl.tests.generic;

import android.os.Parcel;
import android.os.Parcelable;

public class Pair<A, B> implements Parcelable {
  public A mFirst;
  public B mSecond;

  public Pair() {}
  public Pair(Parcel source) { readFromParcel(source); }

  public int describeContents() { return 0; }

  public void writeToParcel(Parcel dest, int flags) {
    dest.writeValue(mFirst);
    dest.writeValue(mSecond);
  }

  public void readFromParcel(Parcel source) {
    mFirst = (A) source.readValue(null);
    mSecond = (B) source.readValue(null);
  }

  public static final Parcelable.Creator<Pair> CREATOR = new Parcelable.Creator<Pair>() {
    public Pair createFromParcel(Parcel source) { return new Pair(source); }

    public Pair[] newArray(int size) { return new Pair[size]; }
  };
}
