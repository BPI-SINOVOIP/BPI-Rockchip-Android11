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

#ifndef DOC_COMMENT_H_

#define DOC_COMMENT_H_

#include <hidl-util/Formatter.h>

#include <string>
#include <vector>

#include "Location.h"

namespace android {

enum class CommentType {
    // when no particular style is specified
    UNSPECIFIED,

    // multiline comment that begins with /**
    DOC_MULTILINE,
    // begins with /* (used for headers)
    MULTILINE,
    // begins with '//'
    SINGLELINE,
};

struct DocComment {
    // parse comment and remove leading comment characters
    DocComment(const std::string& comment, const Location& location,
               CommentType type = CommentType::UNSPECIFIED);
    // raw comment
    DocComment(const std::vector<std::string>& lines, const Location& location,
               CommentType type = CommentType::UNSPECIFIED);

    void merge(const DocComment* comment);

    void emit(Formatter& out, CommentType type = CommentType::UNSPECIFIED) const;

    const std::vector<std::string>& lines() const { return mLines; }

    const Location& location() const { return mLocation; }

  private:
    std::vector<std::string> mLines;
    CommentType mType;
    Location mLocation;
};

struct DocCommentable {
    void setDocComment(const DocComment* docComment) { mDocComment = docComment; }
    void emitDocComment(Formatter& out) const {
        if (mDocComment != nullptr) {
            mDocComment->emit(out);
        }
    }

    const DocComment* getDocComment() const { return mDocComment; }

  private:
    const DocComment* mDocComment = nullptr;
};

}  // namespace android

#endif  // DOC_COMMENT_H_
