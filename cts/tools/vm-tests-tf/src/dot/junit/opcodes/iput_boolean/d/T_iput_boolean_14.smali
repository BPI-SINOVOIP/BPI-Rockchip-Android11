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

.source "T_iput_boolean_14.java"
.class  public Ldot/junit/opcodes/iput_boolean/d/T_iput_boolean_14;
.super  Ldot/junit/opcodes/iput_boolean/d/T_iput_boolean_1;


.method public constructor <init>()V
.registers 1

       invoke-direct {v0}, Ldot/junit/opcodes/iput_boolean/d/T_iput_boolean_1;-><init>()V
       return-void
.end method

.method public  getProtectedField()Z
.registers 2

       iget-boolean v0, v1, Ldot/junit/opcodes/iput_boolean/d/T_iput_boolean_1;->st_p1:Z
       return v0
.end method

.method public run()V
.registers 3

       const v1, 1
       iput-boolean v1, v2, Ldot/junit/opcodes/iput_boolean/d/T_iput_boolean_1;->st_p1:Z
       return-void
.end method


