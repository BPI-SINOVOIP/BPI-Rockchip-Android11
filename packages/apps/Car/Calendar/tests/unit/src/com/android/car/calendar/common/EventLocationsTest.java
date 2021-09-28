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

import static com.google.common.truth.Truth.assertThat;

import org.junit.Before;
import org.junit.Test;

public class EventLocationsTest {

    private static final String BASE_NUMBER = "30 303986300";

    private EventLocations mEventLocations;

    @Before
    public void setUp() {
        mEventLocations = new EventLocations();
    }

    @Test
    public void isValidLocation_meetingRooms_isFalse() {
        assertThat(mEventLocations.isValidLocation("MUC-ARP-6Z3-Radln (5) [GVC]")).isFalse();
        assertThat(mEventLocations.isValidLocation("SFO-SPE-3-Anchor Brewing Co. (1) [GVC]"))
                .isFalse();
        assertThat(mEventLocations.isValidLocation("SFO-SPE-3-Speakeasy Ales & Lagers (10) [GVC]"))
                .isFalse();
        assertThat(
                        mEventLocations.isValidLocation(
                                "MTV-900-1-Good Charlotte (13) [GVC, No External Guests]"))
                .isFalse();
        assertThat(
                        mEventLocations.isValidLocation(
                                "MTV-900-2-Panic! at the Disco (5) [GVC, Jamboard]"))
                .isFalse();
        assertThat(mEventLocations.isValidLocation("US-MTV-900-1-1F2 (collaboration area)"))
                .isFalse();
    }

    @Test
    public void isValidLocation_notMeetingRooms_isTrue() {
        assertThat(mEventLocations.isValidLocation("My place")).isTrue();
        assertThat(mEventLocations.isValidLocation("At JDP-1974-09")).isTrue();
        assertThat(mEventLocations.isValidLocation("178.3454, 234.345")).isTrue();
    }
}
