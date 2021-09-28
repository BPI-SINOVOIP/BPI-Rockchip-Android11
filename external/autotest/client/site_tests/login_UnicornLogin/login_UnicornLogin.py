# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging

from autotest_lib.client.bin import test
from autotest_lib.client.cros import cryptohome
from autotest_lib.client.common_lib import error
from autotest_lib.client.common_lib.cros import chrome
try:
    # Importing this private util fails on public boards (e.g amd64-generic)
    from autotest_lib.client.common_lib.cros import password_util
except ImportError:
    logging.error('Failed to import password_util from autotest-private')


class login_UnicornLogin(test.test):
  """Sign into a unicorn account."""
  version = 1


  def run_once(self):
    """Test function body."""


    with chrome.Chrome(auto_login=False,
                       disable_gaia_services=False) as cr:
      parent = password_util.get_unicorn_parent_credentials()
      child = password_util.get_unicorn_child_credentials()
      cr.browser.oobe.NavigateUnicornLogin(
          child_user=child.username, child_pass=child.password,
          parent_user=parent.username, parent_pass=parent.password)
      if not cryptohome.is_vault_mounted(
          user=chrome.NormalizeEmail(child.username)):
        raise error.TestFail('Expected to find a mounted vault for %s'
                             % child.username)
      tab = cr.browser.tabs.New()
      # TODO(achuith): Use a better signal of being logged in, instead of
      # parsing accounts.google.com.
      tab.Navigate('http://accounts.google.com')
      tab.WaitForDocumentReadyStateToBeComplete()
      res = tab.EvaluateJavaScript(
          '''
              var res = '',
              divs = document.getElementsByTagName('div');
              for (var i = 0; i < divs.length; i++) {
                res = divs[i].textContent;
                if (res.search('%s') > 1) {
                  break;
                }
              }
              res;
          ''' % child.username.lower())
      if not res:
        raise error.TestFail('No references to %s on accounts page.'
                             % child.username)
      tab.Close()
