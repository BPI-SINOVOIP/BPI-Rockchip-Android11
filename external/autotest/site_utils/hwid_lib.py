# Copyright 2016 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import json
import logging
import urllib2

from autotest_lib.client.common_lib import global_config

# HWID info types to request.
HWID_INFO_LABEL = 'dutlabel'
HWID_INFO_BOM = 'bom'
HWID_INFO_SKU = 'sku'
HWID_INFO_TYPES = [HWID_INFO_BOM, HWID_INFO_SKU, HWID_INFO_LABEL]

# HWID url vars.
HWID_VERSION = 'v1'
HWID_BASE_URL = 'https://www.googleapis.com/chromeoshwid'
CHROMEOS_HWID_SERVER_URL = "https://chromeos-hwid.appspot.com/api/chromeoshwid"
URL_FORMAT_STRING='%(base_url)s/%(version)s/%(info_type)s/%(hwid)s/?key=%(key)s'

# Key file name to use when we don't want hwid labels.
KEY_FILENAME_NO_HWID = 'no_hwid_labels'

class HwIdException(Exception):
    """Raised whenever anything fails in the hwid info request."""


def get_hwid_info(hwid, info_type, key_file):
    """Given a hwid and info type, return a dict of the requested info.

    @param hwid: hwid to use for the query.
    @param info_type: String of info type requested.
    @param key_file: Filename that holds the key for authentication.

    @return: A dict of the info.

    @raises HwIdException: If hwid/info_type/key_file is invalid or there's an
                           error anywhere related to getting the raw hwid info
                           or decoding it.
    """
    # There are situations we don't want to call out to the hwid service, we use
    # the key_file name as the indicator for that.
    if key_file == KEY_FILENAME_NO_HWID:
        return {}

    if not isinstance(hwid, str):
        raise ValueError('hwid is not a string.')

    if info_type not in HWID_INFO_TYPES:
        raise ValueError('invalid info type: "%s".' % info_type)

    hwid_info_dict = _try_hwid_v1(hwid, info_type)
    if hwid_info_dict is not None:
        return hwid_info_dict
    else:
        logging.debug("Switch back to use old endpoint")

    key = None
    with open(key_file) as f:
        key = f.read().strip()

    url_format_dict = {'base_url': HWID_BASE_URL,
                       'version': HWID_VERSION,
                       'info_type': info_type,
                       'hwid': urllib2.quote(hwid),
                       'key': key}
    return _fetch_hwid_response(url_format_dict)


def get_all_possible_dut_labels(key_file):
    """Return all possible labels that can be supplied by dutlabels.

    We can send a dummy key to the service to retrieve all the possible
    labels the service will provide under the dutlabel api call.  We need
    this in order to track which labels we can remove if they're not detected
    on a dut anymore.

    @param key_file: Filename that holds the key for authentication.

    @return: A list of all possible labels.
    """
    return get_hwid_info('dummy_hwid', HWID_INFO_LABEL, key_file).get(
            'possible_labels', [])


def _try_hwid_v1(hwid, info_type):
    """Try chromeos-hwid endpoints for fetching hwid info.

    @param hwid: a string hardware ID.
    @param info_type: String of info type requested.

    @return a dict of hwid info.
    """
    key_file_path = global_config.global_config.get_config_value(
            'CROS', 'NEW_HWID_KEY', type=str, default="")
    if key_file_path == "":
        return None

    key = None
    with open(key_file_path) as f:
        key = f.read().strip()

    if key is None:
        return None

    url_format_dict = {'base_url': CHROMEOS_HWID_SERVER_URL,
                       'version': HWID_VERSION,
                       'info_type': info_type,
                       'hwid': urllib2.quote(hwid),
                       'key': key}

    try:
        return _fetch_hwid_response(url_format_dict)
    except Exception as e:
        logging.debug("fail to call new HWID endpoint: %s", str(e))
        return None


def _fetch_hwid_response(req_parameter_dict):
    """Fetch and parse hwid response.

    @param req_parameter_dict: A dict of url parameters.

    @return a dict of hwid info.
    """
    url_request = URL_FORMAT_STRING % req_parameter_dict
    try:
        page_contents = urllib2.urlopen(url_request)
    except (urllib2.URLError, urllib2.HTTPError) as e:
        # TODO(kevcheng): Might need to scrub out key from exception message.
        raise HwIdException('error retrieving raw hwid info: %s' % e)

    try:
        hwid_info_dict = json.load(page_contents)
    except ValueError as e:
        raise HwIdException('error decoding hwid info: %s - "%s"' %
                            (e, page_contents.getvalue()))
    finally:
        page_contents.close()

    return hwid_info_dict
