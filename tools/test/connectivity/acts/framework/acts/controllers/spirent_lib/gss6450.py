#!/usr/bin/env python3
#
#   Copyright 2019 - The Android Open Source Project
#
#   Licensed under the Apache License, Version 2.0 (the "License");
#   you may not use this file except in compliance with the License.
#   You may obtain a copy of the License at
#
#           http://www.apache.org/licenses/LICENSE-2.0
#
#   Unless required by applicable law or agreed to in writing, software
#   distributed under the License is distributed on an "AS IS" BASIS,
#   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#   See the License for the specific language governing permissions and
#   limitations under the License.
"""Python module for Spirent GSS6450 GNSS RPS."""

import datetime
import numbers
from acts.controllers import abstract_inst


class GSS6450Error(abstract_inst.SocketInstrumentError):
    """GSS6450 Instrument Error Class."""


class GSS6450(abstract_inst.RequestInstrument):
    """GSS6450 Class, inherted from abstract_inst RequestInstrument."""

    def __init__(self, ip_addr):
        """Init method for GSS6450.

        Args:
            ip_addr: IP Address.
                Type, str.
        """
        super(GSS6450, self).__init__(ip_addr)

        self.idn = 'Spirent-GSS6450'

    def _put(self, cmd):
        """Send put command via GSS6450 HTTP Request and get response.

        Args:
            cmd: parameters listed in SHM_PUT.
                Type, Str.

        Returns:
            resp: Response from the _query method.
                Type, Str.
        """
        put_cmd = 'shm_put.shtml?' + cmd
        resp = self._query(put_cmd)

        return resp

    def _get(self, cmd):
        """Send get command via GSS6450 HTTP Request and get response.

        Args:
            cmd: parameters listed in SHM_GET.
                Type, Str.

        Returns:
          resp: Response from the _query method.
              Type, Str.
        """
        get_cmd = 'shm_get.shtml?' + cmd
        resp = self._query(get_cmd)

        return resp

    def get_scenario_filename(self):
        """Get the scenario filename of GSS6450.

        Returns:
            filename: RPS Scenario file name.
                Type, Str.
        """
        resp_raw = self._get('-f')
        filename = resp_raw.split(':')[-1].strip(' ')
        self._logger.debug('Got scenario file name: "%s".', filename)

        return filename

    def get_scenario_description(self):
        """Get the scenario description of GSS6450.

        Returns:
            description: RPS Scenario description.
                Type, Str.
        """
        resp_raw = self._get('-d')
        description = resp_raw.split('-d')[-1].strip(' ')

        if description:
            self._logger.debug('Got scenario description: "%s".', description)
        else:
            self._logger.warning('Got scenario description with empty string.')

        return description

    def get_scenario_location(self):
        """Get the scenario location of GSS6450.

        Returns:
            location: RPS Scenario location.
                Type, Str.
        """
        resp_raw = self._get('-i')
        location = resp_raw.split('-i')[-1].strip(' ')

        if location:
            self._logger.debug('Got scenario location: "%s".', location)
        else:
            self._logger.warning('Got scenario location with empty string.')

        return location

    def get_operation_mode(self):
        """Get the operation mode of GSS6450.

        Returns:
            mode: RPS Operation Mode.
                Type, Str.
                Option, STOPPED/PLAYING/RECORDING
        """
        resp_raw = self._get('-m')
        mode = resp_raw.split('-m')[-1].strip(' ')
        self._logger.debug('Got operation mode: "%s".', mode)

        return mode

    def get_battery_level(self):
        """Get the battery level of GSS6450.

        Returns:
            batterylevel: RPS Battery Level.
                Type, float.
        """
        resp_raw = self._get('-l')
        batterylevel = float(resp_raw.split('-l')[-1].strip(' '))
        self._logger.debug('Got battery level: %s%%.', batterylevel)

        return batterylevel

    def get_rfport_voltage(self):
        """Get the RF port voltage of GSS6450.

        Returns:
            voltageout: RPS RF port voltage.
                Type, str
        """
        resp_raw = self._get('-v')
        voltageout = resp_raw.split('-v')[-1].strip(' ')
        self._logger.debug('Got RF port voltage: "%s".', voltageout)

        return voltageout

    def get_storage_media(self):
        """Get the storage media of GSS6450.

        Returns:
            media: RPS storage.
                Type, str

        Raises:
            GSS6450Error: raise when request response is not support.
        """
        resp_raw = self._get('-M')
        resp_num = resp_raw.split('-M')[-1].strip(' ')

        if resp_num == '1':
            media = '1-INTERNAL'
        elif resp_num == '2':
            media = '2-REMOVABLE'
        else:
            errmsg = ('"{}" is not recognized as GSS6450 valid storage media'
                      ' type'.format(resp_num))
            raise GSS6450Error(error=errmsg, command='get_storage_media')

        self._logger.debug('Got current storage media: %s.', media)

        return media

    def get_attenuation(self):
        """Get the attenuation of GSS6450.

        Returns:
            attenuation: RPS attenuation level, in dB.
                Type, list of float.
        """
        resp_raw = self._get('-a')
        resp_str = resp_raw.split('-a')[-1].strip(' ')
        self._logger.debug('Got attenuation: %s dB.', resp_str)
        attenuation = [float(itm) for itm in resp_str.split(',')]

        return attenuation

    def get_elapsed_time(self):
        """Get the running scenario elapsed time of GSS6450.

        Returns:
            etime: RPS elapsed time.
                Type, datetime.timedelta.
        """
        resp_raw = self._get('-e')
        resp_str = resp_raw.split('-e')[-1].strip(' ')
        self._logger.debug('Got senario elapsed time: "%s".', resp_str)
        etime_tmp = datetime.datetime.strptime(resp_str, '%H:%M:%S')
        etime = datetime.timedelta(hours=etime_tmp.hour,
                                   minutes=etime_tmp.minute,
                                   seconds=etime_tmp.second)

        return etime

    def get_playback_offset(self):
        """Get the running scenario playback offset of GSS6450.

        Returns:
            offset: RPS playback offset.
                Type, datetime.timedelta.
        """
        resp_raw = self._get('-o')
        offset_tmp = float(resp_raw.split('-o')[-1].strip(' '))
        self._logger.debug('Got senario playback offset: %s sec.', offset_tmp)
        offset = datetime.timedelta(seconds=offset_tmp)

        return offset

    def play_scenario(self, scenario=''):
        """Start to play scenario in GSS6450.

        Args:
            scenario: Scenario to play.
                Type, str.
                Default, '', which will run current selected one.
        """
        if scenario:
            cmd = '-f{},-wP'.format(scenario)
        else:
            cmd = '-wP'

        _ = self._put(cmd)

        if scenario:
            infmsg = 'Started playing scenario: "{}".'.format(scenario)
        else:
            infmsg = 'Started playing current scenario.'

        self._logger.debug(infmsg)

    def record_scenario(self, scenario=''):
        """Start to record scenario in GSS6450.

        Args:
            scenario: Scenario to record.
                Type, str.
                Default, '', which will run current selected one.
        """
        if scenario:
            cmd = '-f{},-wR'.format(scenario)
        else:
            cmd = '-wR'

        _ = self._put(cmd)

        if scenario:
            infmsg = 'Started recording scenario: "{}".'.format(scenario)
        else:
            infmsg = 'Started recording scenario.'

        self._logger.debug(infmsg)

    def stop_scenario(self):
        """Start to stop playing/recording scenario in GSS6450."""
        _ = self._put('-wS')

        self._logger.debug('Stopped playing/recording scanrio.')

    def set_rfport_voltage(self, voltageout):
        """Set the RF port voltage of GSS6450.

        Args:
            voltageout: RPS RF port voltage.
                Type, str

        Raises:
            GSS6450Error: raise when voltageout input is not valid.
        """
        if voltageout == 'OFF':
            voltage_cmd = '0'
        elif voltageout == '3.3V':
            voltage_cmd = '3'
        elif voltageout == '5V':
            voltage_cmd = '5'
        else:
            errmsg = ('"{}" is not recognized as GSS6450 valid RF port voltage'
                      ' type'.format(voltageout))
            raise GSS6450Error(error=errmsg, command='set_rfport_voltage')

        _ = self._put('-v{},-wV'.format(voltage_cmd))
        self._logger.debug('Set RF port voltage: "%s".', voltageout)

    def set_attenuation(self, attenuation):
        """Set the attenuation of GSS6450.

        Args:
            attenuation: RPS attenuation level, in dB.
                Type, numerical.

        Raises:
            GSS6450Error: raise when attenuation is not in range.
        """
        if not 0 <= attenuation <= 31:
            errmsg = ('"attenuation" must be within [0, 31], '
                      'current input is {}').format(str(attenuation))
            raise GSS6450Error(error=errmsg, command='set_attenuation')

        attenuation_raw = round(attenuation)

        if attenuation_raw != attenuation:
            warningmsg = ('"attenuation" must be integer, current input '
                          'will be rounded to {}'.format(attenuation_raw))
            self._logger.warning(warningmsg)

        _ = self._put('-a{},-wA'.format(attenuation_raw))

        self._logger.debug('Set attenuation: %s dB.', attenuation_raw)

    def set_playback_offset(self, offset):
        """Set the playback offset of GSS6450.

        Args:
            offset: RPS playback offset.
                Type, datetime.timedelta, or numerical.

        Raises:
            GSS6450Error: raise when offset is not numeric or timedelta.
        """
        if isinstance(offset, datetime.timedelta):
            offset_raw = offset.total_seconds()
        elif isinstance(offset, numbers.Number):
            offset_raw = offset
        else:
            raise GSS6450Error(error=('"offset" must be numerical value or '
                                      'datetime.timedelta'),
                               command='set_playback_offset')

        _ = self._put('-o{}'.format(offset_raw))

        self._logger.debug('Set playback offset: %s sec.', offset_raw)

    def set_storage_media(self, media):
        """Set the storage media of GSS6450.

        Args:
            media: RPS storage Media, Internal or External.
                Type, str. Option, 'internal', 'removable'

        Raises:
            GSS6450Error: raise when media option is not support.
        """
        if media == 'internal':
            raw_media = '1'
        elif media == 'removable':
            raw_media = '2'
        else:
            raise GSS6450Error(
                error=('"media" input must be in ["internal", "removable"]. '
                       ' Current input is {}'.format(media)),
                command='set_storage_media')

        _ = self._put('-M{}-wM'.format(raw_media))

        resp_raw = self.get_storage_media()
        if raw_media != resp_raw[0]:
            raise GSS6450Error(
                error=('Setting media "{}" is not the same as queried media '
                       '"{}".'.format(media, resp_raw)),
                command='set_storage_media')
