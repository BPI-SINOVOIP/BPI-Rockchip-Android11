#!/usr/bin/python2
#
# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""
Call `cros_config` from the DUT while respecting new fallback commands.

When `cros_config` is called on a non-Unibuild DUT, the usual config
files (and thus behavior) are not available, so it resorts to plan B.
The following file's kCommandMap contains a list of fallback commands:
    platform2/chromeos-config/libcros_config/cros_config_fallback.cc
If the requested cros_config path/property are mapped to a fallback
command, then that command is called, and its results is returned.
That behavior is all native to cros_config.

Let's say you define a new fallback command in cros_config_fallback.cc:
    `cros_config /foo bar` --> `mosys baz quux`
You then call `cros_config /foo bar` during an Autotest test.
But, alas! Your test breaks on non-Unibuild boards during qual tests!
Why? Your new fallback needs to get promoted from ToT to Canary to Dev
to Beta to Stable. In contrast, Autotest is typically run from whatever
version is available: often ToT.
So, ToT Autotest runs `cros_config /foo bar` on a DUT with a Dev image.
`cros_config` looks for a fallback command, but cannot find the new one.
It returns the empty string and an error code of 1, and your test fails.

This file serves to replicate the fallbacks in cros_config_fallback.cc,
so that you don't have to wait for your new fallbacks to get promoted.

"""

import logging

import common
from autotest_lib.client.common_lib import error

FALLBACKS = {
        '/firmware image-name': 'mosys platform model',
        '/ name': 'mosys platform model',
        '/ brand-code': 'mosys platform brand',
        '/identity sku-id': 'mosys platform sku',
        '/identity platform-name': 'mosys platform name'
}


def get_fallback(cros_config_args):
    """
    Get the fallback command for a cros_config command, if one exists.
    NOTE: Does not strip trailing newlines.

    @param cros_config_args: '$path $property', as used by cros_config
    @return: The fallback command if one exists, else empty string

    @type cros_config_args: string
    @rtype: string | None

    """
    return FALLBACKS.get(cros_config_args)


def call_cros_config_with_fallback(cros_config_args, run, **run_kwargs):
    """
    Try to call cros_config via a supplied run function.
    If it fails, and a fallback command exists, call its fallback.
    In order to replicate the behavior of the real cros_config, we
    (attempt to) strip exactly one trailing \n from the fallback result.

    @param cros_config_args: '$path $property', as used by cros_config
    @param run: A function which passes a command to the DUT, and
               returns a client.common_lib.utils.CmdResult object.
    @param **run_kwargs: Any kwargs to be passed into run()
    @return: The CmdResult of either cros_config or its fallback,
             whichever ran more recently.

    @type cros_config: string
    @type run: func(string, **kwargs) -> CmdResult
    @rtype: client.common_lib.utils.CmdResult

    @raises CmdError or CmdTimeoutError if either:
            1. cros_config raised one, and there was no fallback, or
            2. the fallback command raised one

    """
    cros_config_cmd = 'cros_config %s' % cros_config_args
    fallback_cmd = get_fallback(cros_config_args)
    try:
        result = run(cros_config_cmd, **run_kwargs)
    except error.CmdError as e:
        if fallback_cmd:
            logging.debug('<%s> raised an error.', cros_config_cmd)
            result = e.result_obj
        else:
            raise
    else:
        if result.exit_status:
            logging.debug('<%s> returned with an exit code.', cros_config_cmd)
        else:
            return result
    if fallback_cmd:
        logging.debug('Trying fallback cmd: <%s>', fallback_cmd)
        result = run(fallback_cmd, **run_kwargs)
        if result.stdout and result.stdout[-1] == '\n':
            result.stdout = result.stdout[:-1]
    else:
        logging.debug('No fallback command found for <%s>.', cros_config_cmd)
    return result


def call_cros_config_get_output(cros_config_args, run, **run_kwargs):
    """
    Get the stdout from cros_config (or its fallback).

    @param cros_config_args: '$path $property', as used by cros_config
    @param run: A function which passes a command to the DUT, and
               returns a client.common_lib.utils.CmdResult object.
    @param **run_kwargs: Any kwargs to be passed into run()
    @return: The string stdout of either cros_config or its fallback,
             or empty string in the case of error.

    @type cros_config: string
    @type run: func(string, **kwargs) -> CmdResult
    @rtype: string

    """
    try:
        result = call_cros_config_with_fallback(cros_config_args, run,
                                                **run_kwargs)
    except error.CmdError:
        return ''
    if result.exit_status:
        return ''
    return result.stdout
