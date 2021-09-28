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

#pragma once

#include <string>
#include <vector>

namespace android {

struct Coordinator;
struct Formatter;
struct FQName;
struct Interface;
struct Method;
struct NamedType;
struct Scope;
struct Type;

struct AidlHelper {
    /* FQName helpers */
    // getAidlName returns the type names
    // android.hardware.foo@1.0::IBar.Baz -> IBarBaz
    static std::string getAidlName(const FQName& fqName);

    // getAidlPackage returns the AIDL package
    // android.hardware.foo@1.x -> android.hardware.foo
    // android.hardware.foo@2.x -> android.hardware.foo2
    static std::string getAidlPackage(const FQName& fqName);

    // getAidlFQName = getAidlPackage + "." + getAidlName
    static std::string getAidlFQName(const FQName& fqName);

    static void emitFileHeader(Formatter& out, const NamedType& type);
    static Formatter getFileWithHeader(const NamedType& namedType, const Coordinator& coordinator);

    /* Methods for Type */
    static std::string getAidlType(const Type& type, const FQName& relativeTo);

    /* Methods for NamedType */
    static void emitAidl(const NamedType& namedType, const Coordinator& coordinator);

    /* Methods for Scope */
    static void emitAidl(const Scope& scope, const Coordinator& coordinator);

    /* Methods for Interface */
    static void emitAidl(const Interface& interface, const Coordinator& coordinator);
    // Returns all methods that would exist in an AIDL equivalent interface
    static std::vector<const Method*> getUserDefinedMethods(const Interface& interface);

    static Formatter& notes();
    static void setNotes(Formatter* formatter);

  private:
    // This is the formatter to use for additional conversion output
    static Formatter* notesFormatter;
};

}  // namespace android
