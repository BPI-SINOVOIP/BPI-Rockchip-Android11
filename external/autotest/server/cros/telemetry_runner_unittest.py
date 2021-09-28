#!/usr/bin/python2
# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import json
import unittest

import common
from autotest_lib.server.cros import telemetry_runner

histograms_sample = [
    {
        'values': [
            'story1'
        ],
        'guid': '00000001-...',
        'type': 'GenericSet'
    },
    {
        'values': [
            'story2'
        ],
        'guid': '00000002-...',
        'type': 'GenericSet'
    },
    {
        'values': [
            'benchmark1'
        ],
        'guid': 'a0000001-...',
        'type': 'GenericSet'
    },
    {
        'values': [
            'benchmark_desc1'
        ],
        'guid': 'b0000001-...',
        'type': 'GenericSet'
    },
    {
        'sampleValues': [1.0, 2.0],
        'name': 'metric1',
        'diagnostics': {
            'stories': '00000001-...',
            'benchmarks': 'a0000001-...',
            'benchmarkDescriptions': 'b0000001-...'
        },
        'unit': 'ms_smallerIsBetter'
    },
    {
        'sampleValues': [1.0, 2.0],
        'name': 'metric1',
        'diagnostics': {
            'stories': '00000002-...',
            'benchmarks': 'a0000001-...',
            'benchmarkDescriptions': 'b0000001-...'
        },
        'unit': 'ms_smallerIsBetter'
    }
]

chartjson_sample = {
    'format_version': 1.0,
    'benchmark_name': 'benchmark1',
    'benchmark_description': 'benchmark_desc1',
    'benchmark_metadata': {
        'type': 'telemetry_benchmark',
        'name': 'benchmark1',
        'description': 'benchmark_desc1'
    },
    'charts': {
        'metric1': {
            'story1': {
                'std': 0.5,
                'name': 'metric1',
                'type': 'list_of_scalar_values',
                'values': [1.0, 2.0],
                'units': 'ms',
                'improvement_direction': 'down'
            },
            'story2': {
                'std': 0.5,
                'name': 'metric1',
                'type': 'list_of_scalar_values',
                'values': [1.0, 2.0],
                'units': 'ms',
                'improvement_direction': 'down'
            },
            'summary': {
                'std': 0.5,
                'name': 'metric1',
                'type': 'list_of_scalar_values',
                'values': [1.0, 1.0, 2.0, 2.0],
                'units': 'ms',
                'improvement_direction': 'down'
            }
        },
    }
}

class TelemetryRunnerTestCase(unittest.TestCase):
    """Test telemetry runner module."""

    def test_convert_chart_json(self):
        # Deep comparison of 2 objects with json dumps.
        converted = telemetry_runner.TelemetryRunner.convert_chart_json(
            histograms_sample)
        chartjson_dumps = json.dumps(chartjson_sample, sort_keys=True)
        chartjson_dumps2 = json.dumps(converted, sort_keys=True)
        self.assertEqual(chartjson_dumps, chartjson_dumps2)

if __name__ == '__main__':
  unittest.main()
