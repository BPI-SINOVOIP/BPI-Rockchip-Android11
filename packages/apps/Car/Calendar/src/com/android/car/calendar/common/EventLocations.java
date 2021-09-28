/*
 * Copyright 2020 The Android Open Source Project
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

package com.android.car.calendar.common;

import java.util.regex.Pattern;

/** Utilities operating on the event location field. */
public class EventLocations {
    private static final Pattern ROOM_LOCATION_PATTERN =
            Pattern.compile("^[A-Z]{2,4}(?:-[0-9A-Z]{1,5}){2,}");

    /** Returns true if the location is valid for navigation. */
    public boolean isValidLocation(String locationText) {
        return !ROOM_LOCATION_PATTERN.matcher(locationText).find();
    }
}
