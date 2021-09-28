# Copyright 2013 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

import subprocess
import unittest


class Error(Exception):
    pass


class SocketError(Error):

    def __init__(self, device_id, message):
        """Exception raised for socket errors.

        Args:
            device_id (str): device id
            message (str): explanation of the error
        """
        Error.__init__(self)
        self.message = message
        locale = self.get_device_locale(device_id)
        if locale != "en-US":
            print "Unsupported default language %s" % locale
            print "Please set the default language to English (United States)"
            print "in Settings > Language & input > Languages\n"

    def get_device_locale(self, device_id):
        """Return the default locale of a given device.

        Args:
            device_id (str): device id

        Returns:
             str: Device locale.
        """
        locale_property = "persist.sys.locale"

        com = ("adb -s %s shell getprop %s" % (device_id, locale_property))
        proc = subprocess.Popen(com.split(), stdout=subprocess.PIPE)
        output, error = proc.communicate()
        assert error is None

        return output


class __UnitTest(unittest.TestCase):
    """Run a suite of unit tests on this module.
    """

if __name__ == "__main__":
    unittest.main()

