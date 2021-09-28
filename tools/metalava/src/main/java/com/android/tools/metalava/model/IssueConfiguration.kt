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

package com.android.tools.metalava.model

import com.android.tools.metalava.Severity
import com.android.tools.metalava.doclava1.Issues

/** An issue configuration is a set of overrides for severities for various [Issues.Issue] */
class IssueConfiguration {
    private val overrides = mutableMapOf<Issues.Issue, Severity>()

    /** Returns the severity of the given issue */
    fun getSeverity(issue: Issues.Issue): Severity {
        overrides[issue]?.let { return it }
        if (issue.defaultLevel == Severity.INHERIT) {
            return getSeverity(issue.parent!!)
        }
        return issue.defaultLevel
    }

    fun setSeverity(issue: Issues.Issue, severity: Severity) {
        check(severity != Severity.INHERIT)
        overrides[issue] = severity
    }

    /** Set the severity of the given issue to [Severity.ERROR] */
    fun error(issue: Issues.Issue) {
        setSeverity(issue, Severity.ERROR)
    }

    /** Set the severity of the given issue to [Severity.HIDDEN] */
    fun hide(issue: Issues.Issue) {
        setSeverity(issue, Severity.HIDDEN)
    }

    fun reset() {
        overrides.clear()
    }
}

/** Default error configuration: uses the severities as configured in [Options] */
val defaultConfiguration = IssueConfiguration()

/** Current configuration to apply when reporting errors */
var configuration = defaultConfiguration
