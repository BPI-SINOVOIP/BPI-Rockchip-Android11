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

package android.nfc.cts;

import static org.junit.Assert.*;
import static org.junit.Assume.assumeTrue;

import android.content.pm.PackageManager;
import android.content.ComponentName;
import android.content.Context;
import android.nfc.NfcAdapter;
import android.nfc.cardemulation.CardEmulation;
import android.provider.Settings;
import android.util.Log;

import androidx.test.InstrumentationRegistry;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;

import java.util.Arrays;
import java.util.List;

public class NfcPreferredPaymentTest {
    private final static String mTag = "Nfc";

    private final static String mRouteDestination = "Host";
    private final static String mDescription = "CTS Nfc Test Service";
    private final static List<String> mAids = Arrays.asList("A000000004101011",
                                                            "A000000004101012",
                                                            "A000000004101013");
    private static final ComponentName CtsNfcTestService =
            new ComponentName("android.nfc.cts", "android.nfc.cts.CtsMyHostApduService");

    private NfcAdapter mAdapter;
    private CardEmulation mCardEmulation;
    private Context mContext;

    private boolean supportsHardware() {
        final PackageManager pm = InstrumentationRegistry.getContext().getPackageManager();
        return pm.hasSystemFeature(PackageManager.FEATURE_NFC);
    }

    @Before
    public void setUp() throws Exception {
        assumeTrue(supportsHardware());
        mContext = InstrumentationRegistry.getContext();
        mAdapter = NfcAdapter.getDefaultAdapter(mContext);
        assertNotNull(mAdapter);
        mCardEmulation = CardEmulation.getInstance(mAdapter);
        Settings.Secure.putString(mContext.getContentResolver(),
                Settings.Secure.NFC_PAYMENT_DEFAULT_COMPONENT,
                CtsNfcTestService.flattenToString());
    }

    @After
    public void tearDown() throws Exception {
    }

    /** Tests getAidsForPreferredPaymentService API */
    @Test
    public void testAidsForPreferredPaymentService() {
        try {
            List<String> aids = mCardEmulation.getAidsForPreferredPaymentService();
            for (String aid :aids) {
                Log.i(mTag, "AidsForPreferredPaymentService: " + aid);
            }

            assertTrue("Retrieve incorrect preferred payment aid list", mAids.equals(aids));
        } catch (Exception e) {
            fail("Unexpected Exception " + e);
        }
    }

    /** Tests getRouteDestinationForPreferredPaymentService API */
    @Test
    public void testRouteDestinationForPreferredPaymentService() {
        try {
            String routeDestination =
                    mCardEmulation.getRouteDestinationForPreferredPaymentService();
            Log.i(mTag, "RouteDestinationForPreferredPaymentService: " + routeDestination);

            assertTrue("Retrieve incorrect preferred payment route destination",
                    routeDestination.equals(mRouteDestination));
        } catch (Exception e) {
            fail("Unexpected Exception " + e);
        }
    }

    /** Tests getDescriptionForPreferredPaymentService API */
    @Test
    public void testDescriptionForPreferredPaymentService() {
        try {
            CharSequence description = mCardEmulation.getDescriptionForPreferredPaymentService();
            Log.i(mTag, "DescriptionForPreferredPaymentService: " + description.toString());

            assertTrue("Retrieve incorrect preferred payment description",
                description.toString().equals(mDescription.toString()));
        } catch (Exception e) {
            fail("Unexpected Exception " + e);
        }
    }

}
