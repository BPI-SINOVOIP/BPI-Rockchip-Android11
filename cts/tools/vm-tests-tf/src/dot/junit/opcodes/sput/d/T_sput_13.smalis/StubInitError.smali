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

.class public Ldot/junit/opcodes/sput/d/StubInitError;
.super Ljava/lang/Object;
.source "StubInitError.java"


# static fields
.field public static value:I


# direct methods
.method static constructor <clinit>()V
    .registers 2

    const/4 v0, 0x0

    const/4 v1, 0x5

    div-int/2addr v1, v0

    sput v1, Ldot/junit/opcodes/sput/d/StubInitError;->value:I

    return-void
.end method
