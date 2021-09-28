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

.class public Ldot/junit/opcodes/iput_object/d/T_iput_object_1;
.super Ljava/lang/Object;
.source "T_iput_object_1.java"


# instance fields
.field public st_i1:Ljava/lang/Object;

.field protected st_p1:Ljava/lang/Object;

.field private st_pvt1:Ljava/lang/Object;


# direct methods
.method public constructor <init>()V
    .registers 1

    invoke-direct {p0}, Ljava/lang/Object;-><init>()V

    return-void
.end method


# virtual methods
.method public getPvtField()Ljava/lang/Object;
    .registers 2

    iget-object v0, p0, Ldot/junit/opcodes/iput_object/d/T_iput_object_1;->st_pvt1:Ljava/lang/Object;

    return-object v0
.end method

.method public run()V
    .registers 3

    iput-object p0, p0, Ldot/junit/opcodes/iput_object/d/T_iput_object_1;->st_i1:Ljava/lang/Object;

    return-void
.end method
