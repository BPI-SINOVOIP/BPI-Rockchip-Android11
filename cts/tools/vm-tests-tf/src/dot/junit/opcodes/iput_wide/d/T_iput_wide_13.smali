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

.class public Ldot/junit/opcodes/iput_wide/d/T_iput_wide_13;
.super Ljava/lang/Object;
.source "T_iput_wide_13.java"


# instance fields
.field public st_i1:J


# direct methods
.method public constructor <init>()V
    .registers 1

    invoke-direct {p0}, Ljava/lang/Object;-><init>()V

    return-void
.end method


# virtual methods
.method public run()V
    .registers 3

    const p0, 0x0

    const-wide v0, 0xb55a014129L

    iput-wide v0, p0, Ldot/junit/opcodes/iput_wide/d/T_iput_wide_13;->st_i1:J

    return-void
.end method
