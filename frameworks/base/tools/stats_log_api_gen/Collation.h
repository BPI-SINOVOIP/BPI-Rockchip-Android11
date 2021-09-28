/*
 * Copyright (C) 2017, The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef ANDROID_STATS_LOG_API_GEN_COLLATION_H
#define ANDROID_STATS_LOG_API_GEN_COLLATION_H

#include <google/protobuf/descriptor.h>
#include <stdint.h>

#include <map>
#include <set>
#include <vector>

#include "frameworks/base/cmds/statsd/src/atom_field_options.pb.h"

namespace android {
namespace stats_log_api_gen {

using google::protobuf::Descriptor;
using google::protobuf::FieldDescriptor;
using std::map;
using std::set;
using std::shared_ptr;
using std::string;
using std::vector;

const int PULL_ATOM_START_ID = 10000;

const int FIRST_UID_IN_CHAIN_ID = 0;

enum AnnotationId : uint8_t {
    ANNOTATION_ID_IS_UID = 1,
    ANNOTATION_ID_TRUNCATE_TIMESTAMP = 2,
    ANNOTATION_ID_PRIMARY_FIELD = 3,
    ANNOTATION_ID_EXCLUSIVE_STATE = 4,
    ANNOTATION_ID_PRIMARY_FIELD_FIRST_UID = 5,
    ANNOTATION_ID_DEFAULT_STATE = 6,
    ANNOTATION_ID_TRIGGER_STATE_RESET = 7,
    ANNOTATION_ID_STATE_NESTED = 8,
};

const int ATOM_ID_FIELD_NUMBER = -1;

const string DEFAULT_MODULE_NAME = "DEFAULT";

/**
 * The types for atom parameters.
 */
typedef enum {
    JAVA_TYPE_UNKNOWN = 0,

    JAVA_TYPE_ATTRIBUTION_CHAIN = 1,
    JAVA_TYPE_BOOLEAN = 2,
    JAVA_TYPE_INT = 3,
    JAVA_TYPE_LONG = 4,
    JAVA_TYPE_FLOAT = 5,
    JAVA_TYPE_DOUBLE = 6,
    JAVA_TYPE_STRING = 7,
    JAVA_TYPE_ENUM = 8,
    JAVA_TYPE_KEY_VALUE_PAIR = 9,

    JAVA_TYPE_OBJECT = -1,
    JAVA_TYPE_BYTE_ARRAY = -2,
} java_type_t;

enum AnnotationType {
    ANNOTATION_TYPE_UNKNOWN = 0,
    ANNOTATION_TYPE_INT = 1,
    ANNOTATION_TYPE_BOOL = 2,
};

union AnnotationValue {
    int intValue;
    bool boolValue;

    AnnotationValue(const int value) : intValue(value) {
    }
    AnnotationValue(const bool value) : boolValue(value) {
    }
};

struct Annotation {
    const AnnotationId annotationId;
    const int atomId;
    AnnotationType type;
    AnnotationValue value;

    inline Annotation(AnnotationId annotationId, int atomId, AnnotationType type,
                      AnnotationValue value)
        : annotationId(annotationId), atomId(atomId), type(type), value(value) {
    }
    inline ~Annotation() {
    }

    inline bool operator<(const Annotation& that) const {
        return atomId == that.atomId ? annotationId < that.annotationId : atomId < that.atomId;
    }
};

struct SharedComparator {
    template <typename T>
    inline bool operator()(const shared_ptr<T>& lhs, const shared_ptr<T>& rhs) const {
        return (*lhs) < (*rhs);
    }
};

using AnnotationSet = set<shared_ptr<Annotation>, SharedComparator>;

using FieldNumberToAnnotations = map<int, AnnotationSet>;

/**
 * The name and type for an atom field.
 */
struct AtomField {
    string name;
    java_type_t javaType;

    // If the field is of type enum, the following map contains the list of enum
    // values.
    map<int /* numeric value */, string /* value name */> enumValues;

    inline AtomField() : name(), javaType(JAVA_TYPE_UNKNOWN) {
    }
    inline AtomField(const AtomField& that)
        : name(that.name), javaType(that.javaType), enumValues(that.enumValues) {
    }

    inline AtomField(string n, java_type_t jt) : name(n), javaType(jt) {
    }
    inline ~AtomField() {
    }
};

/**
 * The name and code for an atom.
 */
struct AtomDecl {
    int code;
    string name;

    string message;
    vector<AtomField> fields;

    FieldNumberToAnnotations fieldNumberToAnnotations;

    vector<int> primaryFields;
    int exclusiveField = 0;
    int defaultState = INT_MAX;
    int triggerStateReset = INT_MAX;
    bool nested = true;

    int uidField = 0;

    AtomDecl();
    AtomDecl(const AtomDecl& that);
    AtomDecl(int code, const string& name, const string& message);
    ~AtomDecl();

    inline bool operator<(const AtomDecl& that) const {
        return (code == that.code) ? (name < that.name) : (code < that.code);
    }
};

using AtomDeclSet = set<shared_ptr<AtomDecl>, SharedComparator>;

// Maps a field number to a set of atoms that have annotation(s) for their field with that field
// number.
using FieldNumberToAtomDeclSet = map<int, AtomDeclSet>;

using SignatureInfoMap = map<vector<java_type_t>, FieldNumberToAtomDeclSet>;

struct Atoms {
    SignatureInfoMap signatureInfoMap;
    AtomDeclSet decls;
    AtomDeclSet non_chained_decls;
    SignatureInfoMap nonChainedSignatureInfoMap;
};

/**
 * Gather the information about the atoms.  Returns the number of errors.
 */
int collate_atoms(const Descriptor* descriptor, const string& moduleName, Atoms* atoms);
int collate_atom(const Descriptor* atom, AtomDecl* atomDecl, vector<java_type_t>* signature);

}  // namespace stats_log_api_gen
}  // namespace android

#endif  // ANDROID_STATS_LOG_API_GEN_COLLATION_H
