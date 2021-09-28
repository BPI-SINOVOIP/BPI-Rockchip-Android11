#!/usr/bin/env python3
# -*- coding: utf-8 -*-

# Copyright 2020 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Tests for email_sender."""

from __future__ import print_function

import contextlib
import io
import json
import unittest
import unittest.mock as mock

import cros_utils.email_sender as email_sender


class Test(unittest.TestCase):
  """Tests for email_sender."""

  @mock.patch('cros_utils.email_sender.AtomicallyWriteFile')
  def test_x20_email_sending_rejects_invalid_inputs(self, write_file):
    test_cases = [
        {
            # no subject
            'subject': '',
            'identifier': 'foo',
            'direct_recipients': ['gbiv@google.com'],
            'text_body': 'hi',
        },
        {
            'subject': 'foo',
            # no identifier
            'identifier': '',
            'direct_recipients': ['gbiv@google.com'],
            'text_body': 'hi',
        },
        {
            'subject': 'foo',
            'identifier': 'foo',
            # no recipients
            'direct_recipients': [],
            'text_body': 'hi',
        },
        {
            'subject': 'foo',
            'identifier': 'foo',
            'direct_recipients': ['gbiv@google.com'],
            # no body
        },
        {
            'subject': 'foo',
            'identifier': 'foo',
            # direct recipients lack @google.
            'direct_recipients': ['gbiv'],
            'text_body': 'hi',
        },
        {
            'subject': 'foo',
            'identifier': 'foo',
            # non-list recipients
            'direct_recipients': 'gbiv@google.com',
            'text_body': 'hi',
        },
        {
            'subject': 'foo',
            'identifier': 'foo',
            # non-list recipients
            'well_known_recipients': 'sheriff',
            'text_body': 'hi',
        },
    ]

    sender = email_sender.EmailSender()
    for case in test_cases:
      with self.assertRaises(ValueError):
        sender.SendX20Email(**case)

    write_file.assert_not_called()

  @mock.patch('cros_utils.email_sender.AtomicallyWriteFile')
  def test_x20_email_sending_translates_to_reasonable_json(self, write_file):
    written_obj = None

    @contextlib.contextmanager
    def actual_write_file(file_path):
      nonlocal written_obj

      self.assertTrue(
          file_path.startswith(email_sender.X20_PATH + '/'), file_path)
      f = io.StringIO()
      yield f
      written_obj = json.loads(f.getvalue())

    write_file.side_effect = actual_write_file
    email_sender.EmailSender().SendX20Email(
        subject='hello',
        identifier='world',
        well_known_recipients=['sheriff'],
        direct_recipients=['gbiv@google.com'],
        text_body='text',
        html_body='html',
    )

    self.assertEqual(
        written_obj, {
            'subject': 'hello',
            'email_identifier': 'world',
            'well_known_recipients': ['sheriff'],
            'direct_recipients': ['gbiv@google.com'],
            'body': 'text',
            'html_body': 'html',
        })


if __name__ == '__main__':
  unittest.main()
