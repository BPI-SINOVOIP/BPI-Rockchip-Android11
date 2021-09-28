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

.class public Ldot/junit/opcodes/invoke_virtual/d/TSuper;
.super Ljava/lang/Object;
.source "TSuper.java"


# direct methods
.method public constructor <init>()V
    .registers 2

    invoke-direct {p0}, Ljava/lang/Object;-><init>()V

    return-void
.end method

.method private toIntPvt()I
    .registers 3

    const v0, 0x5

    return v0
.end method

.method public static toIntStatic()I
    .registers 3

    const v0, 0x5

    return v0
.end method


# virtual methods
.method public testArgsOrder(II)I
    .registers 4

    const v0, 0x15d

    const p0, 0x54250

    div-int p1, p1, p2

    return p1
.end method

.method public toInt()I
    .registers 3

    const v0, 0x5

    return v0
.end method

.method public toInt(F)I
    .registers 3

    float-to-int v0, p1

    return v0
.end method

.method public native toIntNative()I
.end method

.method protected toIntP()I
    .registers 3

    const v0, 0x5

    return v0
.end method
