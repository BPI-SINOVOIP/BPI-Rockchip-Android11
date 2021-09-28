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

package com.android.server.wifi.hotspot2;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import android.net.wifi.EAPConstants;

import androidx.test.filters.SmallTest;

import com.android.server.wifi.IMSIParameter;
import com.android.server.wifi.WifiBaseTest;
import com.android.server.wifi.hotspot2.anqp.CellularNetwork;
import com.android.server.wifi.hotspot2.anqp.DomainNameElement;
import com.android.server.wifi.hotspot2.anqp.NAIRealmData;
import com.android.server.wifi.hotspot2.anqp.NAIRealmElement;
import com.android.server.wifi.hotspot2.anqp.RoamingConsortiumElement;
import com.android.server.wifi.hotspot2.anqp.ThreeGPPNetworkElement;
import com.android.server.wifi.hotspot2.anqp.eap.AuthParam;
import com.android.server.wifi.hotspot2.anqp.eap.EAPMethod;
import com.android.server.wifi.hotspot2.anqp.eap.NonEAPInnerAuth;

import org.junit.Test;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.HashMap;
import java.util.HashSet;
import java.util.Map;
import java.util.Set;

/**
 * Unit tests for {@link com.android.server.wifi.hotspot2.ANQPMatcher}.
 */
@SmallTest
public class ANQPMatcherTest extends WifiBaseTest {
    /**
     * Verify that domain name match will fail when a null Domain Name ANQP element is provided.
     *
     * @throws Exception
     */
    @Test
    public void matchDomainNameWithNullElement() throws Exception {
        assertFalse(ANQPMatcher.matchDomainName(null, "test.com", null, null));
    }

    /**
     * Verify that domain name match will succeed when the specified FQDN matches a domain name
     * in the Domain Name ANQP element.
     *
     * @throws Exception
     */
    @Test
    public void matchDomainNameUsingFQDN() throws Exception {
        String fqdn = "test.com";
        String[] domains = new String[] {fqdn};
        DomainNameElement element = new DomainNameElement(Arrays.asList(domains));
        assertTrue(ANQPMatcher.matchDomainName(element, fqdn, null, null));
    }

    /**
     * Verify that domain name match will succeed when the specified IMSI parameter and IMSI list
     * matches a 3GPP network domain in the Domain Name ANQP element.
     *
     * @throws Exception
     */
    @Test
    public void matchDomainNameUsingIMSI() throws Exception {
        IMSIParameter imsiParam = new IMSIParameter("123456", true);
        String simImsi = "123456789012345";
        // 3GPP network domain with MCC=123 and MNC=456.
        String[] domains = new String[] {"wlan.mnc456.mcc123.3gppnetwork.org"};
        DomainNameElement element = new DomainNameElement(Arrays.asList(domains));
        assertTrue(ANQPMatcher.matchDomainName(element, null, imsiParam, simImsi));
    }

    /**
     * Verify that roaming consortium match will fail when a null Roaming Consortium ANQP
     * element is provided.
     *
     * @throws Exception
     */
    @Test
    public void matchRoamingConsortiumWithNullElement() throws Exception {
        assertFalse(ANQPMatcher.matchRoamingConsortium(null, new long[0], false));
    }

    /**
     * Verify that a roaming consortium match will succeed when the specified OI matches
     * an OI in the Roaming Consortium ANQP element.
     *
     * @throws Exception
     */
    @Test
    public void matchRoamingConsortium() throws Exception {
        long oi = 0x1234L;
        RoamingConsortiumElement element =
                new RoamingConsortiumElement(Arrays.asList(new Long[] {oi}));
        assertTrue(ANQPMatcher.matchRoamingConsortium(element, new long[] {oi}, false));
    }

    /**
     * Verify that no match will be returned when matching a null NAI Realm
     * ANQP element.
     *
     * @throws Exception
     */
    @Test
    public void matchNAIRealmWithNullElement() throws Exception {
        assertFalse(ANQPMatcher.matchNAIRealm(null, "test.com"));
    }

    /**
     * Verify that no match will be returned when matching a NAI Realm
     * ANQP element contained no NAI realm data.
     *
     * @throws Exception
     */
    @Test
    public void matchNAIRealmWithEmtpyRealmData() throws Exception {
        NAIRealmElement element = new NAIRealmElement(new ArrayList<NAIRealmData>());
        assertFalse(ANQPMatcher.matchNAIRealm(element, "test.com"));
    }

    /**
     * Verify that a realm match will be returned when the specified realm matches a realm
     * in the NAI Realm ANQP element with no EAP methods.
     *
     * @throws Exception
     */
    @Test
    public void matchNAIRealmWithRealmMatch() throws Exception {
        String realm = "test.com";
        NAIRealmData realmData = new NAIRealmData(
                Arrays.asList(new String[] {realm}), new ArrayList<EAPMethod>());
        NAIRealmElement element = new NAIRealmElement(
                Arrays.asList(new NAIRealmData[] {realmData}));
        assertTrue(ANQPMatcher.matchNAIRealm(element, realm));
    }

    /**
     * Verify that a realm match will be returned when the specified realm and EAP
     * method matches a realm in the NAI Realm ANQP element.
     *
     * @throws Exception
     */
    @Test
    public void matchNAIRealmWithRealmMethodMatch() throws Exception {
        // Test data.
        String realm = "test.com";
        int eapMethodID = EAPConstants.EAP_TLS;

        // Setup NAI Realm element.
        EAPMethod method = new EAPMethod(eapMethodID, new HashMap<Integer, Set<AuthParam>>());
        NAIRealmData realmData = new NAIRealmData(
                Arrays.asList(new String[] {realm}), Arrays.asList(new EAPMethod[] {method}));
        NAIRealmElement element = new NAIRealmElement(
                Arrays.asList(new NAIRealmData[] {realmData}));

        assertTrue(ANQPMatcher.matchNAIRealm(element, realm));
    }

    /**
     * Verify that a realm match will be returned when the specified realm, EAP
     * method, and the authentication parameter matches a realm with the associated EAP method and
     * authentication parameter in the NAI Realm ANQP element.
     *
     * @throws Exception
     */
    @Test
    public void matchNAIRealmWithExactMatch() throws Exception {
        // Test data.
        String realm = "test.com";
        int eapMethodID = EAPConstants.EAP_TTLS;
        NonEAPInnerAuth authParam = new NonEAPInnerAuth(NonEAPInnerAuth.AUTH_TYPE_MSCHAP);
        Set<AuthParam> authSet = new HashSet<>();
        authSet.add(authParam);
        Map<Integer, Set<AuthParam>> authMap = new HashMap<>();
        authMap.put(authParam.getAuthTypeID(), authSet);

        // Setup NAI Realm element.
        EAPMethod method = new EAPMethod(eapMethodID, authMap);
        NAIRealmData realmData = new NAIRealmData(
                Arrays.asList(new String[] {realm}), Arrays.asList(new EAPMethod[] {method}));
        NAIRealmElement element = new NAIRealmElement(
                Arrays.asList(new NAIRealmData[] {realmData}));

        assertTrue(ANQPMatcher.matchNAIRealm(element, realm));
    }

    /**
     * Verify that a realm match will be returned when the specified EAP method
     * doesn't match with the corresponding EAP method in the NAI Realm ANQP element.
     *
     * @throws Exception
     */
    @Test
    public void matchNAIRealmWithEAPMethodMismatch() throws Exception {
        // Test data.
        String realm = "test.com";
        int eapMethodID = EAPConstants.EAP_TTLS;
        NonEAPInnerAuth authParam = new NonEAPInnerAuth(NonEAPInnerAuth.AUTH_TYPE_MSCHAP);
        Set<AuthParam> authSet = new HashSet<>();
        authSet.add(authParam);
        Map<Integer, Set<AuthParam>> authMap = new HashMap<>();
        authMap.put(authParam.getAuthTypeID(), authSet);

        // Setup NAI Realm element.
        EAPMethod method = new EAPMethod(eapMethodID, authMap);
        NAIRealmData realmData = new NAIRealmData(
                Arrays.asList(new String[] {realm}), Arrays.asList(new EAPMethod[] {method}));
        NAIRealmElement element = new NAIRealmElement(
                Arrays.asList(new NAIRealmData[] {realmData}));

        assertTrue(ANQPMatcher.matchNAIRealm(element, realm));
    }

    /**
     * Verify that a realm match will be returned when the specified authentication
     * parameter doesn't match with the corresponding authentication parameter in the NAI Realm
     * ANQP element.
     *
     * @throws Exception
     */
    @Test
    public void matchNAIRealmWithAuthTypeMismatch() throws Exception {
        // Test data.
        String realm = "test.com";
        int eapMethodID = EAPConstants.EAP_TTLS;
        NonEAPInnerAuth authParam = new NonEAPInnerAuth(NonEAPInnerAuth.AUTH_TYPE_MSCHAP);
        Set<AuthParam> authSet = new HashSet<>();
        authSet.add(authParam);
        Map<Integer, Set<AuthParam>> authMap = new HashMap<>();
        authMap.put(authParam.getAuthTypeID(), authSet);

        // Setup NAI Realm element.
        EAPMethod method = new EAPMethod(eapMethodID, authMap);
        NAIRealmData realmData = new NAIRealmData(
                Arrays.asList(new String[] {realm}), Arrays.asList(new EAPMethod[] {method}));
        NAIRealmElement element = new NAIRealmElement(
                Arrays.asList(new NAIRealmData[] {realmData}));

        // Mismatch in authentication type which we ignore.
        assertTrue(ANQPMatcher.matchNAIRealm(element, realm));
    }

    /**
     * Verify that 3GPP Network match will fail when a null element is provided.
     *
     * @throws Exception
     */
    @Test
    public void matchThreeGPPNetworkWithNullElement() throws Exception {
        IMSIParameter imsiParam = new IMSIParameter("12345", true);
        String simImsi = "123456789012345";
        assertFalse(ANQPMatcher.matchThreeGPPNetwork(null, imsiParam, simImsi));
    }

    /**
     * Verify that 3GPP network will succeed when the given 3GPP Network ANQP element contained
     * a MCC-MNC that matches the both IMSI parameter and a SIM IMSI.
     *
     * @throws Exception
     */
    @Test
    public void matchThreeGPPNetwork() throws Exception {
        IMSIParameter imsiParam = new IMSIParameter("123456", true);
        String simImsi = "123456789012345";

        CellularNetwork network = new CellularNetwork(Arrays.asList(new String[] {"123456"}));
        ThreeGPPNetworkElement element =
                new ThreeGPPNetworkElement(Arrays.asList(new CellularNetwork[] {network}));
        // The MCC-MNC provided in 3GPP Network ANQP element matches both IMSI parameter
        // and an IMSI from the installed SIM card.
        assertTrue(ANQPMatcher.matchThreeGPPNetwork(element, imsiParam, simImsi));
    }

    /**
     * Verify that 3GPP network will succeed when the given 3GPP Network ANQP element contained
     * a MCC-MNC that matches the both IMSI parameter and a SIM IMSI contains 5 digits mccmnc.
     *
     * @throws Exception
     */
    @Test
    public void matchThreeGPPNetworkWith5DigitsMccMnc() throws Exception {
        IMSIParameter imsiParam = new IMSIParameter("12345", true);
        String simImsi = "123456789012345";

        CellularNetwork network = new CellularNetwork(Arrays.asList(new String[] {"12345"}));
        ThreeGPPNetworkElement element =
                new ThreeGPPNetworkElement(Arrays.asList(new CellularNetwork[] {network}));
        // The MCC-MNC provided in 3GPP Network ANQP element matches both IMSI parameter
        // and an IMSI from the installed SIM card.
        assertTrue(ANQPMatcher.matchThreeGPPNetwork(element, imsiParam, simImsi));
    }

    /**
     * Verify that 3GPP network will failed when the given 3GPP Network ANQP element contained
     * a MCC-MNC that match the IMSI parameter but not the SIM IMSI.
     *
     * @throws Exception
     */
    @Test
    public void matchThreeGPPNetworkWithoutSimImsiMatch() throws Exception {
        IMSIParameter imsiParam = new IMSIParameter("123457", true);
        String simImsi = "123456789012345";

        CellularNetwork network = new CellularNetwork(Arrays.asList(new String[] {"123457"}));
        ThreeGPPNetworkElement element =
                new ThreeGPPNetworkElement(Arrays.asList(new CellularNetwork[] {network}));
        // The MCC-MNC provided in 3GPP Network ANQP element doesn't match any of the IMSIs
        // from the installed SIM card.
        assertFalse(ANQPMatcher.matchThreeGPPNetwork(element, imsiParam, simImsi));
    }

    /**
     * Verify that 3GPP network will failed when the given 3GPP Network ANQP element contained
     * a MCC-MNC that doesn't match with the IMSI parameter.
     *
     * @throws Exception
     */
    @Test
    public void matchThreeGPPNetworkWithImsiParamMismatch() throws Exception {
        IMSIParameter imsiParam = new IMSIParameter("12345", true);
        String simImsi = "123456789012345";

        CellularNetwork network = new CellularNetwork(Arrays.asList(new String[] {"123356"}));
        ThreeGPPNetworkElement element =
                new ThreeGPPNetworkElement(Arrays.asList(new CellularNetwork[] {network}));
        // The MCC-MNC provided in 3GPP Network ANQP element doesn't match the IMSI parameter.
        assertFalse(ANQPMatcher.matchThreeGPPNetwork(element, imsiParam, simImsi));
    }

    /**
     * Verify that domain name match will fail when domain contains invalid values.
     *
     * @throws Exception
     */
    @Test
    public void verifyInvalidDomain() throws Exception {
        IMSIParameter imsiParam = new IMSIParameter("123456", true);
        String simImsi = "123456789012345";
        // 3GPP network domain with MCC=123 and MNC=456.
        String[] domains = new String[] {"wlan.mnc456.mccI23.3gppnetwork.org"};
        DomainNameElement element = new DomainNameElement(Arrays.asList(domains));
        assertFalse(ANQPMatcher.matchDomainName(element, null, imsiParam, simImsi));
    }

    /**
     * Verify that match is found when HomeOI contains some of the RCOIs advertised by an AP marked
     * as not required.
     *
     * @throws Exception
     */
    @Test
    public void matchAnyHomeOi() throws Exception {
        long[] providerOis = new long[] {0x1234L, 0x5678L, 0xabcdL};
        Long[] anqpOis = new Long[] {0x1234L, 0x5678L, 0xdeadL, 0xf0cdL};
        RoamingConsortiumElement element =
                new RoamingConsortiumElement(Arrays.asList(anqpOis));
        assertTrue(ANQPMatcher.matchRoamingConsortium(element, providerOis, false));
    }

    /**
     * Verify that no match is found when HomeOI does not contain any of the RCOIs advertised by an
     * AP marked as not required.
     *
     * @throws Exception
     */
    @Test
    public void matchAnyHomeOiNegative() throws Exception {
        long[] providerOis = new long[] {0x1234L, 0x5678L, 0xabcdL};
        Long[] anqpOis = new Long[] {0xabc2L, 0x1232L};
        RoamingConsortiumElement element =
                new RoamingConsortiumElement(Arrays.asList(anqpOis));
        assertFalse(ANQPMatcher.matchRoamingConsortium(element, providerOis, false));
    }

    /**
     * Verify that match is found when HomeOI contains all of the RCOIs advertised by an AP marked
     * as required.
     *
     * @throws Exception
     */
    @Test
    public void matchAllHomeOi() throws Exception {
        long[] providerOis = new long[] {0x1234L, 0x5678L, 0xabcdL};
        Long[] anqpOis = new Long[] {0x1234L, 0x5678L, 0xabcdL, 0xdeadL, 0xf0cdL};
        RoamingConsortiumElement element =
                new RoamingConsortiumElement(Arrays.asList(anqpOis));
        assertTrue(ANQPMatcher.matchRoamingConsortium(element, providerOis, true));
    }

    /**
     * Verify that match is not found when HomeOI does not contain all of the RCOIs advertised by an
     * AP marked as required.
     *
     * @throws Exception
     */
    @Test
    public void matchAllHomeOiNegative() throws Exception {
        long[] providerOis = new long[] {0x1234L, 0x5678L, 0xabcdL};
        Long[] anqpOis = new Long[] {0x1234L, 0x5678L, 0xdeadL, 0xf0cdL};
        RoamingConsortiumElement element =
                new RoamingConsortiumElement(Arrays.asList(anqpOis));
        assertFalse(ANQPMatcher.matchRoamingConsortium(element, providerOis, true));
    }
}
