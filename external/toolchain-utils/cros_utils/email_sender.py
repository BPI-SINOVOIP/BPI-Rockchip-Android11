#!/usr/bin/env python3
# -*- coding: utf-8 -*-

# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Utilities to send email either through SMTP or SendGMR."""

from __future__ import print_function

import base64
import contextlib
import datetime
import getpass
import json
import os
import smtplib
import tempfile
from email import encoders as Encoders
from email.mime.base import MIMEBase
from email.mime.multipart import MIMEMultipart
from email.mime.text import MIMEText

from cros_utils import command_executer

X20_PATH = '/google/data/rw/teams/c-compiler-chrome/prod_emails'


@contextlib.contextmanager
def AtomicallyWriteFile(file_path):
  temp_path = file_path + '.in_progress'
  try:
    with open(temp_path, 'w') as f:
      yield f
    os.rename(temp_path, file_path)
  except:
    os.remove(temp_path)
    raise


class EmailSender(object):
  """Utility class to send email through SMTP or SendGMR."""

  class Attachment(object):
    """Small class to keep track of attachment info."""

    def __init__(self, name, content):
      self.name = name
      self.content = content

  def SendX20Email(self,
                   subject,
                   identifier,
                   well_known_recipients=(),
                   direct_recipients=(),
                   text_body=None,
                   html_body=None):
    """Enqueues an email in our x20 outbox.

    These emails ultimately get sent by the machinery in
    //depot/google3/googleclient/chrome/chromeos_toolchain/mailer/mail.go. This
    kind of sending is intended for accounts that don't have smtp or gmr access
    (e.g., role accounts), but can be used by anyone with x20 access.

    All emails are sent from `mdb.c-compiler-chrome+${identifier}@google.com`.

    Args:
      subject: email subject. Must be nonempty.
      identifier: email identifier, or the text that lands after the `+` in the
                  "From" email address. Must be nonempty.
      well_known_recipients: a list of well-known recipients for the email.
                             These are translated into addresses by our mailer.
                             Current potential values for this are ('sheriff',
                             'cwp-team', 'cros-team', 'mage'). Either this or
                             direct_recipients must be a nonempty list.
      direct_recipients: @google.com emails to send addresses to. Either this
                         or well_known_recipients must be a nonempty list.
      text_body: a 'text/plain' email body to send. Either this or html_body
                 must be a nonempty string. Both may be specified
      html_body: a 'text/html' email body to send. Either this or text_body
                 must be a nonempty string. Both may be specified
    """
    # `str`s act a lot like tuples/lists. Ensure that we're not accidentally
    # iterating over one of those (or anything else that's sketchy, for that
    # matter).
    if not isinstance(well_known_recipients, (tuple, list)):
      raise ValueError('`well_known_recipients` is unexpectedly a %s' %
                       type(well_known_recipients))

    if not isinstance(direct_recipients, (tuple, list)):
      raise ValueError(
          '`direct_recipients` is unexpectedly a %s' % type(direct_recipients))

    if not subject or not identifier:
      raise ValueError('both `subject` and `identifier` must be nonempty')

    if not (well_known_recipients or direct_recipients):
      raise ValueError('either `well_known_recipients` or `direct_recipients` '
                       'must be specified')

    for recipient in direct_recipients:
      if not recipient.endswith('@google.com'):
        raise ValueError('All recipients must end with @google.com')

    if not (text_body or html_body):
      raise ValueError('either `text_body` or `html_body` must be specified')

    email_json = {
        'email_identifier': identifier,
        'subject': subject,
    }

    if well_known_recipients:
      email_json['well_known_recipients'] = well_known_recipients

    if direct_recipients:
      email_json['direct_recipients'] = direct_recipients

    if text_body:
      email_json['body'] = text_body

    if html_body:
      email_json['html_body'] = html_body

    # The name of this has two parts:
    # - An easily sortable time, to provide uniqueness and let our emailer
    #   send things in the order they were put into the outbox.
    # - 64 bits of entropy, so two racing email sends don't clobber the same
    #   file.
    now = datetime.datetime.utcnow().isoformat('T', 'seconds') + 'Z'
    entropy = base64.urlsafe_b64encode(os.getrandom(8))
    entropy_str = entropy.rstrip(b'=').decode('utf-8')
    result_path = os.path.join(X20_PATH, now + '_' + entropy_str + '.json')

    with AtomicallyWriteFile(result_path) as f:
      json.dump(email_json, f)

  def SendEmail(self,
                email_to,
                subject,
                text_to_send,
                email_cc=None,
                email_bcc=None,
                email_from=None,
                msg_type='plain',
                attachments=None):
    """Choose appropriate email method and call it."""
    if os.path.exists('/usr/bin/sendgmr'):
      self.SendGMREmail(email_to, subject, text_to_send, email_cc, email_bcc,
                        email_from, msg_type, attachments)
    else:
      self.SendSMTPEmail(email_to, subject, text_to_send, email_cc, email_bcc,
                         email_from, msg_type, attachments)

  def SendSMTPEmail(self, email_to, subject, text_to_send, email_cc, email_bcc,
                    email_from, msg_type, attachments):
    """Send email via standard smtp mail."""
    # Email summary to the current user.
    msg = MIMEMultipart()

    if not email_from:
      email_from = os.path.basename(__file__)

    msg['To'] = ','.join(email_to)
    msg['Subject'] = subject

    if email_from:
      msg['From'] = email_from
    if email_cc:
      msg['CC'] = ','.join(email_cc)
      email_to += email_cc
    if email_bcc:
      msg['BCC'] = ','.join(email_bcc)
      email_to += email_bcc

    msg.attach(MIMEText(text_to_send, msg_type))
    if attachments:
      for attachment in attachments:
        part = MIMEBase('application', 'octet-stream')
        part.set_payload(attachment.content)
        Encoders.encode_base64(part)
        part.add_header('Content-Disposition',
                        'attachment; filename="%s"' % attachment.name)
        msg.attach(part)

    # Send the message via our own SMTP server, but don't include the
    # envelope header.
    s = smtplib.SMTP('localhost')
    s.sendmail(email_from, email_to, msg.as_string())
    s.quit()

  def SendGMREmail(self, email_to, subject, text_to_send, email_cc, email_bcc,
                   email_from, msg_type, attachments):
    """Send email via sendgmr program."""
    ce = command_executer.GetCommandExecuter(log_level='none')

    if not email_from:
      email_from = getpass.getuser() + '@google.com'

    to_list = ','.join(email_to)

    if not text_to_send:
      text_to_send = 'Empty message body.'

    to_be_deleted = []
    try:
      with tempfile.NamedTemporaryFile(
          'w', encoding='utf-8', delete=False) as f:
        f.write(text_to_send)
        f.flush()
      to_be_deleted.append(f.name)

      # Fix single-quotes inside the subject. In bash, to escape a single quote
      # (e.g 'don't') you need to replace it with '\'' (e.g. 'don'\''t'). To
      # make Python read the backslash as a backslash rather than an escape
      # character, you need to double it. So...
      subject = subject.replace("'", "'\\''")

      if msg_type == 'html':
        command = ("sendgmr --to='%s' --from='%s' --subject='%s' "
                   "--html_file='%s' --body_file=/dev/null" %
                   (to_list, email_from, subject, f.name))
      else:
        command = ("sendgmr --to='%s' --from='%s' --subject='%s' "
                   "--body_file='%s'" % (to_list, email_from, subject, f.name))

      if email_cc:
        cc_list = ','.join(email_cc)
        command += " --cc='%s'" % cc_list
      if email_bcc:
        bcc_list = ','.join(email_bcc)
        command += " --bcc='%s'" % bcc_list

      if attachments:
        attachment_files = []
        for attachment in attachments:
          if '<html>' in attachment.content:
            report_suffix = '_report.html'
          else:
            report_suffix = '_report.txt'
          with tempfile.NamedTemporaryFile(
              'w', encoding='utf-8', delete=False, suffix=report_suffix) as f:
            f.write(attachment.content)
            f.flush()
          attachment_files.append(f.name)
        files = ','.join(attachment_files)
        command += " --attachment_files='%s'" % files
        to_be_deleted += attachment_files

      # Send the message via our own GMR server.
      status = ce.RunCommand(command)
      return status

    finally:
      for f in to_be_deleted:
        os.remove(f)
