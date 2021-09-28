# Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""This is a client side WebGL aquarium test.

Description of some of the test result output:
    - interframe time: The time elapsed between two frames. It is the elapsed
            time between two consecutive calls to the render() function.
    - render time: The time it takes in Javascript to construct a frame and
            submit all the GL commands. It is the time it takes for a render()
            function call to complete.
"""

import functools
import logging
import math
import os
import sampler
import system_sampler
import threading
import time

from autotest_lib.client.bin import fps_meter
from autotest_lib.client.bin import utils
from autotest_lib.client.common_lib import error
from autotest_lib.client.common_lib.cros import chrome
from autotest_lib.client.common_lib.cros import memory_eater
from autotest_lib.client.cros.graphics import graphics_utils
from autotest_lib.client.cros import perf
from autotest_lib.client.cros import service_stopper
from autotest_lib.client.cros.power import power_rapl, power_status, power_utils

# Minimum battery charge percentage to run the test
BATTERY_INITIAL_CHARGED_MIN = 10

# Measurement duration in seconds.
MEASUREMENT_DURATION = 30

POWER_DESCRIPTION = 'avg_energy_rate_1000_fishes'

# Time to exclude from calculation after playing a webgl demo [seconds].
STABILIZATION_DURATION = 10


class graphics_WebGLAquarium(graphics_utils.GraphicsTest):
    """WebGL aquarium graphics test."""
    version = 1

    _backlight = None
    _power_status = None
    _service_stopper = None
    _test_power = False
    active_tab = None
    flip_stats = {}
    kernel_sampler = None
    perf_keyval = {}
    sampler_lock = None
    test_duration_secs = 30
    test_setting_num_fishes = 50
    test_settings = {
        50: ('setSetting2', 2),
        1000: ('setSetting6', 6),
    }

    def setup(self):
        """Testcase setup."""
        tarball_path = os.path.join(self.bindir,
                                    'webgl_aquarium_static.tar.bz2')
        utils.extract_tarball_to_dir(tarball_path, self.srcdir)

    def initialize(self):
        """Testcase initialization."""
        super(graphics_WebGLAquarium, self).initialize()
        self.sampler_lock = threading.Lock()
        # TODO: Create samplers for other platforms (e.g. x86).
        if utils.get_board().lower() in ['daisy', 'daisy_spring']:
            # Enable ExynosSampler on Exynos platforms.  The sampler looks for
            # exynos-drm page flip states: 'wait_kds', 'rendered', 'prepared',
            # and 'flipped' in kernel debugfs.

            # Sample 3-second durtaion for every 5 seconds.
            self.kernel_sampler = sampler.ExynosSampler(period=5, duration=3)
            self.kernel_sampler.sampler_callback = self.exynos_sampler_callback
            self.kernel_sampler.output_flip_stats = (
                self.exynos_output_flip_stats)

    def cleanup(self):
        """Testcase cleanup."""
        if self._backlight:
            self._backlight.restore()
        if self._service_stopper:
            self._service_stopper.restore_services()
        super(graphics_WebGLAquarium, self).cleanup()

    def setup_webpage(self, browser, test_url, num_fishes):
        """Open fish tank in a new tab.

        @param browser: The Browser object to run the test with.
        @param test_url: The URL to the aquarium test site.
        @param num_fishes: The number of fishes to run the test with.
        """
        # Create tab and load page. Set the number of fishes when page is fully
        # loaded.
        tab = browser.tabs.New()
        tab.Navigate(test_url)
        tab.Activate()
        self.active_tab = tab
        tab.WaitForDocumentReadyStateToBeComplete()

        # Set the number of fishes when document finishes loading.  Also reset
        # our own FPS counter and start recording FPS and rendering time.
        utils.wait_for_value(
            lambda: tab.EvaluateJavaScript(
                'if (document.readyState === "complete") {'
                '  setSetting(document.getElementById("%s"), %d);'
                '  g_crosFpsCounter.reset();'
                '  true;'
                '} else {'
                '  false;'
                '}' % self.test_settings[num_fishes]
            ),
            expected_value=True,
            timeout_sec=30)

        return tab

    def tear_down_webpage(self):
        """Close the tab containing testing webpage."""
        # Do not close the tab when the sampler_callback is
        # doing its work.
        with self.sampler_lock:
            self.active_tab.Close()
            self.active_tab = None

    def run_fish_test(self, browser, test_url, num_fishes, perf_log=True):
        """Run the test with the given number of fishes.

        @param browser: The Browser object to run the test with.
        @param test_url: The URL to the aquarium test site.
        @param num_fishes: The number of fishes to run the test with.
        @param perf_log: Report perf data only if it's set to True.
        """

        tab = self.setup_webpage(browser, test_url, num_fishes)

        if self.kernel_sampler:
            self.kernel_sampler.start_sampling_thread()
        time.sleep(self.test_duration_secs)
        if self.kernel_sampler:
            self.kernel_sampler.stop_sampling_thread()
            self.kernel_sampler.output_flip_stats('flip_stats_%d' % num_fishes)
            self.flip_stats = {}

        # Get average FPS and rendering time, then close the tab.
        avg_fps = tab.EvaluateJavaScript('g_crosFpsCounter.getAvgFps();')
        if math.isnan(float(avg_fps)):
            raise error.TestFail('Failed: Could not get FPS count.')

        avg_interframe_time = tab.EvaluateJavaScript(
            'g_crosFpsCounter.getAvgInterFrameTime();')
        avg_render_time = tab.EvaluateJavaScript(
            'g_crosFpsCounter.getAvgRenderTime();')
        std_interframe_time = tab.EvaluateJavaScript(
            'g_crosFpsCounter.getStdInterFrameTime();')
        std_render_time = tab.EvaluateJavaScript(
            'g_crosFpsCounter.getStdRenderTime();')
        self.perf_keyval['avg_fps_%04d_fishes' % num_fishes] = avg_fps
        self.perf_keyval['avg_interframe_time_%04d_fishes' % num_fishes] = (
            avg_interframe_time)
        self.perf_keyval['avg_render_time_%04d_fishes' % num_fishes] = (
            avg_render_time)
        self.perf_keyval['std_interframe_time_%04d_fishes' % num_fishes] = (
            std_interframe_time)
        self.perf_keyval['std_render_time_%04d_fishes' % num_fishes] = (
            std_render_time)
        logging.info('%d fish(es): Average FPS = %f, '
                     'average render time = %f', num_fishes, avg_fps,
                     avg_render_time)

        if perf_log:
            # Report frames per second to chromeperf/ dashboard.
            self.output_perf_value(
                description='avg_fps_%04d_fishes' % num_fishes,
                value=avg_fps,
                units='fps',
                higher_is_better=True)

            # Intel only: Record the power consumption for the next few seconds.
            rapl_rate = power_rapl.get_rapl_measurement(
                'rapl_%04d_fishes' % num_fishes)
            # Remove entries that we don't care about.
            rapl_rate = {key: rapl_rate[key]
                         for key in rapl_rate.keys() if key.endswith('pwr')}
            # Report to chromeperf/ dashboard.
            for key, values in rapl_rate.iteritems():
                self.output_perf_value(
                    description=key,
                    value=values,
                    units='W',
                    higher_is_better=False,
                    graph='rapl_power_consumption'
                )

    def run_power_test(self, browser, test_url, ac_ok):
        """Runs the webgl power consumption test and reports the perf results.

        @param browser: The Browser object to run the test with.
        @param test_url: The URL to the aquarium test site.
        @param ac_ok: Boolean on whether its ok to have AC power supplied.
        """

        self._backlight = power_utils.Backlight()
        self._backlight.set_default()

        self._service_stopper = service_stopper.ServiceStopper(
            service_stopper.ServiceStopper.POWER_DRAW_SERVICES)
        self._service_stopper.stop_services()

        if not ac_ok:
            self._power_status = power_status.get_status()
            # Verify that we are running on battery and the battery is
            # sufficiently charged.
            self._power_status.assert_battery_state(BATTERY_INITIAL_CHARGED_MIN)

            measurements = [
                power_status.SystemPower(self._power_status.battery_path)
            ]

        def get_power():
            power_logger = power_status.PowerLogger(measurements)
            power_logger.start()
            time.sleep(STABILIZATION_DURATION)
            start_time = time.time()
            time.sleep(MEASUREMENT_DURATION)
            power_logger.checkpoint('result', start_time)
            keyval = power_logger.calc()
            logging.info('Power output %s', keyval)
            return keyval['result_' + measurements[0].domain + '_pwr']

        self.run_fish_test(browser, test_url, 1000, perf_log=False)
        if not ac_ok:
            energy_rate = get_power()
            # This is a power specific test so we are not capturing
            # avg_fps and avg_render_time in this test.
            self.perf_keyval[POWER_DESCRIPTION] = energy_rate
            self.output_perf_value(
                description=POWER_DESCRIPTION,
                value=energy_rate,
                units='W',
                higher_is_better=False)

    def exynos_sampler_callback(self, sampler_obj):
        """Sampler callback function for ExynosSampler.

        @param sampler_obj: The ExynosSampler object that invokes this callback
                function.
        """
        if sampler_obj.stopped:
            return

        with self.sampler_lock:
            now = time.time()
            results = {}
            info_str = ['\nfb_id wait_kds flipped']
            for value in sampler_obj.frame_buffers.itervalues():
                results[value.fb] = {}
                for state, stats in value.states.iteritems():
                    results[value.fb][state] = (stats.avg, stats.stdev)
                info_str.append('%s: %s %s' % (value.fb,
                                               results[value.fb]['wait_kds'][0],
                                               results[value.fb]['flipped'][0]))
            results['avg_fps'] = self.active_tab.EvaluateJavaScript(
                'g_crosFpsCounter.getAvgFps();')
            results['avg_render_time'] = self.active_tab.EvaluateJavaScript(
                'g_crosFpsCounter.getAvgRenderTime();')
            self.active_tab.ExecuteJavaScript('g_crosFpsCounter.reset();')
            info_str.append('avg_fps: %s, avg_render_time: %s' %
                            (results['avg_fps'], results['avg_render_time']))
            self.flip_stats[now] = results
            logging.info('\n'.join(info_str))

    def exynos_output_flip_stats(self, file_name):
        """Pageflip statistics output function for ExynosSampler.

        @param file_name: The output file name.
        """
        # output format:
        # time fb_id avg_rendered avg_prepared avg_wait_kds avg_flipped
        # std_rendered std_prepared std_wait_kds std_flipped
        with open(file_name, 'w') as f:
            for t in sorted(self.flip_stats.keys()):
                if ('avg_fps' in self.flip_stats[t] and
                        'avg_render_time' in self.flip_stats[t]):
                    f.write('%s %s %s\n' %
                            (t, self.flip_stats[t]['avg_fps'],
                             self.flip_stats[t]['avg_render_time']))
                for fb, stats in self.flip_stats[t].iteritems():
                    if not isinstance(fb, int):
                        continue
                    f.write('%s %s ' % (t, fb))
                    f.write('%s %s %s %s ' % (stats['rendered'][0],
                                              stats['prepared'][0],
                                              stats['wait_kds'][0],
                                              stats['flipped'][0]))
                    f.write('%s %s %s %s\n' % (stats['rendered'][1],
                                               stats['prepared'][1],
                                               stats['wait_kds'][1],
                                               stats['flipped'][1]))

    def write_samples(self, samples, filename):
        """Writes all samples to result dir with the file name "samples'.

        @param samples: A list of all collected samples.
        @param filename: The file name to save under result directory.
        """
        out_file = os.path.join(self.resultsdir, filename)
        with open(out_file, 'w') as f:
            for sample in samples:
                print >> f, sample

    def run_fish_test_with_memory_pressure(
        self, browser, test_url, num_fishes, memory_pressure):
        """Measure fps under memory pressure.

        It measure FPS of WebGL aquarium while adding memory pressure. It runs
        in 2 phases:
          1. Allocate non-swappable memory until |memory_to_reserve_mb| is
          remained. The memory is not accessed after allocated.
          2. Run "active" memory consumer in the background. After allocated,
          Its content is accessed sequentially by page and looped around
          infinitely.
        The second phase is opeared in two possible modes:
          1. "single" mode, which means only one "active" memory consumer. After
          running a single memory consumer with a given memory size, it waits
          for a while to see if system can afford current memory pressure
          (definition here is FPS > 5). If it does, kill current consumer and
          launch another consumer with a larger memory size. The process keeps
          going until system couldn't afford the load.
          2. "multiple"mode. It simply launch memory consumers with a given size
          one by one until system couldn't afford the load (e.g., FPS < 5).
          In "single" mode, CPU load is lighter so we expect swap in/swap out
          rate to be correlated to FPS better. In "multiple" mode, since there
          are multiple busy loop processes, CPU pressure is another significant
          cause of frame drop. Frame drop can happen easily due to busy CPU
          instead of memory pressure.

        @param browser: The Browser object to run the test with.
        @param test_url: The URL to the aquarium test site.
        @param num_fishes: The number of fishes to run the test with.
        @param memory_pressure: Memory pressure parameters.
        """
        consumer_mode = memory_pressure.get('consumer_mode', 'single')
        memory_to_reserve_mb = memory_pressure.get('memory_to_reserve_mb', 500)
        # Empirical number to quickly produce memory pressure.
        if consumer_mode == 'single':
            default_consumer_size_mb = memory_to_reserve_mb + 100
        else:
            default_consumer_size_mb = memory_to_reserve_mb / 2
        consumer_size_mb = memory_pressure.get(
            'consumer_size_mb', default_consumer_size_mb)

        # Setup fish tank.
        self.setup_webpage(browser, test_url, num_fishes)

        # Drop all file caches.
        utils.drop_caches()

        def fps_near_zero(fps_sampler):
            """Returns whether recent fps goes down to near 0.

            @param fps_sampler: A system_sampler.Sampler object.
            """
            last_fps = fps_sampler.get_last_avg_fps(6)
            if last_fps:
                logging.info('last fps %f', last_fps)
                if last_fps <= 5:
                    return True
            return False

        max_allocated_mb = 0
        # Consume free memory and release them by the end.
        with memory_eater.consume_free_memory(memory_to_reserve_mb):
            fps_sampler = system_sampler.SystemSampler(
                memory_eater.MemoryEater.get_active_consumer_pids)
            end_condition = functools.partial(fps_near_zero, fps_sampler)
            with fps_meter.FPSMeter(fps_sampler.sample):
                # Collects some samples before running memory pressure.
                time.sleep(5)
                try:
                    if consumer_mode == 'single':
                        # A single run couldn't generate samples representative
                        # enough.
                        # First runs squeeze more inactive anonymous memory into
                        # zram so in later runs we have a more stable memory
                        # stat.
                        max_allocated_mb = max(
                            memory_eater.run_single_memory_pressure(
                                consumer_size_mb, 100, end_condition, 10, 3,
                                900),
                            memory_eater.run_single_memory_pressure(
                                consumer_size_mb, 20, end_condition, 10, 3,
                                900),
                            memory_eater.run_single_memory_pressure(
                                consumer_size_mb, 10, end_condition, 10, 3,
                                900))
                    elif consumer_mode == 'multiple':
                        max_allocated_mb = (
                            memory_eater.run_multi_memory_pressure(
                                consumer_size_mb, end_condition, 10, 900))
                    else:
                        raise error.TestFail(
                            'Failed: Unsupported consumer mode.')
                except memory_eater.TimeoutException as e:
                    raise error.TestFail(e)

        samples = fps_sampler.get_samples()
        self.write_samples(samples, 'memory_pressure_fps_samples.txt')

        self.perf_keyval['num_samples'] = len(samples)
        self.perf_keyval['max_allocated_mb'] = max_allocated_mb

        logging.info(self.perf_keyval)

        self.output_perf_value(
            description='max_allocated_mb_%d_fishes_reserved_%d_mb' % (
                num_fishes, memory_to_reserve_mb),
            value=max_allocated_mb,
            units='MB',
            higher_is_better=True)


    @graphics_utils.GraphicsTest.failure_report_decorator('graphics_WebGLAquarium')
    def run_once(self,
                 test_duration_secs=30,
                 test_setting_num_fishes=(50, 1000),
                 power_test=False,
                 ac_ok=False,
                 memory_pressure=None):
        """Find a browser with telemetry, and run the test.

        @param test_duration_secs: The duration in seconds to run each scenario
                for.
        @param test_setting_num_fishes: A list of the numbers of fishes to
                enable in the test.
        @param power_test: Boolean on whether to run power_test
        @param ac_ok: Boolean on whether its ok to have AC power supplied.
        @param memory_pressure: A dictionay which specifies memory pressure
                parameters:
                'consumer_mode': 'single' or 'multiple' to have one or moultiple
                concurrent memory consumers.
                'consumer_size_mb': Amount of memory to allocate. In 'single'
                mode, a single memory consumer would allocate memory by the
                specific size. It then gradually allocates more memory until
                FPS down to near 0. In 'multiple' mode, memory consumers of
                this size would be spawn one by one until FPS down to near 0.
                'memory_to_reserve_mb': Amount of memory to reserve before
                running memory consumer. In practical we allocate mlocked
                memory (i.e., not swappable) to consume free memory until this
                amount of free memory remained.
        """
        self.test_duration_secs = test_duration_secs
        self.test_setting_num_fishes = test_setting_num_fishes
        pc_error_reason = None

        with chrome.Chrome(logged_in=False, init_network_controller=True) as cr:
            cr.browser.platform.SetHTTPServerDirectories(self.srcdir)
            test_url = cr.browser.platform.http_server.UrlOf(
                os.path.join(self.srcdir, 'aquarium.html'))

            utils.report_temperature(self, 'temperature_1_start')
            # Wrap the test run inside of a PerfControl instance to make machine
            # behavior more consistent.
            with perf.PerfControl() as pc:
                if not pc.verify_is_valid():
                    raise error.TestFail('Failed: %s' % pc.get_error_reason())
                utils.report_temperature(self, 'temperature_2_before_test')

                if memory_pressure:
                    self.run_fish_test_with_memory_pressure(
                        cr.browser, test_url, num_fishes=1000,
                        memory_pressure=memory_pressure)
                    self.tear_down_webpage()
                elif power_test:
                    self._test_power = True
                    self.run_power_test(cr.browser, test_url, ac_ok)
                    self.tear_down_webpage()
                else:
                    for n in self.test_setting_num_fishes:
                        self.run_fish_test(cr.browser, test_url, n)
                        self.tear_down_webpage()

                if not pc.verify_is_valid():
                    # Defer error handling until after perf report.
                    pc_error_reason = pc.get_error_reason()

        utils.report_temperature(self, 'temperature_3_after_test')
        self.write_perf_keyval(self.perf_keyval)

        if pc_error_reason:
            raise error.TestWarn('Warning: %s' % pc_error_reason)
