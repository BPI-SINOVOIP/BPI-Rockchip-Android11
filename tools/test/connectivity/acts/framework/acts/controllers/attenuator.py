#!/usr/bin/env python3.4
#
#   Copyright 2016 - The Android Open Source Project
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

import importlib
import logging

from acts.keys import Config
from acts.libs.proc import job

ACTS_CONTROLLER_CONFIG_NAME = 'Attenuator'
ACTS_CONTROLLER_REFERENCE_NAME = 'attenuators'
_ATTENUATOR_OPEN_RETRIES = 3


def create(configs):
    objs = []
    for c in configs:
        attn_model = c['Model']
        # Default to telnet.
        protocol = c.get('Protocol', 'telnet')
        module_name = 'acts.controllers.attenuator_lib.%s.%s' % (attn_model,
                                                                 protocol)
        module = importlib.import_module(module_name)
        inst_cnt = c['InstrumentCount']
        attn_inst = module.AttenuatorInstrument(inst_cnt)
        attn_inst.model = attn_model

        ip_address = c[Config.key_address.value]
        port = c[Config.key_port.value]

        for attempt_number in range(1, _ATTENUATOR_OPEN_RETRIES + 1):
            try:
                attn_inst.open(ip_address, port)
            except Exception as e:
                logging.error('Attempt %s to open connection to attenuator '
                              'failed: %s' % (attempt_number, e))
                if attempt_number == _ATTENUATOR_OPEN_RETRIES:
                    ping_output = job.run('ping %s -c 1 -w 1' % ip_address,
                                          ignore_status=True)
                    if ping_output.exit_status == 1:
                        logging.error('Unable to ping attenuator at %s' %
                                      ip_address)
                    else:
                        logging.error('Able to ping attenuator at %s' %
                                      ip_address)
                        job.run('echo "q" | telnet %s %s' % (ip_address, port),
                                ignore_status=True)
                    raise
        for i in range(inst_cnt):
            attn = Attenuator(attn_inst, idx=i)
            if 'Paths' in c:
                try:
                    setattr(attn, 'path', c['Paths'][i])
                except IndexError:
                    logging.error('No path specified for attenuator %d.', i)
                    raise
            objs.append(attn)
    return objs


def get_info(attenuators):
    """Get information on a list of Attenuator objects.

    Args:
        attenuators: A list of Attenuator objects.

    Returns:
        A list of dict, each representing info for Attenuator objects.
    """
    device_info = []
    for attenuator in attenuators:
        info = {
            "Address": attenuator.instrument.address,
            "Attenuator_Port": attenuator.idx
        }
        device_info.append(info)
    return device_info


def destroy(objs):
    for attn in objs:
        attn.instrument.close()


def get_attenuators_for_device(device_attenuator_configs, attenuators,
                               attenuator_key):
    """Gets the list of attenuators associated to a specified device and builds
    a list of the attenuator objects associated to the ip address in the
    device's section of the ACTS config and the Attenuator's IP address.  In the
    example below the access point object has an attenuator dictionary with
    IP address associated to an attenuator object.  The address is the only
    mandatory field and the 'attenuator_ports_wifi_2g' and
    'attenuator_ports_wifi_5g' are the attenuator_key specified above.  These
    can be anything and is sent in as a parameter to this function.  The numbers
    in the list are ports that are in the attenuator object.  Below is an
    standard Access_Point object and the link to a standard Attenuator object.
    Notice the link is the IP address, which is why the IP address is mandatory.

    "AccessPoint": [
        {
          "ssh_config": {
            "user": "root",
            "host": "192.168.42.210"
          },
          "Attenuator": [
            {
              "Address": "192.168.42.200",
              "attenuator_ports_wifi_2g": [
                0,
                1,
                3
              ],
              "attenuator_ports_wifi_5g": [
                0,
                1
              ]
            }
          ]
        }
      ],
      "Attenuator": [
        {
          "Model": "minicircuits",
          "InstrumentCount": 4,
          "Address": "192.168.42.200",
          "Port": 23
        }
      ]
    Args:
        device_attenuator_configs: A list of attenuators config information in
            the acts config that are associated a particular device.
        attenuators: A list of all of the available attenuators objects
            in the testbed.
        attenuator_key: A string that is the key to search in the device's
            configuration.

    Returns:
        A list of attenuator objects for the specified device and the key in
        that device's config.
    """
    attenuator_list = []
    for device_attenuator_config in device_attenuator_configs:
        for attenuator_port in device_attenuator_config[attenuator_key]:
            for attenuator in attenuators:
                if (attenuator.instrument.address ==
                        device_attenuator_config['Address']
                        and attenuator.idx is attenuator_port):
                    attenuator_list.append(attenuator)
    return attenuator_list


"""Classes for accessing, managing, and manipulating attenuators.

Users will instantiate a specific child class, but almost all operation should
be performed on the methods and data members defined here in the base classes
or the wrapper classes.
"""


class AttenuatorError(Exception):
    """Base class for all errors generated by Attenuator-related modules."""


class InvalidDataError(AttenuatorError):
    """"Raised when an unexpected result is seen on the transport layer.

    When this exception is seen, closing an re-opening the link to the
    attenuator instrument is probably necessary. Something has gone wrong in
    the transport.
    """
    pass


class InvalidOperationError(AttenuatorError):
    """Raised when the attenuator's state does not allow the given operation.

    Certain methods may only be accessed when the instance upon which they are
    invoked is in a certain state. This indicates that the object is not in the
    correct state for a method to be called.
    """
    pass


class AttenuatorInstrument(object):
    """Defines the primitive behavior of all attenuator instruments.

    The AttenuatorInstrument class is designed to provide a simple low-level
    interface for accessing any step attenuator instrument comprised of one or
    more attenuators and a controller. All AttenuatorInstruments should override
    all the methods below and call AttenuatorInstrument.__init__ in their
    constructors. Outside of setup/teardown, devices should be accessed via
    this generic "interface".
    """
    model = None
    INVALID_MAX_ATTEN = 999.9

    def __init__(self, num_atten=0):
        """This is the Constructor for Attenuator Instrument.

        Args:
            num_atten: The number of attenuators contained within the
                instrument. In some instances setting this number to zero will
                allow the driver to auto-determine the number of attenuators;
                however, this behavior is not guaranteed.

        Raises:
            NotImplementedError if initialization is called from this class.
        """

        if type(self) is AttenuatorInstrument:
            raise NotImplementedError(
                'Base class should not be instantiated directly!')

        self.num_atten = num_atten
        self.max_atten = AttenuatorInstrument.INVALID_MAX_ATTEN
        self.properties = None

    def set_atten(self, idx, value, strict=True):
        """Sets the attenuation given its index in the instrument.

        Args:
            idx: A zero based index used to identify a particular attenuator in
                an instrument.
            value: a floating point value for nominal attenuation to be set.
            strict: if True, function raises an error when given out of
                bounds attenuation values, if false, the function sets out of
                bounds values to 0 or max_atten.
        """
        raise NotImplementedError('Base class should not be called directly!')

    def get_atten(self, idx):
        """Returns the current attenuation of the attenuator at index idx.

        Args:
            idx: A zero based index used to identify a particular attenuator in
                an instrument.

        Returns:
            The current attenuation value as a floating point value
        """
        raise NotImplementedError('Base class should not be called directly!')


class Attenuator(object):
    """An object representing a single attenuator in a remote instrument.

    A user wishing to abstract the mapping of attenuators to physical
    instruments should use this class, which provides an object that abstracts
    the physical implementation and allows the user to think only of attenuators
    regardless of their location.
    """
    def __init__(self, instrument, idx=0, offset=0):
        """This is the constructor for Attenuator

        Args:
            instrument: Reference to an AttenuatorInstrument on which the
                Attenuator resides
            idx: This zero-based index is the identifier for a particular
                attenuator in an instrument.
            offset: A power offset value for the attenuator to be used when
                performing future operations. This could be used for either
                calibration or to allow group operations with offsets between
                various attenuators.

        Raises:
            TypeError if an invalid AttenuatorInstrument is passed in.
            IndexError if the index is out of range.
        """
        if not isinstance(instrument, AttenuatorInstrument):
            raise TypeError('Must provide an Attenuator Instrument Ref')
        self.model = instrument.model
        self.instrument = instrument
        self.idx = idx
        self.offset = offset

        if self.idx >= instrument.num_atten:
            raise IndexError(
                'Attenuator index out of range for attenuator instrument')

    def set_atten(self, value, strict=True):
        """Sets the attenuation.

        Args:
            value: A floating point value for nominal attenuation to be set.
            strict: if True, function raises an error when given out of
                bounds attenuation values, if false, the function sets out of
                bounds values to 0 or max_atten.

        Raises:
            ValueError if value + offset is greater than the maximum value.
        """
        if value + self.offset > self.instrument.max_atten and strict:
            raise ValueError(
                'Attenuator Value+Offset greater than Max Attenuation!')

        self.instrument.set_atten(self.idx, value + self.offset, strict)

    def get_atten(self):
        """Returns the attenuation as a float, normalized by the offset."""
        return self.instrument.get_atten(self.idx) - self.offset

    def get_max_atten(self):
        """Returns the max attenuation as a float, normalized by the offset."""
        if self.instrument.max_atten == AttenuatorInstrument.INVALID_MAX_ATTEN:
            raise ValueError('Invalid Max Attenuator Value')

        return self.instrument.max_atten - self.offset


class AttenuatorGroup(object):
    """An abstraction for groups of attenuators that will share behavior.

    Attenuator groups are intended to further facilitate abstraction of testing
    functions from the physical objects underlying them. By adding attenuators
    to a group, it is possible to operate on functional groups that can be
    thought of in a common manner in the test. This class is intended to provide
    convenience to the user and avoid re-implementation of helper functions and
    small loops scattered throughout user code.
    """
    def __init__(self, name=''):
        """This constructor for AttenuatorGroup

        Args:
            name: An optional parameter intended to further facilitate the
                passing of easily tracked groups of attenuators throughout code.
                It is left to the user to use the name in a way that meets their
                needs.
        """
        self.name = name
        self.attens = []
        self._value = 0

    def add_from_instrument(self, instrument, indices):
        """Adds an AttenuatorInstrument to the group.

        This function will create Attenuator objects for all of the indices
        passed in and add them to the group.

        Args:
            instrument: the AttenuatorInstrument to pull attenuators from.
                indices: The index or indices to add to the group. Either a
                range, a list, or a single integer.

        Raises
        ------
        TypeError
            Requires a valid AttenuatorInstrument to be passed in.
        """
        if not instrument or not isinstance(instrument, AttenuatorInstrument):
            raise TypeError('Must provide an Attenuator Instrument Ref')

        if type(indices) is range or type(indices) is list:
            for i in indices:
                self.attens.append(Attenuator(instrument, i))
        elif type(indices) is int:
            self.attens.append(Attenuator(instrument, indices))

    def add(self, attenuator):
        """Adds an already constructed Attenuator object to this group.

        Args:
            attenuator: An Attenuator object.

        Raises:
            TypeError if the attenuator parameter is not an Attenuator.
        """
        if not isinstance(attenuator, Attenuator):
            raise TypeError('Must provide an Attenuator')

        self.attens.append(attenuator)

    def synchronize(self):
        """Sets all grouped attenuators to the group's attenuation value."""
        self.set_atten(self._value)

    def is_synchronized(self):
        """Returns true if all attenuators have the synchronized value."""
        for att in self.attens:
            if att.get_atten() != self._value:
                return False
        return True

    def set_atten(self, value):
        """Sets the attenuation value of all attenuators in the group.

        Args:
            value: A floating point value for nominal attenuation to be set.
        """
        value = float(value)
        for att in self.attens:
            att.set_atten(value)
        self._value = value

    def get_atten(self):
        """Returns the current attenuation setting of AttenuatorGroup."""
        return float(self._value)
