/*
 * Copyright (C) 2018 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
package com.android.tradefed.invoker.shard.token;

/**
 * Supported token with dynamic sharding. Only tokens defined here can be added to modules to be
 * considered as part of dynamic sharding.
 */
public enum TokenProperty {
    SIM_CARD,
    UICC_SIM_CARD,
    SECURE_ELEMENT_SIM_CARD,
}
