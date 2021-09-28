#include "strings/case.h"

#include "strings/ascii_ctype.h"

namespace dynamic_depth {

void LowerString(string* s) {
  string::iterator end = s->end();
  for (string::iterator i = s->begin(); i != end; ++i) {
    *i = ascii_tolower(*i);
  }
}

}  // namespace dynamic_depth
