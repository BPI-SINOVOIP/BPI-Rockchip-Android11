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

.class public Ldot/junit/opcodes/invoke_direct_range/d/T_invoke_direct_range_24;
.super Ljava/lang/Object;
.source "T_invoke_direct_range_24.java"


# direct methods
.method public constructor <init>()V
    .registers 2

    invoke-direct {p0}, Ljava/lang/Object;-><init>()V

    return-void
.end method

.method private test(II)I
    .registers 7

    const v0, 0x3e7

    const v1, 0x378

    const v2, 0x309

    div-int p0, p1, p2

    return p0
.end method


# virtual methods
.method public run()I
    .registers 7

    const v0, 0x6f

    const v1, 0xde

    const v2, 0x14d

    const-wide v4, 0x32

    move-object v3, p0

    invoke-direct {v3, v5}, Ldot/junit/opcodes/invoke_direct_range/d/T_invoke_direct_range_24;->test(II)I

    move-result v3

    const v4, 0x2

    if-ne v3, v4, :cond_2b

    const v4, 0x6f

    if-ne v0, v4, :cond_2b

    const v4, 0xde

    if-ne v1, v4, :cond_2b

    const v4, 0x14d

    if-ne v2, v4, :cond_2b

    const v0, 0x1

    return v0

    :cond_2b
    const v0, 0x0

    return v0
.end method
