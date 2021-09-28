# Copyright 2020 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from autotest_lib.client.common_lib import error
from chromite.lib import retry_util


_VPD_BASE_CMD = 'vpd -i %s %s %s'
_RW = 'RW_VPD'
_RO = 'RO_VPD'


def _check_partition(partition):
    """
    Used to validate input string in other functions.

    @param partition: If this is not 'RO_VPD' or 'RW_VPD', raise a ValueError.

    """
    if partition not in [_RW, _RO]:
        raise ValueError("partition should be 'RW_VPD' or 'RO_VPD'")


def dump_vpd_log(host, force=True, retries=3):
    """
    Applies changes to the VPD settings by flushing them to the VPD cache and
    output files.

    @param host: Host to run the command on.
    @param force: True to pass in the --force parameter to forcefully dump
                  the log. False to omit it.
    @param retries: Number of times to try rerunning the command in case of
                    error.

    """
    vpd_dump_cmd = 'dump_vpd_log%s' % (' --force' if force else '')
    retry_util.RetryException(error.AutoservRunError, retries, host.run,
                              vpd_dump_cmd)


def vpd_get(host, key, partition='RW_VPD', retries=3):
    """
    Gets the VPD value associated with the input key.

    @param host: Host to run the command on.
    @param key: Key of the desired VPD value.
    @param partition: Which partition to access. 'RO_VPD' or 'RW_VPD'.
    @param retries: Number of times to try rerunning the command in case of
                    error.

    """
    _check_partition(partition)
    get_cmd = _VPD_BASE_CMD % (partition, '-g', key)
    try:
        return retry_util.RetryException(error.AutoservRunError, retries,
                                         host.run, get_cmd).stdout
    except error.AutoservRunError as e:
        if 'was not found' in str(e.result_obj.stderr):
            return None
        else:
            raise e


def vpd_set(host, vpd_dict, partition='RW_VPD', dump=False, force_dump=False,
            retries=3):
    """
    Sets the given key/value pairs in the specified VPD partition.

    @param host: Host to run the command on.
    @param vpd_dict: Dictionary containing the VPD key/value pairs to set.
                     Dictionary keys should be the VPD key strings, and values
                     should be the desired values to write.
    @param partition: Which partition to access. 'RO_VPD' or 'RW_VPD'.
    @param dump: If True, also run dump_vpd_log command after setting the
                 vpd values.
    @param force_dump: Whether or not to forcefully dump the vpd log.
    @param retries: Number of times to try rerunning the command in case of
                    error.

    """
    _check_partition(partition)
    for vpd_key in vpd_dict:
        set_cmd = _VPD_BASE_CMD % (partition, '-s',
                  (vpd_key + '=' + str(vpd_dict[vpd_key])))
        retry_util.RetryException(error.AutoservRunError, retries,
                                  host.run, set_cmd).stdout

    if dump:
        dump_vpd_log(host, force=force_dump, retries=retries)


def vpd_delete(host, key, partition='RW_VPD', retries=3):
    """
    Deletes the specified key from the specified VPD partition.

    @param host: Host to run the command on.
    @param key: The VPD value to delete.
    @param partition: Which partition to access. 'RO_VPD' or 'RW_VPD'.
    @param retries: Number of times to try rerunning the command in case of
                    error.

    """
    _check_partition(partition)
    if not vpd_get(host, key, partition=partition, retries=retries):
        return

    del_cmd = _VPD_BASE_CMD % (partition, '-d', key)
    retry_util.RetryException(error.AutoservRunError, retries, host.run,
                              del_cmd).stdout