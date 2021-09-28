# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""

Helper file for handling policy fetching/parsing/formatting. This file has
working unitests located in ${THIS_FILE_NAME}_unittest.py. If you modify this
file, add/fix/adjust the unittests for proper code coverage and
re-run.

Read instructions in the unittest file on how to run.

"""
import json
import time

from autotest_lib.client.common_lib import error
# Default settings for managed user policies


def get_all_policies(autotest_ext):
    """Returns a dict of all the policies on the device."""

    policy_data = _get_pol_from_api(autotest_ext)
    if not policy_data:
        raise error.TestError('API did not return policy data!')
    _reformat_policies(policy_data)

    return policy_data


def _get_pol_from_api(autotest_ext):
    """Call the getAllPolicies API, return raw result, clear stored data."""
    new_promise = '''new Promise(function(resolve, reject) {
        chrome.autotestPrivate.getAllEnterprisePolicies(function(policies) {
          if (chrome.runtime.lastError) {
            reject(new Error(chrome.runtime.lastError.message));
          } else {
            resolve(policies);
          }
        })
    })'''
    policy_data = autotest_ext.EvaluateJavaScript(new_promise, promise=True)
    return policy_data


def _wait_for_new_pols_after_refresh(autotest_ext):
    """
    Wait up to 1 second for the polices to update when refreshing.

    Note: This is non-breaking, thus even if the policies do not update, once
    the timeout expires, the function will exit.

    """
    prior = _get_pol_from_api(autotest_ext)
    _call_refresh_policies_from_api(autotest_ext)
    t1 = time.time()
    while time.time() - t1 < 1:
        curr = _get_pol_from_api(autotest_ext)
        if curr != prior:
            break


def _call_refresh_policies_from_api(autotest_ext):
    """Call the refreshEnterprisePolicies from autotestPrivate."""
    new_promise = '''new Promise(function(resolve, reject) {
        chrome.autotestPrivate.refreshEnterprisePolicies(function() {
          if (chrome.runtime.lastError) {
            reject(new Error(chrome.runtime.lastError.message));
          } else {
            resolve();
          }
        })
    })'''
    autotest_ext.EvaluateJavaScript(new_promise, promise=True)


def refresh_policies(autotest_ext, wait_for_new=False):
    """Force a policy fetch."""

    if wait_for_new:
        _wait_for_new_pols_after_refresh(autotest_ext)
    else:
        _call_refresh_policies_from_api(autotest_ext)


def _reformat_policies(policy_dict):
    """
    Reformat visually formatted dicts to type dict (and not unicode).

    Given a dict, check if 'value' is a key in the value field. If so, check
    if the data of ['value'] is unicode. If it is, attempt to load it as json.
    If not, but the value field is a dict, recursively call this function to
    check again.

    @param: policy_dict, a policy dictionary.

    """
    for k, v in policy_dict.items():
        if not v:
            # No data
            continue
        if 'value' in v:
            if type(v['value']) == unicode:
                _remove_visual_formatting(v)
        elif isinstance(v, dict):
            _reformat_policies(v)


def _remove_visual_formatting(policy_value):
    """
    Attempt to remove any visual formatting.

    Note: policy_value can be either unicode dict or string. If json.loads()
    raises a ValueError, it is a string, and we do not need to format.

    """
    try:
        policy_value['value'] = json.loads(policy_value['value'])
    except ValueError:
        return
