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

.class public Ldot/junit/opcodes/sget_wide/d/T_sget_wide_1;
.super Ljava/lang/Object;
.source "T_sget_wide_1.java"


# static fields
.field public static i1:J

.field protected static p1:J

.field private static pvt1:J


# direct methods
.method static constructor <clinit>()V
    .registers 2

    const-wide v0, 0xb3a73dd46cbL

    sput-wide v0, Ldot/junit/opcodes/sget_wide/d/T_sget_wide_1;->i1:J

    const-wide v0, 0xa

    sput-wide v0, Ldot/junit/opcodes/sget_wide/d/T_sget_wide_1;->p1:J

    const-wide v0, 0x14

    sput-wide v0, Ldot/junit/opcodes/sget_wide/d/T_sget_wide_1;->pvt1:J

    return-void
.end method

.method public constructor <init>()V
    .registers 1

    invoke-direct {p0}, Ljava/lang/Object;-><init>()V

    return-void
.end method


# virtual methods
.method public run()J
    .registers 3

    sget-wide v1, Ldot/junit/opcodes/sget_wide/d/T_sget_wide_1;->i1:J

    return-wide v1
.end method
