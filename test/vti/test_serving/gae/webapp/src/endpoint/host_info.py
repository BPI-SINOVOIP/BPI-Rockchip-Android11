# Copyright 2017 Google Inc. All rights reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
"""Host Info APIs implemented using Google Cloud Endpoints."""

import datetime
import endpoints
import logging

from google.appengine.api import users
from google.appengine.ext import ndb

from webapp.src import vtslab_status as Status
from webapp.src.endpoint import endpoint_base
from webapp.src.proto import model

HOST_INFO_RESOURCE = endpoints.ResourceContainer(model.HostInfoMessage)

# Product type name for null device.
_NULL_DEVICE_PRODUCT_TYPE = "null"


def AddNullDevices(hostname, null_device_count):
    """Adds null devices to DeviceModel data store.

    Args:
        hostname: string, the host name.
        null_device_count: integer, the number of null devices.
    """
    device_query = model.DeviceModel.query(
        model.DeviceModel.hostname == hostname,
        model.DeviceModel.product == _NULL_DEVICE_PRODUCT_TYPE
    )
    null_devices = device_query.fetch()
    existing_null_device_count = len(null_devices)

    if existing_null_device_count < null_device_count:
        devices_to_put = []
        for _ in range(null_device_count - existing_null_device_count):
            device = model.DeviceModel()
            device.hostname = hostname
            device.serial = "n/a"
            device.product = _NULL_DEVICE_PRODUCT_TYPE
            device.status = Status.DEVICE_STATUS_DICT["ready"]
            device.scheduling_status = Status.DEVICE_SCHEDULING_STATUS_DICT[
                "free"]
            device.timestamp = datetime.datetime.now()
            devices_to_put.append(device)
        if devices_to_put:
            ndb.put_multi(devices_to_put)


@endpoints.api(name='host', version='v1')
class HostInfoApi(endpoint_base.EndpointBase):
    """Endpoint API for host_info."""

    @endpoints.method(
        HOST_INFO_RESOURCE,
        model.DefaultResponse,
        path='set',
        http_method='POST',
        name='set')
    def set(self, request):
        """Sets the host info based on the `request`."""
        if users.get_current_user():
            username = users.get_current_user().email()
        else:
            username = "anonymous"

        devices_to_put = []
        for request_device in request.devices:
            device_query = model.DeviceModel.query(
                model.DeviceModel.serial == request_device.serial
            )
            existing_device = device_query.fetch()
            if existing_device:
                device = existing_device[0]
            else:
                device = model.DeviceModel()
                device.serial = request_device.serial
                device.scheduling_status = Status.DEVICE_SCHEDULING_STATUS_DICT[
                    "free"]
            if not device.product or request_device.product != "error":
                device.product = request_device.product

            device.username = username
            device.hostname = request.hostname
            device.status = request_device.status
            device.timestamp = datetime.datetime.now()
            devices_to_put.append(device)
        if devices_to_put:
            ndb.put_multi(devices_to_put)

        return model.DefaultResponse(
            return_code=model.ReturnCodeMessage.SUCCESS)

    @endpoints.method(
        endpoint_base.GET_REQUEST_RESOURCE,
        model.DeviceResponseMessage,
        path="get",
        http_method="POST",
        name="get")
    def get(self, request):
        """Gets the devices from datastore."""
        return_list, more = self.Get(request=request,
                                     metaclass=model.DeviceModel,
                                     message=model.DeviceInfoMessage)

        return model.DeviceResponseMessage(devices=return_list, has_next=more)

    @endpoints.method(
        endpoint_base.COUNT_REQUEST_RESOURCE,
        model.CountResponseMessage,
        path="count",
        http_method="POST",
        name="count")
    def count(self, request):
        """Gets total number of DeviceModel entities stored in datastore."""
        filters = self.CreateFilterList(
            filter_string=request.filter, metaclass=model.DeviceModel)

        count = self.Count(metaclass=model.DeviceModel, filters=filters)

        return model.CountResponseMessage(count=count)
