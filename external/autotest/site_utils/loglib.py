# Copyright 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Shared logging functions for autotest drone services

autotest/site_utils/ is home to various upstart jobs and cron jobs that run on
autotest drones. All these jobs currently configure logging in different ways.
Worse, many of these scripts don't use logging at all, instead print()ing to
stdout and use external log file management.

This library provides a single consistent way to manage log configuration and
log directories for these scripts.
"""

from __future__ import absolute_import
from __future__ import division
from __future__ import print_function

import logging
import logging.config
import os


def add_logging_options(parser):
    """Add logging configuration options to argument parser.

    @param parser: ArgumentParser instance.
    """
    parser.add_argument(
            '--log-dir',
            default=None,
            help='(existing) directory to drop log files in.'
                 ' By default, logs to stderr.',
    )


def configure_logging_with_args(parser, args):
    """Convenience function for calling configure_logging().

    @param parser: ArgumentParser instance.
    @param args: Return value from ArgumentParser.parse_args().
    """
    configure_logging(parser.prog, args.log_dir)


def configure_logging(name, log_dir=None):
    """Configure logging globally.

    @param name: Name to prepend to log messages.
                 This should be the name of the program.
    @param log_dir: Path to the (existing) direcotry to create log files in.
                    If None, logs to stderr.
    """
    if log_dir is None:
        handlers = {
                'default': {
                        'class': 'logging.StreamHandler',
                        'formatter': 'default' ,
                }
        }
    else:
        handlers = {
                'default': {
                        'class': 'logging.handlers.TimedRotatingFileHandler',
                        'formatter': 'default' ,
                        'filename': os.path.join(log_dir, '%s.log' % name),
                        'when': 'midnight',
                        'backupCount': 14,
                }
        }


    logging.config.dictConfig({
            'version': 1,
            'handlers': handlers,
            'formatters': {
                    'default': {
                            'format': ('{name}: '
                                        '%(asctime)s:%(levelname)s'
                                        ':%(module)s:%(funcName)s:%(lineno)d'
                                        ': %(message)s'
                                        .format(name=name)),
                    },
            },
            'root': {
                    'level': 'INFO',
                    'handlers': ['default'],
            },
            'disable_existing_loggers': False,
    })
