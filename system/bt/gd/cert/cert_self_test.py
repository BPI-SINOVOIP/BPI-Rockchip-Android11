#!/usr/bin/env python3
#
#   Copyright 2019 - The Android Open Source Project
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

import logging
import time

from mobly import asserts
from datetime import datetime, timedelta
from cert.gd_base_test_facade_only import GdFacadeOnlyBaseTestClass
from cert.event_callback_stream import EventCallbackStream
from cert.event_asserts import EventAsserts

# Test packet nesting
from bluetooth_packets_python3 import hci_packets
from bluetooth_packets_python3 import l2cap_packets


class BogusProto:

    class BogusType:

        def __init__(self):
            self.name = "BogusProto"
            self.is_extension = False
            self.cpp_type = False

        def type(self):
            return 'BogusRpc'

        def label(self):
            return "label"

    class BogusDescriptor:

        def __init__(self, name):
            self.full_name = name

    def __init__(self, value):
        self.value_ = value
        self.DESCRIPTOR = BogusProto.BogusDescriptor(str(value))

    def __str__(self):
        return "BogusRpc value = " + str(self.value_)

    def ListFields(self):
        for field in [BogusProto.BogusType()]:
            yield [field, self.value_]


class FetchEvents:

    def __init__(self, events, delay_ms):
        self.events_ = events
        self.sleep_time_ = (delay_ms * 1.0) / 1000
        self.index_ = 0
        self.done_ = False
        self.then_ = datetime.now()

    def __iter__(self):
        for event in self.events_:
            time.sleep(self.sleep_time_)
            if self.done_:
                return
            logging.debug("yielding %d" % event)
            yield BogusProto(event)

    def done(self):
        return self.done_

    def cancel(self):
        logging.debug("cancel")
        self.done_ = True
        return None


class CertSelfTest(GdFacadeOnlyBaseTestClass):

    def setup_test(self):
        return True

    def teardown_test(self):
        return True

    def test_assert_none_passes(self):
        with EventCallbackStream(FetchEvents(events=[],
                                             delay_ms=50)) as event_stream:
            event_asserts = EventAsserts(event_stream)
            event_asserts.assert_none(timeout=timedelta(milliseconds=10))

    def test_assert_none_passes_after_one_second(self):
        with EventCallbackStream(FetchEvents([1],
                                             delay_ms=1500)) as event_stream:
            event_asserts = EventAsserts(event_stream)
            event_asserts.assert_none(timeout=timedelta(seconds=1.0))

    def test_assert_none_fails(self):
        try:
            with EventCallbackStream(FetchEvents(events=[17],
                                                 delay_ms=50)) as event_stream:
                event_asserts = EventAsserts(event_stream)
                event_asserts.assert_none(timeout=timedelta(seconds=1))
        except Exception as e:
            logging.debug(e)
            return True  # Failed as expected
        return False

    def test_assert_none_matching_passes(self):
        with EventCallbackStream(FetchEvents(events=[1, 2, 3],
                                             delay_ms=50)) as event_stream:
            event_asserts = EventAsserts(event_stream)
            event_asserts.assert_none_matching(
                lambda data: data.value_ == 4, timeout=timedelta(seconds=0.15))

    def test_assert_none_matching_passes_after_1_second(self):
        with EventCallbackStream(
                FetchEvents(events=[1, 2, 3, 4], delay_ms=400)) as event_stream:
            event_asserts = EventAsserts(event_stream)
            event_asserts.assert_none_matching(
                lambda data: data.value_ == 4, timeout=timedelta(seconds=1))

    def test_assert_none_matching_fails(self):
        try:
            with EventCallbackStream(
                    FetchEvents(events=[1, 2, 3], delay_ms=50)) as event_stream:
                event_asserts = EventAsserts(event_stream)
                event_asserts.assert_none_matching(
                    lambda data: data.value_ == 2, timeout=timedelta(seconds=1))
        except Exception as e:
            logging.debug(e)
            return True  # Failed as expected
        return False

    def test_assert_occurs_at_least_passes(self):
        with EventCallbackStream(
                FetchEvents(events=[1, 2, 3, 1, 2, 3],
                            delay_ms=40)) as event_stream:
            event_asserts = EventAsserts(event_stream)
            event_asserts.assert_event_occurs(
                lambda data: data.value_ == 1,
                timeout=timedelta(milliseconds=300),
                at_least_times=2)

    def test_assert_occurs_passes(self):
        with EventCallbackStream(FetchEvents(events=[1, 2, 3],
                                             delay_ms=50)) as event_stream:
            event_asserts = EventAsserts(event_stream)
            event_asserts.assert_event_occurs(
                lambda data: data.value_ == 1, timeout=timedelta(seconds=1))

    def test_assert_occurs_fails(self):
        try:
            with EventCallbackStream(
                    FetchEvents(events=[1, 2, 3], delay_ms=50)) as event_stream:
                event_asserts = EventAsserts(event_stream)
                event_asserts.assert_event_occurs(
                    lambda data: data.value_ == 4, timeout=timedelta(seconds=1))
        except Exception as e:
            logging.debug(e)
            return True  # Failed as expected
        return False

    def test_assert_occurs_at_most_passes(self):
        with EventCallbackStream(FetchEvents(events=[1, 2, 3, 4],
                                             delay_ms=50)) as event_stream:
            event_asserts = EventAsserts(event_stream)
            event_asserts.assert_event_occurs_at_most(
                lambda data: data.value_ < 4,
                timeout=timedelta(seconds=1),
                at_most_times=3)

    def test_assert_occurs_at_most_fails(self):
        try:
            with EventCallbackStream(
                    FetchEvents(events=[1, 2, 3, 4],
                                delay_ms=50)) as event_stream:
                event_asserts = EventAsserts(event_stream)
                event_asserts.assert_event_occurs_at_most(
                    lambda data: data.value_ > 1,
                    timeout=timedelta(seconds=1),
                    at_most_times=2)
        except Exception as e:
            logging.debug(e)
            return True  # Failed as expected
        return False

    def test_skip_a_test(self):
        asserts.skip("Skipping this test because it's blocked by b/xyz")
        assert False

    def test_nested_packets(self):
        handle = 123
        inside = hci_packets.ReadScanEnableBuilder()
        logging.debug(inside.Serialize())
        logging.debug("building outside")
        outside = hci_packets.AclPacketBuilder(
            handle,
            hci_packets.PacketBoundaryFlag.FIRST_NON_AUTOMATICALLY_FLUSHABLE,
            hci_packets.BroadcastFlag.POINT_TO_POINT, inside)
        logging.debug(outside.Serialize())
        logging.debug("Done!")

    def test_l2cap_config_options(self):
        mtu_opt = l2cap_packets.MtuConfigurationOption()
        mtu_opt.mtu = 123
        fcs_opt = l2cap_packets.FrameCheckSequenceOption()
        fcs_opt.fcs_type = l2cap_packets.FcsType.DEFAULT
        request = l2cap_packets.ConfigurationRequestBuilder(
            0x1d,  # Command ID
            0xc1d,  # Channel ID
            l2cap_packets.Continuation.END,
            [mtu_opt, fcs_opt])
        request.Serialize()
        handle = 123
        wrapped = hci_packets.AclPacketBuilder(
            handle,
            hci_packets.PacketBoundaryFlag.FIRST_NON_AUTOMATICALLY_FLUSHABLE,
            hci_packets.BroadcastFlag.POINT_TO_POINT, request)
        asserts.assert_true(
            len(wrapped.Serialize()) == 16, "Packet serialized incorrectly")
