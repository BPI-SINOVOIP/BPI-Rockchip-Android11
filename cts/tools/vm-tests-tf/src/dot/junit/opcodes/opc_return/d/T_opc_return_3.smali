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

.class public Ldot/junit/opcodes/opc_return/d/T_opc_return_3;
.super Ljava/lang/Object;
.source "T_opc_return_3.java"


# direct methods
.method public constructor <init>()V
    .registers 1

    invoke-direct {p0}, Ljava/lang/Object;-><init>()V

    return-void
.end method

.method private declared-synchronized test()F
    .registers 4

    new-instance v2, Ljava/lang/Object;

    invoke-direct {v2}, Ljava/lang/Object;-><init>()V

    monitor-enter v2

    monitor-exit p0

    const v0, 0x3f800000    # 1.0f

    return v0
.end method


# virtual methods
.method public run()Z
    .registers 1

    invoke-direct {p0}, Ldot/junit/opcodes/opc_return/d/T_opc_return_3;->test()F

    const p0, 0x1

    return p0
.end method
