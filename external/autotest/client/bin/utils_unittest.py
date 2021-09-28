#!/usr/bin/python2

__author__ = "kerl@google.com, gwendal@google.com (Gwendal Grignou)"

import io
import mock
import unittest

from autotest_lib.client.bin import utils

_IOSTAT_OUTPUT = (
    'Linux 3.8.11 (localhost)   02/19/19        _x86_64_        (4 CPU)\n'
    '\n'
    'Device            tps    kB_read/s    kB_wrtn/s    kB_read    kB_wrtn\n'
    'ALL              4.45        10.33       292.40     665582     188458\n'
    '\n')

class TestUtils(unittest.TestCase):
    """Test utils functions."""

    # Test methods, disable missing-docstring
    # pylint: disable=missing-docstring
    def setUp(self):
        utils._open_file = self.fake_open
        # Files opened with utils._open_file will contain this string.
        self.fake_file_text = ''

    def fake_open(self, path):
        # Use BytesIO instead of StringIO to support with statements.
        return io.BytesIO(bytes(self.fake_file_text))

    def test_concat_partition(self):
        self.assertEquals("nvme0n1p3", utils.concat_partition("nvme0n1", 3))
        self.assertEquals("mmcblk1p3", utils.concat_partition("mmcblk1", 3))
        self.assertEquals("sda3", utils.concat_partition("sda", 3))

    # The columns in /proc/stat are:
    # user nice system idle iowait irq softirq steal guest guest_nice
    #
    # Although older kernel versions might not contain all of them.
    # Unit is 1/100ths of a second.
    def test_get_cpu_usage(self):
        self.fake_file_text = 'cpu 254544 9 254768 2859878 1 2 3 4 5 6\n'
        usage = utils.get_cpu_usage()
        self.assertEquals({
            'user': 254544,
            'nice': 9,
            'system': 254768,
            'idle': 2859878,
            'iowait': 1,
            'irq': 2,
            'softirq': 3,
            'steal': 4,
            'guest': 5,
            'guest_nice': 6
        }, usage)

    def test_get_cpu_missing_columns(self):
        self.fake_file_text = 'cpu 254544 9 254768 2859878\n'
        usage = utils.get_cpu_usage()
        self.assertEquals({
            'user': 254544,
            'nice': 9,
            'system': 254768,
            'idle': 2859878,
            'iowait': 0,
            'irq': 0,
            'softirq': 0,
            'steal': 0,
            'guest': 0,
            'guest_nice': 0
        }, usage)

    def test_compute_active_cpu_time(self):
        start_usage = {
            'user': 900,
            'nice': 10,
            'system': 90,
            'idle': 10000,
            'iowait': 500,
            'irq': 100,
            'softirq': 50,
            'steal': 150,
            'guest': 170,
            'guest_nice': 30
        }
        end_usage = {
            'user': 1800,
            'nice': 20,
            'system': 180,
            'idle': 13000,
            'iowait': 2000,
            'irq': 200,
            'softirq': 100,
            'steal': 300,
            'guest': 340,
            'guest_nice': 60
        }
        usage = utils.compute_active_cpu_time(start_usage, end_usage)
        self.assertAlmostEqual(usage, 0.25)

    def test_compute_active_cpu_time_idle(self):
        start_usage = {
            'user': 900,
            'nice': 10,
            'system': 90,
            'idle': 10000,
            'iowait': 500,
            'irq': 100,
            'softirq': 50,
            'steal': 150,
            'guest': 170,
            'guest_nice':30
        }
        end_usage = {
            'user': 900,
            'nice': 10,
            'system': 90,
            'idle': 11000,
            'iowait': 1000,
            'irq': 100,
            'softirq': 50,
            'steal': 150,
            'guest': 170,
            'guest_nice':30
        }
        usage = utils.compute_active_cpu_time(start_usage, end_usage)
        self.assertAlmostEqual(usage, 0)

    def test_get_mem_total(self):
        self.fake_file_text = ('MemTotal:  2048000 kB\n'
                               'MemFree:  307200 kB\n'
                               'Buffers:  102400 kB\n'
                               'Cached:   204800 kB\n')
        self.assertAlmostEqual(utils.get_mem_total(), 2000)

    def test_get_mem_free(self):
        self.fake_file_text = ('MemTotal:  2048000 kB\n'
                               'MemFree:  307200 kB\n'
                               'Buffers:  102400 kB\n'
                               'Cached:   204800 kB\n')
        self.assertAlmostEqual(utils.get_mem_free(), 300)

    def test_get_mem_free_plus_buffers_and_cached(self):
        self.fake_file_text = ('MemTotal:  2048000 kB\n'
                               'MemFree:  307200 kB\n'
                               'Buffers:  102400 kB\n'
                               'Cached:   204800 kB\n')
        self.assertAlmostEqual(utils.get_mem_free_plus_buffers_and_cached(),
                               600)

    def test_get_meminfo(self):
        self.fake_file_text = ('MemTotal:      2048000 kB\n'
                               'MemFree:        307200 kB\n'
                               'Buffers:        102400 kB\n'
                               'Cached:         204800 kB\n'
                               'Active(anon):   409600 kB')
        meminfo = utils.get_meminfo()
        self.assertEqual(meminfo.MemTotal, 2048000)
        self.assertEqual(meminfo.Active_anon, 409600)

    def test_get_num_allocated_file_handles(self):
        self.fake_file_text = '123 0 456\n'
        self.assertEqual(utils.get_num_allocated_file_handles(), 123)

    @mock.patch('autotest_lib.client.common_lib.utils.system_output')
    def test_get_storage_statistics(self, system_output_mock):
        system_output_mock.return_value = _IOSTAT_OUTPUT
        statistics = utils.get_storage_statistics()
        self.assertEqual({
            'read_kb': 665582.0,
            'written_kb_per_s': 292.4,
            'read_kb_per_s': 10.33,
            'transfers_per_s': 4.45,
            'written_kb': 188458.0,
        }, statistics)
