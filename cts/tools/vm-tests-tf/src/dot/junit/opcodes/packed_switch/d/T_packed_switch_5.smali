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

.class public Ldot/junit/opcodes/packed_switch/d/T_packed_switch_5;
.super Ljava/lang/Object;
.source "T_packed_switch_5.java"


# direct methods
.method public constructor <init>()V
    .registers 1

    invoke-direct {p0}, Ljava/lang/Object;-><init>()V

    return-void
.end method


# virtual methods
.method public run(J)I
    .registers 5

    packed-switch p1, :pswitch_data_a

    :pswitch_3
    const/4 p0, -0x1

    return p0

    :pswitch_5
    const/4 p0, 0x2

    return p0

    :pswitch_7
    const/16 p0, 0x14

    return p0

    :pswitch_data_a
    .packed-switch -0x1
        :pswitch_5 # -1
        :pswitch_3 #  0
        :pswitch_3 #  1
        :pswitch_7 #  2
        :pswitch_7 #  3
    .end packed-switch
.end method
