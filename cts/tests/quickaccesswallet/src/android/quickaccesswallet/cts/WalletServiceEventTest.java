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
package android.quickaccesswallet.cts;

import static android.service.quickaccesswallet.WalletServiceEvent.TYPE_NFC_PAYMENT_STARTED;

import static com.google.common.truth.Truth.assertThat;

import android.os.Parcel;
import android.service.quickaccesswallet.WalletServiceEvent;

import androidx.test.ext.junit.runners.AndroidJUnit4;

import org.junit.Test;
import org.junit.runner.RunWith;

/**
 * Tests {@link WalletServiceEvent}
 */
@RunWith(AndroidJUnit4.class)
public class WalletServiceEventTest {

    @Test
    public void testParcel_toParcel() {
        WalletServiceEvent event = new WalletServiceEvent(TYPE_NFC_PAYMENT_STARTED);

        Parcel p = Parcel.obtain();
        event.writeToParcel(p, 0);
        p.setDataPosition(0);
        WalletServiceEvent newEvent = WalletServiceEvent.CREATOR.createFromParcel(p);
        assertThat(event.getEventType()).isEqualTo(newEvent.getEventType());
    }
}
