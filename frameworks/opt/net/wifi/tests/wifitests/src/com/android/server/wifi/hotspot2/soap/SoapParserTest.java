/*
 * Copyright (C) 2018 The Android Open Source Project
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

package com.android.server.wifi.hotspot2.soap;

import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.MockitoAnnotations.initMocks;

import androidx.test.filters.SmallTest;

import com.android.server.wifi.WifiBaseTest;

import org.junit.Before;
import org.junit.Test;
import org.ksoap2.serialization.PropertyInfo;
import org.ksoap2.serialization.SoapObject;

/**
 * Unit tests for {@link SoapParser}.
 */
@SmallTest
public class SoapParserTest extends WifiBaseTest {
    private static final String EXEC = "exec";
    private static final String BROWSER_COMMAND = "launchBrowserToURI";
    private static final String TEST_URL = "https://127.0.0.1:12345/index.htm";
    private static final String TEST_STATUS = "OK";
    private static final String TEST_SESSION_ID = "D215D696517BA138F1D28442DF0F4E07";
    private static final String TEST_VERSION = "1.0";

    /**
     * Sets up test.
     */
    @Before
    public void setUp() throws Exception {
        initMocks(this);
    }

    /**
     * Verify if the invalid message type SppSoapResponse message will return {@code null}
     */
    @Test
    public void verifyInvalidMessageType() {
        SoapObject response = new SoapObject("Spp", "InvalidMessageType");
        assertNull(SoapParser.getResponse(response));
    }

    /**
     * Verify if the valid sppPostDevDataResponse message will return {@link PostDevDataResponse}
     */
    @Test
    public void verifyValidSppPostDevDataResponse() {
        PropertyInfo propertyInfo = new PropertyInfo();
        SoapObject subCommand = new SoapObject();
        propertyInfo.setName(EXEC);
        propertyInfo.setValue(subCommand);
        subCommand.addProperty(BROWSER_COMMAND, TEST_URL);

        SoapObject response = new SoapObject("Spp", "sppPostDevDataResponse");
        response.addAttribute(SppResponseMessage.SPPStatusAttribute, TEST_STATUS);
        response.addAttribute(SppResponseMessage.SPPSessionIDAttribute, TEST_SESSION_ID);
        response.addAttribute(SppResponseMessage.SPPVersionAttribute, TEST_VERSION);
        response.addProperty(propertyInfo);

        SppResponseMessage result = SoapParser.getResponse(response);

        assertTrue(result instanceof PostDevDataResponse);
    }
}
