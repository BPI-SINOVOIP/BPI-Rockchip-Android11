# /usr/bin/env python3
#
# Copyright (C) 2019 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License"); you may not
# use this file except in compliance with the License. You may obtain a copy of
# the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
# WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
# License for the specific language governing permissions and limitations under
# the License.

import base64
import os
import time

from acts.metrics.core import ProtoMetric
from acts.metrics.logger import MetricLogger
from acts.test_utils.tel.loggers.protos.telephony_metric_pb2 import TelephonyVoiceTestResult

# Initializes the path to the protobuf
PROTO_PATH = os.path.join(os.path.dirname(__file__),
                          'protos',
                          'telephony_metric.proto')


class TelephonyMetricLogger(MetricLogger):
    """A logger for gathering Telephony test metrics

    Attributes:
        proto: Module used to store Telephony metrics in a proto
    """

    def __init__(self, event):
        super().__init__(event=event)
        self.proto = TelephonyVoiceTestResult()

    def set_result(self, result_type):
        self.proto.result = result_type

    def end(self, event):
        metric = ProtoMetric(name='telephony_voice_test_result',
                             data=self.proto)
        return self.publisher.publish(metric)

