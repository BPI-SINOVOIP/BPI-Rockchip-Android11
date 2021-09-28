// This file contains string processing functions related to
// uppercase, lowercase, etc.
//
// These functions are for ASCII only. If you need to process UTF8 strings,
// take a look at files in i18n/utf8.

#ifndef DYNAMIC_DEPTH_INTERNAL_STRINGS_CASE_H_  // NOLINT
#define DYNAMIC_DEPTH_INTERNAL_STRINGS_CASE_H_  // NOLINT

#include <string>

#include "base/port.h"

namespace dynamic_depth {

// Returns true if the two strings are equal, case-insensitively speaking.
// Uses C/POSIX locale.
inline bool StringCaseEqual(const string& s1, const string& s2) {
  return strcasecmp(s1.c_str(), s2.c_str()) == 0;
}

// ----------------------------------------------------------------------
// LowerString()
// LowerStringToBuf()
//    Convert the characters in "s" to lowercase.
//    Works only with ASCII strings; for UTF8, see ToLower in
//    util/utf8/public/unilib.h
//    Changes contents of "s".  LowerStringToBuf copies at most
//    "n" characters (including the terminating '\0')  from "s"
//    to another buffer.
// ----------------------------------------------------------------------
void LowerString(string* s);

namespace strings {
inline string ToLower(const string& s) {
  string out(s);
  LowerString(&out);
  return out;
}

}  // namespace strings
}  // namespace dynamic_depth

#endif  // DYNAMIC_DEPTH_INTERNAL_STRINGS_CASE_H_  // NOLINT
