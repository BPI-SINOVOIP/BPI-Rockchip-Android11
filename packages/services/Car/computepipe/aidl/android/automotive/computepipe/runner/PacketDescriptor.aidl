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
package android.automotive.computepipe.runner;

import android.automotive.computepipe.runner.PacketDescriptorPacketType;
import android.hardware.graphics.common.HardwareBuffer;

/**
 * Structure that describes the output packet for a specific outputstream
 * that gets returned to the client.
 */
@VintfStability
parcelable PacketDescriptor {
    /**
     * packet id, used in case of zero copy data.
     * Used to notify the runner of consumption.
     */
    int bufId;
    /**
     * type of the buffer
     */
    PacketDescriptorPacketType type;
    /**
     * size of the memory region
     */
    int size;
    /**
     * Handle to pixel data. This handle can be mapped and data retrieved.
     * The description field will contain the
     * graphics buffer description. Must be freed with call to doneWithPacket()
     * This is populated only if type is PIXEL
     */
    HardwareBuffer handle;
    /**
     * Zero copy semantic data handle.
     * Must be freed with call to doneWithPacket().
     * This is populated only if type is SEMANTIC
     */
    ParcelFileDescriptor[] dataFds;
    /**
     * Timestamp of event at source. Timestamp value is milliseconds since epoch.
     */
    long sourceTimeStampMillis;
    /**
     * semantic data. Requires no doneWithPacket() acknowledgement.
     */
    byte[] data;
}
