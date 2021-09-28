# Copyright (C) 2016 The Android Open Source Project
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

instrumentation_metric_tests := \
    AutofillPerfTests \
    BlobStorePerfTests \
    crashcollector \
    CorePerfTests \
    DocumentsUIAppPerfTests \
    MtpServicePerfTests \
    RsBlasBenchmark \
    ImageProcessingJB \
    MediaProviderClientTests \
    MultiUserPerfDummyApp \
    MultiUserPerfTests \
    NeuralNetworksApiBenchmark \
    PackageManagerPerfTests \
    TextClassifierPerfTests \
    WmPerfTests \
    trace_config_detailed.textproto

    # TODO(b/72332760): Uncomment when fixed
    #DocumentsUIPerfTests

ifneq ($(strip $(BOARD_PERFSETUP_SCRIPT)),)
instrumentation_metric_tests += perf-setup.sh
endif
