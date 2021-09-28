/*
 * Copyright 2020 The Android Open Source Project
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

/*
 *  Definitions for HCI Event Parameter Minimum Length
 */
static const uint8_t hci_event_parameters_minimum_length[] = {
    0,    //  0x00 - N/A
    1,    //  0x01 - HCI_Inquiry_Complete Event
    15,   //  0x02 - HCI_Inquiry_Result Event (Num_Responses = 1)
    11,   //  0x03 - HCI_Connection_Complete Event
    10,   //  0x04 - HCI_Connection_Request Event
    4,    //  0x05 - HCI_Disconnection_Complete Event
    3,    //  0x06 - HCI_Authentication_Complete Event
    255,  //  0x07 - HCI_Remote_Name_Request_Complete Event
    4,    //  0x08 - HCI_Encryption_Change Event
    3,    //  0x09 - HCI_Change_Connection_Link_Key_Complete Event
    4,    //  0x0A - HCI_Master_Link_Key_Complete Event
    11,   //  0x0B - HCI_Read_Remote_Supported_Features_Complete Event
    8,    //  0x0C - HCI_Read_Remote_Version_Information_Complete Event
    21,   //  0x0D - HCI_QoS_Setup_Complete Event
    3,    //  0x0E - HCI_Command_Complete Event (Depends on command)
    4,    //  0x0F - HCI_Command_Status Event
    1,    //  0x10 - HCI_Hardware_Error Event
    2,    //  0x11 - HCI_Flush_Occurred Event
    8,    //  0x12 - HCI_Role_Change Event
    5,    //  0x13 - HCI_Number_Of_Completed_Packets Event (Num_Handles = 1)
    6,    //  0x14 - HCI_Mode_Change Event
    23,   //  0x15 - HCI_Return_Link_Keys Event (Num_Keys = 1)
    6,    //  0x16 - HCI_PIN_Code_Request Event
    6,    //  0x17 - HCI_Link_Key_Request Event
    23,   //  0x18 - HCI_Link_Key_Notification Event
    3,    //  0x19 - HCI_Loopback_Command Event (Depends on command)
    1,    //  0x1A - HCI_Data_Buffer_Overflow Event
    3,    //  0x1B - HCI_Max_Slots_Change Event
    5,    //  0x1C - HCI_Read_Clock_Offset_Complete Event
    5,    //  0x1D - HCI_Connection_Packet_Type_Changed Event
    2,    //  0x1E - HCI_QoS_Violation Event
    7,    //  0x1F - HCI_Page_Scan_Mode_Change Event (Deprecated)
    7,    //  0x20 - HCI_Page_Scan_Repetition_Mode_Change Event
    22,   //  0x21 - HCI_Flow_Specification_Complet Event
    15,   //  0x22 - HCI_Inquiry_Result_with_RSSI Event (Num_Responses = 1)
    13,   //  0x23 - HCI_Read_Remote_Extended_Features_Complete Event
    0,    //  0x24 - N/A
    0,    //  0x25 - N/A
    0,    //  0x26 - N/A
    0,    //  0x27 - N/A
    0,    //  0x28 - N/A
    0,    //  0x29 - N/A
    0,    //  0x2A - N/A
    0,    //  0x2B - N/A
    17,   //  0x2C - HCI_Synchronous_Connection_Complete Event
    9,    //  0x2D - HCI_Synchronous_Connection_Changed Event
    11,   //  0x2E - HCI_Sniff_Subrating Event
    255,  //  0x2F - HCI_Extended_Inquiry_Result Event
    3,    //  0x30 - HCI_Encryption_Key_Refresh_Complete Event
    6,    //  0x31 - HCI_IO_Capability_Request Event
    9,    //  0x32 - HCI_IO_Capability_Response Event
    10,   //  0x33 - HCI_User_Confirmation_Request Event
    6,    //  0x34 - HCI_User_Passkey_Request Event
    6,    //  0x35 - HCI_Remote_OOB_Data_Request Event
    7,    //  0x36 - HCI_Simple_Pairing_Complete Event
    0,    //  0x37 - N/A
    4,    //  0x38 - HCI_Link_Supervision_Timeout_Changed Event
    2,    //  0x39 - HCI_Enhanced_Flush_Complete Event
    0,    //  0x3A - N/A
    10,   //  0x3B - HCI_User_Passkey_Notification Event
    7,    //  0x3C - HCI_Keypress_Notification Event
    14,   //  0x3D - HCI_Remote_Host_Supported_Features_Notification Event
    0,    //  0x3E - LE Meta event
    0,    //  0x3F - N/A
    2,    //  0x40 - HCI_Physical_Link_Complete Event
    1,    //  0x41 - HCI_Channel_Selected Event
    3,    //  0x42 - HCI_Disconnection_Physical_Link_Complete Event
    2,    //  0x43 - HCI_Physical_Link_Loss_Early_Warning Event
    1,    //  0x44 - HCI_Physical_Link_Recovery Event
    5,    //  0x45 - HCI_Logical_Link_Complete Event
    4,    //  0x46 - HCI_Disconnection_Logical_Link_Complete Event
    3,    //  0x47 - HCI_Flow_Spec_Modify_Complete Event
    9,    //  0x48 - HCI_Number_Of_Completed_Data_Blocks Event (Num_Handles = 1)
    2,    //  0x49 - HCI_AMP_Start_Test Event
    2,    //  0x4A - HCI_AMP_Test_End Event
    18,   //  0x4B - HCI_AMP_Receiver_Report Event
    3,    //  0x4C - HCI_Short_Range_Mode_Change_Complete Event
    2,    //  0x4D - HCI_AMP_Status_Change Event
    9,    //  0x4E - HCI_Triggered_Clock_Capture Event
    1,    //  0x4F - HCI_Synchronization_Train_Complete Event
    29,   //  0x50 - HCI_Synchronization_Train_Received Event
    18,   //  0x51 - HCI_Connectionless_Slave_Broadcast_Receive Event
          //  (Data_Length = 0)
    7,    //  0x52 - HCI_Connectionless_Slave_Broadcast_Timeout Event
    7,    //  0x53 - HCI_Truncated_Page_Complete Event
    0,    //  0x54 - HCI_Slave_Page_Response_Timeout Event
    10,   //  0x55 - HCI_Connectionless_Slave_Broadcast_Channel_Map_Change Event
    4,    //  0x56 - HCI_Inquiry_Response_Notification Event
    2,    //  0x57 - HCI_Authenticated_Payload_Timeout_Expired Event
    8,    //  0x58 - HCI_SAM_Status_Change Event
    0,    //  0x59 - N/A
    0,    //  0x5A - N/A
    0,    //  0x5B - N/A
    0,    //  0x5C - N/A
    0,    //  0x5D - N/A
    0,    //  0x5E - N/A
    0,    //  0x5F - N/A
    0,    //  0x60 - N/A
    0,    //  0x61 - N/A
    0,    //  0x62 - N/A
    0,    //  0x63 - N/A
    0,    //  0x64 - N/A
    0,    //  0x65 - N/A
    0,    //  0x66 - N/A
    0,    //  0x67 - N/A
    0,    //  0x68 - N/A
    0,    //  0x69 - N/A
    0,    //  0x6A - N/A
    0,    //  0x6B - N/A
    0,    //  0x6C - N/A
    0,    //  0x6D - N/A
    0,    //  0x6E - N/A
    0,    //  0x6F - N/A
    0,    //  0x70 - N/A
    0,    //  0x71 - N/A
    0,    //  0x72 - N/A
    0,    //  0x73 - N/A
    0,    //  0x74 - N/A
    0,    //  0x75 - N/A
    0,    //  0x76 - N/A
    0,    //  0x77 - N/A
    0,    //  0x78 - N/A
    0,    //  0x79 - N/A
    0,    //  0x7A - N/A
    0,    //  0x7B - N/A
    0,    //  0x7C - N/A
    0,    //  0x7D - N/A
    0,    //  0x7E - N/A
    0,    //  0x7F - N/A
    0,    //  0x80 - N/A
    0,    //  0x81 - N/A
    0,    //  0x82 - N/A
    0,    //  0x83 - N/A
    0,    //  0x84 - N/A
    0,    //  0x85 - N/A
    0,    //  0x86 - N/A
    0,    //  0x87 - N/A
    0,    //  0x88 - N/A
    0,    //  0x89 - N/A
    0,    //  0x8A - N/A
    0,    //  0x8B - N/A
    0,    //  0x8C - N/A
    0,    //  0x8D - N/A
    0,    //  0x8E - N/A
    0,    //  0x8F - N/A
    0,    //  0x90 - N/A
    0,    //  0x91 - N/A
    0,    //  0x92 - N/A
    0,    //  0x93 - N/A
    0,    //  0x94 - N/A
    0,    //  0x95 - N/A
    0,    //  0x96 - N/A
    0,    //  0x97 - N/A
    0,    //  0x98 - N/A
    0,    //  0x99 - N/A
    0,    //  0x9A - N/A
    0,    //  0x9B - N/A
    0,    //  0x9C - N/A
    0,    //  0x9D - N/A
    0,    //  0x9E - N/A
    0,    //  0x9F - N/A
    0,    //  0xA0 - N/A
    0,    //  0xA1 - N/A
    0,    //  0xA2 - N/A
    0,    //  0xA3 - N/A
    0,    //  0xA4 - N/A
    0,    //  0xA5 - N/A
    0,    //  0xA6 - N/A
    0,    //  0xA7 - N/A
    0,    //  0xA8 - N/A
    0,    //  0xA9 - N/A
    0,    //  0xAA - N/A
    0,    //  0xAB - N/A
    0,    //  0xAC - N/A
    0,    //  0xAD - N/A
    0,    //  0xAE - N/A
    0,    //  0xAF - N/A
    0,    //  0xB0 - N/A
    0,    //  0xB1 - N/A
    0,    //  0xB2 - N/A
    0,    //  0xB3 - N/A
    0,    //  0xB4 - N/A
    0,    //  0xB5 - N/A
    0,    //  0xB6 - N/A
    0,    //  0xB7 - N/A
    0,    //  0xB8 - N/A
    0,    //  0xB9 - N/A
    0,    //  0xBA - N/A
    0,    //  0xBB - N/A
    0,    //  0xBC - N/A
    0,    //  0xBD - N/A
    0,    //  0xBE - N/A
    0,    //  0xBF - N/A
    0,    //  0xC0 - N/A
    0,    //  0xC1 - N/A
    0,    //  0xC2 - N/A
    0,    //  0xC3 - N/A
    0,    //  0xC4 - N/A
    0,    //  0xC5 - N/A
    0,    //  0xC6 - N/A
    0,    //  0xC7 - N/A
    0,    //  0xC8 - N/A
    0,    //  0xC9 - N/A
    0,    //  0xCA - N/A
    0,    //  0xCB - N/A
    0,    //  0xCC - N/A
    0,    //  0xCD - N/A
    0,    //  0xCE - N/A
    0,    //  0xCF - N/A
    0,    //  0xD0 - N/A
    0,    //  0xD1 - N/A
    0,    //  0xD2 - N/A
    0,    //  0xD3 - N/A
    0,    //  0xD4 - N/A
    0,    //  0xD5 - N/A
    0,    //  0xD6 - N/A
    0,    //  0xD7 - N/A
    0,    //  0xD8 - N/A
    0,    //  0xD9 - N/A
    0,    //  0xDA - N/A
    0,    //  0xDB - N/A
    0,    //  0xDC - N/A
    0,    //  0xDD - N/A
    0,    //  0xDE - N/A
    0,    //  0xDF - N/A
    0,    //  0xE0 - N/A
    0,    //  0xE1 - N/A
    0,    //  0xE2 - N/A
    0,    //  0xE3 - N/A
    0,    //  0xE4 - N/A
    0,    //  0xE5 - N/A
    0,    //  0xE6 - N/A
    0,    //  0xE7 - N/A
    0,    //  0xE8 - N/A
    0,    //  0xE9 - N/A
    0,    //  0xEA - N/A
    0,    //  0xEB - N/A
    0,    //  0xEC - N/A
    0,    //  0xED - N/A
    0,    //  0xEE - N/A
    0,    //  0xEF - N/A
    0,    //  0xF0 - N/A
    0,    //  0xF1 - N/A
    0,    //  0xF2 - N/A
    0,    //  0xF3 - N/A
    0,    //  0xF4 - N/A
    0,    //  0xF5 - N/A
    0,    //  0xF6 - N/A
    0,    //  0xF7 - N/A
    0,    //  0xF8 - N/A
    0,    //  0xF9 - N/A
    0,    //  0xFA - N/A
    0,    //  0xFB - N/A
    0,    //  0xFC - N/A
    0,    //  0xFD - N/A
    0,    //  0xFE - N/A
    0,    //  0xFF - HCI_Vendor_Specific Event
};
