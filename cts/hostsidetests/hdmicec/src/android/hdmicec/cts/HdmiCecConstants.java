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

package android.hdmicec.cts;

public final class HdmiCecConstants {

    private HdmiCecConstants() {}

    public static final int REBOOT_TIMEOUT = 60000;

    public static final int PHYSICAL_ADDRESS = 0x1000;
    public static final int TV_PHYSICAL_ADDRESS = 0x0000;
    public static final int PHYSICAL_ADDRESS_LENGTH = 4; /* Num nibbles in CEC message */

    public static final int PLAYBACK_DEVICE_TYPE = 0x04;

    public static final int CEC_CONTROL_SELECT = 0x0;
    public static final int CEC_CONTROL_UP = 0x1;
    public static final int CEC_CONTROL_DOWN = 0x2;
    public static final int CEC_CONTROL_LEFT = 0x3;
    public static final int CEC_CONTROL_RIGHT = 0x4;
    public static final int CEC_CONTROL_BACK = 0xd;
    public static final int CEC_CONTROL_VOLUME_UP = 0x41;
    public static final int CEC_CONTROL_VOLUME_DOWN = 0x42;
    public static final int CEC_CONTROL_MUTE = 0x43;

    public static final int UNRECOGNIZED_OPCODE = 0x0;

    public static final int CEC_DEVICE_TYPE_TV = 0;
    public static final int CEC_DEVICE_TYPE_RECORDER = 1;
    public static final int CEC_DEVICE_TYPE_RESERVED = 2;
    public static final int CEC_DEVICE_TYPE_TUNER = 3;
    public static final int CEC_DEVICE_TYPE_PLAYBACK_DEVICE = 4;
    public static final int CEC_DEVICE_TYPE_AUDIO_SYSTEM = 5;

    /** Feature Abort Reasons */
    public static final int ABORT_UNRECOGNIZED_MODE = 0;
    public static final int ABORT_NOT_IN_CORRECT_MODE = 1;
    public static final int ABORT_CANNOT_PROVIDE_SOURCE = 2;
    public static final int ABORT_INVALID_OPERAND = 3;
    public static final int ABORT_REFUSED = 4;
    public static final int ABORT_UNABLE_TO_DETERMINE = 5;
}
