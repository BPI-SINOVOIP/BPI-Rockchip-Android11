#!/usr/bin/env python3
###############################################################################################################
#
#  Copyright (C) 2019 Motorola Mobility LLC
#
#  Redistribution and use in source and binary forms, with or without modification, are permitted provided that
#  the following conditions are met:
#
#  1. Redistributions of source code must retain the above copyright notice, this list of conditions and the
#     following disclaimer.
#
#  2. Redistributions in binary form must reproduce the above copyright notice, this list of conditions and
#     the following disclaimer in the documentation and/or other materials provided with the distribution.
#
#  3. Neither the name of the copyright holder nor the names of its contributors may be used to endorse or
#     promote products derived from this software without specific prior written permission.
#
#  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED
#  WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
#  PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR
#  ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED
#  TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
#  HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
#  NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
#  POSSIBILITY OF SUCH DAMAGE.
#
###############################################################################################################
###############################################################################################################
#
#                             Bluetooth Virtual Sniffing for Android
#
#  This script supports Bluetooth Virtual Sniffing via Live Import Feature of Frontline Bluetooth Sniffer(FTS)
#
#  It extracts the HCI packets from Snoop logs and redirect it to FTS for live HCI sniffing.
#
#  It uses liveimport.ini and LiveImportAPI.dll from FTS path to communicate with FTS sniffer software.
#
#  It works on both Windows and Ubuntu. For Ubuntu, both FTS and Python should be installed on Wine.
#
#  FTS_INI_PATH & FTS_DLL_PATH should be set to absolute path of liveimport.ini and LiveImportAPI.dll of FTS
#
#  Example below - This may change per machine per FTS version in Windows vs Ubuntu (Wine), set accordingly.
#  For Windows
#    FTS_INI_PATH = 'C:\\Program Files (x86)\\Frontline Test System II\\Frontline 13.2\\'
#    FTS_DLL_PATH = 'C:\\Program Files (x86)\\Frontline Test System II\\Frontline 13.2\\Executables\\Core\\'
#
#  For Ubuntu - FTS path recognized by Wine (not Ubuntu path)
#    FTS_INI_PATH = 'C:\\Program Files\\Frontline Test System II\\Frontline 13.2\\'
#    FTS_DLL_PATH = 'C:\\Program Files\\Frontline Test System II\\Frontline 13.2\\Executables\\Core\\'
#
###############################################################################################################

import os
import platform
import socket
import struct
import subprocess
import sys
import time
if sys.version_info[0] >= 3:
    import configparser
else:
    import ConfigParser as configparser

from calendar import timegm
from ctypes import byref, c_bool, c_longlong, CDLL
from _ctypes import FreeLibrary
from datetime import datetime

# Update below to right path corresponding to your machine, FTS version and OS used.
FTS_INI_PATH = 'C:\\Program Files (x86)\\Frontline Test System II\\Frontline 15.12\\'
FTS_DLL_PATH = 'C:\\Program Files (x86)\\Frontline Test System II\\Frontline 15.12\\Executables\\Core\\'

iniName = 'liveimport.ini'
if (platform.architecture()[0] == '32bit'):
    dllName = 'LiveImportAPI.dll'
else:
    dllName = 'LiveImportAPI_x64.dll'

launchFtsCmd = '\"' + FTS_DLL_PATH + 'fts.exe\"' + ' \"/ComProbe Protocol Analysis System=Generic\"' + ' \"/oemkey=Virtual\"'

# Unix Epoch delta since 01/01/1970
FILETIME_EPOCH_DELTA = 116444736000000000
HUNDREDS_OF_NANOSECONDS = 10000000

HOST = 'localhost'
PORT = 8872
SNOOP_ID = 16
SNOOP_HDR = 24


def get_file_time():
    """
    Obtain current time in file time format for display
    """
    date_time = datetime.now()
    file_time = FILETIME_EPOCH_DELTA + (
        timegm(date_time.timetuple()) * HUNDREDS_OF_NANOSECONDS)
    file_time = file_time + (date_time.microsecond * 10)
    return file_time


def get_connection_string():
    """
    Read ConnectionString from liveimport.ini
    """
    config = configparser.ConfigParser()
    config.read(FTS_INI_PATH + iniName)
    try:
        conn_str = config.get('General', 'ConnectionString')
    except (configparser.NoSectionError, configparser.NoOptionError):
        return None

    return conn_str


def get_configuration_string():
    """
    Read Configuration string from liveimport.ini
    """
    config_str = ''
    config = configparser.ConfigParser()
    config.read(FTS_INI_PATH + iniName)
    try:
        config_items = config.items('Configuration')
    except (configparser.NoSectionError, configparser.NoOptionError):
        return None

    if config_items is not None:
        for item in config_items:
            key, value = item
            config_str += ("%s=%s\n" % (key, value))
        return config_str
    else:
        return None


def check_live_import_connection(live_import):
    """
    Launch FTS app in Virtual Sniffing Mode
    Check if FTS App is ready to start receiving the data.
    If not, wait until 1 min and exit if FTS didn't start.
    """
    is_connection_running = c_bool()
    count = 0

    status = live_import.IsAppReady(byref(is_connection_running))
    if (is_connection_running.value == True):
        print("FTS is already launched, Start capture if not already started")
        return True

    print("Launching FTS Virtual Sniffing")
    try:
        ftsProcess = subprocess.Popen((launchFtsCmd), stdout=subprocess.PIPE)
    except:
        print("Error in Launching FTS.. exiting")
        return False

    while (is_connection_running.value == False and count < 12):
        status = live_import.IsAppReady(byref(is_connection_running))
        if (status < 0):
            print("Live Import Internal Error %d" % (status))
            return False
        if (is_connection_running.value == False):
            print("Waiting for 5 sec.. Open FTS Virtual Sniffing")
            time.sleep(5)
            count += 1
    if (is_connection_running.value == True):
        print("FTS is ready to receive the data, Start capture now")
        return True
    else:
        print("FTS Virtual Sniffing didn't start until 1 min.. exiting")
        return False


def init_live_import(conn_str, config_str):
    """
    Load DLL and Initialize the LiveImport module for FTS.
    """
    success = c_bool()
    try:
        live_import = CDLL(FTS_DLL_PATH + dllName)
    except:
        return None

    if live_import is None:
        print("Error: Path to LiveImportAPI.dll is incorrect.. exiting")
        return None

    print(dllName + " loaded successfully")
    result = live_import.InitializeLiveImport(
        conn_str.encode('ascii', 'ignore'), config_str.encode(
            'ascii', 'ignore'), byref(success))
    if (result < 0):
        print("Live Import Init failed")
        return None
    else:
        print("Live Import Init success")
        return live_import


def release_live_import(live_import):
    """
    Cleanup and exit live import module.
    """
    if live_import is not None:
        live_import.ReleaseLiveImport()
        FreeLibrary(live_import._handle)


def main():

    print("Bluetooth Virtual Sniffing for Fluoride")
    connection_str = get_connection_string()
    if connection_str is None:
        print("Error: path to liveimport.ini is incorrect.. exiting")
        exit(0)

    configuration_str = get_configuration_string()
    if configuration_str is None:
        print("Error: path to liveimport.ini is incorrect.. exiting")
        exit(0)

    live_import = init_live_import(connection_str, configuration_str)
    if live_import is None:
        print("Error: Path to LiveImportAPI.dll is incorrect.. exiting")
        exit(0)

    if (check_live_import_connection(live_import) == False):
        release_live_import(live_import)
        exit(0)

    # Wait until the forward socket is ready
    print("Waiting until adb is ready")
    os.system('adb wait-for-device forward tcp:8872 tcp:8872')

    btsnoop_sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    btsnoop_sock.connect((HOST, PORT))
    snoop_id = btsnoop_sock.recv(SNOOP_ID)
    if not snoop_id.startswith(b"btsnoop"):
        print("Error: Snoop ID wasn't received.. exiting")
        release_live_import(live_import)
        exit(0)

    while True:
        try:
            snoop_hdr = btsnoop_sock.recv(SNOOP_HDR)
            if snoop_hdr is not None:
                try:
                    olen, ilen, flags = struct.unpack(">LLL", snoop_hdr[0:12])
                except struct.error:
                    print("Error: Invalid data", repr(snoop_hdr))
                    continue

                file_time = get_file_time()
                timestamp = c_longlong(file_time)

                snoop_data = b''
                while (len(snoop_data) < olen):
                    data_frag = btsnoop_sock.recv(olen - len(snoop_data))
                    if data_frag is not None:
                        snoop_data += data_frag

                print("Bytes received %d Olen %d ilen %d flags %d" %
                      (len(snoop_data), olen, ilen, flags))
                packet_type = struct.unpack(">B", snoop_data[0:1])[0]
                if packet_type == 1:
                    drf = 1
                    isend = 0
                elif packet_type == 2:
                    drf = 2
                    if (flags & 0x01):
                        isend = 1
                    else:
                        isend = 0
                elif packet_type == 3:
                    drf = 4
                    if (flags & 0x01):
                        isend = 1
                    else:
                        isend = 0
                elif packet_type == 4:
                    drf = 8
                    isend = 1

                result = live_import.SendFrame(olen - 1, olen - 1,
                                               snoop_data[1:olen], drf, isend,
                                               timestamp)
                if (result < 0):
                    print("Send frame failed")
        except KeyboardInterrupt:
            print("Cleanup and exit")
            release_live_import(live_import)
            btsnoop_sock.close()
            exit(0)


if __name__ == '__main__':
    main()
