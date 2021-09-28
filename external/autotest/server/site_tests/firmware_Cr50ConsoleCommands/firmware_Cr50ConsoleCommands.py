# Copyright 2017 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import difflib
import logging
import os
import pprint
import re
import time

from autotest_lib.client.common_lib import error
from autotest_lib.server.cros.faft.cr50_test import Cr50Test


class firmware_Cr50ConsoleCommands(Cr50Test):
    """
    Verify the cr50 console output for important commands.

    This test verifies the output of pinmux, help, gpiocfg. These are the main
    console commands we can use to check cr50 configuration.
    """
    version = 1

    # The board properties that are actively being used. This also excludes all
    # ccd board properties, because they might change based on whether ccd is
    # enabled.
    #
    # This information is in ec/board/cr50/scratch_reg1.h
    RELEVANT_PROPERTIES = 0x63
    COMPARE_LINES = '\n'
    COMPARE_WORDS = None
    SORTED = True
    CMD_RETRY_COUNT = 5
    TESTS = [
        ['pinmux', 'pinmux(.*)>', COMPARE_LINES, not SORTED],
        ['help', 'Known commands:(.*)HELP LIST.*>', COMPARE_WORDS, SORTED],
        ['gpiocfg', 'gpiocfg(.*)>', COMPARE_LINES, not SORTED],
    ]
    CCD_HOOK_WAIT = 2
    # Lists connecting the board property values to the labels.
    #   [ board property, match label, exclude label ]
    # exclude can be none if there is no label that shoud be excluded based on
    # the property.
    BOARD_PROPERTIES = [
        ['BOARD_SLAVE_CONFIG_SPI', 'sps', 'i2cs'],
        ['BOARD_SLAVE_CONFIG_I2C', 'i2cs', 'sps,sps_ds_resume'],
        ['BOARD_USE_PLT_RESET', 'plt_rst', 'sys_rst'],
        ['BOARD_CLOSED_SOURCE_SET1', 'closed_source_set1', 'open_source_set'],
    ]

    def initialize(self, host, cmdline_args, full_args):
        super(firmware_Cr50ConsoleCommands, self).initialize(host, cmdline_args,
                full_args)
        self.host = host
        self.missing = []
        self.extra = []
        self.past_matches = {}

        # Make sure the console is restricted
        if self.cr50.get_cap('GscFullConsole')[self.cr50.CAP_REQ] == 'Always':
            logging.info('Restricting console')
            self.fast_open(enable_testlab=True)
            self.cr50.set_cap('GscFullConsole', 'IfOpened')
            time.sleep(self.CCD_HOOK_WAIT)
            self.cr50.set_ccd_level('lock')


    def parse_output(self, output, split_str):
        """Split the output with the given delimeter and remove empty strings"""
        output = output.split(split_str) if split_str else output.split()
        cleaned_output = []
        for line in output:
            # Replace whitespace characters with one space.
            line = ' '.join(line.strip().split())
            if line:
                cleaned_output.append(line)
        return cleaned_output


    def get_output(self, cmd, regexp, split_str, sort):
        """Return the cr50 console output"""
        old_output = []
        output = self.cr50.send_command_retry_get_output(
                cmd, [regexp], safe=True, compare_output=True)[0][1].strip()

        # Record the original command output
        results_path = os.path.join(self.resultsdir, cmd)
        with open(results_path, 'w') as f:
            f.write(output)

        output = self.parse_output(output, split_str)
        if sort:
            # Sort the output ignoring any '-'s at the start of the command.
            output.sort(key=lambda cmd: cmd.lstrip('-'))
        if not len(output):
            raise error.TestFail('Could not get %s output' % cmd)
        return '\n'.join(output) + '\n'


    def get_expected_output(self, cmd, split_str):
        """Return the expected cr50 console output"""
        path = os.path.join(os.path.dirname(os.path.realpath(__file__)), cmd)
        logging.info('reading %s', path)
        if not os.path.isfile(path):
            raise error.TestFail('Could not find %s file %s' % (cmd, path))

        with open(path, 'r') as f:
            contents = f.read()

        return self.parse_output(contents, split_str)


    def check_command(self, cmd, regexp, split_str, sort):
        """Compare the actual console command output to the expected output"""
        expected_output = self.get_expected_output(cmd, split_str)
        output = self.get_output(cmd, regexp, split_str, sort)
        diff_info = difflib.unified_diff(expected_output, output.splitlines())
        logging.debug('%s DIFF:\n%s', cmd, '\n'.join(diff_info))
        missing = []
        extra = []
        for regexp in expected_output:
            match = re.search(regexp, output)
            if match:
                # Update the past_matches dict with the matches from this line.
                #
                # Make sure if matches for any keys existed before, they exist
                # now and if they didn't exist, they don't exist now.
                for k, v in match.groupdict().iteritems():
                    old_val = self.past_matches.get(k, [v, v])[0]
                    if old_val and not v:
                        missing.append('%s:%s' % (k, regexp))
                    elif not old_val and v:
                        extra.append('%s:%s' % (k, v))
                    else:
                        self.past_matches[k] = [v, regexp]

            # Remove the matching string from the output.
            output, n = re.subn('%s\s*' % regexp, '', output, 1)
            if not n:
                missing.append(regexp)


        if missing:
            self.missing.append('%s-(%s)' % (cmd, ', '.join(missing)))
        output = output.strip()
        if output:
            extra.extend(output.split('\n'))
        if extra:
            self.extra.append('%s-(%s)' % (cmd, ', '.join(extra)))


    def get_image_properties(self):
        """Save the board properties

        The saved board property flags will not include oboslete flags or the wp
        setting. These won't change the gpio or pinmux settings.
        """
        self.include = []
        self.exclude = []
        for prop, include, exclude in self.BOARD_PROPERTIES:
            if self.cr50.uses_board_property(prop):
                self.include.extend(include.split(','))
                if exclude:
                    self.exclude.extend(exclude.split(','))
            else:
                self.exclude.append(include)
        version = self.cr50.get_version().split('.')
        # Factory images end with 22. Expect guc attributes if the version
        # ends in 22.
        if version[2] == '22':
            self.include.append('guc')
        else:
            self.exclude.append('guc')

        # Use the major version to determine prePVT or MP. prePVT have even
        # major versions. prod have odd.
        if int(version[1]) % 2:
            self.include.append('mp')
            self.exclude.append('prepvt')
        else:
            self.exclude.append('mp')
            self.include.append('prepvt')
        brdprop = self.cr50.get_board_properties()
        logging.info('brdprop: 0x%x', brdprop)
        logging.info('include: %s', ', '.join(self.include))
        logging.info('exclude: %s', ', '.join(self.exclude))


    def run_once(self, host):
        """Verify the Cr50 gpiocfg, pinmux, and help output."""
        err = []
        test_err = []
        self.get_image_properties()
        for command, regexp, split_str, sort in self.TESTS:
            self.check_command(command, regexp, split_str, sort)

        if len(self.missing):
            err.append('MISSING OUTPUT: ' + ', '.join(self.missing))
        if len(self.extra):
            err.append('EXTRA OUTPUT: ' + ', '.join(self.extra))
        logging.debug("Past matches:\n%s", pprint.pformat(self.past_matches))

        if len(err):
            raise error.TestFail('\t'.join(err))

        # Check all of the labels we did/didn't match. Make sure they match the
        # expected cr50 settings. Raise a test error if there are any mismatches
        missing_labels = []
        for label in self.include:
            if label in self.past_matches and not self.past_matches[label][0]:
                missing_labels.append(label)
        extra_labels = []
        for label in self.exclude:
            if label in self.past_matches and self.past_matches[label][0]:
                extra_labels.append(label)
        if missing_labels:
            test_err.append('missing: %s' % ', '.join(missing_labels))
        if extra_labels:
            test_err.append('matched: %s' % ', '.join(extra_labels))
        if test_err:
            raise error.TestError('\t'.join(test_err))
