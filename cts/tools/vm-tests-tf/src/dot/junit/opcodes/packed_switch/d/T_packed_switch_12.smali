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

.class public Ldot/junit/opcodes/packed_switch/d/T_packed_switch_12;
.super Ljava/lang/Object;
.source "T_packed_switch_12.java"


# direct methods
.method public constructor <init>()V
    .registers 1

    invoke-direct {p0}, Ljava/lang/Object;-><init>()V

    return-void
.end method


# virtual methods
.method public run(I)I
    .registers 5

    goto :goto_b

    packed-switch p1, :pswitch_data_c

    :pswitch_4
    const/4 v2, -0x1

    return v2

    :pswitch_6
    const/4 v2, 0x2

    return v2

    :pswitch_8
    const/16 v2, 0x14

    return v2

    :goto_b
    nop

    :pswitch_data_c
    .packed-switch -0x1
        :pswitch_6 # -1
        :pswitch_4 #  0
        :pswitch_4 #  1
        :pswitch_8 #  2
        :pswitch_8 #  3
    .end packed-switch
.end method
