#!/bin/bash
#
# Copyright 2018 The Android Open Source Project
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

if [ "$#" -ne 1 ]; then
  echo "usage: deploy-endpoint.sh prod|test|public"
  exit 1
fi

if [ $1 = "public" ]; then
  SERVICE="vtslab-schedule"
else
  SERVICE="vtslab-schedule-$1"
fi

echo "Creating OpenAPI spec files for $SERVICE.appspot.com ..."
python lib/endpoints/endpointscfg.py get_openapi_spec webapp.src.endpoint.build_info.BuildInfoApi webapp.src.endpoint.host_info.HostInfoApi webapp.src.endpoint.lab_info.LabInfoApi webapp.src.endpoint.schedule_info.ScheduleInfoApi webapp.src.endpoint.job_queue.JobQueueApi --hostname $SERVICE.appspot.com --x-google-api-name

echo "Depolying the endpoint API implementation to $SERVICE ..."

gcloud endpoints services deploy buildv1openapi.json hostv1openapi.json labv1openapi.json schedulev1openapi.json jobv1openapi.json --project=$SERVICE
gcloud endpoints configs list --service=$SERVICE.appspot.com

echo "Deployment done!"
