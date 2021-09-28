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

.class public Ldot/junit/opcodes/new_instance/d/T_new_instance_2;
.super Ljava/lang/Object;
.source "T_new_instance_2.java"


# instance fields
.field i:I


# direct methods
.method public constructor <init>()V
    .registers 1

    invoke-direct {p0}, Ljava/lang/Object;-><init>()V

    return-void
.end method

.method public static run()I
    .registers 5

    new-instance v0, Ldot/junit/opcodes/new_instance/d/T_new_instance_2;
#    invoke-direct {v0}, Ldot/junit/opcodes/new_instance/d/T_new_instance_2;-><init>()V

    iget v1, v0, Ldot/junit/opcodes/new_instance/d/T_new_instance_2;->i:I

    return v1
.end method
