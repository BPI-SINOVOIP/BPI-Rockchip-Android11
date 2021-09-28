#!/bin/bash

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

source $(dirname $BASH_SOURCE)/common.sh

SRCGEN_TOOL_BINARY=${ANDROID_HOST_OUT}/bin/android_icu4j_srcgen_binary
WHITELIST_API_FILE=${ICU_SRCGEN_DIR}/whitelisted-public-api.txt

${SRCGEN_TOOL_BINARY} GeneratePublicApiReport ${ANDROID_ICU4J_DIR}/src/main/java ${WHITELIST_API_FILE}

TEMP_CONTENT=$(cat ${WHITELIST_API_FILE})

# Prepend the license and README in the header
cat > ${WHITELIST_API_FILE} <<'_EOF'
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

_EOF

echo "$TEMP_CONTENT" >> ${WHITELIST_API_FILE}
