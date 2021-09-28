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

.class public Ldot/junit/opcodes/fill_array_data/d/T_fill_array_data_2;
.super Ljava/lang/Object;
.source "T_fill_array_data_2.java"


# direct methods
.method public constructor <init>()V
    .registers 1

    invoke-direct {p0}, Ljava/lang/Object;-><init>()V

    return-void
.end method


# virtual methods
.method public run([D)V
    .registers 10

    fill-array-data p1, :array_4

    return-void

    :array_4
    .array-data 8
        0x3ff0000000000000L    # 1.0
        0x4000000000000000L    # 2.0
        0x4008000000000000L    # 3.0
        0x4010000000000000L    # 4.0
        0x4014000000000000L    # 5.0
    .end array-data
.end method
