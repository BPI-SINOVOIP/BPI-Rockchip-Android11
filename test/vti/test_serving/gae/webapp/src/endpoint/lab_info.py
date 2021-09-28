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
"""Lab Info APIs implemented using Google Cloud Endpoints."""

import datetime
import endpoints
import logging

from google.appengine.ext import ndb

from webapp.src import vtslab_status as Status
from webapp.src.endpoint import endpoint_base
from webapp.src.endpoint import host_info
from webapp.src.proto import model

LAB_INFO_RESOURCE = endpoints.ResourceContainer(model.LabInfoMessage)
LAB_HOST_INFO_RESOURCE = endpoints.ResourceContainer(model.LabHostInfoMessage)


@endpoints.api(name='lab', version='v1')
class LabInfoApi(endpoint_base.EndpointBase):
    """Endpoint API for lab_info."""

    @endpoints.method(
        LAB_INFO_RESOURCE,
        model.DefaultResponse,
        path="clear",
        http_method="POST",
        name="clear")
    def clear(self, request):
        """Clears lab info in DB."""
        lab_query = model.LabModel.query()
        existing_labs = lab_query.fetch(keys_only=True)
        if existing_labs and len(existing_labs) > 0:
            ndb.delete_multi(existing_labs)
        return model.DefaultResponse(
            return_code=model.ReturnCodeMessage.SUCCESS)

    @endpoints.method(
        LAB_INFO_RESOURCE,
        model.DefaultResponse,
        path="set",
        http_method="POST",
        name="set")
    def set(self, request):
        """Sets the lab info based on `request`."""
        if "host" in [x.name for x in request.all_fields()]:
            labs_to_put = []
            for host in request.host:
                duplicate_query = model.LabModel.query(
                    model.LabModel.name == request.name,
                    model.LabModel.owner == request.owner,
                    model.LabModel.hostname == host.hostname)
                duplicates = duplicate_query.fetch()
                if duplicates:
                    lab = duplicates[0]
                else:
                    lab = model.LabModel()
                lab.name = request.name
                lab.owner = request.owner
                lab.admin = request.admin
                lab.hostname = host.hostname
                lab.ip = host.ip
                lab.script = host.script

                null_device_count = 0
                devices_to_put = []
                for config_device in host.device:
                    if config_device.product == "null":
                        null_device_count += 1
                        continue
                    if config_device.serial and config_device.product:
                        device_query = model.DeviceModel.query(
                            model.DeviceModel.serial == config_device.serial)
                        devices = device_query.fetch()
                        if devices:
                            device = devices[0]
                            if (device.hostname != host.hostname) and (
                                    device.status !=
                                    Status.DEVICE_STATUS_DICT["no-response"]):
                                logging.error(
                                    "{} is alive in another host.".format(
                                        config_device.serial))
                                # TODO: send an alert to lab.admin
                                continue
                            if device.hostname == host.hostname and set(
                                    device.device_equipment) == set(
                                        config_device.device_equipment):
                                # no need to update.
                                continue
                        else:
                            device = model.DeviceModel()
                            device.status = Status.DEVICE_STATUS_DICT[
                                "no-response"]
                            device.product = config_device.product
                            device.serial = config_device.serial
                            device.hostname = host.hostname
                            device.scheduling_status = (
                                Status.DEVICE_SCHEDULING_STATUS_DICT["free"])
                            device.timestamp = datetime.datetime.now()
                        device.device_equipment = config_device.device_equipment
                        devices_to_put.append(device)
                    else:
                        logging.error("Lab config does not have device "
                                      "information correctly; it should "
                                      "specify device product and serial.")
                if devices_to_put:
                    ndb.put_multi(devices_to_put)

                lab.timestamp = datetime.datetime.now()
                labs_to_put.append(lab)

                if null_device_count > 0:
                    host_info.AddNullDevices(host.hostname, null_device_count)

            if labs_to_put:
                ndb.put_multi(labs_to_put)

        return model.DefaultResponse(
            return_code=model.ReturnCodeMessage.SUCCESS)

    @endpoints.method(
        LAB_HOST_INFO_RESOURCE,
        model.DefaultResponse,
        path="set_version",
        http_method="POST",
        name="set_version")
    def set_version(self, request):
        """Sets vtslab version of the host <hostname>"""
        lab_query = model.LabModel.query(
            model.LabModel.hostname == request.hostname)
        labs = lab_query.fetch()

        labs_to_put = []
        for lab in labs:
            lab.vtslab_version = request.vtslab_version.split(":")[0]
            labs_to_put.append(lab)
        if labs_to_put:
            ndb.put_multi(labs_to_put)

        return model.DefaultResponse(
            return_code=model.ReturnCodeMessage.SUCCESS)

    @endpoints.method(
        endpoint_base.GET_REQUEST_RESOURCE,
        model.LabResponseMessage,
        path="get",
        http_method="POST",
        name="get")
    def get(self, request):
        """Gets the labs from datastore."""
        return_list, more = self.Get(request=request,
                                     metaclass=model.LabModel,
                                     message=model.LabMessage)

        return model.LabResponseMessage(labs=return_list, has_next=more)

    @endpoints.method(
        endpoint_base.COUNT_REQUEST_RESOURCE,
        model.CountResponseMessage,
        path="count",
        http_method="POST",
        name="count")
    def count(self, request):
        """Gets total number of BuildModel entities stored in datastore."""
        filters = self.CreateFilterList(
            filter_string=request.filter, metaclass=model.LabModel)

        count = self.Count(metaclass=model.LabModel, filters=filters)

        return model.CountResponseMessage(count=count)
