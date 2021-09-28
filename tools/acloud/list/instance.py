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
r"""Instance class.

Define the instance class used to hold details about an AVD instance.

The instance class will hold details about AVD instances (remote/local) used to
enable users to understand what instances they've created. This will be leveraged
for the list, delete, and reconnect commands.

The details include:
- instance name (for remote instances)
- creation date/instance duration
- instance image details (branch/target/build id)
- and more!
"""

import collections
import datetime
import logging
import os
import re
import subprocess
import tempfile

# pylint: disable=import-error
import dateutil.parser
import dateutil.tz

from acloud.internal import constants
from acloud.internal.lib import cvd_runtime_config
from acloud.internal.lib import utils
from acloud.internal.lib.adb_tools import AdbTools


logger = logging.getLogger(__name__)

_ACLOUD_CVD_TEMP = os.path.join(tempfile.gettempdir(), "acloud_cvd_temp")
_CVD_RUNTIME_FOLDER_NAME = "cuttlefish_runtime"
_CVD_STATUS_BIN = "cvd_status"
_MSG_UNABLE_TO_CALCULATE = "Unable to calculate"
_RE_GROUP_ADB = "local_adb_port"
_RE_GROUP_VNC = "local_vnc_port"
_RE_SSH_TUNNEL_PATTERN = (r"((.*\s*-L\s)(?P<%s>\d+):127.0.0.1:%s)"
                          r"((.*\s*-L\s)(?P<%s>\d+):127.0.0.1:%s)"
                          r"(.+%s)")
_RE_TIMEZONE = re.compile(r"^(?P<time>[0-9\-\.:T]*)(?P<timezone>[+-]\d+:\d+)$")

_COMMAND_PS_LAUNCH_CVD = ["ps", "-wweo", "lstart,cmd"]
_RE_RUN_CVD = re.compile(r"(?P<date_str>^[^/]+)(.*run_cvd)")
_DISPLAY_STRING = "%(x_res)sx%(y_res)s (%(dpi)s)"
_RE_ZONE = re.compile(r".+/zones/(?P<zone>.+)$")
_LOCAL_ZONE = "local"
_FULL_NAME_STRING = ("device serial: %(device_serial)s (%(instance_name)s) "
                     "elapsed time: %(elapsed_time)s")
_INDENT = " " * 3
LocalPorts = collections.namedtuple("LocalPorts", [constants.VNC_PORT,
                                                   constants.ADB_PORT])


def GetDefaultCuttlefishConfig():
    """Get the path of default cuttlefish instance config.

    Return:
        String, path of cf runtime config.
    """
    return os.path.join(os.path.expanduser("~"), _CVD_RUNTIME_FOLDER_NAME,
                        constants.CUTTLEFISH_CONFIG_FILE)


def GetLocalInstanceName(local_instance_id):
    """Get local cuttlefish instance name by instance id.

    Args:
        local_instance_id: Integer of instance id.

    Return:
        String, the instance name.
    """
    return "%s-%d" % (constants.LOCAL_INS_NAME, local_instance_id)


def GetLocalInstanceConfig(local_instance_id):
    """Get the path of instance config.

    Args:
        local_instance_id: Integer of instance id.

    Return:
        String, path of cf runtime config.
    """
    cfg_path = os.path.join(GetLocalInstanceRuntimeDir(local_instance_id),
                            constants.CUTTLEFISH_CONFIG_FILE)
    if os.path.isfile(cfg_path):
        return cfg_path
    return None


def GetAllLocalInstanceConfigs():
    """Get the list of instance config.

    Return:
        List of instance config path.
    """
    cfg_list = []
    # Check if any instance config is under home folder.
    cfg_path = GetDefaultCuttlefishConfig()
    if os.path.isfile(cfg_path):
        cfg_list.append(cfg_path)

    # Check if any instance config is under acloud cvd temp folder.
    if os.path.exists(_ACLOUD_CVD_TEMP):
        for ins_name in os.listdir(_ACLOUD_CVD_TEMP):
            cfg_path = os.path.join(_ACLOUD_CVD_TEMP,
                                    ins_name,
                                    _CVD_RUNTIME_FOLDER_NAME,
                                    constants.CUTTLEFISH_CONFIG_FILE)
            if os.path.isfile(cfg_path):
                cfg_list.append(cfg_path)
    return cfg_list


def GetLocalInstanceHomeDir(local_instance_id):
    """Get local instance home dir according to instance id.

    Args:
        local_instance_id: Integer of instance id.

    Return:
        String, path of instance home dir.
    """
    return os.path.join(_ACLOUD_CVD_TEMP,
                        GetLocalInstanceName(local_instance_id))


def GetLocalInstanceRuntimeDir(local_instance_id):
    """Get instance runtime dir

    Args:
        local_instance_id: Integer of instance id.

    Return:
        String, path of instance runtime dir.
    """
    return os.path.join(GetLocalInstanceHomeDir(local_instance_id),
                        _CVD_RUNTIME_FOLDER_NAME)


def _GetCurrentLocalTime():
    """Return a datetime object for current time in local time zone."""
    return datetime.datetime.now(dateutil.tz.tzlocal())


def _GetElapsedTime(start_time):
    """Calculate the elapsed time from start_time till now.

    Args:
        start_time: String of instance created time.

    Returns:
        datetime.timedelta of elapsed time, _MSG_UNABLE_TO_CALCULATE for
        datetime can't parse cases.
    """
    match = _RE_TIMEZONE.match(start_time)
    try:
        # Check start_time has timezone or not. If timezone can't be found,
        # use local timezone to get elapsed time.
        if match:
            return _GetCurrentLocalTime() - dateutil.parser.parse(start_time)

        return _GetCurrentLocalTime() - dateutil.parser.parse(
            start_time).replace(tzinfo=dateutil.tz.tzlocal())
    except ValueError:
        logger.debug(("Can't parse datetime string(%s)."), start_time)
        return _MSG_UNABLE_TO_CALCULATE


class Instance(object):
    """Class to store data of instance."""

    # pylint: disable=too-many-locals
    def __init__(self, name, fullname, display, ip, status=None, adb_port=None,
                 vnc_port=None, ssh_tunnel_is_connected=None, createtime=None,
                 elapsed_time=None, avd_type=None, avd_flavor=None,
                 is_local=False, device_information=None, zone=None):
        self._name = name
        self._fullname = fullname
        self._status = status
        self._display = display  # Resolution and dpi
        self._ip = ip
        self._adb_port = adb_port  # adb port which is forwarding to remote
        self._vnc_port = vnc_port  # vnc port which is forwarding to remote
        # True if ssh tunnel is still connected
        self._ssh_tunnel_is_connected = ssh_tunnel_is_connected
        self._createtime = createtime
        self._elapsed_time = elapsed_time
        self._avd_type = avd_type
        self._avd_flavor = avd_flavor
        self._is_local = is_local  # True if this is a local instance
        self._device_information = device_information
        self._zone = zone

    def __repr__(self):
        """Return full name property for print."""
        return self._fullname

    def Summary(self):
        """Let's make it easy to see what this class is holding."""
        representation = []
        representation.append(" name: %s" % self._name)
        representation.append("%s IP: %s" % (_INDENT, self._ip))
        representation.append("%s create time: %s" % (_INDENT, self._createtime))
        representation.append("%s elapse time: %s" % (_INDENT, self._elapsed_time))
        representation.append("%s status: %s" % (_INDENT, self._status))
        representation.append("%s avd type: %s" % (_INDENT, self._avd_type))
        representation.append("%s display: %s" % (_INDENT, self._display))
        representation.append("%s vnc: 127.0.0.1:%s" % (_INDENT, self._vnc_port))
        representation.append("%s zone: %s" % (_INDENT, self._zone))

        if self._adb_port and self._device_information:
            representation.append("%s adb serial: 127.0.0.1:%s" %
                                  (_INDENT, self._adb_port))
            representation.append("%s product: %s" % (
                _INDENT, self._device_information["product"]))
            representation.append("%s model: %s" % (
                _INDENT, self._device_information["model"]))
            representation.append("%s device: %s" % (
                _INDENT, self._device_information["device"]))
            representation.append("%s transport_id: %s" % (
                _INDENT, self._device_information["transport_id"]))
        else:
            representation.append("%s adb serial: disconnected" % _INDENT)

        return "\n".join(representation)

    @property
    def name(self):
        """Return the instance name."""
        return self._name

    @property
    def fullname(self):
        """Return the instance full name."""
        return self._fullname

    @property
    def ip(self):
        """Return the ip."""
        return self._ip

    @property
    def status(self):
        """Return status."""
        return self._status

    @property
    def display(self):
        """Return display."""
        return self._display

    @property
    def ssh_tunnel_is_connected(self):
        """Return the connect status."""
        return self._ssh_tunnel_is_connected

    @property
    def createtime(self):
        """Return create time."""
        return self._createtime

    @property
    def avd_type(self):
        """Return avd_type."""
        return self._avd_type

    @property
    def avd_flavor(self):
        """Return avd_flavor."""
        return self._avd_flavor

    @property
    def islocal(self):
        """Return if it is a local instance."""
        return self._is_local

    @property
    def adb_port(self):
        """Return adb_port."""
        return self._adb_port

    @property
    def vnc_port(self):
        """Return vnc_port."""
        return self._vnc_port

    @property
    def zone(self):
        """Return zone."""
        return self._zone


class LocalInstance(Instance):
    """Class to store data of local cuttlefish instance."""
    def __init__(self, cf_config_path):
        """Initialize a localInstance object.

        Args:
            cf_config_path: String, path to the cf runtime config.
        """
        self._cf_runtime_cfg = cvd_runtime_config.CvdRuntimeConfig(cf_config_path)
        self._instance_dir = self._cf_runtime_cfg.instance_dir
        self._virtual_disk_paths = self._cf_runtime_cfg.virtual_disk_paths
        self._local_instance_id = int(self._cf_runtime_cfg.instance_id)

        display = _DISPLAY_STRING % {"x_res": self._cf_runtime_cfg.x_res,
                                     "y_res": self._cf_runtime_cfg.y_res,
                                     "dpi": self._cf_runtime_cfg.dpi}
        # TODO(143063678), there's no createtime info in
        # cuttlefish_config.json so far.
        name = GetLocalInstanceName(self._local_instance_id)
        fullname = (_FULL_NAME_STRING %
                    {"device_serial": "127.0.0.1:%s" % self._cf_runtime_cfg.adb_port,
                     "instance_name": name,
                     "elapsed_time": None})
        adb_device = AdbTools(self._cf_runtime_cfg.adb_port)
        device_information = None
        if adb_device.IsAdbConnected():
            device_information = adb_device.device_information

        super(LocalInstance, self).__init__(
            name=name, fullname=fullname, display=display, ip="127.0.0.1",
            status=constants.INS_STATUS_RUNNING,
            adb_port=self._cf_runtime_cfg.adb_port,
            vnc_port=self._cf_runtime_cfg.vnc_port,
            createtime=None, elapsed_time=None, avd_type=constants.TYPE_CF,
            is_local=True, device_information=device_information,
            zone=_LOCAL_ZONE)

    def Summary(self):
        """Return the string that this class is holding."""
        instance_home = "%s instance home: %s" % (_INDENT, self._instance_dir)
        return "%s\n%s" % (super(LocalInstance, self).Summary(), instance_home)

    def CvdStatus(self):
        """check if local instance is active.

        Execute cvd_status cmd to check if it exit without error.

        Returns
            True if instance is active.
        """
        cvd_env = os.environ.copy()
        cvd_env[constants.ENV_CUTTLEFISH_CONFIG_FILE] = self._cf_runtime_cfg.config_path
        cvd_env[constants.ENV_CVD_HOME] = GetLocalInstanceHomeDir(self._local_instance_id)
        cvd_env[constants.ENV_CUTTLEFISH_INSTANCE] = str(self._local_instance_id)
        try:
            cvd_status_cmd = os.path.join(self._cf_runtime_cfg.cvd_tools_path,
                                          _CVD_STATUS_BIN)
            logger.debug("Running cmd[%s] to check cvd status.", cvd_status_cmd)
            process = subprocess.Popen(cvd_status_cmd,
                                       stdin=None,
                                       stdout=subprocess.PIPE,
                                       stderr=subprocess.STDOUT,
                                       env=cvd_env)
            stdout, _ = process.communicate()
            if process.returncode != 0:
                if stdout:
                    logger.debug("Local instance[%s] is not active: %s",
                                 self.name, stdout.strip())
                return False
            return True
        except subprocess.CalledProcessError as cpe:
            logger.error("Failed to run cvd_status: %s", cpe.output)
            return False

    def Delete(self):
        """Execute stop_cvd to stop local cuttlefish instance.

        - We should get the same host tool used to launch cvd to delete instance
        , So get stop_cvd bin from the cvd runtime config.
        - Add CUTTLEFISH_CONFIG_FILE env variable to tell stop_cvd which cvd
        need to be deleted.
        - Stop adb since local instance use the fixed adb port and could be
         reused again soon.
        """
        stop_cvd_cmd = os.path.join(self.cf_runtime_cfg.cvd_tools_path,
                                    constants.CMD_STOP_CVD)
        logger.debug("Running cmd[%s] to delete local cvd", stop_cvd_cmd)
        with open(os.devnull, "w") as dev_null:
            cvd_env = os.environ.copy()
            if self.instance_dir:
                cvd_env[constants.ENV_CUTTLEFISH_CONFIG_FILE] = self._cf_runtime_cfg.config_path
                cvd_env[constants.ENV_CVD_HOME] = GetLocalInstanceHomeDir(
                    self._local_instance_id)
                cvd_env[constants.ENV_CUTTLEFISH_INSTANCE] = str(self._local_instance_id)
            else:
                logger.error("instance_dir is null!! instance[%d] might not be"
                             " deleted", self._local_instance_id)
            subprocess.check_call(
                utils.AddUserGroupsToCmd(stop_cvd_cmd,
                                         constants.LIST_CF_USER_GROUPS),
                stderr=dev_null, stdout=dev_null, shell=True, env=cvd_env)

        adb_cmd = AdbTools(self.adb_port)
        # When relaunch a local instance, we need to pass in retry=True to make
        # sure adb device is completely gone since it will use the same adb port
        adb_cmd.DisconnectAdb(retry=True)

    @property
    def instance_dir(self):
        """Return _instance_dir."""
        return self._instance_dir

    @property
    def instance_id(self):
        """Return _local_instance_id."""
        return self._local_instance_id

    @property
    def virtual_disk_paths(self):
        """Return virtual_disk_paths"""
        return self._virtual_disk_paths

    @property
    def cf_runtime_cfg(self):
        """Return _cf_runtime_cfg"""
        return self._cf_runtime_cfg


class LocalGoldfishInstance(Instance):
    """Class to store data of local goldfish instance."""

    _INSTANCE_NAME_PATTERN = re.compile(
        r"^local-goldfish-instance-(?P<id>\d+)$")
    _CREATION_TIMESTAMP_FILE_NAME = "creation_timestamp.txt"
    _INSTANCE_NAME_FORMAT = "local-goldfish-instance-%(id)s"
    _EMULATOR_DEFAULT_CONSOLE_PORT = 5554
    _GF_ADB_DEVICE_SERIAL = "emulator-%(console_port)s"

    def __init__(self, local_instance_id, avd_flavor=None, create_time=None,
                 x_res=None, y_res=None, dpi=None):
        """Initialize a LocalGoldfishInstance object.

        Args:
            local_instance_id: Integer of instance id.
            avd_flavor: String, the flavor of the virtual device.
            create_time: String, the creation date and time.
            x_res: Integer of x dimension.
            y_res: Integer of y dimension.
            dpi: Integer of dpi.
        """
        self._id = local_instance_id
        # By convention, adb port is console port + 1.
        adb_port = self.console_port + 1

        name = self._INSTANCE_NAME_FORMAT % {"id": local_instance_id}

        elapsed_time = _GetElapsedTime(create_time) if create_time else None

        fullname = _FULL_NAME_STRING % {"device_serial": self.device_serial,
                                        "instance_name": name,
                                        "elapsed_time": elapsed_time}

        if x_res and y_res and dpi:
            display = _DISPLAY_STRING % {"x_res": x_res, "y_res": y_res,
                                         "dpi": dpi}
        else:
            display = "unknown"

        adb = AdbTools(adb_port)
        device_information = (adb.device_information if
                              adb.device_information else None)

        super(LocalGoldfishInstance, self).__init__(
            name=name, fullname=fullname, display=display, ip="127.0.0.1",
            status=None, adb_port=adb_port, avd_type=constants.TYPE_GF,
            createtime=create_time, elapsed_time=elapsed_time,
            avd_flavor=avd_flavor, is_local=True,
            device_information=device_information)

    @staticmethod
    def _GetInstanceDirRoot():
        """Return the root directory of all instance directories."""
        return os.path.join(tempfile.gettempdir(), "acloud_gf_temp")

    @property
    def console_port(self):
        """Return the console port as an integer"""
        # Emulator requires the console port to be an even number.
        return self._EMULATOR_DEFAULT_CONSOLE_PORT + (self._id - 1) * 2

    @property
    def device_serial(self):
        """Return the serial number that contains the console port."""
        return self._GF_ADB_DEVICE_SERIAL % {"console_port": self.console_port}

    @property
    def instance_dir(self):
        """Return the path to instance directory."""
        return os.path.join(self._GetInstanceDirRoot(),
                            self._INSTANCE_NAME_FORMAT % {"id": self._id})

    @property
    def creation_timestamp_path(self):
        """Return the file path containing the creation timestamp."""
        return os.path.join(self.instance_dir,
                            self._CREATION_TIMESTAMP_FILE_NAME)

    def WriteCreationTimestamp(self):
        """Write creation timestamp to file."""
        with open(self.creation_timestamp_path, "w") as timestamp_file:
            timestamp_file.write(str(_GetCurrentLocalTime()))

    def DeleteCreationTimestamp(self, ignore_errors):
        """Delete the creation timestamp file.

        Args:
            ignore_errors: Boolean, whether to ignore the errors.

        Raises:
            OSError if fails to delete the file.
        """
        try:
            os.remove(self.creation_timestamp_path)
        except OSError as e:
            if not ignore_errors:
                raise
            logger.warning("Can't delete creation timestamp: %s", e)

    @classmethod
    def GetExistingInstances(cls):
        """Get a list of instances that have creation timestamp files."""
        instance_root = cls._GetInstanceDirRoot()
        if not os.path.isdir(instance_root):
            return []

        instances = []
        for name in os.listdir(instance_root):
            match = cls._INSTANCE_NAME_PATTERN.match(name)
            timestamp_path = os.path.join(instance_root, name,
                                          cls._CREATION_TIMESTAMP_FILE_NAME)
            if match and os.path.isfile(timestamp_path):
                instance_id = int(match.group("id"))
                with open(timestamp_path, "r") as timestamp_file:
                    timestamp = timestamp_file.read().strip()
                instances.append(LocalGoldfishInstance(instance_id,
                                                       create_time=timestamp))
        return instances


class RemoteInstance(Instance):
    """Class to store data of remote instance."""

    # pylint: disable=too-many-locals
    def __init__(self, gce_instance):
        """Process the args into class vars.

        RemoteInstace initialized by gce dict object. We parse the required data
        from gce_instance to local variables.
        Reference:
        https://cloud.google.com/compute/docs/reference/rest/v1/instances/get

        We also gather more details on client side including the forwarding adb
        port and vnc port which will be used to determine the status of ssh
        tunnel connection.

        The status of gce instance will be displayed in _fullname property:
        - Connected: If gce instance and ssh tunnel and adb connection are all
         active.
        - No connected: If ssh tunnel or adb connection is not found.
        - Terminated: If we can't retrieve the public ip from gce instance.

        Args:
            gce_instance: dict object queried from gce.
        """
        name = gce_instance.get(constants.INS_KEY_NAME)

        create_time = gce_instance.get(constants.INS_KEY_CREATETIME)
        elapsed_time = _GetElapsedTime(create_time)
        status = gce_instance.get(constants.INS_KEY_STATUS)
        zone = self._GetZoneName(gce_instance.get(constants.INS_KEY_ZONE))

        ip = None
        for network_interface in gce_instance.get("networkInterfaces"):
            for access_config in network_interface.get("accessConfigs"):
                ip = access_config.get("natIP")

        # Get metadata
        display = None
        avd_type = None
        avd_flavor = None
        for metadata in gce_instance.get("metadata", {}).get("items", []):
            key = metadata["key"]
            value = metadata["value"]
            if key == constants.INS_KEY_DISPLAY:
                display = value
            elif key == constants.INS_KEY_AVD_TYPE:
                avd_type = value
            elif key == constants.INS_KEY_AVD_FLAVOR:
                avd_flavor = value

        # Find ssl tunnel info.
        adb_port = None
        vnc_port = None
        device_information = None
        if ip:
            forwarded_ports = self.GetAdbVncPortFromSSHTunnel(ip, avd_type)
            adb_port = forwarded_ports.adb_port
            vnc_port = forwarded_ports.vnc_port
            ssh_tunnel_is_connected = adb_port is not None

            adb_device = AdbTools(adb_port)
            if adb_device.IsAdbConnected():
                device_information = adb_device.device_information
                fullname = (_FULL_NAME_STRING %
                            {"device_serial": "127.0.0.1:%d" % adb_port,
                             "instance_name": name,
                             "elapsed_time": elapsed_time})
            else:
                fullname = (_FULL_NAME_STRING %
                            {"device_serial": "not connected",
                             "instance_name": name,
                             "elapsed_time": elapsed_time})
        # If instance is terminated, its ip is None.
        else:
            ssh_tunnel_is_connected = False
            fullname = (_FULL_NAME_STRING %
                        {"device_serial": "terminated",
                         "instance_name": name,
                         "elapsed_time": elapsed_time})

        super(RemoteInstance, self).__init__(
            name=name, fullname=fullname, display=display, ip=ip, status=status,
            adb_port=adb_port, vnc_port=vnc_port,
            ssh_tunnel_is_connected=ssh_tunnel_is_connected,
            createtime=create_time, elapsed_time=elapsed_time, avd_type=avd_type,
            avd_flavor=avd_flavor, is_local=False,
            device_information=device_information,
            zone=zone)

    @staticmethod
    def _GetZoneName(zone_info):
        """Get the zone name from the zone information of gce instance.

        Zone information is like:
        "https://www.googleapis.com/compute/v1/projects/project/zones/us-central1-c"
        We want to get "us-central1-c" as zone name.

        Args:
            zone_info: String, zone information of gce instance.

        Returns:
            Zone name of gce instance. None if zone name can't find.
        """
        zone_match = _RE_ZONE.match(zone_info)
        if zone_match:
            return zone_match.group("zone")

        logger.debug("Can't get zone name from %s.", zone_info)
        return None

    @staticmethod
    def GetAdbVncPortFromSSHTunnel(ip, avd_type):
        """Get forwarding adb and vnc port from ssh tunnel.

        Args:
            ip: String, ip address.
            avd_type: String, the AVD type.

        Returns:
            NamedTuple ForwardedPorts(vnc_port, adb_port) holding the ports
            used in the ssh forwarded call. Both fields are integers.
        """
        if avd_type not in utils.AVD_PORT_DICT:
            return utils.ForwardedPorts(vnc_port=None, adb_port=None)

        default_vnc_port = utils.AVD_PORT_DICT[avd_type].vnc_port
        default_adb_port = utils.AVD_PORT_DICT[avd_type].adb_port
        re_pattern = re.compile(_RE_SSH_TUNNEL_PATTERN %
                                (_RE_GROUP_VNC, default_vnc_port,
                                 _RE_GROUP_ADB, default_adb_port, ip))
        adb_port = None
        vnc_port = None
        process_output = subprocess.check_output(constants.COMMAND_PS)
        for line in process_output.splitlines():
            match = re_pattern.match(line)
            if match:
                adb_port = int(match.group(_RE_GROUP_ADB))
                vnc_port = int(match.group(_RE_GROUP_VNC))
                break

        logger.debug(("grathering detail for ssh tunnel. "
                      "IP:%s, forwarding (adb:%d, vnc:%d)"), ip, adb_port,
                     vnc_port)

        return utils.ForwardedPorts(vnc_port=vnc_port, adb_port=adb_port)
