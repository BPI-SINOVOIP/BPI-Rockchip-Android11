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

import com.android.ide.common.rendering.api.RenderSession;
import com.android.ide.common.rendering.api.SessionParams;
import com.android.layoutlib.bridge.intensive.RenderTestBase;
import com.android.layoutlib.bridge.intensive.setup.ConfigGenerator;
import com.android.layoutlib.bridge.intensive.setup.LayoutLibTestCallback;
import com.android.layoutlib.bridge.intensive.setup.LayoutPullParser;
import com.android.layoutlib.bridge.intensive.util.SessionParamsBuilder;
import com.android.tools.idea.validator.LayoutValidator;
import com.android.tools.idea.validator.ValidatorData;
import com.android.tools.idea.validator.ValidatorData.Issue;
import com.android.tools.idea.validator.ValidatorData.Level;
import com.android.tools.idea.validator.ValidatorData.Policy;
import com.android.tools.idea.validator.ValidatorData.Type;
import com.android.tools.idea.validator.ValidatorResult;

import org.junit.Test;

import java.util.EnumSet;
import java.util.List;
import java.util.stream.Collectors;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;

/**
 * Sanity check for a11y checks. For now it lacks checking the following:
 * - ClassNameCheck
 * - ClickableSpanCheck
 * - EditableContentDescCheck
 * - LinkPurposeUnclearCheck
 * As these require more complex UI for testing.
 *
 * It's also missing:
 * - TraversalOrderCheck
 * Because in Layoutlib test env, traversalBefore/after attributes seems to be lost. Tested on
 * studio and it seems to work ok.
 */
public class AccessibilityValidatorTests extends RenderTestBase {

    @Test
    public void testDuplicateClickableBoundsCheck() throws Exception {
        render("a11y_test_dup_clickable_bounds.xml", session -> {
            ValidatorResult result = getRenderResult(session);
            List<Issue> dupBounds = filter(result.getIssues(), "DuplicateClickableBoundsCheck");

            ExpectedLevels expectedLevels = new ExpectedLevels();
            expectedLevels.expectedErrors = 1;
            expectedLevels.check(dupBounds);
        });
    }

    @Test
    public void testDuplicateSpeakableTextsCheck() throws Exception {
        render("a11y_test_duplicate_speakable.xml", session -> {
            ValidatorResult result = getRenderResult(session);
            List<Issue> duplicateSpeakableTexts = filter(result.getIssues(),
                    "DuplicateSpeakableTextCheck");

            ExpectedLevels expectedLevels = new ExpectedLevels();
            expectedLevels.expectedInfos = 1;
            expectedLevels.expectedWarnings = 1;
            expectedLevels.check(duplicateSpeakableTexts);
        });
    }

    @Test
    public void testRedundantDescriptionCheck() throws Exception {
        render("a11y_test_redundant_desc.xml", session -> {
            ValidatorResult result = getRenderResult(session);
            List<Issue> redundant = filter(result.getIssues(), "RedundantDescriptionCheck");

            ExpectedLevels expectedLevels = new ExpectedLevels();
            expectedLevels.expectedVerboses = 3;
            expectedLevels.expectedWarnings = 1;
            expectedLevels.check(redundant);
        });
    }

    @Test
    public void testLabelFor() throws Exception {
        render("a11y_test_speakable_text_present.xml", session -> {
            ValidatorResult result = getRenderResult(session);
            List<Issue> speakableCheck = filter(result.getIssues(), "SpeakableTextPresentCheck");

            // Post-JB MR2 support labelFor, so SpeakableTextPresentCheck does not need to find any
            // speakable text. Expected 1 verbose result saying something along the line of
            // didn't run or not important for a11y.
            ExpectedLevels expectedLevels = new ExpectedLevels();
            expectedLevels.expectedVerboses = 1;
            expectedLevels.check(speakableCheck);
        });
    }

    @Test
    public void testImportantForAccessibility() throws Exception {
        render("a11y_test_speakable_text_present2.xml", session -> {
            ValidatorResult result = getRenderResult(session);
            List<Issue> speakableCheck = filter(result.getIssues(), "SpeakableTextPresentCheck");

            // Post-JB MR2 support importantForAccessibility, so SpeakableTextPresentCheck
            // does not need to find any speakable text. Expected 2 verbose results.
            ExpectedLevels expectedLevels = new ExpectedLevels();
            expectedLevels.expectedVerboses = 2;
            expectedLevels.check(speakableCheck);
        });
    }

    @Test
    public void testSpeakableTextPresentCheck() throws Exception {
        render("a11y_test_speakable_text_present3.xml", session -> {
            ValidatorResult result = getRenderResult(session);
            List<Issue> speakableCheck = filter(result.getIssues(), "SpeakableTextPresentCheck");

            ExpectedLevels expectedLevels = new ExpectedLevels();
            expectedLevels.expectedVerboses = 1;
            expectedLevels.expectedErrors = 1;
            expectedLevels.check(speakableCheck);

            // Make sure no other errors in the system.
            speakableCheck = filter(speakableCheck, EnumSet.of(Level.ERROR));
            assertEquals(1, speakableCheck.size());
            List<Issue> allErrors = filter(
                    result.getIssues(), EnumSet.of(Level.ERROR, Level.WARNING, Level.INFO));
            checkEquals(speakableCheck, allErrors);
        });
    }

    @Test
    public void testTextContrastCheck() throws Exception {
        render("a11y_test_text_contrast.xml", session -> {
            ValidatorResult result = getRenderResult(session);
            List<Issue> textContrast = filter(result.getIssues(), "TextContrastCheck");

            // ATF doesn't count alpha values unless image is passed.
            ExpectedLevels expectedLevels = new ExpectedLevels();
            expectedLevels.expectedErrors = 3;
            expectedLevels.expectedWarnings = 1; // This is true only if image is passed.
            expectedLevels.expectedVerboses = 2;
            expectedLevels.check(textContrast);

            // Make sure no other errors in the system.
            textContrast = filter(textContrast, EnumSet.of(Level.ERROR));
            List<Issue> filtered = filter(result.getIssues(), EnumSet.of(Level.ERROR));
            checkEquals(filtered, textContrast);
        });
    }

    @Test
    public void testTextContrastCheckNoImage() throws Exception {
        render("a11y_test_text_contrast.xml", session -> {
            ValidatorResult result = getRenderResult(session);
            List<Issue> textContrast = filter(result.getIssues(), "TextContrastCheck");

            // ATF doesn't count alpha values unless image is passed.
            ExpectedLevels expectedLevels = new ExpectedLevels();
            expectedLevels.expectedErrors = 3;
            expectedLevels.expectedVerboses = 3;
            expectedLevels.check(textContrast);

            // Make sure no other errors in the system.
            textContrast = filter(textContrast, EnumSet.of(Level.ERROR));
            List<Issue> filtered = filter(result.getIssues(), EnumSet.of(Level.ERROR));
            checkEquals(filtered, textContrast);
        }, false);
    }

    @Test
    public void testImageContrastCheck() throws Exception {
        render("a11y_test_image_contrast.xml", session -> {
            ValidatorResult result = getRenderResult(session);
            List<Issue> imageContrast = filter(result.getIssues(), "ImageContrastCheck");

            ExpectedLevels expectedLevels = new ExpectedLevels();
            expectedLevels.expectedWarnings = 1;
            expectedLevels.expectedVerboses = 1;
            expectedLevels.check(imageContrast);

            // Make sure no other errors in the system.
            imageContrast = filter(imageContrast, EnumSet.of(Level.ERROR, Level.WARNING));
            List<Issue> filtered = filter(result.getIssues(), EnumSet.of(Level.ERROR, Level.WARNING));
            checkEquals(filtered, imageContrast);
        });
    }

    @Test
    public void testImageContrastCheckNoImage() throws Exception {
        render("a11y_test_image_contrast.xml", session -> {
            ValidatorResult result = getRenderResult(session);
            List<Issue> imageContrast = filter(result.getIssues(), "ImageContrastCheck");

            ExpectedLevels expectedLevels = new ExpectedLevels();
            expectedLevels.expectedVerboses = 3;
            expectedLevels.check(imageContrast);

            // Make sure no other errors in the system.
            imageContrast = filter(imageContrast, EnumSet.of(Level.ERROR, Level.WARNING));
            List<Issue> filtered = filter(result.getIssues(), EnumSet.of(Level.ERROR, Level.WARNING));
            checkEquals(filtered, imageContrast);
        }, false);
    }

    @Test
    public void testTouchTargetSizeCheck() throws Exception {
        render("a11y_test_touch_target_size.xml", session -> {
            ValidatorResult result = getRenderResult(session);
            List<Issue> targetSizes = filter(result.getIssues(), "TouchTargetSizeCheck");

            ExpectedLevels expectedLevels = new ExpectedLevels();
            expectedLevels.expectedErrors = 5;
            expectedLevels.expectedVerboses = 1;
            expectedLevels.check(targetSizes);

            // Make sure no other errors in the system.
            targetSizes = filter(targetSizes, EnumSet.of(Level.ERROR));
            List<Issue> filtered = filter(result.getIssues(), EnumSet.of(Level.ERROR));
            checkEquals(filtered, targetSizes);
        });
    }

    private void checkEquals(List<Issue> list1, List<Issue> list2) {
        assertEquals(list1.size(), list2.size());
        for (int i = 0; i < list1.size(); i++) {
            assertEquals(list1.get(i), list2.get(i));
        }
    }

    private List<Issue> filter(List<ValidatorData.Issue> results, EnumSet<Level> errors) {
        return results.stream().filter(
                issue -> errors.contains(issue.mLevel)).collect(Collectors.toList());
    }

    private List<Issue> filter(
            List<ValidatorData.Issue> results, String sourceClass) {
        return results.stream().filter(
                issue -> sourceClass.equals(issue.mSourceClass)).collect(Collectors.toList());
    }

    private ValidatorResult getRenderResult(RenderSession session) {
        Object validationData = session.getValidationData();
        assertNotNull(validationData);
        assertTrue(validationData instanceof ValidatorResult);
        return (ValidatorResult) validationData;
    }
    private void render(String fileName, RenderSessionListener verifier) throws Exception {
        render(fileName, verifier, true);
    }

    private void render(
            String fileName,
            RenderSessionListener verifier,
            boolean enableImageCheck) throws Exception {
        LayoutValidator.updatePolicy(new Policy(
                EnumSet.of(Type.ACCESSIBILITY, Type.RENDER),
                EnumSet.of(Level.ERROR, Level.WARNING, Level.INFO, Level.VERBOSE)));

        LayoutPullParser parser = createParserFromPath(fileName);
        LayoutLibTestCallback layoutLibCallback =
                new LayoutLibTestCallback(getLogger(), mDefaultClassLoader);
        layoutLibCallback.initResources();
        SessionParamsBuilder params = getSessionParamsBuilder()
                .setParser(parser)
                .setConfigGenerator(ConfigGenerator.NEXUS_5)
                .setCallback(layoutLibCallback)
                .disableDecoration()
                .enableLayoutValidation();

        if (enableImageCheck) {
            params.enableLayoutValidationImageCheck();
        }

        render(sBridge, params.build(), -1, verifier);
    }

    /**
     * Helper class that checks the list of issues..
     */
    private static class ExpectedLevels {
        // Number of errors expected
        public int expectedErrors = 0;
        // Number of warnings expected
        public int expectedWarnings = 0;
        // Number of infos expected
        public int expectedInfos = 0;
        // Number of verboses expected
        public int expectedVerboses = 0;

        public void check(List<Issue> issues) {
            int errors = 0;
            int warnings = 0;
            int infos = 0;
            int verboses = 0;

            for (Issue issue : issues) {
                switch (issue.mLevel) {
                    case ERROR:
                        errors++;
                        break;
                    case WARNING:
                        warnings++;
                        break;
                    case INFO:
                        infos++;
                        break;
                    case VERBOSE:
                        verboses++;
                        break;
                }
            }

            assertEquals("Number of expected errors", expectedErrors, errors);
            assertEquals("Number of expected warnings",expectedWarnings, warnings);
            assertEquals("Number of expected infos", expectedInfos, infos);
            assertEquals("Number of expected verboses", expectedVerboses, verboses);

            int size = expectedErrors + expectedWarnings + expectedInfos + expectedVerboses;
            assertEquals("expected size", size, issues.size());
        }
    };
}
