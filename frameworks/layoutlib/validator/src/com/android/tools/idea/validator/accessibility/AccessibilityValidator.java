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

package com.android.tools.idea.validator.accessibility;

import com.android.tools.idea.validator.ValidatorData;
import com.android.tools.idea.validator.ValidatorData.Fix;
import com.android.tools.idea.validator.ValidatorData.Issue.IssueBuilder;
import com.android.tools.idea.validator.ValidatorData.Level;
import com.android.tools.idea.validator.ValidatorData.Type;
import com.android.tools.idea.validator.ValidatorResult;
import com.android.tools.idea.validator.ValidatorResult.Metric;
import com.android.tools.layoutlib.annotations.NotNull;
import com.android.tools.layoutlib.annotations.Nullable;

import android.view.View;

import java.awt.image.BufferedImage;
import java.util.ArrayList;
import java.util.EnumSet;
import java.util.HashSet;
import java.util.List;
import java.util.Locale;
import java.util.ResourceBundle;
import java.util.Set;

import com.google.android.apps.common.testing.accessibility.framework.AccessibilityCheckPreset;
import com.google.android.apps.common.testing.accessibility.framework.AccessibilityCheckResult.AccessibilityCheckResultType;
import com.google.android.apps.common.testing.accessibility.framework.AccessibilityHierarchyCheck;
import com.google.android.apps.common.testing.accessibility.framework.AccessibilityHierarchyCheckResult;
import com.google.android.apps.common.testing.accessibility.framework.Parameters;
import com.google.android.apps.common.testing.accessibility.framework.strings.StringManager;
import com.google.android.apps.common.testing.accessibility.framework.uielement.AccessibilityHierarchyAndroid;
import com.google.common.collect.BiMap;

/**
 * Validator specific for running Accessibility specific issues.
 */
public class AccessibilityValidator {

    static {
        /**
         * Overriding default ResourceBundle ATF uses. ATF would use generic Java resources
         * instead of Android's .xml.
         *
         * By default ATF generates ResourceBundle to support Android specific env/ classloader,
         * which is quite different from Layoutlib, which supports multiple classloader depending
         * on env (testing vs in studio).
         *
         * To support ATF in Layoutlib, easiest way is to convert resources from Android xml to
         * generic Java resources (strings.properties), and have the default ResourceBundle ATF
         * uses be redirected.
         */
        StringManager.setResourceBundleProvider(locale -> ResourceBundle.getBundle("strings"));
    }

    /**
     * Run Accessibility specific validation test and receive results.
     * @param view the root view
     * @param image the output image of the view. Null if not available.
     * @param policy e.g: list of levels to allow
     * @return results with all the accessibility issues and warnings.
     */
    @NotNull
    public static ValidatorResult validateAccessibility(
            @NotNull View view,
            @Nullable BufferedImage image,
            @NotNull ValidatorData.Policy policy) {

        EnumSet<Level> filter = policy.mLevels;
        ValidatorResult.Builder builder = new ValidatorResult.Builder();
        builder.mMetric.startTimer();
        if (!policy.mTypes.contains(Type.ACCESSIBILITY)) {
            return builder.build();
        }

        List<AccessibilityHierarchyCheckResult> results = getHierarchyCheckResults(
                builder.mMetric,
                view,
                builder.mSrcMap,
                image,
                policy.mChecks);

        for (AccessibilityHierarchyCheckResult result : results) {
            ValidatorData.Level level = convertLevel(result.getType());
            if (!filter.contains(level)) {
                continue;
            }

            try {
                IssueBuilder issueBuilder = new IssueBuilder()
                        .setMsg(result.getMessage(Locale.ENGLISH).toString())
                        .setLevel(level)
                        .setFix(generateFix(result))
                        .setSourceClass(result.getSourceCheckClass().getSimpleName());
                if (result.getElement() != null) {
                    issueBuilder.setSrcId(result.getElement().getCondensedUniqueId());
                }
                AccessibilityHierarchyCheck subclass = AccessibilityCheckPreset
                        .getHierarchyCheckForClass(result
                                .getSourceCheckClass()
                                .asSubclass(AccessibilityHierarchyCheck.class));
                if (subclass != null) {
                    issueBuilder.setHelpfulUrl(subclass.getHelpUrl());
                }
                builder.mIssues.add(issueBuilder.build());
            } catch (Exception e) {
                builder.mIssues.add(new IssueBuilder()
                        .setType(Type.INTERNAL_ERROR)
                        .setMsg(e.getMessage())
                        .setLevel(Level.ERROR)
                        .setSourceClass("AccessibilityValidator").build());
            }
        }
        builder.mMetric.endTimer();
        return builder.build();
    }

    @NotNull
    private static ValidatorData.Level convertLevel(@NotNull AccessibilityCheckResultType type) {
        switch (type) {
            case ERROR:
                return Level.ERROR;
            case WARNING:
                return Level.WARNING;
            case INFO:
                return Level.INFO;
            // TODO: Maybe useful later?
            case SUPPRESSED:
            case NOT_RUN:
            default:
                return Level.VERBOSE;
        }
    }

    @Nullable
    private static ValidatorData.Fix generateFix(@NotNull AccessibilityHierarchyCheckResult result) {
        // TODO: Once ATF is ready to return us with appropriate fix, build proper fix here.
        return new Fix("");
    }

    @NotNull
    private static List<AccessibilityHierarchyCheckResult> getHierarchyCheckResults(
            @NotNull Metric metric,
            @NotNull View view,
            @NotNull BiMap<Long, View> originMap,
            @Nullable BufferedImage image,
            HashSet<AccessibilityHierarchyCheck> policyChecks) {

        @NotNull Set<AccessibilityHierarchyCheck> checks = policyChecks.isEmpty()
                ? AccessibilityCheckPreset
                        .getAccessibilityHierarchyChecksForPreset(AccessibilityCheckPreset.LATEST)
                : policyChecks;

        @NotNull AccessibilityHierarchyAndroid hierarchy = AccessibilityHierarchyAndroid
                .newBuilder(view)
                .setViewOriginMap(originMap)
                .build();
        ArrayList<AccessibilityHierarchyCheckResult> a11yResults = new ArrayList();

        Parameters parameters = null;
        if (image != null) {
            parameters = new Parameters();
            parameters.putScreenCapture(new AtfBufferedImage(image, metric));
        }

        for (AccessibilityHierarchyCheck check : checks) {
            a11yResults.addAll(check.runCheckOnHierarchy(hierarchy, null, parameters));
        }

        return a11yResults;
    }
}
