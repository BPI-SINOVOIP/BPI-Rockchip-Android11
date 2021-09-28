# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import logging

class AssistantAdapterError(Exception):
    """Error in AssistantAdapterNative."""
    pass

class AssistantFacadeRemoteAdapter(object):
    """AssistantFacadeRemoteAdapter is an adapter to remotely control DUT
    assistant.

    The Autotest host object representing the remote DUT, passed to this
    class on initialization, can be accessed from its _client property.

    """
    def __init__(self, host, remote_facade_proxy):
        """Construct a AssistantFacadeRemoteAdapter.

        @param host: Host object representing a remote host.
        @param remote_facade_proxy: RemoteFacadeProxy object.
        """
        self._client = host
        self._proxy = remote_facade_proxy


    @property
    def _assistant_proxy(self):
        return self._proxy.assistant


    def restart_chrome_for_assistant(self, enable_dsp_hotword=True):
        """Restarts Chrome with Google assistant enabled.

        @param enable_dsp_hotword: A bool to control the usage of dsp for
                hotword.
        """
        logging.info("Restarting Chrome with Google assistant enabled.")
        self._assistant_proxy.restart_chrome_for_assistant(enable_dsp_hotword)
        logging.info("Chrome process restarted with assistant enabled.")


    def send_text_query(self, text):
        """Sends text query to Google assistant and gets respond.

        @param text: A str object for text qeury.

        @returns: A str object for query response.
        """
        return self._assistant_proxy.send_text_query(text)


    def enable_hotword(self):
        """Enable hotword feature in Google assistant."""
        logging.info("Enabling hotword in Google assistant.")
        self._assistant_proxy.enable_hotword()
