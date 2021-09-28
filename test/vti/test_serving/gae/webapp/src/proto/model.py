#!/usr/bin/env python
#
# Copyright (C) 2017 The Android Open Source Project
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

from google.appengine.ext import ndb

from protorpc import messages
from protorpc import message_types


class BuildModel(ndb.Model):
    """A model for representing an individual build entry."""
    manifest_branch = ndb.StringProperty()
    build_id = ndb.StringProperty()
    build_target = ndb.StringProperty()
    build_type = ndb.StringProperty()
    artifact_type = ndb.StringProperty()
    artifacts = ndb.StringProperty(repeated=True)
    timestamp = ndb.DateTimeProperty(auto_now=False)
    signed = ndb.BooleanProperty()


class BuildInfoMessage(messages.Message):
    """A message for representing an individual build entry."""
    manifest_branch = messages.StringField(1)
    build_id = messages.StringField(2)
    build_target = messages.StringField(3)
    build_type = messages.StringField(4)
    artifact_type = messages.StringField(5)
    artifacts = messages.StringField(6, repeated=True)
    signed = messages.BooleanField(7)
    timestamp = message_types.DateTimeField(8)


class ScheduleControlModel(ndb.Model):
    """A model for representing a schedule control data entry."""
    enabled = ndb.BooleanProperty()
    # "global" or empty string to enable/disable all schedules.
    schedule_name = ndb.StringProperty()


class ScheduleModel(ndb.Model):
    """A model for representing an individual schedule entry."""
    # schedule name for green build schedule, optional.
    name = ndb.StringProperty()
    schedule_type = ndb.StringProperty()

    # device image information
    build_storage_type = ndb.IntegerProperty()
    manifest_branch = ndb.StringProperty()
    build_target = ndb.StringProperty()  # type:name
    device_pab_account_id = ndb.StringProperty()
    require_signed_device_build = ndb.BooleanProperty()
    has_bootloader_img = ndb.BooleanProperty(default=True)
    has_radio_img = ndb.BooleanProperty(default=True)

    # GSI information
    gsi_storage_type = ndb.IntegerProperty()
    gsi_branch = ndb.StringProperty()
    gsi_build_target = ndb.StringProperty()
    gsi_pab_account_id = ndb.StringProperty()
    gsi_vendor_version = ndb.StringProperty()

    # test suite information
    test_storage_type = ndb.IntegerProperty()
    test_branch = ndb.StringProperty()
    test_build_target = ndb.StringProperty()
    test_pab_account_id = ndb.StringProperty()

    test_name = ndb.StringProperty()
    period = ndb.IntegerProperty()
    schedule = ndb.StringProperty()
    priority = ndb.StringProperty()
    priority_value = ndb.IntegerProperty()
    device = ndb.StringProperty(repeated=True)
    shards = ndb.IntegerProperty()
    param = ndb.StringProperty(repeated=True)
    timestamp = ndb.DateTimeProperty(auto_now=False)
    retry_count = ndb.IntegerProperty()

    children_jobs = ndb.KeyProperty(kind="JobModel", repeated=True)
    error_count = ndb.IntegerProperty()
    suspended = ndb.BooleanProperty()
    image_package_repo_base = ndb.StringProperty()

    required_host_equipment = ndb.StringProperty(repeated=True)
    required_device_equipment = ndb.StringProperty(repeated=True)

    report_bucket = ndb.StringProperty(repeated=True)
    report_spreadsheet_id = ndb.StringProperty(repeated=True)
    report_persistent_url = ndb.StringProperty(repeated=True)
    report_reference_url = ndb.StringProperty(repeated=True)

    owner = ndb.StringProperty(repeated=True)


class ScheduleControlInfoMessage(messages.Message):
    """A message for representing a schedule control data entry."""
    enabled = messages.BooleanField(1)
    schedule_name = messages.StringField(2)


class ScheduleInfoMessage(messages.Message):
    """A message for representing an individual schedule entry."""
    # Next ID = 39
    # schedule name for green build schedule, optional.
    name = messages.StringField(16)
    schedule_type = messages.StringField(19)

    # device image information
    build_storage_type = messages.IntegerField(21)
    manifest_branch = messages.StringField(1)
    build_target = messages.StringField(2)
    device_pab_account_id = messages.StringField(17)
    require_signed_device_build = messages.BooleanField(20)
    has_bootloader_img = messages.BooleanField(27)
    has_radio_img = messages.BooleanField(28)

    # GSI information
    gsi_storage_type = messages.IntegerField(22)
    gsi_branch = messages.StringField(9)
    gsi_build_target = messages.StringField(10)
    gsi_pab_account_id = messages.StringField(11)
    gsi_vendor_version = messages.StringField(24)

    # test suite information
    test_storage_type = messages.IntegerField(23)
    test_branch = messages.StringField(12)
    test_build_target = messages.StringField(13)
    test_pab_account_id = messages.StringField(14)

    test_name = messages.StringField(3)
    period = messages.IntegerField(4)
    schedule = messages.StringField(18)
    priority = messages.StringField(5)
    device = messages.StringField(6, repeated=True)
    shards = messages.IntegerField(7)
    param = messages.StringField(8, repeated=True)
    retry_count = messages.IntegerField(15)

    required_host_equipment = messages.StringField(25, repeated=True)
    required_device_equipment = messages.StringField(26, repeated=True)

    report_bucket = messages.StringField(29, repeated=True)
    report_spreadsheet_id = messages.StringField(30, repeated=True)
    report_persistent_url = messages.StringField(32, repeated=True)
    report_reference_url = messages.StringField(33, repeated=True)

    image_package_repo_base = messages.StringField(31)
    timestamp = message_types.DateTimeField(34)
    owner = messages.StringField(35, repeated=True)

    suspended = messages.BooleanField(36)
    urlsafe_key = messages.StringField(37)
    error_count = messages.IntegerField(38)


class LabModel(ndb.Model):
    """A model for representing an individual lab entry."""
    name = ndb.StringProperty()
    owner = ndb.StringProperty()
    admin = ndb.StringProperty(repeated=True)
    hostname = ndb.StringProperty()
    ip = ndb.StringProperty()
    # devices is a comma-separated list of serial=product pairs
    devices = ndb.StringProperty()
    timestamp = ndb.DateTimeProperty(auto_now=False)
    vtslab_version = ndb.StringProperty()
    host_equipment = ndb.StringProperty(repeated=True)


class LabDeviceInfoMessage(messages.Message):
    """A message for representing an individual lab host's device entry."""
    serial = messages.StringField(1, repeated=False)
    product = messages.StringField(2, repeated=False)
    device_equipment = messages.StringField(3, repeated=True)


class LabHostInfoMessage(messages.Message):
    """A message for representing an individual lab's host entry."""
    hostname = messages.StringField(1, repeated=False)
    ip = messages.StringField(2, repeated=False)
    script = messages.StringField(3)
    device = messages.MessageField(LabDeviceInfoMessage, 4, repeated=True)
    vtslab_version = messages.StringField(5)
    host_equipment = messages.StringField(6, repeated=True)


class LabInfoMessage(messages.Message):
    """A message for representing an individual lab entry."""
    name = messages.StringField(1)
    owner = messages.StringField(2)
    admin = messages.StringField(4, repeated=True)
    host = messages.MessageField(LabHostInfoMessage, 3, repeated=True)


class LabMessage(messages.Message):
    """A model for representing a LabModel entity."""
    name = messages.StringField(1)
    owner = messages.StringField(2)
    admin = messages.StringField(3, repeated=True)
    hostname = messages.StringField(4)
    ip = messages.StringField(5)
    devices = messages.StringField(6)
    vtslab_version = messages.StringField(7)
    host_equipment = messages.StringField(8, repeated=True)


class DeviceModel(ndb.Model):
    """A model for representing an individual device entry."""
    hostname = ndb.StringProperty()
    product = ndb.StringProperty()
    serial = ndb.StringProperty()
    status = ndb.IntegerProperty()
    scheduling_status = ndb.IntegerProperty()
    timestamp = ndb.DateTimeProperty(auto_now=False)
    device_equipment = ndb.StringProperty(repeated=True)


class DeviceInfoMessage(messages.Message):
    """A message for representing an individual host's device entry."""
    serial = messages.StringField(1)
    product = messages.StringField(2)
    status = messages.IntegerField(3)
    scheduling_status = messages.IntegerField(4)
    hostname = messages.StringField(5)
    device_equipment = messages.StringField(6, repeated=True)
    timestamp = message_types.DateTimeField(7)


class HostInfoMessage(messages.Message):
    """A message for representing an individual host entry."""
    hostname = messages.StringField(1)
    devices = messages.MessageField(DeviceInfoMessage, 2, repeated=True)


class JobModel(ndb.Model):
    """A model for representing an individual job entry."""
    test_type = ndb.IntegerProperty()

    hostname = ndb.StringProperty()
    priority = ndb.StringProperty()
    test_name = ndb.StringProperty()
    require_signed_device_build = ndb.BooleanProperty()
    has_bootloader_img = ndb.BooleanProperty()
    has_radio_img = ndb.BooleanProperty()
    device = ndb.StringProperty()
    serial = ndb.StringProperty(repeated=True)

    # device image information
    build_storage_type = ndb.IntegerProperty()
    manifest_branch = ndb.StringProperty()
    build_target = ndb.StringProperty()
    build_id = ndb.StringProperty()
    pab_account_id = ndb.StringProperty()

    shards = ndb.IntegerProperty()
    param = ndb.StringProperty(repeated=True)
    status = ndb.IntegerProperty()
    period = ndb.IntegerProperty()

    # GSI information
    gsi_storage_type = ndb.IntegerProperty()
    gsi_branch = ndb.StringProperty()
    gsi_build_target = ndb.StringProperty()
    gsi_build_id = ndb.StringProperty()
    gsi_pab_account_id = ndb.StringProperty()
    gsi_vendor_version = ndb.StringProperty()

    # test suite information
    test_storage_type = ndb.IntegerProperty()
    test_branch = ndb.StringProperty()
    test_build_target = ndb.StringProperty()
    test_build_id = ndb.StringProperty()
    test_pab_account_id = ndb.StringProperty()

    timestamp = ndb.DateTimeProperty(auto_now=False)
    heartbeat_stamp = ndb.DateTimeProperty(auto_now=False)
    retry_count = ndb.IntegerProperty()

    infra_log_url = ndb.StringProperty()

    parent_schedule = ndb.KeyProperty(kind="ScheduleModel")

    image_package_repo_base = ndb.StringProperty()

    report_bucket = ndb.StringProperty(repeated=True)
    report_spreadsheet_id = ndb.StringProperty(repeated=True)
    report_persistent_url = ndb.StringProperty(repeated=True)
    report_reference_url = ndb.StringProperty(repeated=True)


class JobMessage(messages.Message):
    """A message for representing an individual job entry."""
    # Next ID = 39
    test_type = messages.IntegerField(29)

    hostname = messages.StringField(1)
    priority = messages.StringField(2)
    test_name = messages.StringField(3)
    require_signed_device_build = messages.BooleanField(23)
    has_bootloader_img = messages.BooleanField(31)
    has_radio_img = messages.BooleanField(32)
    device = messages.StringField(4)
    serial = messages.StringField(5, repeated=True)

    # device image information
    build_storage_type = messages.IntegerField(25)
    manifest_branch = messages.StringField(6)
    build_target = messages.StringField(7)
    build_id = messages.StringField(10)
    pab_account_id = messages.StringField(20)

    shards = messages.IntegerField(8)
    param = messages.StringField(9, repeated=True)
    status = messages.IntegerField(11)
    period = messages.IntegerField(12)

    # GSI information
    gsi_storage_type = messages.IntegerField(26)
    gsi_branch = messages.StringField(13)
    gsi_build_target = messages.StringField(14)
    gsi_build_id = messages.StringField(21)
    gsi_pab_account_id = messages.StringField(15)
    gsi_vendor_version = messages.StringField(28)

    # test suite information
    test_storage_type = messages.IntegerField(27)
    test_branch = messages.StringField(16)
    test_build_target = messages.StringField(17)
    test_build_id = messages.StringField(22)
    test_pab_account_id = messages.StringField(18)

    retry_count = messages.IntegerField(19)

    infra_log_url = messages.StringField(24)

    image_package_repo_base = messages.StringField(30)

    report_bucket = messages.StringField(33, repeated=True)
    report_spreadsheet_id = messages.StringField(34, repeated=True)
    report_persistent_url = messages.StringField(35, repeated=True)
    report_reference_url = messages.StringField(36, repeated=True)

    timestamp = message_types.DateTimeField(37)
    heartbeat_stamp = message_types.DateTimeField(38)


class ReturnCodeMessage(messages.Enum):
    """Enum for default return code."""
    SUCCESS = 0
    FAIL = 1


class DefaultResponse(messages.Message):
    """A default response proto message."""
    return_code = messages.EnumField(ReturnCodeMessage, 1)


class JobLeaseResponse(messages.Message):
    """A job lease response proto message."""
    return_code = messages.EnumField(ReturnCodeMessage, 1)
    jobs = messages.MessageField(JobMessage, 2, repeated=True)


class KeyValueModel(ndb.Model):
    """A simple key-value model.

    This class uses name as key and store one value or more than one values
    to store values which require continuous monitoring such as counters,
    or flags.
    """
    name = ndb.StringProperty()
    string_value = ndb.StringProperty()
    integer_value = ndb.IntegerProperty()
    boolean_value = ndb.BooleanProperty()


class GetRequestMessage(messages.Message):
    """A message to request entities through /get endpoints."""
    size = messages.IntegerField(1)
    offset = messages.IntegerField(2)
    filter = messages.StringField(3)
    sort = messages.StringField(4)
    direction = messages.StringField(5)


class BuildResponseMessage(messages.Message):
    """A message containing build entities to respond to /get endpoints."""
    builds = messages.MessageField(BuildInfoMessage, 1, repeated=True)
    has_next = messages.BooleanField(2)


class DeviceResponseMessage(messages.Message):
    """A message containing device entities to respond to /get endpoints."""
    devices = messages.MessageField(DeviceInfoMessage, 1, repeated=True)
    has_next = messages.BooleanField(2)


class JobResponseMessage(messages.Message):
    """A message containing job entities to respond to /get endpoints."""
    jobs = messages.MessageField(JobMessage, 1, repeated=True)
    has_next = messages.BooleanField(2)


class LabResponseMessage(messages.Message):
    """A message containing lab entities to respond to /get endpoints."""
    labs = messages.MessageField(LabMessage, 1, repeated=True)
    has_next = messages.BooleanField(2)


class ScheduleResponseMessage(messages.Message):
    """A message containing schedule entities to respond to /get endpoints."""
    schedules = messages.MessageField(ScheduleInfoMessage, 1, repeated=True)
    has_next = messages.BooleanField(2)


class CountRequestMessage(messages.Message):
    """A message to request a count of entities through /count endpoints."""
    filter = messages.StringField(1)


class CountResponseMessage(messages.Message):
    """A message of a count of entities to respond to /count endpoints."""
    count = messages.IntegerField(1)


class ScheduleSuspendMessage(messages.Message):
    """A response message to schedule endpoint API's /suspend method."""

    class SingleScheduleSuspendMessage(messages.Message):
        urlsafe_key = messages.StringField(1)
        suspend = messages.BooleanField(2)

    schedules = messages.MessageField(
        SingleScheduleSuspendMessage, 1, repeated=True)
