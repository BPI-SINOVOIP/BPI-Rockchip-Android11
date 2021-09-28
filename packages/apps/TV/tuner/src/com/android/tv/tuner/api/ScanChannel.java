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
package com.android.tv.tuner.api;

import android.util.Log;
import com.android.tv.tuner.data.Channel;


/** Channel information gathered from a <em>scan</em> */
public final class ScanChannel {
    private static final String TAG = "ScanChannel";
    public final int type;
    public final Channel.DeliverySystemType deliverySystemType;
    public final int frequency;
    public final String modulation;
    public final String filename;
    /**
     * Radio frequency (channel) number specified at
     * https://en.wikipedia.org/wiki/North_American_television_frequencies This can be {@code null}
     * for cases like cable signal.
     */
    public final Integer radioFrequencyNumber;

    public static ScanChannel forTuner(
            String deliverySystemType, int frequency, String modulation,
            Integer radioFrequencyNumber) {
        return new ScanChannel(
                Channel.TunerType.TYPE_TUNER_VALUE, lookupDeliveryStringToInt(deliverySystemType),
                frequency, modulation, null, radioFrequencyNumber);
    }

    public static ScanChannel forFile(int frequency, String filename) {
        return new ScanChannel(Channel.TunerType.TYPE_FILE_VALUE,
                Channel.DeliverySystemType.DELIVERY_SYSTEM_UNDEFINED, frequency, "file:",
                filename, null);
    }

    private ScanChannel(
            int type,
            Channel.DeliverySystemType deliverySystemType,
            int frequency,
            String modulation,
            String filename,
            Integer radioFrequencyNumber) {
        this.type = type;
        this.deliverySystemType = deliverySystemType;
        this.frequency = frequency;
        this.modulation = modulation;
        this.filename = filename;
        this.radioFrequencyNumber = radioFrequencyNumber;
    }

    private static Channel.DeliverySystemType lookupDeliveryStringToInt(String deliverySystemType) {
        Channel.DeliverySystemType ret;
        switch (deliverySystemType) {
            case "A":
                ret = Channel.DeliverySystemType.DELIVERY_SYSTEM_ATSC;
                break;
            case "C":
                ret = Channel.DeliverySystemType.DELIVERY_SYSTEM_DVBC;
                break;
            case "S":
                ret = Channel.DeliverySystemType.DELIVERY_SYSTEM_DVBS;
                break;
            case "S2":
                ret = Channel.DeliverySystemType.DELIVERY_SYSTEM_DVBS2;
                break;
            case "T":
                ret = Channel.DeliverySystemType.DELIVERY_SYSTEM_DVBT;
                break;
            case "T2":
                ret = Channel.DeliverySystemType.DELIVERY_SYSTEM_DVBT2;
                break;
            default:
                Log.e(TAG, "Unknown delivery system type");
                ret = Channel.DeliverySystemType.DELIVERY_SYSTEM_UNDEFINED;
                break;
        }
        return ret;
    }
}
