# Copyright (C) 2008 The Android Open Source Project
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

.class public Ldot/junit/opcodes/invoke_direct_range/d/T_invoke_direct_range_27;
.super Ljava/lang/Object;
.source "T_invoke_direct_range_27.java"


# direct methods
.method public constructor <init>()V
    .registers 2

    invoke-direct {p0}, Ljava/lang/Object;-><init>()V

    return-void
.end method

.method public static run()V
    .registers 3

    new-instance v0, Ldot/junit/opcodes/invoke_direct_range/d/T_invoke_direct_range_27;

    invoke-direct/range {v0 .. v0}, Ldot/junit/opcodes/invoke_direct_range/d/T_invoke_direct_range_27;->test()V

    return-void
.end method

.method private test()V
    .registers 1

    return-void
.end method
