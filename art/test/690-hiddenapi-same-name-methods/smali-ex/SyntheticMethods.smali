#
# Copyright (C) 2019 The Android Open Source Project
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

.class LSyntheticMethods;
.super Ljava/lang/Object;

# Expect to choose the non-hidden, virtual method.

# Non-hidden methods
.method public synthetic foo()Ljava/lang/Number;
    .registers 1
    const/4 v0, 0x0
    return-object v0
.end method

.method private synthetic foo()Ljava/lang/Double;
    .registers 1
    const/4 v0, 0x0
    return-object v0
.end method

# Hidden methods
.method public synthetic foo()Ljava/lang/Integer;
    .registers 1
    const/4 v0, 0x0
    return-object v0
.end method

.method private synthetic foo()Ljava/lang/Boolean;
    .registers 1
    const/4 v0, 0x0
    return-object v0
.end method
