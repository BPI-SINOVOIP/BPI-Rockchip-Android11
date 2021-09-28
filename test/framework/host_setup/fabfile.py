#
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
#

import imp
import os
import sys
import time

from fabric.api import env
from fabric.api import local
from fabric.api import run
from fabric.api import settings
from fabric.api import sudo
from fabric.contrib.files import contains
from fabric.contrib.files import exists
from fabric.context_managers import cd

_PIP_REQUIREMENTS_PATHS = [
    "test/vts/script/pip_requirements.txt",
    "test/framework/harnesses/host_controller/script/pip_requirements.txt"
]

# Path to the file that contains the abs path to the deployed vtslab pakcage.
_VTSLAB_PACKAGE_PATH_FILENAME = ".vtslab_package_path"

# Zone filter list for GCE instances.
_DEFAULT_GCE_ZONE_LIST = [
    "us-east1-b",
    "asia-northeast1-a",
]


def SetPassword(password):
    """Sets password for hosts to access through ssh and to run sudo commands

    usage: $ fab SetPassword:<password for hosts>

    Args:
        password: string, password for hosts.
    """
    env.password = password


def GetHosts(hosts_file_path, gce_instance_name=None, account=None):
    """Configures env.hosts to a given list of hosts.

    usage: $ fab GetHosts:<path to a source file contains hosts info>

    Args:
        hosts_file_path: string, path to a python file passed from command file
                         input.
        gce_instance_name: string, GCE instance name.
        account: string, account name used for the ssh connection.
    """
    if hosts_file_path.endswith(".py"):
        hosts_module = imp.load_source('hosts_module', hosts_file_path)
        env.hosts = hosts_module.EmitHostList()
    else:
        if not gce_instance_name or not account:
            print(
                "Please specify gce_instance_name and account using -H option. "
                "Example: -H <Google_Cloud_project>,<GCE_instance>,<account>")
            sys.exit(0)
        env.key_filename = "~/.ssh/google_compute_engine"
        gce_list_out = local(
            "gcloud compute instances list --project=%s --filter=\"zone:(%s)\""
            % (hosts_file_path, " ".join(_DEFAULT_GCE_ZONE_LIST)),
            capture=True)
        for line in gce_list_out.split("\n")[1:]:
            if line.startswith(gce_instance_name):
                env.hosts.append("%s@%s" % (account, line.strip().split()[-2]))


def SetupIptables(ip_address_file_path):
    """Configures iptables setting for all hosts listed.

    usage: $ fab SetupIptables:<path to a source file contains ip addresses of
             certified machines>

    Args:
        ip_address_file_path: string, path to a python file passed from command
                              file input.
    """
    ip_addresses_module = imp.load_source('ip_addresses_module',
                                          ip_address_file_path)
    ip_address_list = ip_addresses_module.EmitIPAddressList()

    sudo("apt-get install -y iptables-persistent")
    sudo("iptables -P INPUT ACCEPT")
    sudo("iptables -P FORWARD ACCEPT")
    sudo("iptables -F")

    for ip_address in ip_address_list:
        sudo(
            "iptables -A INPUT -p tcp -s %s --dport 22 -j ACCEPT" % ip_address)

    sudo("iptables -P INPUT DROP")
    sudo("iptables -P FORWARD DROP")
    sudo("iptables -A INPUT -p icmp -j ACCEPT")
    sudo("netfilter-persistent save")
    sudo("netfilter-persistent reload")


def SetupSudoers():
    """Append sudo rules for vtslab user.

    usage: $ fab SetupSudoers
    """
    if not contains("/etc/sudoers", "vtslab", use_sudo=True):
        sudo("echo '' | sudo tee -a /etc/sudoers")
        sudo("echo '# Let vtslab account have all authorization' | "
             "sudo tee -a /etc/sudoers")
        sudo("echo 'vtslab  ALL=(ALL:ALL) ALL' | sudo tee -a /etc/sudoers")


def SetupUSBPermission():
    """Sets up the USB permission for adb and fastboot.

    usage: $ fab SetupUSBPermission
    """
    sudo("curl --create-dirs -L -o /etc/udev/rules.d/51-android.rules -O -L "
         "https://raw.githubusercontent.com/snowdream/51-android/master/"
         "51-android.rules")
    sudo("chmod a+r /etc/udev/rules.d/51-android.rules")
    sudo("service udev restart")


def SetupADBVendorKeysEnvVar():
    """Appends scripts for ADB_VENDOR_KEYS path setup.

    In setup step, this function looks into .bashrc file for this script, and
    if there is not then appends the below scripts to .bashrc.
    Later when shell env created (through ssh or screen instance creation time),
    .bashrc file will look for _VTSLAB_PACKAGE_PATH_FILENAME and use the
    contents of the file to set ADB_VENDOR_KEYS.

    usage: $ fab SetupADBVendorKeysEnvVar
    """
    if not contains("~/.bashrc", _VTSLAB_PACKAGE_PATH_FILENAME):
        run("echo '' >> ~/.bashrc", )
        run("echo '# Set $ADB_VENDOR_KEYS as paths to adb private key files "
            "within the vtslab-package' >> ~/.bashrc")
        run("echo 'if [ -f ~/%s ]; then' >> ~/.bashrc" %
            _VTSLAB_PACKAGE_PATH_FILENAME)
        run("echo '  export ADB_VENDOR_KEYS=$(find $(cat ~/%s)/android-vtslab/"
            "testcases/DATA/ak -name \".*.ak\" | tr \"\\n\" \":\")' "
            ">> ~/.bashrc" % _VTSLAB_PACKAGE_PATH_FILENAME)
        run("echo 'fi' >> ~/.bashrc")


def _CheckADBVendorKeysEnvVar(vtslab_package_file_name=""):
    """Checks if there is a change in ADB_VENDOR_KEYS env variable.

    if there is, then the adbd needs to be restarted in the screen context
    before running the HC.

    Args:
        vtslab_package_file_name: string, the HC package file name that is about
                                  to be deployed.

    Returns:
        True if the list of the adb vendor key files have changed,
        False otherwise.
    """
    former_key_set = set()
    current_key_set = set()
    set_keyfile_set = lambda set, path_list: map(set.add, map(os.path.basename,
                                                              path_list))
    vtslab_package_path_filepath = "~/%s" % _VTSLAB_PACKAGE_PATH_FILENAME

    if exists(vtslab_package_path_filepath):
        former_HC_package_path = run("cat %s" % vtslab_package_path_filepath)
        former_HC_package_adbkey_path = os.path.join(
            former_HC_package_path, "android-vtslab/testcases/DATA/ak")
        if exists(former_HC_package_adbkey_path):
            adb_vendor_keys_list = run("find %s -name \".*.ak\"" %
                                       former_HC_package_adbkey_path).split()
            set_keyfile_set(former_key_set, adb_vendor_keys_list)

    if exists("~/run/%s.dir/android-vtslab/testcases/DATA/ak" %
              vtslab_package_file_name):
        adb_vendor_keys_list = run(
            "find ~/run/%s.dir/android-vtslab/testcases/DATA/ak -name \".*.ak\""
            % vtslab_package_file_name).split()
        set_keyfile_set(current_key_set, adb_vendor_keys_list)

    return former_key_set != current_key_set


def SetupPackages(ip_address_file_path=None):
    """Sets up the execution environment for vts `run` command.

    Need to temporarily open the ports for apt-get and pip commands.

    usage: $ fab SetupPackages

    Args:
        ip_address_file_path: string, path to a python file passed from command
                              file input. Will be passed to SetupIptables().
    """
    sudo("iptables -P INPUT ACCEPT")

    # todo : replace "kr.ubuntu." to "ubuntu" in /etc/apt/sources.list
    sudo("apt-get upgrade -y")
    sudo("apt-get update -y")
    sudo("apt-get install -y git-core gnupg flex bison gperf build-essential "
         "zip curl zlib1g-dev gcc-multilib g++-multilib x11proto-core-dev "
         "libx11-dev lib32z-dev ccache libgl1-mesa-dev libxml2-utils xsltproc "
         "unzip liblz4-tool udev screen")

    sudo("apt-get install -y android-tools-adb")
    sudo("usermod -aG plugdev $LOGNAME")

    SetupUSBPermission()

    sudo("apt-get update")
    sudo("apt-get install -y python2.7")
    sudo("apt-get install -y python-pip")
    run("pip install --upgrade pip")
    sudo("apt-get install -y python-virtualenv")

    sudo("apt-get install -y python-dev python-protobuf protobuf-compiler "
         "python-setuptools")

    for req_path in _PIP_REQUIREMENTS_PATHS:
        full_path = os.path.join(os.environ["ANDROID_BUILD_TOP"], req_path)
        pip_requirement_list = []
        try:
            requirements_fd = open(full_path, "r")
            lines = requirements_fd.readlines()
            for line in lines:
                req = line.rstrip()
                if req != "" and not req.startswith("#"):
                    pip_requirement_list.append(req)
        except IOError as e:
            print("%s: %s" % (e.strerror, full_path))
            return
        sudo("pip install %s" % " ".join(pip_requirement_list))

    sudo("pip install --upgrade protobuf")

    lsb_result = run("lsb_release -c -s")
    sudo("echo \"deb http://packages.cloud.google.com/apt cloud-sdk-%s "
         "main\" | sudo tee -a /etc/apt/sources.list.d/google-cloud-sdk.list" %
         lsb_result)
    sudo("curl https://packages.cloud.google.com/apt/doc/apt-key.gpg | "
         "sudo apt-key add -")
    sudo("apt-get update && sudo apt-get install -y google-cloud-sdk")
    sudo("apt-get install -y google-cloud-sdk-app-engine-java "
         "google-cloud-sdk-app-engine-python kubectl")

    sudo("apt-get install -y m4 bison")

    if ip_address_file_path is not None:
        SetupIptables(ip_address_file_path)

    SetupADBVendorKeysEnvVar()


def DeployVtslab(vtslab_package_gcs_url=None):
    """Deploys vtslab package.

    Fetches and deploy vtslab by going through the processes described below
    1. Send the "exit --wait_for_jobs=True" command to all detached screen.
       And let the screen to terminate itself.
    2. Create a new screen instance that downloads and runs the new HC,
       give password and device command to the HC without actually attaching it

    usage: $ fab DeployVtslab -p <password> -H hosts.py -f <gs://vtslab-release/...>

    Args:
        vtslab_package_gcs_url: string, URL to a certain vtslab package file.
    """
    if not vtslab_package_gcs_url:
        print("Please specify vtslab package file URL using -f option.")
        return
    elif not vtslab_package_gcs_url.startswith("gs://vtslab-release/"):
        print("Please spcify a valid URL for the vtslab package.")
        return
    else:
        vti = "vtslab-schedule-" + vtslab_package_gcs_url[len(
            "gs://vtslab-release/"):].split("/")[0] + ".appspot.com"
    with settings(warn_only=True):
        screen_list_result = run("screen -list")
    lines = screen_list_result.split("\n")
    for line in lines:
        if "(Detached)" in line:
            screen_name = line.split("\t")[1]
            print(screen_name)
            with settings(warn_only=True):
                run("screen -S %s -X stuff \"exit --wait_for_jobs=True\"" %
                    screen_name)
                run("screen -S %s -X stuff \"^M\"" % screen_name)
                run("screen -S %s -X stuff \"exit\"" % screen_name)
                run("screen -S %s -X stuff \"^M\"" % screen_name)

    vtslab_package_file_name = os.path.basename(vtslab_package_gcs_url)
    run("mkdir -p ~/run/%s.dir/" % vtslab_package_file_name)
    with cd("~/run/%s.dir" % vtslab_package_file_name):
        run("gsutil cp %s ./" % vtslab_package_gcs_url)
        run("unzip -o %s" % vtslab_package_file_name)
        adb_vendor_keys_changed = _CheckADBVendorKeysEnvVar(
            vtslab_package_file_name)
        run("pwd > ~/%s" % _VTSLAB_PACKAGE_PATH_FILENAME)

    with cd("~/run/%s.dir/android-vtslab/tools" % vtslab_package_file_name):
        new_screen_name = run("cat ../testcases/version.txt")

    with cd("~/run/%s.dir/android-vtslab/tools" % vtslab_package_file_name):
        run("./make_screen %s ; sleep 1" % new_screen_name)

    if adb_vendor_keys_changed:
        reset_adbd = ""
        while reset_adbd.lower() not in ["y", "n"]:
            if reset_adbd:
                print("Please type 'y' or 'n'")
            reset_adbd = raw_input(
                "Reset adb server daemon on host %s (y/n)? " % env.host)
        if reset_adbd.lower() == "y":
            run("screen -S %s -X stuff \"adb kill-server^M\"" %
                new_screen_name)
            run("screen -S %s -X stuff \"adb start-server^M\"" %
                new_screen_name)
    run("screen -S %s -X stuff \"./run --vti=%s\"" % (new_screen_name, vti))
    run("screen -S %s -X stuff \"^M\"" % new_screen_name)
    time.sleep(5)
    run("screen -S %s -X stuff \"password\"" % new_screen_name)
    run("screen -S %s -X stuff \"^M\"" % new_screen_name)
    run("screen -S %s -X stuff \"%s\"" % (new_screen_name, env.password))
    run("screen -S %s -X stuff \"^M\"" % new_screen_name)
    run("screen -S %s -X stuff \"device --lease=True\"" % new_screen_name)
    run("screen -S %s -X stuff \"^M\"" % new_screen_name)

    with cd("~/run/%s.dir" % vtslab_package_file_name):
        run("rm %s" % vtslab_package_file_name)


def DeployGCE(vtslab_package_gcs_url=None):
    """Deploys a vtslab package to GCE nodes.

    Fetches and deploy vtslab on monitor-hc by doing;
    1. Download android-vtslab-<>.zip from GCS using the given URL and upzip it.
    2. Send the Ctrl-c key input to all detached screen, then cursor-up
       key input and enter key input, making the screen to execute
       the last run command.

    usage: $ fab DeployVtslab -p <password> -H <Google Cloud Platform project name> -f <gs://vtslab-release/...>

    Args:
        vtslab_package_gcs_url: string, URL to a certain vtslab package file.
    """
    if not vtslab_package_gcs_url:
        print("Please specify vtslab package file URL using -f option.")
        return
    elif not vtslab_package_gcs_url.startswith("gs://"):
        print("Please spcify a valid URL for the vtslab package.")
        return

    vtslab_package_file_name = os.path.basename(vtslab_package_gcs_url)
    with cd("~/run"):
        run("gsutil cp %s ./" % vtslab_package_gcs_url)
        run("unzip -o %s" % vtslab_package_file_name)
        run("rm %s" % vtslab_package_file_name)

    with settings(warn_only=True):
        screen_list_result = run("screen -list")
    lines = screen_list_result.split("\n")
    for line in lines:
        if "(Detached)" in line:
            screen_name = line.split("\t")[1]
            run("screen -S %s -X stuff \"^C\"" % screen_name)
            run("screen -S %s -X stuff \"\033[A\"" % screen_name)
            run("screen -S %s -X stuff \"^M\"" % screen_name)
