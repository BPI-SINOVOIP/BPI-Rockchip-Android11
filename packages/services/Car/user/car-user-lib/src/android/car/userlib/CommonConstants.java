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
package android.car.userlib;

/**
 * Provides constants used by both CarService and CarServiceHelper packages.
 */
public final class CommonConstants {

    private CommonConstants() {
        throw new UnsupportedOperationException("contains only static constants");
    }

    /**
     * Constants used on {@link android.os.Bundle bundles} sent by
     * {@link com.android.car.user.CarUserService} on binder calls.
     */
    public static final class CarUserServiceConstants {

        public static final String BUNDLE_USER_ID = "user.id";
        public static final String BUNDLE_USER_FLAGS = "user.flags";
        public static final String BUNDLE_USER_NAME = "user.name";
        public static final String BUNDLE_USER_LOCALES = "user.locales";
        public static final String BUNDLE_INITIAL_INFO_ACTION = "initial_info.action";

        private CarUserServiceConstants() {
            throw new UnsupportedOperationException("contains only static constants");
        }
    }
}
