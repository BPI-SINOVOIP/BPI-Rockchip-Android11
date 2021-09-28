# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Utility functions for AFE-based interactions.

NOTE: This module should only be used in the context of a running test. Any
      utilities that require accessing the AFE, should do so by creating
      their own instance of the AFE client and interact with it directly.
"""

import common
import logging
import traceback
import urlparse

from autotest_lib.client.common_lib import global_config
from autotest_lib.server.cros import autoupdater
from autotest_lib.server.cros import provision
from autotest_lib.server.cros.dynamic_suite import frontend_wrappers
from autotest_lib.site_utils import stable_version_classify as sv
from autotest_lib.server import site_utils as server_utils
from autotest_lib.server.cros.dynamic_suite import constants as ds_constants
from autotest_lib.server.cros.dynamic_suite import tools

from chromite.lib import auto_updater
from chromite.lib import remote_access


AFE = frontend_wrappers.RetryingAFE(timeout_min=5, delay_sec=10)
_CROS_VERSION_MAP = AFE.get_stable_version_map(AFE.CROS_IMAGE_TYPE)
_FIRMWARE_VERSION_MAP = AFE.get_stable_version_map(AFE.FIRMWARE_IMAGE_TYPE)
_FAFT_VERSION_MAP = AFE.get_stable_version_map(AFE.FAFT_IMAGE_TYPE)

_CONFIG = global_config.global_config
ENABLE_DEVSERVER_TRIGGER_AUTO_UPDATE = _CONFIG.get_config_value(
        'CROS', 'enable_devserver_trigger_auto_update', type=bool,
        default=False)


def _host_in_lab(host):
    """Check if the host is in the lab and an object the AFE knows.

    This check ensures that autoserv and the host's current job is running
    inside a fully Autotest instance, aka a lab environment. If this is the
    case it then verifies the host is registed with the configured AFE
    instance.

    @param host: Host object to verify.

    @returns The host model object.
    """
    if not host.job or not host.job.in_lab:
        return False
    return host._afe_host


def _log_image_name(image_name):
    try:
        logging.debug("_log_image_name: image (%s)", image_name)
        server_utils.ParseBuildName(name=image_name)
    except Exception:
        logging.error(traceback.format_exc())


def _format_image_name(board, version):
    return "%s-release/%s" % (board, version)


def get_stable_cros_image_name_v2(info, _config_override=None):
    if sv.classify_board(info.board, _config_override=_config_override) == sv.FROM_HOST_CONFIG:
        logging.debug("get_stable_cros_image_name_v2: board %s from host_info_store" % info.board)
        out = _format_image_name(board=info.board, version=info.cros_stable_version)
        _log_image_name(out)
        return out
    logging.debug("get_stable_cros_image_name_v2: board %s from autotest frontend" % info.board)
    return get_stable_cros_image_name(info.board)


def get_stable_servo_cros_image_name_v2(servo_version_from_hi, board, _config_override=None):
    """
    @param servo_version_from_hi (string or None) : the stable version image name taken from the host info store.
                                                    A value of None means that that the host_info_store does not exist or
                                                    ultimately not contain a servo_stable_version field.
    @param board (string) : the board of the labstation or servo v3 that we're getting the stable version of
    """
    logging.debug("get_stable_servo_cros_image_name_v2: servo_version_from_hi (%s) board (%s)" % (servo_version_from_hi, board))
    if sv.classify_board(board, _config_override=_config_override) != sv.FROM_HOST_CONFIG:
        logging.debug("get_stable_servo_cros_image_name_v2: servo version for board (%s) from afe" % board)
        return get_stable_cros_image_name(board)
    if servo_version_from_hi is not None:
        logging.debug("get_stable_servo_cros_image_name_v2: servo version (%s) from host_info_store" % servo_version_from_hi)
        out = _format_image_name(board=board, version=servo_version_from_hi)
        _log_image_name(out)
        return out
    logging.debug("get_stable_servo_cros_image_name_v2: no servo version provided. board is (%s)" % board)
    logging.debug("get_stable_servo_cros_image_name_v2: falling back to afe if possible")
    out = None
    # get_stable_cros_image_name uses the AFE as the source of truth.
    try:
        out = get_stable_cros_image_name(board)
    except Exception:
        logging.error("get_stable_servo_cros_image_name_v2: error falling back to AFE (%s)" % traceback.format_exc())
    return out


def get_stable_cros_image_name(board):
    """Retrieve the Chrome OS stable image name for a given board.

    @param board: Board to lookup.

    @returns Name of a Chrome OS image to be installed in order to
            repair the given board.
    """
    return _CROS_VERSION_MAP.get_image_name(board)


def get_stable_firmware_version_v2(info, _config_override=None):
    if sv.classify_model(info.model, _config_override=_config_override) == sv.FROM_HOST_CONFIG:
        logging.debug("get_stable_firmware_version_v2: model %s from host_info_store" % info.model)
        return info.firmware_stable_version
    logging.debug("get_stable_cros_image_name_v2: model %s from autotest frontend" % info.model)
    return get_stable_firmware_version(info.model)


def get_stable_firmware_version(model):
    """Retrieve the stable firmware version for a given model.

    @param model: Model to lookup.

    @returns A version of firmware to be installed via
             `chromeos-firmwareupdate` from a repair build.
    """
    return _FIRMWARE_VERSION_MAP.get_version(model)


def get_stable_faft_version_v2(info, _config_override=None):
    if sv.classify_board(info.board, _config_override=_config_override) == sv.FROM_HOST_CONFIG:
        logging.debug("get_stable_faft_version_v2: model %s from host_info_store" % info.model)
        return info.faft_stable_version
    logging.debug("get_stable_faft_version_v2: model %s from autotest frontend" % info.model)
    return get_stable_faft_version(info.board)


def get_stable_faft_version(board):
    """Retrieve the stable firmware version for FAFT DUTs.

    @param board: Board to lookup.

    @returns A version of firmware to be installed in order to
            repair firmware on a DUT used for FAFT testing.
    """
    return _FAFT_VERSION_MAP.get_version(board)


def clean_provision_labels(host):
    """Clean provision-related labels.

    @param host: Host object.
    """
    info = host.host_info_store.get()
    info.clear_version_labels()
    attributes = host.get_attributes_to_clear_before_provision()
    for key in attributes:
      info.attributes.pop(key, None)

    host.host_info_store.commit(info)


def add_provision_labels(host, version_prefix, image_name,
                         provision_attributes={}):
    """Add provision labels for host.

    @param host: Host object.
    @param version_prefix: a string version prefix, e.g. "cros-version:"
    @param image_name: a string image name, e.g. peppy-release/R70-11011.0.0.
    @param provision_attributes: a map, including attributes for provisioning,
        e.g. {"job_repo_url": "http://..."}
    """
    info = host.host_info_store.get()
    info.attributes.update(provision_attributes)
    info.set_version_label(version_prefix, image_name)
    host.host_info_store.commit(info)


def machine_install_and_update_labels(host, update_url,
                                      use_quick_provision=False,
                                      with_cheets=False, staging_server=None):
    """Install a build and update the version labels on a host.

    @param host: Host object where the build is to be installed.
    @param update_url: URL of the build to install.
    @param use_quick_provision:  If true, then attempt to use
        quick-provision for the update.
    @param with_cheets: If true, installation is for a specific, custom
        version of Android for a target running ARC.
    @param staging_server: Sever where images have been staged. Typically,
        an instance of dev_server.ImageServer.
    """
    clean_provision_labels(host)
    # TODO(crbug.com/1049346): The try-except block exists to catch failures in
    # chromite auto_updater that may occur due to autotest/chromite version
    # mismatch. This should be removed once that bug is resolved.
    try:
        # Get image_name in the format <board>-release/Rxx-12345.0.0 from the
        # update_url.
        image_name = '/'.join(urlparse.urlparse(update_url).path.split('/')[-2:])
        with remote_access.ChromiumOSDeviceHandler(host.ip) as device:
            updater = auto_updater.ChromiumOSUpdater(
                device, build_name=None, payload_dir=image_name,
                staging_server=staging_server.url())
            updater.CheckPayloads()
            updater.PreparePayloadPropsFile()
            updater.RunUpdate()
        repo_url = tools.get_package_url(staging_server.url(), image_name)
        host_attributes = {ds_constants.JOB_REPO_URL: repo_url}
    except Exception as e:
        logging.warning(
            "Chromite auto_updater has failed with the exception: %s", e)
        logging.debug("Attempting to provision with quick provision.")
        updater = autoupdater.ChromiumOSUpdater(
            update_url, host=host, use_quick_provision=use_quick_provision)
        image_name, host_attributes = updater.run_update()
    if with_cheets:
        image_name += provision.CHEETS_SUFFIX
    add_provision_labels(host, host.VERSION_PREFIX, image_name,
                         host_attributes)
