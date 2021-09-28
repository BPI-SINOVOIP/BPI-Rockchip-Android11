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
 * limitations under the License
 */

package android.os.cts;

import android.os.IBinder;
import android.os.Parcel;
import android.os.Parcelable;


public class ExceptionalParcelable implements Parcelable {
    private final IBinder mBinder;

    ExceptionalParcelable(IBinder binder) {
        mBinder = binder;
    }

    public int describeContents() {
        return 0;
    }

    /**
     * Write a binder to the Parcel and then throw an exception
     */
    public void writeToParcel(Parcel out, int flags) {
        // Write a binder for the exception to overwrite
        out.writeStrongBinder(mBinder);

        // Throw an exception
        throw new IllegalArgumentException("A truly exceptional message");
    }

    public static final Creator<ExceptionalParcelable> CREATOR =
            new Creator<ExceptionalParcelable>() {
                @Override
                public ExceptionalParcelable createFromParcel(Parcel source) {
                    return new ExceptionalParcelable(source.readStrongBinder());
                }

                @Override
                public ExceptionalParcelable[] newArray(int size) {
                    return new ExceptionalParcelable[size];
                }
            };
}
