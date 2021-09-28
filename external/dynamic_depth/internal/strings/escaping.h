// This is a grab-bag file for string utilities involved in escaping and
// unescaping strings in various ways. Who knew there were so many?
//
// There are more escaping functions in:
//   webutil/html/tagutils.h (Escaping strings for HTML, PRE, JavaScript, etc.)
//   webutil/url/url.h (Escaping for URL's, both RFC-2396 and other methods)
//   template/template_modifiers.h (All sorts of stuff)
//   util/regexp/re2/re2.h (Escaping for literals within regular expressions
//                         - see RE2::QuoteMeta).
// And probably many more places, as well.

#ifndef DYNAMIC_DEPTH_INTERNAL_STRINGS_ESCAPING_H_  // NOLINT
#define DYNAMIC_DEPTH_INTERNAL_STRINGS_ESCAPING_H_  // NOLINT

#include <stddef.h>  // For ptrdiff_t.

#include <string>

#include "base/port.h"

namespace dynamic_depth {

// ----------------------------------------------------------------------
// Base64Unescape()
//    Converts "src" which is encoded in Base64 to its binary equivalent and
//    writes it to "dest". If src contains invalid characters, dest is cleared
//    and the function returns false. Returns true on success.
// ----------------------------------------------------------------------
bool Base64Unescape(const string& src, string* dest);

// ----------------------------------------------------------------------
// WebSafeBase64Unescape()
//    This is a variation of Base64Unescape which uses '-' instead of '+', and
//    '_' instead of '/'. src is not null terminated, instead specify len. I
//    recommend that slen<szdest, but we honor szdest anyway.
//    RETURNS the length of dest, or -1 if there's an error.

//    The variation that stores into a string clears the string first, and
//    returns false (with dest empty) if src contains invalid chars; for
//    this version src and dest must be different strings.
// ----------------------------------------------------------------------
bool WebSafeBase64Unescape(const string& src, string* dest);

// ----------------------------------------------------------------------
// Base64Escape()
//    Encode "src" to "dest" using base64 encoding.
//    src is not null terminated, instead specify len.
//    'dest' should have at least CalculateBase64EscapedLen() length.
//    RETURNS the length of dest.
//    It also has an extra parameter "do_padding",
//    which when set to false will prevent padding with "=".
// ----------------------------------------------------------------------
void Base64Escape(const unsigned char* src, ptrdiff_t szsrc, string* dest,
                  bool do_padding);

// ----------------------------------------------------------------------
// b2a_hex()
//  Description: Binary-to-Ascii hex conversion.  This converts
//   'num' bytes of binary to a 2*'num'-character hexadecimal representation
//    Return value: 2*'num' characters of ascii string
// ----------------------------------------------------------------------
string b2a_hex(const char* from, ptrdiff_t num);

}  // namespace dynamic_depth

#endif  // DYNAMIC_DEPTH_INTERNAL_STRINGS_ESCAPING_H_  // NOLINT
