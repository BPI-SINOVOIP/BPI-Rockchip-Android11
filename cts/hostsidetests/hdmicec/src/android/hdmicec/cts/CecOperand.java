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

package android.hdmicec.cts;

import java.util.HashMap;
import java.util.Map;

public enum CecOperand {
    FEATURE_ABORT(0x00),
    TEXT_VIEW_ON(0x0d),
    SET_MENU_LANGUAGE(0x32),
    STANDBY(0x36),
    USER_CONTROL_PRESSED(0x44),
    USER_CONTROL_RELEASED(0x45),
    GIVE_OSD_NAME(0x46),
    SET_OSD_NAME(0x47),
    SYSTEM_AUDIO_MODE_REQUEST(0x70),
    GIVE_AUDIO_STATUS(0x71),
    SET_SYSTEM_AUDIO_MODE(0x72),
    REPORT_AUDIO_STATUS(0x7a),
    GIVE_SYSTEM_AUDIO_MODE_STATUS(0x7d),
    SYSTEM_AUDIO_MODE_STATUS(0x7e),
    ACTIVE_SOURCE(0x82),
    GIVE_PHYSICAL_ADDRESS(0x83),
    REPORT_PHYSICAL_ADDRESS(0x84),
    REQUEST_ACTIVE_SOURCE(0x85),
    SET_STREAM_PATH(0x86),
    DEVICE_VENDOR_ID(0x87),
    VENDOR_COMMAND(0x89),
    GIVE_DEVICE_VENDOR_ID(0x8c),
    GIVE_POWER_STATUS(0x8f),
    REPORT_POWER_STATUS(0x90),
    GET_MENU_LANGUAGE(0x91),
    INACTIVE_SOURCE(0x9d),
    CEC_VERSION(0x9e),
    GET_CEC_VERSION(0x9f),
    REPORT_SHORT_AUDIO_DESCRIPTOR(0xa3),
    REQUEST_SHORT_AUDIO_DESCRIPTOR(0xa4),
    INITIATE_ARC(0xc0),
    ARC_INITIATED(0xc1),
    REQUEST_ARC_INITIATION(0xc3),
    REQUEST_ARC_TERMINATION(0xc4),
    TERMINATE_ARC(0xc5),
    ABORT(0xff);

    private final int operandCode;
    private static Map operandMap = new HashMap<>();

    static {
        for (CecOperand operand : CecOperand.values()) {
            operandMap.put(operand.operandCode, operand);
        }
    }

    public static CecOperand getOperand(int messageId) {
        return (CecOperand) operandMap.get(messageId);
    }

    @Override
    public String toString() {
        return String.format("%02x", operandCode);
    }

    private CecOperand(int operandCode) {
        this.operandCode = operandCode;
    }
}
