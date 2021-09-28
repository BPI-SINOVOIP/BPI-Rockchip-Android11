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

package android.car.hardware.property;

import android.annotation.NonNull;
import android.car.hardware.CarPropertyValue;
import android.os.Parcel;
import android.os.Parcelable;

/** @hide */
public class CarPropertyEvent implements Parcelable {
    public static final int PROPERTY_EVENT_PROPERTY_CHANGE = 0;
    public static final int PROPERTY_EVENT_ERROR = 1;
    /**
     * EventType of this message
     */
    private final int mEventType;

    /**
     * ErrorCode of this message.
     */
    private final @CarPropertyManager.CarSetPropertyErrorCode int mErrorCode;
    private final CarPropertyValue<?> mCarPropertyValue;

    // Use it as default value for error events.
    private static final int ERROR_EVENT_VALUE = -1;

    /**
     * @return EventType field
     */
    public int getEventType() {
        return mEventType;
    }

    /**
     * Returns {@link CarPropertyValue} associated with this event.
     */
    public CarPropertyValue<?> getCarPropertyValue() {
        return mCarPropertyValue;
    }

    @Override
    public int describeContents() {
        return 0;
    }

    @Override
    public void writeToParcel(Parcel dest, int flags) {
        dest.writeInt(mEventType);
        dest.writeInt(mErrorCode);
        dest.writeParcelable(mCarPropertyValue, flags);
    }

    public static final Parcelable.Creator<CarPropertyEvent> CREATOR =
            new Parcelable.Creator<CarPropertyEvent>() {

        @Override
        public CarPropertyEvent createFromParcel(Parcel in) {
            return new CarPropertyEvent(in);
        }

        @Override
        public CarPropertyEvent[] newArray(int size) {
            return new CarPropertyEvent[size];
        }
        };

    /**
     * Constructor for {@link CarPropertyEvent}.
     */
    public CarPropertyEvent(int eventType, @NonNull CarPropertyValue<?> carPropertyValue) {
        mEventType  = eventType;
        mErrorCode = CarPropertyManager.CAR_SET_PROPERTY_ERROR_CODE_UNKNOWN;
        mCarPropertyValue = carPropertyValue;
    }

    /**
     * Constructor for {@link CarPropertyEvent} with an error code.
     */
    public CarPropertyEvent(int eventType, @NonNull CarPropertyValue<?> carPropertyValue,
            @CarPropertyManager.CarSetPropertyErrorCode int errorCode) {
        mEventType  = eventType;
        mErrorCode = errorCode;
        mCarPropertyValue = carPropertyValue;
    }


    /**
     * Constructor for {@link CarPropertyEvent} when it is an error event.
     *
     * The status of {@link CarPropertyValue} should be {@link CarPropertyValue#STATUS_ERROR}.
     * In {@link CarPropertyManager}, the value of {@link CarPropertyValue} will be dropped.
     */
    public static CarPropertyEvent createErrorEventWithErrorCode(int propertyId, int areaId,
            @CarPropertyManager.CarSetPropertyErrorCode int errorCode) {
        CarPropertyValue<Integer> valueWithErrorCode = new CarPropertyValue<>(propertyId, areaId,
                CarPropertyValue.STATUS_ERROR, 0, ERROR_EVENT_VALUE);
        CarPropertyEvent event = new CarPropertyEvent(PROPERTY_EVENT_ERROR, valueWithErrorCode,
                errorCode);
        return event;
    }

    public @CarPropertyManager.CarSetPropertyErrorCode int getErrorCode() {
        return mErrorCode;
    }

    private CarPropertyEvent(Parcel in) {
        mEventType  = in.readInt();
        mErrorCode = in.readInt();
        mCarPropertyValue = in.readParcelable(CarPropertyValue.class.getClassLoader());
    }

    @Override
    public String toString() {
        return "CarPropertyEvent{"
                + "mEventType=" + mEventType
                + ", mErrorCode=" + mErrorCode
                + ", mCarPropertyValue=" + mCarPropertyValue
                + '}';
    }
}
