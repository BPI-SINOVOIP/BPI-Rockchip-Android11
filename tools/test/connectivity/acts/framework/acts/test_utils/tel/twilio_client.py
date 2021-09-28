#! /usr/bin/env python3
# Copyright (C) 2016 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License"); you may not
# use this file except in compliance with the License. You may obtain a copy of
# the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
# WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
# License for the specific language governing permissions and limitations under
# the License.

"""The twilio client that initiates the call."""

# TODO(danielvernon):Generalize client to use any service including phone.

from twilio.rest import Client
import yaml

ACCOUNT_SID_KEY = 'account_sid'
AUTH_TOKEN_KEY = 'auth_token'
PHONE_NUMBER_KEY = 'phone_number'
MUSIC_URL = 'http://twimlets.com/holdmusic?Bucket=com.twilio.music.ambient'
URLS_KEY = 'urls'

class TwilioClient:
    """A class that wraps the Twilio Client class and can make calls.

    Attributes:
         __account_sid: The account id
         __auth_token: The authentication token
         __phone_number: The phoone number
         urls: urls that will be played during call
    """

    def __init__(self, cfg_path):
        self.__account_sid = None
        self.__auth_token = None
        self.__phone_number = None
        self.urls = None

        self.load_config(cfg_path)
        self.client = Client(self.__account_sid, self.__auth_token)
        self.call_handle = self.client.api.account.calls

    def load_config(self, cfg_path):
        """Loads the config for twilio.

        Args:
            cfg_path: A string, which is the path to the config file.
        """
        with open(cfg_path) as cfg_file:
            cfg = yaml.load(cfg_file)
            self.__account_sid = cfg[ACCOUNT_SID_KEY]
            self.__auth_token = cfg[AUTH_TOKEN_KEY]
            self.__phone_number = cfg[PHONE_NUMBER_KEY]
            self.urls = cfg[URLS_KEY]

    def call(self, to):
        """Makes request to Twilio API to call number and play music.

        Must be registered with Twilio account in order to use this client.
        Arguments:
            to: the number to call (str). example -- '+12345678910'

        Returns:
            call.sid: the sid of the call request.
        """

        call = self.call_handle.create(to=to, from_=self.__phone_number,
                                       url=MUSIC_URL, status_callback=MUSIC_URL)
        return call.sid
