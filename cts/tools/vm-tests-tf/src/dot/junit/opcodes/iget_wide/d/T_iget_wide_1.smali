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

.class public Ldot/junit/opcodes/iget_wide/d/T_iget_wide_1;
.super Ljava/lang/Object;
.source "T_iget_wide_1.java"


# instance fields
.field public i1:J

.field protected p1:J

.field private pvt1:J


# direct methods
.method public constructor <init>()V
    .registers 3

    invoke-direct {p0}, Ljava/lang/Object;-><init>()V

    const-wide v0, 0xb3a73dd46cbL

    iput-wide v0, p0, Ldot/junit/opcodes/iget_wide/d/T_iget_wide_1;->i1:J

    const-wide v0, 0xa

    iput-wide v0, p0, Ldot/junit/opcodes/iget_wide/d/T_iget_wide_1;->p1:J

    const-wide v0, 0x14

    iput-wide v0, p0, Ldot/junit/opcodes/iget_wide/d/T_iget_wide_1;->pvt1:J

    return-void
.end method


# virtual methods
.method public run()J
    .registers 3

    iget-wide v1, p0, Ldot/junit/opcodes/iget_wide/d/T_iget_wide_1;->i1:J

    return-wide v1
.end method
