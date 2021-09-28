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

package android.telephony.ims.cts;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;

import android.net.Uri;
import android.os.Parcel;
import android.telephony.ims.RcsContactUceCapability;

import androidx.test.ext.junit.runners.AndroidJUnit4;

import org.junit.Ignore;
import org.junit.Test;
import org.junit.runner.RunWith;

import java.util.List;

@RunWith(AndroidJUnit4.class)
public class RcsContactUceCapabilityTest {

    private static final Uri TEST_CONTACT = Uri.fromParts("sip", "me.test", null);
    private static final Uri TEST_VT_CONTACT = Uri.fromParts("sip", "contact.test", null);
    private static final String TEST_EXTENSION_TAG = "+g.3gpp.iari-ref=\"urn%3Aurn-7%3A3gpp"
            + "-application.ims.iari.rcs.mnc000.mcc000.testService\"";

    @Test
    @Ignore("RCS APIs not public yet")
    public void createParcelUnparcel() {
        if (!ImsUtils.shouldTestImsService()) {
            return;
        }

        RcsContactUceCapability.Builder builder = new RcsContactUceCapability.Builder(TEST_CONTACT);
        builder.add(RcsContactUceCapability.CAPABILITY_IP_VIDEO_CALL, TEST_VT_CONTACT);
        builder.add(RcsContactUceCapability.CAPABILITY_IP_VOICE_CALL);
        builder.add(TEST_EXTENSION_TAG);
        RcsContactUceCapability testCapability = builder.build();

        // parcel and unparcel
        Parcel infoParceled = Parcel.obtain();
        testCapability.writeToParcel(infoParceled, 0);
        infoParceled.setDataPosition(0);
        RcsContactUceCapability unparceledCapability =
                RcsContactUceCapability.CREATOR.createFromParcel(infoParceled);
        infoParceled.recycle();

        assertTrue(unparceledCapability.isCapable(
                RcsContactUceCapability.CAPABILITY_IP_VOICE_CALL));
        assertTrue(unparceledCapability.isCapable(
                RcsContactUceCapability.CAPABILITY_IP_VIDEO_CALL));
        assertEquals(TEST_VT_CONTACT, unparceledCapability.getServiceUri(
                RcsContactUceCapability.CAPABILITY_IP_VIDEO_CALL));
        assertEquals(TEST_CONTACT, unparceledCapability.getServiceUri(
                RcsContactUceCapability.CAPABILITY_IP_VOICE_CALL));
        assertEquals(TEST_CONTACT, unparceledCapability.getContactUri());

        List<String> extensions = unparceledCapability.getCapableExtensionTags();
        assertEquals(1, extensions.size());
        assertEquals(TEST_EXTENSION_TAG, extensions.get(0));
    }
}
