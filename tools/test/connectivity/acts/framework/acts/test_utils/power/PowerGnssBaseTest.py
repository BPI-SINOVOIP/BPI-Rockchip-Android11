#!/usr/bin/env python3
#
#   Copyright 2019 - The Android Open Source Project
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

import logging
import time

import os

import acts.test_utils.power.PowerBaseTest as PBT

from acts import base_test
from acts.controllers import monsoon
from bokeh.layouts import column, layout
from bokeh.models import CustomJS, ColumnDataSource
from bokeh.models import tools as bokeh_tools
from bokeh.models.widgets import DataTable, TableColumn
from bokeh.plotting import figure, output_file, save
from acts.controllers.monsoon_lib.api.common import PassthroughStates

LOGTIME_RETRY_COUNT = 3
RESET_BATTERY_STATS = 'dumpsys batterystats --reset'
RECOVER_MONSOON_RETRY_COUNT = 3
MONSOON_RETRY_INTERVAL = 300

class PowerGnssBaseTest(PBT.PowerBaseTest):
    """
    Base Class for all GNSS Power related tests
    """

    def setup_class(self):
        super().setup_class()
        req_params = ['customjsfile', 'maskfile']
        self.unpack_userparams(req_params)

    def collect_power_data(self):
        """Measure power and plot."""
        result = super().collect_power_data()
        self.monsoon_data_plot_power(self.mon_info, result, tag='_Power')
        return result

    def monsoon_data_plot_power(self, mon_info, monsoon_results, tag=''):
        """Plot the monsoon power data using bokeh interactive plotting tool.

        Args:
            mon_info: Dictionary with the monsoon packet config.
            monsoon_results: a MonsoonResult or list of MonsoonResult objects to
                             to plot.
            tag: an extra tag to append to the resulting filename.

        """

        if not isinstance(monsoon_results, list):
            monsoon_results = [monsoon_results]
        logging.info('Plotting the power measurement data.')

        voltage = monsoon_results[0].voltage

        total_current = 0
        total_samples = 0
        for result in monsoon_results:
            total_current += result.average_current * result.num_samples
            total_samples += result.num_samples
        avg_current = total_current / total_samples

        time_relative = [
            data_point.time
            for monsoon_result in monsoon_results
            for data_point in monsoon_result.get_data_points()
        ]

        power_data = [
            data_point.current * voltage
            for monsoon_result in monsoon_results
            for data_point in monsoon_result.get_data_points()
        ]

        total_data_points = sum(
            result.num_samples for result in monsoon_results)
        color = ['navy'] * total_data_points

        # Preparing the data and source link for bokehn java callback
        source = ColumnDataSource(
            data=dict(x0=time_relative, y0=power_data, color=color))
        s2 = ColumnDataSource(
            data=dict(
                z0=[mon_info.duration],
                y0=[round(avg_current, 2)],
                x0=[round(avg_current * voltage, 2)],
                z1=[round(avg_current * voltage * mon_info.duration, 2)],
                z2=[round(avg_current * mon_info.duration, 2)]))
        # Setting up data table for the output
        columns = [
            TableColumn(field='z0', title='Total Duration (s)'),
            TableColumn(field='y0', title='Average Current (mA)'),
            TableColumn(field='x0', title='Average Power (4.2v) (mW)'),
            TableColumn(field='z1', title='Average Energy (mW*s)'),
            TableColumn(field='z2', title='Normalized Average Energy (mA*s)')
        ]
        dt = DataTable(
            source=s2, columns=columns, width=1300, height=60, editable=True)

        plot_title = (os.path.basename(
            os.path.splitext(monsoon_results[0].tag)[0])
                      + tag)
        output_file(os.path.join(mon_info.data_path, plot_title + '.html'))
        tools = 'box_zoom,box_select,pan,crosshair,redo,undo,reset,hover,save'
        # Create a new plot with the datatable above
        plot = figure(
            plot_width=1300,
            plot_height=700,
            title=plot_title,
            tools=tools,
            output_backend='webgl')
        plot.add_tools(bokeh_tools.WheelZoomTool(dimensions='width'))
        plot.add_tools(bokeh_tools.WheelZoomTool(dimensions='height'))
        plot.line('x0', 'y0', source=source, line_width=2)
        plot.circle('x0', 'y0', source=source, size=0.5, fill_color='color')
        plot.xaxis.axis_label = 'Time (s)'
        plot.yaxis.axis_label = 'Power (mW)'
        plot.title.text_font_size = {'value': '15pt'}
        jsscript = open(self.customjsfile, 'r')
        customjsscript = jsscript.read()
        # Callback Java scripting
        source.callback = CustomJS(
            args=dict(mytable=dt),
            code=customjsscript)

        # Layout the plot and the datatable bar
        save(layout([[dt], [plot]]))

    def disconnect_usb(self, ad, sleeptime):
        """Disconnect usb while device is on sleep and
        connect the usb again once the sleep time completes

        sleeptime: sleep time where dut is disconnected from usb
        """
        self.dut.adb.shell(RESET_BATTERY_STATS)
        time.sleep(1)
        for _ in range(LOGTIME_RETRY_COUNT):
            self.mon_info.dut.usb(PassthroughStates.OFF)
            if not ad.is_connected():
                time.sleep(sleeptime)
                self.mon_info.dut.usb(PassthroughStates.ON)
                break
        else:
            self.log.error('Test failed after maximum retry')
            for _ in range(RECOVER_MONSOON_RETRY_COUNT):
                if self.monsoon_recover():
                    break
                else:
                    self.log.warning(
                        'Wait for {} second then try again'.format(
                            MONSOON_RETRY_INTERVAL))
                    time.sleep(MONSOON_RETRY_INTERVAL)
            else:
                self.log.error('Failed to recover monsoon')
