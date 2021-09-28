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

import android.automotive.computepipe.runner.PacketDescriptor;

@VintfStability
interface IPipeStream {
    /**
     * Receives calls from the AIDL implementation each time a new packet is available.
     * Semantic data is contaied in the packet descriptor.
     * Only Zero copy data packets received by this method must be returned via calls to
     * IPipeRunner::doneWithPacket(), using the bufId field in the descriptor.
     * After the pipe execution has stopped this callback may continue to happen for sometime.
     * Those packets must still be returned. Last frame will be indicated with
     * a null packet. After that there will not be any further packets.
     *
     * @param: packet is a descriptor for the packet.
     */
    oneway void deliverPacket(in PacketDescriptor packet);
}
