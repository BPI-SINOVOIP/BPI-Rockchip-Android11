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

.class public Ldot/junit/opcodes/check_cast/d/T_check_cast_2;
.super Ljava/lang/Object;
.source "T_check_cast_2.java"


# direct methods
.method public constructor <init>()V
    .registers 1

    invoke-direct {p0}, Ljava/lang/Object;-><init>()V

    return-void
.end method


# virtual methods
.method public run()I
    .registers 20

    const v1, 0x0

# (SubClass instanceof SuperClass)
    new-instance v10, Ldot/junit/opcodes/check_cast/d/T_check_cast_2/SubClass;

    invoke-direct {v10}, Ldot/junit/opcodes/check_cast/d/T_check_cast_2/SubClass;-><init>()V

    check-cast v10, Ldot/junit/opcodes/check_cast/d/T_check_cast_2/SuperClass;

# (SubClass[] instanceof SuperClass[])
    const v11, 0x1

    new-array v10, v11, [Ldot/junit/opcodes/check_cast/d/T_check_cast_2/SubClass;

    check-cast v10, [Ldot/junit/opcodes/check_cast/d/T_check_cast_2/SuperClass;

# (SubClass[] instanceof Object)
    new-array v10, v11, [Ldot/junit/opcodes/check_cast/d/T_check_cast_2/SubClass;

    check-cast v10, Ljava/lang/Object;

# (SubClass instanceof SuperInterface)
    new-instance v10, Ldot/junit/opcodes/check_cast/d/T_check_cast_2/SubClass;

    invoke-direct {v10}, Ldot/junit/opcodes/check_cast/d/T_check_cast_2/SubClass;-><init>()V

    check-cast v10, Ldot/junit/opcodes/check_cast/d/T_check_cast_2/SuperInterface;

# !(SuperClass instanceof SubClass)
    new-instance v10, Ldot/junit/opcodes/check_cast/d/T_check_cast_2/SuperClass;

    invoke-direct {v10}, Ldot/junit/opcodes/check_cast/d/T_check_cast_2/SuperClass;-><init>()V

    :try_start_21
    check-cast v10, Ldot/junit/opcodes/check_cast/d/T_check_cast_2/SubClass;
    :try_end_23
    .catch Ljava/lang/ClassCastException; {:try_start_21 .. :try_end_23} :catch_24

    goto :goto_27

    :catch_24
    add-int/lit16 v1, v1, 0x1

    goto :goto_27

# !(SubClass instanceof SuperInterface2)
    :goto_27
    new-instance v10, Ldot/junit/opcodes/check_cast/d/T_check_cast_2/SubClass;

    invoke-direct {v10}, Ldot/junit/opcodes/check_cast/d/T_check_cast_2/SubClass;-><init>()V

    :try_start_2c
    check-cast v10, Ldot/junit/opcodes/check_cast/d/T_check_cast_2/SuperInterface2;
    :try_end_2e
    .catch Ljava/lang/ClassCastException; {:try_start_2c .. :try_end_2e} :catch_2f

    goto :goto_32

    :catch_2f
    add-int/lit16 v1, v1, 0x1

    goto :goto_32

# !(SubClass[] instanceof SuperInterface)
    :goto_32
    new-array v10, v11, [Ldot/junit/opcodes/check_cast/d/T_check_cast_2/SubClass;

    :try_start_34
    check-cast v10, Ldot/junit/opcodes/check_cast/d/T_check_cast_2/SuperInterface;
    :try_end_36
    .catch Ljava/lang/ClassCastException; {:try_start_34 .. :try_end_36} :catch_37

    goto :goto_3a

    :catch_37
    add-int/lit16 v1, v1, 0x1

    goto :goto_3a

# !(SubClass[] instanceof SubClass)
    :goto_3a
    new-array v10, v11, [Ldot/junit/opcodes/check_cast/d/T_check_cast_2/SubClass;

    :try_start_3c
    check-cast v10, Ldot/junit/opcodes/check_cast/d/T_check_cast_2/SubClass;
    :try_end_3e
    .catch Ljava/lang/ClassCastException; {:try_start_3c .. :try_end_3e} :catch_3f

    goto :goto_42

    :catch_3f
    add-int/lit16 v1, v1, 0x1

    goto :goto_42

# !(SuperClass[] instanceof SubClass[])
    :goto_42
    new-array v10, v11, [Ldot/junit/opcodes/check_cast/d/T_check_cast_2/SuperClass;

    :try_start_44
    check-cast v10, [Ldot/junit/opcodes/check_cast/d/T_check_cast_2/SubClass;
    :try_end_46
    .catch Ljava/lang/ClassCastException; {:try_start_44 .. :try_end_46} :catch_47

    goto :goto_49

    :catch_47
    add-int/lit16 v1, v1, 0x1

    :goto_49
    return v1
.end method
