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

import collections
import os
import tempfile

from acts.test_utils.instrumentation.proto.gen import instrumentation_data_pb2

DEFAULT_INST_LOG_DIR = 'instrument-logs'

START_TIMESTAMP = 'start'
END_TIMESTAMP = 'end'


class ProtoParserError(Exception):
    """Class for exceptions raised by the proto parser."""


def pull_proto(ad, dest_dir, source_path=None):
    """Pull latest instrumentation result proto from device.

    Args:
        ad: AndroidDevice object
        dest_dir: Directory on the host where the proto will be sent
        source_path: Path on the device where the proto is generated. If None,
            pull the latest proto from DEFAULT_INST_PROTO_DIR.

    Returns: Path to the retrieved proto file
    """
    if source_path:
        filename = os.path.basename(source_path)
    else:
        default_full_proto_dir = os.path.join(
            ad.external_storage_path, DEFAULT_INST_LOG_DIR)
        filename = ad.adb.shell('ls %s -t | head -n1' % default_full_proto_dir)
        if not filename:
            raise ProtoParserError(
                'No instrumentation result protos found at default location.')
        source_path = os.path.join(default_full_proto_dir, filename)
    ad.pull_files(source_path, dest_dir)
    dest_path = os.path.join(dest_dir, filename)
    if not os.path.exists(dest_path):
        raise ProtoParserError(
            'Failed to pull instrumentation result proto: %s -> %s'
            % (source_path, dest_path))
    return dest_path


def get_session_from_local_file(proto_file):
    """Get a instrumentation_data_pb2.Session object from a proto file on the
    host.

    Args:
        proto_file: Path to the proto file (on host)

    Returns: A instrumentation_data_pb2.Session
    """
    with open(proto_file, 'rb') as f:
        return instrumentation_data_pb2.Session.FromString(f.read())


def get_session_from_device(ad, proto_file=None):
    """Get a instrumentation_data_pb2.Session object from a proto file on
    device.

    Args:
        ad: AndroidDevice object
        proto_file: Path to the proto file (on device). If None, defaults to
            latest proto from DEFAULT_INST_PROTO_DIR.

    Returns: A instrumentation_data_pb2.Session
    """
    with tempfile.TemporaryDirectory() as tmp_dir:
        pulled_proto = pull_proto(ad, tmp_dir, proto_file)
        return get_session_from_local_file(pulled_proto)


def get_test_timestamps(session):
    """Parse an instrumentation_data_pb2.Session to get the timestamps for each
    test.

    Args:
        session: an instrumentation_data.Session object

    Returns: a dict in the format
        {
            <test name> : (<begin_time>, <end_time>),
            ...
        }
    """
    timestamps = collections.defaultdict(dict)
    for test_status in session.test_status:
        entries = test_status.results.entries
        # Timestamp entries have the key 'timestamp-message'
        if any(entry.key == 'timestamps-message' for entry in entries):
            test_name = None
            timestamp = None
            timestamp_type = None
            for entry in entries:
                if entry.key == 'test':
                    test_name = entry.value_string
                if entry.key == 'timestamp':
                    timestamp = entry.value_long
                if entry.key == 'start-timestamp':
                    timestamp_type = START_TIMESTAMP
                if entry.key == 'end-timestamp':
                    timestamp_type = END_TIMESTAMP
            if test_name and timestamp and timestamp_type:
                timestamps[test_name][timestamp_type] = timestamp
    return timestamps
