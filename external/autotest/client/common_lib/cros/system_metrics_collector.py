class Metric(object):
    """Abstract base class for metrics."""
    def __init__(self,
                 description,
                 units=None,
                 higher_is_better=False):
        """
        Initializes a Metric.
        @param description: Description of the metric, e.g., used as label on a
                dashboard chart
        @param units: Units of the metric, e.g. percent, seconds, MB.
        @param higher_is_better: Whether a higher value is considered better or
                not.
        """
        self._description = description
        self._units = units
        self._higher_is_better = higher_is_better
        self._samples = []

    @property
    def description(self):
        """Description of the metric."""
        return self._description

    @property
    def units(self):
        """Units of the metric."""
        return self._units

    @property
    def higher_is_better(self):
        """Whether a higher value is considered better or not."""
        return self._higher_is_better

    @property
    def values(self):
        """Measured values of the metric."""
        if len(self._samples) == 0:
            return self._samples
        return self._aggregate(self._samples)

    @values.setter
    def values(self, samples):
        self._samples = samples

    def _aggregate(self, samples):
        """
        Subclasses can override this to aggregate the metric into a single
        sample.
        """
        return samples

    def _store_sample(self, sample):
        self._samples.append(sample)

    def pre_collect(self):
        """
        Hook called before metrics are being collected.
        """
        pass

    def post_collect(self):
        """
        Hook called after metrics has been collected.
        """
        pass

    def collect_metric(self):
        """
        Collects one sample.

        Implementations should call self._store_sample() once if it's not an
        aggregate, i.e., it overrides self._aggregate().
        """
        pass

    @classmethod
    def from_metric(cls, other):
        """
        Instantiate from an existing metric instance.
        """
        metric = cls(
                description=other.description,
                units=other.units,
                higher_is_better=other.higher_is_better)
        metric.values = other.values
        return metric

class PeakMetric(Metric):
    """
    Metric that collects the peak of another metric.
    """

    @property
    def description(self):
        return 'peak_' + super(PeakMetric, self).description

    def _aggregate(self, samples):
        return max(samples)

class SumMetric(Metric):
    """
    Metric that sums another metric.
    """

    @property
    def description(self):
        return 'sum_' + super(SumMetric, self).description

    def _aggregate(self, samples):
        return sum(samples)

class MemUsageMetric(Metric):
    """
    Metric that collects memory usage in percent.

    Memory usage is collected in percent. Buffers and cached are calculated
    as free memory.
    """
    def __init__(self, system_facade):
        super(MemUsageMetric, self).__init__('memory_usage', units='percent')
        self.system_facade = system_facade

    def collect_metric(self):
        total_memory = self.system_facade.get_mem_total()
        free_memory = self.system_facade.get_mem_free_plus_buffers_and_cached()
        used_memory = total_memory - free_memory
        usage_percent = (used_memory * 100) / total_memory
        self._store_sample(usage_percent)

class CpuUsageMetric(Metric):
    """
    Metric that collects cpu usage in percent.
    """
    def __init__(self, system_facade):
        super(CpuUsageMetric, self).__init__('cpu_usage', units='percent')
        self.last_usage = None
        self.system_facade = system_facade

    def pre_collect(self):
        self.last_usage = self.system_facade.get_cpu_usage()

    def collect_metric(self):
        """
        Collects CPU usage in percent.
        """
        current_usage = self.system_facade.get_cpu_usage()
        # Compute the percent of active time since the last update to
        # current_usage.
        usage_percent = 100 * self.system_facade.compute_active_cpu_time(
                self.last_usage, current_usage)
        self._store_sample(usage_percent)
        self.last_usage = current_usage

class AllocatedFileHandlesMetric(Metric):
    """
    Metric that collects the number of allocated file handles.
    """
    def __init__(self, system_facade):
        super(AllocatedFileHandlesMetric, self).__init__(
                'allocated_file_handles', units='handles')
        self.system_facade = system_facade

    def collect_metric(self):
        self._store_sample(self.system_facade.get_num_allocated_file_handles())

class StorageWrittenAmountMetric(Metric):
    """
    Metric that collects amount of kB written to persistent storage.
    """
    def __init__(self, system_facade):
        super(StorageWrittenAmountMetric, self).__init__(
                'storage_written_amount', units='kB')
        self.last_written_kb = None
        self.system_facade = system_facade

    def pre_collect(self):
        statistics = self.system_facade.get_storage_statistics()
        self.last_written_kb = statistics['written_kb']

    def collect_metric(self):
        """
        Collects total amount of data written to persistent storage in kB.
        """
        statistics = self.system_facade.get_storage_statistics()
        written_kb = statistics['written_kb']
        written_period = written_kb - self.last_written_kb
        self._store_sample(written_period)
        self.last_written_kb = written_kb

class StorageWrittenCountMetric(Metric):
    """
    Metric that collects the number of writes to persistent storage.
    """
    def __init__(self, system_facade):
        super(StorageWrittenCountMetric, self).__init__(
                'storage_written_count', units='count')
        self.system_facade = system_facade

    def pre_collect(self):
        command = ('/usr/sbin/fatrace', '--timestamp', '--filter=W')
        self.system_facade.start_bg_worker(command)

    def collect_metric(self):
        output = self.system_facade.get_and_discard_bg_worker_output()
        # fatrace outputs a line of text for each file it detects being written
        # to.
        written_count = output.count('\n')
        self._store_sample(written_count)

    def post_collect(self):
        self.system_facade.stop_bg_worker()

class TemperatureMetric(Metric):
    """
    Metric that collects the max of the temperatures measured on all sensors.
    """
    def __init__(self, system_facade):
        super(TemperatureMetric, self).__init__('temperature', units='Celsius')
        self.system_facade = system_facade

    def collect_metric(self):
        self._store_sample(self.system_facade.get_current_temperature_max())

def create_default_metric_set(system_facade):
    """
    Creates the default set of metrics.

    @param system_facade the system facade to initialize the metrics with.
    @return a list with Metric instances.
    """
    cpu = CpuUsageMetric(system_facade)
    mem = MemUsageMetric(system_facade)
    file_handles = AllocatedFileHandlesMetric(system_facade)
    storage_written_amount = StorageWrittenAmountMetric(system_facade)
    storage_written_count = StorageWrittenCountMetric(system_facade)
    temperature = TemperatureMetric(system_facade)
    peak_cpu = PeakMetric.from_metric(cpu)
    peak_mem = PeakMetric.from_metric(mem)
    peak_temperature = PeakMetric.from_metric(temperature)
    sum_storage_written_amount = SumMetric.from_metric(storage_written_amount)
    sum_storage_written_count = SumMetric.from_metric(storage_written_count)
    return [cpu,
            mem,
            file_handles,
            storage_written_amount,
            storage_written_count,
            temperature,
            peak_cpu,
            peak_mem,
            peak_temperature,
            sum_storage_written_amount,
            sum_storage_written_count]

class SystemMetricsCollector(object):
    """
    Collects system metrics.
    """
    def __init__(self, system_facade, metrics = None):
        """
        Initialize with facade and metric classes.

        @param system_facade The system facade to use for querying the system,
                e.g. system_facade_native.SystemFacadeNative for client tests.
        @param metrics List of metric instances. If None, the default set will
                be created.
        """
        self.metrics = (create_default_metric_set(system_facade)
                        if metrics is None else metrics)

    def pre_collect(self):
        """
        Calls pre hook of metrics.
        """
        for metric in self.metrics:
            metric.pre_collect()

    def post_collect(self):
        """
        Calls post hook of metrics.
        """
        for metric in self.metrics:
            metric.post_collect()

    def collect_snapshot(self):
        """
        Collects one snapshot of metrics.
        """
        for metric in self.metrics:
            metric.collect_metric()

    def write_metrics(self, writer_function):
        """
        Writes the collected metrics using the specified writer function.

        @param writer_function: A function with the following signature:
                 f(description, value, units, higher_is_better)
        """
        for metric in self.metrics:
            writer_function(
                    description=metric.description,
                    value=metric.values,
                    units=metric.units,
                    higher_is_better=metric.higher_is_better)
