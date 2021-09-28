# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import collections
import logging
import operator
import re

from autotest_lib.client.common_lib import error
from autotest_lib.client.common_lib import utils
from autotest_lib.client.common_lib.cros.network import iw_event_logger

# These must mirror the values in 'iw list' output.
CHAN_FLAG_DISABLED = 'disabled'
CHAN_FLAG_NO_IR = 'no IR'
CHAN_FLAG_PASSIVE_SCAN = 'passive scan'
CHAN_FLAG_RADAR_DETECT = 'radar detection'
DEV_MODE_AP = 'AP'
DEV_MODE_IBSS = 'IBSS'
DEV_MODE_MONITOR = 'monitor'
DEV_MODE_MESH_POINT = 'mesh point'
DEV_MODE_STATION = 'managed'
SUPPORTED_DEV_MODES = (DEV_MODE_AP, DEV_MODE_IBSS, DEV_MODE_MONITOR,
                       DEV_MODE_MESH_POINT, DEV_MODE_STATION)

class _PrintableWidth:
    """Printable width constant objects used by packet_capturer."""
    def __init__(self, name):
        self._name = name

    def __repr__(self):
        return '\'%s\'' % self._name

    def __str__(self):
        return self._name

WIDTH_HT20 = _PrintableWidth('HT20')
WIDTH_HT40_PLUS = _PrintableWidth('HT40+')
WIDTH_HT40_MINUS = _PrintableWidth('HT40-')
WIDTH_VHT80 = _PrintableWidth('VHT80')
WIDTH_VHT160 = _PrintableWidth('VHT160')
WIDTH_VHT80_80 = _PrintableWidth('VHT80+80')

VHT160_CENTER_CHANNELS = ('50','114')

SECURITY_OPEN = 'open'
SECURITY_WEP = 'wep'
SECURITY_WPA = 'wpa'
SECURITY_WPA2 = 'wpa2'
# Mixed mode security is WPA2/WPA
SECURITY_MIXED = 'mixed'

# Table of lookups between the output of item 'secondary channel offset:' from
# iw <device> scan to constants.

HT_TABLE = {'no secondary': WIDTH_HT20,
            'above': WIDTH_HT40_PLUS,
            'below': WIDTH_HT40_MINUS}

IwBand = collections.namedtuple(
    'Band', ['num', 'frequencies', 'frequency_flags', 'mcs_indices'])
IwBss = collections.namedtuple('IwBss', ['bss', 'frequency', 'ssid', 'security',
                                         'width', 'signal'])
IwNetDev = collections.namedtuple('IwNetDev', ['phy', 'if_name', 'if_type'])
IwTimedScan = collections.namedtuple('IwTimedScan', ['time', 'bss_list'])

# The fields for IwPhy are as follows:
#   name: string name of the phy, such as "phy0"
#   bands: list of IwBand objects.
#   modes: List of strings containing interface modes supported, such as "AP".
#   commands: List of strings containing nl80211 commands supported, such as
#          "authenticate".
#   features: List of strings containing nl80211 features supported, such as
#          "T-DLS".
#   max_scan_ssids: Maximum number of SSIDs which can be scanned at once.
IwPhy = collections.namedtuple(
    'Phy', ['name', 'bands', 'modes', 'commands', 'features',
            'max_scan_ssids', 'avail_tx_antennas', 'avail_rx_antennas',
            'supports_setting_antenna_mask', 'support_vht'])

DEFAULT_COMMAND_IW = 'iw'

# Redirect stderr to stdout on Cros since adb commands cannot distinguish them
# on Brillo.
IW_TIME_COMMAND_FORMAT = '(time -p %s) 2>&1'
IW_TIME_COMMAND_OUTPUT_START = 'real'

IW_LINK_KEY_BEACON_INTERVAL = 'beacon int'
IW_LINK_KEY_DTIM_PERIOD = 'dtim period'
IW_LINK_KEY_FREQUENCY = 'freq'
IW_LINK_KEY_SIGNAL = 'signal'
IW_LINK_KEY_RX_BITRATE = 'rx bitrate'
IW_LINK_KEY_RX_DROPS = 'rx drop misc'
IW_LINK_KEY_RX_PACKETS = 'rx packets'
IW_LINK_KEY_TX_BITRATE = 'tx bitrate'
IW_LINK_KEY_TX_FAILURES = 'tx failed'
IW_LINK_KEY_TX_PACKETS = 'tx packets'
IW_LINK_KEY_TX_RETRIES = 'tx retries'
IW_LOCAL_EVENT_LOG_FILE = './debug/iw_event_%d.log'

# Strings from iw/util.c describing supported HE features
HE_MAC_PLUS_HTC_HE = '+HTC HE Supported'
HE_MAC_TWT_REQUESTER = 'TWT Requester'
HE_MAC_TWT_RESPONDER = 'TWT Responder'
HE_MAC_DYNAMIC_BA_FRAGMENTATION = 'Dynamic BA Fragementation Level'
HE_MAC_MAX_MSDUS = 'Maximum number of MSDUS Fragments'
HE_MAC_MIN_PAYLOAD_128 = 'Minimum Payload size of 128 bytes'
HE_MAC_TRIGGER_FRAME_PADDING = 'Trigger Frame MAC Padding Duration'
HE_MAC_MULTI_TID_AGGREGATION = 'Multi-TID Aggregation Support'
HE_MAC_ALL_ACK = 'All Ack'
HE_MAC_TRS = 'TRS'
HE_MAC_BSR = 'BSR'
HE_MAC_TWT_BROADCAST = 'Broadcast TWT'
HE_MAC_32_BIT_BA_BITMAP = '32-bit BA Bitmap'
HE_MAC_MU_CASCADING = 'MU Cascading'
HE_MAC_ACK_AGGREGATION = 'Ack-Enabled Aggregation'
HE_MAC_OM_CONTROL = 'OM Control'
HE_MAC_OFDMA_RA = 'OFDMA RA'
HE_MAC_MAX_AMPDU_LENGTH_EXPONENT = 'Maximum A-MPDU Length Exponent'
HE_MAC_AMSDU_FRAGMENTATION = 'A-MSDU Fragmentation'
HE_MAC_FLEXIBLE_TWT = 'Flexible TWT Scheduling'
HE_MAC_RX_CONTROL_FRAME_TO_MULTIBSS = 'RX Control Frame to MultiBSS'
HE_MAC_BSRP_BQRP_AMPDU_AGGREGATION = 'BSRP BQRP A-MPDU Aggregation'
HE_MAC_QTP = 'QTP'
HE_MAC_BQR = 'BQR'
HE_MAC_SRP_RESPONDER_ROLE = 'SRP Responder Role'
HE_MAC_NDP_FEEDBACK_REPORT = 'NDP Feedback Report'
HE_MAC_OPS = 'OPS'
HE_MAC_AMSDU_IN_AMPDU = 'A-MSDU in A-MPDU'
HE_MAC_MULTI_TID_AGGREGATION_TX = 'Multi-TID Aggregation TX'
HE_MAC_SUBCHANNEL_SELECTIVE = 'HE Subchannel Selective Transmission'
HE_MAC_UL_2X966_TONE_RU = 'UL 2x996-Tone RU'
HE_MAC_OM_CONTROL_DISABLE_RX = 'OM Control UL MU Data Disable RX'

HE_PHY_24HE40 = 'HE40/2.4GHz'
HE_PHY_5HE40_80 = 'HE40/HE80/5GHz'
HE_PHY_5HE160 = 'HE160/5GHz'
HE_PHY_5HE160_80_80 = 'HE160/HE80+80/5GHz'
HE_PHY_242_TONE_RU_24 = '242 tone RUs/2.4GHz'
HE_PHY_242_TONE_RU_5 = '242 tone RUs/5GHz'
HE_PHY_PUNCTURED_PREAMBLE_RX = 'Punctured Preamble RX'
HE_PHY_DEVICE_CLASS = 'Device Class'
HE_PHY_LDPC_CODING_IN_PAYLOAD = 'LDPC Coding in Payload'
HE_PHY_HE_SU_PPDU_1X_HE_LTF_08_GI = 'HE SU PPDU with 1x HE-LTF and 0.8us GI'
HE_PHY_HE_MIDAMBLE_RX_MAX_NSTS = 'Midamble Rx Max NSTS'
HE_PHY_NDP_4X_HE_LTF_32_GI = 'NDP with 4x HE-LTF and 3.2us GI'
HE_PHY_STBC_TX_LEQ_80 = 'STBC Tx <= 80MHz'
HE_PHY_STBC_RX_LEQ_80 = 'STBC Rx <= 80MHz'
HE_PHY_DOPPLER_TX = 'Doppler Tx'
HE_PHY_DOPPLER_RX = 'Doppler Rx'
HE_PHY_FULL_BAND_UL_MU_MIMO = 'Full Bandwidth UL MU-MIMO'
HE_PHY_PART_BAND_UL_MU_MIMO = 'Partial Bandwidth UL MU-MIMO'
HE_PHY_DCM_MAX_CONSTELLATION = 'DCM Max Constellation'
HE_PHY_DCM_MAX_NSS_TX = 'DCM Max NSS Tx'
HE_PHY_DCM_MAX_CONSTELLATION_RX = 'DCM Max Constellation Rx'
HE_PHY_DCM_MAX_NSS_RX = 'DCM Max NSS Rx'
HE_PHY_RX_MU_PPDU_FROM_NON_AP = 'Rx HE MU PPDU from Non-AP STA'
HE_PHY_SU_BEAMFORMER = 'SU Beamformer'
HE_PHY_SU_BEAMFORMEE = 'SU Beamformee'
HE_PHY_MU_BEAMFORMER = 'MU Beamformer'
HE_PHY_BEAMFORMEE_STS_LEQ_80 = 'Beamformee STS <= 80Mhz'
HE_PHY_BEAMFORMEE_STS_GT_80 = 'Beamformee STS > 80Mhz'
HE_PHY_SOUNDING_DIMENSIONS_LEQ_80 = 'Sounding Dimensions <= 80Mhz'
HE_PHY_SOUNDING_DIMENSIONS_GT_80 = 'Sounding Dimensions > 80Mhz'
HE_PHY_NG_EQ_16_SU_FB = 'Ng = 16 SU Feedback'
HE_PHY_NG_EQ_16_MU_FB = 'Ng = 16 MU Feedback'
HE_PHY_CODEBOOK_SIZE_SU_FB = 'Codebook Size SU Feedback'
HE_PHY_CODEBOOK_SIZE_MU_FB = 'Codebook Size MU Feedback'
HE_PHY_TRIGGERED_SU_BEAMFORMING_FB = 'Triggered SU Beamforming Feedback'
HE_PHY_TRIGGERED_MU_BEAMFORMING_FB = 'Triggered MU Beamforming Feedback'
HE_PHY_TRIGGERED_CQI_FB = 'Triggered CQI Feedback'
HE_PHY_PART_BAND_EXT_RANGE = 'Partial Bandwidth Extended Range'
HE_PHY_PART_BAND_DL_MU_MIMO = 'Partial Bandwidth DL MU-MIMO'
HE_PHY_PPE_THRESHOLD = 'PPE Threshold Present'
HE_PHY_SRP_SR = 'SRP-based SR'
HE_PHY_POWER_BOOST_FACTOR_AR = 'Power Boost Factor ar'
HE_PHY_SU_PPDU_4X_HE_LTF_08_GI = 'HE SU PPDU & HE PPDU 4x HE-LTF 0.8us GI'
HE_PHY_MAX_NC = 'Max NC'
HE_PHY_STBC_TX_GT_80 = 'STBC Tx > 80MHz'
HE_PHY_STBC_RX_GT_80 = 'STBC Rx > 80MHz'
HE_PHY_ER_SU_PPDU_4X_HE_LTF_08_GI = 'HE ER SU PPDU 4x HE-LTF 0.8us GI'
HE_PHY_20_IN_44_PPDU_24 = '20MHz in 40MHz HE PPDU 2.4GHz'
HE_PHY_20_IN_160_80_80 = '20MHz in 160/80+80MHz HE PPDU'
HE_PHY_80_IN_160_80_80 = '80MHz in 160/80+80MHz HE PPDU'
HE_PHY_ER_SU_PPDU_1X_HE_LTF_08_GI = 'HE ER SU PPDU 1x HE-LTF 0.8us GI'
HE_PHY_MIDAMBLE_RX_2X_AND_1X_HE_LTF = 'Midamble Rx 2x & 1x HE-LTF'
HE_PHY_DCM_MAX_BW = 'DCM Max BW'
HE_PHY_LONGER_THAN_16HE_OFDM_SYM = 'Longer Than 16HE SIG-B OFDM Symbols'
HE_PHY_NON_TRIGGERED_CQI_FB = 'Non-Triggered CQI Feedback'
HE_PHY_TX_1024_QAM = 'TX 1024-QAM'
HE_PHY_RX_1024_QAM = 'RX 1024-QAM'
HE_PHY_RX_FULL_BW_SU_USING_MU_COMPRESSION_SIGB = \
        'RX Full BW SU Using HE MU PPDU with Compression SIGB'
HE_PHY_RX_FULL_BW_SU_USING_MU_NON_COMPRESSION_SIGB = \
        'RX Full BW SU Using HE MU PPDU with Non-Compression SIGB'


def _get_all_link_keys(link_information):
    """Parses link or station dump output for link key value pairs.

    Link or station dump information is in the format below:

    Connected to 74:e5:43:10:4f:c0 (on wlan0)
          SSID: PMKSACaching_4m9p5_ch1
          freq: 5220
          RX: 5370 bytes (37 packets)
          TX: 3604 bytes (15 packets)
          signal: -59 dBm
          tx bitrate: 13.0 MBit/s MCS 1

          bss flags:      short-slot-time
          dtim period:    5
          beacon int:     100

    @param link_information: string containing the raw link or station dump
        information as reported by iw. Note that this parsing assumes a single
        entry, in the case of multiple entries (e.g. listing stations from an
        AP, or listing mesh peers), the entries must be split on a per
        peer/client basis before this parsing operation.
    @return a dictionary containing all the link key/value pairs.

    """
    link_key_value_pairs = {}
    keyval_regex = re.compile(r'^\s+(.*):\s+(.*)$')
    for link_key in link_information.splitlines()[1:]:
        match = re.search(keyval_regex, link_key)
        if match:
            # Station dumps can contain blank lines.
            link_key_value_pairs[match.group(1)] = match.group(2)
    return link_key_value_pairs


def _extract_bssid(link_information, interface_name, station_dump=False):
    """Get the BSSID that |interface_name| is associated with.

    See doc for _get_all_link_keys() for expected format of the station or link
    information entry.

    @param link_information: string containing the raw link or station dump
        information as reported by iw. Note that this parsing assumes a single
        entry, in the case of multiple entries (e.g. listing stations from an AP
        or listing mesh peers), the entries must be split on a per peer/client
        basis before this parsing operation.
    @param interface_name: string name of interface (e.g. 'wlan0').
    @param station_dump: boolean indicator of whether the link information is
        from a 'station dump' query. If False, it is assumed the string is from
        a 'link' query.
    @return string bssid of the current association, or None if no matching
        association information is found.

    """
    # We're looking for a line like this when parsing the output of a 'link'
    # query:
    #   Connected to 04:f0:21:03:7d:bb (on wlan0)
    # We're looking for a line like this when parsing the output of a
    # 'station dump' query:
    #   Station 04:f0:21:03:7d:bb (on mesh-5000mhz)
    identifier = 'Station' if station_dump else 'Connected to'
    search_re = r'%s ([0-9a-fA-F:]{17}) \(on %s\)' % (identifier,
                                                      interface_name)
    match = re.match(search_re, link_information)
    if match is None:
        return None
    return match.group(1)


class IwRunner(object):
    """Defines an interface to the 'iw' command."""


    def __init__(self, remote_host=None, command_iw=DEFAULT_COMMAND_IW):
        self._run = utils.run
        self._host = remote_host
        if remote_host:
            self._run = remote_host.run
        self._command_iw = command_iw
        self._log_id = 0


    def _parse_scan_results(self, output):
        """Parse the output of the 'scan' and 'scan dump' commands.

        Here is an example of what a single network would look like for
        the input parameter.  Some fields have been removed in this example:
          BSS 00:11:22:33:44:55(on wlan0)
          freq: 2447
          beacon interval: 100 TUs
          signal: -46.00 dBm
          Information elements from Probe Response frame:
          SSID: my_open_network
          Extended supported rates: 24.0 36.0 48.0 54.0
          HT capabilities:
          Capabilities: 0x0c
          HT20
          HT operation:
          * primary channel: 8
          * secondary channel offset: no secondary
          * STA channel width: 20 MHz
          RSN: * Version: 1
          * Group cipher: CCMP
          * Pairwise ciphers: CCMP
          * Authentication suites: PSK
          * Capabilities: 1-PTKSA-RC 1-GTKSA-RC (0x0000)

        @param output: string command output.

        @returns a list of IwBss namedtuples; None if the scan fails

        """
        bss = None
        frequency = None
        ssid = None
        ht = None
        vht = None
        signal = None
        security = None
        supported_securities = []
        bss_list = []
        # TODO(crbug.com/1032892): The parsing logic here wasn't really designed
        # for the presence of multiple information elements like HT, VHT, and
        # (eventually) HE. We should eventually update it to check that we are
        # in the right section (e.g., verify the '* channel width' match is a
        # match in the VHT section and not a different section). Also, we should
        # probably add in VHT20, and VHT40 whenever we finish this bug.
        for line in output.splitlines():
            line = line.strip()
            bss_match = re.match('BSS ([0-9a-f:]+)', line)
            if bss_match:
                if bss != None:
                    security = self.determine_security(supported_securities)
                    iwbss = IwBss(bss, frequency, ssid, security,
                                  vht if vht else ht, signal)
                    bss_list.append(iwbss)
                    bss = frequency = ssid = security = ht = vht = None
                    supported_securities = []
                bss = bss_match.group(1)
            if line.startswith('freq:'):
                frequency = int(line.split()[1])
            if line.startswith('signal:'):
                signal = float(line.split()[1])
            if line.startswith('SSID: '):
                _, ssid = line.split(': ', 1)
            if line.startswith('* secondary channel offset'):
                ht = HT_TABLE[line.split(':')[1].strip()]
            # Checking for the VHT channel width based on IEEE 802.11-2016
            # Table 9-252.
            if line.startswith('* channel width:'):
                chan_width_subfield = line.split(':')[1].strip()[0]
                if chan_width_subfield == '1':
                    vht = WIDTH_VHT80
                # 2 and 3 are deprecated but are included here for older APs.
                if chan_width_subfield == '2':
                    vht = WIDTH_VHT160
                if chan_width_subfield == '3':
                    vht = WIDTH_VHT80_80
            if line.startswith('* center freq segment 2:'):
                center_chan_two = line.split(':')[1].strip()
                if vht == WIDTH_VHT80:
                    if center_chan_two in VHT160_CENTER_CHANNELS:
                        vht = WIDTH_VHT160
                    elif center_chan_two != '0':
                        vht = WIDTH_VHT80_80
            if line.startswith('WPA'):
               supported_securities.append(SECURITY_WPA)
            if line.startswith('RSN'):
               supported_securities.append(SECURITY_WPA2)
        security = self.determine_security(supported_securities)
        bss_list.append(IwBss(bss, frequency, ssid, security,
                              vht if vht else ht, signal))
        return bss_list


    def _parse_scan_time(self, output):
        """
        Parse the scan time in seconds from the output of the 'time -p "scan"'
        command.

        'time -p' Command output format is below:
        real     0.01
        user     0.01
        sys      0.00

        @param output: string command output.

        @returns float time in seconds.

        """
        output_lines = output.splitlines()
        for line_num, line in enumerate(output_lines):
            line = line.strip()
            if (line.startswith(IW_TIME_COMMAND_OUTPUT_START) and
                output_lines[line_num + 1].startswith('user') and
                output_lines[line_num + 2].startswith('sys')):
                return float(line.split()[1])
        raise error.TestFail('Could not parse scan time.')


    def add_interface(self, phy, interface, interface_type):
        """
        Add an interface to a WiFi PHY.

        @param phy: string name of PHY to add an interface to.
        @param interface: string name of interface to add.
        @param interface_type: string type of interface to add (e.g. 'monitor').

        """
        self._run('%s phy %s interface add %s type %s' %
                  (self._command_iw, phy, interface, interface_type))


    def disconnect_station(self, interface):
        """
        Disconnect a STA from a network.

        @param interface: string name of interface to disconnect.

        """
        self._run('%s dev %s disconnect' % (self._command_iw, interface))


    def get_current_bssid(self, interface_name):
        """Get the BSSID that |interface_name| is associated with.

        @param interface_name: string name of interface (e.g. 'wlan0').
        @return string bssid of our current association, or None.

        """
        result = self._run('%s dev %s link' %
                           (self._command_iw, interface_name),
                           ignore_status=True)
        if result.exit_status:
            # See comment in get_link_value.
            return None

        return _extract_bssid(result.stdout, interface_name)


    def get_interface(self, interface_name):
        """Get full information about an interface given an interface name.

        @param interface_name: string name of interface (e.g. 'wlan0').
        @return IwNetDev tuple.

        """
        matching_interfaces = [iw_if for iw_if in self.list_interfaces()
                                     if iw_if.if_name == interface_name]
        if len(matching_interfaces) != 1:
            raise error.TestFail('Could not find interface named %s' %
                                 interface_name)

        return matching_interfaces[0]


    def get_link_value(self, interface, iw_link_key):
        """Get the value of a link property for |interface|.

        Checks the link using iw, and parses the result to return a link key.

        @param iw_link_key: string one of IW_LINK_KEY_* defined above.
        @param interface: string desired value of iw link property.
        @return string containing the corresponding link property value, None
            if there was a parsing error or the iw command failed.

        """
        result = self._run('%s dev %s link' % (self._command_iw, interface),
                           ignore_status=True)
        if result.exit_status:
            # When roaming, there is a period of time for mac80211 based drivers
            # when the driver is 'associated' with an SSID but not a particular
            # BSS.  This causes iw to return an error code (-2) when attempting
            # to retrieve information specific to the BSS.  This does not happen
            # in mwifiex drivers.
            return None
        actual_value = _get_all_link_keys(result.stdout).get(iw_link_key)
        if actual_value is not None:
            logging.info('Found iw link key %s with value %s.',
                         iw_link_key, actual_value)
        return actual_value


    def get_station_dump(self, interface):
        """Gets information about connected peers.

        Returns information about the currently connected peers. When the host
        is in station mode, it returns a single entry, with information about
        the link to the AP it is currently connected to. If the host is in mesh
        or AP mode, it can return multiple entries, one for each connected
        station, or mesh peer.

        @param interface: string name of interface to get peer information
            from.
        @return a list of dictionaries with link information about each
            connected peer (ordered by peer mac address).

        """
        result = self._run('%s dev %s station dump' %
                           (self._command_iw, interface))
        parts = re.split(r'^Station ', result.stdout, flags=re.MULTILINE)[1:]
        peer_list_raw = ['Station ' + x for x in parts]
        parsed_peer_info = []

        for peer in peer_list_raw:
            peer_link_keys = _get_all_link_keys(peer)
            rssi_str = peer_link_keys.get(IW_LINK_KEY_SIGNAL, '0')
            rssi_int = int(rssi_str.split()[0])

            tx_bitrate = peer_link_keys.get(IW_LINK_KEY_TX_BITRATE, '0')
            tx_failures = int(peer_link_keys.get(IW_LINK_KEY_TX_FAILURES, 0))
            tx_packets = int(peer_link_keys.get(IW_LINK_KEY_TX_PACKETS, 0))
            tx_retries = int(peer_link_keys.get(IW_LINK_KEY_TX_RETRIES, 0))

            rx_bitrate = peer_link_keys.get(IW_LINK_KEY_RX_BITRATE, '0')
            rx_drops = int(peer_link_keys.get(IW_LINK_KEY_RX_DROPS, 0))
            rx_packets = int(peer_link_keys.get(IW_LINK_KEY_RX_PACKETS, 0))

            mac = _extract_bssid(link_information=peer,
                                 interface_name=interface,
                                 station_dump=True)

            # If any of these are missing, they will be None
            peer_info = {'rssi_int': rssi_int,
                         'rssi_str': rssi_str,
                         'tx_bitrate': tx_bitrate,
                         'tx_failures': tx_failures,
                         'tx_packets': tx_packets,
                         'tx_retries': tx_retries,
                         'rx_bitrate': rx_bitrate,
                         'rx_drops': rx_drops,
                         'rx_packets': rx_packets,
                         'mac': mac}

            # don't evaluate if tx_packets 0
            if tx_packets:
                peer_info['tx_retry_rate'] = tx_retries / float(tx_packets)
                peer_info['tx_failure_rate'] =  tx_failures / float(tx_packets)

            # don't evaluate if rx_packets is 0
            if rx_packets:
                peer_info['rx_drop_rate'] = rx_drops / float(rx_packets)

            parsed_peer_info.append(peer_info)
        return sorted(parsed_peer_info, key=operator.itemgetter('mac'))


    def get_operating_mode(self, interface):
        """Gets the operating mode for |interface|.

        @param interface: string name of interface to get peer information
            about.

        @return string one of DEV_MODE_* defined above, or None if no mode is
            found, or if an unsupported mode is found.

        """
        ret = self._run('%s dev %s info' % (self._command_iw, interface))
        mode_regex = r'^\s*type (.*)$'
        match = re.search(mode_regex, ret.stdout, re.MULTILINE)
        if match:
            operating_mode = match.group(1)
            if operating_mode in SUPPORTED_DEV_MODES:
                return operating_mode
            logging.warning(
                'Unsupported operating mode %s found for interface: %s. '
                'Supported modes: %s', operating_mode, interface,
                SUPPORTED_DEV_MODES)
        return None


    def get_radio_config(self, interface):
        """Gets the channel information of a specfic interface using iw.

        @param interface: string name of interface to get radio information
            from.

        @return dictionary containing the channel information.

        """
        channel_config = {}
        ret = self._run('%s dev %s info' % (self._command_iw, interface))
        channel_config_regex = (r'^\s*channel ([0-9]+) \(([0-9]+) MHz\), '
                                 'width: ([2,4,8]0) MHz, center1: ([0-9]+) MHz')
        match = re.search(channel_config_regex, ret.stdout, re.MULTILINE)

        if match:
            channel_config['number'] = int(match.group(1))
            channel_config['freq'] = int(match.group(2))
            channel_config['width'] = int(match.group(3))
            channel_config['center1_freq'] = int(match.group(4))

        return channel_config


    def ibss_join(self, interface, ssid, frequency):
        """
        Join a WiFi interface to an IBSS.

        @param interface: string name of interface to join to the IBSS.
        @param ssid: string SSID of IBSS to join.
        @param frequency: int frequency of IBSS in Mhz.

        """
        self._run('%s dev %s ibss join %s %d' %
                  (self._command_iw, interface, ssid, frequency))


    def ibss_leave(self, interface):
        """
        Leave an IBSS.

        @param interface: string name of interface to remove from the IBSS.

        """
        self._run('%s dev %s ibss leave' % (self._command_iw, interface))


    def list_interfaces(self, desired_if_type=None):
        """List WiFi related interfaces on this system.

        @param desired_if_type: string type of interface to filter
                our returned list of interfaces for (e.g. 'managed').

        @return list of IwNetDev tuples.

        """

        # Parse output in the following format:
        #
        #   $ adb shell iw dev
        #   phy#0
        #     Unnamed/non-netdev interface
        #       wdev 0x2
        #       addr aa:bb:cc:dd:ee:ff
        #       type P2P-device
        #     Interface wlan0
        #       ifindex 4
        #       wdev 0x1
        #       addr aa:bb:cc:dd:ee:ff
        #       ssid Whatever
        #       type managed

        output = self._run('%s dev' % self._command_iw).stdout
        interfaces = []
        phy = None
        if_name = None
        if_type = None
        for line in output.splitlines():
            m = re.match('phy#([0-9]+)', line)
            if m:
                phy = 'phy%d' % int(m.group(1))
                if_name = None
                if_type = None
                continue
            if not phy:
                continue
            m = re.match('[\s]*Interface (.*)', line)
            if m:
                if_name = m.group(1)
                continue
            if not if_name:
                continue
            # Common values for type are 'managed', 'monitor', and 'IBSS'.
            m = re.match('[\s]*type ([a-zA-Z]+)', line)
            if m:
                if_type = m.group(1)
                interfaces.append(IwNetDev(phy=phy, if_name=if_name,
                                           if_type=if_type))
                # One phy may have many interfaces, so don't reset it.
                if_name = None

        if desired_if_type:
            interfaces = [interface for interface in interfaces
                          if interface.if_type == desired_if_type]
        return interfaces


    def list_phys(self):
        """
        List WiFi PHYs on the given host.

        @return list of IwPhy tuples.

        """
        output = self._run('%s list' % self._command_iw).stdout

        pending_phy_name = None
        current_band = None
        current_section = None
        all_phys = []

        def add_pending_phy():
            """Add the pending phy into |all_phys|."""
            bands = tuple(IwBand(band.num,
                                 tuple(band.frequencies),
                                 dict(band.frequency_flags),
                                 tuple(band.mcs_indices))
                          for band in pending_phy_bands)
            new_phy = IwPhy(pending_phy_name,
                            bands,
                            tuple(pending_phy_modes),
                            tuple(pending_phy_commands),
                            tuple(pending_phy_features),
                            pending_phy_max_scan_ssids,
                            pending_phy_tx_antennas,
                            pending_phy_rx_antennas,
                            pending_phy_tx_antennas and pending_phy_rx_antennas,
                            pending_phy_support_vht)
            all_phys.append(new_phy)

        for line in output.splitlines():
            match_phy = re.search('Wiphy (.*)', line)
            if match_phy:
                if pending_phy_name:
                    add_pending_phy()
                pending_phy_name = match_phy.group(1)
                pending_phy_bands = []
                pending_phy_modes = []
                pending_phy_commands = []
                pending_phy_features = []
                pending_phy_max_scan_ssids = None
                pending_phy_tx_antennas = 0
                pending_phy_rx_antennas = 0
                pending_phy_support_vht = False
                continue

            match_section = re.match('\s*(\w.*):\s*$', line)
            if match_section:
                current_section = match_section.group(1)
                match_band = re.match('Band (\d+)', current_section)
                if match_band:
                    current_band = IwBand(num=int(match_band.group(1)),
                                          frequencies=[],
                                          frequency_flags={},
                                          mcs_indices=[])
                    pending_phy_bands.append(current_band)
                continue

            # Check for max_scan_ssids. This isn't a section, but it
            # also isn't within a section.
            match_max_scan_ssids = re.match('\s*max # scan SSIDs: (\d+)',
                                            line)
            if match_max_scan_ssids and pending_phy_name:
                pending_phy_max_scan_ssids = int(
                    match_max_scan_ssids.group(1))
                continue

            if (current_section == 'Supported interface modes' and
                pending_phy_name):
                mode_match = re.search('\* (\w+)', line)
                if mode_match:
                    pending_phy_modes.append(mode_match.group(1))
                    continue

            if current_section == 'Supported commands' and pending_phy_name:
                command_match = re.search('\* (\w+)', line)
                if command_match:
                    pending_phy_commands.append(command_match.group(1))
                    continue

            if (current_section is not None and
                current_section.startswith('VHT Capabilities') and
                pending_phy_name):
                pending_phy_support_vht = True
                continue

            match_avail_antennas = re.match('\s*Available Antennas: TX (\S+)'
                                            ' RX (\S+)', line)
            if match_avail_antennas and pending_phy_name:
                pending_phy_tx_antennas = int(
                        match_avail_antennas.group(1), 16)
                pending_phy_rx_antennas = int(
                        match_avail_antennas.group(2), 16)
                continue

            match_device_support = re.match('\s*Device supports (.*)\.', line)
            if match_device_support and pending_phy_name:
                pending_phy_features.append(match_device_support.group(1))
                continue

            if not all([current_band, pending_phy_name,
                        line.startswith('\t')]):
                continue

            # E.g.
            # * 2412 MHz [1] (20.0 dBm)
            # * 2467 MHz [12] (20.0 dBm) (passive scan)
            # * 2472 MHz [13] (disabled)
            # * 5260 MHz [52] (19.0 dBm) (no IR, radar detection)
            match_chan_info = re.search(
                r'(?P<frequency>\d+) MHz'
                r' (?P<chan_num>\[\d+\])'
                r'(?: \((?P<tx_power_limit>[0-9.]+ dBm)\))?'
                r'(?: \((?P<flags>[a-zA-Z, ]+)\))?', line)
            if match_chan_info:
                frequency = int(match_chan_info.group('frequency'))
                current_band.frequencies.append(frequency)
                flags_string = match_chan_info.group('flags')
                if flags_string:
                    current_band.frequency_flags[frequency] = frozenset(
                        flags_string.split(','))
                else:
                    # Populate the dict with an empty set, to make
                    # things uniform for client code.
                    current_band.frequency_flags[frequency] = frozenset()
                continue

            # re_mcs needs to match something like:
            # HT TX/RX MCS rate indexes supported: 0-15, 32
            if re.search('HT TX/RX MCS rate indexes supported: ', line):
                rate_string = line.split(':')[1].strip()
                for piece in rate_string.split(','):
                    if piece.find('-') > 0:
                        # Must be a range like '  0-15'
                        begin, end = piece.split('-')
                        for index in range(int(begin), int(end) + 1):
                            current_band.mcs_indices.append(index)
                    else:
                        # Must be a single rate like '32   '
                        current_band.mcs_indices.append(int(piece))
        if pending_phy_name:
            add_pending_phy()
        return all_phys


    def remove_interface(self, interface, ignore_status=False):
        """
        Remove a WiFi interface from a PHY.

        @param interface: string name of interface (e.g. mon0)
        @param ignore_status: boolean True iff we should ignore failures
                to remove the interface.

        """
        self._run('%s dev %s del' % (self._command_iw, interface),
                  ignore_status=ignore_status)


    def determine_security(self, supported_securities):
        """Determines security from the given list of supported securities.

        @param supported_securities: list of supported securities from scan

        """
        if not supported_securities:
            security = SECURITY_OPEN
        elif len(supported_securities) == 1:
            security = supported_securities[0]
        else:
            security = SECURITY_MIXED
        return security


    def scan(self, interface, frequencies=(), ssids=()):
        """Performs a scan.

        @param interface: the interface to run the iw command against
        @param frequencies: list of int frequencies in Mhz to scan.
        @param ssids: list of string SSIDs to send probe requests for.

        @returns a list of IwBss namedtuples; None if the scan fails

        """
        scan_result = self.timed_scan(interface, frequencies, ssids)
        if scan_result is None:
            return None
        return scan_result.bss_list


    def timed_scan(self, interface, frequencies=(), ssids=()):
        """Performs a timed scan.

        @param interface: the interface to run the iw command against
        @param frequencies: list of int frequencies in Mhz to scan.
        @param ssids: list of string SSIDs to send probe requests for.

        @returns a IwTimedScan namedtuple; None if the scan fails

        """
        freq_param = ''
        if frequencies:
            freq_param = ' freq %s' % ' '.join(map(str, frequencies))
        ssid_param = ''
        if ssids:
           ssid_param = ' ssid "%s"' % '" "'.join(ssids)

        iw_command = '%s dev %s scan%s%s' % (self._command_iw,
                interface, freq_param, ssid_param)
        command = IW_TIME_COMMAND_FORMAT % iw_command
        scan = self._run(command, ignore_status=True)
        if scan.exit_status != 0:
            # The device was busy
            logging.debug('scan exit_status: %d', scan.exit_status)
            return None
        if not scan.stdout:
            raise error.TestFail('Missing scan parse time')

        if scan.stdout.startswith(IW_TIME_COMMAND_OUTPUT_START):
            logging.debug('Empty scan result')
            bss_list = []
        else:
            bss_list = self._parse_scan_results(scan.stdout)
        scan_time = self._parse_scan_time(scan.stdout)
        return IwTimedScan(scan_time, bss_list)


    def scan_dump(self, interface):
        """Dump the contents of the scan cache.

        Note that this does not trigger a scan.  Instead, it returns
        the kernel's idea of what BSS's are currently visible.

        @param interface: the interface to run the iw command against

        @returns a list of IwBss namedtuples; None if the scan fails

        """
        result = self._run('%s dev %s scan dump' % (self._command_iw,
                                                    interface))
        return self._parse_scan_results(result.stdout)


    def set_tx_power(self, interface, power):
        """
        Set the transmission power for an interface.

        @param interface: string name of interface to set Tx power on.
        @param power: string power parameter. (e.g. 'auto').

        """
        self._run('%s dev %s set txpower %s' %
                  (self._command_iw, interface, power))


    def set_freq(self, interface, freq):
        """
        Set the frequency for an interface.

        @param interface: string name of interface to set frequency on.
        @param freq: int frequency

        """
        self._run('%s dev %s set freq %d' %
                  (self._command_iw, interface, freq))


    def set_regulatory_domain(self, domain_string):
        """
        Set the regulatory domain of the current machine.  Note that
        the regulatory change happens asynchronously to the exit of
        this function.

        @param domain_string: string regulatory domain name (e.g. 'US').

        """
        self._run('%s reg set %s' % (self._command_iw, domain_string))


    def get_regulatory_domain(self):
        """
        Get the regulatory domain of the current machine.

        @returns a string containing the 2-letter regulatory domain name
            (e.g. 'US').

        """
        output = self._run('%s reg get' % self._command_iw).stdout
        m = re.search('^country (..):', output, re.MULTILINE)
        if not m:
            return None
        return m.group(1)


    def is_regulatory_self_managed(self):
        """
        Determine if any WiFi device on the system manages its own regulatory
        info (NL80211_ATTR_WIPHY_SELF_MANAGED_REG).

        @returns True if self-managed, False otherwise.
        """
        output = self._run('%s reg get' % self._command_iw).stdout
        m = re.search('^phy#.*\(self-managed\)', output, re.MULTILINE)
        return not m is None


    def wait_for_scan_result(self, interface, bsses=(), ssids=(),
                             timeout_seconds=30, wait_for_all=False):
        """Returns a list of IWBSS objects for given list of bsses or ssids.

        This method will scan for a given timeout and return all of the networks
        that have a matching ssid or bss.  If wait_for_all is true and all
        networks are not found within the given timeout an empty list will
        be returned.

        @param interface: which interface to run iw against
        @param bsses: a list of BSS strings
        @param ssids: a list of ssid strings
        @param timeout_seconds: the amount of time to wait in seconds
        @param wait_for_all: True to wait for all listed bsses or ssids; False
                             to return if any of the networks were found

        @returns a list of IwBss collections that contain the given bss or ssid;
            if the scan is empty or returns an error code None is returned.

        """

        logging.info('Performing a scan with a max timeout of %d seconds.',
                     timeout_seconds)

        # If the in-progress scan takes more than 30 seconds to
        # complete it will most likely never complete; abort.
        # See crbug.com/309148
        scan_results = list()
        try:
            scan_results = utils.poll_for_condition(
                    condition=lambda: self.scan(interface),
                    timeout=timeout_seconds,
                    sleep_interval=5, # to allow in-progress scans to complete
                    desc='Timed out getting IWBSSes that match desired')
        except utils.TimeoutError as e:
            pass

        if not scan_results: # empty list or None
            return None

        # get all IWBSSes from the scan that match any of the desired
        # ssids or bsses passed in
        matching_iwbsses = filter(
                lambda iwbss: iwbss.ssid in ssids or iwbss.bss in bsses,
                scan_results)
        if wait_for_all:
            found_bsses = [iwbss.bss for iwbss in matching_iwbsses]
            found_ssids = [iwbss.ssid for iwbss in matching_iwbsses]
            # if an expected bss or ssid was not found, and it was required
            # by the caller that all expected be found, return empty list
            if any(bss not in found_bsses for bss in bsses) or any(
                    ssid not in found_ssids for ssid in ssids):
                return list()
        return list(matching_iwbsses)


    def set_antenna_bitmap(self, phy, tx_bitmap, rx_bitmap):
        """Set antenna chain mask on given phy (radio).

        This function will set the antennas allowed to use for TX and
        RX on the |phy| based on the |tx_bitmap| and |rx_bitmap|.
        This command is only allowed when the interfaces on the phy are down.

        @param phy: phy name
        @param tx_bitmap: bitmap of allowed antennas to use for TX
        @param rx_bitmap: bitmap of allowed antennas to use for RX

        """
        command = '%s phy %s set antenna %d %d' % (self._command_iw, phy,
                                                   tx_bitmap, rx_bitmap)
        self._run(command)


    def get_event_logger(self):
        """Create and return a IwEventLogger object.

        @returns a IwEventLogger object.

        """
        local_file = IW_LOCAL_EVENT_LOG_FILE % (self._log_id)
        self._log_id += 1
        return iw_event_logger.IwEventLogger(self._host, self._command_iw,
                                             local_file)


    def vht_supported(self):
        """Returns True if VHT is supported; False otherwise."""
        result = self._run('%s list' % self._command_iw).stdout
        if 'VHT Capabilities' in result:
            return True
        return False


    def he_supported(self):
        """Returns True if HE (802.11ax) is supported; False otherwise."""
        result = self._run('%s list' % self._command_iw).stdout
        if 'HE MAC Capabilities' in result:
            return True
        return False


    def frequency_supported(self, frequency):
        """Returns True if the given frequency is supported; False otherwise.

        @param frequency: int Wifi frequency to check if it is supported by
                          DUT.
        """
        phys = self.list_phys()
        for phy in phys:
            for band in phy.bands:
                if frequency in band.frequencies:
                    return True
        return False


    def get_fragmentation_threshold(self, phy):
        """Returns the fragmentation threshold for |phy|.

        @param phy: phy name
        """
        ret = self._run('%s phy %s info' % (self._command_iw, phy))
        frag_regex = r'^\s+Fragmentation threshold:\s+([0-9]+)$'
        match = re.search(frag_regex, ret.stdout, re.MULTILINE)

        if match:
            return int(match.group(1))

        return None


    def get_info(self, phy=None):
        """
        Returns the output of 'iw phy info' for |phy|, or 'iw list' if no phy
        specified.

        @param phy: optional string giving the name of the phy
        @return string stdout of the command run
        """
        if phy and phy not in [iw_phy.name for iw_phy in self.list_phys()]:
            logging.info('iw could not find phy %s', phy)
            return None

        if phy:
            out = self._run('%s phy %s info' % (self._command_iw, phy)).stdout
        else:
            out = self._run('%s list' % self._command_iw).stdout
        if 'Wiphy' in out:
            return out
        return None