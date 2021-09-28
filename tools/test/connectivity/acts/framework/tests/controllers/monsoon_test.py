#!/usr/bin/env python3
#
#   Copyright 2020 - The Android Open Source Project
#
#   Licensed under the Apache License, Version 2.0 (the "License");
#   you may not use this file except in compliance with the License.
#   You may obtain a copy of the License at
#
#       http://www.apache.org/licenses/LICENSE-2.0
#
#   Unless required by applicable law or agreed to in writing, software
#   distributed under the License is distributed on an "AS IS" BASIS,
#   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#   See the License for the specific language governing permissions and
#   limitations under the License.

import unittest

import mock

from acts.controllers import monsoon
from acts.controllers.monsoon_lib.api.hvpm.monsoon import Monsoon as HvpmMonsoon
from acts.controllers.monsoon_lib.api.lvpm_stock.monsoon import Monsoon as LvpmStockMonsoon


@mock.patch('acts.controllers.monsoon_lib.api.lvpm_stock.monsoon.MonsoonProxy')
@mock.patch('acts.controllers.monsoon_lib.api.hvpm.monsoon.HVPM')
class MonsoonTest(unittest.TestCase):
    """Tests the acts.controllers.iperf_client module functions."""
    def test_create_can_create_lvpm_from_id_only(self, *_):
        monsoons = monsoon.create([12345])
        self.assertIsInstance(monsoons[0], LvpmStockMonsoon)

    def test_create_can_create_lvpm_from_dict(self, *_):
        monsoons = monsoon.create([{'type': 'LvpmStockMonsoon', 'serial': 10}])
        self.assertIsInstance(monsoons[0], LvpmStockMonsoon)
        self.assertEqual(monsoons[0].serial, 10)

    def test_create_can_create_hvpm_from_id_only(self, *_):
        monsoons = monsoon.create([23456])
        self.assertIsInstance(monsoons[0], HvpmMonsoon)

    def test_create_can_create_hvpm_from_dict(self, *_):
        monsoons = monsoon.create([{'type': 'HvpmMonsoon', 'serial': 10}])
        self.assertIsInstance(monsoons[0], HvpmMonsoon)
        self.assertEqual(monsoons[0].serial, 10)

    def test_raises_error_if_monsoon_type_is_unknown(self, *_):
        with self.assertRaises(ValueError):
            monsoon.create([{'type': 'UNKNOWN', 'serial': 10}])

    def test_raises_error_if_monsoon_serial_not_provided(self, *_):
        with self.assertRaises(ValueError):
            monsoon.create([{'type': 'LvpmStockMonsoon'}])


if __name__ == '__main__':
    unittest.main()
