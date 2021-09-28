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

.class public Ldot/junit/opcodes/move_from16/d/T_move_from16_1;
.super Ljava/lang/Object;
.source "T_move_from16_1.java"


# direct methods
.method public constructor <init>()V
    .registers 1

    invoke-direct {p0}, Ljava/lang/Object;-><init>()V

    return-void
.end method

.method public static run()Z
    .registers 5000

    const v10, 0x162e

    move/16 v4001, v10

    move/from16 v255, v4001

    move/from16 v1, v255

    const/16 v4, 0x162e

    if-ne v1, v4, :cond_12

    const v1, 0x1

    return v1

    :cond_12
    const v1, 0x0

    return v1
.end method
