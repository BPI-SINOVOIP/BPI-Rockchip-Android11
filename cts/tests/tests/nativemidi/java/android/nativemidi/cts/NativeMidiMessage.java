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

package android.nativemidi.cts;

public class NativeMidiMessage {
    public static final int AMIDI_PACKET_SIZE = 1024;
    public static final int AMIDI_PACKET_OVERHEAD = 9;
    public static final int AMIDI_BUFFER_SIZE = AMIDI_PACKET_SIZE - AMIDI_PACKET_OVERHEAD;

    public int opcode;
    public byte[] buffer = new byte[AMIDI_BUFFER_SIZE];
    public int len;
    public long timestamp;
    public long timeReceived;

    public NativeMidiMessage() {}

    public String toString() {
        StringBuilder sb = new StringBuilder();
        sb.append("{opcode:" + opcode + " [len:" + len + "|");
        for(int index = 0; index < len; index++) {
            sb.append("0x" + Integer.toHexString(buffer[index] & 0x00FF));
            if (index < len - 1) {
                sb.append(" ");
            }
        }
        sb.append("] timestamp:" + Long.toHexString(timestamp));

        return sb.toString();
    }

}