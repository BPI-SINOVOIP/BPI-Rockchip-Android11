# Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging

from locale import *

PYSHARK_LOAD_TIMEOUT = 2
FRAME_FIELD_RADIOTAP_DATARATE = 'radiotap.datarate'
FRAME_FIELD_RADIOTAP_MCS_INDEX = 'radiotap.mcs_index'
FRAME_FIELD_WLAN_FRAME_TYPE = 'wlan.fc_type_subtype'
FRAME_FIELD_WLAN_SOURCE_ADDR = 'wlan.sa'
FRAME_FIELD_WLAN_MGMT_SSID = 'wlan_mgt.ssid'
RADIOTAP_KNOWN_BAD_FCS_REJECTOR = (
    'not radiotap.flags.badfcs or radiotap.flags.badfcs==0')
RADIOTAP_LOW_SIGNAL_REJECTOR = ('radiotap.dbm_antsignal > -85')
WLAN_BEACON_FRAME_TYPE = '0x08'
WLAN_BEACON_ACCEPTOR = 'wlan.fc.type_subtype==0x08'
WLAN_PROBE_REQ_FRAME_TYPE = '0x04'
WLAN_PROBE_REQ_ACCEPTOR = 'wlan.fc.type_subtype==0x04'
WLAN_QOS_NULL_TYPE = '0x2c'
PYSHARK_BROADCAST_SSID = 'SSID: '
BROADCAST_SSID = ''

setlocale(LC_ALL, '')

class Frame(object):
    """A frame from a packet capture."""
    TIME_FORMAT = "%H:%M:%S.%f"


    def __init__(self, frametime, bit_rate, mcs_index, ssid, source_addr,
                 frame_type):
        self._datetime = frametime
        self._bit_rate = bit_rate
        self._mcs_index = mcs_index
        self._ssid = ssid
        self._source_addr = source_addr
        self._frame_type = frame_type


    @property
    def time_datetime(self):
        """The time of the frame, as a |datetime| object."""
        return self._datetime


    @property
    def bit_rate(self):
        """The bitrate used to transmit the frame, as an int."""
        return self._bit_rate


    @property
    def frame_type(self):
        """802.11 type/subtype field, as a hex string."""
        return self._frame_type


    @property
    def mcs_index(self):
        """
        The MCS index used to transmit the frame, as an int.

        The value may be None, if the frame was not transmitted
        using 802.11n modes.
        """
        return self._mcs_index


    @property
    def ssid(self):
        """
        The SSID of the frame, as a string.

        The value may be None, if the frame does not have an SSID.
        """
        return self._ssid


    @property
    def source_addr(self):
        """The source address of the frame, as a string."""
        return self._source_addr


    @property
    def time_string(self):
        """The time of the frame, in local time, as a string."""
        return self._datetime.strftime(self.TIME_FORMAT)


    def __str__(self):
        return '%s: rate %s, MCS %s, SSID %s, SA %s, Type %s' % (
                self.time_datetime, self.bit_rate, self.mcs_index, self.ssid,
                self.source_addr, self.frame_type)


def _fetch_frame_field_value(frame, field):
    """
    Retrieve the value of |field| within the |frame|.

    @param frame: Pyshark packet object corresponding to a captured frame.
    @param field: Field for which the value needs to be extracted from |frame|.

    @return Value extracted from the frame if the field exists, else None.

    """
    layer_object = frame
    for layer in field.split('.'):
        try:
            layer_object = getattr(layer_object, layer)
        except AttributeError:
            return None
    return layer_object


def _open_capture(pcap_path, display_filter):
    """
    Get pyshark packet object parsed contents of a pcap file.

    @param pcap_path: string path to pcap file.
    @param display_filter: string filter to apply to captured frames.

    @return list of Pyshark packet objects.

    """
    import pyshark
    capture = pyshark.FileCapture(
        input_file=pcap_path, display_filter=display_filter)
    capture.load_packets(timeout=PYSHARK_LOAD_TIMEOUT)
    return capture


def get_frames(local_pcap_path, display_filter, reject_bad_fcs=True,
               reject_low_signal=False):
    """
    Get a parsed representation of the contents of a pcap file.
    If the RF shielding in the wificell or other chambers is imperfect,
    we'll see packets from the external environment in the packet capture
    and tests that assert if the packet capture has certain properties
    (i.e. only packets of a certain kind) will fail. A good way to reject
    these packets ("leakage from the outside world") is to look at signal
    strength. The DUT is usually either next to the AP or <5ft from the AP
    in these chambers. A signal strength of < -85 dBm in an incoming packet
    should imply it is leakage. The reject_low_signal option is turned off by
    default and external packets are part of the capture by default.
    Be careful to not turn on this option in an attenuated setup, where the
    DUT/AP packets will also have a low signal (i.e. network_WiFi_AttenPerf).

    @param local_pcap_path: string path to a local pcap file on the host.
    @param display_filter: string filter to apply to captured frames.
    @param reject_bad_fcs: bool, for frames with bad Frame Check Sequence.
    @param reject_low_signal: bool, for packets with signal < -85 dBm. These
                              are likely from the external environment and show
                              up due to poor shielding in the RF chamber.

    @return list of Frame structs.

    """
    if reject_bad_fcs is True:
        display_filter = '(%s) and (%s)' % (RADIOTAP_KNOWN_BAD_FCS_REJECTOR,
                                            display_filter)

    if reject_low_signal is True:
        display_filter = '(%s) and (%s)' % (RADIOTAP_LOW_SIGNAL_REJECTOR,
                                            display_filter)

    logging.debug('Capture: %s, Filter: %s', local_pcap_path, display_filter)
    capture_frames = _open_capture(local_pcap_path, display_filter)
    frames = []
    logging.info('Parsing frames')

    for frame in capture_frames:
        rate = _fetch_frame_field_value(frame, FRAME_FIELD_RADIOTAP_DATARATE)
        if rate:
            rate = atof(rate)
        else:
            logging.debug('Capture frame missing rate: %s', frame)

        frametime = frame.sniff_time

        mcs_index = _fetch_frame_field_value(
            frame, FRAME_FIELD_RADIOTAP_MCS_INDEX)
        if mcs_index:
            mcs_index = int(mcs_index)

        source_addr = _fetch_frame_field_value(
            frame, FRAME_FIELD_WLAN_SOURCE_ADDR)

        # Get the SSID for any probe requests
        frame_type = _fetch_frame_field_value(
            frame, FRAME_FIELD_WLAN_FRAME_TYPE)
        if (frame_type in [WLAN_BEACON_FRAME_TYPE, WLAN_PROBE_REQ_FRAME_TYPE]):
            ssid = _fetch_frame_field_value(frame, FRAME_FIELD_WLAN_MGMT_SSID)
            # Since the SSID name is a variable length field, there seems to be
            # a bug in the pyshark parsing, it returns 'SSID: ' instead of ''
            # for broadcast SSID's.
            if ssid == PYSHARK_BROADCAST_SSID:
                ssid = BROADCAST_SSID
        else:
            ssid = None

        frames.append(Frame(frametime, rate, mcs_index, ssid, source_addr,
                            frame_type=frame_type))

    return frames


def get_probe_ssids(local_pcap_path, probe_sender=None):
    """
    Get the SSIDs that were named in 802.11 probe requests frames.

    Parse a pcap, returning all the SSIDs named in 802.11 probe
    request frames. If |probe_sender| is specified, only probes
    from that MAC address will be considered.

    @param pcap_path: string path to a local pcap file on the host.
    @param remote_host: Host object (if the file is remote).
    @param probe_sender: MAC address of the device sending probes.

    @return: A frozenset of the SSIDs that were probed.

    """
    if probe_sender:
        display_filter = '%s and wlan.addr==%s' % (
                WLAN_PROBE_REQ_ACCEPTOR, probe_sender)
    else:
        display_filter = WLAN_PROBE_REQ_ACCEPTOR

    frames = get_frames(local_pcap_path, display_filter, reject_bad_fcs=True)

    return frozenset(
            [frame.ssid for frame in frames if frame.ssid is not None])
