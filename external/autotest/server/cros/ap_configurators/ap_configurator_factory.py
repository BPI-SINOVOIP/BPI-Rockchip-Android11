# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""File containing class to build all available ap_configurators."""

import logging

import common
from autotest_lib.client.common_lib.cros.network import ap_constants
from autotest_lib.server import site_utils
from autotest_lib.server.cros import ap_config
from autotest_lib.server.cros.ap_configurators import ap_cartridge
from autotest_lib.server.cros.ap_configurators import ap_spec
from autotest_lib.server.cros.ap_configurators import static_ap_configurator
from autotest_lib.server.cros.dynamic_suite import frontend_wrappers


class APConfiguratorFactory(object):
    """Class that instantiates all available APConfigurators.

    @attribute CONFIGURATOR_MAP: a dict of strings, mapping to model-specific
                                 APConfigurator objects.
    @attribute BANDS: a string, bands supported by an AP.
    @attribute MODES: a string, 802.11 modes supported by an AP.
    @attribute SECURITIES: a string, security methods supported by an AP.
    @attribute HOSTNAMES: a string, AP hostname.
    @attribute ap_list: a list of APConfigurator objects.
    @attribute ap_config: an APConfiguratorConfig object.
    """


    BANDS = 'bands'
    MODES = 'modes'
    SECURITIES = 'securities'
    HOSTNAMES = 'hostnames'


    def __init__(self, ap_test_type, spec=None):
        webdriver_ready = False
        self.ap_list = []
        self.test_type = ap_test_type
        for ap in ap_config.get_ap_list():
            self.ap_list.append(static_ap_configurator.StaticAPConfigurator(ap))


    def _get_aps_by_visibility(self, visible=True):
        """Returns all configurators that support setting visibility.

        @param visibility = True if SSID should be visible; False otherwise.

        @returns aps: a set of APConfigurators"""
        if visible:
            return set(self.ap_list)

        return set(filter(lambda ap: ap.is_visibility_supported(),
                          self.ap_list))


    def _get_aps_by_mode(self, band, mode):
        """Returns all configurators that support a given 802.11 mode.

        @param band: an 802.11 band.
        @param mode: an 802.11 modes.

        @returns aps: a set of APConfigurators.
        """
        if not mode:
            return set(self.ap_list)

        aps = []
        for ap in self.ap_list:
            modes = ap.get_supported_modes()
            for d in modes:
                if d['band'] == band and mode in d['modes']:
                    aps.append(ap)
        return set(aps)


    def _get_aps_by_security(self, security):
        """Returns all configurators that support a given security mode.

        @param security: the security type

        @returns aps: a set of APConfigurators.
        """

        if not security:
            return set(self.ap_list)

        aps = []
        for ap in self.ap_list:
            if ap.is_security_mode_supported(security):
                aps.append(ap)
        return set(aps)


    def _get_aps_by_band(self, band, channel=None):
        """Returns all APs that support a given band.

        @param band: the band desired.

        @returns aps: a set of APConfigurators.
        """
        if not band:
            return set(self.ap_list)

        aps = []
        for ap in self.ap_list:
            bands_and_channels = ap.get_supported_bands()
            for d in bands_and_channels:
                if channel:
                    if d['band'] == band and channel in d['channels']:
                        aps.append(ap)
                elif d['band'] == band:
                    aps.append(ap)
        return set(aps)


    def get_aps_by_hostnames(self, hostnames, ap_list=None):
        """Returns specific APs by host name.

        @param hostnames: a list of strings, AP's wan_hostname defined in the AP
                          configuration file.
        @param ap_list: a list of APConfigurator objects.

        @return a list of APConfigurators.
        """
        if ap_list == None:
            ap_list = self.ap_list

        aps = []
        for ap in ap_list:
            if ap.host_name in hostnames:
                logging.info('Found AP by hostname %s', ap.host_name)
                aps.append(ap)

        return aps


    def _get_aps_by_configurator_type(self, configurator_type, ap_list):
        """Returns APs that match the given configurator type.

        @param configurator_type: the type of configurtor to return.
        @param ap_list: a list of APConfigurator objects.

        @return a list of APConfigurators.
        """
        aps = []
        for ap in ap_list:
            if ap.configurator_type == configurator_type:
                aps.append(ap)

        return aps


    def get_ap_configurators_by_spec(self, spec=None, pre_configure=False):
        """Returns available configurators meeting spec.

        @param spec: a validated ap_spec object
        @param pre_configure: boolean, True to set all of the configuration
                              options for the APConfigurator object using the
                              given ap_spec; False otherwise.  An ap_spec must
                              be passed for this to have any effect.
        @returns aps: a list of APConfigurator objects
        """
        if not spec:
            return self.ap_list

        # APSpec matching is exact. With AFE deprecated and AP's lock mechanism
        # in datastore, there is no need to check spec.hostnames by location.
        # If a hostname is passed, the capabilities of a given configurator
        # match everything in the APSpec. This helps to prevent failures during
        # the pre-scan phase.
        aps = self._get_aps_by_band(spec.band, channel=spec.channel)
        aps &= self._get_aps_by_mode(spec.band, spec.mode)
        aps &= self._get_aps_by_security(spec.security)
        aps &= self._get_aps_by_visibility(spec.visible)
        matching_aps = list(aps)

        if spec.configurator_type != ap_spec.CONFIGURATOR_ANY:
            matching_aps = self._get_aps_by_configurator_type(
                           spec.configurator_type, matching_aps)
        if spec.hostnames is not None:
            matching_aps = self.get_aps_by_hostnames(spec.hostnames,
                                                     ap_list=matching_aps)
        if pre_configure:
            for ap in matching_aps:
                ap.set_using_ap_spec(spec)
        return matching_aps


    def turn_off_all_routers(self, broken_pdus):
        """Powers down all of the routers.

        @param broken_pdus: list of bad/offline PDUs.
        """
        ap_power_cartridge = ap_cartridge.APCartridge()
        for ap in self.ap_list:
            ap.power_down_router()
            ap_power_cartridge.push_configurator(ap)
        ap_power_cartridge.run_configurators(broken_pdus)
