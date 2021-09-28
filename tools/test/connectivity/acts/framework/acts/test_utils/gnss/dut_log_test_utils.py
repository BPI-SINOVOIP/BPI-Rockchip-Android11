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

import os
import time
import errno

DEVICE_CFG_FOLDER = "/data/vendor/radio/diag_logs/cfg/"
DEVICE_DIAGMDLOG_FOLDER = "/data/vendor/radio/diag_logs/logs/"
MDLOG_SETTLING_TIME = 2
MDLOG_PROCESS_KILL_TIME = 3
NOHUP_CMD = "nohup diag_mdlog -f {} -o {} -s 100 -c &> /dev/null &"


def find_device_qxdm_log_mask(ad, maskfile):
    """Finds device's diagmd mask file

           Args:
               ad: the target android device, AndroidDevice object
               maskfile: Device's mask file name

           Return:
               exists, if cfg file is present

           Raises:
               FileNotFoundError if maskfile is not present
    """

    if ".cfg" not in maskfile:
        # errno.ENOENT - No such file or directory
        raise FileNotFoundError(errno.ENOENT, os.strerror(errno.ENOENT),
                                maskfile)
    else:
        cfg_path = os.path.join(DEVICE_CFG_FOLDER, maskfile)
        device_mask_file = ad.adb.shell('test -e %s && echo exists' % cfg_path)
        return device_mask_file


def set_diagmdlog_command(ad, maskfile):
    """Sets diagmdlog command to run in background

       Args:
           ad: the target android device, AndroidDevice object
           maskfile: mask file name

    """
    cfg_path = os.path.join(DEVICE_CFG_FOLDER, maskfile)
    ad.adb.shell(NOHUP_CMD.format(cfg_path, DEVICE_DIAGMDLOG_FOLDER))
    ad.log.info("Running diag_mdlog in the background")
    time.sleep(MDLOG_SETTLING_TIME)


def verify_diagmd_folder_exists(ad):
    """Verify diagmd folder existence in device

       Args:
           ad: the target android device, AndroidDevice object

    """
    mask_folder_exists = ad.adb.shell(
        'test -d %s && echo exists' % DEVICE_CFG_FOLDER)
    diag_folder_exists = ad.adb.shell(
        'test -d %s && echo exists' % DEVICE_DIAGMDLOG_FOLDER)
    if not mask_folder_exists and diag_folder_exists:
        ad.adb.shell("mkdir " + DEVICE_CFG_FOLDER)
        ad.adb.shell("mkdir " + DEVICE_DIAGMDLOG_FOLDER)


def start_diagmdlog_background(ad, maskfile="default.cfg", is_local=True):
    """Runs diagmd_log in background

       Args:
           ad: the target android device, AndroidDevice object
           maskfile: Local Mask file path or Device's mask file name
           is_local: False, take cfgfile from config.
                     True, find cfgfile in device and run diagmdlog

       Raises:
           FileNotFoundError if maskfile is not present
           ProcessLookupError if diagmdlog process not present
    """
    if is_local:
        find_device_qxdm_log_mask(ad, maskfile)
        set_diagmdlog_command(ad, maskfile)
    else:
        if not os.path.isfile(maskfile):
            # errno.ENOENT - No such file or directory
            raise FileNotFoundError(errno.ENOENT, os.strerror(errno.ENOENT),
                                    maskfile)
        else:
            cfgfilename = os.path.basename(maskfile)
            verify_diagmd_folder_exists(ad)
            ad.adb.push("{} {}".format(maskfile, DEVICE_CFG_FOLDER))
            set_diagmdlog_command(ad, cfgfilename)
    output = ad.adb.shell("pgrep diag_mdlog")
    ad.log.info("Checking diag_mdlog in process")
    if not output:
        # errno.ESRCH - No such process
        raise ProcessLookupError(errno.ESRCH, os.strerror(errno.ESRCH),
                                 "diag_mdlog")


def stop_background_diagmdlog(ad, local_logpath, keep_logs=True):
    """Stop diagmdlog and pulls diag_mdlog from android device

       Args:
           ad: the target android device, AndroidDevice object
           local_logpath: Local file path to pull the diag_mdlog logs
           keep_logs: False, delete log files from the diag_mdlog path

       Raises:
           ProcessLookupError if diagmdlog process not present
    """
    ps_output = ad.adb.shell("pgrep diag_mdlog")
    ad.log.info("Checking diag_mdlog in process")
    if ps_output:
        output = ad.adb.shell("diag_mdlog -k")
        time.sleep(MDLOG_PROCESS_KILL_TIME)
        if "stopping" in output:
            ad.log.debug("Stopping diag_mdlog")
            ad.adb.pull("{} {}".format(DEVICE_DIAGMDLOG_FOLDER, local_logpath))
            ad.log.debug("Pulling diag_logs from the device to local")
            if not keep_logs:
                ad.adb.shell("rm -rf " + DEVICE_DIAGMDLOG_FOLDER + "*.*")
                ad.log.debug("diagmd logs are deleted from device")
            else:
                ad.log.debug("diagmd logs are not deleted from device")
        else:
            output = ad.adb.shell("pidof diag_mdlog")
            if output:
                ad.adb.shell("kill -9 {}".format(output))
                ad.log.debug("Kill the existing qxdm process")
                ad.adb.pull("{} {}".format(DEVICE_DIAGMDLOG_FOLDER,
                                           local_logpath))
                ad.log.debug("Pulling diag_logs from the device to local")
            else:
                # errno.ESRCH - No such process
                raise ProcessLookupError(errno.ESRCH, os.strerror(errno.ESRCH),
                                         "diag_mdlog")
    else:
        # errno.ESRCH - No such process
        raise ProcessLookupError(errno.ESRCH, os.strerror(errno.ESRCH),
                                 "diag_mdlog")
