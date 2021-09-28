# Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging

from autotest_lib.client.common_lib import error
from autotest_lib.server.cros.network import attenuator
from autotest_lib.server.cros.network import attenuator_hosts

from chromite.lib import timeout_util

HOST_TO_FIXED_ATTENUATIONS = attenuator_hosts.HOST_FIXED_ATTENUATIONS


class AttenuatorController(object):
    """Represents a minicircuits variable attenuator.

    This device is used to vary the attenuation between a router and a client.
    This allows us to measure throughput as a function of signal strength and
    test some roaming situations.  The throughput vs signal strength tests
    are referred to rate vs range (RvR) tests in places.

    """

    @property
    def supported_attenuators(self):
        """@return iterable of int attenuators supported on this host."""
        return self._fixed_attenuations.keys()


    def __init__(self, hostname):
        """Construct a AttenuatorController.

        @param hostname: Hostname representing minicircuits attenuator.

        """
        self.hostname = hostname
        super(AttenuatorController, self).__init__()
        part = hostname.split('.', 1)[0]
        if part not in HOST_TO_FIXED_ATTENUATIONS.keys():
            raise error.TestError('Unexpected RvR host name %r.' % hostname)
        self._fixed_attenuations = HOST_TO_FIXED_ATTENUATIONS[part]
        num_atten = len(self.supported_attenuators)

        self._attenuator = attenuator.Attenuator(hostname, num_atten)
        self.set_variable_attenuation(0)


    def _approximate_frequency(self, attenuator_num, freq):
        """Finds an approximate frequency to freq.

        In case freq is not present in self._fixed_attenuations, we use a value
        from a nearby channel as an approximation.

        @param attenuator_num: attenuator in question on the remote host.  Each
                attenuator has a different fixed path loss per frequency.
        @param freq: int frequency in MHz.
        @returns int approximate frequency from self._fixed_attenuations.

        """
        old_offset = None
        approx_freq = None
        for defined_freq in self._fixed_attenuations[attenuator_num].keys():
            new_offset = abs(defined_freq - freq)
            if old_offset is None or new_offset < old_offset:
                old_offset = new_offset
                approx_freq = defined_freq

        logging.debug('Approximating attenuation for frequency %d with '
                      'constants for frequency %d.', freq, approx_freq)
        return approx_freq


    def close(self):
        """Close variable attenuator connection."""
        self._attenuator.close()


    def set_total_attenuation(self, atten_db, frequency_mhz,
                              attenuator_num=None):
        """Set the total attenuation on one or all attenuators.

        @param atten_db: int level of attenuation in dB.  This must be
                higher than the fixed attenuation level of the affected
                attenuators.
        @param frequency_mhz: int frequency for which to calculate the
                total attenuation.  The fixed component of attenuation
                varies with frequency.
        @param attenuator_num: int attenuator to change, or None to
                set all variable attenuators.

        """
        affected_attenuators = self.supported_attenuators
        if attenuator_num is not None:
            affected_attenuators = [attenuator_num]
        for atten in affected_attenuators:
            freq_to_fixed_loss = self._fixed_attenuations[atten]
            approx_freq = self._approximate_frequency(atten,
                                                      frequency_mhz)
            variable_atten_db = atten_db - freq_to_fixed_loss[approx_freq]
            self.set_variable_attenuation(variable_atten_db,
                                          attenuator_num=atten)


    def set_variable_attenuation(self, atten_db, attenuator_num=None):
        """Set the variable attenuation on one or all attenuators.

        @param atten_db: int non-negative level of attenuation in dB.
        @param attenuator_num: int attenuator to change, or None to
                set all variable attenuators.

        """
        affected_attenuators = self.supported_attenuators
        if attenuator_num is not None:
            affected_attenuators = [attenuator_num]
        for atten in affected_attenuators:
            try:
                self._attenuator.set_atten(atten, atten_db)
                if int(self._attenuator.get_atten(atten)) != atten_db:
                    raise error.TestError('Attenuation did not set as expected '
                                          'on attenuator %d' % atten)
            except error.TestError:
                self._attenuator.reopen(self.hostname)
                self._attenuator.set_atten(atten, atten_db)
                if int(self._attenuator.get_atten(atten)) != atten_db:
                    raise error.TestError('Attenuation did not set as expected '
                                          'on attenuator %d' % atten)
            logging.info('%ddb attenuation set successfully on attenautor %d',
                         atten_db, atten)


    def get_minimal_total_attenuation(self):
        """Get attenuator's maximum fixed attenuation value.

        This is pulled from the current attenuator's lines and becomes the
        minimal total attenuation when stepping through attenuation levels.

        @return maximum starting attenuation value

        """
        max_atten = 0
        for atten_num in self._fixed_attenuations.iterkeys():
            atten_values = self._fixed_attenuations[atten_num].values()
            max_atten = max(max(atten_values), max_atten)
        return max_atten


    def set_signal_level(self, client_context, requested_sig_level,
            min_sig_level_allowed=-85, tolerance_percent=3, timeout=240):
        """Set wifi signal to desired level by changing attenuation.

        @param client_context: Client context object.
        @param requested_sig_level: Negative int value in dBm for wifi signal
                level to be set.
        @param min_sig_level_allowed: Minimum signal level allowed; this is to
                ensure that we don't set a signal that is too weak and DUT can
                not associate.
        @param tolerance_percent: Percentage to be used to calculate the desired
                range for the wifi signal level.
        """
        atten_db = 0
        starting_sig_level = client_context.wifi_signal_level
        if not starting_sig_level:
            raise error.TestError("No signal detected.")
        if not (min_sig_level_allowed <= requested_sig_level <=
                starting_sig_level):
            raise error.TestError("Requested signal level (%d) is either "
                                  "higher than current signal level (%r) with "
                                  "0db attenuation or lower than minimum "
                                  "signal level (%d) allowed." %
                                  (requested_sig_level,
                                  starting_sig_level,
                                  min_sig_level_allowed))

        try:
            with timeout_util.Timeout(timeout):
                while True:
                    client_context.reassociate(timeout_seconds=1)
                    current_sig_level = client_context.wifi_signal_level
                    logging.info("Current signal level %r", current_sig_level)
                    if not current_sig_level:
                        raise error.TestError("No signal detected.")
                    if self.signal_in_range(requested_sig_level,
                            current_sig_level, tolerance_percent):
                        logging.info("Signal level set to %r.",
                                     current_sig_level)
                        break
                    if current_sig_level > requested_sig_level:
                        self.set_variable_attenuation(atten_db)
                        atten_db +=1
                    if current_sig_level < requested_sig_level:
                        self.set_variable_attenuation(atten_db)
                        atten_db -= 1
        except (timeout_util.TimeoutError, error.TestError,
                error.TestFail) as e:
            raise error.TestError("Not able to set wifi signal to requested "
                                  "level. \n%s" % e)


    def signal_in_range(self, req_sig_level, curr_sig_level, tolerance_percent):
        """Check if wifi signal is within the threshold of requested signal.

        @param req_sig_level: Negative int value in dBm for wifi signal
                level to be set.
        @param curr_sig_level: Current wifi signal level seen by the DUT.
        @param tolerance_percent: Percentage to be used to calculate the desired
                range for the wifi signal level.

        @returns True if wifi signal is in the desired range.
        """
        min_sig = req_sig_level + (req_sig_level * tolerance_percent / 100)
        max_sig = req_sig_level - (req_sig_level * tolerance_percent / 100)
        if min_sig <= curr_sig_level <= max_sig:
            return True
        return False
