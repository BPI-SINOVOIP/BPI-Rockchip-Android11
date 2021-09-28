from autotest_lib.client.common_lib.cros import system_metrics_collector

import unittest

_FATRACE_OUTPUT_1 = (
    'metrics_daemon(2276): W /var/log/vmlog/vmlog.20190221-005447\n'
    'powerd(612): CW /var/lib/metrics/uma-events\n'
    'powerd(612): CWO /var/lib/metrics/uma-events\n')

_FATRACE_OUTPUT_2 = (
    'chrome(13096): CWO /home/chronos/.com.google.Chrome.PMfcCO\n'
    'chrome(13096): W /home/chronos/.com.google.Chrome.PMfcCO\n'
    'chrome(13096): CW /home/chronos/Local State\n'
    'metrics_daemon(2276): W /var/log/vmlog/vmlog.20190221-005447\n')

# pylint: disable=missing-docstring
class TestSystemMetricsCollector(unittest.TestCase):
    """
    Tests for the system_metrics_collector module.
    """
    def test_mem_usage_metric(self):
        metric = system_metrics_collector.MemUsageMetric(FakeSystemFacade())
        metric.collect_metric()
        self.assertAlmostEqual(60, metric.values[0])

    def test_file_handles_metric(self):
        metric = system_metrics_collector.AllocatedFileHandlesMetric(
                FakeSystemFacade())
        metric.collect_metric()
        self.assertEqual(11, metric.values[0])

    def test_cpu_usage_metric(self):
        system_facade = FakeSystemFacade()
        metric = system_metrics_collector.CpuUsageMetric(system_facade)
        metric.pre_collect()
        system_facade.active_cpu_time += 0.1
        metric.collect_metric()
        self.assertAlmostEqual(50, metric.values[0])

    def test_tempature_metric(self):
        metric = system_metrics_collector.TemperatureMetric(FakeSystemFacade())
        metric.collect_metric()
        self.assertAlmostEqual(43, metric.values[0])

    def test_storage_written_amount_metric(self):
        system_facade = FakeSystemFacade()
        metric = system_metrics_collector.StorageWrittenAmountMetric(
                system_facade)
        metric.pre_collect()
        system_facade.storage_statistics['written_kb'] += 1337
        metric.collect_metric()
        self.assertEqual(1337, metric.values[0])

    def test_storage_written_count_metric(self):
        system_facade = FakeSystemFacade()
        metric = system_metrics_collector.StorageWrittenCountMetric(
                system_facade)
        metric.pre_collect()
        system_facade.bg_worker_output = _FATRACE_OUTPUT_1
        metric.collect_metric()
        system_facade.bg_worker_output = _FATRACE_OUTPUT_2
        metric.collect_metric()
        metric.post_collect()
        self.assertEqual(3, metric.values[0])
        self.assertEqual(4, metric.values[1])

    def test_collector(self):
        collector = system_metrics_collector.SystemMetricsCollector(
                FakeSystemFacade(), [TestMetric()])
        collector.collect_snapshot()
        d = {}
        def _write_func(**kwargs):
            d.update(kwargs)
        collector.write_metrics(_write_func)
        self.assertEquals('test_description', d['description'])
        self.assertEquals([1], d['value'])
        self.assertEquals(False, d['higher_is_better'])
        self.assertEquals('test_unit', d['units'])

    def test_collector_default_set_of_metrics_no_error(self):
        # Only verify no errors are thrown when collecting using
        # the default metric set.
        collector = system_metrics_collector.SystemMetricsCollector(
                FakeSystemFacade())
        collector.pre_collect()
        collector.collect_snapshot()
        collector.collect_snapshot()
        collector.post_collect()
        collector.write_metrics(lambda **kwargs: None)

    def test_aggregate_metric_zero_samples(self):
        metric = TestAggregateMetric()
        self.assertEqual(metric.values, [])

    def test_aggregate_metric_one_sample(self):
        metric = TestAggregateMetric()
        metric.collect_metric()
        self.assertEqual(metric.values, 1)

    def test_aggregate_metric_many_samples(self):
        metric = TestAggregateMetric()
        metric.collect_metric()
        metric.value = 2
        metric.collect_metric()
        metric.value = 3
        metric.collect_metric()
        self.assertEqual(metric.values, 3)

    def test_aggregate_metric_from_metric_one_sample(self):
        test_metric = TestMetric()
        aggregate_metric = LastElementMetric.from_metric(test_metric)
        test_metric.collect_metric()
        aggregate_metric.collect_metric()
        self.assertEqual(test_metric.values, [1])
        self.assertEqual(aggregate_metric.values, 1)

    def test_aggregate_metric_from_metric_many_samples(self):
        test_metric = TestMetric()
        aggregate_metric = LastElementMetric.from_metric(test_metric)
        test_metric.collect_metric()
        aggregate_metric.collect_metric()
        test_metric.value = 2
        test_metric.collect_metric()
        aggregate_metric.collect_metric()
        test_metric.value = 3
        test_metric.collect_metric()
        aggregate_metric.collect_metric()
        self.assertEqual(test_metric.values, [1, 2, 3])
        self.assertEqual(aggregate_metric.values, 3)

    def test_peak_metric_description(self):
        metric = system_metrics_collector.PeakMetric('foo')
        self.assertEqual(metric.description, 'peak_foo')

    def test_peak_metric_many_samples(self):
        metric = TestPeakMetric()
        metric.collect_metric()
        metric.value = 2
        metric.collect_metric()
        metric.value = 0
        metric.collect_metric()
        self.assertEqual(metric.values, 2)

    def test_peak_metric_from_metric_many_samples(self):
        test_metric = TestMetric()
        peak_metric = system_metrics_collector.PeakMetric.from_metric(
                test_metric)
        test_metric.collect_metric()
        peak_metric.collect_metric()
        test_metric.value = 2
        test_metric.collect_metric()
        peak_metric.collect_metric()
        test_metric.value = 0
        test_metric.collect_metric()
        peak_metric.collect_metric()
        self.assertEqual(peak_metric.values, 2)

    def test_sum_metric_description(self):
        metric = system_metrics_collector.SumMetric('foo')
        self.assertEqual(metric.description, 'sum_foo')

    def test_sum_metric_many_samples(self):
        metric = TestSumMetric()
        metric.collect_metric()
        metric.value = 2
        metric.collect_metric()
        metric.value = 3
        metric.collect_metric()
        self.assertEqual(metric.values, 6)

    def test_sum_metric_from_metric_many_samples(self):
        test_metric = TestMetric()
        sum_metric = system_metrics_collector.SumMetric.from_metric(
                test_metric)
        test_metric.collect_metric()
        sum_metric.collect_metric()
        test_metric.value = 40
        test_metric.collect_metric()
        sum_metric.collect_metric()
        test_metric.value = 1
        test_metric.collect_metric()
        sum_metric.collect_metric()
        self.assertEqual(sum_metric.values, 42)

class FakeSystemFacade(object):
    def __init__(self):
        self.mem_total_mb = 1000.0
        self.mem_free_mb = 400.0
        self.file_handles = 11
        self.active_cpu_time = 0.4
        self.current_temperature_max = 43
        self.storage_statistics = {
            'transfers_per_s': 4.45,
            'read_kb_per_s': 10.33,
            'written_kb_per_s':  292.40,
            'read_kb': 665582,
            'written_kb': 188458,
        }
        self.bg_worker_output = ''

    def get_mem_total(self):
        return self.mem_total_mb

    def get_mem_free_plus_buffers_and_cached(self):
        return self.mem_free_mb

    def get_num_allocated_file_handles(self):
        return self.file_handles

    def get_cpu_usage(self):
        return {}

    def compute_active_cpu_time(self, last_usage, current_usage):
        return self.active_cpu_time

    def get_current_temperature_max(self):
        return self.current_temperature_max

    def get_storage_statistics(self, device=None):
        return self.storage_statistics

    def start_bg_worker(self, command):
        pass

    def get_and_discard_bg_worker_output(self):
        return self.bg_worker_output

    def stop_bg_worker(self):
        pass

class TestMetric(system_metrics_collector.Metric):
    def __init__(self):
        super(TestMetric, self).__init__(
                'test_description', units='test_unit')
        self.value = 1

    def collect_metric(self):
        self._store_sample(self.value)

class LastElementMetric(system_metrics_collector.Metric):
    def _aggregate(self, x):
        return x[-1]

class TestAggregateMetric(TestMetric, LastElementMetric):
    pass

class TestPeakMetric(TestMetric, system_metrics_collector.PeakMetric):
    pass

class TestSumMetric(TestMetric, system_metrics_collector.SumMetric):
    pass
