#!/usr/bin/env python3
# -*- coding: utf-8 -*-
# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Tests for the tabulator module."""

from __future__ import print_function

__author__ = 'asharif@google.com (Ahmad Sharif)'

# System modules
import unittest

# Local modules
from cros_utils import tabulator


class TabulatorTest(unittest.TestCase):
  """Tests for the Tabulator class."""

  def testResult(self):
    table = ['k1', ['1', '3'], ['55']]
    result = tabulator.Result()
    cell = tabulator.Cell()
    result.Compute(cell, table[2], table[1])
    expected = ' '.join([str(float(v)) for v in table[2]])
    self.assertTrue(cell.value == expected)

    result = tabulator.AmeanResult()
    cell = tabulator.Cell()
    result.Compute(cell, table[2], table[1])
    self.assertTrue(cell.value == float(table[2][0]))

  def testStringMean(self):
    smr = tabulator.StringMeanResult()
    cell = tabulator.Cell()
    value = 'PASS'
    values = [value for _ in range(3)]
    smr.Compute(cell, values, None)
    self.assertTrue(cell.value == value)
    values.append('FAIL')
    smr.Compute(cell, values, None)
    self.assertTrue(cell.value == '?')

  def testStorageFormat(self):
    sf = tabulator.StorageFormat()
    cell = tabulator.Cell()
    base = 1024.0
    cell.value = base
    sf.Compute(cell)
    self.assertTrue(cell.string_value == '1.0K')
    cell.value = base**2
    sf.Compute(cell)
    self.assertTrue(cell.string_value == '1.0M')
    cell.value = base**3
    sf.Compute(cell)
    self.assertTrue(cell.string_value == '1.0G')

  def testLerp(self):
    c1 = tabulator.Color(0, 0, 0, 0)
    c2 = tabulator.Color(255, 0, 0, 0)
    c3 = tabulator.Color.Lerp(0.5, c1, c2)
    self.assertTrue(c3.r == 127.5)
    self.assertTrue(c3.g == 0)
    self.assertTrue(c3.b == 0)
    self.assertTrue(c3.a == 0)
    c3.Round()
    self.assertTrue(c3.r == 127)

  def testGmean(self):
    a = [1.0e+308] * 3
    # pylint: disable=protected-access
    b = tabulator.Result()._GetGmean(a)
    self.assertTrue(0.99e+308 <= b <= 1.01e+308)

  def testIgnoreMinMax(self):
    amr = tabulator.AmeanResult(ignore_min_max=True)
    cell = tabulator.Cell()
    values = [1, 2]
    amr.Compute(cell, values, None)
    self.assertTrue(cell.value == 1.5)
    values = [1, 2, 8]
    amr.Compute(cell, values, None)
    self.assertTrue(cell.value == 2)

  def testTableGenerator(self):
    # yapf: disable
    runs = [[{'k1': '10', 'k2': '12'},
             {'k1': '13', 'k2': '14', 'k3': '15'}],
            [{'k1': '50', 'k2': '51', 'k3': '52', 'k4': '53'}]]
    # yapf: enable
    labels = ['vanilla', 'modified']
    tg = tabulator.TableGenerator(runs, labels)
    table = tg.GetTable()
    header = table.pop(0)

    self.assertTrue(header == ['keys', 'vanilla', 'modified'])
    row = table.pop(0)
    self.assertTrue(row == ['k1', ['10', '13'], ['50']])
    row = table.pop(0)
    self.assertTrue(row == ['k2', ['12', '14'], ['51']])
    row = table.pop(0)
    self.assertTrue(row == ['k3', [None, '15'], ['52']])
    row = table.pop(0)
    self.assertTrue(row == ['k4', [None, None], ['53']])

    table = tg.GetTable()
    columns = [
        tabulator.Column(tabulator.AmeanResult(), tabulator.Format()),
        tabulator.Column(tabulator.AmeanRatioResult(),
                         tabulator.PercentFormat()),
    ]
    tf = tabulator.TableFormatter(table, columns)
    table = tf.GetCellTable()
    self.assertTrue(table)

  def testSamplesTableGenerator(self):
    # yapf: disable
    keyvals = {
        'bench1': [[{'samples': 1}, {'samples': 2}],
                   [{'samples': 3}, {'samples': 4}]],
        'bench2': [[{'samples': 5}, {}],
                   [{'samples': 6}, {'samples': 7}]]
    }
    # yapf: enable
    weights = {'bench1': 0.2, 'bench2': 0.7}
    iter_counts = {'bench1': 2, 'bench2': 2}
    labels = ['vanilla', 'modified']
    tg = tabulator.SamplesTableGenerator(keyvals, labels, iter_counts, weights)
    (table, new_keyvals, new_iter_counts) = tg.GetTable()

    columns = [
        tabulator.Column(tabulator.IterationResult(), tabulator.Format()),
        tabulator.Column(tabulator.AmeanResult(), tabulator.Format()),
        tabulator.Column(tabulator.AmeanRatioResult(),
                         tabulator.PercentFormat()),
    ]
    # This is the function to load column info.
    tf = tabulator.TableFormatter(table, columns, samples_table=True)
    # This is the function where to do all weighting calculation.
    cell_table = tf.GetCellTable('summary')
    self.assertTrue(cell_table)

    header = table.pop(0)
    self.assertTrue(header == ['Benchmarks', 'Weights', 'vanilla', 'modified'])
    row = table.pop(0)
    # yapf: disable
    self.assertTrue(row == ['bench1', 0.2,
                            ((2, 0), [1 * 0.2, 2 * 0.2]),
                            ((2, 0), [3 * 0.2, 4 * 0.2])])
    row = table.pop(0)
    self.assertTrue(row == ['bench2', 0.7,
                            ((1, 1), [5 * 0.7, None]),
                            ((2, 0), [6 * 0.7, 7 * 0.7])])
    row = table.pop(0)
    self.assertTrue(row == ['Composite Benchmark (samples)', 'N/A',
                            ((1, 1), [1 * 0.2 + 5 * 0.7, None]),
                            ((2, 0), [3 * 0.2 + 6 * 0.7, 4 * 0.2 + 7 * 0.7])])
    # yapf: enable
    self.assertTrue('Composite Benchmark' in new_keyvals.keys())
    self.assertTrue('Composite Benchmark' in new_iter_counts.keys())

  def testColspan(self):
    simple_table = [
        ['binary', 'b1', 'b2', 'b3'],
        ['size', 100, 105, 108],
        ['rodata', 100, 80, 70],
        ['data', 100, 100, 100],
        ['debug', 100, 140, 60],
    ]
    columns = [
        tabulator.Column(tabulator.AmeanResult(), tabulator.Format()),
        tabulator.Column(tabulator.MinResult(), tabulator.Format()),
        tabulator.Column(tabulator.AmeanRatioResult(),
                         tabulator.PercentFormat()),
        tabulator.Column(tabulator.AmeanRatioResult(),
                         tabulator.ColorBoxFormat()),
    ]
    our_table = [simple_table[0]]
    for row in simple_table[1:]:
      our_row = [row[0]]
      for v in row[1:]:
        our_row.append([v])
      our_table.append(our_row)

    tf = tabulator.TableFormatter(our_table, columns)
    cell_table = tf.GetCellTable()
    self.assertTrue(cell_table[0][0].colspan == 1)
    self.assertTrue(cell_table[0][1].colspan == 2)
    self.assertTrue(cell_table[0][2].colspan == 4)
    self.assertTrue(cell_table[0][3].colspan == 4)
    for row in cell_table[1:]:
      for cell in row:
        self.assertTrue(cell.colspan == 1)


if __name__ == '__main__':
  unittest.main()
