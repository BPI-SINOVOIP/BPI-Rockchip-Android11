// This code is compiled directly on many platforms, including client
// platforms like Windows, Mac, and embedded systems.  Before making
// any changes here, make sure that you're not breaking any platforms.
//
// MOE:begin_strip
// This file is open source. You may export it with your open source projects
// as long as you use MOE to strip proprietary comments.
// MOE:end_strip

#ifndef DYNAMIC_DEPTH_INTERNAL_BASE_INTEGRAL_TYPES_H_  // NOLINT
#define DYNAMIC_DEPTH_INTERNAL_BASE_INTEGRAL_TYPES_H_  // NOLINT

// These typedefs are also defined in base/swig/google.swig. In the
// SWIG environment, we use those definitions and avoid duplicate
// definitions here with an ifdef. The definitions should be the
// same in both files, and ideally be only defined in this file.
#ifndef SWIG  // NOLINT
// Standard typedefs
// MOE:begin_strip
// All Google2 code is compiled with -funsigned-char to make "char"
// unsigned.  Google2 code therefore doesn't need a "uchar" type.
// MOE:end_strip
// Signed integer types with width of exactly 8, 16, 32, or 64 bits
// respectively, for use when exact sizes are required.
typedef signed char schar;
typedef signed char int8;
typedef short int16;  // NOLINT
typedef int int32;
#ifdef COMPILER_MSVC
typedef __int64 int64;
#else
typedef long long int64;  // NOLINT
#endif /* COMPILER_MSVC */

// NOTE: unsigned types are DANGEROUS in loops and other arithmetical
// places.  Use the signed types unless your variable represents a bit
// pattern (eg a hash value) or you really need the extra bit.  Do NOT
// use 'unsigned' to express "this value should always be positive";
// use assertions for this.

// Unsigned integer types with width of exactly 8, 16, 32, or 64 bits
// respectively, for use when exact sizes are required.
typedef unsigned char uint8;
typedef unsigned short uint16;  // NOLINT
typedef unsigned int uint32;
#ifdef COMPILER_MSVC
typedef unsigned __int64 uint64;
#else
typedef unsigned long long uint64;
#endif /* COMPILER_MSVC */

// A type to represent a Unicode code-point value. As of Unicode 4.0,
// such values require up to 21 bits.
// (For type-checking on pointers, make this explicitly signed,
// and it should always be the signed version of whatever int32 is.)
typedef signed int char32;

//  A type to represent a natural machine word (for e.g. efficiently
// scanning through memory for checksums or index searching). Don't use
// this for storing normal integers. Ideally this would be just
// unsigned int, but our 64-bit architectures use the LP64 model
// (http://en.wikipedia.org/wiki/64-bit_computing#64-bit_data_models), hence
// their ints are only 32 bits. We want to use the same fundamental
// type on all archs if possible to preserve *printf() compatibility.
typedef unsigned long uword_t;  // NOLINT

#endif /* SWIG */

// long long macros to be used because gcc and vc++ use different suffixes,
// and different size specifiers in format strings
#undef GG_LONGLONG
#undef GG_ULONGLONG
#undef GG_LL_FORMAT

#ifdef COMPILER_MSVC /* if Visual C++ */

// VC++ long long suffixes
#define GG_LONGLONG(x) x##I64
#define GG_ULONGLONG(x) x##UI64

// Length modifier in printf format string for int64's (e.g. within %d)
#define GG_LL_FORMAT "I64"  // As in printf("%I64d", ...)
#define GG_LL_FORMAT_W L"I64"

#else /* not Visual C++ */

#define GG_LONGLONG(x) x##LL
#define GG_ULONGLONG(x) x##ULL
#define GG_LL_FORMAT "ll"  // As in "%lld". Note that "q" is poor form also.
#define GG_LL_FORMAT_W L"ll"

#endif  // COMPILER_MSVC

static const uint8 kuint8max = ((uint8)0xFF);
static const uint16 kuint16max = ((uint16)0xFFFF);
static const uint32 kuint32max = ((uint32)0xFFFFFFFF);
static const uint64 kuint64max = ((uint64)GG_LONGLONG(0xFFFFFFFFFFFFFFFF));
static const int8 kint8min = ((int8)~0x7F);
static const int8 kint8max = ((int8)0x7F);
static const int16 kint16min = ((int16)~0x7FFF);
static const int16 kint16max = ((int16)0x7FFF);
static const int32 kint32min = ((int32)~0x7FFFFFFF);
static const int32 kint32max = ((int32)0x7FFFFFFF);
static const int64 kint64min = ((int64)GG_LONGLONG(~0x7FFFFFFFFFFFFFFF));
static const int64 kint64max = ((int64)GG_LONGLONG(0x7FFFFFFFFFFFFFFF));

// TODO(jyrki): remove this eventually.
// No object has kIllegalFprint as its Fingerprint.
typedef uint64 Fprint;
static const Fprint kIllegalFprint = 0;
static const Fprint kMaxFprint = GG_ULONGLONG(0xFFFFFFFFFFFFFFFF);

#endif  // DYNAMIC_DEPTH_INTERNAL_BASE_INTEGRAL_TYPES_H_  // NOLINT
