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

package android.telephony.cts;

import static org.junit.Assert.assertNull;

import android.os.Bundle;
import android.telephony.CellBroadcastService;
import android.telephony.cdma.CdmaSmsCbProgramData;

import org.junit.Test;

import java.util.List;
import java.util.function.Consumer;

public class CellBroadcastServiceTest {

    @Test
    public void testConstructor() {
        CellBroadcastService testCellBroadcastService = new CellBroadcastService() {
            @Override
            public void onGsmCellBroadcastSms(int slotIndex, byte[] message) {
                // do nothing
            }

            @Override
            public void onCdmaCellBroadcastSms(int slotIndex, byte[] bearerData,
                    int serviceCategory) {
                // do nothing
            }

            @Override
            public void onCdmaScpMessage(int slotIndex, List<CdmaSmsCbProgramData> smsCbProgramData,
                    String originatingAddress, Consumer<Bundle> callback) {
                // do nothing
            }

            @Override
            public CharSequence getCellBroadcastAreaInfo(int slotIndex) {
                return null;
            }
        };

        assertNull(testCellBroadcastService.getCellBroadcastAreaInfo(0));
    }
}
