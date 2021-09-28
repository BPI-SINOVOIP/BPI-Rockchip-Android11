# Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""A collection of context managers for working with shill objects."""

import dbus
import logging

from contextlib import contextmanager

from autotest_lib.client.common_lib import utils
from autotest_lib.client.cros.networking import shill_proxy
from autotest_lib.client.cros.power import sys_power

class ContextError(Exception):
    """An error raised by a context managers dealing with shill objects."""
    pass


class AllowedTechnologiesContext(object):
    """A context manager for allowing only specified technologies in shill.

    Usage:
        # Suppose both 'wifi' and 'cellular' technology are originally enabled.
        allowed = [shill_proxy.ShillProxy.TECHNOLOGY_CELLULAR]
        with AllowedTechnologiesContext(allowed):
            # Within this context, only the 'cellular' technology is allowed to
            # be enabled. The 'wifi' technology is temporarily prohibited and
            # disabled until after the context ends.

    """

    def __init__(self, allowed):
        self._allowed = set(allowed)


    def __enter__(self):
        shill = shill_proxy.ShillProxy.get_proxy()

        # The EnabledTechologies property is an array of strings of technology
        # identifiers.
        enabled = shill.get_dbus_property(
                shill.manager,
                shill_proxy.ShillProxy.MANAGER_PROPERTY_ENABLED_TECHNOLOGIES)
        self._originally_enabled = set(enabled)

        # The ProhibitedTechnologies property is a comma-separated string of
        # technology identifiers.
        prohibited_csv = shill.get_dbus_property(
                shill.manager,
                shill_proxy.ShillProxy.MANAGER_PROPERTY_PROHIBITED_TECHNOLOGIES)
        prohibited = prohibited_csv.split(',') if prohibited_csv else []
        self._originally_prohibited = set(prohibited)

        prohibited = ((self._originally_prohibited | self._originally_enabled)
                      - self._allowed)
        prohibited_csv = ','.join(prohibited)

        logging.debug('Allowed technologies = [%s]', ','.join(self._allowed))
        logging.debug('Originally enabled technologies = [%s]',
                      ','.join(self._originally_enabled))
        logging.debug('Originally prohibited technologies = [%s]',
                      ','.join(self._originally_prohibited))
        logging.debug('To be prohibited technologies = [%s]',
                      ','.join(prohibited))

        # Setting the ProhibitedTechnologies property will disable those
        # prohibited technologies.
        shill.set_dbus_property(
                shill.manager,
                shill_proxy.ShillProxy.MANAGER_PROPERTY_PROHIBITED_TECHNOLOGIES,
                prohibited_csv)

        return self


    def __exit__(self, exc_type, exc_value, traceback):
        shill = shill_proxy.ShillProxy.get_proxy()

        prohibited_csv = ','.join(self._originally_prohibited)
        shill.set_dbus_property(
                shill.manager,
                shill_proxy.ShillProxy.MANAGER_PROPERTY_PROHIBITED_TECHNOLOGIES,
                prohibited_csv)

        # Re-enable originally enabled technologies as they may have been
        # disabled.
        enabled = shill.get_dbus_property(
                shill.manager,
                shill_proxy.ShillProxy.MANAGER_PROPERTY_ENABLED_TECHNOLOGIES)
        to_be_reenabled = self._originally_enabled - set(enabled)
        for technology in to_be_reenabled:
            shill.manager.EnableTechnology(technology)

        return False


class ServiceAutoConnectContext(object):
    """A context manager for overriding a service's 'AutoConnect' property.

    As the service object of the same service may change during the lifetime
    of the context, this context manager does not take a service object at
    construction. Instead, it takes a |get_service| function at construction,
    which it invokes to obtain a service object when entering and exiting the
    context. It is assumed that |get_service| always returns a service object
    that refers to the same service.

    Usage:
        def get_service():
            # Some function that returns a service object.

        with ServiceAutoConnectContext(get_service, False):
            # Within this context, the 'AutoConnect' property of the service
            # returned by |get_service| is temporarily set to False if it's
            # initially set to True. The property is restored to its initial
            # value after the context ends.

    """
    def __init__(self, get_service, autoconnect):
        self._get_service = get_service
        self._autoconnect = autoconnect
        self._initial_autoconnect = None


    def __enter__(self):
        service = self._get_service()
        if service is None:
            raise ContextError('Could not obtain a service object.')

        # Always set the AutoConnect property even if the requested value
        # is the same so that shill will retain the AutoConnect property, else
        # shill may override it.
        service_properties = service.GetProperties()
        self._initial_autoconnect = shill_proxy.ShillProxy.dbus2primitive(
            service_properties[
                shill_proxy.ShillProxy.SERVICE_PROPERTY_AUTOCONNECT])
        logging.info('ServiceAutoConnectContext: change autoconnect to %s',
                     self._autoconnect)
        service.SetProperty(
            shill_proxy.ShillProxy.SERVICE_PROPERTY_AUTOCONNECT,
            self._autoconnect)

        # Make sure the cellular service gets persisted by taking it out of
        # the ephemeral profile.
        if not service_properties[
                shill_proxy.ShillProxy.SERVICE_PROPERTY_PROFILE]:
            shill = shill_proxy.ShillProxy.get_proxy()
            manager_properties = shill.manager.GetProperties(utf8_strings=True)
            active_profile = manager_properties[
                    shill_proxy.ShillProxy.MANAGER_PROPERTY_ACTIVE_PROFILE]
            logging.info('ServiceAutoConnectContext: change cellular service '
                         'profile to %s', active_profile)
            service.SetProperty(
                    shill_proxy.ShillProxy.SERVICE_PROPERTY_PROFILE,
                    active_profile)

        return self


    def __exit__(self, exc_type, exc_value, traceback):
        if self._initial_autoconnect != self._autoconnect:
            service = self._get_service()
            if service is None:
                raise ContextError('Could not obtain a service object.')

            logging.info('ServiceAutoConnectContext: restore autoconnect to %s',
                         self._initial_autoconnect)
            service.SetProperty(
                shill_proxy.ShillProxy.SERVICE_PROPERTY_AUTOCONNECT,
                self._initial_autoconnect)
        return False


    @property
    def autoconnect(self):
        """AutoConnect property value within this context."""
        return self._autoconnect


    @property
    def initial_autoconnect(self):
        """Initial AutoConnect property value when entering this context."""
        return self._initial_autoconnect


@contextmanager
def stopped_shill():
    """A context for executing code which requires shill to be stopped.

    This context stops shill on entry to the context, and starts shill
    before exit from the context. This context further guarantees that
    shill will be not restarted by recover_duts and that recover_duts
    will not otherwise try to run its network connectivity checks, while
    this context is active.

    Note that the no-restart guarantee applies only if the user of
    this context completes with a 'reasonable' amount of time. In
    particular: if networking is unavailable for 15 minutes or more,
    recover_duts will reboot the DUT.

    """
    sys_power.pause_check_network_hook()
    utils.stop_service('shill')
    yield
    utils.start_service('shill')
    sys_power.resume_check_network_hook()


class StaticIPContext(object):
    """StaticIPConfig context manager class.

    Set a StaticIPConfig to the given service.

    """
    def __init__(self, service, config):
        self._service = service
        self._config = config


    def __enter__(self):
        """Configure the StaticIP parameters for the Service and apply those
        parameters to the interface by forcing a re-connect."""
        self._service.SetProperty(
            shill_proxy.ShillProxy.SERVICE_PROPERTY_STATIC_IP_CONFIG,
            dbus.Dictionary(self._config, signature='sv'))
        self._service.Disconnect()
        self._service.Connect()


    def __exit__(self, exception, value, traceback):
        """Clear configuration of StaticIP parameters for the Service and force
        a re-connect."""
        self._service.ClearProperty(
            shill_proxy.ShillProxy.SERVICE_PROPERTY_STATIC_IP_CONFIG)
        self._service.Disconnect()
        self._service.Connect()
