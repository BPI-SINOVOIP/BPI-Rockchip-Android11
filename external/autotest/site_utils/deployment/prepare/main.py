#!/usr/bin/python2 -u
# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Tool to (re)prepare a DUT for lab deployment."""

from __future__ import absolute_import
from __future__ import division
from __future__ import print_function

import argparse
import errno
import logging
import logging.config
import os
import sys

import common
from autotest_lib.client.common_lib import enum
from autotest_lib.server import afe_utils
from autotest_lib.server.hosts import file_store
from autotest_lib.site_utils.deployment.prepare import dut as preparedut


RETURN_CODES = enum.Enum(
        'OK',
        'STAGE_USB_FAILURE',
        'INSTALL_FIRMWARE_FAILURE',
        'INSTALL_TEST_IMAGE_FAILURE',
        'BOOT_FROM_RECOVERY_MODE_FAILURE',
        'SETUP_LABSTATION_FAILURE',
        'UPDATE_LABEL_FAILURE',
        'OTHER_FAILURES',
)


class DutPreparationError(Exception):
  """Generic error raised during DUT preparation."""


def main():
  """Tool to (re)prepare a DUT for lab deployment."""
  opts = _parse_args()
  _configure_logging('prepare_dut', os.path.join(opts.results_dir, _LOG_FILE))

  try:
    info = _read_store(opts.host_info_file)
  except Exception as err:
    logging.error("fail to prepare: %s", err)
    return RETURN_CODES.OTHER_FAILURES

  with _create_host(opts.hostname, info, opts.results_dir) as host:
    if opts.dry_run:
      logging.info('DRY RUN: Would have run actions %s', opts.actions)
      return

    if 'stage-usb' in opts.actions:
      try:
        repair_image = afe_utils.get_stable_cros_image_name_v2(info)
        logging.info('Using repair image %s, obtained from AFE', repair_image)
        preparedut.download_image_to_servo_usb(host, repair_image)
      except Exception as err:
        logging.error("fail to stage image to usb: %s", err)
        return RETURN_CODES.STAGE_USB_FAILURE

    if 'install-test-image' in opts.actions:
      try:
        preparedut.install_test_image(host)
      except Exception as err:
        logging.error("fail to install test image: %s", err)
        return RETURN_CODES.INSTALL_TEST_IMAGE_FAILURE

    if 'install-firmware' in opts.actions:
      try:
        preparedut.install_firmware(host)
      except Exception as err:
        logging.error("fail to install firmware: %s", err)
        return RETURN_CODES.INSTALL_FIRMWARE_FAILURE

    if 'verify-recovery-mode' in opts.actions:
      try:
        preparedut.verify_boot_into_rec_mode(host)
      except Exception as err:
        logging.error("fail to boot from recovery mode: %s", err)
        return RETURN_CODES.BOOT_FROM_RECOVERY_MODE_FAILURE

    if 'setup-labstation' in opts.actions:
      try:
        preparedut.setup_labstation(host)
      except Exception as err:
        logging.error("fail to setup labstation: %s", err)
        return RETURN_CODES.SETUP_LABSTATION_FAILURE

    if 'update-label' in opts.actions:
      try:
        host.labels.update_labels(host, task_name='deploy')
      except Exception as err:
        logging.error("fail to update label: %s", err)
        return RETURN_CODES.UPDATE_LABEL_FAILURE

  return RETURN_CODES.OK


_LOG_FILE = 'prepare_dut.log'
_DUT_LOGS_DIR = 'dut_logs'


def _parse_args():
  parser = argparse.ArgumentParser(
      description='Prepare / validate DUT for lab deployment.')

  parser.add_argument(
      'actions',
      nargs='+',
      choices=['stage-usb', 'install-test-image', 'install-firmware',
               'verify-recovery-mode', 'update-label', 'setup-labstation'],
      help='DUT preparation actions to execute.',
  )
  parser.add_argument(
      '--dry-run',
      action='store_true',
      default=False,
      help='Run in dry-run mode. No changes will be made to the DUT.',
  )
  parser.add_argument(
      '--results-dir',
      required=True,
      help='Directory to drop logs and output artifacts in.',
  )

  parser.add_argument(
      '--hostname',
      required=True,
      help='Hostname of the DUT to prepare.',
  )
  parser.add_argument(
      '--host-info-file',
      required=True,
      help=('Full path to HostInfo file.'
            ' DUT inventory information is read from the HostInfo file.'),
  )

  return parser.parse_args()


def _configure_logging(name, tee_file):
    """Configure logging globally.

    @param name: Name to prepend to log messages.
                 This should be the name of the program.
    @param tee_file: File to tee logs to, in addition to stderr.
    """
    logging.config.dictConfig({
        'version': 1,
        'formatters': {
            'stderr': {
                'format': ('{name}: '
                           '%(asctime)s:%(levelname)s'
                           ':%(module)s:%(funcName)s:%(lineno)d'
                           ': %(message)s'
                           .format(name=name)),
            },
            'tee_file': {
                'format': ('%(asctime)s:%(levelname)s'
                           ':%(module)s:%(funcName)s:%(lineno)d'
                           ': %(message)s'),
            },
        },
        'handlers': {
            'stderr': {
                'class': 'logging.StreamHandler',
                'formatter': 'stderr',
            },
            'tee_file': {
                'class': 'logging.FileHandler',
                'formatter': 'tee_file',
                'filename': tee_file,
            },
        },
        'root': {
            'level': 'DEBUG',
            'handlers': ['stderr', 'tee_file'],
        },
        'disable_existing_loggers': False,
    })


def _read_store(path):
  """Read a HostInfo from a file at path."""
  store = file_store.FileStore(path)
  return store.get()


def _create_host(hostname, info, results_dir):
  """Yield a hosts.CrosHost object with the given inventory information.

  @param hostname: Hostname of the DUT.
  @param info: A HostInfo with the inventory information to use.
  @param results_dir: Path to directory for logs / output artifacts.
  @yield server.hosts.CrosHost object.
  """
  if not info.board:
    raise DutPreparationError('No board in DUT labels')
  if not info.model:
    raise DutPreparationError('No model in DUT labels')

  if info.os == 'labstation':
    return preparedut.create_labstation_host(hostname, info.board, info.model)

  # We assume target host is a cros DUT by default
  if 'servo_host' not in info.attributes:
    raise DutPreparationError('No servo_host in DUT attributes')
  if 'servo_port' not in info.attributes:
    raise DutPreparationError('No servo_port in DUT attributes')

  dut_logs_dir = os.path.join(results_dir, _DUT_LOGS_DIR)
  try:
    os.makedirs(dut_logs_dir)
  except OSError as e:
    if e.errno != errno.EEXIST:
      raise

  return preparedut.create_cros_host(
      hostname,
      info.board,
      info.model,
      info.attributes['servo_host'],
      info.attributes['servo_port'],
      info.attributes.get('servo_serial'),
      dut_logs_dir,
  )


if __name__ == '__main__':
  sys.exit(main())
