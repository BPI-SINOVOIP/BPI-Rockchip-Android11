#!/usr/bin/env python3
#
# Copyright 2016 - The Android Open Source Project
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

from mobly.asserts import *


# Have an instance of unittest.TestCase so we could reuse some logic from
# python's own unittest.
# _ProxyTest is required because py2 does not allow instantiating
# unittest.TestCase directly.
class _ProxyTest(unittest.TestCase):
    def runTest(self):
        pass


_pyunit_proxy = _ProxyTest()


def assert_almost_equal(first,
                        second,
                        places=7,
                        msg=None,
                        delta=None,
                        extras=None):
    """
    Assert FIRST to be within +/- DELTA to SECOND, otherwise fail the
    test.
    :param first: The first argument, LHS
    :param second: The second argument, RHS
    :param places: For floating points, how many decimal places to look into
    :param msg: Message to display on failure
    :param delta: The +/- first and second could be apart from each other
    :param extras: Extra object passed to test failure handler
    :return:
    """
    my_msg = None
    try:
        if delta:
            _pyunit_proxy.assertAlmostEqual(
                first, second, msg=msg, delta=delta)
        else:
            _pyunit_proxy.assertAlmostEqual(
                first, second, places=places, msg=msg)
    except Exception as e:
        my_msg = str(e)
        if msg:
            my_msg = "%s %s" % (my_msg, msg)
    # This is a hack to remove the stacktrace produced by the above exception.
    if my_msg is not None:
        fail(my_msg, extras=extras)
