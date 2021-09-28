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
import unittest

import mock
from acts.test_utils.instrumentation import instrumentation_proto_parser \
    as parser
from acts.test_utils.instrumentation.instrumentation_proto_parser import \
    ProtoParserError
from acts.test_utils.instrumentation.proto.gen import instrumentation_data_pb2


DEST_DIR = 'dest/proto_dir'
SOURCE_PATH = 'source/proto/protofile'
SAMPLE_PROTO = 'data/sample.instrumentation_data_proto'
SAMPLE_TIMESTAMP_PROTO = 'data/sample_timestamp.instrumentation_data_proto'


class InstrumentationProtoParserTest(unittest.TestCase):
    """Unit tests for instrumentation proto parser."""

    def setUp(self):
        self.ad = mock.MagicMock()
        self.ad.external_storage_path = ''

    @mock.patch('os.path.exists', return_value=True)
    def test_pull_proto_returns_correct_path_given_source(self, *_):
        self.assertEqual(parser.pull_proto(self.ad, DEST_DIR, SOURCE_PATH),
                         'dest/proto_dir/protofile')

    @mock.patch('os.path.exists', return_value=True)
    def test_pull_proto_returns_correct_path_from_default_location(self, *_):
        self.ad.adb.shell.return_value = 'default'
        self.assertEqual(parser.pull_proto(self.ad, DEST_DIR),
                         'dest/proto_dir/default')

    def test_pull_proto_fails_if_no_default_proto_found(self, *_):
        self.ad.adb.shell.return_value = None
        with self.assertRaisesRegex(
                ProtoParserError, 'No instrumentation result'):
            parser.pull_proto(self.ad, DEST_DIR)

    @mock.patch('os.path.exists', return_value=False)
    def test_pull_proto_fails_if_adb_pull_fails(self, *_):
        with self.assertRaisesRegex(ProtoParserError, 'Failed to pull'):
            parser.pull_proto(self.ad, DEST_DIR, SOURCE_PATH)

    def test_parser_converts_valid_proto(self):
        proto_file = os.path.join(os.path.dirname(__file__), SAMPLE_PROTO)
        self.assertIsInstance(parser.get_session_from_local_file(proto_file),
                              instrumentation_data_pb2.Session)

    def test_get_test_timestamps(self):
        proto_file = os.path.join(os.path.dirname(__file__),
                                  SAMPLE_TIMESTAMP_PROTO)
        session = parser.get_session_from_local_file(proto_file)
        timestamps = parser.get_test_timestamps(session)
        self.assertEqual(
            timestamps['partialWakelock'][parser.START_TIMESTAMP],
            1567029917802)
        self.assertEqual(
            timestamps['partialWakelock'][parser.END_TIMESTAMP], 1567029932879)


if __name__ == '__main__':
    unittest.main()
