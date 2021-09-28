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
r"""List entry point.

List will handle all the logic related to list a local/remote instance
of an Android Virtual Device.
"""

from __future__ import print_function
import getpass
import logging
import os

from acloud import errors
from acloud.internal import constants
from acloud.internal.lib import auth
from acloud.internal.lib import gcompute_client
from acloud.internal.lib import utils
from acloud.list import instance
from acloud.public import config


logger = logging.getLogger(__name__)

_COMMAND_PS_LAUNCH_CVD = ["ps", "-wweo", "lstart,cmd"]


def _ProcessInstances(instance_list):
    """Get more details of remote instances.

    Args:
        instance_list: List of dicts which contain info about the remote instances,
                       they're the response from the GCP GCE api.

    Returns:
        instance_detail_list: List of instance.Instance() with detail info.
    """
    return [instance.RemoteInstance(gce_instance) for gce_instance in instance_list]


def PrintInstancesDetails(instance_list, verbose=False):
    """Display instances information.

    Example of non-verbose case:
    [1]device serial: 127.0.0.1:55685 (ins-1ff036dc-5128057-cf-x86-phone-userdebug)
    [2]device serial: 127.0.0.1:60979 (ins-80952669-5128057-cf-x86-phone-userdebug)
    [3]device serial: 127.0.0.1:6520 (local-instance)

    Example of verbose case:
    [1] name: ins-244710f0-5091715-aosp-cf-x86-phone-userdebug
        IP: None
        create time: 2018-10-25T06:32:08.182-07:00
        status: TERMINATED
        avd type: cuttlefish
        display: 1080x1920 (240)

    [2] name: ins-82979192-5091715-aosp-cf-x86-phone-userdebug
        IP: 35.232.77.15
        adb serial: 127.0.0.1:33537
        create time: 2018-10-25T06:34:22.716-07:00
        status: RUNNING
        avd type: cuttlefish
        display: 1080x1920 (240)

    Args:
        verbose: Boolean, True to print all details and only full name if False.
        instance_list: List of instances.
    """
    if not instance_list:
        print("No remote or local instances found")

    for num, instance_info in enumerate(instance_list, 1):
        idx_str = "[%d]" % num
        utils.PrintColorString(idx_str, end="")
        if verbose:
            print(instance_info.Summary())
            # add space between instances in verbose mode.
            print("")
        else:
            print(instance_info)


def GetRemoteInstances(cfg):
    """Look for remote instances.

    We're going to query the GCP project for all instances that created by user.

    Args:
        cfg: AcloudConfig object.

    Returns:
        instance_list: List of remote instances.
    """
    credentials = auth.CreateCredentials(cfg)
    compute_client = gcompute_client.ComputeClient(cfg, credentials)
    filter_item = "labels.%s=%s" % (constants.LABEL_CREATE_BY, getpass.getuser())
    all_instances = compute_client.ListInstances(instance_filter=filter_item)

    logger.debug("Instance list from: (filter: %s\n%s):",
                 filter_item, all_instances)

    return _ProcessInstances(all_instances)


def _GetLocalCuttlefishInstances():
    """Look for local cuttelfish instances.

    Gather local instances information from cuttlefish runtime config.

    Returns:
        instance_list: List of local instances.
    """
    local_instance_list = []
    for cf_runtime_config_path in instance.GetAllLocalInstanceConfigs():
        ins = instance.LocalInstance(cf_runtime_config_path)
        if ins.CvdStatus():
            local_instance_list.append(ins)
        else:
            logger.info("cvd runtime config found but instance is not active:%s"
                        , cf_runtime_config_path)
    return local_instance_list


def GetActiveCVD(local_instance_id):
    """Check if the local AVD with specific instance id is running

    Args:
        local_instance_id: Integer of instance id.

    Return:
        LocalInstance object.
    """
    cfg_path = instance.GetLocalInstanceConfig(local_instance_id)
    if cfg_path:
        ins = instance.LocalInstance(cfg_path)
        if ins.CvdStatus():
            return ins
    cfg_path = instance.GetDefaultCuttlefishConfig()
    if local_instance_id == 1 and os.path.isfile(cfg_path):
        ins = instance.LocalInstance(cfg_path)
        if ins.CvdStatus():
            return ins
    return None


def GetLocalInstances():
    """Look for local cuttleifsh and goldfish instances.

    Returns:
        List of local instances.
    """
    # Running instances on local is not supported on all OS.
    if not utils.IsSupportedPlatform():
        return []

    return (_GetLocalCuttlefishInstances() +
            instance.LocalGoldfishInstance.GetExistingInstances())


def GetInstances(cfg):
    """Look for remote/local instances.

    Args:
        cfg: AcloudConfig object.

    Returns:
        instance_list: List of instances.
    """
    return GetRemoteInstances(cfg) + GetLocalInstances()


def ChooseInstancesFromList(instances):
    """Let user choose instances from a list.

    Args:
        instances: List of Instance objects.

    Returns:
         List of Instance objects.
    """
    if len(instances) > 1:
        print("Multiple instances detected, choose any one to proceed:")
        return utils.GetAnswerFromList(instances, enable_choose_all=True)
    return instances


def ChooseInstances(cfg, select_all_instances=False):
    """Get instances.

    Retrieve all remote/local instances and if there is more than 1 instance
    found, ask user which instance they'd like.

    Args:
        cfg: AcloudConfig object.
        select_all_instances: True if select all instances by default and no
                              need to ask user to choose.

    Returns:
        List of Instance() object.
    """
    instances = GetInstances(cfg)
    if not select_all_instances:
        return ChooseInstancesFromList(instances)
    return instances


def ChooseOneRemoteInstance(cfg):
    """Get one remote cuttlefish instance.

    Retrieve all remote cuttlefish instances and if there is more than 1 instance
    found, ask user which instance they'd like.

    Args:
        cfg: AcloudConfig object.

    Raises:
        errors.NoInstancesFound: No cuttlefish remote instance found.

    Returns:
        list.Instance() object.
    """
    instances_list = GetCFRemoteInstances(cfg)
    if not instances_list:
        raise errors.NoInstancesFound(
            "Can't find any cuttlefish remote instances, please try "
            "'$acloud create' to create instances")
    if len(instances_list) > 1:
        print("Multiple instances detected, choose any one to proceed:")
        instances = utils.GetAnswerFromList(instances_list,
                                            enable_choose_all=False)
        return instances[0]

    return instances_list[0]


def FilterInstancesByNames(instances, names):
    """Find instances by names.

    Args:
        instances: Collection of Instance objects.
        names: Collection of strings, the names of the instances to search for.

    Returns:
        List of Instance objects.

    Raises:
        errors.NoInstancesFound if any instance is not found.
    """
    instance_map = {inst.name: inst for inst in instances}
    found_instances = []
    missing_instance_names = []
    for name in names:
        if name in instance_map:
            found_instances.append(instance_map[name])
        else:
            missing_instance_names.append(name)

    if missing_instance_names:
        raise errors.NoInstancesFound("Did not find the following instances: %s" %
                                      " ".join(missing_instance_names))
    return found_instances


def GetInstancesFromInstanceNames(cfg, instance_names):
    """Get instances from instance names.

    Turn a list of instance names into a list of Instance().

    Args:
        cfg: AcloudConfig object.
        instance_names: list of instance name.

    Returns:
        List of Instance() objects.

    Raises:
        errors.NoInstancesFound: No instances found.
    """
    return FilterInstancesByNames(GetInstances(cfg), instance_names)


def FilterInstancesByAdbPort(instances, adb_port):
    """Find an instance by adb port.

    Args:
        instances: Collection of Instance objects.
        adb_port: int, adb port of the instance to search for.

    Returns:
        List of Instance() objects.

    Raises:
        errors.NoInstancesFound: No instances found.
    """
    all_instance_info = []
    for instance_object in instances:
        if instance_object.adb_port == adb_port:
            return [instance_object]
        all_instance_info.append(instance_object.fullname)

    # Show devices information to user when user provides wrong adb port.
    if all_instance_info:
        hint_message = ("No instance with adb port %d, available instances:\n%s"
                        % (adb_port, "\n".join(all_instance_info)))
    else:
        hint_message = "No instances to delete."
    raise errors.NoInstancesFound(hint_message)


def GetCFRemoteInstances(cfg):
    """Look for cuttlefish remote instances.

    Args:
        cfg: AcloudConfig object.

    Returns:
        instance_list: List of instance names.
    """
    instances = GetRemoteInstances(cfg)
    return [ins for ins in instances if ins.avd_type == constants.TYPE_CF]


def Run(args):
    """Run list.

    Args:
        args: Namespace object from argparse.parse_args.
    """
    instances = GetLocalInstances()
    cfg = config.GetAcloudConfig(args)
    if not args.local_only and cfg.SupportRemoteInstance():
        instances.extend(GetRemoteInstances(cfg))

    PrintInstancesDetails(instances, args.verbose)
