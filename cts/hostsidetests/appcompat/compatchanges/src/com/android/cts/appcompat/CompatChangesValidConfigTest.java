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

package com.android.cts.appcompat;

import static com.google.common.truth.Truth.assertThat;


import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;
import java.util.Objects;
import java.util.regex.Matcher;
import java.util.regex.Pattern;
import java.util.stream.Collectors;

import android.compat.cts.CompatChangeGatingTestCase;

import org.w3c.dom.Document;
import org.w3c.dom.Element;
import org.w3c.dom.NamedNodeMap;
import org.w3c.dom.Node;
import org.w3c.dom.NodeList;

import javax.xml.parsers.DocumentBuilder;
import javax.xml.parsers.DocumentBuilderFactory;

public final class CompatChangesValidConfigTest extends CompatChangeGatingTestCase {

    private static class Change {
        private static final Pattern CHANGE_REGEX = Pattern.compile("^ChangeId\\((?<changeId>[0-9]+)"
                                                + "(; name=(?<changeName>[^;]+))?"
                                                + "(; enableAfterTargetSdk=(?<targetSdk>[0-9]+))?"
                                                + "(; (?<disabled>disabled))?"
                                                + "(; (?<loggingOnly>loggingOnly))?"
                                                + "(; packageOverrides=(?<overrides>[^\\)]+))?"
                                                + "\\)");
        long changeId;
        String changeName;
        int targetSdk;
        boolean disabled;
        boolean loggingOnly;
        boolean hasOverrides;

        private Change(long changeId, String changeName, int targetSdk, boolean disabled,
                boolean loggingOnly, boolean hasOverrides) {
            this.changeId = changeId;
            this.changeName = changeName;
            this.targetSdk = targetSdk;
            this.disabled = disabled;
            this.loggingOnly = loggingOnly;
            this.hasOverrides = hasOverrides;
        }

        static Change fromString(String line) {
            long changeId = 0;
            String changeName;
            int targetSdk = 0;
            boolean disabled = false;
            boolean loggingOnly = false;
            boolean hasOverrides = false;

            Matcher matcher = CHANGE_REGEX.matcher(line);
            if (!matcher.matches()) {
                throw new RuntimeException("Could not match line " + line);
            }

            try {
                changeId = Long.parseLong(matcher.group("changeId"));
            } catch (NumberFormatException e) {
                throw new RuntimeException("No or invalid changeId!", e);
            }
            changeName = matcher.group("changeName");
            String targetSdkAsString = matcher.group("targetSdk");
            if (targetSdkAsString != null) {
                try {
                    targetSdk = Integer.parseInt(targetSdkAsString);
                } catch (NumberFormatException e) {
                    throw new RuntimeException("Invalid targetSdk for change!", e);
                }
            }
            if (matcher.group("disabled") != null) {
                disabled = true;
            }
            if (matcher.group("loggingOnly") != null) {
                loggingOnly = true;
            }
            if (matcher.group("overrides") != null) {
                hasOverrides = true;
            }
            return new Change(changeId, changeName, targetSdk, disabled, loggingOnly, hasOverrides);
        }

        static Change fromNode(Node node) {
            Element element = (Element) node;
            long changeId = Long.parseLong(element.getAttribute("id"));
            String changeName = element.getAttribute("name");
            int targetSdk = 0;
            if (element.hasAttribute("enableAfterTargetSdk")) {
                targetSdk = Integer.parseInt(element.getAttribute("enableAfterTargetSdk"));
            }
            boolean disabled = false;
            if (element.hasAttribute("disabled")) {
                disabled = true;
            }
            boolean loggingOnly = false;
            if (element.hasAttribute("loggingOnly")) {
                loggingOnly = true;
            }
            boolean hasOverrides = false;
            return new Change(changeId, changeName, targetSdk, disabled, loggingOnly, hasOverrides);
        }
        @Override
        public int hashCode() {
            return Objects.hash(changeId, changeName, targetSdk, disabled, hasOverrides);
        }
        @Override
        public boolean equals(Object other) {
            if (this == other) {
                return true;
            }
            if (other == null || !(other instanceof Change)) {
                return false;
            }
            Change that = (Change) other;
            return this.changeId == that.changeId
                && Objects.equals(this.changeName, that.changeName)
                && this.targetSdk == that.targetSdk
                && this.disabled == that.disabled
                && this.loggingOnly == that.loggingOnly
                && this.hasOverrides == that.hasOverrides;
        }
        @Override
        public String toString() {
            final StringBuilder sb = new StringBuilder();
            sb.append("ChangeId(" + changeId);
            if (changeName != null && !changeName.isEmpty()) {
                sb.append("; name=" + changeName);
            }
            if (targetSdk != 0) {
                sb.append("; enableAfterTargetSdk=" + targetSdk);
            }
            if (disabled) {
                sb.append("; disabled");
            }
            if (hasOverrides) {
                sb.append("; packageOverrides={something}");
            }
            sb.append(")");
            return sb.toString();
        }
    }

    /**
     * Get the on device compat config.
     */
    private List<Change> getOnDeviceCompatConfig() throws Exception {
        String config = runCommand("dumpsys platform_compat");
        return Arrays.stream(config.split("\n"))
                .map(Change::fromString)
                .collect(Collectors.toList());
    }

    /**
     * Parse the expected (i.e. defined in platform) config xml.
     */
    private List<Change> getExpectedCompatConfig() throws Exception {
        DocumentBuilderFactory dbf = DocumentBuilderFactory.newInstance();
        DocumentBuilder db = dbf.newDocumentBuilder();
        Document dom = db.parse(getClass().getResourceAsStream("/cts_all_compat_config.xml"));
        Element root = dom.getDocumentElement();
        NodeList changeNodes = root.getElementsByTagName("compat-change");
        List<Change> changes = new ArrayList<>();
        for (int nodeIdx = 0; nodeIdx < changeNodes.getLength(); ++nodeIdx) {
            Change change = Change.fromNode(changeNodes.item(nodeIdx));
            // Exclude logging only changes from the expected config. See b/155264388.
            if (!change.loggingOnly) {
                changes.add(change);
            }
        }
        return changes;
    }

    /**
     * Check that there are no overrides.
     */
    public void testNoOverrides() throws Exception {
        for (Change c : getOnDeviceCompatConfig()) {
            assertThat(c.hasOverrides).isFalse();
        }
    }

    /**
     * Check that the on device config contains all the expected change ids defined in the platform.
     * The device may contain extra changes, but none may be removed.
     */
    public void testDeviceContainsExpectedConfig() throws Exception {
        assertThat(getOnDeviceCompatConfig()).containsAllIn(getExpectedCompatConfig());
    }

}
