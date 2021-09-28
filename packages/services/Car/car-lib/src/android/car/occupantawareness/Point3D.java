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

package android.car.occupantawareness;

import android.annotation.NonNull;
import android.os.Parcel;
import android.os.Parcelable;

/**
 * A point in 3D space, in millimeters.
 *
 * @hide
 */
public final class Point3D implements Parcelable {
    /** The x-component of the point. */
    public final double x;

    /** The y-component of the point. */
    public final double y;

    /** The z-component of the point. */
    public final double z;

    public Point3D(double valueX, double valueY, double valueZ) {
        x = valueX;
        y = valueY;
        z = valueZ;
    }

    @Override
    public int describeContents() {
        return 0;
    }

    @Override
    public void writeToParcel(@NonNull Parcel dest, int flags) {
        dest.writeDouble(x);
        dest.writeDouble(y);
        dest.writeDouble(z);
    }

    @Override
    public String toString() {
        return String.format("%f, %f, %f", x, y, z);
    }

    public static final @NonNull Parcelable.Creator<Point3D> CREATOR =
            new Parcelable.Creator<Point3D>() {
                public Point3D createFromParcel(Parcel in) {
                    return new Point3D(in);
                }

                public Point3D[] newArray(int size) {
                    return new Point3D[size];
                }
            };

    private Point3D(Parcel in) {
        x = in.readDouble();
        y = in.readDouble();
        z = in.readDouble();
    }
}
