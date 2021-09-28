#include "strings/escaping.h"

#include <cassert>

#include "android-base/logging.h"
#include "strings/ascii_ctype.h"

namespace dynamic_depth {

// ----------------------------------------------------------------------
// ptrdiff_t Base64Unescape() - base64 decoder
// ptrdiff_t Base64Escape() - base64 encoder
// ptrdiff_t WebSafeBase64Unescape() - Google's variation of base64 decoder
// ptrdiff_t WebSafeBase64Escape() - Google's variation of base64 encoder
//
// Check out
// http://tools.ietf.org/html/rfc2045 for formal description, but what we
// care about is that...
//   Take the encoded stuff in groups of 4 characters and turn each
//   character into a code 0 to 63 thus:
//           A-Z map to 0 to 25
//           a-z map to 26 to 51
//           0-9 map to 52 to 61
//           +(- for WebSafe) maps to 62
//           /(_ for WebSafe) maps to 63
//   There will be four numbers, all less than 64 which can be represented
//   by a 6 digit binary number (aaaaaa, bbbbbb, cccccc, dddddd respectively).
//   Arrange the 6 digit binary numbers into three bytes as such:
//   aaaaaabb bbbbcccc ccdddddd
//   Equals signs (one or two) are used at the end of the encoded block to
//   indicate that the text was not an integer multiple of three bytes long.
// ----------------------------------------------------------------------

bool Base64UnescapeInternal(const char* src_param, size_t szsrc, char* dest,
                            size_t szdest, const signed char* unbase64,
                            size_t* len) {
  static const char kPad64Equals = '=';
  static const char kPad64Dot = '.';

  size_t destidx = 0;
  int decode = 0;
  int state = 0;
  unsigned int ch = 0;
  unsigned int temp = 0;

  // If "char" is signed by default, using *src as an array index results in
  // accessing negative array elements. Treat the input as a pointer to
  // unsigned char to avoid this.
  const unsigned char* src = reinterpret_cast<const unsigned char*>(src_param);

  // The GET_INPUT macro gets the next input character, skipping
  // over any whitespace, and stopping when we reach the end of the
  // string or when we read any non-data character.  The arguments are
  // an arbitrary identifier (used as a label for goto) and the number
  // of data bytes that must remain in the input to avoid aborting the
  // loop.
#define GET_INPUT(label, remain)                          \
  label:                                                  \
  --szsrc;                                                \
  ch = *src++;                                            \
  decode = unbase64[ch];                                  \
  if (decode < 0) {                                       \
    if (ascii_isspace(ch) && szsrc >= remain) goto label; \
    state = 4 - remain;                                   \
    break;                                                \
  }

  // if dest is null, we're just checking to see if it's legal input
  // rather than producing output.  (I suspect this could just be done
  // with a regexp...).  We duplicate the loop so this test can be
  // outside it instead of in every iteration.

  if (dest) {
    // This loop consumes 4 input bytes and produces 3 output bytes
    // per iteration.  We can't know at the start that there is enough
    // data left in the string for a full iteration, so the loop may
    // break out in the middle; if so 'state' will be set to the
    // number of input bytes read.

    while (szsrc >= 4) {
      // We'll start by optimistically assuming that the next four
      // bytes of the string (src[0..3]) are four good data bytes
      // (that is, no nulls, whitespace, padding chars, or illegal
      // chars).  We need to test src[0..2] for nulls individually
      // before constructing temp to preserve the property that we
      // never read past a null in the string (no matter how long
      // szsrc claims the string is).

      if (!src[0] || !src[1] || !src[2] ||
          ((temp = ((unsigned(unbase64[src[0]]) << 18) |
                    (unsigned(unbase64[src[1]]) << 12) |
                    (unsigned(unbase64[src[2]]) << 6) |
                    (unsigned(unbase64[src[3]])))) &
           0x80000000)) {
        // Iff any of those four characters was bad (null, illegal,
        // whitespace, padding), then temp's high bit will be set
        // (because unbase64[] is -1 for all bad characters).
        //
        // We'll back up and resort to the slower decoder, which knows
        // how to handle those cases.

        GET_INPUT(first, 4);
        temp = decode;
        GET_INPUT(second, 3);
        temp = (temp << 6) | decode;
        GET_INPUT(third, 2);
        temp = (temp << 6) | decode;
        GET_INPUT(fourth, 1);
        temp = (temp << 6) | decode;
      } else {
        // We really did have four good data bytes, so advance four
        // characters in the string.

        szsrc -= 4;
        src += 4;
        decode = -1;
        ch = '\0';
      }

      // temp has 24 bits of input, so write that out as three bytes.

      if (destidx + 3 > szdest) return false;
      dest[destidx + 2] = temp;
      temp >>= 8;
      dest[destidx + 1] = temp;
      temp >>= 8;
      dest[destidx] = temp;
      destidx += 3;
    }
  } else {
    while (szsrc >= 4) {
      if (!src[0] || !src[1] || !src[2] ||
          ((temp = ((unsigned(unbase64[src[0]]) << 18) |
                    (unsigned(unbase64[src[1]]) << 12) |
                    (unsigned(unbase64[src[2]]) << 6) |
                    (unsigned(unbase64[src[3]])))) &
           0x80000000)) {
        GET_INPUT(first_no_dest, 4);
        GET_INPUT(second_no_dest, 3);
        GET_INPUT(third_no_dest, 2);
        GET_INPUT(fourth_no_dest, 1);
      } else {
        szsrc -= 4;
        src += 4;
        decode = -1;
        ch = '\0';
      }
      destidx += 3;
    }
  }

#undef GET_INPUT

  // if the loop terminated because we read a bad character, return
  // now.
  if (decode < 0 && ch != '\0' && ch != kPad64Equals && ch != kPad64Dot &&
      !ascii_isspace(ch))
    return false;

  if (ch == kPad64Equals || ch == kPad64Dot) {
    // if we stopped by hitting an '=' or '.', un-read that character -- we'll
    // look at it again when we count to check for the proper number of
    // equals signs at the end.
    ++szsrc;
    --src;
  } else {
    // This loop consumes 1 input byte per iteration.  It's used to
    // clean up the 0-3 input bytes remaining when the first, faster
    // loop finishes.  'temp' contains the data from 'state' input
    // characters read by the first loop.
    while (szsrc > 0) {
      --szsrc;
      ch = *src++;
      decode = unbase64[ch];
      if (decode < 0) {
        if (ascii_isspace(ch)) {
          continue;
        } else if (ch == '\0') {
          break;
        } else if (ch == kPad64Equals || ch == kPad64Dot) {
          // back up one character; we'll read it again when we check
          // for the correct number of pad characters at the end.
          ++szsrc;
          --src;
          break;
        } else {
          return false;
        }
      }

      // Each input character gives us six bits of output.
      temp = (temp << 6) | decode;
      ++state;
      if (state == 4) {
        // If we've accumulated 24 bits of output, write that out as
        // three bytes.
        if (dest) {
          if (destidx + 3 > szdest) return false;
          dest[destidx + 2] = temp;
          temp >>= 8;
          dest[destidx + 1] = temp;
          temp >>= 8;
          dest[destidx] = temp;
        }
        destidx += 3;
        state = 0;
        temp = 0;
      }
    }
  }

  // Process the leftover data contained in 'temp' at the end of the input.
  int expected_equals = 0;
  switch (state) {
    case 0:
      // Nothing left over; output is a multiple of 3 bytes.
      break;

    case 1:
      // Bad input; we have 6 bits left over.
      return false;

    case 2:
      // Produce one more output byte from the 12 input bits we have left.
      if (dest) {
        if (destidx + 1 > szdest) return false;
        temp >>= 4;
        dest[destidx] = temp;
      }
      ++destidx;
      expected_equals = 2;
      break;

    case 3:
      // Produce two more output bytes from the 18 input bits we have left.
      if (dest) {
        if (destidx + 2 > szdest) return false;
        temp >>= 2;
        dest[destidx + 1] = temp;
        temp >>= 8;
        dest[destidx] = temp;
      }
      destidx += 2;
      expected_equals = 1;
      break;

    default:
      // state should have no other values at this point.
      LOG(FATAL) << "This can't happen; base64 decoder state = " << state;
  }

  // The remainder of the string should be all whitespace, mixed with
  // exactly 0 equals signs, or exactly 'expected_equals' equals
  // signs.  (Always accepting 0 equals signs is a google extension
  // not covered in the RFC, as is accepting dot as the pad character.)

  int equals = 0;
  while (szsrc > 0 && *src) {
    if (*src == kPad64Equals || *src == kPad64Dot)
      ++equals;
    else if (!ascii_isspace(*src))
      return false;
    --szsrc;
    ++src;
  }

  const bool ok = (equals == 0 || equals == expected_equals);
  if (ok) *len = destidx;
  return ok;
}

// The arrays below were generated by the following code
// #include <sys/time.h>
// #include <stdlib.h>
// #include <string.h>
// main()
// {
//   static const char Base64[] =
//     "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
//   char* pos;
//   int idx, i, j;
//   printf("    ");
//   for (i = 0; i < 255; i += 8) {
//     for (j = i; j < i + 8; j++) {
//       pos = strchr(Base64, j);
//       if ((pos == NULL) || (j == 0))
//         idx = -1;
//       else
//         idx = pos - Base64;
//       if (idx == -1)
//         printf(" %2d,     ", idx);
//       else
//         printf(" %2d/*%c*/,", idx, j);
//     }
//     printf("\n    ");
//   }
// }
//
// where the value of "Base64[]" was replaced by one of the base-64 conversion
// tables from the functions below.
static const signed char kUnBase64[] = {
    -1,       -1,       -1,       -1,       -1,       -1,        -1,
    -1,       -1,       -1,       -1,       -1,       -1,        -1,
    -1,       -1,       -1,       -1,       -1,       -1,        -1,
    -1,       -1,       -1,       -1,       -1,       -1,        -1,
    -1,       -1,       -1,       -1,       -1,       -1,        -1,
    -1,       -1,       -1,       -1,       -1,       -1,        -1,
    -1,       62 /*+*/, -1,       -1,       -1,       63 /*/ */, 52 /*0*/,
    53 /*1*/, 54 /*2*/, 55 /*3*/, 56 /*4*/, 57 /*5*/, 58 /*6*/,  59 /*7*/,
    60 /*8*/, 61 /*9*/, -1,       -1,       -1,       -1,        -1,
    -1,       -1,       0 /*A*/,  1 /*B*/,  2 /*C*/,  3 /*D*/,   4 /*E*/,
    5 /*F*/,  6 /*G*/,  07 /*H*/, 8 /*I*/,  9 /*J*/,  10 /*K*/,  11 /*L*/,
    12 /*M*/, 13 /*N*/, 14 /*O*/, 15 /*P*/, 16 /*Q*/, 17 /*R*/,  18 /*S*/,
    19 /*T*/, 20 /*U*/, 21 /*V*/, 22 /*W*/, 23 /*X*/, 24 /*Y*/,  25 /*Z*/,
    -1,       -1,       -1,       -1,       -1,       -1,        26 /*a*/,
    27 /*b*/, 28 /*c*/, 29 /*d*/, 30 /*e*/, 31 /*f*/, 32 /*g*/,  33 /*h*/,
    34 /*i*/, 35 /*j*/, 36 /*k*/, 37 /*l*/, 38 /*m*/, 39 /*n*/,  40 /*o*/,
    41 /*p*/, 42 /*q*/, 43 /*r*/, 44 /*s*/, 45 /*t*/, 46 /*u*/,  47 /*v*/,
    48 /*w*/, 49 /*x*/, 50 /*y*/, 51 /*z*/, -1,       -1,        -1,
    -1,       -1,       -1,       -1,       -1,       -1,        -1,
    -1,       -1,       -1,       -1,       -1,       -1,        -1,
    -1,       -1,       -1,       -1,       -1,       -1,        -1,
    -1,       -1,       -1,       -1,       -1,       -1,        -1,
    -1,       -1,       -1,       -1,       -1,       -1,        -1,
    -1,       -1,       -1,       -1,       -1,       -1,        -1,
    -1,       -1,       -1,       -1,       -1,       -1,        -1,
    -1,       -1,       -1,       -1,       -1,       -1,        -1,
    -1,       -1,       -1,       -1,       -1,       -1,        -1,
    -1,       -1,       -1,       -1,       -1,       -1,        -1,
    -1,       -1,       -1,       -1,       -1,       -1,        -1,
    -1,       -1,       -1,       -1,       -1,       -1,        -1,
    -1,       -1,       -1,       -1,       -1,       -1,        -1,
    -1,       -1,       -1,       -1,       -1,       -1,        -1,
    -1,       -1,       -1,       -1,       -1,       -1,        -1,
    -1,       -1,       -1,       -1,       -1,       -1,        -1,
    -1,       -1,       -1,       -1,       -1,       -1,        -1,
    -1,       -1,       -1,       -1,       -1,       -1,        -1,
    -1,       -1,       -1,       -1};
static const signed char kUnWebSafeBase64[] = {
    -1,       -1,       -1,       -1,       -1,       -1,       -1,
    -1,       -1,       -1,       -1,       -1,       -1,       -1,
    -1,       -1,       -1,       -1,       -1,       -1,       -1,
    -1,       -1,       -1,       -1,       -1,       -1,       -1,
    -1,       -1,       -1,       -1,       -1,       -1,       -1,
    -1,       -1,       -1,       -1,       -1,       -1,       -1,
    -1,       -1,       -1,       62 /*-*/, -1,       -1,       52 /*0*/,
    53 /*1*/, 54 /*2*/, 55 /*3*/, 56 /*4*/, 57 /*5*/, 58 /*6*/, 59 /*7*/,
    60 /*8*/, 61 /*9*/, -1,       -1,       -1,       -1,       -1,
    -1,       -1,       0 /*A*/,  1 /*B*/,  2 /*C*/,  3 /*D*/,  4 /*E*/,
    5 /*F*/,  6 /*G*/,  07 /*H*/, 8 /*I*/,  9 /*J*/,  10 /*K*/, 11 /*L*/,
    12 /*M*/, 13 /*N*/, 14 /*O*/, 15 /*P*/, 16 /*Q*/, 17 /*R*/, 18 /*S*/,
    19 /*T*/, 20 /*U*/, 21 /*V*/, 22 /*W*/, 23 /*X*/, 24 /*Y*/, 25 /*Z*/,
    -1,       -1,       -1,       -1,       63 /*_*/, -1,       26 /*a*/,
    27 /*b*/, 28 /*c*/, 29 /*d*/, 30 /*e*/, 31 /*f*/, 32 /*g*/, 33 /*h*/,
    34 /*i*/, 35 /*j*/, 36 /*k*/, 37 /*l*/, 38 /*m*/, 39 /*n*/, 40 /*o*/,
    41 /*p*/, 42 /*q*/, 43 /*r*/, 44 /*s*/, 45 /*t*/, 46 /*u*/, 47 /*v*/,
    48 /*w*/, 49 /*x*/, 50 /*y*/, 51 /*z*/, -1,       -1,       -1,
    -1,       -1,       -1,       -1,       -1,       -1,       -1,
    -1,       -1,       -1,       -1,       -1,       -1,       -1,
    -1,       -1,       -1,       -1,       -1,       -1,       -1,
    -1,       -1,       -1,       -1,       -1,       -1,       -1,
    -1,       -1,       -1,       -1,       -1,       -1,       -1,
    -1,       -1,       -1,       -1,       -1,       -1,       -1,
    -1,       -1,       -1,       -1,       -1,       -1,       -1,
    -1,       -1,       -1,       -1,       -1,       -1,       -1,
    -1,       -1,       -1,       -1,       -1,       -1,       -1,
    -1,       -1,       -1,       -1,       -1,       -1,       -1,
    -1,       -1,       -1,       -1,       -1,       -1,       -1,
    -1,       -1,       -1,       -1,       -1,       -1,       -1,
    -1,       -1,       -1,       -1,       -1,       -1,       -1,
    -1,       -1,       -1,       -1,       -1,       -1,       -1,
    -1,       -1,       -1,       -1,       -1,       -1,       -1,
    -1,       -1,       -1,       -1,       -1,       -1,       -1,
    -1,       -1,       -1,       -1,       -1,       -1,       -1,
    -1,       -1,       -1,       -1,       -1,       -1,       -1,
    -1,       -1,       -1,       -1};

static bool Base64UnescapeInternal(const char* src, size_t slen, string* dest,
                                   const signed char* unbase64) {
  // Determine the size of the output string.  Base64 encodes every 3 bytes into
  // 4 characters.  any leftover chars are added directly for good measure.
  // This is documented in the base64 RFC: http://tools.ietf.org/html/rfc3548
  const size_t dest_len = 3 * (slen / 4) + (slen % 4);

  dest->resize(dest_len);

  // We are getting the destination buffer by getting the beginning of the
  // string and converting it into a char *.
  size_t len;
  const bool ok =
      Base64UnescapeInternal(src, slen, dest->empty() ? NULL : &*dest->begin(),
                             dest_len, unbase64, &len);
  if (!ok) {
    dest->clear();
    return false;
  }

  // could be shorter if there was padding
  DCHECK_LE(len, dest_len);
  dest->erase(len);

  return true;
}

bool Base64Unescape(const string& src, string* dest) {
  return Base64UnescapeInternal(src.data(), src.size(), dest, kUnBase64);
}

bool WebSafeBase64Unescape(const string& src, string* dest) {
  return Base64UnescapeInternal(src.data(), src.size(), dest, kUnWebSafeBase64);
}

// Base64Escape
//
// NOTE: We have to use an unsigned type for src because code built
// in the the /google tree treats characters as signed unless
// otherwised specified.
int Base64EscapeInternal(const unsigned char* src, int szsrc, char* dest,
                         int szdest, const char* base64, bool do_padding) {
  static const char kPad64 = '=';

  if (szsrc <= 0) return 0;

  char* cur_dest = dest;
  const unsigned char* cur_src = src;

  // Three bytes of data encodes to four characters of cyphertext.
  // So we can pump through three-byte chunks atomically.
  while (szsrc > 2) { /* keep going until we have less than 24 bits */
    if ((szdest -= 4) < 0) return 0;
    cur_dest[0] = base64[cur_src[0] >> 2];
    cur_dest[1] = base64[((cur_src[0] & 0x03) << 4) + (cur_src[1] >> 4)];
    cur_dest[2] = base64[((cur_src[1] & 0x0f) << 2) + (cur_src[2] >> 6)];
    cur_dest[3] = base64[cur_src[2] & 0x3f];

    cur_dest += 4;
    cur_src += 3;
    szsrc -= 3;
  }

  /* now deal with the tail (<=2 bytes) */
  switch (szsrc) {
    case 0:
      // Nothing left; nothing more to do.
      break;
    case 1:
      // One byte left: this encodes to two characters, and (optionally)
      // two pad characters to round out the four-character cypherblock.
      if ((szdest -= 2) < 0) return 0;
      cur_dest[0] = base64[cur_src[0] >> 2];
      cur_dest[1] = base64[(cur_src[0] & 0x03) << 4];
      cur_dest += 2;
      if (do_padding) {
        if ((szdest -= 2) < 0) return 0;
        cur_dest[0] = kPad64;
        cur_dest[1] = kPad64;
        cur_dest += 2;
      }
      break;
    case 2:
      // Two bytes left: this encodes to three characters, and (optionally)
      // one pad character to round out the four-character cypherblock.
      if ((szdest -= 3) < 0) return 0;
      cur_dest[0] = base64[cur_src[0] >> 2];
      cur_dest[1] = base64[((cur_src[0] & 0x03) << 4) + (cur_src[1] >> 4)];
      cur_dest[2] = base64[(cur_src[1] & 0x0f) << 2];
      cur_dest += 3;
      if (do_padding) {
        if ((szdest -= 1) < 0) return 0;
        cur_dest[0] = kPad64;
        cur_dest += 1;
      }
      break;
    default:
      // Should not be reached: blocks of 3 bytes are handled
      // in the while loop before this switch statement.
      CHECK(false) << "Logic problem? szsrc = " << szsrc;
      break;
  }
  return (cur_dest - dest);
}

static const char kBase64Chars[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

// Digit conversion.
static const char kHexTable[513] =
    "000102030405060708090a0b0c0d0e0f"
    "101112131415161718191a1b1c1d1e1f"
    "202122232425262728292a2b2c2d2e2f"
    "303132333435363738393a3b3c3d3e3f"
    "404142434445464748494a4b4c4d4e4f"
    "505152535455565758595a5b5c5d5e5f"
    "606162636465666768696a6b6c6d6e6f"
    "707172737475767778797a7b7c7d7e7f"
    "808182838485868788898a8b8c8d8e8f"
    "909192939495969798999a9b9c9d9e9f"
    "a0a1a2a3a4a5a6a7a8a9aaabacadaeaf"
    "b0b1b2b3b4b5b6b7b8b9babbbcbdbebf"
    "c0c1c2c3c4c5c6c7c8c9cacbcccdcecf"
    "d0d1d2d3d4d5d6d7d8d9dadbdcdddedf"
    "e0e1e2e3e4e5e6e7e8e9eaebecedeeef"
    "f0f1f2f3f4f5f6f7f8f9fafbfcfdfeff";

size_t CalculateBase64EscapedLenInternal(size_t input_len, bool do_padding) {
  // Base64 encodes three bytes of input at a time. If the input is not
  // divisible by three, we pad as appropriate.
  //
  // (from http://tools.ietf.org/html/rfc3548)
  // Special processing is performed if fewer than 24 bits are available
  // at the end of the data being encoded.  A full encoding quantum is
  // always completed at the end of a quantity.  When fewer than 24 input
  // bits are available in an input group, zero bits are added (on the
  // right) to form an integral number of 6-bit groups.  Padding at the
  // end of the data is performed using the '=' character.  Since all base
  // 64 input is an integral number of octets, only the following cases
  // can arise:

  // Base64 encodes each three bytes of input into four bytes of output.
  size_t len = (input_len / 3) * 4;

  if (input_len % 3 == 0) {
    // (from http://tools.ietf.org/html/rfc3548)
    // (1) the final quantum of encoding input is an integral multiple of 24
    // bits; here, the final unit of encoded output will be an integral
    // multiple of 4 characters with no "=" padding,
  } else if (input_len % 3 == 1) {
    // (from http://tools.ietf.org/html/rfc3548)
    // (2) the final quantum of encoding input is exactly 8 bits; here, the
    // final unit of encoded output will be two characters followed by two
    // "=" padding characters, or
    len += 2;
    if (do_padding) {
      len += 2;
    }
  } else {  // (input_len % 3 == 2)
    // (from http://tools.ietf.org/html/rfc3548)
    // (3) the final quantum of encoding input is exactly 16 bits; here, the
    // final unit of encoded output will be three characters followed by one
    // "=" padding character.
    len += 3;
    if (do_padding) {
      len += 1;
    }
  }

  assert(len >= input_len);  // make sure we didn't overflow
  return len;
}

void Base64EscapeInternal(const unsigned char* src, size_t szsrc, string* dest,
                          bool do_padding, const char* base64_chars) {
  const size_t calc_escaped_size =
      CalculateBase64EscapedLenInternal(szsrc, do_padding);
  dest->resize(calc_escaped_size);
  const int escaped_len = Base64EscapeInternal(
      src, static_cast<int>(szsrc), dest->empty() ? NULL : &*dest->begin(),
      static_cast<int>(dest->size()), base64_chars, do_padding);
  DCHECK_EQ(calc_escaped_size, escaped_len);
  dest->erase(escaped_len);
}

void Base64Escape(const unsigned char* src, ptrdiff_t szsrc, string* dest,
                  bool do_padding) {
  if (szsrc < 0) return;
  Base64EscapeInternal(src, szsrc, dest, do_padding, kBase64Chars);
}

// This is a templated function so that T can be either a char* or a string.
template <typename T>
static void b2a_hex_t(const unsigned char* src, T dest, ptrdiff_t num) {
  auto dest_ptr = &dest[0];
  for (auto src_ptr = src; src_ptr != (src + num); ++src_ptr, dest_ptr += 2) {
    const char* hex_p = &kHexTable[*src_ptr * 2];
    std::copy(hex_p, hex_p + 2, dest_ptr);
  }
}

string b2a_hex(const char* b, ptrdiff_t len) {
  string result;
  result.resize(len << 1);
  b2a_hex_t<string&>(reinterpret_cast<const unsigned char*>(b), result, len);
  return result;
}

}  // namespace dynamic_depth
