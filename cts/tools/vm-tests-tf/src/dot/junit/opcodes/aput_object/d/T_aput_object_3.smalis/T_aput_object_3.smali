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

.class public Ldot/junit/opcodes/aput_object/d/T_aput_object_3;
.super Ljava/lang/Object;
.source "T_aput_object_3.java"


# direct methods
.method public constructor <init>()V
    .registers 1

    invoke-direct {p0}, Ljava/lang/Object;-><init>()V

    return-void
.end method


# virtual methods
.method public run()I
    .registers 32

    const v1, 0x0

    const v0, 0x1

# v2 = SubClass[]
    new-array v2, v0, [Ldot/junit/opcodes/aput_object/d/SubClass;

# v3 =  SuperClass[]
    new-array v3, v0, [Ldot/junit/opcodes/aput_object/d/SuperClass;

# v4 =     SubClass
    new-instance v4, Ldot/junit/opcodes/aput_object/d/SubClass;

    invoke-direct {v4}, Ldot/junit/opcodes/aput_object/d/SubClass;-><init>()V

# v5 =     SuperClass
    new-instance v5, Ldot/junit/opcodes/aput_object/d/SuperClass;

    invoke-direct {v5}, Ldot/junit/opcodes/aput_object/d/SuperClass;-><init>()V

# v6 = SuperInterface[]
    new-array v6, v0, [Ldot/junit/opcodes/aput_object/d/SuperInterface;

# v7 =     Object[]
    new-array v7, v0, [Ljava/lang/Object;

# v8 = SuperInterface2[]
    new-array v8, v0, [Ldot/junit/opcodes/aput_object/d/SuperInterface2;

    const/4 v0, 0x0

# (SubClass -> SuperClass[])
    aput-object v4, v3, v0

# (SubClass -> SuperInterface[])
    aput-object v4, v6, v0

# (SubClass -> Object[])
    aput-object v4, v7, v0

    :try_start_21
# !(SuperClass -> SubClass[])
    aput-object v5, v2, v0
    :try_end_23
    .catch Ljava/lang/ArrayStoreException; {:try_start_21 .. :try_end_23} :catch_24

    goto :goto_27

    :catch_24
    add-int/lit8 v1, v1, 0x1

    goto :goto_27

    :goto_27
    :try_start_27
# !(SuperClass -> SuperInterface2[])
    aput-object v5, v8, v0
    :try_end_29
    .catch Ljava/lang/ArrayStoreException; {:try_start_27 .. :try_end_29} :catch_2a

    goto :goto_2d

    :catch_2a
    add-int/lit8 v1, v1, 0x1

    goto :goto_2d

    :goto_2d
    :try_start_2d
# !(SubClass[] -> SuperInterface[])
    aput-object v2, v6, v0
    :try_end_2f
    .catch Ljava/lang/ArrayStoreException; {:try_start_2d .. :try_end_2f} :catch_30

    goto :goto_33

    :catch_30
    add-int/lit8 v1, v1, 0x1

    goto :goto_33

    :goto_33
    return v1
.end method
