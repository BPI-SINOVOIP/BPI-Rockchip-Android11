#!/usr/bin/env python3
#
#   Copyright 2018 - The Android Open Source Project
#
#   Licensed under the Apache License, Version 2.0 (the "License");
#   you may not use this file except in compliance with the License.
#   You may obtain a copy of the License at
#
#       http://www.apache.org/licenses/LICENSE-2.0
#
#   Unless required by applicable law or agreed to in writing, software
#   distributed under the License is distributed on an "AS IS" BASIS,
#   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#   See the License for the specific language governing permissions and
#   limitations under the License.

"""Apollo's event logs regexp for each button action."""

EVENT_REGEX = (
  r'(?P<time_stamp>\d+)\s(?P<log_level>[A-Z])(.*)State Match(.*)\r\n')
VOL_CHANGE_REGEX = (
  r'(?P<time_stamp>\d+)\sVolume = (?P<vol_level>\d+)(.*)\r\n')
VOLUP_REGEX = (
  r'(?P<time_stamp>\d+)\s(?P<log_level>[A-Z])(.*)State Match(.*)3202(.*)\r\n')
VOLDOWN_REGEX = (
  r'(?P<time_stamp>\d+)\s(?P<log_level>[A-Z])(.*)State Match(.*)320a(.*)\r\n')
AVRCP_PLAY_REGEX = (r'(?P<time_stamp>\d+)\sAVRCP '
                    r'play\r\n')
AVRCP_PAUSE_REGEX = (r'(?P<time_stamp>\d+)\sAVRCP '
                     r'paused\r\n')
MIC_OPEN_EVENT_REGEX = (
  r'(?P<time_stamp>\d+)\s(?P<log_level>[A-Z])(.*)State Match(.*)'
  r'\[3206\](.*)\r\n')
MIC_CLOSE_EVENT_REGEX = (
  r'(?P<time_stamp>\d+)\s(?P<log_level>[A-Z])(.*)State Match(.*)'
  r'\[3207\](.*)\r\n')
PREV_TRACK_EVENT_REGEX = (
  r'(?P<time_stamp>\d+)\s(?P<log_level>[A-Z])(.*)State Match(.*)'
  r'\[3208\](.*)\r\n')
PREV_CHANNEL_EVENT_REGEX = (
  r'(?P<time_stamp>\d+)\s(?P<log_level>[A-Z])(.*)State Match(.*)'
  r'\[3209\](.*)\r\n')
NEXT_TRACK_EVENT_REGEX = (
  r'(?P<time_stamp>\d+)\s(?P<log_level>[A-Z])(.*)State Match(.*)'
  r'\[3200\](.*)\r\n')
NEXT_CHANNEL_EVENT_REGEX = (
  r'(?P<time_stamp>\d+)\s(?P<log_level>[A-Z])(.*)State Match(.*)'
  r'\[3201\](.*)\r\n')
FETCH_NOTIFICATION_EVENT_REGEX = (
  r'(?P<time_stamp>\d+)\s(?P<log_level>[A-Z])(.*)State Match(.*)'
  r'\[3205\](.*)\r\n')
VOICE_CMD_COMPLETE_REGEX = (
  r'(?P<time_stamp>\d+)\s(?P<log_level>[A-Z])\sDspOnVoiceCommandComplete\r\n')
VOICE_CMD_START_REGEX = (
  r'(?P<time_stamp>\d+)\s(?P<log_level>[A-Z])\sDspStartVoiceCommand(.*)\r\n')
MIC_OPEN_PROMT_REGEX = (
  r'(?P<time_stamp>\d+)\s(?P<log_level>[A-Z])(.*)AudioPromptPlay 33(.*)\r\n')
MIC_CLOSE_PROMT_REGEX = (
  r'(?P<time_stamp>\d+)\s(?P<log_level>[A-Z])(.*)AudioPromptPlay 34(.*)\r\n')
POWER_ON_EVENT_REGEX = (
  r'(?P<time_stamp>\d+)\s(?P<log_level>[A-Z]) --hello--(.*)PowerOn(.*)\r\n')
POWER_OFF_EVENT_REGEX = (
  r'(?P<time_stamp>\d+)\s(?P<log_level>[A-Z]) EvtAW:320d(.*)\r\n')
