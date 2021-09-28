/*
 * Copyright 2020 The Android Open Source Project
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

#ifndef ANDROID_AUDIO_METADATA_H
#define ANDROID_AUDIO_METADATA_H

#ifdef __cplusplus

#include <any>
#include <map>
#include <string>
#include <tuple>
#include <vector>

/**
 * Audio Metadata: a C++ Object based map.
 *
 * Data is a map of strings to Datum Objects.
 *
 * Datum is a C++ "Object", a direct instance of std::any, but limited
 * to only the following allowed types:
 *
 * Native                              Java equivalent
 * int32                               (int)
 * int64                               (long)
 * float                               (float)
 * double                              (double)
 * std::string                         (String)
 * Data (std::map<std::string, Datum>) (Map<String, Object>)
 *
 * Metadata code supports advanced automatic parceling.
 * TEST ONLY:
 * std::vector<Datum>                  (Object[])             --> vector of Objects
 * std::pair<Datum, Datum>             (Pair<Object, Object>) --> pair of Objects
 * std::vector<std::vector<std::pair<std::string, short>>>    --> recursive containers
 * struct { int i0; std::vector<int> v1; std::pair<int, int> p2; } --> struct parceling
 *
 * The Data map accepts typed Keys, which designate the type T of the
 * value associated with the Key<T> in the template parameter.
 *
 * CKey<T> is the constexpr version suitable for fixed compile-time constants.
 * Key<T> is the non-constexpr version.
 *
 * Notes: for future extensibility:
 *
 * In order to add a new type in.
 *
 * 1) Add the new type to the END of the metadata_types lists below.
 *
 * 2) The new type can be a primitive, or make use of containers std::map, std::vector,
 * std::pair, or be simple structs (see below).
 *
 * 3) Simple structs contain no pointers and all public data. The members can be based
 * on existing types.
 *   a) If trivially copyable (packed) primitive data,
 *      add to primitive_metadata_types.
 *   b) If the struct requires member-wise parceling, add to structural_metadata_types
 *      (current limit is 4 members).
 *
 * 4) The type system is recursive.
 *
 * Design notes:
 * 1) Tuple is intentionally not implemented as it isn't that readable.  This can
 * be revisited if the need comes up.  If you have more than a couple of elements,
 * we suggest embedding in a Data typed map or a Simple struct.
 *
 * 2) Each custom type e.g. vector<int>, pair<short, char> takes one
 * slot in the type index.  A full type description language is not implemented
 * here for brevity and clarity.
 */

namespace android::audio_utils::metadata {

// Determine if a type is a specialization of a templated type
// Example: is_specialization_v<T, std::vector>
// https://stackoverflow.com/questions/16337610/how-to-know-if-a-type-is-a-specialization-of-stdvector

template <typename Test, template <typename...> class Ref>
struct is_specialization : std::false_type {};

template <template <typename...> class Ref, typename... Args>
struct is_specialization<Ref<Args...>, Ref>: std::true_type {};

template <typename Test, template <typename...> class Ref>
inline constexpr bool is_specialization_v = is_specialization<Test, Ref>::value;

// For static assert(false) we need a template version to avoid early failure.
// See: https://stackoverflow.com/questions/51523965/template-dependent-false
template <typename T>
inline constexpr bool dependent_false_v = false;

// Determine the number of arguments required for structured binding.
// See the discussion here and follow the links:
// https://isocpp.org/blog/2016/08/cpp17-structured-bindings-convert-struct-to-a-tuple-simple-reflection
struct any_type {
  template<class T>
  constexpr operator T(); // non explicit
};

template <typename T, typename... TArgs>
decltype(void(T{std::declval<TArgs>()...}), std::true_type{}) test_is_braces_constructible(int);

template <typename, typename...>
std::false_type test_is_braces_constructible(...);

template <typename T, typename... TArgs>
using is_braces_constructible = decltype(test_is_braces_constructible<T, TArgs...>(0));

// Set up type comparison system
// see std::variant for the how the type_index() may be used.

/*
 * returns the index of type T in the type parameter list Ts.
 */
template <typename T, typename... Ts>
inline constexpr ssize_t type_index() {
    constexpr bool checks[] = {std::is_same_v<std::decay_t<T>, std::decay_t<Ts>>...};
    for (size_t i = 0; i < sizeof...(Ts); ++i) {
        if (checks[i]) return i; // the index in Ts.
    }
    return -1; // none found.
}

// compound_type is a holder of types. There are concatenation tricks of type lists
// but we don't need them here.
template <typename... Ts>
struct compound_type {
    inline static constexpr size_t size_v = sizeof...(Ts);
    template <typename T>
    inline static constexpr bool contains_v = type_index<T, Ts...>() >= 0;
    template <typename T>
    inline static constexpr ssize_t index_of() { return type_index<T, Ts...>(); }

    // create a tupe equivalent of the compound type. This is useful for
    // finding the nth type by std::tuple_element
    using tuple_t = std::tuple<Ts...>;

    /**
     * Applies function f to a datum pointer a.
     *
     * \param f is the function to apply.  It should take one argument,
     *     which is a (typed) pointer to the value stored in a.
     * \param a is the Datum object (derived from std::any).
     * \param result if non-null stores the return value of f (if f has a return value).
     *     result may be nullptr if one does not care about the return value of f.
     * \return true on success, false if there is no applicable data stored in a.
     */
    template <typename F, typename A>
    static bool apply(F f, A *a, std::any *result = nullptr) {
        return apply_impl<F, A, Ts...>(f, a, result);
    }

    // helper
    // Linear search in the number of types because of non-cached std:any_cast
    // lookup.  See std::visit for std::variant for constant time implementation.
    template <typename F, typename A, typename T, typename... Ts2>
    static bool apply_impl(F f, A *a, std::any *result) {
        auto t = std::any_cast<T>(a); // may be const ptr or not.
        if (t == nullptr) {
            return apply_impl<F, A, Ts2...>(f, a, result);
        }

        // only save result if the function has a non-void return type.
        // and result is not nullptr.
        using result_type = std::invoke_result_t<F, T*>;
        if constexpr (!std::is_same_v<result_type, void>) {
            if (result != nullptr) {
                *result = (result_type)f(t);
                return true;
            }
        }

        f(t); // discard the result
        return true;
    }

    // helper base class
    template <typename F, typename A>
    static bool apply_impl(F f __unused, A *a __unused, std::any *result __unused) {
        return false;
    }
};

#ifdef METADATA_TESTING

// This is a helper struct to verify that we are moving Datums instead
// of copying them.
struct MoveCount {
    int32_t mMoveCount = 0;
    int32_t mCopyCount = 0;

    MoveCount() = default;
    MoveCount(MoveCount&& other) {
        mMoveCount = other.mMoveCount + 1;
        mCopyCount = other.mCopyCount;
    }
    MoveCount(const MoveCount& other) {
        mMoveCount = other.mMoveCount;
        mCopyCount = other.mCopyCount + 1;
    }
    MoveCount &operator=(MoveCount&& other) {
        mMoveCount = other.mMoveCount + 1;
        mCopyCount = other.mCopyCount;
        return *this;
    }
    MoveCount &operator=(const MoveCount& other) {
        mMoveCount = other.mMoveCount;
        mCopyCount = other.mCopyCount + 1;
        return *this;
    }
};

// We can automatically parcel this "Arbitrary" struct
// since it has no pointers and all public members.
struct Arbitrary {
    int i0;
    std::vector<int> v1;
    std::pair<int, int> p2;
};

#endif

class Data;
class Datum;

// The order of this list must be maintained for binary compatibility
using metadata_types = compound_type<
    int32_t,
    int64_t,
    float,
    double,
    std::string,
    Data /* std::map<std::string, Datum> */
    // OK to add at end.
#ifdef METADATA_TESTING
    , std::vector<Datum>      // another complex object for testing
    , std::pair<Datum, Datum> // another complex object for testing
    , std::vector<std::vector<std::pair<std::string, short>>> // complex object
    , MoveCount
    , Arbitrary
#endif
    >;

// A subset of the metadata types may be directly copied as bytes
using primitive_metadata_types = compound_type<int32_t, int64_t, float, double
#ifdef METADATA_TESTING
    , MoveCount
#endif
    >;

// A subset of metadata types which are a struct-based.
using structural_metadata_types = compound_type<
#ifdef METADATA_TESTING
    Arbitrary
#endif
    >;

template <typename T>
inline constexpr bool is_primitive_metadata_type_v =
    primitive_metadata_types::contains_v<T>;

template <typename T>
inline constexpr bool is_structural_metadata_type_v =
    structural_metadata_types::contains_v<T>;

template <typename T>
inline constexpr bool is_metadata_type_v =
    metadata_types::contains_v<T>;

/**
 * Datum is the C++ version of Object, based on std::any
 * to be portable to other Data Object systems on std::any. For C++, there
 * are two forms of generalized Objects, std::variant and std::any.
 *
 * What is a variant?
 * std::variant is like a std::pair<type_index, union>, where the types
 * are kept in the template parameter list, and you only need to store
 * the type_index of the current value's type in the template parameter
 * list to find the value's type (to access data in the union).
 *
 * What is an any?
 * std::any is a std::pair<type_func, pointer> (though the standard encourages
 * small buffer optimization of the pointer for small data types,
 * so the pointer might actually be data).  The type_func is cleverly
 * implemented templated, so that one type_func exists per type.
 *
 * For datum, we use std::any, which is different than mediametrics::Item
 * (which uses std::variant).
 *
 * std::any is the C++ version of Java's Object.  One benefit of std::any
 * over std::variant is that it is portable outside of this package as a
 * std::any, to another C++ Object system based on std::any
 * (as we any_cast to discover the type). std::variant does not have this
 * portability (without copy conversion) because it requires an explicit
 * type list to be known in the template, so you can't exchange them freely
 * as the union size and the type/type ordering will be different in general
 * between two variant-based Object systems.
 *
 * std::any may work better with some recursive types than variants,
 * as it uses pointers so that physical size need not be known for type
 * definition.
 *
 * This is a design choice: mediametrics::Item as a closed system,
 * metadata::Datum as an open system.
 *
 * CAUTION:
 * For efficiency, prefer the use of std::any_cast<T>(std::any *)
 *       which returns a pointer to T (no extra copies.)
 *
 * std::any_cast<T>(std::any) returns an instance of T (copy constructor).
 * std::get<N>(std::variant) returns a reference (no extra copies).
 *
 * The Data map operations are optimized to return references to
 * avoid unnecessary copies.
 */

class Datum : public std::any {
public:
    // Don't add any virtual functions or non-static member variables.

    Datum() = default;

    // Do not make these explicit
    // Force type of std::any to exactly the values we permit to be parceled.
    template <typename T, typename = std::enable_if_t<is_metadata_type_v<T>>>
    Datum(T && t) : std::any(std::forward<T>(t)) {};

    template <typename T, typename = std::enable_if_t<is_metadata_type_v<T>>>
    Datum& operator=(T&& t) {
        static_cast<std::any *>(this)->operator=(std::forward<T>(t));
        return *this;
    }

    Datum(const char *t) : std::any(std::string(t)) {}; // special string handling
};

// PREVENT INCORRECT MODIFICATIONS
// Datum is a helping wrapper on std::any
// Don't add any non-static members
static_assert(sizeof(Datum) == sizeof(std::any));
// Nothing is virtual
static_assert(!std::is_polymorphic_v<Datum>);

/**
 * Keys
 *
 * Audio Metadata keys are typed.  Similar to variant's template typenames,
 * which directly indicate possible types in the union, the Audio Metadata
 * Keys contain the Value's Type in the Key's template type parameter.
 *
 * Example:
 *
 * inline constexpr CKey<int64_t> MY_BIGINT("bigint_is_mine");
 * inline constexpr CKey<Data> TABLE("table");
 *
 * Thus if we have a Data object d:
 *
 * decltype(d[TABLE]) is Data
 * decltype(d[MY_BIGINT) is int64_t
 */

/**
 * Key is a non-constexpr key which has local storage in a string.
 */
template <typename T, typename = std::enable_if_t<is_metadata_type_v<T>>>
class Key : private std::string {
public:
    using std::string::string; // base constructor
    const char *getName() const { return c_str(); }
};

/**
 * CKey is a constexpr key, which is preferred.
 *
 * inline constexpr CKey<int64_t> MY_BIGINT("bigint_is_mine");
 */
template <typename T, typename = std::enable_if_t<is_metadata_type_v<T>>>
class CKey  {
    const char * const mName;
public:
    explicit constexpr CKey(const char *name) : mName(name) {}
    CKey(const Key<T> &key) : mName(key.getName()) {}
    const char *getName() const { return mName; }
};

/**
 * Data is the storage for our Datums.
 *
 * It is implemented on top of std::map<std::string, Datum>
 * but we augment it with typed Key
 * getters and setters, as well as operator[] overloads.
 */
class Data : public std::map<std::string, Datum> {
public:
    // Don't add any virtual functions or non-static member variables.

    // We supplement the raw form of map with
    // the following typed form using Key.

    // Intentionally there is no get(), we suggest *get_ptr()
    template <template <typename /* T */, typename... /* enable-ifs */> class K, typename T>
    T* get_ptr(const K<T>& key, bool allocate = false) {
        auto it = find(key.getName());
        if (it == this->end()) {
            if (!allocate) return nullptr;
            it = emplace(key.getName(), T{}).first;
        }
        return std::any_cast<T>(&it->second);
    }

    template <template <typename, typename...> class K, typename T>
    const T* get_ptr(const K<T>& key) const {
        auto it = find(key.getName());
        if (it == this->end()) return nullptr;
        return std::any_cast<T>(&it->second);
    }

    template <template <typename, typename...> class K, typename T>
    void put(const K<T>& key, T && t) {
        (*this)[key.getName()] = std::forward<T>(t);
    }

    template <template <typename, typename...> class K>
    void put(const K<std::string>& key, const char *value) {
        (*this)[key.getName()] = value;
    }

    // We overload our operator[] so we unhide the one in the base class.
    using std::map<std::string, Datum>::operator[];

    template <template <typename, typename...> class K, typename T>
    T& operator[](const K<T> &key) {
        return *get_ptr(key, /* allocate */ true);
    }

    template <template <typename, typename...> class K, typename T>
    const T& operator[](const K<T> &key) const {
        return *get_ptr(key);
    }
};

// PREVENT INCORRECT MODIFICATIONS
// Data is a helping wrapper on std::map
// Don't add any non-static members
static_assert(sizeof(Data) == sizeof(std::map<std::string, Datum>));
// Nothing is virtual
static_assert(!std::is_polymorphic_v<Data>);

/**
 * Parceling of Datum by recursive descent to a ByteString
 *
 * Parceling Format:
 * All values are native endian order.
 *
 * Datum = {
 *           (type_size_t)  Type (the type index from type_as_value<T>.)
 *           (datum_size_t) Size (size of Payload)
 *           (byte string)  Payload<Type>
 *         }
 *
 * Payload<Primitive_Type> = { bytes in native endian order }
 *
 * Payload<String> = { (index_size_t) number of elements (not including zero termination)
 *                     bytes of string data.
 *                   }
 *
 * Vector, Map, Container types:
 * Payload<Type> = { (index_size_t) number of elements
 *                   (byte string)  Payload<Element_Type> * number
 *                 }
 *
 * Pair container types:
 * Payload<Type> = { (byte string) Payload<first>,
 *                   (byte string) Payload<second>
 *                 }
 *
 * Note: Data is a std::map<std::string, Datum>
 *
 * Design notes:
 *
 * 1) The size of each datum allows skipping of unknown types for compatibility
 * of older code with newer Datums.
 *
 * Examples:
 * Payload<Int32> of 123
 * [ value of 123                   ] =  0x7b 0x00 0x00 0x00       123
 *
 * Example of Payload<String> of std::string("hi"):
 * [ (index_size_t) length          ] = 0x02 0x00 0x00 0x00        2 strlen("hi")
 * [ raw bytes "hi"                 ] = 0x68 0x69                  "hi"
 *
 * Payload<Data>
 * [ (index_size_t) entries ]
 * [ raw bytes   (entry 1) Key   (Payload<String>)
 *                         Value (Datum)
 *                ...  (until #entries) ]
 *
 * Example of Payload<Data> of {{"hello", "world"},
 *                              {"value", (int32_t)1000}};
 * [ (index_size_t) #entries        ] = 0x02 0x00 0x00 0x00        2 entries
 *    Key (Payload<String>)
 *    [ index_size_t length         ] = 0x05 0x00 0x00 0x00        5 strlen("hello")
 *    [ raw bytes "hello"           ] = 0x68 0x65 0x6c 0x6c 0x6f   "hello"
 *    Value (Datum)
 *    [ (type_size_t) type          ] = 0x05 0x00 0x00 0x00        5 (TYPE_STRING)
 *    [ (datum_size_t) size         ] = 0x09 0x00 0x00 0x00        sizeof(index_size_t) +
 *                                                                 strlen("world")
 *       Payload<String>
 *       [ (index_size_t) length    ] = 0x05 0x00 0x00 0x00        5 strlen("world")
 *       [ raw bytes "world"        ] = 0x77 0x6f 0x72 0x6c 0x64   "world"
 *    Key (Payload<String>)
 *    [ index_size_t length         ] = 0x05 0x00 0x00 0x00        5 strlen("value")
 *    [ raw bytes "value"           ] = 0x76 0x61 0x6c 0x75 0x65   "value"
 *    Value (Datum)
 *    [ (type_size_t) type          ] = 0x01 0x00 0x00 0x00        1 (TYPE_INT32)
 *    [ (datum_size_t) size         ] = 0x04 0x00 0x00 0x00        4 sizeof(int32_t)
 *        Payload<Int32>
 *        [ raw bytes 1000          ] = 0xe8 0x03 0x00 0x00        1000
 *
 * Metadata is passed as a Payload<Data>.
 * An implementation dependent detail is that the Keys are always
 * stored sorted, so the byte string representation generated is unique.
 */

// Platform Apex compatibility note:
// type_size_t may not change.
using type_size_t = uint32_t;

// Platform Apex compatibility note:
// index_size_t must not change.
using index_size_t = uint32_t;

// Platform Apex compatibility note:
// datum_size_t must not change.
using datum_size_t = uint32_t;

// The particular implementation of ByteString may change
// without affecting compatibility.
using ByteString = std::basic_string<uint8_t>;

/*
    These should correspond to the Java AudioMetadata.java

    Permitted type indexes:

    TYPE_NONE = 0,
    TYPE_INT32 = 1,
    TYPE_INT64 = 2,
    TYPE_FLOAT = 3,
    TYPE_DOUBLE = 4,
    TYPE_STRING = 5,
    TYPE_DATA = 6,
*/

template <typename T>
inline constexpr type_size_t get_type_as_value() {
    return (type_size_t)(metadata_types::index_of<T>() + 1);
}

template <typename T>
inline constexpr type_size_t type_as_value = get_type_as_value<T>();

// forward decl for recursion - do not remove.
bool copyToByteString(const Datum& datum, ByteString &bs);

template <template <typename ...> class V, typename... Args>
bool copyToByteString(const V<Args...>& v, ByteString&bs);
// end forward decl

// primitives handled here
template <typename T>
std::enable_if_t<
    is_primitive_metadata_type_v<T> || std::is_arithmetic_v<std::decay_t<T>>,
    bool
    >
copyToByteString(const T& t, ByteString& bs) {
    bs.append((uint8_t*)&t, sizeof(t));
    return true;
}

// pairs handled here
template <typename A, typename B>
bool copyToByteString(const std::pair<A, B>& p, ByteString& bs) {
    return copyToByteString(p.first, bs) && copyToByteString(p.second, bs);
}

// containers
template <template <typename ...> class V, typename... Args>
bool copyToByteString(const V<Args...>& v, ByteString& bs) {
    if (v.size() > std::numeric_limits<index_size_t>::max()) return false;
    index_size_t size = v.size();
    if (!copyToByteString(size, bs)) return false;
    if constexpr (std::is_same_v<std::decay_t<V<Args...>>, std::string>) {
        bs.append((uint8_t*)v.c_str());
    }  else /* constexpr */ {
        for (const auto &d : v) { // handles std::vector and std::map
            if (!copyToByteString(d, bs)) return false;
        }
    }
    return true;
}

// simple struct data (use structured binding to extract members)
template <typename T>
std::enable_if_t<
    is_structural_metadata_type_v<T>,
    bool
    >
copyToByteString(const T& t, ByteString& bs) {
    using type = std::decay_t<T>;
    if constexpr (is_braces_constructible<type, any_type, any_type, any_type, any_type>{}) {
        const auto& [e1, e2, e3, e4] = t;
        return copyToByteString(e1, bs)
            && copyToByteString(e2, bs)
            && copyToByteString(e3, bs)
            && copyToByteString(e4, bs);
    } else if constexpr (is_braces_constructible<type, any_type, any_type, any_type>{}) {
        const auto& [e1, e2, e3] = t;
        return copyToByteString(e1, bs)
            && copyToByteString(e2, bs)
            && copyToByteString(e3, bs);
    } else if constexpr (is_braces_constructible<type, any_type, any_type>{}) {
        const auto& [e1, e2] = t;
        return copyToByteString(e1, bs)
            && copyToByteString(e2, bs);
    } else if constexpr(is_braces_constructible<type, any_type>{}) {
        const auto& [e1] = t;
        return copyToByteString(e1, bs);
    } else if constexpr (is_braces_constructible<type>{}) {
        return true; // like std::monostate - no members
    } else /* constexpr */ {
        static_assert(dependent_false_v<T>);
    }
}

// Datum
bool copyToByteString(const Datum& datum, ByteString &bs) {
    bool success = false;
    return metadata_types::apply([&bs, &success](auto ptr) {
             // save type
             const type_size_t type = type_as_value<decltype(*ptr)>;
             if (!copyToByteString(type, bs)) return;

             // get current location
             const size_t idx = bs.size();

             // save size (replaced later)
             datum_size_t datum_size = 0;
             if (!copyToByteString(datum_size, bs)) return;

             // copy data
             if (!copyToByteString(*ptr, bs)) return;

             // save correct size
             const size_t diff = bs.size() - idx - sizeof(datum_size);
             if (diff > std::numeric_limits<datum_size_t>::max()) return;
             datum_size = diff;
             bs.replace(idx, sizeof(datum_size), (uint8_t*)&datum_size, sizeof(datum_size));
             success = true;
         }, &datum) && success;
}

/**
 * Obtaining the Datum back from ByteString
 */

// A container that lists all the unknown types found during parsing.
using ByteStringUnknowns = std::vector<type_size_t>;

// forward decl for recursion - do not remove.
bool copyFromByteString(Datum *datum, const ByteString &bs, size_t& idx,
        ByteStringUnknowns *unknowns);

template <template <typename ...> class V, typename... Args>
bool copyFromByteString(V<Args...> *v, const ByteString& bs, size_t& idx,
        ByteStringUnknowns *unknowns);

// primitive
template <typename T>
std::enable_if_t<
        is_primitive_metadata_type_v<T> ||
        std::is_arithmetic_v<std::decay_t<T>>,
        bool
        >
copyFromByteString(T *dest, const ByteString& bs, size_t& idx,
        ByteStringUnknowns *unknowns __unused) {
    if (idx + sizeof(T) > bs.size()) return false;
    bs.copy((uint8_t*)dest, sizeof(T), idx);
    idx += sizeof(T);
    return true;
}

// pairs
template <typename A, typename B>
bool copyFromByteString(std::pair<A, B>* p, const ByteString& bs, size_t& idx,
        ByteStringUnknowns *unknowns) {
    return copyFromByteString(&p->first, bs, idx, unknowns)
            && copyFromByteString(&p->second, bs, idx, unknowns);
}

// containers
template <template <typename ...> class V, typename... Args>
bool copyFromByteString(V<Args...> *v, const ByteString& bs, size_t& idx,
        ByteStringUnknowns *unknowns) {
    index_size_t size;
    if (!copyFromByteString(&size, bs, idx, unknowns)) return false;

    if constexpr (std::is_same_v<std::decay_t<V<Args...>>, std::string>) {
        if (size > bs.size() - idx) return false;
        v->resize(size);
        for (index_size_t i = 0; i < size; ++i) {
            (*v)[i] = bs[idx++];
        }
    } else if constexpr (is_specialization_v<std::decay_t<V<Args...>>, std::vector>) {
        for (index_size_t i = 0; i < size; ++i) {
            std::decay_t<decltype(*v->begin())> value{};
            if (!copyFromByteString(&value, bs, idx, unknowns)) {
                return false;
            }
            if constexpr (std::is_same_v<std::decay_t<decltype(value)>, Datum>) {
                if (!value.has_value()) {
                    continue;  // ignore empty datum values in a vector.
                }
            }
            v->emplace_back(std::move(value));
        }
    } else if constexpr (is_specialization_v<std::decay_t<V<Args...>>, std::map>) {
        for (index_size_t i = 0; i < size; ++i) {
            // we can't directly use pair because there may be internal const decls.
            std::decay_t<decltype(v->begin()->first)> key{};
            std::decay_t<decltype(v->begin()->second)> value{};
            if (!copyFromByteString(&key, bs, idx, unknowns) ||
                    !copyFromByteString(&value, bs, idx, unknowns)) {
                return false;
            }
            if constexpr (std::is_same_v<std::decay_t<decltype(value)>, Datum>) {
                if (!value.has_value()) {
                    continue;  // ignore empty datum values in a map.
                }
            }
            v->emplace(std::move(key), std::move(value));
        }
    } else /* constexpr */ {
        for (index_size_t i = 0; i < size; ++i) {
            std::decay_t<decltype(*v->begin())> value{};
            if (!copyFromByteString(&value, bs, idx, unknowns)) {
                return false;
            }
            v->emplace(std::move(value));
        }
    }
    return true;
}

// simple structs (use structured binding to extract members)
template <typename T>
typename std::enable_if_t<is_structural_metadata_type_v<T>, bool>
copyFromByteString(T *t, const ByteString& bs, size_t& idx,
        ByteStringUnknowns *unknowns) {
    using type = std::decay_t<T>;
    if constexpr (is_braces_constructible<type, any_type, any_type, any_type, any_type>{}) {
        auto& [e1, e2, e3, e4] =  *t;
        return copyFromByteString(&e1, bs, idx, unknowns)
            && copyFromByteString(&e2, bs, idx, unknowns)
            && copyFromByteString(&e3, bs, idx, unknowns)
            && copyFromByteString(&e4, bs, idx, unknowns);
    } else if constexpr (is_braces_constructible<type, any_type, any_type, any_type>{}) {
        auto& [e1, e2, e3] =  *t;
        return copyFromByteString(&e1, bs, idx, unknowns)
            && copyFromByteString(&e2, bs, idx, unknowns)
            && copyFromByteString(&e3, bs, idx, unknowns);
    } else if constexpr (is_braces_constructible<type, any_type, any_type>{}) {
        auto& [e1, e2] =  *t;
        return copyFromByteString(&e1, bs, idx, unknowns)
            && copyFromByteString(&e2, bs, idx, unknowns);
    } else if constexpr (is_braces_constructible<type, any_type>{}) {
        auto& [e1] =  *t;
        return copyFromByteString(&e1, bs, idx, unknowns);
    } else if constexpr (is_braces_constructible<type>{}) {
        return true; // like std::monostate - no members
    } else /* constexpr */ {
        static_assert(dependent_false_v<T>);
    }
}

namespace tedious_details {
//
// We build a function table at compile time to lookup the proper copyFromByteString method.
// See:
// https://stackoverflow.com/questions/36785345/void-to-the-nth-element-of-stdtuple-at-runtime
// Constant time implementation of std::visit (variant)

template <typename CompoundT, size_t Index>
bool copyFromByteString(Datum *datum, const ByteString &bs, size_t &idx, size_t endIdx,
        ByteStringUnknowns *unknowns) {
   using T = std::tuple_element_t<Index, typename CompoundT::tuple_t>;
   T value;
   if (!android::audio_utils::metadata::copyFromByteString(
           &value, bs, idx, unknowns)) return false;  // have we parsed correctly?
   if (idx != endIdx) return false;  // have we consumed the correct number of bytes?
   *datum = std::move(value);
   return true;
}

template <typename CompoundT, size_t... Indexes>
constexpr bool copyFromByteString(Datum *datum, const ByteString &bs,
        size_t &idx, size_t endIdx, ByteStringUnknowns *unknowns,
        size_t typeIndex, std::index_sequence<Indexes...>)
{
    using function_type =
            bool (*)(Datum*, const ByteString&, size_t&, size_t, ByteStringUnknowns*);
    function_type constexpr ptrs[] = {
        &copyFromByteString<CompoundT, Indexes>...
    };
    return ptrs[typeIndex](datum, bs, idx, endIdx, unknowns);
}

template <typename CompoundT>
__attribute__((noinline))
constexpr bool copyFromByteString(Datum *datum, const ByteString &bs,
        size_t &idx, size_t endIdx, ByteStringUnknowns *unknowns, size_t typeIndex) {
  return copyFromByteString<CompoundT>(
          datum, bs, idx, endIdx, unknowns,
          typeIndex, std::make_index_sequence<CompoundT::size_v>());
}

} // namespace tedious_details

bool copyFromByteString(Datum *datum, const ByteString &bs, size_t& idx,
        ByteStringUnknowns *unknowns) {
    type_size_t type;
    if (!copyFromByteString(&type, bs, idx, unknowns)) return false;

    datum_size_t datum_size;
    if (!copyFromByteString(&datum_size, bs, idx, unknowns)) return false;
    if (datum_size > bs.size() - idx) return false;
    const size_t endIdx = idx + datum_size;

    if (type == 0 || type > metadata_types::size_v) {
        idx = endIdx; // skip unrecognized type.
        if (unknowns != nullptr) {
            unknowns->push_back(type);
            return true;  // allow further recursion.
        }
        return false;
    }

    // use special trick to instantiate all the types for copyFromByteString
    // in a table and find the right method from table lookup.
    return tedious_details::copyFromByteString<metadata_types>(
            datum, bs, idx, endIdx, unknowns, type - 1);
}

// Handy helpers - these are the most efficient ways to parcel Data.
/**
 * Returns the Data map from a byte string.
 *
 * If unknowns is nullptr, then any unknown entries during parsing will cause
 * an empty map to be returned.
 *
 * If unknowns is non-null, then it contains all of the unknown types
 * encountered during parsing, and a partial map will be returned excluding all
 * unknown types encountered.
 */
Data dataFromByteString(const ByteString &bs,
        ByteStringUnknowns *unknowns = nullptr) {
    Data d;
    size_t idx = 0;
    if (!copyFromByteString(&d, bs, idx, unknowns)) {
        return {};
    }
    return d; // copy elision
}

ByteString byteStringFromData(const Data &data) {
    ByteString bs;
    copyToByteString(data, bs);
    return bs; // copy elision
}

} // namespace android::audio_utils::metadata

#endif // __cplusplus

// C API (see C++ API above for details)

/** \cond */
__BEGIN_DECLS
/** \endcond */

typedef struct audio_metadata_t audio_metadata_t;

/**
 * \brief Creates a metadata object
 *
 * \return the metadata object or NULL on failure. Caller must call
 *         audio_metadata_destroy to free memory.
 */
audio_metadata_t *audio_metadata_create();

/**
 * \brief Put key value pair where the value type is int32_t to audio metadata.
 *
 * \param metadata         the audio metadata object.
 * \param key              the key of the element to be put.
 * \param value            the value of the element to be put.
 * \return 0 if the key value pair is put successfully into the audio metadata.
 *         -EINVAL if metadata or key is null.
 */
int audio_metadata_put_int32(audio_metadata_t *metadata, const char *key, int32_t value);

/**
 * \brief Put key value pair where the value type is int64_t to audio metadata.
 *
 * \param metadata         the audio metadata object.
 * \param key              the key of the element to be put.
 * \param value            the value of the element to be put.
 * \return 0 if the key value pair is put successfully into the audio metadata.
 *         -EINVAL if metadata or key is null.
 */
int audio_metadata_put_int64(audio_metadata_t *metadata, const char *key, int64_t value);

/**
 * \brief Put key value pair where the value type is float to audio metadata.
 *
 * \param metadata         the audio metadata object.
 * \param key              the key of the element to be put.
 * \param value            the value of the element to be put.
 * \return 0 if the key value pair is put successfully into the audio metadata.
 *         -EINVAL if metadata or key is null.
 */
int audio_metadata_put_float(audio_metadata_t *metadata, const char *key, float value);

/**
 * \brief Put key value pair where the value type is double to audio metadata.
 *
 * \param metadata         the audio metadata object.
 * \param key              the key of the element to be put.
 * \param value            the value of the element to be put.
 * \return 0 if the key value pair is put successfully into the audio metadata.
 *         -EINVAL if metadata or key is null.
 */
int audio_metadata_put_double(audio_metadata_t *metadata, const char *key, double value);

/**
 * \brief Put key value pair where the value type is `const char *` to audio metadata.
 *
 * \param metadata         the audio metadata object.
 * \param key              the key of the element to be put.
 * \param value            the value of the element to be put.
 * \return 0 if the key value pair is put successfully into the audio metadata.
 *         -EINVAL if metadata, key or value is null.
 */
int audio_metadata_put_string(audio_metadata_t *metadata, const char *key, const char *value);

/**
 * \brief Put key value pair where the value type is audio_metadata_t to audio metadata.
 *
 * \param metadata         the audio metadata object.
 * \param key              the key of the element to be put.
 * \param value            the value of the element to be put.
 * \return 0 if the key value pair is put successfully into the audio metadata.
 *         -EINVAL if metadata, key or value is null.
 */
int audio_metadata_put_data(audio_metadata_t *metadata, const char *key, audio_metadata_t *value);

/**
 * \brief The type is not allowed in audio metadata. Only log the key and return -EINVAL here.
 */
int audio_metadata_put_unknown(audio_metadata_t *metadata, const char *key, const void *value);

// use C Generics to provide interfaces for put/get functions
// See: https://en.cppreference.com/w/c/language/generic

/**
 * A generic interface to put key value pair into the audio metadata.
 */
#define audio_metadata_put(metadata, key, value) _Generic((value), \
    int32_t: audio_metadata_put_int32,                             \
    int64_t: audio_metadata_put_int64,                             \
    float: audio_metadata_put_float,                               \
    double: audio_metadata_put_double,                             \
    const char*: audio_metadata_put_string,                        \
    audio_metadata_t*: audio_metadata_put_data,                    \
    default: audio_metadata_put_unknown                            \
    )(metadata, key, value)

/**
 * \brief Get mapped value whose type is int32_t by a given key from audio metadata.
 *
 * \param metadata         the audio metadata object.
 * \param key              the key value to get value.
 * \param value            the mapped value to be written.
 * \return -EINVAL when 1) metadata is null, 2) key is null, or 3) value is null.
 *         -ENOENT when 1) key is found in the audio metadata,
 *                      2) the type of mapped value is not int32_t.
 *         0 if successfully find the mapped value.
 */
int audio_metadata_get_int32(audio_metadata_t *metadata, const char *key, int32_t *value);

/**
 * \brief Get mapped value whose type is int64_t by a given key from audio metadata.
 *
 * \param metadata         the audio metadata object.
 * \param key              the key value to get value.
 * \param value            the mapped value to be written.
 * \return -EINVAL when 1) metadata is null, 2) key is null, or 3) value is null.
 *         -ENOENT when 1) key is found in the audio metadata,
 *                      2) the type of mapped value is not int32_t.
 *         0 if successfully find the mapped value.
 */
int audio_metadata_get_int64(audio_metadata_t *metadata, const char *key, int64_t *value);

/**
 * \brief Get mapped value whose type is float by a given key from audio metadata.
 *
 * \param metadata         the audio metadata object.
 * \param key              the key value to get value.
 * \param value            the mapped value to be written.
 * \return -EINVAL when 1) metadata is null, 2) key is null, or 3) value is null.
 *         -ENOENT when 1) key is found in the audio metadata,
 *                      2) the type of mapped value is not float.
 *         0 if successfully find the mapped value.
 */
int audio_metadata_get_float(audio_metadata_t *metadata, const char *key, float *value);

/**
 * \brief Get mapped value whose type is double by a given key from audio metadata.
 *
 * \param metadata         the audio metadata object.
 * \param key              the key value to get value.
 * \param value            the mapped value to be written.
 * \return -EINVAL when 1) metadata is null, 2) key is null, or 3) value is null.
 *         -ENOENT when 1) key is found in the audio metadata,
 *                      2) the type of mapped value is not double.
 *         0 if successfully find the mapped value.
 */
int audio_metadata_get_double(audio_metadata_t *metadata, const char *key, double *value);

/**
 * \brief Get mapped value whose type is std::string by a given key from audio metadata.
 *
 * \param metadata         the audio metadata object.
 * \param key              the key value to get value.
 * \param value            the mapped value to be written. The memory will be allocated in the
 *                         function, which must be freed by caller.
 * \return -EINVAL when 1) metadata is null, 2) key is null, or 3) value is null.
 *         -ENOENT when 1) key is found in the audio metadata,
 *                      2) the type of mapped value is not std::string.
 *         -ENOMEM when fails allocating memory for value.
 *         0 if successfully find the mapped value.
 */
int audio_metadata_get_string(audio_metadata_t *metadata, const char *key, char **value);

/**
 * \brief Get mapped value whose type is audio_metadata_t by a given key from audio metadata.
 *
 * \param metadata         the audio metadata object.
 * \param key              the key value to get value.
 * \param value            the mapped value to be written. The memory will be allocated in the
 *                         function, which should be free by caller via audio_metadata_destroy.
 * \return -EINVAL when 1) metadata is null, 2) key is null, or 3) value is null.
 *         -ENOENT when 1) key is found in the audio metadata,
 *                      2) the type of mapped value is not audio_utils::metadata::Data.
 *         -ENOMEM when fails allocating memory for value.
 *         0 if successfully find the mapped value.
 */
int audio_metadata_get_data(audio_metadata_t *metadata, const char *key, audio_metadata_t **value);

/**
 * \brief The data type is not allowed in audio metadata. Only log the key and return -EINVAL here.
 */
int audio_metadata_get_unknown(audio_metadata_t *metadata, const char *key, void *value);

/**
 * A generic interface to get mapped value by a given key from audio metadata. The value object
 * will remain the same if the key is not found in the audio metadata.
 */
#define audio_metadata_get(metadata, key, value) _Generic((value), \
    int32_t*: audio_metadata_get_int32,                            \
    int64_t*: audio_metadata_get_int64,                            \
    float*: audio_metadata_get_float,                              \
    double*: audio_metadata_get_double,                            \
    char**: audio_metadata_get_string,                             \
    audio_metadata_t**: audio_metadata_get_data,                   \
    default: audio_metadata_get_unknown                            \
    )(metadata, key, value)

/**
 * \brief Remove item from audio metadata.
 *
 * \param metadata         the audio metadata object.
 * \param key              the key of the item that is going to be removed.
 * \return -EINVAL if metadata or key is null. Otherwise, return the number of elements erased.
 */
ssize_t audio_metadata_erase(audio_metadata_t *metadata, const char *key);

/**
 * \brief Destroys the metadata object
 *
 * \param metadata         object returned by create, if NULL nothing happens.
 */
void audio_metadata_destroy(audio_metadata_t *metadata);

/**
 * \brief Unpack byte string into a given audio metadata
 *
 * \param byteString       a byte string that contains data to convert to audio metadata.
 * \param length           the length of the byte string
 * \return the audio metadata object that contains the converted data. Caller must call
 *         audio_metadata_destroy to free the memory.
 */
audio_metadata_t *audio_metadata_from_byte_string(const uint8_t *byteString, size_t length);

/**
 * \brief Pack the audio metadata into a byte string
 *
 * \param metadata         the audio metadata object to be converted.
 * \param byteString       the buffer to write data to. The memory will be allocated
 *                         in the function, which must be freed by caller via free().
 * \return -EINVAL if metadata or byteString is null.
 *         -ENOMEM if fails to allocate memory for byte string.
 *         The length of the byte string.
 */
ssize_t byte_string_from_audio_metadata(audio_metadata_t *metadata, uint8_t **byteString);

/** \cond */
__END_DECLS
/** \endcond */

#endif // !ANDROID_AUDIO_METADATA_H
