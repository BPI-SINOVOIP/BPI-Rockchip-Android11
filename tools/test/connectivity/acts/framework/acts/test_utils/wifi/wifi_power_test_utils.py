#!/usr/bin/env python3
#
#   Copyright 2017 Google, Inc.
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
from acts import utils
from acts.libs.proc import job
from acts.controllers.ap_lib import bridge_interface as bi
from acts.test_utils.wifi import wifi_test_utils as wutils
from bokeh.layouts import column, layout
from bokeh.models import CustomJS, ColumnDataSource
from bokeh.models import tools as bokeh_tools
from bokeh.models.widgets import DataTable, TableColumn
from bokeh.plotting import figure, output_file, save
from acts.controllers.ap_lib import hostapd_security
from acts.controllers.ap_lib import hostapd_ap_preset

# http://www.secdev.org/projects/scapy/
# On ubuntu, sudo pip3 install scapy
import scapy.all as scapy

GET_FROM_PHONE = 'get_from_dut'
GET_FROM_AP = 'get_from_ap'
ENABLED_MODULATED_DTIM = 'gEnableModulatedDTIM='
MAX_MODULATED_DTIM = 'gMaxLIModulatedDTIM='


def monsoon_data_plot(mon_info, monsoon_results, tag=''):
    """Plot the monsoon current data using bokeh interactive plotting tool.

    Plotting power measurement data with bokeh to generate interactive plots.
    You can do interactive data analysis on the plot after generating with the
    provided widgets, which make the debugging much easier. To realize that,
    bokeh callback java scripting is used. View a sample html output file:
    https://drive.google.com/open?id=0Bwp8Cq841VnpT2dGUUxLYWZvVjA

    Args:
        mon_info: obj with information of monsoon measurement, including
            monsoon device object, measurement frequency, duration, etc.
        monsoon_results: a MonsoonResult or list of MonsoonResult objects to
                         to plot.
        tag: an extra tag to append to the resulting filename.

    Returns:
        plot: the plotting object of bokeh, optional, will be needed if multiple
           plots will be combined to one html file.
        dt: the datatable object of bokeh, optional, will be needed if multiple
           datatables will be combined to one html file.
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

    current_data = [
        data_point.current * 1000
        for monsoon_result in monsoon_results
        for data_point in monsoon_result.get_data_points()
    ]

    total_data_points = sum(result.num_samples for result in monsoon_results)
    color = ['navy'] * total_data_points

    # Preparing the data and source link for bokehn java callback
    source = ColumnDataSource(
        data=dict(x0=time_relative, y0=current_data, color=color))
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

    plot_title = (os.path.basename(os.path.splitext(monsoon_results[0].tag)[0])
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
    plot.yaxis.axis_label = 'Current (mA)'
    plot.title.text_font_size = {'value': '15pt'}

    # Callback JavaScript
    source.selected.js_on_change(
        "indices",
        CustomJS(args=dict(source=source, mytable=dt), code="""
    var inds = cb_obj.indices;
    var d1 = source.data;
    var d2 = mytable.source.data;
    ym = 0
    ts = 0
    d2['x0'] = []
    d2['y0'] = []
    d2['z1'] = []
    d2['z2'] = []
    d2['z0'] = []
    min=max=d1['x0'][inds[0]]
    if (inds.length==0) {return;}
    for (i = 0; i < inds.length; i++) {
    ym += d1['y0'][inds[i]]
    d1['color'][inds[i]] = "red"
    if (d1['x0'][inds[i]] < min) {
      min = d1['x0'][inds[i]]}
    if (d1['x0'][inds[i]] > max) {
      max = d1['x0'][inds[i]]}
    }
    ym /= inds.length
    ts = max - min
    dx0 = Math.round(ym*4.2*100.0)/100.0
    dy0 = Math.round(ym*100.0)/100.0
    dz1 = Math.round(ym*4.2*ts*100.0)/100.0
    dz2 = Math.round(ym*ts*100.0)/100.0
    dz0 = Math.round(ts*1000.0)/1000.0
    d2['z0'].push(dz0)
    d2['x0'].push(dx0)
    d2['y0'].push(dy0)
    d2['z1'].push(dz1)
    d2['z2'].push(dz2)
    mytable.change.emit();
    """))

    # Layout the plot and the datatable bar
    save(layout([[dt], [plot]]))
    return plot, dt


def change_dtim(ad, gEnableModulatedDTIM, gMaxLIModulatedDTIM=10):
    """Function to change the DTIM setting in the phone.

    Args:
        ad: the target android device, AndroidDevice object
        gEnableModulatedDTIM: Modulated DTIM, int
        gMaxLIModulatedDTIM: Maximum modulated DTIM, int
    """
    # First trying to find the ini file with DTIM settings
    ini_file_phone = ad.adb.shell('ls /vendor/firmware/wlan/*/*.ini')
    ini_file_local = ini_file_phone.split('/')[-1]

    # Pull the file and change the DTIM to desired value
    ad.adb.pull('{} {}'.format(ini_file_phone, ini_file_local))

    with open(ini_file_local, 'r') as fin:
        for line in fin:
            if ENABLED_MODULATED_DTIM in line:
                gE_old = line.strip('\n')
                gEDTIM_old = line.strip(ENABLED_MODULATED_DTIM).strip('\n')
            if MAX_MODULATED_DTIM in line:
                gM_old = line.strip('\n')
                gMDTIM_old = line.strip(MAX_MODULATED_DTIM).strip('\n')
    fin.close()
    if int(gEDTIM_old) == gEnableModulatedDTIM and int(
            gMDTIM_old) == gMaxLIModulatedDTIM:
        ad.log.info('Current DTIM is already the desired value,'
                    'no need to reset it')
        return 0

    gE_new = ENABLED_MODULATED_DTIM + str(gEnableModulatedDTIM)
    gM_new = MAX_MODULATED_DTIM + str(gMaxLIModulatedDTIM)

    sed_gE = 'sed -i \'s/{}/{}/g\' {}'.format(gE_old, gE_new, ini_file_local)
    sed_gM = 'sed -i \'s/{}/{}/g\' {}'.format(gM_old, gM_new, ini_file_local)
    job.run(sed_gE)
    job.run(sed_gM)

    # Push the file to the phone
    push_file_to_phone(ad, ini_file_local, ini_file_phone)
    ad.log.info('DTIM changes checked in and rebooting...')
    ad.reboot()
    # Wait for auto-wifi feature to start
    time.sleep(20)
    ad.adb.shell('dumpsys battery set level 100')
    ad.log.info('DTIM updated and device back from reboot')
    return 1


def push_file_to_phone(ad, file_local, file_phone):
    """Function to push local file to android phone.

    Args:
        ad: the target android device
        file_local: the locla file to push
        file_phone: the file/directory on the phone to be pushed
    """
    ad.adb.root()
    cmd_out = ad.adb.remount()
    if 'Permission denied' in cmd_out:
        ad.log.info('Need to disable verity first and reboot')
        ad.adb.disable_verity()
        time.sleep(1)
        ad.reboot()
        ad.log.info('Verity disabled and device back from reboot')
        ad.adb.root()
        ad.adb.remount()
    time.sleep(1)
    ad.adb.push('{} {}'.format(file_local, file_phone))


def ap_setup(ap, network, bandwidth=80):
    """Set up the whirlwind AP with provided network info.

    Args:
        ap: access_point object of the AP
        network: dict with information of the network, including ssid, password
                 bssid, channel etc.
        bandwidth: the operation bandwidth for the AP, default 80MHz
    Returns:
        brconfigs: the bridge interface configs
    """
    log = logging.getLogger()
    bss_settings = []
    ssid = network[wutils.WifiEnums.SSID_KEY]
    if "password" in network.keys():
        password = network["password"]
        security = hostapd_security.Security(
            security_mode="wpa", password=password)
    else:
        security = hostapd_security.Security(security_mode=None, password=None)
    channel = network["channel"]
    config = hostapd_ap_preset.create_ap_preset(
        channel=channel,
        ssid=ssid,
        security=security,
        bss_settings=bss_settings,
        vht_bandwidth=bandwidth,
        profile_name='whirlwind',
        iface_wlan_2g=ap.wlan_2g,
        iface_wlan_5g=ap.wlan_5g)
    config_bridge = ap.generate_bridge_configs(channel)
    brconfigs = bi.BridgeInterfaceConfigs(config_bridge[0], config_bridge[1],
                                          config_bridge[2])
    ap.bridge.startup(brconfigs)
    ap.start_ap(config)
    log.info("AP started on channel {} with SSID {}".format(channel, ssid))
    return brconfigs


def bokeh_plot(data_sets,
               legends,
               fig_property,
               shaded_region=None,
               output_file_path=None):
    """Plot bokeh figs.
        Args:
            data_sets: data sets including lists of x_data and lists of y_data
                       ex: [[[x_data1], [x_data2]], [[y_data1],[y_data2]]]
            legends: list of legend for each curve
            fig_property: dict containing the plot property, including title,
                      lables, linewidth, circle size, etc.
            shaded_region: optional dict containing data for plot shading
            output_file_path: optional path at which to save figure
        Returns:
            plot: bokeh plot figure object
    """
    tools = 'box_zoom,box_select,pan,crosshair,redo,undo,reset,hover,save'
    plot = figure(
        plot_width=1300,
        plot_height=700,
        title=fig_property['title'],
        tools=tools,
        output_backend="webgl")
    plot.add_tools(bokeh_tools.WheelZoomTool(dimensions="width"))
    plot.add_tools(bokeh_tools.WheelZoomTool(dimensions="height"))
    colors = [
        'red', 'green', 'blue', 'olive', 'orange', 'salmon', 'black', 'navy',
        'yellow', 'darkred', 'goldenrod'
    ]
    if shaded_region:
        band_x = shaded_region["x_vector"]
        band_x.extend(shaded_region["x_vector"][::-1])
        band_y = shaded_region["lower_limit"]
        band_y.extend(shaded_region["upper_limit"][::-1])
        plot.patch(
            band_x, band_y, color='#7570B3', line_alpha=0.1, fill_alpha=0.1)

    for x_data, y_data, legend in zip(data_sets[0], data_sets[1], legends):
        index_now = legends.index(legend)
        color = colors[index_now % len(colors)]
        plot.line(
            x_data,
            y_data,
            legend=str(legend),
            line_width=fig_property['linewidth'],
            color=color)
        plot.circle(
            x_data,
            y_data,
            size=fig_property['markersize'],
            legend=str(legend),
            fill_color=color)

    # Plot properties
    plot.xaxis.axis_label = fig_property['x_label']
    plot.yaxis.axis_label = fig_property['y_label']
    plot.legend.location = "top_right"
    plot.legend.click_policy = "hide"
    plot.title.text_font_size = {'value': '15pt'}
    if output_file_path is not None:
        output_file(output_file_path)
        save(plot)
    return plot


def save_bokeh_plots(plot_array, output_file_path):
    all_plots = column(children=plot_array)
    output_file(output_file_path)
    save(all_plots)


def run_iperf_client_nonblocking(ad, server_host, extra_args=""):
    """Start iperf client on the device with nohup.

    Return status as true if iperf client start successfully.
    And data flow information as results.

    Args:
        ad: the android device under test
        server_host: Address of the iperf server.
        extra_args: A string representing extra arguments for iperf client,
            e.g. "-i 1 -t 30".

    """
    log = logging.getLogger()
    ad.adb.shell_nb("nohup >/dev/null 2>&1 sh -c 'iperf3 -c {} {} &'".format(
        server_host, extra_args))
    log.info("IPerf client started")


def get_wifi_rssi(ad):
    """Get the RSSI of the device.

    Args:
        ad: the android device under test
    Returns:
        RSSI: the rssi level of the device
    """
    RSSI = ad.droid.wifiGetConnectionInfo()['rssi']
    return RSSI


def get_phone_ip(ad):
    """Get the WiFi IP address of the phone.

    Args:
        ad: the android device under test
    Returns:
        IP: IP address of the phone for WiFi, as a string
    """
    IP = ad.droid.connectivityGetIPv4Addresses('wlan0')[0]

    return IP


def get_phone_mac(ad):
    """Get the WiFi MAC address of the phone.

    Args:
        ad: the android device under test
    Returns:
        mac: MAC address of the phone for WiFi, as a string
    """
    mac = ad.droid.wifiGetConnectionInfo()["mac_address"]

    return mac


def get_phone_ipv6(ad):
    """Get the WiFi IPV6 address of the phone.

    Args:
        ad: the android device under test
    Returns:
        IPv6: IPv6 address of the phone for WiFi, as a string
    """
    IPv6 = ad.droid.connectivityGetLinkLocalIpv6Address('wlan0')[:-6]

    return IPv6


def get_if_addr6(intf, address_type):
    """Returns the Ipv6 address from a given local interface.

    Returns the desired IPv6 address from the interface 'intf' in human
    readable form. The address type is indicated by the IPv6 constants like
    IPV6_ADDR_LINKLOCAL, IPV6_ADDR_GLOBAL, etc. If no address is found,
    None is returned.

    Args:
        intf: desired interface name
        address_type: addrees typle like LINKLOCAL or GLOBAL

    Returns:
        Ipv6 address of the specified interface in human readable format
    """
    for if_list in scapy.in6_getifaddr():
        if if_list[2] == intf and if_list[1] == address_type:
            return if_list[0]

    return None


def wait_for_dhcp(interface_name):
    """Wait the DHCP address assigned to desired interface.

    Getting DHCP address takes time and the wait time isn't constant. Utilizing
    utils.timeout to keep trying until success

    Args:
        interface_name: desired interface name
    Returns:
        ip: ip address of the desired interface name
    Raise:
        TimeoutError: After timeout, if no DHCP assigned, raise
    """
    log = logging.getLogger()
    reset_host_interface(interface_name)
    start_time = time.time()
    time_limit_seconds = 60
    ip = '0.0.0.0'
    while start_time + time_limit_seconds > time.time():
        ip = scapy.get_if_addr(interface_name)
        if ip == '0.0.0.0':
            time.sleep(1)
        else:
            log.info(
                'DHCP address assigned to %s as %s' % (interface_name, ip))
            return ip
    raise TimeoutError('Timed out while getting if_addr after %s seconds.' %
                       time_limit_seconds)


def reset_host_interface(intferface_name):
    """Reset the host interface.

    Args:
        intferface_name: the desired interface to reset
    """
    log = logging.getLogger()
    intf_down_cmd = 'ifconfig %s down' % intferface_name
    intf_up_cmd = 'ifconfig %s up' % intferface_name
    try:
        job.run(intf_down_cmd)
        time.sleep(10)
        job.run(intf_up_cmd)
        log.info('{} has been reset'.format(intferface_name))
    except job.Error:
        raise Exception('No such interface')


def bringdown_host_interface(intferface_name):
    """Reset the host interface.

    Args:
        intferface_name: the desired interface to reset
    """
    log = logging.getLogger()
    intf_down_cmd = 'ifconfig %s down' % intferface_name
    try:
        job.run(intf_down_cmd)
        time.sleep(2)
        log.info('{} has been brought down'.format(intferface_name))
    except job.Error:
        raise Exception('No such interface')


def create_pkt_config(test_class):
    """Creates the config for generating multicast packets

    Args:
        test_class: object with all networking paramters

    Returns:
        Dictionary with the multicast packet config
    """
    addr_type = (scapy.IPV6_ADDR_LINKLOCAL
                 if test_class.ipv6_src_type == 'LINK_LOCAL' else
                 scapy.IPV6_ADDR_GLOBAL)

    mac_dst = test_class.mac_dst
    if GET_FROM_PHONE in test_class.mac_dst:
        mac_dst = get_phone_mac(test_class.dut)

    ipv4_dst = test_class.ipv4_dst
    if GET_FROM_PHONE in test_class.ipv4_dst:
        ipv4_dst = get_phone_ip(test_class.dut)

    ipv6_dst = test_class.ipv6_dst
    if GET_FROM_PHONE in test_class.ipv6_dst:
        ipv6_dst = get_phone_ipv6(test_class.dut)

    ipv4_gw = test_class.ipv4_gwt
    if GET_FROM_AP in test_class.ipv4_gwt:
        ipv4_gw = test_class.access_point.ssh_settings.hostname

    pkt_gen_config = {
        'interf': test_class.pkt_sender.interface,
        'subnet_mask': test_class.sub_mask,
        'src_mac': test_class.mac_src,
        'dst_mac': mac_dst,
        'src_ipv4': test_class.ipv4_src,
        'dst_ipv4': ipv4_dst,
        'src_ipv6': test_class.ipv6_src,
        'src_ipv6_type': addr_type,
        'dst_ipv6': ipv6_dst,
        'gw_ipv4': ipv4_gw
    }
    return pkt_gen_config
