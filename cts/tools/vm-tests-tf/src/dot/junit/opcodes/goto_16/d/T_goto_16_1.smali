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

.class public Ldot/junit/opcodes/goto_16/d/T_goto_16_1;
.super Ljava/lang/Object;
.source "T_goto_16_1.java"


# direct methods
.method public constructor <init>()V
    .registers 1

    invoke-direct {p0}, Ljava/lang/Object;-><init>()V

    return-void
.end method


# virtual methods
.method public run(I)I
    .registers 5

# test positive offset
    goto/16 :goto_3

    :goto_2
    return p1

    :goto_3
    add-int/lit8 p1, p1, -0x1

    if-lez p1, :cond_9

# test negative offset
    goto/16 :goto_3

    :cond_9
    goto :goto_2
.end method
