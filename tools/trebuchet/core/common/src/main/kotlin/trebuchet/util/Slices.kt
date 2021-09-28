/*
 * Copyright 2019 Google Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     https://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package trebuchet.util.slices

data class SliceContents(val name: String, val value: String?)

const val SLICE_NAME_ACTIVITY_DESTROY = "activityDestroy"
const val SLICE_NAME_ACTIVITY_PAUSE = "activityPause"
const val SLICE_NAME_ACTIVITY_RESUME = "activityResume"
const val SLICE_NAME_ACTIVITY_START = "activityStart"
const val SLICE_NAME_ACTIVITY_THREAD_MAIN = "ActivityThreadMain"
const val SLICE_NAME_APP_IMAGE_INTERN_STRING = "AppImage:InternString"
const val SLICE_NAME_APP_IMAGE_LOADING = "AppImage:Loading"
const val SLICE_NAME_APP_LAUNCH = "launching"
const val SLICE_NAME_ALTERNATE_DEX_OPEN_START = "Dex file open"
const val SLICE_NAME_BIND_APPLICATION = "bindApplication"
const val SLICE_NAME_DRAWING = "drawing"
const val SLICE_NAME_INFLATE = "inflate"
const val SLICE_NAME_OPEN_DEX_FILE_FUNCTION = "std::unique_ptr<const DexFile> art::OatDexFile::OpenDexFile(std::string *) const"
const val SLICE_NAME_OBFUSCATED_TRACE_START = "X."
const val SLICE_NAME_POST_FORK = "PostFork"
const val SLICE_NAME_PROC_START = "Start proc"
const val SLICE_NAME_REPORT_FULLY_DRAWN = "ActivityManager:ReportingFullyDrawn"
const val SLICE_NAME_ZYGOTE_INIT = "ZygoteInit"

val SLICE_MAPPERS = arrayOf(
    Regex("^(Collision check)"),
    Regex("^(launching):\\s+([\\w\\.])+"),
    Regex("^(Lock contention).*"),
    Regex("^(monitor contention).*"),
    Regex("^(NetworkSecurityConfigProvider.install)"),
    Regex("^(notifyContentCapture\\((?:true|false)\\))\\sfor\\s(.*)"),
    Regex("^(Open dex file)(?: from RAM)? ([\\w\\./]*)"),
    Regex("^(Open oat file)\\s+(.*)"),
    Regex("^(RegisterDexFile)\\s+(.*)"),
    Regex("^($SLICE_NAME_REPORT_FULLY_DRAWN)\\s+(.*)"),
    Regex("^(serviceCreate):.*className=([\\w\\.]+)"),
    Regex("^(serviceStart):.*cmp=([\\w\\./]+)"),
    Regex("^(Setup proxies)"),
    Regex("^($SLICE_NAME_PROC_START):\\s+(.*)"),
    Regex("^(SuspendThreadByThreadId) suspended (.+)$"),
    Regex("^(VerifyClass)(.*)"),

    // Default pattern for slices with a single-word name.
    Regex("^([\\w:]+)$")
)

fun parseSliceName(sliceString: String): SliceContents {
    when {
    // Handle special cases.
        sliceString == SLICE_NAME_OPEN_DEX_FILE_FUNCTION -> return SliceContents("Open dex file function invocation", null)
        sliceString.startsWith(SLICE_NAME_ALTERNATE_DEX_OPEN_START) -> return SliceContents("Open dex file", sliceString.split(" ").last().trim())
        sliceString.startsWith(SLICE_NAME_OBFUSCATED_TRACE_START) -> return SliceContents("Obfuscated trace point", null)
        sliceString[0] == '/' -> return SliceContents("Load Dex files from classpath", null)

        else -> {
            // Search the slice mapping patterns.
            for (pattern in SLICE_MAPPERS) {
                val matchResult = pattern.find(sliceString)

                if (matchResult != null) {
                    val sliceType = matchResult.groups[1]!!.value.trim()

                    val sliceDetails =
                        if (matchResult.groups.size > 2 && !matchResult.groups[2]!!.value.isEmpty()) {
                            matchResult.groups[2]!!.value.trim()
                        } else {
                            null
                        }

                    return SliceContents(sliceType, sliceDetails)
                }
            }

            return SliceContents("Unknown Slice", sliceString)
        }
    }

}