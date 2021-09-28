# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import logging

from autotest_lib.client.common_lib.cros import assistant_util
# TODO (crbug.com/949874): Remove this when we make sure assistant_util_private
# is available.
try:
    from autotest_lib.client.common_lib.cros import assistant_util_private
except ImportError:
    logging.error("Failed to import assistant_util_private")

class AssistantNativeError(Exception):
    """Error in AssistantFacadeNative."""
    pass

class AssistantFacadeNative(object):
    """Facade to access the assistant-related functionality.

    The methods inside this class only accept Python native types.

    """
    def __init__(self, resource):
        self._resource = resource


    def restart_chrome_for_assistant(self, enable_dsp_hotword=True):
        """Restarts Chrome with Google assistant enabled.

        @param enable_dsp_hotword: A bool to control the usage of dsp for
                hotword.
        """
        # TODO (paulhsia): Remove this when voice command is ready for non
        # gaia_login environment.
        cred = assistant_util_private.get_login_credential()
        custom_chrome_setup = {
                "autotest_ext": True,
                "gaia_login": True,
                "enable_assistant": True,
                "username": cred.username,
                "password": cred.password,
        }

        if enable_dsp_hotword:
            custom_chrome_setup["extra_browser_args"] = (
                ["--enable-features=EnableDspHotword"])
        self._resource.start_custom_chrome(custom_chrome_setup)


    def send_text_query(self, text):
        """Sends text query to Google assistant and gets response.

        @param text: A str object for text qeury.

        @returns: A str object for query response.
        """
        ext = self._resource.get_extension()
        return assistant_util.send_text_query(ext, text)


    def enable_hotword(self):
        """Enables hotword in Google assistant."""
        ext = self._resource.get_extension()
        assistant_util.enable_hotword(ext)
