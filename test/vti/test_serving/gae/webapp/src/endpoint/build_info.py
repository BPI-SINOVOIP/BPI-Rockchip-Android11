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
"""Build Info APIs implemented using Google Cloud Endpoints."""

import datetime
import endpoints
import logging

from webapp.src.endpoint import endpoint_base
from webapp.src.proto import model

BUILD_INFO_RESOURCE = endpoints.ResourceContainer(model.BuildInfoMessage)


@endpoints.api(name="build", version="v1")
class BuildInfoApi(endpoint_base.EndpointBase):
    """Endpoint API for build_info."""

    @endpoints.method(
        BUILD_INFO_RESOURCE,
        model.DefaultResponse,
        path="set",
        http_method="POST",
        name="set")
    def set(self, request):
        """Sets the build info based on the `request`."""
        build_query = model.BuildModel.query(
            model.BuildModel.build_id == request.build_id,
            model.BuildModel.build_target == request.build_target,
            model.BuildModel.build_type == request.build_type,
            model.BuildModel.artifact_type == request.artifact_type)
        existing_builds = build_query.fetch()

        if existing_builds and len(existing_builds) > 1:
            logging.warning(
                "Duplicated builds found for [build_id]{} "
                "[build_target]{} [build_type]{} [artifact_type]{}".format(
                    request.build_id, request.build_target, request.build_type,
                    request.artifact_type))

        if existing_builds:
            build = existing_builds[0]
            if request.signed:
                # only signed builds need to overwrite the exist entities.
                build.signed = request.signed
        else:
            build = model.BuildModel()
            common_attributes = self.GetCommonAttributes(request,
                                                         model.BuildModel)
            for attr in common_attributes:
                setattr(build, attr, getattr(request, attr))

        build.timestamp = datetime.datetime.now()
        build.put()

        return model.DefaultResponse(
            return_code=model.ReturnCodeMessage.SUCCESS)

    @endpoints.method(
        endpoint_base.GET_REQUEST_RESOURCE,
        model.BuildResponseMessage,
        path="get",
        http_method="POST",
        name="get")
    def get(self, request):
        """Gets the builds from datastore."""
        return_list, more = self.Get(request=request,
                                     metaclass=model.BuildModel,
                                     message=model.BuildInfoMessage)
        return model.BuildResponseMessage(builds=return_list, has_next=more)

    @endpoints.method(
        endpoint_base.COUNT_REQUEST_RESOURCE,
        model.CountResponseMessage,
        path="count",
        http_method="POST",
        name="count")
    def count(self, request):
        """Gets total number of BuildModel entities stored in datastore."""
        filters = self.CreateFilterList(
            filter_string=request.filter, metaclass=model.BuildModel)
        count = self.Count(metaclass=model.BuildModel, filters=filters)

        return model.CountResponseMessage(count=count)
