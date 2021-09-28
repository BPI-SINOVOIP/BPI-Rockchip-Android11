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

.class public Ldot/junit/opcodes/move_result/d/T_move_result_1;
.super Ljava/lang/Object;
.source "T_move_result_1.java"


# direct methods
.method public constructor <init>()V
    .registers 1

    invoke-direct {p0}, Ljava/lang/Object;-><init>()V

    return-void
.end method

.method private static foo()I
    .registers 1

    const v0, 0x3039

    return v0
.end method

.method public static run()Z
    .registers 16

    const v0, 0x0

    invoke-static {}, Ldot/junit/opcodes/move_result/d/T_move_result_1;->foo()I

    move-result v0

    const v1, 0x3039

    if-eq v0, v1, :cond_10

    const v0, 0x0

    return v0

    :cond_10
    const v0, 0x1

    return v0
.end method
