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

.class public Ldot/junit/opcodes/sparse_switch/d/T_sparse_switch_13;
.super Ljava/lang/Object;
.source "T_sparse_switch_13.java"


# direct methods
.method public constructor <init>()V
    .registers 1

    invoke-direct {p0}, Ljava/lang/Object;-><init>()V

    return-void
.end method


# virtual methods
.method public run(I)I
    .registers 5

    goto :goto_10

    sparse-switch p1, :sswitch_data_10

    const p1, -0x1

    return p1

    :sswitch_8
    const p1, 0x2

    return p1

    :sswitch_c
    const p1, 0x14

    return p1

    :goto_10
    :sswitch_data_10
    .sparse-switch
        -0x1 -> :sswitch_8
        0xa -> :sswitch_c
        0xf -> :sswitch_c
    .end sparse-switch
.end method
