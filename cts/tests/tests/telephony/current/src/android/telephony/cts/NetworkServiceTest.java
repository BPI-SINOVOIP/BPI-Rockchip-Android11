/*
 * Copyright (C) 2019 The Android Open Source Project
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

import static org.junit.Assert.assertEquals;

import android.telephony.NetworkService;
import android.telephony.NetworkService.NetworkServiceProvider;

import org.junit.Test;

public class NetworkServiceTest {

    private static final int SLOT_INDEX_CTS = 1;

    private static class CtsNetworkService extends NetworkService {
        private class CtsNetworkServiceProvider extends NetworkServiceProvider {
            CtsNetworkServiceProvider(int slotId) {
                super(slotId);
            }

            @Override
            public void close() {}
        }

        @Override
        public NetworkServiceProvider onCreateNetworkServiceProvider(int slotIndex) {
            return new CtsNetworkServiceProvider(slotIndex);
        }
    }

    @Test
    public void testNetworkService() {
        CtsNetworkService networkService = new CtsNetworkService();
        NetworkServiceProvider networkServiceProvider =
                networkService.onCreateNetworkServiceProvider(SLOT_INDEX_CTS);
        assertEquals(SLOT_INDEX_CTS, networkServiceProvider.getSlotIndex());

        networkServiceProvider.notifyNetworkRegistrationInfoChanged();
    }
}
