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

import static org.mockito.Mockito.when;

import android.net.Uri;
import android.telephony.TelephonyManager;

import com.google.common.collect.Iterables;

import org.junit.Before;
import org.junit.Test;
import org.mockito.Mockito;

import java.io.UnsupportedEncodingException;
import java.util.List;
import java.util.Locale;

public class EventDescriptionsTest {
    private static final String BASE_NUMBER = "30 303986300";
    private static final String LOCAL_NUMBER = "0" + BASE_NUMBER;
    private static final String INTERNATIONAL_NUMBER = "+49 " + BASE_NUMBER;
    private static final String ACCESS = ",,12;3*45#";
    private static final String COUNTRY_ISO_CODE = "DE";

    private EventDescriptions mEventDescriptions;
    private TelephonyManager mMockTelephonyManager;

    @Before
    public void setUp() {
        mMockTelephonyManager = Mockito.mock(TelephonyManager.class);
        when(mMockTelephonyManager.getNetworkCountryIso()).thenReturn(COUNTRY_ISO_CODE);
        mEventDescriptions = new EventDescriptions(Locale.GERMANY, mMockTelephonyManager);
    }

    @Test
    public void extractNumberAndPin_localNumber_resultIsLocal() {
        List<Dialer.NumberAndAccess> numberAndAccesses =
                mEventDescriptions.extractNumberAndPins(LOCAL_NUMBER);
        assertThat(numberAndAccesses).isNotEmpty();
        Dialer.NumberAndAccess numberAndAccess = Iterables.getFirst(numberAndAccesses, null);
        assertThat(numberAndAccess.getNumber()).isEqualTo(LOCAL_NUMBER);
    }

    @Test
    public void extractNumberAndPin_internationalNumber_resultIsLocal() {
        List<Dialer.NumberAndAccess> numberAndAccesses =
                mEventDescriptions.extractNumberAndPins(INTERNATIONAL_NUMBER);
        assertThat(numberAndAccesses).isNotEmpty();
        Dialer.NumberAndAccess numberAndAccess = Iterables.getFirst(numberAndAccesses, null);
        assertThat(numberAndAccess.getNumber()).isEqualTo(INTERNATIONAL_NUMBER);
    }

    @Test
    public void extractNumberAndPin_internationalNumberWithDifferentLocale_resultIsInternational() {
        EventDescriptions eventDescriptions =
                new EventDescriptions(Locale.FRANCE, mMockTelephonyManager);
        List<Dialer.NumberAndAccess> numberAndAccesses =
                eventDescriptions.extractNumberAndPins(INTERNATIONAL_NUMBER);
        assertThat(numberAndAccesses).isNotEmpty();
        Dialer.NumberAndAccess numberAndAccess = Iterables.getFirst(numberAndAccesses, null);
        assertThat(numberAndAccess.getNumber()).isEqualTo(INTERNATIONAL_NUMBER);
    }

    @Test
    public void extractNumberAndPin_internationalNumberAndPin() {
        String input = INTERNATIONAL_NUMBER + " PIN: " + ACCESS;
        List<Dialer.NumberAndAccess> numberAndAccesses =
                mEventDescriptions.extractNumberAndPins(input);
        assertThat(numberAndAccesses).isNotEmpty();
        Dialer.NumberAndAccess numberAndAccess = Iterables.getFirst(numberAndAccesses, null);
        assertThat(numberAndAccess.getNumber()).isEqualTo(INTERNATIONAL_NUMBER);
        assertThat(numberAndAccess.getAccess()).isEqualTo(ACCESS);
    }

    @Test
    public void extractNumberAndPin_internationalNumberAndCode() {
        String input = INTERNATIONAL_NUMBER + " with access code " + ACCESS;
        List<Dialer.NumberAndAccess> numberAndAccesses =
                mEventDescriptions.extractNumberAndPins(input);
        assertThat(numberAndAccesses).isNotEmpty();
        Dialer.NumberAndAccess numberAndAccess = Iterables.getFirst(numberAndAccesses, null);
        assertThat(numberAndAccess.getNumber()).isEqualTo(INTERNATIONAL_NUMBER);
        assertThat(numberAndAccess.getAccess()).isEqualTo(ACCESS);
    }

    @Test
    public void extractNumberAndPin_multipleNumbers() {
        String input =
                INTERNATIONAL_NUMBER
                        + " PIN: "
                        + ACCESS
                        + "\n  an invalid one is "
                        + BASE_NUMBER
                        + " but a local one is "
                        + LOCAL_NUMBER;
        List<Dialer.NumberAndAccess> numberAndAccesses =
                mEventDescriptions.extractNumberAndPins(input);

        // Keep all variations of a base number.
        assertThat(numberAndAccesses).hasSize(3);
    }

    @Test
    public void extractNumberAndPin_encodedTelFormat() throws UnsupportedEncodingException {
        String encoded = Uri.encode(INTERNATIONAL_NUMBER + ACCESS);
        String input = "blah blah <tel:" + encoded + "> blah blah";
        List<Dialer.NumberAndAccess> numberAndAccesses =
                mEventDescriptions.extractNumberAndPins(input);
        assertThat(numberAndAccesses).hasSize(1);
        Dialer.NumberAndAccess numberAndAccess = Iterables.getFirst(numberAndAccesses, null);
        assertThat(numberAndAccess.getNumber()).isEqualTo(INTERNATIONAL_NUMBER);
        assertThat(numberAndAccess.getAccess()).isEqualTo(ACCESS);
    }

    @Test
    public void extractNumberAndPin_smallNumber_returnsNull() throws UnsupportedEncodingException {
        String input = "blah blah 345 - blah blah";
        List<Dialer.NumberAndAccess> numberAndAccesses =
                mEventDescriptions.extractNumberAndPins(input);
        assertThat(numberAndAccesses).isEmpty();
    }
}
