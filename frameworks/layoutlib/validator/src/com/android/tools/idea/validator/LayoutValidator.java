/*
 * Copyright (C) 2020 The Android Open Source Project
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

package com.android.tools.idea.validator;

import com.android.tools.idea.validator.ValidatorData.Level;
import com.android.tools.idea.validator.ValidatorData.Policy;
import com.android.tools.idea.validator.ValidatorData.Type;
import com.android.tools.idea.validator.accessibility.AccessibilityValidator;
import com.android.tools.layoutlib.annotations.NotNull;
import com.android.tools.layoutlib.annotations.Nullable;

import android.view.View;

import java.awt.image.BufferedImage;
import java.util.EnumSet;

/**
 * Main class for validating layout.
 */
public class LayoutValidator {

    public static final ValidatorData.Policy DEFAULT_POLICY = new Policy(
            EnumSet.of(Type.ACCESSIBILITY, Type.RENDER),
            EnumSet.of(Level.ERROR, Level.WARNING));

    private static ValidatorData.Policy sPolicy = DEFAULT_POLICY;

    /**
     * Validate the layout using the default policy.
     * Precondition: View must be attached to the window.
     *
     * @return The validation results. If no issue is found it'll return empty result.
     */
    @NotNull
    public static ValidatorResult validate(@NotNull View view, @Nullable BufferedImage image) {
        if (view.isAttachedToWindow()) {
            return AccessibilityValidator.validateAccessibility(view, image, sPolicy);
        }
        // TODO: Add non-a11y layout validation later.
        return new ValidatorResult.Builder().build();
    }

    /**
     * Update the policy with which to run the validation call.
     * @param policy new policy.
     */
    public static void updatePolicy(@NotNull ValidatorData.Policy policy) {
        sPolicy = policy;
    }
}
