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

package com.android.server.wifi;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import androidx.test.filters.SmallTest;

import org.junit.Test;

import java.util.HashMap;
import java.util.Map;

/**
 * Unit tests for {@link com.android.server.wifi.IMSIParameter}.
 */
@SmallTest
public class IMSIParameterTest extends WifiBaseTest {
    private static final String VALID_FULL_IMSI = "123456789012345";
    private static final String VALID_MCC_MNC = "12345";
    private static final String VALID_MCC_MNC_6 = "123456";
    private static final String INVALID_MCC_MNC = "1234";

    /**
     * Data points for testing function {@link IMSIParameter#build}.
     */
    private static final Map<String, IMSIParameter> BUILD_PARAM_TEST_MAP = new HashMap<>();
    static {
        BUILD_PARAM_TEST_MAP.put(null, null);
        BUILD_PARAM_TEST_MAP.put("", null);
        BUILD_PARAM_TEST_MAP.put("1234a123", null);    // IMSI contained invalid character.
        BUILD_PARAM_TEST_MAP.put("1234567890123451", null);   // IMSI is over full IMSI length.
        BUILD_PARAM_TEST_MAP.put("123456789012345", new IMSIParameter("123456789012345", false));
        BUILD_PARAM_TEST_MAP.put("1234*", null);
        BUILD_PARAM_TEST_MAP.put("12345*", new IMSIParameter("12345", true));
        BUILD_PARAM_TEST_MAP.put("123456*", new IMSIParameter("123456", true));
        BUILD_PARAM_TEST_MAP.put("1234567*", null);
    }

    /**
     * Verify the expectations of {@link IMSIParameter#build} function using the predefined
     * test data {@link #BUILD_PARAM_TEST_MAP}.
     *
     * @throws Exception
     */
    @Test
    public void verifyBuildIMSIParameter() throws Exception {
        for (Map.Entry<String, IMSIParameter> entry : BUILD_PARAM_TEST_MAP.entrySet()) {
            assertEquals(entry.getValue(), IMSIParameter.build(entry.getKey()));
        }
    }

    /**
     * Verify that attempt to match a null IMSI will not cause any crash and should return false.
     *
     * @throws Exception
     */
    @Test
    public void matchesNullImsi() throws Exception {
        IMSIParameter param = new IMSIParameter("12345", false);
        assertFalse(param.matchesImsi(null));
    }

    /**
     * Verify that an IMSIParameter containing a full IMSI will only match against an IMSI of the
     * same value.
     *
     * @throws Exception
     */
    @Test
    public void matchesImsiWithFullImsi() throws Exception {
        IMSIParameter param = new IMSIParameter(VALID_FULL_IMSI, false);

        // Full IMSI requires exact matching.
        assertFalse(param.matchesImsi(INVALID_MCC_MNC));
        assertFalse(param.matchesImsi(VALID_MCC_MNC));
        assertTrue(param.matchesImsi(VALID_FULL_IMSI));
    }

    /**
     * Verify that an IMSIParameter containing a prefix IMSI will match against any IMSI that
     * starts with the same prefix.
     *
     * @throws Exception
     */
    @Test
    public void matchesImsiWithPrefixImsi() throws Exception {
        IMSIParameter param = new IMSIParameter(VALID_MCC_MNC, true);

        // Prefix IMSI will match any full IMSI that starts with the same prefix.
        assertFalse(param.matchesImsi(INVALID_MCC_MNC));
        assertTrue(param.matchesImsi(VALID_FULL_IMSI));
    }

    /**
     * Verify that attempt to match a null MCC-MNC will not cause any crash and should return
     * false.
     *
     * @throws Exception
     */
    @Test
    public void matchesNullMccMnc() throws Exception {
        IMSIParameter param = new IMSIParameter(VALID_MCC_MNC, true);
        assertFalse(param.matchesMccMnc(null));
    }

    /**
     * Verify that an IMSIParameter containing a full IMSI will only match against a 6 digit
     * MCC-MNC that is a prefix of the IMSI.
     *
     * @throws Exception
     */
    @Test
    public void matchesMccMncFullImsi() throws Exception {
        IMSIParameter param = new IMSIParameter(VALID_FULL_IMSI, false);

        assertFalse(param.matchesMccMnc(INVALID_MCC_MNC));    // Invalid length for MCC-MNC
        assertTrue(param.matchesMccMnc(VALID_MCC_MNC));
        assertTrue(param.matchesMccMnc(VALID_MCC_MNC_6));
    }

    /**
     * Verify that an IMSIParameter containing an IMSI prefix whose length is different from
     * that of matching MccMnc, it should return false.
     *
     * @throws Exception
     */
    @Test
    public void matchesMccMncWithPrefixImsiWithDifferentMccMncLength() throws Exception {
        IMSIParameter param = new IMSIParameter(VALID_MCC_MNC, true);

        assertFalse(param.matchesMccMnc(VALID_MCC_MNC_6));     // length of Prefix mismatch
    }
}
