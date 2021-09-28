# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unit tests for power telemetry utils."""

import unittest

import power_telemetry_utils


class TestInterpolateData(unittest.TestCase):
  """Collection of tests to test smooten_data function in utils."""

  def test_Interpolate(self):
      """Test that regular smoothening of data works."""
      data = [1.2, 3.6, float('nan'), float('nan'), 2.7]
      expected_interp_data = [1.2, 3.6, 3.3, 3.0, 2.7]
      interp_data = power_telemetry_utils.interpolate_missing_data(data)
      self.assertListEqual(interp_data, expected_interp_data)

  def test_InterpolateAllNaN(self):
      """Test that a full NaN array cannot be smoothed."""
      data = [float('nan'), float('nan'), float('nan'), float('nan')]
      with self.assertRaisesRegexp(power_telemetry_utils.TelemetryUtilsError,
                                   'Data has no valid readings.'):
          power_telemetry_utils.interpolate_missing_data(data)

  def test_InterpolateGapStartAtBeginning(self):
      """Test that a gap starting at the start gets the first known value."""
      data = [float('nan'), float('nan'), 2.6]
      expected_interp_data = [2.6, 2.6, 2.6]
      interp_data = power_telemetry_utils.interpolate_missing_data(data)
      self.assertListEqual(interp_data, expected_interp_data)

  def test_InterpolateGapEndsAtEnd(self):
      """Test that a gap that ends at the end receives the last known value."""
      data = [2.6, float('nan'), float('nan')]
      expected_interp_data = [2.6, 2.6, 2.6]
      interp_data = power_telemetry_utils.interpolate_missing_data(data)
      self.assertListEqual(interp_data, expected_interp_data)

  def test_InterpolateTwoGaps(self):
      """Test that two distinct gaps receive distinct values."""
      data = [2.6, float('nan'), 3.4, 2.0 , float('nan'), 2.5]
      expected_interp_data = [2.6, 3.0, 3.4, 2.0, 2.25, 2.5]
      interp_data = power_telemetry_utils.interpolate_missing_data(data)
      self.assertListEqual(interp_data, expected_interp_data)

  def test_InterpolateHandlesIntegerDivision(self):
      """Test that integer division does not cause issues."""
      data = [2, float('nan'), 3]
      expected_interp_data = [2, 2.5, 3]
      interp_data = power_telemetry_utils.interpolate_missing_data(data)
      self.assertListEqual(interp_data, expected_interp_data)

  def test_AcceptableNaNRatio(self):
      """Validation succeeds if the ratio of NaN is below the threshold."""
      data = [2, float('nan'), 3, 4, 5, 6]
      # This should pass as there are only 1/6 NaN in the data.
      max_nan_ratio = 0.3
      args = {'max_nan_ratio': max_nan_ratio}
      interp_data = power_telemetry_utils.interpolate_missing_data(data,
                                                                   **args)

  def test_ExcessiveNaNRatio(self):
      """Validation fails if the ratio of NaN to valid readings is too high."""
      data = [2, float('nan'), 3, 4, 5, 6]
      # This should fail as there are 1/6 NaN in the data.
      max_nan_ratio = 0.1
      args = {'max_nan_ratio': max_nan_ratio}
      with self.assertRaisesRegexp(power_telemetry_utils.TelemetryUtilsError,
                                   'NaN ratio of'):
          interp_data = power_telemetry_utils.interpolate_missing_data(data,
                                                                       **args)

  def test_ExcessiveNaNSampleGap(self):
      """Validation fails on too many consecutive NaN samples."""
      data = [2, float('nan'), float('nan'), float('nan'), 3, 4, 5, 6]
      # This should fail as there is a 3 NaN gap.
      max_sample_gap = 2
      args = {'max_sample_gap': max_sample_gap}
      with self.assertRaisesRegexp(power_telemetry_utils.TelemetryUtilsError,
                                   'Too many consecutive NaN samples:'):
          interp_data = power_telemetry_utils.interpolate_missing_data(data,
                                                                       **args)

  def test_ExcessiveNaNSampleGapAtBeginning(self):
      """Validation fails on too many consecutive NaN samples at the start."""
      data = [float('nan'), float('nan'), float('nan'), 2]
      # This should fail as there is a 3 NaN gap.
      max_sample_gap = 2
      args = {'max_sample_gap': max_sample_gap}
      with self.assertRaisesRegexp(power_telemetry_utils.TelemetryUtilsError,
                                   'Too many consecutive NaN samples:'):
          interp_data = power_telemetry_utils.interpolate_missing_data(data,
                                                                       **args)

  def test_ExcessiveNaNSampleGapAtEnd(self):
      """Validation fails on too many consecutive NaN samples at the end."""
      data = [2, float('nan'), float('nan'), float('nan')]
      # This should fail as there is a 3 NaN gap.
      max_sample_gap = 2
      args = {'max_sample_gap': max_sample_gap}
      with self.assertRaisesRegexp(power_telemetry_utils.TelemetryUtilsError,
                                   'Too many consecutive NaN samples:'):
          interp_data = power_telemetry_utils.interpolate_missing_data(data,
                                                                       **args)

  def test_AcceptableNaNTimeSampleGap(self):
      """Validation succeeds if NaN gap is below threshold given a timeline."""
      data = [2, float('nan'), float('nan'), 3, 4, 5, 6]
      # Timeline is s for the data above.
      timeline = [1, 4, 7, 10, 13, 16, 19]
      # This should not fail as there is only 9s gap.
      max_sample_time_gap = 10
      args = {'max_sample_time_gap': max_sample_time_gap,
              'timeline': timeline}
      interp_data = power_telemetry_utils.interpolate_missing_data(data, **args)

  def test_ExcessiveNaNTimeSampleGap(self):
      """Validation fails if NaN gap is too long on a given timeline."""
      data = [2, float('nan'), float('nan'), 3, 4, 5, 6]
      # Timeline is s for the data above.
      timeline = [1, 4, 7, 10, 13, 16, 19]
      # This should fail as there 9s of gap.
      max_sample_time_gap = 8
      args = {'max_sample_time_gap': max_sample_time_gap,
              'timeline': timeline}
      with self.assertRaisesRegexp(power_telemetry_utils.TelemetryUtilsError,
                                   'Excessively long sample gap'):
          interp_data = power_telemetry_utils.interpolate_missing_data(data,
                                                                       **args)

  def test_NaNTimeSampleGapRequiresTimeline(self):
      """|timeline| arg is required if checking for sample gap time."""
      data = [2, float('nan'), float('nan'), 3, 4, 5, 6]
      # Timeline is s for the data above.
      timeline = [1, 4, 7, 10, 13, 16, 19]
      # This should fail the timeline is not provided in the args but the check
      # is requested.
      max_sample_time_gap = 2
      args = {'max_sample_time_gap': max_sample_time_gap}
      with self.assertRaisesRegexp(power_telemetry_utils.TelemetryUtilsError,
                                   'Supplying max_sample_time_gap'):
          interp_data = power_telemetry_utils.interpolate_missing_data(data,
                                                                       **args)
