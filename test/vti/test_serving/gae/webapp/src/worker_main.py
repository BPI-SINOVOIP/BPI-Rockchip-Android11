#!/usr/bin/env python
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

from webapp.src.tasks import indexing
from webapp.src.scheduler import schedule_worker
import webapp2


app = webapp2.WSGIApplication([
        ("/worker/schedule_handler", schedule_worker.ScheduleHandler),
        ("/worker/indexing", indexing.IndexingHandler)
    ], debug=True)
