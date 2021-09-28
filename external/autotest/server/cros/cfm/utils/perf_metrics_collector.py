import threading

from autotest_lib.client.common_lib.cros import system_metrics_collector
from autotest_lib.client.common_lib.cros.cfm.metrics import (
        media_metrics_collector)


class PerfMetricsCollector(object):
    """
    Metrics collector that runs in seprate thread than the caller.
    """
    def __init__(self, system_facade, cfm_facade, writer_function,
                 additional_system_metrics=[]):
        """
        Constructor.

        @param system_facade facade object to access system utils.
        @param cfm_facade facade object to access cfm utils.
        @param writer_function function called to collected metrics.
        @param additional_system_metrics Additional metrics to collect.
        """
        metric_set = system_metrics_collector.create_default_metric_set(
            system_facade)
        for metric in additional_system_metrics:
            metric_set.append(metric)
        self._system_metrics_collector = (
            system_metrics_collector.SystemMetricsCollector(system_facade,
                                                            metric_set))
        # Media metrics
        data_point_collector = media_metrics_collector.DataPointCollector(
            cfm_facade)
        self._media_metrics_collector = (media_metrics_collector
                                        .MetricsCollector(data_point_collector))

        self._writer_function = writer_function
        # Collector thread.
        self._collector_thread = threading.Thread(
            target=self._collect_snapshots_until_stopped)
        self._stop = threading.Event()

    def start(self):
        """
        Starts metrics collection.
        """
        self._system_metrics_collector.pre_collect()
        self._collector_thread.start()

    def stop(self):
        """
        Stops metrics collection.
        """
        self._stop.set()
        self._system_metrics_collector.post_collect()

    def upload_metrics(self):
        """
        Uploads collected metrics.
        """
        self._upload_system_metrics()
        self._upload_media_metrics()

    def _collect_snapshots_until_stopped(self):
        while not self._stop.wait(10):
            self._system_metrics_collector.collect_snapshot()
            self._media_metrics_collector.collect_snapshot()

    def _upload_system_metrics(self):
        self._system_metrics_collector.write_metrics(self._writer_function)

    def _get_jmi_data(self, data_type):
        """
        Gets jmi data for the given data type.

        @param data_type: Type of data to be retrieved from jmi data logs.
        @return Data for given data type from jmidata log.
        """
        timestamped_values = self._media_metrics_collector.get_metric(data_type)
        # Strip timestamps.
        values = [x[1] for x in timestamped_values]
        # Each entry in values is a list, extract the raw values:
        res = []
        for value_list in values:
            res.extend(value_list)
        # Ensure we always return at least one element, or perf uploads will
        # be sad.
        return res or [0]

    def _get_last_value(self, data_type):
        """
        Gets last value of a list of numbers.

        @param data_type: Type of data to be retrieved from jmi data log.
        @return The last value in the jmidata for the specified data_type. 0 if
                there are no values in the jmidata for this data_type.
        """
        data = self._get_jmi_data(data_type)
        if not data:
            return 0
        return data[-1]

    def _upload_media_metrics(self):
        """
        Write jmidata results to results-chart.json file for Perf Dashboard.
        """
        # Video/Sender metrics
        self._writer_function(
            description='avg_encode_ms',
            value=self._get_jmi_data(media_metrics_collector.AVG_ENCODE_MS),
            units='ms',
            higher_is_better=False)

        self._writer_function(
            description='vid_out_frame_height', # video_out_res
            value=self._get_jmi_data(media_metrics_collector.
                VIDEO_SENT_FRAME_HEIGHT),
            units='px',
            higher_is_better=True)

        self._writer_function(
            description='vid_out_frame_width',
            value=self._get_jmi_data(media_metrics_collector.
                VIDEO_SENT_FRAME_WIDTH),
            units='px',
            higher_is_better=True)

        self._writer_function(
            description='vid_out_framerate_captured',
            value=self._get_jmi_data(media_metrics_collector.
                FRAMERATE_CAPTURED),
            units='fps',
            higher_is_better=True)

        self._writer_function(
            description='vid_out_framerate_encoded',
            value=self._get_jmi_data(media_metrics_collector.
                FRAMERATE_ENCODED),
            units='fps',
            higher_is_better=True)

        self._writer_function(
            description='vid_out_sent_packets',
            value=self._get_last_value(media_metrics_collector.
                VIDEO_SENT_PACKETS),
            units='packets',
            higher_is_better=True)

        # Video/Receiver metrics
        self._writer_function(
            description='vid_in_framerate_received',
            value=self._get_jmi_data(media_metrics_collector.
                FRAMERATE_NETWORK_RECEIVED),
            units='fps',
            higher_is_better=True)

        self._writer_function(
            description='vid_in_framerate_decoded',
            value=self._get_jmi_data(media_metrics_collector.FRAMERATE_DECODED),
            units='fps',
            higher_is_better=True)

        self._writer_function(
            description='vid_in_framerate_to_renderer',
            value=self._get_jmi_data(media_metrics_collector.
                FRAMERATE_TO_RENDERER),
            units='fps',
            higher_is_better=True)

        self._writer_function(
            description='video_in_frame_heigth', # video_in_res
            value=self._get_jmi_data(media_metrics_collector.
                VIDEO_RECEIVED_FRAME_HEIGHT),
            units='px',
            higher_is_better=True)

        self._writer_function(
            description='vid_in_frame_width',
            value=self._get_jmi_data(media_metrics_collector.
                VIDEO_RECEIVED_FRAME_WIDTH),
            units='px',
            higher_is_better=True)

        # Adaptation metrics
        self._writer_function(
            description='vid_out_adapt_changes',
            value=self._get_last_value(media_metrics_collector.
                ADAPTATION_CHANGES),
            units='count',
            higher_is_better=False)

        self._writer_function(
            description='vid_out_adapt_reasons',
            value=self._get_jmi_data(media_metrics_collector.ADAPTATION_REASON),
            units='reasons',
            higher_is_better=False)

        # System metrics
        self._writer_function(
            description='cpu_usage_jmi',
            value=self._get_jmi_data(media_metrics_collector.
                CPU_PERCENT_OF_TOTAL),
            units='percent',
            higher_is_better=False)

        self._writer_function(
            description='process_js_memory',
            value=[(x / (1024 * 1024)) for x in self._get_jmi_data(
                media_metrics_collector.PROCESS_JS_MEMORY_USED)],
            units='MB',
            higher_is_better=False)

        self._writer_function(
            description='renderer_cpu_usage',
            value=self._get_jmi_data(
                media_metrics_collector.RENDERER_CPU_PERCENT_OF_TOTAL),
            units='percent',
            higher_is_better=False)

        self._writer_function(
            description='browser_cpu_usage',
            value=self._get_jmi_data(
                media_metrics_collector.BROWSER_CPU_PERCENT_OF_TOTAL),
            units='percent',
            higher_is_better=False)

        self._writer_function(
            description='gpu_cpu_usage',
            value=self._get_jmi_data(
                media_metrics_collector.GPU_PERCENT_OF_TOTAL),
            units='percent',
            higher_is_better=False)

        # Other
        self._writer_function(
            description='active_streams',
            value=self._get_jmi_data(media_metrics_collector.
                NUMBER_OF_ACTIVE_INCOMING_VIDEO_STREAMS),
            units='count',
            higher_is_better=True)
