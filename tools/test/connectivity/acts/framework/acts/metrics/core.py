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

import os

from acts.libs.proto.proto_utils import parse_proto_to_ascii
from acts.libs.proto.proto_utils import to_descriptor_proto
from acts.utils import dump_string_to_file


class ProtoMetric(object):
    """A wrapper around a protobuf containing metrics data.

    This is the primary data structure used as the output of MetricLoggers. It
    is generally intended to be used as-is as a simple wrapper structure, but
    can be extended to provide self-populating APIs.

    Attributes:
        name: The name of the metric.
        data: The data of the metric.
    """

    def __init__(self, name=None, data=None):
        """Initializes a metric with given name and data.

        Args:
            name: The name of the metric. Used in identifiers such as filename.
            data: The data of the metric. Should be a protobuf object.
        """
        self.name = name
        if not data:
            raise ValueError("Parameter 'data' cannot be None.")
        self.data = data

    def get_binary(self):
        """Gets the binary representation of the protobuf data."""
        return self.data.SerializeToString()

    def get_ascii(self):
        """Gets the ascii representation of the protobuf data."""
        return parse_proto_to_ascii(self.data)

    def get_descriptor_binary(self):
        """Gets the binary representation of the descriptor protobuf."""
        return to_descriptor_proto(self.data).SerializeToString()

    def get_descriptor_ascii(self):
        """Gets the ascii representation of the descriptor protobuf."""
        return parse_proto_to_ascii(to_descriptor_proto(self.data))


class MetricPublisher(object):
    """A helper object for publishing metric data.

    This is a base class intended to be implemented to accommodate specific
    metric types and output formats.

    Attributes:
        context: The context in which the metrics are being published.
    """

    def __init__(self, context):
        """Initializes a publisher for the give context.

        Args:
            context: The context in which the metrics are being published.
                     Typically matches that of a containing MetricLogger.
        """
        if not context:
            raise ValueError("Parameter 'context' cannot be None.")
        self.context = context

    def publish(self, metrics):
        """Publishes a list of metrics.

        Args:
            metrics: A list of metrics to publish. The requirements on the
            object type of these metrics is up to the implementing class.
        """
        raise NotImplementedError()


class ProtoMetricPublisher(MetricPublisher):
    """A MetricPublisher that will publish ProtoMetrics to files.

    Attributes:
        publishes_binary: Whether to publish the binary proto.
        publishes_ascii: Whether to publish the ascii proto.
        publishes_descriptor_binary: Whether to publish the binary descriptor.
        publishes_descriptor_ascii: Whether to publish the ascii descriptor.
    """

    ASCII_EXTENSION = 'proto.data'
    BINARY_EXTENSION = 'proto.bin'
    ASCII_DESCRIPTOR_EXTENSION = 'proto.desc'
    BINARY_DESCRIPTOR_EXTENSION = 'proto.desc.bin'

    METRICS_DIR = 'metrics'

    def __init__(self,
                 context,
                 publishes_binary=True,
                 publishes_ascii=True,
                 publishes_descriptor_binary=True,
                 publishes_descriptor_ascii=True):
        """Initializes a ProtoMetricPublisher.

        Args:
            context: The context in which the metrics are being published.
            publishes_binary: Whether to publish the binary proto.
            publishes_ascii: Whether to publish the ascii proto.
            publishes_descriptor_binary: Whether to publish the binary
                                         descriptor.
            publishes_descriptor_ascii: Whether to publish the ascii
                                        descriptor.
        """
        super().__init__(context)
        self.publishes_binary = publishes_binary
        self.publishes_ascii = publishes_ascii
        self.publishes_descriptor_binary = publishes_descriptor_binary
        self.publishes_descriptor_ascii = publishes_descriptor_ascii

    def get_output_path(self):
        """Gets the output directory path of the metrics."""
        return os.path.join(self.context.get_full_output_path(),
                            self.METRICS_DIR)

    def publish(self, metrics):
        """Publishes the given list of metrics.

        Based on the publish_* attributes of this class, this will publish
        the varying data formats provided by the metric object. Data is written
        to files on disk named according to the name of the metric.

        Args:
            metrics: The list metric to publish. Assumed to be a list of
                     ProtoMetric objects.
        """
        if isinstance(metrics, list):
            for metric in metrics:
                self._publish_single(metric)
        else:
            self._publish_single(metrics)

    def _publish_single(self, metric):
        """Publishes a single metric.

        Based on the publish_* attributes of this class, this will publish
        the varying data formats provided by the metric object. Data is written
        to files on disk named according to the name of the metric.

        Args:
            metric: The metric to publish. Assumed to be a ProtoMetric object.
        """
        output_path = self.get_output_path()

        os.makedirs(output_path, exist_ok=True)

        if self.publishes_binary:
            self.write_binary(metric, output_path)
        if self.publishes_ascii:
            self.write_ascii(metric, output_path)
        if self.publishes_descriptor_binary:
            self.write_descriptor_binary(metric, output_path)
        if self.publishes_descriptor_ascii:
            self.write_descriptor_ascii(metric, output_path)

    def write_binary(self, metric, output_path):
        """Writes the binary format of the protobuf to file.

        Args:
            metric: The metric to write.
            output_path: The output directory path to write the file to.
        """
        filename = self._get_output_file(
            output_path, metric.name, self.BINARY_EXTENSION)
        dump_string_to_file(metric.get_binary(), filename, mode='wb')

    def write_ascii(self, metric, output_path):
        """Writes the ascii format of the protobuf to file.

        Args:
            metric: The metric to write.
            output_path: The output directory path to write the file to.
        """
        filename = self._get_output_file(
            output_path, metric.name, self.ASCII_EXTENSION)
        dump_string_to_file(metric.get_ascii(), filename)

    def write_descriptor_binary(self, metric, output_path):
        """Writes the binary format of the protobuf descriptor to file.

        Args:
            metric: The metric to write.
            output_path: The output directory path to write the file to.
        """
        filename = self._get_output_file(
            output_path, metric.name, self.BINARY_DESCRIPTOR_EXTENSION)
        dump_string_to_file(metric.get_descriptor_binary(), filename, mode='wb')

    def write_descriptor_ascii(self, metric, output_path):
        """Writes the ascii format of the protobuf descriptor to file.

        Args:
            metric: The metric to write.
            output_path: The output directory path to write the file to.
        """
        filename = self._get_output_file(
            output_path, metric.name, self.ASCII_DESCRIPTOR_EXTENSION)
        dump_string_to_file(metric.get_descriptor_ascii(), filename)

    def _get_output_file(self, output_path, filename, extension):
        """Gets the full output file path.

        Args:
            output_path: The output directory path.
            filename: The base filename of the file.
            extension: The extension of the file, without the leading '.'

        Returns:
            The full file path.
        """
        return os.path.join(output_path, "%s.%s" % (filename, extension))
