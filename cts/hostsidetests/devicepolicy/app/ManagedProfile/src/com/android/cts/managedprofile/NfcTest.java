/*
 * Copyright (C) 2015 The Android Open Source Project
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

package com.android.cts.managedprofile;

import android.content.Intent;
import android.content.pm.PackageManager;
import android.content.pm.ResolveInfo;
import android.test.AndroidTestCase;

public class NfcTest extends AndroidTestCase {
    private static final String SAMPLE_TEXT = "This is my text to send.";
    private static final String TEXT_MIME_TYPE = "text/plain";
    private static final String NFC_BEAM_ACTIVITY = "com.android.nfc.BeamShareActivity";
    private static int NFC_RESOLVE_TIME_STEP_MILLIS = 1000;
    private static int NFC_RESOLVE_TIMEOUT_MILLIS = 16000;

    public void testNfcShareDisabled() throws Exception {
        Intent intent = getTextShareIntent();
        // After the "no_outgoing_beam" configuration item is modified, it takes a while
        // until NFC receives the DEVICE_POLICY_MANAGER_STATE_CHANGED broadcast
        waitForNfcBeamActivityDisabled(intent, NFC_RESOLVE_TIMEOUT_MILLIS);
        assertFalse("Nfc beam activity should not be resolved", isNfcBeamActivityResolved(intent));
    }

    public void testNfcShareEnabled() throws Exception {
        Intent intent = getTextShareIntent();
        assertTrue("Nfc beam activity should be resolved", isNfcBeamActivityResolved(intent));
    }

    private void waitForNfcBeamActivityDisabled(Intent intent, int maxDelayMillis)
            throws Exception {
        int totalDelayedMillis = 0;
        while (isNfcBeamActivityResolved(intent) && totalDelayedMillis <= maxDelayMillis) {
            Thread.sleep(NFC_RESOLVE_TIME_STEP_MILLIS);
            totalDelayedMillis += NFC_RESOLVE_TIME_STEP_MILLIS;
        }
    }

    private Intent getTextShareIntent() {
        Intent intent = new Intent();
        intent.setAction(Intent.ACTION_SEND);
        intent.putExtra(Intent.EXTRA_TEXT, SAMPLE_TEXT);
        intent.setType(TEXT_MIME_TYPE);
        return intent;
    }

    private boolean isNfcBeamActivityResolved(Intent intent) {
        PackageManager pm = mContext.getPackageManager();
        for (ResolveInfo resolveInfo : pm.queryIntentActivities(intent, 0)) {
            if (NFC_BEAM_ACTIVITY.equals(resolveInfo.activityInfo.name)) {
                return true;
            }
        }

        return false;
    }
}

