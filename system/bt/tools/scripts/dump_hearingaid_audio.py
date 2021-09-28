#!/usr/bin/env python
# Copyright 2018 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License"); you may not
# use this file except in compliance with the License. You may obtain a copy of
# the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
# WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
# License for the specific language governing permissions and limitations under
# the License.
#
#
#
#
# This script extracts Hearing Aid audio data from btsnoop.
# Generates a valid audio file which can be played using player like smplayer.
#
# Audio File Name Format:
# [PEER_ADDRESS]-[START_TIMESTAMP]-[AUDIO_TYPE]-[SAMPLE_RATE].[CODEC]
#
# Debug Infomation File Name Format:
# debug_ver_[DEBUG_VERSION]-[PEER_ADDRESS]-[START_TIMESTAMP]-[AUDIO_TYPE]-[SAMPLE_RATE].txt
#
# Player:
# smplayer
#
# NOTE:
# Please make sure you HCI Snoop data file includes the following frames:
# HearingAid "LE Enhanced Connection Complete", GATT write for Audio Control
# Point with "Start cmd", and the data frames.

import argparse
import os
import struct
import sys
import time

IS_SENT = "IS_SENT"
PEER_ADDRESS = "PEER_ADDRESS"
CONNECTION_HANDLE = "CONNECTION_HANDLE"
AUDIO_CONTROL_ATTR_HANDLE = "AUDIO_CONTROL_ATTR_HANDLE"
START = "START"
TIMESTAMP_STR_FORMAT = "TIMESTAMP_STR_FORMAT"
TIMESTAMP_TIME_FORMAT = "TIMESTAMP_TIME_FORMAT"
CODEC = "CODEC"
SAMPLE_RATE = "SAMPLE_RATE"
AUDIO_TYPE = "AUDIO_TYPE"
DEBUG_VERSION = "DEBUG_VERSION"
DEBUG_DATA = "DEBUG_DATA"
AUDIO_DATA_B = "AUDIO_DATA_B"

# Debug packet header struct
header_list_str = [
    "Event Processed", "Number Packet Nacked By Slave",
    "Number Packet Nacked By Master"
]
# Debug frame information structs
data_list_str = [
    "Event Number", "Overrun", "Underrun", "Skips", "Rendered Audio Frame",
    "First PDU Option", "Second PDU Option", "Third PDU Option"
]

AUDIO_CONTROL_POINT_UUID = "f0d4de7e4a88476c9d9f1937b0996cc0"
SEC_CONVERT = 1000000
folder = None
full_debug = False
simple_debug = False

force_audio_control_attr_handle = None
default_audio_control_attr_handle = 0x0079

audio_data = {}

#=======================================================================
# Parse ACL Data Function
#=======================================================================

#-----------------------------------------------------------------------
# Parse Hearing Aid Packet
#-----------------------------------------------------------------------


def parse_acl_ha_debug_buffer(data, result):
    """This function extracts HA debug buffer"""
    if len(data) < 5:
        return

    version, data = unpack_data(data, 1)
    update_audio_data(CONNECTION_HANDLE, result[CONNECTION_HANDLE],
                      DEBUG_VERSION, str(version))

    debug_str = result[TIMESTAMP_TIME_FORMAT]
    for p in range(3):
        byte_data, data = unpack_data(data, 1)
        debug_str = debug_str + ", " + header_list_str[p] + "=" + str(
            byte_data).rjust(3)

    if full_debug:
        debug_str = debug_str + "\n" + "|".join(data_list_str) + "\n"
        while True:
            if len(data) < 7:
                break
            base = 0
            data_list_content = []
            for counter in range(6):
                p = base + counter
                byte_data, data = unpack_data(data, 1)
                if p == 1:
                    data_list_content.append(
                        str(byte_data & 0x03).rjust(len(data_list_str[p])))
                    data_list_content.append(
                        str((byte_data >> 2) & 0x03).rjust(
                            len(data_list_str[p + 1])))
                    data_list_content.append(
                        str((byte_data >> 4) & 0x0f).rjust(
                            len(data_list_str[p + 2])))
                    base = 2
                else:
                    data_list_content.append(
                        str(byte_data).rjust(len(data_list_str[p])))
            debug_str = debug_str + "|".join(data_list_content) + "\n"

    update_audio_data(CONNECTION_HANDLE, result[CONNECTION_HANDLE], DEBUG_DATA,
                      debug_str)


def parse_acl_ha_audio_data(data, result):
    """This function extracts HA audio data."""
    if len(data) < 2:
        return
    # Remove audio packet number
    audio_data_b = data[1:]
    update_audio_data(CONNECTION_HANDLE, result[CONNECTION_HANDLE],
                      AUDIO_DATA_B, audio_data_b)


def parse_acl_ha_audio_type(data, result):
    """This function parses HA audio control cmd audio type."""
    audio_type, data = unpack_data(data, 1)
    if audio_type is None:
        return
    elif audio_type == 0x01:
        audio_type = "Ringtone"
    elif audio_type == 0x02:
        audio_type = "Phonecall"
    elif audio_type == 0x03:
        audio_type = "Media"
    else:
        audio_type = "Unknown"
    update_audio_data(CONNECTION_HANDLE, result[CONNECTION_HANDLE], AUDIO_TYPE,
                      audio_type)


def parse_acl_ha_codec(data, result):
    """This function parses HA audio control cmd codec and sample rate."""
    codec, data = unpack_data(data, 1)
    if codec == 0x01:
        codec = "G722"
        sample_rate = "16KHZ"
    elif codec == 0x02:
        codec = "G722"
        sample_rate = "24KHZ"
    else:
        codec = "Unknown"
        sample_rate = "Unknown"
    update_audio_data(CONNECTION_HANDLE, result[CONNECTION_HANDLE], CODEC,
                      codec)
    update_audio_data(CONNECTION_HANDLE, result[CONNECTION_HANDLE], SAMPLE_RATE,
                      sample_rate)
    parse_acl_ha_audio_type(data, result)


def parse_acl_ha_audio_control_cmd(data, result):
    """This function parses HA audio control cmd is start/stop."""
    control_cmd, data = unpack_data(data, 1)
    if control_cmd == 0x01:
        update_audio_data(CONNECTION_HANDLE, result[CONNECTION_HANDLE], START,
                          True)
        update_audio_data(CONNECTION_HANDLE, result[CONNECTION_HANDLE],
                          TIMESTAMP_STR_FORMAT, result[TIMESTAMP_STR_FORMAT])
        parse_acl_ha_codec(data, result)
    elif control_cmd == 0x02:
        update_audio_data(CONNECTION_HANDLE, result[CONNECTION_HANDLE], START,
                          False)


#-----------------------------------------------------------------------
# Parse ACL Packet
#-----------------------------------------------------------------------


def parse_acl_att_long_uuid(data, result):
    """This function parses ATT long UUID to get attr_handle."""
    # len (1 byte) + start_attr_handle (2 bytes) + properties (1 byte) +
    # attr_handle (2 bytes) + long_uuid (16 bytes) = 22 bytes
    if len(data) < 22:
        return
    # skip unpack len, start_attr_handle, properties.
    data = data[4:]
    attr_handle, data = unpack_data(data, 2)
    long_uuid_list = []
    for p in range(0, 16):
        long_uuid_list.append("{0:02x}".format(struct.unpack(">B", data[p])[0]))
    long_uuid_list.reverse()
    long_uuid = "".join(long_uuid_list)
    # Check long_uuid is AUDIO_CONTROL_POINT uuid to get the attr_handle.
    if long_uuid == AUDIO_CONTROL_POINT_UUID:
        update_audio_data(CONNECTION_HANDLE, result[CONNECTION_HANDLE],
                          AUDIO_CONTROL_ATTR_HANDLE, attr_handle)


def parse_acl_opcode(data, result):
    """This function parses acl data opcode."""
    # opcode (1 byte) = 1 bytes
    if len(data) < 1:
        return
    opcode, data = unpack_data(data, 1)
    # Check opcode is 0x12 (write request) and attr_handle is
    # audio_control_attr_handle for check it is HA audio control cmd.
    if result[IS_SENT] and opcode == 0x12:
        if len(data) < 2:
            return
        attr_handle, data = unpack_data(data, 2)
        if attr_handle == \
            get_audio_control_attr_handle(result[CONNECTION_HANDLE]):
            parse_acl_ha_audio_control_cmd(data, result)
    # Check opcode is 0x09 (read response) to parse ATT long UUID.
    elif not result[IS_SENT] and opcode == 0x09:
        parse_acl_att_long_uuid(data, result)


def parse_acl_handle(data, result):
    """This function parses acl data handle."""
    # connection_handle (2 bytes) + total_len (2 bytes) + pdu (2 bytes)
    # + channel_id (2 bytes) = 8 bytes
    if len(data) < 8:
        return
    connection_handle, data = unpack_data(data, 2)
    connection_handle = connection_handle & 0x0FFF
    # skip unpack total_len
    data = data[2:]
    pdu, data = unpack_data(data, 2)
    channel_id, data = unpack_data(data, 2)

    # Check ATT packet or "Coc Data Packet" to get ATT information and audio
    # data.
    if connection_handle <= 0x0EFF:
        if channel_id <= 0x003F:
            result[CONNECTION_HANDLE] = connection_handle
            parse_acl_opcode(data, result)
        elif channel_id >= 0x0040 and channel_id <= 0x007F:
            result[CONNECTION_HANDLE] = connection_handle
            sdu, data = unpack_data(data, 2)
            if pdu - 2 == sdu:
                if result[IS_SENT]:
                    parse_acl_ha_audio_data(data, result)
                else:
                    if simple_debug:
                        parse_acl_ha_debug_buffer(data, result)


#=======================================================================
# Parse HCI EVT Function
#=======================================================================


def parse_hci_evt_peer_address(data, result):
    """This function parses peer address from hci event."""
    peer_address_list = []
    address_empty_list = ["00", "00", "00", "00", "00", "00"]
    for n in range(0, 3):
        if len(data) < 6:
            return
        for p in range(0, 6):
            peer_address_list.append("{0:02x}".format(
                struct.unpack(">B", data[p])[0]))
        # Check the address is empty or not.
        if peer_address_list == address_empty_list:
            del peer_address_list[:]
            data = data[6:]
        else:
            break
    peer_address_list.reverse()
    peer_address = "_".join(peer_address_list)
    update_audio_data("", "", PEER_ADDRESS, peer_address)
    update_audio_data(PEER_ADDRESS, peer_address, CONNECTION_HANDLE,
                      result[CONNECTION_HANDLE])


def parse_hci_evt_code(data, result):
    """This function parses hci event content."""
    # hci_evt (1 byte) + param_total_len (1 byte) + sub_event (1 byte)
    # + status (1 byte) + connection_handle (2 bytes) + role (1 byte)
    # + address_type (1 byte) = 8 bytes
    if len(data) < 8:
        return
    hci_evt, data = unpack_data(data, 1)
    # skip unpack param_total_len.
    data = data[1:]
    sub_event, data = unpack_data(data, 1)
    status, data = unpack_data(data, 1)
    connection_handle, data = unpack_data(data, 2)
    connection_handle = connection_handle & 0x0FFF
    # skip unpack role, address_type.
    data = data[2:]
    # We will directly check it is LE Enhanced Connection Complete or not
    # for get Connection Handle and Address.
    if not result[IS_SENT] and hci_evt == 0x3E and sub_event == 0x0A \
        and status == 0x00 and connection_handle <= 0x0EFF:
        result[CONNECTION_HANDLE] = connection_handle
        parse_hci_evt_peer_address(data, result)


#=======================================================================
# Common Parse Function
#=======================================================================


def parse_packet_data(data, result):
    """This function parses packet type."""
    packet_type, data = unpack_data(data, 1)
    if packet_type == 0x02:
        # Try to check HearingAid audio control packet and data packet.
        parse_acl_handle(data, result)
    elif packet_type == 0x04:
        # Try to check HearingAid connection successful packet.
        parse_hci_evt_code(data, result)


def parse_packet(btsnoop_file):
    """This function parses packet len, timestamp."""
    packet_result = {}

    # ori_len (4 bytes) + include_len (4 bytes) + packet_flag (4 bytes)
    # + drop (4 bytes) + timestamp (8 bytes) = 24 bytes
    packet_header = btsnoop_file.read(24)
    if len(packet_header) != 24:
        return False

    ori_len, include_len, packet_flag, drop, timestamp = \
        struct.unpack(">IIIIq", packet_header)

    if ori_len == include_len:
        packet_data = btsnoop_file.read(ori_len)
        if len(packet_data) != ori_len:
            return False
        if packet_flag != 2 and drop == 0:
            packet_result[IS_SENT] = (packet_flag == 0)
            packet_result[TIMESTAMP_STR_FORMAT], packet_result[
                TIMESTAMP_TIME_FORMAT] = convert_time_str(timestamp)
            parse_packet_data(packet_data, packet_result)
    else:
        return False

    return True


#=======================================================================
# Update and DumpData Function
#=======================================================================


def dump_audio_data(data):
    """This function dumps audio data into file."""
    file_type = "." + data[CODEC]
    file_name_list = []
    file_name_list.append(data[PEER_ADDRESS])
    file_name_list.append(data[TIMESTAMP_STR_FORMAT])
    file_name_list.append(data[AUDIO_TYPE])
    file_name_list.append(data[SAMPLE_RATE])
    if folder is not None:
        if not os.path.exists(folder):
            os.makedirs(folder)
        audio_file_name = os.path.join(folder,
                                       "-".join(file_name_list) + file_type)
        if data.has_key(DEBUG_VERSION):
            file_prefix = "debug_ver_" + data[DEBUG_VERSION] + "-"
            debug_file_name = os.path.join(
                folder, file_prefix + "-".join(file_name_list) + ".txt")
    else:
        audio_file_name = "-".join(file_name_list) + file_type
        if data.has_key(DEBUG_VERSION):
            file_prefix = "debug_ver_" + data[DEBUG_VERSION] + "-"
            debug_file_name = file_prefix + "-".join(file_name_list) + ".txt"

    sys.stdout.write("Start to dump Audio File : %s\n" % audio_file_name)
    if data.has_key(AUDIO_DATA_B):
        with open(audio_file_name, "wb+") as audio_file:
            audio_file.write(data[AUDIO_DATA_B])
            sys.stdout.write(
                "Finished to dump Audio File: %s\n\n" % audio_file_name)
    else:
        sys.stdout.write("Fail to dump Audio File: %s\n" % audio_file_name)
        sys.stdout.write("There isn't any Hearing Aid audio data.\n\n")

    if simple_debug:
        sys.stdout.write(
            "Start to dump audio %s Debug File\n" % audio_file_name)
        if data.has_key(DEBUG_DATA):
            with open(debug_file_name, "wb+") as debug_file:
                debug_file.write(data[DEBUG_DATA])
                sys.stdout.write(
                    "Finished to dump Debug File: %s\n\n" % debug_file_name)
        else:
            sys.stdout.write(
                "Fail to dump audio %s Debug File\n" % audio_file_name)
            sys.stdout.write("There isn't any Hearing Aid debug data.\n\n")


def update_audio_data(relate_key, relate_value, key, value):
    """
  This function records the dump audio file related information.
  audio_data = {
    PEER_ADDRESS:{
      PEER_ADDRESS: PEER_ADDRESS,
      CONNECTION_HANDLE: CONNECTION_HANDLE,
      AUDIO_CONTROL_ATTR_HANDLE: AUDIO_CONTROL_ATTR_HANDLE,
      START: True or False,
      TIMESTAMP_STR_FORMAT: START_TIMESTAMP_STR_FORMAT,
      CODEC: CODEC,
      SAMPLE_RATE: SAMPLE_RATE,
      AUDIO_TYPE: AUDIO_TYPE,
      DEBUG_VERSION: DEBUG_VERSION,
      DEBUG_DATA: DEBUG_DATA,
      AUDIO_DATA_B: AUDIO_DATA_B
    },
    PEER_ADDRESS_2:{
      PEER_ADDRESS: PEER_ADDRESS,
      CONNECTION_HANDLE: CONNECTION_HANDLE,
      AUDIO_CONTROL_ATTR_HANDLE: AUDIO_CONTROL_ATTR_HANDLE,
      START: True or False,
      TIMESTAMP_STR_FORMAT: START_TIMESTAMP_STR_FORMAT,
      CODEC: CODEC,
      SAMPLE_RATE: SAMPLE_RATE,
      AUDIO_TYPE: AUDIO_TYPE,
      DEBUG_VERSION: DEBUG_VERSION,
      DEBUG_DATA: DEBUG_DATA,
      AUDIO_DATA_B: AUDIO_DATA_B
    }
  }
  """
    if key == PEER_ADDRESS:
        if audio_data.has_key(value):
            # Dump audio data and clear previous data.
            update_audio_data(key, value, START, False)
            # Extra clear CONNECTION_HANDLE due to new connection create.
            if audio_data[value].has_key(CONNECTION_HANDLE):
                audio_data[value].pop(CONNECTION_HANDLE, "")
        else:
            device_audio_data = {key: value}
            temp_audio_data = {value: device_audio_data}
            audio_data.update(temp_audio_data)
    else:
        for i in audio_data:
            if audio_data[i].has_key(relate_key) \
                and audio_data[i][relate_key] == relate_value:
                if key == START:
                    if audio_data[i].has_key(key) and audio_data[i][key]:
                        dump_audio_data(audio_data[i])
                    # Clear data except PEER_ADDRESS, CONNECTION_HANDLE and
                    # AUDIO_CONTROL_ATTR_HANDLE.
                    audio_data[i].pop(key, "")
                    audio_data[i].pop(TIMESTAMP_STR_FORMAT, "")
                    audio_data[i].pop(CODEC, "")
                    audio_data[i].pop(SAMPLE_RATE, "")
                    audio_data[i].pop(AUDIO_TYPE, "")
                    audio_data[i].pop(DEBUG_VERSION, "")
                    audio_data[i].pop(DEBUG_DATA, "")
                    audio_data[i].pop(AUDIO_DATA_B, "")
                elif key == AUDIO_DATA_B or key == DEBUG_DATA:
                    if audio_data[i].has_key(START) and audio_data[i][START]:
                        if audio_data[i].has_key(key):
                            ori_data = audio_data[i].pop(key, "")
                            value = ori_data + value
                    else:
                        # Audio doesn't start, don't record.
                        return
                device_audio_data = {key: value}
                audio_data[i].update(device_audio_data)


#=======================================================================
# Tool Function
#=======================================================================


def get_audio_control_attr_handle(connection_handle):
    """This function gets audio_control_attr_handle."""
    # If force_audio_control_attr_handle is set, will use it first.
    if force_audio_control_attr_handle is not None:
        return force_audio_control_attr_handle

    # Try to check the audio_control_attr_handle is record into audio_data.
    for i in audio_data:
        if audio_data[i].has_key(CONNECTION_HANDLE) \
            and audio_data[i][CONNECTION_HANDLE] == connection_handle:
            if audio_data[i].has_key(AUDIO_CONTROL_ATTR_HANDLE):
                return audio_data[i][AUDIO_CONTROL_ATTR_HANDLE]

    # Return default attr_handle if audio_data doesn't record it.
    return default_audio_control_attr_handle


def unpack_data(data, byte):
    """This function unpacks data."""
    if byte == 1:
        value = struct.unpack(">B", data[0])[0]
    elif byte == 2:
        value = struct.unpack(">H", data[1] + data[0])[0]
    else:
        value = ""
    data = data[byte:]
    return value, data


def convert_time_str(timestamp):
    """This function converts time to string format."""
    really_timestamp = float(timestamp) / SEC_CONVERT
    local_timestamp = time.localtime(really_timestamp)
    dt = really_timestamp - long(really_timestamp)
    ms_str = "{0:06}".format(int(round(dt * 1000000)))

    str_format = time.strftime("%m_%d__%H_%M_%S", local_timestamp)
    full_str_format = str_format + "_" + ms_str

    time_format = time.strftime("%m-%d %H:%M:%S", local_timestamp)
    full_time_format = time_format + "." + ms_str
    return full_str_format, full_time_format


def set_config():
    """This function is for set config by flag and check the argv is correct."""
    argv_parser = argparse.ArgumentParser(
        description="Extracts Hearing Aid audio data from BTSNOOP.")
    argv_parser.add_argument("BTSNOOP", help="BLUETOOTH BTSNOOP file.")
    argv_parser.add_argument(
        "-f", "--folder", help="select output folder.", dest="folder")
    argv_parser.add_argument(
        "-c1",
        "--connection-handle1",
        help="set a fake connection handle 1 to capture \
                           audio dump.",
        dest="connection_handle1",
        type=int)
    argv_parser.add_argument(
        "-c2",
        "--connection-handle2",
        help="set a fake connection handle 2 to capture \
                           audio dump.",
        dest="connection_handle2",
        type=int)
    argv_parser.add_argument(
        "-ns",
        "--no-start",
        help="No audio 'Start' cmd is \
                           needed before extracting audio data.",
        dest="no_start",
        default="False")
    argv_parser.add_argument(
        "-dc",
        "--default-codec",
        help="set a default \
                           codec.",
        dest="codec",
        default="G722")
    argv_parser.add_argument(
        "-a",
        "--attr-handle",
        help="force to select audio control attr handle.",
        dest="audio_control_attr_handle",
        type=int)
    argv_parser.add_argument(
        "-d",
        "--debug",
        help="dump full debug buffer content.",
        dest="full_debug",
        default="False")
    argv_parser.add_argument(
        "-sd",
        "--simple-debug",
        help="dump debug buffer header content.",
        dest="simple_debug",
        default="False")
    arg = argv_parser.parse_args()

    if arg.folder is not None:
        global folder
        folder = arg.folder

    if arg.connection_handle1 is not None and arg.connection_handle2 is not None \
        and arg.connection_handle1 == arg.connection_handle2:
        argv_parser.error("connection_handle1 can't be same with \
                          connection_handle2")
        exit(1)

    if not (arg.no_start.lower() == "true" or arg.no_start.lower() == "false"):
        argv_parser.error(
            "-ns/--no-start arg is invalid, it should be true/false.")
        exit(1)

    if arg.connection_handle1 is not None:
        fake_name = "ConnectionHandle" + str(arg.connection_handle1)
        update_audio_data("", "", PEER_ADDRESS, fake_name)
        update_audio_data(PEER_ADDRESS, fake_name, CONNECTION_HANDLE,
                          arg.connection_handle1)
        if arg.no_start.lower() == "true":
            update_audio_data(PEER_ADDRESS, fake_name, START, True)
            update_audio_data(PEER_ADDRESS, fake_name, TIMESTAMP_STR_FORMAT,
                              "Unknown")
            update_audio_data(PEER_ADDRESS, fake_name, CODEC, arg.codec)
            update_audio_data(PEER_ADDRESS, fake_name, SAMPLE_RATE, "Unknown")
            update_audio_data(PEER_ADDRESS, fake_name, AUDIO_TYPE, "Unknown")

    if arg.connection_handle2 is not None:
        fake_name = "ConnectionHandle" + str(arg.connection_handle2)
        update_audio_data("", "", PEER_ADDRESS, fake_name)
        update_audio_data(PEER_ADDRESS, fake_name, CONNECTION_HANDLE,
                          arg.connection_handle2)
        if arg.no_start.lower() == "true":
            update_audio_data(PEER_ADDRESS, fake_name, START, True)
            update_audio_data(PEER_ADDRESS, fake_name, TIMESTAMP_STR_FORMAT,
                              "Unknown")
            update_audio_data(PEER_ADDRESS, fake_name, CODEC, arg.codec)
            update_audio_data(PEER_ADDRESS, fake_name, SAMPLE_RATE, "Unknown")
            update_audio_data(PEER_ADDRESS, fake_name, AUDIO_TYPE, "Unknown")

    if arg.audio_control_attr_handle is not None:
        global force_audio_control_attr_handle
        force_audio_control_attr_handle = arg.audio_control_attr_handle

    global full_debug
    global simple_debug
    if arg.full_debug.lower() == "true":
        full_debug = True
        simple_debug = True
    elif arg.simple_debug.lower() == "true":
        simple_debug = True

    if os.path.isfile(arg.BTSNOOP):
        return arg.BTSNOOP
    else:
        argv_parser.error("BTSNOOP file not found: %s" % arg.BTSNOOP)
        exit(1)


def main():
    btsnoop_file_name = set_config()

    with open(btsnoop_file_name, "rb") as btsnoop_file:
        identification = btsnoop_file.read(8)
        if identification != "btsnoop\0":
            sys.stderr.write(
                "Check identification fail. It is not correct btsnoop file.")
            exit(1)

        ver, data_link = struct.unpack(">II", btsnoop_file.read(4 + 4))
        if (ver != 1) or (data_link != 1002):
            sys.stderr.write(
                "Check ver or dataLink fail. It is not correct btsnoop file.")
            exit(1)

        while True:
            if not parse_packet(btsnoop_file):
                break

        for i in audio_data:
            if audio_data[i].get(START, False):
                dump_audio_data(audio_data[i])


if __name__ == "__main__":
    main()
