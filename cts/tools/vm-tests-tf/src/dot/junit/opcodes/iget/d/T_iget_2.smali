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

.class public Ldot/junit/opcodes/iget/d/T_iget_2;
.super Ljava/lang/Object;
.source "T_iget_2.java"


# instance fields
.field public val:F


# direct methods
.method static constructor <clinit>()V
    .registers 2

    return-void
.end method

.method public constructor <init>()V
    .registers 2

    invoke-direct {p0}, Ljava/lang/Object;-><init>()V

    const v0, 0x42f60000    # 123.0f

    iput v0, p0, Ldot/junit/opcodes/iget/d/T_iget_2;->val:F

    return-void
.end method


# virtual methods
.method public run()F
    .registers 4

    iget v1, p0, Ldot/junit/opcodes/iget/d/T_iget_2;->val:F

    return v1
.end method
