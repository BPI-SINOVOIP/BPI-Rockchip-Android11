/*
 * Copyright (C) 2019 The Android Open Source Project
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

package com.android.tv.settings.users;

/**
 * Interface for communication between the primary user and the restricted profile user. The
 * restricted profile exit pin code is saved in the internal storage of the primary user. On
 * exiting from the restricted profile, the entered pin code must be verified to equal the one
 * saved in the primary user's shared preferences.
 */
interface IRestrictedProfilePinService {
  /** Check if the entered pin matches the restricted profile exit pin. */
  boolean isPinCorrect(in String pin);

  /** Set the restricted profile pin. */
  void setPin(in String pin);

  /** Delete the restricted profile pin. */
  void deletePin();

  /** Check whether the restricted profile is set. */
  boolean isPinSet();
}

