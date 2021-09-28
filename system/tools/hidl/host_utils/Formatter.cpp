/*
 * Copyright (C) 2016 The Android Open Source Project
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

#include "Formatter.h"

#include <assert.h>

#include <android-base/logging.h>
#include <android-base/strings.h>
#include <string>
#include <vector>

namespace android {

Formatter::Formatter() : mFile(nullptr /* invalid */), mIndentDepth(0), mCurrentPosition(0) {}

Formatter::Formatter(FILE* file, size_t spacesPerIndent)
    : mFile(file == nullptr ? stdout : file),
      mIndentDepth(0),
      mSpacesPerIndent(spacesPerIndent),
      mCurrentPosition(0) {}

Formatter::~Formatter() {
    if (mFile != stdout) {
        fclose(mFile);
    }
    mFile = nullptr;
}

void Formatter::indent(size_t level) {
    mIndentDepth += level;
}

void Formatter::unindent(size_t level) {
    assert(mIndentDepth >= level);
    mIndentDepth -= level;
}

Formatter& Formatter::indent(size_t level, const std::function<void(void)>& func) {
    this->indent(level);
    func();
    this->unindent(level);
    return *this;
}

Formatter& Formatter::indent(const std::function<void(void)>& func) {
    return this->indent(1, func);
}

Formatter& Formatter::block(const std::function<void(void)>& func) {
    (*this) << "{\n";
    this->indent(func);
    return (*this) << "}";
}

void Formatter::pushLinePrefix(const std::string& prefix) {
    mLinePrefix.push_back(prefix);
}

void Formatter::popLinePrefix() {
    mLinePrefix.pop_back();
}

Formatter &Formatter::endl() {
    return (*this) << "\n";
}

Formatter& Formatter::sIf(const std::string& cond, const std::function<void(void)>& block) {
    (*this) << "if (" << cond << ") ";
    return this->block(block);
}

Formatter& Formatter::sElseIf(const std::string& cond, const std::function<void(void)>& block) {
    (*this) << " else if (" << cond << ") ";
    return this->block(block);
}

Formatter& Formatter::sElse(const std::function<void(void)>& block) {
    (*this) << " else ";
    return this->block(block);
}

Formatter& Formatter::sFor(const std::string& stmts, const std::function<void(void)>& block) {
    (*this) << "for (" << stmts << ") ";
    return this->block(block);
}

Formatter& Formatter::sTry(const std::function<void(void)>& block) {
    (*this) << "try ";
    return this->block(block);
}

Formatter& Formatter::sCatch(const std::string& exception, const std::function<void(void)>& block) {
    (*this) << " catch (" << exception << ") ";
    return this->block(block);
}

Formatter& Formatter::sFinally(const std::function<void(void)>& block) {
    (*this) << " finally ";
    return this->block(block);
}

Formatter& Formatter::sWhile(const std::string& cond, const std::function<void(void)>& block) {
    (*this) << "while (" << cond << ") ";
    return this->block(block);
}

Formatter& Formatter::operator<<(const std::string& out) {
    const size_t len = out.length();
    size_t start = 0;

    const std::string& prefix = base::Join(mLinePrefix, "");
    while (start < len) {
        size_t pos = out.find('\n', start);

        if (pos == std::string::npos) {
            if (mCurrentPosition == 0) {
                fprintf(mFile, "%*s", (int)(getIndentation()), "");
                fprintf(mFile, "%s", prefix.c_str());
                mCurrentPosition = getIndentation() + prefix.size();
            }

            std::string sub = out.substr(start);
            output(sub);
            mCurrentPosition += sub.size();
            break;
        }

        if (mCurrentPosition == 0 && (pos > start || !prefix.empty())) {
            fprintf(mFile, "%*s", (int)(getIndentation()), "");
            fprintf(mFile, "%s", prefix.c_str());
            mCurrentPosition = getIndentation() + prefix.size();
        }

        if (pos == start) {
            fprintf(mFile, "\n");
            mCurrentPosition = 0;
        } else if (pos > start) {
            output(out.substr(start, pos - start + 1));
            mCurrentPosition = 0;
        }

        start = pos + 1;
    }

    return *this;
}

void Formatter::printBlock(const WrappedOutput::Block& block, size_t lineLength) {
    size_t prefixSize = 0;
    for (const std::string& prefix : mLinePrefix) {
        prefixSize += prefix.size();
    }

    size_t lineStart = mCurrentPosition ?: (getIndentation() + prefixSize);
    size_t blockSize = block.computeSize(false);
    if (blockSize + lineStart < lineLength) {
        block.print(*this, false);
        return;
    }

    // Everything will not fit on this line. Try to fit it on the next line.
    blockSize = block.computeSize(true);
    if ((blockSize + getIndentation() + mSpacesPerIndent + prefixSize) < lineLength) {
        *this << "\n";
        indent();

        block.print(*this, true);

        unindent();
        return;
    }

    if (!block.content.empty()) {
        // Doesn't have subblocks. This means that the block itself is too big.
        // Have to print it out.
        *this << "\n";
        indent();

        block.print(*this, true);

        unindent();
        return;
    }

    // Everything will not fit on this line. Go through all the children
    for (const WrappedOutput::Block& subBlock : block.blocks) {
        printBlock(subBlock, lineLength);
    }
}

Formatter& Formatter::operator<<(const WrappedOutput& wrappedOutput) {
    printBlock(wrappedOutput.mRootBlock, wrappedOutput.mLineLength);

    return *this;
}

// NOLINT to suppress missing parentheses warning about __type__.
#define FORMATTER_INPUT_INTEGER(__type__)                       \
    Formatter& Formatter::operator<<(__type__ n) { /* NOLINT */ \
        return (*this) << std::to_string(n);                    \
    }

FORMATTER_INPUT_INTEGER(short);
FORMATTER_INPUT_INTEGER(unsigned short);
FORMATTER_INPUT_INTEGER(int);
FORMATTER_INPUT_INTEGER(unsigned int);
FORMATTER_INPUT_INTEGER(long);
FORMATTER_INPUT_INTEGER(unsigned long);
FORMATTER_INPUT_INTEGER(long long);
FORMATTER_INPUT_INTEGER(unsigned long long);
FORMATTER_INPUT_INTEGER(float);
FORMATTER_INPUT_INTEGER(double);
FORMATTER_INPUT_INTEGER(long double);

#undef FORMATTER_INPUT_INTEGER

// NOLINT to suppress missing parentheses warning about __type__.
#define FORMATTER_INPUT_CHAR(__type__)                          \
    Formatter& Formatter::operator<<(__type__ c) { /* NOLINT */ \
        return (*this) << std::string(1, (char)c);              \
    }

FORMATTER_INPUT_CHAR(char);
FORMATTER_INPUT_CHAR(signed char);
FORMATTER_INPUT_CHAR(unsigned char);

#undef FORMATTER_INPUT_CHAR

bool Formatter::isValid() const {
    return mFile != nullptr;
}

size_t Formatter::getIndentation() const {
    return mSpacesPerIndent * mIndentDepth;
}

void Formatter::output(const std::string &text) const {
    CHECK(isValid());

    fprintf(mFile, "%s", text.c_str());
}

WrappedOutput::Block::Block(const std::string& content, Block* const parent)
    : content(content), parent(parent) {}

size_t WrappedOutput::Block::computeSize(bool wrapped) const {
    CHECK(content.empty() || blocks.empty());

    // There is a wrap, so the block would not be printed
    if (printUnlessWrapped && wrapped) return 0;

    size_t size = content.size();
    for (auto block = blocks.begin(); block != blocks.end(); ++block) {
        if (block == blocks.begin()) {
            // Only the first one can be wrapped (since content.empty())
            size += block->computeSize(wrapped);
        } else {
            size += block->computeSize(false);
        }
    }

    return size;
}

void WrappedOutput::Block::print(Formatter& out, bool wrapped) const {
    CHECK(content.empty() || blocks.empty());

    // There is a wrap, so the block should not be printed
    if (printUnlessWrapped && wrapped) return;

    out << content;
    for (auto block = blocks.begin(); block != blocks.end(); ++block) {
        if (block == blocks.begin()) {
            // Only the first one can be wrapped (since content.empty())
            block->print(out, wrapped);
        } else {
            block->print(out, false);
        }
    }
}

WrappedOutput::WrappedOutput(size_t lineLength)
    : mLineLength(lineLength), mRootBlock(Block("", nullptr)) {
    mCurrentBlock = &mRootBlock;
}

WrappedOutput& WrappedOutput::operator<<(const std::string& str) {
    std::vector<Block>& blockVec = mCurrentBlock->blocks;
    if (!blockVec.empty()) {
        Block& last = blockVec.back();
        if (!last.populated && last.blocks.empty()) {
            last.content += str;

            return *this;
        }
    }

    blockVec.emplace_back(str, mCurrentBlock);
    return *this;
}

WrappedOutput& WrappedOutput::printUnlessWrapped(const std::string& str) {
    std::vector<Block>& blockVec = mCurrentBlock->blocks;
    if (!blockVec.empty()) {
        blockVec.back().populated = true;
    }

    blockVec.emplace_back(str, mCurrentBlock);
    blockVec.back().populated = true;
    blockVec.back().printUnlessWrapped = true;

    return *this;
}

void WrappedOutput::group(const std::function<void(void)>& block) {
    std::vector<Block>& blockVec = mCurrentBlock->blocks;
    if (!blockVec.empty()) {
        blockVec.back().populated = true;
    }

    blockVec.emplace_back("", mCurrentBlock);
    mCurrentBlock = &blockVec.back();

    block();

    mCurrentBlock->populated = true;
    mCurrentBlock = mCurrentBlock->parent;
}

}  // namespace android
