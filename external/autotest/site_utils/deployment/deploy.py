#!/usr/bin/env python2
# Copyright 2015 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Main routine for the `deploy` command line tool.

# Purpose
This command automates key steps for several use cases relating to DUTs
in the Autotest Test Lab:
  * Deploying a DUT+Servo assembly with a DUT that is fresh from the
    factory.
  * Redeploying an existing DUT+Servo assembly to a new location.
  * Manually repairing a DUT+Servo assembly that has failed automated
    repair.

# Syntax

deploy <subcommand> [options] [HOSTNAME ...]

## Available subcommands:
  servo:  Validate that the servo is in working order, and then install
      the repair image for the target DUTs on the servo's USB stick.
  firmware:  Install dev-signed RO+RW firmware on the target DUTs, and
      then install the image from the servo USB stick to the target
      DUTs.
  test-image:  Install the image from the servo USB stick to the target
      DUTs.
  repair:  Install the image from the servo USB stick to the target
      DUTs.  Differs from the 'test-image' subcommand in that certain
      default behaviors are different.

For all subcommands, the servo part of the assembly must be fully
functional for deployment to succeed.

For all subcommands except the `servo` subcommand, installing the
current repair imge on the servo's USB stick may be skipped to save
time.  If this step is skipped, the user is responsible for making
sure the correct image is on the stick prior to running the command.

For the `servo` subcommand, the DUT need not be present or in working
order.  Other subcommands require the DUT to meet certain requirements,
outlined below.

For the `firmware` subcommand, the DUT must begin in dev-mode, with
hardware write-protect disabled.  At successful completion, the DUT is
in verified boot mode.

For the `test-image` and `repair` subcommands, the DUT must already have
dev-signed firmware installed, and must be in verified boot mode.

## Available options:

-w / --web SERVER
    Specify an alternative AFE RPC service.

-d / --dir DIRECTORY
    Specify a directory where logs from the command will be stored.
    By default, a new directory will be created under ~/Documents.

-i / --build BUILD
    Install the given BUILD onto the servo USB stick, and update the AFE
    to make that build the default repair image for the target DUTS.
    BUILD is specified in a form like 'R66-10447.0.0'.

-f / --hostname_file FILE
    Specifies a CSV formatted file with information about the target DUTs.
    When supplied, this overrides any HOSTNAME arguments on the command
    line.

-b / --board BOARD
    Specifies the board to assume for all target DUTs.

-m / --model MODEL
    Specifies the model to assume for all target DUTs.

--[no]stageusb
    This option isn't available for the `servo` subcommand.  For other
    subcommands, when true this option enables the servo validation and
    installation steps performed by the `servo` subcommand.

## Command line arguments:

HOSTNAME ...
    If no `-f` option is supplied, the command line must have a list of
    the hostnames of the target DUTs.
"""

import sys

import common
from autotest_lib.site_utils.deployment import cmdparse
from autotest_lib.site_utils.deployment import install


def main(argv):
    """Standard main routine.

    @param argv  Command line arguments including `sys.argv[0]`.
    """
    install.install_duts(cmdparse.parse_command(argv))


if __name__ == '__main__':
    try:
        main(sys.argv)
    except KeyboardInterrupt:
        pass
    except EnvironmentError as e:
        sys.stderr.write('Unexpected OS error:\n    %s\n' % e)
    except Exception as e:
        sys.stderr.write('Unexpected exception:\n    %s\n' % e)
