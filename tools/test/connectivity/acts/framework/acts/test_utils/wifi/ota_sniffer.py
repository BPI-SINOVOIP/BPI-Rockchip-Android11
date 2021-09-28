#!/usr/bin/env python3
#
#   Copyright 2019 - The Android Open Source Project
#
#   Licensed under the Apache License, Version 2.0 (the 'License');
#   you may not use this file except in compliance with the License.
#   You may obtain a copy of the License at
#
#       http://www.apache.org/licenses/LICENSE-2.0
#
#   Unless required by applicable law or agreed to in writing, software
#   distributed under the License is distributed on an 'AS IS' BASIS,
#   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#   See the License for the specific language governing permissions and
#   limitations under the License.

import csv
import os
from acts import context
from acts import logger
from acts import utils
from acts.controllers.utils_lib import ssh


def create(configs):
    """Factory method for sniffer.
    Args:
        configs: list of dicts with sniffer settings.
        Settings must contain the following : ssh_settings, type, OS, interface.

    Returns:
        objs: list of sniffer class objects.
    """
    objs = []
    for config in configs:
        try:
            if config["type"] == "tshark":
                if config["os"] == "unix":
                    objs.append(TsharkSnifferOnUnix(config))
                elif config["os"] == "linux":
                    objs.append(TsharkSnifferOnLinux(config))
                else:
                    raise RuntimeError("Wrong sniffer config")

            elif config["type"] == "mock":
                objs.append(MockSniffer(config))
        except KeyError:
            raise KeyError("Invalid sniffer configurations")
        return objs


def destroy(objs):
    return


class OtaSnifferBase(object):
    """Base class defining common sniffers functions."""

    _log_file_counter = 0

    @property
    def started(self):
        raise NotImplementedError('started must be specified.')

    def start_capture(self, network, duration=30):
        """Starts the sniffer Capture.

        Args:
            network: dict containing network information such as SSID, etc.
            duration: duration of sniffer capture in seconds.
        """
        raise NotImplementedError('start_capture must be specified.')

    def stop_capture(self, tag=""):
        """Stops the sniffer Capture.

        Args:
            tag: string to tag sniffer capture file name with.
        """
        raise NotImplementedError('stop_capture must be specified.')

    def _get_remote_dump_path(self):
        """Returns name of the sniffer dump file."""
        return "sniffer_dump.csv"

    def _get_full_file_path(self, tag=None):
        """Returns the full file path for the sniffer capture dump file.

        Returns the full file path (on test machine) for the sniffer capture
        dump file.

        Args:
            tag: The tag appended to the sniffer capture dump file .
        """
        tags = [tag, "count", OtaSnifferBase._log_file_counter]
        out_file_name = 'Sniffer_Capture_%s.csv' % ('_'.join(
            [str(x) for x in tags if x != '' and x is not None]))
        OtaSnifferBase._log_file_counter += 1

        file_path = os.path.join(self.log_path, out_file_name)
        return file_path

    @property
    def log_path(self):
        current_context = context.get_current_context()
        full_out_dir = os.path.join(current_context.get_full_output_path(),
                                    'sniffer_captures')

        # Ensure the directory exists.
        os.makedirs(full_out_dir, exist_ok=True)

        return full_out_dir


class MockSniffer(OtaSnifferBase):
    """Class that implements mock sniffer for test development and debug."""
    def __init__(self, config):
        self.log = logger.create_tagged_trace_logger("Mock Sniffer")

    def start_capture(self, network, duration=30):
        """Starts sniffer capture on the specified machine.

        Args:
            network: dict of network credentials.
            duration: duration of the sniff.
        """
        self.log.info("Starting sniffer.")

    def stop_capture(self):
        """Stops the sniffer.

        Returns:
            log_file: name of processed sniffer.
        """

        self.log.info("Stopping sniffer.")
        log_file = self._get_full_file_path()
        with open(log_file, 'w') as file:
            file.write('this is a sniffer dump.')
        return log_file


class TsharkSnifferBase(OtaSnifferBase):
    """Class that implements Tshark based sniffer controller. """

    TYPE_SUBTYPE_DICT = {
        "0": "Association Requests",
        "1": "Association Responses",
        "2": "Reassociation Requests",
        "3": "Resssociation Responses",
        "4": "Probe Requests",
        "5": "Probe Responses",
        "8": "Beacon",
        "9": "ATIM",
        "10": "Disassociations",
        "11": "Authentications",
        "12": "Deauthentications",
        "13": "Actions",
        "24": "Block ACK Requests",
        "25": "Block ACKs",
        "26": "PS-Polls",
        "27": "RTS",
        "28": "CTS",
        "29": "ACK",
        "30": "CF-Ends",
        "31": "CF-Ends/CF-Acks",
        "32": "Data",
        "33": "Data+CF-Ack",
        "34": "Data+CF-Poll",
        "35": "Data+CF-Ack+CF-Poll",
        "36": "Null",
        "37": "CF-Ack",
        "38": "CF-Poll",
        "39": "CF-Ack+CF-Poll",
        "40": "QoS Data",
        "41": "QoS Data+CF-Ack",
        "42": "QoS Data+CF-Poll",
        "43": "QoS Data+CF-Ack+CF-Poll",
        "44": "QoS Null",
        "46": "QoS CF-Poll (Null)",
        "47": "QoS CF-Ack+CF-Poll (Null)"
    }

    TSHARK_COLUMNS = [
        "frame_number", "frame_time_relative", "mactime", "frame_len", "rssi",
        "channel", "ta", "ra", "bssid", "type", "subtype", "duration", "seq",
        "retry", "pwrmgmt", "moredata", "ds", "phy", "radio_datarate",
        "vht_datarate", "radiotap_mcs_index", "vht_mcs", "wlan_data_rate",
        "11n_mcs_index", "11ac_mcs", "11n_bw", "11ac_bw", "vht_nss", "mcs_gi",
        "vht_gi", "vht_coding", "ba_bm", "fc_status", "bf_report"
    ]

    TSHARK_OUTPUT_COLUMNS = [
        "frame_number", "frame_time_relative", "mactime", "ta", "ra", "bssid",
        "rssi", "channel", "frame_len", "Info", "radio_datarate",
        "radiotap_mcs_index", "pwrmgmt", "phy", "vht_nss", "vht_mcs",
        "vht_datarate", "11ac_mcs", "11ac_bw", "vht_gi", "vht_coding",
        "wlan_data_rate", "11n_mcs_index", "11n_bw", "mcs_gi", "type",
        "subtype", "duration", "seq", "retry", "moredata", "ds", "ba_bm",
        "fc_status", "bf_report"
    ]

    TSHARK_FIELDS_LIST = [
        'frame.number', 'frame.time_relative', 'radiotap.mactime', 'frame.len',
        'radiotap.dbm_antsignal', 'wlan_radio.channel', 'wlan.ta', 'wlan.ra',
        'wlan.bssid', 'wlan.fc.type', 'wlan.fc.type_subtype', 'wlan.duration',
        'wlan.seq', 'wlan.fc.retry', 'wlan.fc.pwrmgt', 'wlan.fc.moredata',
        'wlan.fc.ds', 'wlan_radio.phy', 'radiotap.datarate',
        'radiotap.vht.datarate.0', 'radiotap.mcs.index', 'radiotap.vht.mcs.0',
        'wlan_radio.data_rate', 'wlan_radio.11n.mcs_index',
        'wlan_radio.11ac.mcs', 'wlan_radio.11n.bandwidth',
        'wlan_radio.11ac.bandwidth', 'radiotap.vht.nss.0', 'radiotap.mcs.gi',
        'radiotap.vht.gi', 'radiotap.vht.coding.0', 'wlan.ba.bm',
        'wlan.fcs.status', 'wlan.vht.compressed_beamforming_report.snr'
    ]

    def __init__(self, config):
        self.sniffer_proc_pid = None
        self.log = logger.create_tagged_trace_logger("Tshark Sniffer")
        self.ssh_config = config["ssh_config"]
        self.sniffer_os = config["os"]
        self.sniffer_interface = config["interface"]

        #Logging into sniffer
        self.log.info("Logging into sniffer.")
        self._sniffer_server = ssh.connection.SshConnection(
            ssh.settings.from_config(self.ssh_config))

        self.tshark_fields = self._generate_tshark_fields(
            self.TSHARK_FIELDS_LIST)

        self.tshark_path = self._sniffer_server.run("which tshark").stdout

    @property
    def _started(self):
        return self.sniffer_proc_pid is not None

    def _scan_for_networks(self):
        """Scans for wireless networks on the sniffer."""
        raise NotImplementedError

    def _init_network_association(self, ssid, password):
        """Associates the sniffer to the network to sniff.

        Args:
            ssid: SSID of the wireless network to connect to.
            password: password of the wireless network to connect to.
        """
        raise NotImplementedError

    def _get_tshark_command(self, duration):
        """Frames the appropriate tshark command.

        Args:
            duration: duration to sniff for.

        Returns:
            tshark_command : appropriate tshark command.
        """

        tshark_command = "{} -l -i {} -I -t u -a duration:{}".format(
            self.tshark_path, self.sniffer_interface, int(duration))

        return tshark_command

    def _generate_tshark_fields(self, fields):
        """Generates tshark fields to be appended to the tshark command.

        Args:
            fields: list of tshark fields to be appended to the tshark command.

        Returns:
            tshark_fields: string of tshark fields to be appended to the tshark command.
        """

        tshark_fields = '-T fields -y IEEE802_11_RADIO -E separator="^"'
        for field in fields:
            tshark_fields = tshark_fields + " -e {}".format(field)
        return tshark_fields

    def _connect_to_network(self, network):
        """ Connects to a wireless network using networksetup utility.

        Args:
            network: dictionary of network credentials; SSID and password.
        """

        self.log.info("Connecting to network {}".format(network["SSID"]))

        # Scan to see if the requested SSID is available
        scan_result = self._scan_for_networks()

        if network["SSID"] not in scan_result:
            self.log.warning("{} not found in scan".format(network["SSID"]))

        if "password" not in network.keys():
            network["password"] = ""

        self._init_network_association(network["SSID"], network["password"])

    def _run_tshark(self, sniffer_command):
        """Starts the sniffer.

        Args:
            sniffer_command: sniffer command to execute.
        """

        self.log.info("Starting sniffer.")
        sniffer_job = self._sniffer_server.run_async(sniffer_command)
        self.sniffer_proc_pid = sniffer_job.stdout

    def _stop_tshark(self):
        """ Stops the sniffer."""

        self.log.info("Stopping sniffer")

        # while loop to kill the sniffer process
        kill_line_logged = False

        while True:
            try:
                # Returns 1 if process was killed
                self._sniffer_server.run(
                    "ps aux| grep {} | grep -v grep".format(
                        self.sniffer_proc_pid))
            except:
                break
            try:
                # Returns error if process was killed already
                if not kill_line_logged:
                    self.log.info('Killing tshark process.')
                    kill_line_logged = True
                self._sniffer_server.run("kill -15 {}".format(
                    str(self.sniffer_proc_pid)))
            except:
                # Except is hit when tshark is already dead but we will break
                # out of the loop when confirming process is dead using ps aux
                pass

    def _process_tshark_dump(self, temp_dump_file, tag):
        """ Process tshark dump for better readability.

        Processes tshark dump for better readability and saves it to a file.
        Adds an info column at the end of each row. Format of the info columns:
        subtype of the frame, sequence no and retry status.

        Args:
            temp_dump_file : string of sniffer capture output.
            tag : tag to be appended to the dump file.
        Returns:
            log_file : name of the file where the processed dump is stored.
        """
        log_file = self._get_full_file_path(tag)
        with open(temp_dump_file, "r") as input_csv, open(log_file,
                                                          "w") as output_csv:
            reader = csv.DictReader(input_csv,
                                    fieldnames=self.TSHARK_COLUMNS,
                                    delimiter="^")
            writer = csv.DictWriter(output_csv,
                                    fieldnames=self.TSHARK_OUTPUT_COLUMNS,
                                    delimiter="\t")
            writer.writeheader()
            for row in reader:
                if row["subtype"] in self.TYPE_SUBTYPE_DICT.keys():
                    row["Info"] = "{sub} S={seq} retry={retry_status}".format(
                        sub=self.TYPE_SUBTYPE_DICT[row["subtype"]],
                        seq=row["seq"],
                        retry_status=row["retry"])
                else:
                    row["Info"] = "{} S={} retry={}\n".format(
                        row["subtype"], row["seq"], row["retry"])
                writer.writerow(row)
        return log_file

    def start_capture(self, network, duration=30):
        """Starts sniffer capture on the specified machine.

        Args:
            network: dict describing network to sniff on.
            duration: duration of sniff.
        """

        # Checking for existing sniffer processes
        if self._started:
            self.log.info("Sniffer already running")
            return

        # Connecting to network
        self._connect_to_network(network)

        tshark_command = self._get_tshark_command(duration)

        sniffer_command = "{tshark} {fields} > {log_file}".format(
            tshark=tshark_command,
            fields=self.tshark_fields,
            log_file=self._get_remote_dump_path())

        # Starting sniffer capture by executing tshark command
        self._run_tshark(sniffer_command)

    def stop_capture(self, tag=""):
        """Stops the sniffer.

        Args:
            tag: tag to be appended to the sniffer output file.
        Returns:
            log_file: path to sniffer dump.
        """
        # Checking if there is an ongoing sniffer capture
        if not self._started:
            self.log.error("No sniffer process running")
            return
        # Killing sniffer process
        self._stop_tshark()

        # Processing writing capture output to file
        temp_dump_path = os.path.join(self.log_path, "sniffer_temp_dump.csv")
        self._sniffer_server.pull_file(temp_dump_path,
                                       self._get_remote_dump_path())
        log_file = self._process_tshark_dump(temp_dump_path, tag)

        self.sniffer_proc_pid = None
        utils.exe_cmd("rm -f {}".format(temp_dump_path))
        return log_file


class TsharkSnifferOnUnix(TsharkSnifferBase):
    """Class that implements Tshark based sniffer controller on Unix systems."""
    def _scan_for_networks(self):
        """Scans the wireless networks on the sniffer.

        Returns:
            scan_results : output of the scan command.
        """

        scan_command = "/usr/local/bin/airport -s"
        scan_result = self._sniffer_server.run(scan_command).stdout

        return scan_result

    def _init_network_association(self, ssid, password):
        """Associates the sniffer to the network to sniff.

        Associates the sniffer to wireless network to sniff using networksetup utility.

        Args:
            ssid: SSID of the wireless network to connect to.
            password: password of the wireless network to connect to.
        """

        connect_command = "networksetup -setairportnetwork en0 {} {}".format(
            ssid, password)
        self._sniffer_server.run(connect_command)


class TsharkSnifferOnLinux(TsharkSnifferBase):
    """Class that implements Tshark based sniffer controller on Linux systems."""
    def _scan_for_networks(self):
        """Scans the wireless networks on the sniffer.

        Returns:
            scan_results : output of the scan command.
        """

        scan_command = "nmcli device wifi rescan; nmcli device wifi list"
        scan_result = self._sniffer_server.run(scan_command).stdout

        return scan_result

    def _init_network_association(self, ssid, password):
        """Associates the sniffer to the network to sniff.

        Associates the sniffer to wireless network to sniff using nmcli utility.

        Args:
            ssid: SSID of the wireless network to connect to.
            password: password of the wireless network to connect to.
        """
        if password != "":
            connect_command = "sudo nmcli device wifi connect {} password {}".format(
                ssid, password)
        else:
            connect_command = "sudo nmcli device wifi connect {}".format(ssid)
        self._sniffer_server.run(connect_command)
