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
package android.car.test.mocks;

import static org.mockito.ArgumentMatchers.argThat;

import android.annotation.Nullable;
import android.annotation.UserIdInt;
import android.content.pm.UserInfo;
import android.hardware.automotive.vehicle.V2_0.VehiclePropValue;
import android.os.UserHandle;
import android.util.Log;

import org.mockito.ArgumentMatcher;

import java.util.Arrays;

/**
 * Provides custom mockito matcher for Android / Car objects.
 *
 */
public final class CarArgumentMatchers {

    private static final String TAG = CarArgumentMatchers.class.getSimpleName();

    /**
     * Matches if a {@link UserInfo} has the given {@code userId}.
     */
    public static UserInfo isUserInfo(@UserIdInt int userId) {
        return argThat(new UserInfoMatcher(userId));
    }

    /**
     * Matches if a {@link UserHandle} has the given {@code userId}.
     */
    public static UserHandle isUserHandle(@UserIdInt int userId) {
        return argThat(new UserHandleMatcher(userId));
    }

    /**
     * Matches if a {@link VehiclePropValue} has the given {@code prop}.
     */
    public static VehiclePropValue isProperty(int prop) {
        return argThat(new PropertyIdMatcher(prop));
    }

    /**
     * Matches if a {@link VehiclePropValue} has the given {@code prop} and {@code int32} values.
     */
    public static VehiclePropValue isPropertyWithValues(int prop, int...values) {
        return argThat(new PropertyIdMatcher(prop, values));
    }

    private static class UserInfoMatcher implements ArgumentMatcher<UserInfo> {

        public final @UserIdInt int userId;

        private UserInfoMatcher(@UserIdInt int userId) {
            this.userId = userId;
        }

        @Override
        public boolean matches(@Nullable UserInfo argument) {
            if (argument == null) {
                Log.w(TAG, "isUserInfo(): null argument");
                return false;
            }
            if (argument.id != userId) {
                Log.w(TAG, "isUserInfo(): argument mismatch (expected " + userId
                        + ", got " + argument.id);
                return false;
            }
            return true;
        }

        @Override
        public String toString() {
            return "isUserInfo(userId=" + userId + ")";
        }
    }

    private static class UserHandleMatcher implements ArgumentMatcher<UserHandle> {

        public final @UserIdInt int userId;

        private UserHandleMatcher(@UserIdInt int userId) {
            this.userId = userId;
        }

        @Override
        public boolean matches(UserHandle argument) {
            if (argument == null) {
                Log.w(TAG, "isUserHandle(): null argument");
                return false;
            }
            if (argument.getIdentifier() != userId) {
                Log.w(TAG, "isUserHandle(): argument mismatch (expected " + userId
                        + ", got " + argument.getIdentifier());
                return false;
            }
            return true;
        }

        @Override
        public String toString() {
            return "isUserHandle(userId=" + userId + ")";
        }
    }

    private static class PropertyIdMatcher implements ArgumentMatcher<VehiclePropValue> {

        final int mProp;
        private final int[] mValues;

        private PropertyIdMatcher(int prop) {
            this(prop, null);
        }

        private PropertyIdMatcher(int prop, int[] values) {
            mProp = prop;
            mValues = values;
        }

        @Override
        public boolean matches(VehiclePropValue argument) {
            Log.v(TAG, "PropertyIdMatcher: argument=" + argument);
            if (argument.prop != mProp) {
                Log.w(TAG, "PropertyIdMatcher: Invalid prop on " + argument);
                return false;
            }
            if (mValues == null) return true;
            // Make sure values match
            if (mValues.length != argument.value.int32Values.size()) {
                Log.w(TAG, "PropertyIdMatcher: number of values (expected " + mValues.length
                        + ") mismatch on " + argument);
                return false;
            }

            for (int i = 0; i < mValues.length; i++) {
                if (mValues[i] != argument.value.int32Values.get(i)) {
                    Log.w(TAG, "PropertyIdMatcher: value mismatch at index " + i + " on " + argument
                            + ": expected " + Arrays.toString(mValues));
                    return false;
                }
            }
            return true;
        }

        @Override
        public String toString() {
            return "prop: " + mProp + " values: " + Arrays.toString(mValues);
        }
    }

    private CarArgumentMatchers() {
        throw new UnsupportedOperationException("contains only static methods");
    }
}
