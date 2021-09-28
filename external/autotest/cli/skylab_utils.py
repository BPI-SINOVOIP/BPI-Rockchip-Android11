# Copyright 2018 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Constants and util methods to interact with skylab inventory repo."""

import logging
import re

import common

from autotest_lib.client.common_lib import revision_control
from chromite.lib import gob_util

try:
    from skylab_inventory import text_manager
except ImportError:
    pass


INTERNAL_GERRIT_HOST = 'chrome-internal-review.googlesource.com'
INTERNAL_GERRIT_HOST_URL = 'https://%s' % INTERNAL_GERRIT_HOST
# The git url of the internal skylab_inventory
INTERNAL_INVENTORY_REPO_URL = ('https://chrome-internal.googlesource.com/'
                               'chromeos/infra_internal/skylab_inventory.git')
INTERNAL_INVENTORY_CHANGE_PATTERN = (
        r'https://chrome-internal-review.googlesource.com/c/chromeos/'
        'infra_internal/skylab_inventory/\\+/([0-9]*)')
MSG_INVALID_IN_SKYLAB = 'This is currently not supported with --skylab.'
MSG_ONLY_VALID_IN_SKYLAB = 'This only applies to actions on skylab inventory.'


class SkylabInventoryNotImported(Exception):
    """skylab_inventory is not imported."""


class InventoryRepoChangeNotFound(Exception):
    """Error raised when no inventory repo change number is found."""


class InventoryRepoDirNotClean(Exception):
    """Error raised when the given inventory_repo_dir contains local changes."""


def get_cl_url(change_number):
    return INTERNAL_GERRIT_HOST_URL + '/' + str(change_number)


def get_cl_message(change_number):
    return ('Please submit the CL at %s to make the change effective.' %
            get_cl_url(change_number))


def construct_commit_message(subject, bug=None, test=None):
    """Construct commit message for skylab inventory repo commit.

    @param subject: Commit message subject.
    @param bug: Bug number of the commit.
    @param test: Tests of the commit.

    @return: A commit message string.
    """
    return '\n'.join([subject, '', 'BUG=%s' % bug, 'TEST=%s' % test])


def extract_inventory_change(output):
    """Extract the change number from the output.

    @param output: The git command output containing the change gerrit url.

    @return: The change number (int) of the inventory change.
    """
    m = re.search(INTERNAL_INVENTORY_CHANGE_PATTERN, output)

    if not m:
        raise InventoryRepoChangeNotFound(
                'Could not extract CL number from "%r"' % output)

    return int(m.group(1))


def submit_inventory_change(change_number):
    """Set review labels and submit the inventory change.

    @param change_number: The change number (int) of the inventory change.
    """
    logging.info('Setting review labels for %s.',
                  get_cl_url(change_number))
    gob_util.SetReview(
        INTERNAL_GERRIT_HOST,
        change=change_number,
        labels={'Code-Review': 2, 'Verified': 1},
        msg='Set TBR by "atest --skylab"',
        notify='OWNER')

    logging.info('Submitting the change.')
    gob_util.SubmitChange(
        INTERNAL_GERRIT_HOST,
        change=change_number)


class InventoryRepo(object):
    """Class to present a inventory repository."""


    def __init__(self, inventory_repo_dir):
        self.inventory_repo_dir = inventory_repo_dir
        self.git_repo = None


    def initialize(self):
        """Initialize inventory repo at the given dir."""
        self.git_repo = revision_control.GitRepo(
                self.inventory_repo_dir,
                giturl=INTERNAL_INVENTORY_REPO_URL,
                abs_work_tree=self.inventory_repo_dir)

        if self.git_repo.is_repo_initialized():
            if self.git_repo.status():
                raise InventoryRepoDirNotClean(
                       'The inventory_repo_dir "%s" contains uncommitted '
                       'changes. Please clean up the local repo directory or '
                       'use another clean directory.' % self.inventory_repo_dir)

            logging.info('Inventory repo was already initialized, start '
                         'pulling.')
            self.git_repo.checkout('master')
            self.git_repo.pull()
        else:
            logging.info('No inventory repo was found, start cloning.')
            self.git_repo.clone(shallow=True)


    def get_data_dir(self, data_subdir='skylab'):
        """Get path to the data dir."""
        return text_manager.get_data_dir(self.inventory_repo_dir, data_subdir)


    def upload_change(self, commit_message, draft=False, dryrun=False,
                      submit=False):
        """Commit and upload the change to gerrit.

        @param commit_message: Commit message of the CL to upload.
        @param draft: Boolean indicating whether to upload the CL as a draft.
        @param dryrun: Boolean indicating whether to run upload as a dryrun.
        @param submit: Boolean indicating whether to submit the CL directly.

        @return: Change number (int) of the CL if it's uploaded to Gerrit.
        """
        self.git_repo.commit(commit_message)

        remote = self.git_repo.remote()
        output = self.git_repo.upload_cl(
                remote, 'master', draft=draft, dryrun=dryrun)

        if not dryrun:
            change_number = extract_inventory_change(output)

            if submit:
                submit_inventory_change(change_number)

            return change_number
