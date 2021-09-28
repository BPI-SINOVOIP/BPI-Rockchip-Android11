#
# Copyright (C) 2018 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#

import logging
import pytz


def GetTimeWithTimezone(dt, timezone="US/Pacific"):
    """Converts timezone of datetime.datetime() instance.

    Args:
        dt: datetime.datetime() instance.
        timezone: a string representing timezone listed in TZ database.

    Returns:
        datetime.datetime() instance with the given timezone.
    """
    if not dt:
        return None
    utc_time = dt.replace(tzinfo=pytz.utc)
    try:
        converted_time = utc_time.astimezone(pytz.timezone(timezone))
    except pytz.UnknownTimeZoneError as e:
        logging.exception(e)
        converted_time = dt
    return converted_time
