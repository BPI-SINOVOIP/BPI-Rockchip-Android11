# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging

from autotest_lib.client.bin import test
from autotest_lib.client.common_lib import error
from autotest_lib.client.common_lib.cros.network import iw_runner

class network_WiFiHECaps(test.test):
    """Test the specified HE client capabilities."""
    version = 1

    def run_once(self, phy=None, features=None):
        """
        Check for support of the features specified in control file.

        Features are passed in as a list of lists containing string constants.
        The test will pass if the DUT supports at least one string in each
        list. Essentially, all the elements in the list are joined with AND
        while all the elements of a list are joined with OR.

        @param phy string name of wifi phy to use, or None to allow the
                test to choose.
        @param features list of lists of string constants from iw_runner
                specifying the HE features to check on the DUT.

        """
        iw = iw_runner.IwRunner()
        if not phy:
            phys = iw.list_phys()
            if not phys:
                raise error.TestError('No valid WiFi phy found')
            phy = phys[0].name
        if not iw.he_supported():
            raise error.TestNAError('HE not supported by DUT')

        phy_info = iw.get_info()
        if not phy_info:
            raise error.TestError('Could not get phy info using iw_runner')

        if not features:
            features = []

        featurelist = [f for inner_list in features for f in inner_list]
        is_supported = {f : False for f in featurelist}
        values = {}

        for line in phy_info.splitlines():
            line = line.strip()
            for f in featurelist:
                if not is_supported[f] and f in line:
                    is_supported[f] = True
                    l = line.split(':', 1)
                    if len(l) > 1:
                        values[f] = l[1].strip()
                    break

        supported = ['These features are supported by the DUT:']
        not_supported = ['These features are NOT supported by the DUT:']
        for f in featurelist:
            if is_supported[f]:
                if values.get(f, None):
                    f += ('; has value %s' % values[f])
                supported.append(f)
            else:
                not_supported.append(f)
        logging.info(' '.join(supported))
        logging.info(' '.join(not_supported))

        for inner_list in features:
            list_passed = False
            for f in inner_list:
                if is_supported[f]:
                    list_passed = True
                    break
            if not list_passed:
                raise error.TestError('Test failed because none of %r are '
                                      'supported by the DUT.' % inner_list)
