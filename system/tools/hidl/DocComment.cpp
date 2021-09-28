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

#include "DocComment.h"

#include <android-base/logging.h>
#include <android-base/strings.h>
#include <hidl-util/StringHelper.h>

#include <cctype>
#include <vector>

#include "Location.h"

namespace android {

DocComment::DocComment(const std::string& comment, const Location& location, CommentType type)
    : DocComment(std::vector<std::string>(), location, type) {
    std::vector<std::string> lines = base::Split(base::Trim(comment), "\n");

    bool foundFirstLine = false;

    for (size_t l = 0; l < lines.size(); l++) {
        const std::string& line = lines[l];

        // Delete prefixes like "    * ", "   *", or "    ".
        size_t idx = 0;
        for (; idx < line.size() && isspace(line[idx]); idx++)
            ;
        if (idx < line.size() && line[idx] == '*') idx++;
        if (idx < line.size() && line[idx] == ' ') idx++;

        const std::string& sanitizedLine = line.substr(idx);
        int i = sanitizedLine.size();
        for (; i > 0 && isspace(sanitizedLine[i - 1]); i--)
            ;

        // Either the size is 0 or everything was whitespace.
        bool isEmptyLine = i == 0;

        foundFirstLine = foundFirstLine || !isEmptyLine;
        if (!foundFirstLine) continue;

        // if isEmptyLine, i == 0 so substr == ""
        mLines.push_back(sanitizedLine.substr(0, i));
    }
}

DocComment::DocComment(const std::vector<std::string>& lines, const Location& location,
                       CommentType type)
    : mLines(lines), mType(type), mLocation(location) {}

void DocComment::merge(const DocComment* comment) {
    mLines.insert(mLines.end(), 2, "");
    mLines.insert(mLines.end(), comment->mLines.begin(), comment->mLines.end());
    mLocation.setLocation(mLocation.begin(), comment->mLocation.end());
}

void DocComment::emit(Formatter& out, CommentType type) const {
    CommentType useType = type;
    if (useType == CommentType::UNSPECIFIED) useType = mType;
    if (useType == CommentType::UNSPECIFIED) useType = CommentType::DOC_MULTILINE;

    bool isMultiline = useType != CommentType::SINGLELINE;

    // singleline comments include '//' as part of line text
    if (isMultiline) {
        switch (useType) {
            case CommentType::DOC_MULTILINE:
                out << "/**\n";
                break;
            case CommentType::MULTILINE:
                out << "/*\n";
                break;
            default:
                LOG(FATAL) << "bad type: " << static_cast<int>(useType);
        }

        out.pushLinePrefix(" *");
    }

    for (const std::string& line : mLines) {
        out << (line.empty() && isMultiline ? "" : " ") << line << "\n";
    }

    if (isMultiline) {
        out.popLinePrefix();
        out << " */\n";
    }
}

}  // namespace android
