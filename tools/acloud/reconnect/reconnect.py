# Copyright 2018 - The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
r"""Reconnect entry point.

Reconnect will:
 - re-establish ssh tunnels for adb/vnc port forwarding for a remote instance
 - adb connect to forwarded ssh port for remote instance
 - restart vnc for remote/local instances
"""

import re

from acloud import errors
from acloud.internal import constants
from acloud.internal.lib import auth
from acloud.internal.lib import android_compute_client
from acloud.internal.lib import utils
from acloud.internal.lib.adb_tools import AdbTools
from acloud.list import list as list_instance
from acloud.public import config
from acloud.public import report


_RE_DISPLAY = re.compile(r"([\d]+)x([\d]+)\s.*")
_VNC_STARTED_PATTERN = "ssvnc vnc://127.0.0.1:%(vnc_port)d"


def StartVnc(vnc_port, display):
    """Start vnc connect to AVD.

    Confirm whether there is already a connection before VNC connection.
    If there is a connection, it will not be connected. If not, connect it.
    Before reconnecting, clear old disconnect ssvnc viewer.

    Args:
        vnc_port: Integer of vnc port number.
        display: String, vnc connection resolution. e.g., 1080x720 (240)
    """
    vnc_started_pattern = _VNC_STARTED_PATTERN % {"vnc_port": vnc_port}
    if not utils.IsCommandRunning(vnc_started_pattern):
        #clean old disconnect ssvnc viewer.
        utils.CleanupSSVncviewer(vnc_port)

        match = _RE_DISPLAY.match(display)
        if match:
            utils.LaunchVncClient(vnc_port, match.group(1), match.group(2))
        else:
            utils.LaunchVncClient(vnc_port)


def AddPublicSshRsaToInstance(cfg, user, instance_name):
    """Add the public rsa key to the instance's metadata.

    When the public key doesn't exist in the metadata, it will add it.

    Args:
        cfg: An AcloudConfig instance.
        user: String, the ssh username to access instance.
        instance_name: String, instance name.
    """
    credentials = auth.CreateCredentials(cfg)
    compute_client = android_compute_client.AndroidComputeClient(
        cfg, credentials)
    compute_client.AddSshRsaInstanceMetadata(
        user,
        cfg.ssh_public_key_path,
        instance_name)


@utils.TimeExecute(function_description="Reconnect instances")
def ReconnectInstance(ssh_private_key_path,
                      instance,
                      reconnect_report,
                      extra_args_ssh_tunnel=None,
                      connect_vnc=True):
    """Reconnect to the specified instance.

    It will:
     - re-establish ssh tunnels for adb/vnc port forwarding
     - re-establish adb connection
     - restart vnc client
     - update device information in reconnect_report

    Args:
        ssh_private_key_path: Path to the private key file.
                              e.g. ~/.ssh/acloud_rsa
        instance: list.Instance() object.
        reconnect_report: Report object.
        extra_args_ssh_tunnel: String, extra args for ssh tunnel connection.
        connect_vnc: Boolean, True will launch vnc.

    Raises:
        errors.UnknownAvdType: Unable to reconnect to instance of unknown avd
                               type.
    """
    if instance.avd_type not in utils.AVD_PORT_DICT:
        raise errors.UnknownAvdType("Unable to reconnect to instance (%s) of "
                                    "unknown avd type: %s" %
                                    (instance.name, instance.avd_type))

    adb_cmd = AdbTools(instance.adb_port)
    vnc_port = instance.vnc_port
    adb_port = instance.adb_port
    # ssh tunnel is up but device is disconnected on adb
    if instance.ssh_tunnel_is_connected and not adb_cmd.IsAdbConnectionAlive():
        adb_cmd.DisconnectAdb()
        adb_cmd.ConnectAdb()
    # ssh tunnel is down and it's a remote instance
    elif not instance.ssh_tunnel_is_connected and not instance.islocal:
        adb_cmd.DisconnectAdb()
        forwarded_ports = utils.AutoConnect(
            ip_addr=instance.ip,
            rsa_key_file=ssh_private_key_path,
            target_vnc_port=utils.AVD_PORT_DICT[instance.avd_type].vnc_port,
            target_adb_port=utils.AVD_PORT_DICT[instance.avd_type].adb_port,
            ssh_user=constants.GCE_USER,
            extra_args_ssh_tunnel=extra_args_ssh_tunnel)
        vnc_port = forwarded_ports.vnc_port
        adb_port = forwarded_ports.adb_port

    if vnc_port and connect_vnc:
        StartVnc(vnc_port, instance.display)

    device_dict = {
        constants.IP: instance.ip,
        constants.INSTANCE_NAME: instance.name,
        constants.VNC_PORT: vnc_port,
        constants.ADB_PORT: adb_port
    }

    if vnc_port and adb_port:
        reconnect_report.AddData(key="devices", value=device_dict)
    else:
        # We use 'ps aux' to grep adb/vnc fowarding port from ssh tunnel
        # command. Therefore we report failure here if no vnc_port and
        # adb_port found.
        reconnect_report.AddData(key="device_failing_reconnect", value=device_dict)
        reconnect_report.AddError(instance.name)


def Run(args):
    """Run reconnect.

    Args:
        args: Namespace object from argparse.parse_args.
    """
    cfg = config.GetAcloudConfig(args)
    instances_to_reconnect = []
    if args.instance_names is not None:
        # user input instance name to get instance object.
        instances_to_reconnect = list_instance.GetInstancesFromInstanceNames(
            cfg, args.instance_names)
    if not instances_to_reconnect:
        instances_to_reconnect = list_instance.ChooseInstances(cfg, args.all)

    reconnect_report = report.Report(command="reconnect")
    for instance in instances_to_reconnect:
        if instance.avd_type not in utils.AVD_PORT_DICT:
            utils.PrintColorString("Skipping reconnect of instance %s due to "
                                   "unknown avd type (%s)." %
                                   (instance.name, instance.avd_type),
                                   utils.TextColors.WARNING)
            continue
        if not instance.islocal:
            AddPublicSshRsaToInstance(cfg, constants.GCE_USER, instance.name)
        ReconnectInstance(cfg.ssh_private_key_path,
                          instance,
                          reconnect_report,
                          cfg.extra_args_ssh_tunnel,
                          connect_vnc=(args.autoconnect is True))

    utils.PrintDeviceSummary(reconnect_report)
