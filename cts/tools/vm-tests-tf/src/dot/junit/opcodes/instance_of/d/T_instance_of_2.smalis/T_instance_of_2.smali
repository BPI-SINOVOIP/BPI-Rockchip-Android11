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

.class public Ldot/junit/opcodes/instance_of/d/T_instance_of_2;
.super Ljava/lang/Object;
.source "T_instance_of_2.java"


# direct methods
.method public constructor <init>()V
    .registers 1

    invoke-direct {p0}, Ljava/lang/Object;-><init>()V

    return-void
.end method


# virtual methods
.method public run()Z
    .registers 20

    const v0, 0x0

# (SubClass instanceof SuperClass)
    new-instance v10, Ldot/junit/opcodes/instance_of/d/T_instance_of_2/SubClass;

    invoke-direct {v10}, Ldot/junit/opcodes/instance_of/d/T_instance_of_2/SubClass;-><init>()V

    instance-of v15, v10, Ldot/junit/opcodes/instance_of/d/T_instance_of_2/SuperClass;

    if-eqz v15, :cond_4b

# (SubClass[] instanceof SuperClass[])
    const v11, 0x1

    new-array v10, v11, [Ldot/junit/opcodes/instance_of/d/T_instance_of_2/SubClass;

    instance-of v15, v10, [Ldot/junit/opcodes/instance_of/d/T_instance_of_2/SuperClass;

    if-eqz v15, :cond_4b

# (SubClass[] instanceof Object)
    new-array v10, v11, [Ldot/junit/opcodes/instance_of/d/T_instance_of_2/SubClass;

    instance-of v15, v10, Ljava/lang/Object;

    if-eqz v15, :cond_4b

# (SubClass instanceof SuperInterface)
    new-instance v10, Ldot/junit/opcodes/instance_of/d/T_instance_of_2/SubClass;

    invoke-direct {v10}, Ldot/junit/opcodes/instance_of/d/T_instance_of_2/SubClass;-><init>()V

    instance-of v15, v10, Ldot/junit/opcodes/instance_of/d/T_instance_of_2/SuperInterface;

    if-eqz v15, :cond_4b

# !(SuperClass instanceof SubClass)
    new-instance v10, Ldot/junit/opcodes/instance_of/d/T_instance_of_2/SuperClass;

    invoke-direct {v10}, Ldot/junit/opcodes/instance_of/d/T_instance_of_2/SuperClass;-><init>()V

    instance-of v15, v10, Ldot/junit/opcodes/instance_of/d/T_instance_of_2/SubClass;

    if-nez v15, :cond_4b

# !(SubClass instanceof SuperInterface2)
    new-instance v10, Ldot/junit/opcodes/instance_of/d/T_instance_of_2/SubClass;

    invoke-direct {v10}, Ldot/junit/opcodes/instance_of/d/T_instance_of_2/SubClass;-><init>()V

    instance-of v15, v10, Ldot/junit/opcodes/instance_of/d/T_instance_of_2/SuperInterface2;

    if-nez v15, :cond_4b

# !(SubClass[] instanceof SuperInterface)
    new-array v10, v11, [Ldot/junit/opcodes/instance_of/d/T_instance_of_2/SubClass;

    instance-of v15, v10, Ldot/junit/opcodes/instance_of/d/T_instance_of_2/SuperInterface;

    if-nez v15, :cond_4b

# !(SubClass[] instanceof SubClass)
    new-array v10, v11, [Ldot/junit/opcodes/instance_of/d/T_instance_of_2/SubClass;

    instance-of v15, v10, Ldot/junit/opcodes/instance_of/d/T_instance_of_2/SubClass;

    if-nez v15, :cond_4b

# !(SuperClass[] instanceof SubClass[])
    new-array v10, v11, [Ldot/junit/opcodes/instance_of/d/T_instance_of_2/SuperClass;

    instance-of v15, v10, [Ldot/junit/opcodes/instance_of/d/T_instance_of_2/SubClass;

    if-nez v15, :cond_4b

    const v0, 0x1

    :cond_4b
    return v0
.end method
