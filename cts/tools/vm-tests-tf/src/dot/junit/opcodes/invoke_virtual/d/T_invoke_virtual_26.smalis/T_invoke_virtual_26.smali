# Copyright (C) 2016 The Android Open Source Project
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

.class public Ldot/junit/opcodes/invoke_virtual/d/T_invoke_virtual_26;
.super Ljava/lang/Object;
.source "T_invoke_virtual_26.java"

# interfaces
.implements Ldot/junit/opcodes/invoke_virtual/d/ITestDefault;
.implements Ldot/junit/opcodes/invoke_virtual/d/ITestDefault2;


# direct methods
.method public constructor <init>()V
    .registers 2

    invoke-direct {p0}, Ljava/lang/Object;-><init>()V

    return-void
.end method


# virtual methods
.method public run()V
    .registers 1

    invoke-virtual {p0}, Ldot/junit/opcodes/invoke_virtual/d/T_invoke_virtual_26;->testDefault()V

    return-void
.end method
