# Copyright (c) 2018 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from autotest_lib.client.bin import test
from autotest_lib.client.common_lib import error
from autotest_lib.client.common_lib.cros import chrome
from autotest_lib.client.cros import constants as cros_constants
from autotest_lib.client.cros.multimedia import local_facade_factory


class display_DisplayContainEdid(test.test):
    """
    Verifies that display information returned from Chrome OS specific
    chrome.system.display API contain EDID information.
    """
    version = 1

    def run_once(self):
        with chrome.Chrome(
                extension_paths = [cros_constants.DISPLAY_TEST_EXTENSION],
                autotest_ext=True) as cr:
            display_facade = local_facade_factory.LocalFacadeFactory(
                cr).create_display_facade()

            displays = display_facade.get_display_info()

            if len(displays) == 0:
                raise error.TestError('No displays connected!')

            edid = displays[0].edid

            no_manufacturer = edid.manufacturer_id == None
            no_year = edid.year_of_manufacture == None
            no_product = edid.product_id == None

            if no_manufacturer or no_year or no_product:
                raise error.TestError(
                    'Incorrect edid, manufacturer: {}, year: {}, product: {}'.format(
                        edid.manufacturer_id,
                        edid.year_of_manufacture,
                        edid.product_id))
