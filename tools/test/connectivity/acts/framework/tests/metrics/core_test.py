#!/usr/bin/env python3
#
#   Copyright 2018 - The Android Open Source Project
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

from functools import partial
from mock import call
from mock import Mock
from mock import patch
import unittest
from unittest import TestCase
from acts.metrics.core import MetricPublisher
from acts.metrics.core import ProtoMetric
from acts.metrics.core import ProtoMetricPublisher

PARSE_PROTO_TO_ASCII = 'acts.metrics.core.parse_proto_to_ascii'
TO_DESCRIPTOR_PROTO = 'acts.metrics.core.to_descriptor_proto'
DUMP_STRING_TO_FILE = 'acts.metrics.core.dump_string_to_file'
MAKEDIRS = 'acts.metrics.core.os.makedirs'

class ProtoMetricTest(TestCase):
    """Unit tests for the ProtoMetric class."""

    TEST_NAME = 'metric name'

    def setUp(self):
        self.data = Mock()
        pass

    def test_default_init_attributes(self):
        metric = ProtoMetric(name = self.TEST_NAME, data=self.data)
        self.assertEqual(metric.name, self.TEST_NAME)
        self.assertEqual(metric.data, self.data)

    def test_no_data_init_raises_error(self):
        self.assertRaises(ValueError, lambda: ProtoMetric(name=self.TEST_NAME))

    def test_get_binary(self):
        metric = ProtoMetric(name=self.TEST_NAME, data=self.data)
        self.data.SerializeToString = Mock()

        metric.get_binary()

        self.data.SerializeToString.assert_called_once_with()

    @patch(PARSE_PROTO_TO_ASCII)
    def test_get_ascii(self, parse_proto_to_ascii):
        metric = ProtoMetric(name=self.TEST_NAME, data=self.data)

        metric.get_ascii()

        parse_proto_to_ascii.assert_called_once_with(self.data)

    @patch(TO_DESCRIPTOR_PROTO)
    def test_get_descriptor_binary(self, to_descriptor_proto):
        metric = ProtoMetric(name=self.TEST_NAME, data=self.data)
        descriptor_proto = Mock()
        descriptor_proto.SerializeToString = Mock()
        to_descriptor_proto.return_value = descriptor_proto

        metric.get_descriptor_binary()

        to_descriptor_proto.assert_called_once_with(self.data)
        descriptor_proto.SerializeToString.assert_called_once_with()

    @patch(PARSE_PROTO_TO_ASCII)
    @patch(TO_DESCRIPTOR_PROTO)
    def test_get_descriptor_ascii(self, to_desc_proto, parse_proto):
        metric = ProtoMetric(name=self.TEST_NAME, data=self.data)
        descriptor_proto = Mock()
        to_desc_proto.return_value = descriptor_proto

        metric.get_descriptor_ascii()

        to_desc_proto.assert_called_once_with(self.data)
        parse_proto.assert_called_once_with(descriptor_proto)


class MetricPublisherTest(TestCase):
    """Unit tests for the MetricPublisher class."""

    def test_default_init_attributes(self):
        context = Mock()
        publisher = MetricPublisher(context)

        self.assertEqual(publisher.context, context)

    def test_none_init_raises(self):
        self.assertRaises(ValueError, lambda: MetricPublisher(None))

    def test_publish_not_implemented(self):
        context = Mock()
        metrics = Mock()
        publisher = MetricPublisher(context)

        self.assertRaises(NotImplementedError, lambda: publisher.publish(metrics))


class ProtoMetricPublisherTest(TestCase):
    """Unit tests for the ProtoMetricPublisher class"""

    def test_init_attributes(self):
        context = Mock()
        context.get_full_output_path.return_value = 'output/path'
        publishes_binary = Mock()
        publishes_ascii = Mock()
        publishes_descriptor_binary = Mock()
        publishes_descriptor_ascii = Mock()

        publisher = ProtoMetricPublisher(
            context,
            publishes_binary=publishes_binary,
            publishes_ascii=publishes_ascii,
            publishes_descriptor_binary=publishes_descriptor_binary,
            publishes_descriptor_ascii=publishes_descriptor_ascii)

        self.assertEqual(publisher.context, context)
        self.assertEqual(publisher.publishes_binary, publishes_binary)
        self.assertEqual(publisher.publishes_ascii, publishes_ascii)
        self.assertEqual(publisher.publishes_descriptor_binary,
                         publishes_descriptor_binary)
        self.assertEqual(publisher.publishes_descriptor_ascii,
                         publishes_descriptor_ascii)

    def test_default_init_publishes_everything(self):
        context = Mock()
        context.get_full_output_path.return_value = 'output/path'
        publisher = ProtoMetricPublisher(context)

        self.assertEqual(publisher.publishes_binary, True)
        self.assertEqual(publisher.publishes_ascii, True)
        self.assertEqual(publisher.publishes_descriptor_binary, True)
        self.assertEqual(publisher.publishes_descriptor_ascii, True)

    def test_get_output_path(self):
        context = Mock()
        context.get_full_output_path.return_value = 'output/path'

        publisher = ProtoMetricPublisher(context)
        output_path = publisher.get_output_path()

        metrics_output_path = 'output/path/metrics'
        self.assertEqual(output_path, metrics_output_path)
        context.get_full_output_path.assert_called_once_with()

    @patch(MAKEDIRS)
    @patch(DUMP_STRING_TO_FILE)
    def test_publish_all_disabled(self, dump_string_to_file, makedirs):
        del makedirs
        context = Mock()
        context.get_full_output_path.return_value = 'output/path'
        metrics = [Mock()]
        publisher = ProtoMetricPublisher(
            context,
            publishes_binary=False,
            publishes_ascii=False,
            publishes_descriptor_binary=False,
            publishes_descriptor_ascii=False)

        publisher.publish(metrics)

        assert not dump_string_to_file.called

    @patch(MAKEDIRS)
    @patch(DUMP_STRING_TO_FILE)
    def test_publish_makes_dirs(self, dump_string_to_file, makedirs):
        del dump_string_to_file
        context = Mock()
        context.get_full_output_path.return_value = 'output/path'
        metrics = [Mock()]
        publisher = ProtoMetricPublisher(
            context,
            publishes_binary=False,
            publishes_ascii=False,
            publishes_descriptor_binary=False,
            publishes_descriptor_ascii=False)

        publisher.publish(metrics)

        metrics_output_path= 'output/path/metrics'
        makedirs.assert_called_once_with(metrics_output_path, exist_ok=True)

    @patch(MAKEDIRS)
    @patch(DUMP_STRING_TO_FILE)
    def test_publish_binary(self, dump_string_to_file, makedirs):
        del makedirs
        context = Mock()
        context.get_full_output_path.return_value = 'output/path'
        metric = Mock()
        binary = Mock()
        metric.get_binary = Mock(return_value=binary)
        metric.name = 'metric'
        publisher = ProtoMetricPublisher(
            context,
            publishes_binary=True,
            publishes_ascii=False,
            publishes_descriptor_binary=False,
            publishes_descriptor_ascii=False)

        publisher.publish([metric])

        file_path = ('output/path/metrics/metric.' +
                    ProtoMetricPublisher.BINARY_EXTENSION)
        dump_string_to_file.assert_called_once_with(
            binary,
            file_path,
            mode='wb')

    @patch(MAKEDIRS)
    @patch(DUMP_STRING_TO_FILE)
    def test_publish_ascii(self, dump_string_to_file, makedirs):
        del makedirs
        context = Mock()
        context.get_full_output_path.return_value = 'output/path'
        metric = Mock()
        ascii = Mock()
        metric.get_ascii = Mock(return_value=ascii)
        metric.name = 'metric'
        publisher = ProtoMetricPublisher(
            context,
            publishes_binary=False,
            publishes_ascii=True,
            publishes_descriptor_binary=False,
            publishes_descriptor_ascii=False)

        publisher.publish([metric])

        file_path = ('output/path/metrics/metric.' +
                    ProtoMetricPublisher.ASCII_EXTENSION)
        dump_string_to_file.assert_called_once_with(
            ascii,
            file_path)

    @patch(MAKEDIRS)
    @patch(DUMP_STRING_TO_FILE)
    def test_publish_descriptor_binary(self, dump_string_to_file, makedirs):
        del makedirs
        context = Mock()
        context.get_full_output_path.return_value = 'output/path'
        metric = Mock()
        descriptor_binary = Mock()
        metric.get_descriptor_binary = Mock(return_value=descriptor_binary)
        metric.name = 'metric'
        publisher = ProtoMetricPublisher(
            context,
            publishes_binary=False,
            publishes_ascii=False,
            publishes_descriptor_binary=True,
            publishes_descriptor_ascii=False)

        publisher.publish([metric])

        file_path = ('output/path/metrics/metric.' +
                    ProtoMetricPublisher.BINARY_DESCRIPTOR_EXTENSION)
        dump_string_to_file.assert_called_once_with(
            descriptor_binary,
            file_path,
            mode='wb')

    @patch(MAKEDIRS)
    @patch(DUMP_STRING_TO_FILE)
    def test_publish_ascii_descriptor(self, dump_string_to_file, makedirs):
        del makedirs
        context = Mock()
        context.get_full_output_path.return_value = 'output/path'
        metric = Mock()
        descriptor_ascii = Mock()
        metric.get_ascii = Mock(return_value=descriptor_ascii)
        metric.name = 'metric'
        publisher = ProtoMetricPublisher(
            context,
            publishes_binary=False,
            publishes_ascii=True,
            publishes_descriptor_binary=False,
            publishes_descriptor_ascii=False)

        publisher.publish([metric])

        file_path = ('output/path/metrics/metric.' +
                    ProtoMetricPublisher.ASCII_EXTENSION)
        dump_string_to_file.assert_called_once_with(
            descriptor_ascii,
            file_path)

    @patch(MAKEDIRS)
    @patch(DUMP_STRING_TO_FILE)
    def test_publish_multiple_binaries(self, dump_string_to_file, makedirs):
        del makedirs
        context = Mock()
        context.get_full_output_path.return_value = 'output/path'
        metric_1 = Mock()
        metric_2 = Mock()
        binary_1 = Mock()
        binary_2 = Mock()
        metric_1.get_binary = Mock(return_value=binary_1)
        metric_2.get_binary = Mock(return_value=binary_2)
        metric_1.name = 'metric_1'
        metric_2.name = 'metric_2'
        publisher = ProtoMetricPublisher(
            context,
            publishes_binary=True,
            publishes_ascii=False,
            publishes_descriptor_binary=False,
            publishes_descriptor_ascii=False)

        publisher.publish([metric_1, metric_2])

        file_path_1 = ('output/path/metrics/metric_1.' +
                      ProtoMetricPublisher.BINARY_EXTENSION)
        file_path_2 = ('output/path/metrics/metric_2.' +
                      ProtoMetricPublisher.BINARY_EXTENSION)

        call_1 = call(binary_1, file_path_1, mode='wb')
        call_2 = call(binary_2, file_path_2, mode='wb')
        dump_string_to_file.assert_has_calls([call_1, call_2])


if __name__ == '__main__':
    unittest.main()
