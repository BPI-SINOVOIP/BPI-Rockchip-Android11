# Copyright 2015 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Command-line parsing for the DUT deployment tool.

This contains parsing for the legacy `repair_test` and `deployment_test`
commands, and for the new `deploy` command.

The syntax for the two legacy commands is identical; the difference in
the two commands is merely slightly different default options.

The full deployment flow performs all of the following actions:
  * Stage the USB image:  Install the DUT's assigned repair image onto
    the Servo USB stick.
  * Install firmware:  Boot the DUT from the USB stick, and run
    `chromeos-firmwareupdate` to install dev-signed RO and RW firmware.
    The DUT must begin in dev-mode, with hardware write-protect
    disabled.  At successful completion, the DUT is in verified boot
    mode.
  * Install test image:  Boot the DUT in recovery mode from the USB
    stick, and run `chromeos-install` to install the OS.

The new `deploy` command chooses particular combinations of the steps
above based on a subcommand and options:
    `deploy servo`:  Only stage the USB image.
    `deploy firmware`:  Install both the firmware and the test image,
        in that order.  Optionally, first stage the USB image.
    `deploy test-image`: Install the test image.  Optionally, first
        stage the USB image.
    `deploy repair`:  Equivalent to `deploy test-image`, except that
        by default it doesn't upload its logs to storage.

This module exports two functions, `parse_deprecated_command()` (for the
two legacy commands) and `parse_command()` (for the new `deploy`
command).  Although the functions parse slightly different syntaxes,
they return `argparse.Namespace` objects with identical fields, described
below.

The following fields represent parameter inputs to the underlying
deployment:
    `web`:  Server name (or URL) for the AFE RPC service.
    `logdir`:  The directory where logs are to be stored.
    `board`:  Specifies the board to be used when creating DUTs.
    `build`:  When provided, the repair image assigned to the board for
        the target DUT will be updated to this value prior to staging
        USB image.  The build is in a form like 'R66-10447.0.0'.
    `hostname_file`:  Name of a file in CSV format with information
        about the hosts and servos to be deployed/repaired.
    `hostnames`:  List of DUT host names.

The following fields specify options that are used to enable or disable
specific deployment steps:
    `upload`:  When true, logs will be uploaded to googlestorage after
        the command completes.
    `dry_run`:  When true, disables operations with any kind of
        side-effect.  This option implicitly overrides and disables all
        of the deployment steps below.
    `stageusb`:  When true, enable staging the USB image.  Disabling
        this will speed up operations when the stick is known to already
        have the proper image.
    `install_firmware`:  When true, enable firmware installation.
    `install_test_image`:  When true, enable installing the test image via
     send ctrl_u to boot into USB, which only apply to initial DUT deployment.
    `reinstall test image`: when true, enable installing test image through
     recover mode.
     `labstation`: when true, deploy labstation instead of DUT.

The `dry_run` option is off by default.  The `upload` option is on by
default, except for `deploy repair` and `repair_test`.  The values for
all other options are determined by the subcommand.
"""

import argparse
import os


class _ArgumentParser(argparse.ArgumentParser):
    """`argparse.ArgumentParser` extended with boolean option pairs."""

    def add_boolean_argument(self, name, default, **kwargs):
        """Add a pair of argument flags for a boolean option.

        This add a pair of options, named `--<name>` and `--no<name>`.
        The actions of the two options are 'store_true' and
        'store_false', respectively, with the destination `<name>`.

        If neither option is present on the command line, the default
        value for destination `<name>` is given by `default`.

        The given `kwargs` may be any arguments accepted by
        `ArgumentParser.add_argument()`, except for `action` and `dest`.

        @param name     The name of the boolean argument, used to
                        construct the option names and destination field
                        name.
        @param default  Default setting for the option when not present
                        on the command line.
        """
        exclusion_group = self.add_mutually_exclusive_group()
        exclusion_group.add_argument('--%s' % name, action='store_true',
                                     dest=name, **kwargs)
        exclusion_group.add_argument('--no%s' % name, action='store_false',
                                     dest=name, **kwargs)
        self.set_defaults(**{name: bool(default)})


def _add_common_options(parser):
    # frontend.AFE(server=None) will use the default web server,
    # so default for --web is `None`.
    parser.add_argument('-w', '--web', metavar='SERVER', default=None,
                        help='specify web server')
    parser.add_argument('-d', '--dir', dest='logdir',
                        help='directory for logs')
    parser.add_argument('-n', '--dry-run', action='store_true',
                        help='apply no changes, install nothing')
    parser.add_argument('-i', '--build',
                        help='select stable test build version')
    parser.add_argument('-f', '--hostname_file',
                        help='CSV file that contains a list of hostnames and '
                             'their details to install with.')


def _add_upload_option(parser, default):
    """Add a boolean option for whether to upload logs.

    @param parser   _ArgumentParser instance.
    @param default  Default option value.
    """
    parser.add_boolean_argument('upload', default,
                                help='whether to upload logs to GS bucket')


def _add_subcommand(subcommands, name, upload_default, description):
    """Add a subcommand plus standard arguments to the `deploy` command.

    This creates a new argument parser for a subcommand (as for
    `subcommands.add_parser()`).  The parser is populated with the
    standard arguments required by all `deploy` subcommands.

    @param subcommands      Subcommand object as returned by
                            `ArgumentParser.add_subcommands`
    @param name             Name of the new subcommand.
    @param upload_default   Default setting for the `--upload` option.
    @param description      Description for the subcommand, for help text.
    @returns The argument parser for the new subcommand.
    """
    subparser = subcommands.add_parser(name, description=description)
    _add_common_options(subparser)
    _add_upload_option(subparser, upload_default)
    subparser.add_argument('-b', '--board', metavar='BOARD',
                           help='board for DUTs to be installed')
    subparser.add_argument('-m', '--model', metavar='MODEL',
                           help='model for DUTs to be installed.')
    subparser.add_argument('hostnames', nargs='*', metavar='HOSTNAME',
                           help='host names of DUTs to be installed')
    return subparser


def _add_servo_subcommand(subcommands):
    """Add the `servo` subcommand to `subcommands`.

    @param subcommands  Subcommand object as returned by
                        `ArgumentParser.add_subcommands`
    """
    subparser = _add_subcommand(
        subcommands, 'servo', True,
        'Test servo and install the image on the USB stick')
    subparser.set_defaults(stageusb=True,
                           labstation=False,
                           install_firmware=False,
                           install_test_image=False,
                           reinstall_test_image=False)


def _add_stageusb_option(parser):
    """Add a boolean option for whether to stage an image to USB.

    @param parser   _ArgumentParser instance.
    """
    parser.add_boolean_argument('stageusb', False,
                                help='Include USB stick setup')


def _add_firmware_subcommand(subcommands):
    """Add the `firmware` subcommand to `subcommands`.

    @param subcommands  Subcommand object as returned by
                        `ArgumentParser.add_subcommands`
    """
    subparser = _add_subcommand(
        subcommands, 'firmware', True,
        'Install firmware and initial test image on DUT')
    _add_stageusb_option(subparser)
    subparser.add_argument(
            '--using-servo', action='store_true',
            help='Flash DUT firmware directly using servo')
    subparser.set_defaults(labstation=False,
                           install_firmware=True,
                           install_test_image=True,
                           reinstall_test_image=False)


def _add_test_image_subcommand(subcommands):
    """Add the `test-image` subcommand to `subcommands`.

    @param subcommands  Subcommand object as returned by
                        `ArgumentParser.add_subcommands`
    """
    subparser = _add_subcommand(
        subcommands, 'test-image', True,
        'Install initial test image on DUT from servo')
    _add_stageusb_option(subparser)
    subparser.set_defaults(labstation=False,
                           install_firmware=False,
                           install_test_image=True,
                           reinstall_test_image=False)


def _add_repair_subcommand(subcommands):
    """Add the `repair` subcommand to `subcommands`.

    @param subcommands  Subcommand object as returned by
                        `ArgumentParser.add_subcommands`
    """
    subparser = _add_subcommand(
        subcommands, 'repair', False,
        'Re-install test image on DUT from servo')
    _add_stageusb_option(subparser)
    subparser.set_defaults(labstation=False,
                           install_firmware=False,
                           install_test_image=False,
                           reinstall_test_image=True)


def _add_labstation_subcommand(subcommands):
    """Add the `labstation` subcommand to `subcommands`.

    @param subcommands  Subcommand object as returned by
                        `ArgumentParser.add_subcommands`
    """
    subparser = _add_subcommand(
        subcommands, 'labstation', False,
        'Deploy a labstation to autotest, the labstation must be already'
        ' imaged with a labstation test image.')
    subparser.set_defaults(labstation=True,
                           install_firmware=False,
                           install_test_image=False,
                           reinstall_test_image=False)


def parse_command(argv):
    """Parse arguments for the `deploy` command.

    Create an argument parser for the `deploy` command and its
    subcommands.  Then parse the command line arguments, and return an
    `argparse.Namespace` object with the results.

    @param argv         Standard command line argument vector;
                        argv[0] is assumed to be the command name.
    @return `Namespace` object with standard fields as described in the
            module docstring.
    """
    parser = _ArgumentParser(
            prog=os.path.basename(argv[0]),
            description='DUT deployment and repair operations')
    subcommands = parser.add_subparsers()
    _add_servo_subcommand(subcommands)
    _add_firmware_subcommand(subcommands)
    _add_test_image_subcommand(subcommands)
    _add_repair_subcommand(subcommands)
    _add_labstation_subcommand(subcommands)
    return parser.parse_args(argv[1:])
