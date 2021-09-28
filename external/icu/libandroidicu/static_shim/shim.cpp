/* GENERATED SOURCE. DO NOT MODIFY. */
/* Run external/icu/tools/icu4c_srcgen/generate_libandroidicu.py to regenerate */
// © 2018 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html

#include <unicode/icudataver.h>
#include <unicode/putil.h>
#include <unicode/ubidi.h>
#include <unicode/ubiditransform.h>
#include <unicode/ubrk.h>
#include <unicode/ucal.h>
#include <unicode/ucasemap.h>
#include <unicode/ucat.h>
#include <unicode/uchar.h>
#include <unicode/uclean.h>
#include <unicode/ucnv.h>
#include <unicode/ucnv_cb.h>
#include <unicode/ucnv_err.h>
#include <unicode/ucnvsel.h>
#include <unicode/ucol.h>
#include <unicode/ucoleitr.h>
#include <unicode/ucpmap.h>
#include <unicode/ucptrie.h>
#include <unicode/ucsdet.h>
#include <unicode/ucurr.h>
#include <unicode/udat.h>
#include <unicode/udata.h>
#include <unicode/udateintervalformat.h>
#include <unicode/udatpg.h>
#include <unicode/uenum.h>
#include <unicode/ufieldpositer.h>
#include <unicode/uformattable.h>
#include <unicode/ugender.h>
#include <unicode/uidna.h>
#include <unicode/uiter.h>
#include <unicode/uldnames.h>
#include <unicode/ulistformatter.h>
#include <unicode/uloc.h>
#include <unicode/ulocdata.h>
#include <unicode/umsg.h>
#include <unicode/umutablecptrie.h>
#include <unicode/unorm2.h>
#include <unicode/unum.h>
#include <unicode/unumberformatter.h>
#include <unicode/unumsys.h>
#include <unicode/upluralrules.h>
#include <unicode/uregex.h>
#include <unicode/uregion.h>
#include <unicode/ureldatefmt.h>
#include <unicode/ures.h>
#include <unicode/uscript.h>
#include <unicode/usearch.h>
#include <unicode/uset.h>
#include <unicode/ushape.h>
#include <unicode/uspoof.h>
#include <unicode/usprep.h>
#include <unicode/ustring.h>
#include <unicode/utext.h>
#include <unicode/utf8.h>
#include <unicode/utmscale.h>
#include <unicode/utrace.h>
#include <unicode/utrans.h>
#include <unicode/utypes.h>
#include <unicode/uversion.h>

extern "C" {
void UCNV_FROM_U_CALLBACK_ESCAPE_android(const void * context, UConverterFromUnicodeArgs * fromUArgs, const UChar * codeUnits, int32_t length, UChar32 codePoint, UConverterCallbackReason reason, UErrorCode * err) {
  UCNV_FROM_U_CALLBACK_ESCAPE(context, fromUArgs, codeUnits, length, codePoint, reason, err);
}
void UCNV_FROM_U_CALLBACK_SKIP_android(const void * context, UConverterFromUnicodeArgs * fromUArgs, const UChar * codeUnits, int32_t length, UChar32 codePoint, UConverterCallbackReason reason, UErrorCode * err) {
  UCNV_FROM_U_CALLBACK_SKIP(context, fromUArgs, codeUnits, length, codePoint, reason, err);
}
void UCNV_FROM_U_CALLBACK_STOP_android(const void * context, UConverterFromUnicodeArgs * fromUArgs, const UChar * codeUnits, int32_t length, UChar32 codePoint, UConverterCallbackReason reason, UErrorCode * err) {
  UCNV_FROM_U_CALLBACK_STOP(context, fromUArgs, codeUnits, length, codePoint, reason, err);
}
void UCNV_FROM_U_CALLBACK_SUBSTITUTE_android(const void * context, UConverterFromUnicodeArgs * fromUArgs, const UChar * codeUnits, int32_t length, UChar32 codePoint, UConverterCallbackReason reason, UErrorCode * err) {
  UCNV_FROM_U_CALLBACK_SUBSTITUTE(context, fromUArgs, codeUnits, length, codePoint, reason, err);
}
void UCNV_TO_U_CALLBACK_ESCAPE_android(const void * context, UConverterToUnicodeArgs * toUArgs, const char * codeUnits, int32_t length, UConverterCallbackReason reason, UErrorCode * err) {
  UCNV_TO_U_CALLBACK_ESCAPE(context, toUArgs, codeUnits, length, reason, err);
}
void UCNV_TO_U_CALLBACK_SKIP_android(const void * context, UConverterToUnicodeArgs * toUArgs, const char * codeUnits, int32_t length, UConverterCallbackReason reason, UErrorCode * err) {
  UCNV_TO_U_CALLBACK_SKIP(context, toUArgs, codeUnits, length, reason, err);
}
void UCNV_TO_U_CALLBACK_STOP_android(const void * context, UConverterToUnicodeArgs * toUArgs, const char * codeUnits, int32_t length, UConverterCallbackReason reason, UErrorCode * err) {
  UCNV_TO_U_CALLBACK_STOP(context, toUArgs, codeUnits, length, reason, err);
}
void UCNV_TO_U_CALLBACK_SUBSTITUTE_android(const void * context, UConverterToUnicodeArgs * toUArgs, const char * codeUnits, int32_t length, UConverterCallbackReason reason, UErrorCode * err) {
  UCNV_TO_U_CALLBACK_SUBSTITUTE(context, toUArgs, codeUnits, length, reason, err);
}
void u_UCharsToChars_android(const UChar * us, char * cs, int32_t length) {
  u_UCharsToChars(us, cs, length);
}
char * u_austrcpy_android(char * dst, const UChar * src) {
  return u_austrcpy(dst, src);
}
char * u_austrncpy_android(char * dst, const UChar * src, int32_t n) {
  return u_austrncpy(dst, src, n);
}
void u_catclose_android(u_nl_catd catd) {
  u_catclose(catd);
}
const UChar * u_catgets_android(u_nl_catd catd, int32_t set_num, int32_t msg_num, const UChar * s, int32_t * len, UErrorCode * ec) {
  return u_catgets(catd, set_num, msg_num, s, len, ec);
}
u_nl_catd u_catopen_android(const char * name, const char * locale, UErrorCode * ec) {
  return u_catopen(name, locale, ec);
}
void u_charAge_android(UChar32 c, UVersionInfo versionArray) {
  u_charAge(c, versionArray);
}
int32_t u_charDigitValue_android(UChar32 c) {
  return u_charDigitValue(c);
}
UCharDirection u_charDirection_android(UChar32 c) {
  return u_charDirection(c);
}
UChar32 u_charFromName_android(UCharNameChoice nameChoice, const char * name, UErrorCode * pErrorCode) {
  return u_charFromName(nameChoice, name, pErrorCode);
}
UChar32 u_charMirror_android(UChar32 c) {
  return u_charMirror(c);
}
int32_t u_charName_android(UChar32 code, UCharNameChoice nameChoice, char * buffer, int32_t bufferLength, UErrorCode * pErrorCode) {
  return u_charName(code, nameChoice, buffer, bufferLength, pErrorCode);
}
int8_t u_charType_android(UChar32 c) {
  return u_charType(c);
}
void u_charsToUChars_android(const char * cs, UChar * us, int32_t length) {
  u_charsToUChars(cs, us, length);
}
void u_cleanup_android() {
  u_cleanup();
}
int32_t u_countChar32_android(const UChar * s, int32_t length) {
  return u_countChar32(s, length);
}
int32_t u_digit_android(UChar32 ch, int8_t radix) {
  return u_digit(ch, radix);
}
void u_enumCharNames_android(UChar32 start, UChar32 limit, UEnumCharNamesFn * fn, void * context, UCharNameChoice nameChoice, UErrorCode * pErrorCode) {
  u_enumCharNames(start, limit, fn, context, nameChoice, pErrorCode);
}
void u_enumCharTypes_android(UCharEnumTypeRange * enumRange, const void * context) {
  u_enumCharTypes(enumRange, context);
}
const char * u_errorName_android(UErrorCode code) {
  return u_errorName(code);
}
UChar32 u_foldCase_android(UChar32 c, uint32_t options) {
  return u_foldCase(c, options);
}
UChar32 u_forDigit_android(int32_t digit, int8_t radix) {
  return u_forDigit(digit, radix);
}
int32_t u_formatMessage_android(const char * locale, const UChar * pattern, int32_t patternLength, UChar * result, int32_t resultLength, UErrorCode * status, ...) {
  va_list args;
  va_start(args, status);
  int32_t ret = u_vformatMessage(locale, pattern, patternLength, result, resultLength, args, status);
  va_end(args);
  return ret;
}
int32_t u_formatMessageWithError_android(const char * locale, const UChar * pattern, int32_t patternLength, UChar * result, int32_t resultLength, UParseError * parseError, UErrorCode * status, ...) {
  va_list args;
  va_start(args, status);
  int32_t ret = u_vformatMessageWithError(locale, pattern, patternLength, result, resultLength, parseError, args, status);
  va_end(args);
  return ret;
}
UChar32 u_getBidiPairedBracket_android(UChar32 c) {
  return u_getBidiPairedBracket(c);
}
const USet * u_getBinaryPropertySet_android(UProperty property, UErrorCode * pErrorCode) {
  return u_getBinaryPropertySet(property, pErrorCode);
}
uint8_t u_getCombiningClass_android(UChar32 c) {
  return u_getCombiningClass(c);
}
const char * u_getDataDirectory_android() {
  return u_getDataDirectory();
}
void u_getDataVersion_android(UVersionInfo dataVersionFillin, UErrorCode * status) {
  u_getDataVersion(dataVersionFillin, status);
}
int32_t u_getFC_NFKC_Closure_android(UChar32 c, UChar * dest, int32_t destCapacity, UErrorCode * pErrorCode) {
  return u_getFC_NFKC_Closure(c, dest, destCapacity, pErrorCode);
}
const UCPMap * u_getIntPropertyMap_android(UProperty property, UErrorCode * pErrorCode) {
  return u_getIntPropertyMap(property, pErrorCode);
}
int32_t u_getIntPropertyMaxValue_android(UProperty which) {
  return u_getIntPropertyMaxValue(which);
}
int32_t u_getIntPropertyMinValue_android(UProperty which) {
  return u_getIntPropertyMinValue(which);
}
int32_t u_getIntPropertyValue_android(UChar32 c, UProperty which) {
  return u_getIntPropertyValue(c, which);
}
double u_getNumericValue_android(UChar32 c) {
  return u_getNumericValue(c);
}
UProperty u_getPropertyEnum_android(const char * alias) {
  return u_getPropertyEnum(alias);
}
const char * u_getPropertyName_android(UProperty property, UPropertyNameChoice nameChoice) {
  return u_getPropertyName(property, nameChoice);
}
int32_t u_getPropertyValueEnum_android(UProperty property, const char * alias) {
  return u_getPropertyValueEnum(property, alias);
}
const char * u_getPropertyValueName_android(UProperty property, int32_t value, UPropertyNameChoice nameChoice) {
  return u_getPropertyValueName(property, value, nameChoice);
}
void u_getUnicodeVersion_android(UVersionInfo versionArray) {
  u_getUnicodeVersion(versionArray);
}
void u_getVersion_android(UVersionInfo versionArray) {
  u_getVersion(versionArray);
}
UBool u_hasBinaryProperty_android(UChar32 c, UProperty which) {
  return u_hasBinaryProperty(c, which);
}
void u_init_android(UErrorCode * status) {
  u_init(status);
}
UBool u_isIDIgnorable_android(UChar32 c) {
  return u_isIDIgnorable(c);
}
UBool u_isIDPart_android(UChar32 c) {
  return u_isIDPart(c);
}
UBool u_isIDStart_android(UChar32 c) {
  return u_isIDStart(c);
}
UBool u_isISOControl_android(UChar32 c) {
  return u_isISOControl(c);
}
UBool u_isJavaIDPart_android(UChar32 c) {
  return u_isJavaIDPart(c);
}
UBool u_isJavaIDStart_android(UChar32 c) {
  return u_isJavaIDStart(c);
}
UBool u_isJavaSpaceChar_android(UChar32 c) {
  return u_isJavaSpaceChar(c);
}
UBool u_isMirrored_android(UChar32 c) {
  return u_isMirrored(c);
}
UBool u_isUAlphabetic_android(UChar32 c) {
  return u_isUAlphabetic(c);
}
UBool u_isULowercase_android(UChar32 c) {
  return u_isULowercase(c);
}
UBool u_isUUppercase_android(UChar32 c) {
  return u_isUUppercase(c);
}
UBool u_isUWhiteSpace_android(UChar32 c) {
  return u_isUWhiteSpace(c);
}
UBool u_isWhitespace_android(UChar32 c) {
  return u_isWhitespace(c);
}
UBool u_isalnum_android(UChar32 c) {
  return u_isalnum(c);
}
UBool u_isalpha_android(UChar32 c) {
  return u_isalpha(c);
}
UBool u_isbase_android(UChar32 c) {
  return u_isbase(c);
}
UBool u_isblank_android(UChar32 c) {
  return u_isblank(c);
}
UBool u_iscntrl_android(UChar32 c) {
  return u_iscntrl(c);
}
UBool u_isdefined_android(UChar32 c) {
  return u_isdefined(c);
}
UBool u_isdigit_android(UChar32 c) {
  return u_isdigit(c);
}
UBool u_isgraph_android(UChar32 c) {
  return u_isgraph(c);
}
UBool u_islower_android(UChar32 c) {
  return u_islower(c);
}
UBool u_isprint_android(UChar32 c) {
  return u_isprint(c);
}
UBool u_ispunct_android(UChar32 c) {
  return u_ispunct(c);
}
UBool u_isspace_android(UChar32 c) {
  return u_isspace(c);
}
UBool u_istitle_android(UChar32 c) {
  return u_istitle(c);
}
UBool u_isupper_android(UChar32 c) {
  return u_isupper(c);
}
UBool u_isxdigit_android(UChar32 c) {
  return u_isxdigit(c);
}
int32_t u_memcasecmp_android(const UChar * s1, const UChar * s2, int32_t length, uint32_t options) {
  return u_memcasecmp(s1, s2, length, options);
}
UChar * u_memchr_android(const UChar * s, UChar c, int32_t count) {
  return u_memchr(s, c, count);
}
UChar * u_memchr32_android(const UChar * s, UChar32 c, int32_t count) {
  return u_memchr32(s, c, count);
}
int32_t u_memcmp_android(const UChar * buf1, const UChar * buf2, int32_t count) {
  return u_memcmp(buf1, buf2, count);
}
int32_t u_memcmpCodePointOrder_android(const UChar * s1, const UChar * s2, int32_t count) {
  return u_memcmpCodePointOrder(s1, s2, count);
}
UChar * u_memcpy_android(UChar * dest, const UChar * src, int32_t count) {
  return u_memcpy(dest, src, count);
}
UChar * u_memmove_android(UChar * dest, const UChar * src, int32_t count) {
  return u_memmove(dest, src, count);
}
UChar * u_memrchr_android(const UChar * s, UChar c, int32_t count) {
  return u_memrchr(s, c, count);
}
UChar * u_memrchr32_android(const UChar * s, UChar32 c, int32_t count) {
  return u_memrchr32(s, c, count);
}
UChar * u_memset_android(UChar * dest, UChar c, int32_t count) {
  return u_memset(dest, c, count);
}
void u_parseMessage_android(const char * locale, const UChar * pattern, int32_t patternLength, const UChar * source, int32_t sourceLength, UErrorCode * status, ...) {
  va_list args;
  va_start(args, status);
  u_vparseMessage(locale, pattern, patternLength, source, sourceLength, args, status);
  va_end(args);
  return;
}
void u_parseMessageWithError_android(const char * locale, const UChar * pattern, int32_t patternLength, const UChar * source, int32_t sourceLength, UParseError * parseError, UErrorCode * status, ...) {
  va_list args;
  va_start(args, status);
  u_vparseMessageWithError(locale, pattern, patternLength, source, sourceLength, args, parseError, status);
  va_end(args);
  return;
}
void u_setDataDirectory_android(const char * directory) {
  u_setDataDirectory(directory);
}
void u_setMemoryFunctions_android(const void * context, UMemAllocFn * a, UMemReallocFn * r, UMemFreeFn * f, UErrorCode * status) {
  u_setMemoryFunctions(context, a, r, f, status);
}
int32_t u_shapeArabic_android(const UChar * source, int32_t sourceLength, UChar * dest, int32_t destSize, uint32_t options, UErrorCode * pErrorCode) {
  return u_shapeArabic(source, sourceLength, dest, destSize, options, pErrorCode);
}
int32_t u_strCaseCompare_android(const UChar * s1, int32_t length1, const UChar * s2, int32_t length2, uint32_t options, UErrorCode * pErrorCode) {
  return u_strCaseCompare(s1, length1, s2, length2, options, pErrorCode);
}
int32_t u_strCompare_android(const UChar * s1, int32_t length1, const UChar * s2, int32_t length2, UBool codePointOrder) {
  return u_strCompare(s1, length1, s2, length2, codePointOrder);
}
int32_t u_strCompareIter_android(UCharIterator * iter1, UCharIterator * iter2, UBool codePointOrder) {
  return u_strCompareIter(iter1, iter2, codePointOrder);
}
UChar * u_strFindFirst_android(const UChar * s, int32_t length, const UChar * substring, int32_t subLength) {
  return u_strFindFirst(s, length, substring, subLength);
}
UChar * u_strFindLast_android(const UChar * s, int32_t length, const UChar * substring, int32_t subLength) {
  return u_strFindLast(s, length, substring, subLength);
}
int32_t u_strFoldCase_android(UChar * dest, int32_t destCapacity, const UChar * src, int32_t srcLength, uint32_t options, UErrorCode * pErrorCode) {
  return u_strFoldCase(dest, destCapacity, src, srcLength, options, pErrorCode);
}
UChar * u_strFromJavaModifiedUTF8WithSub_android(UChar * dest, int32_t destCapacity, int32_t * pDestLength, const char * src, int32_t srcLength, UChar32 subchar, int32_t * pNumSubstitutions, UErrorCode * pErrorCode) {
  return u_strFromJavaModifiedUTF8WithSub(dest, destCapacity, pDestLength, src, srcLength, subchar, pNumSubstitutions, pErrorCode);
}
UChar * u_strFromUTF32_android(UChar * dest, int32_t destCapacity, int32_t * pDestLength, const UChar32 * src, int32_t srcLength, UErrorCode * pErrorCode) {
  return u_strFromUTF32(dest, destCapacity, pDestLength, src, srcLength, pErrorCode);
}
UChar * u_strFromUTF32WithSub_android(UChar * dest, int32_t destCapacity, int32_t * pDestLength, const UChar32 * src, int32_t srcLength, UChar32 subchar, int32_t * pNumSubstitutions, UErrorCode * pErrorCode) {
  return u_strFromUTF32WithSub(dest, destCapacity, pDestLength, src, srcLength, subchar, pNumSubstitutions, pErrorCode);
}
UChar * u_strFromUTF8_android(UChar * dest, int32_t destCapacity, int32_t * pDestLength, const char * src, int32_t srcLength, UErrorCode * pErrorCode) {
  return u_strFromUTF8(dest, destCapacity, pDestLength, src, srcLength, pErrorCode);
}
UChar * u_strFromUTF8Lenient_android(UChar * dest, int32_t destCapacity, int32_t * pDestLength, const char * src, int32_t srcLength, UErrorCode * pErrorCode) {
  return u_strFromUTF8Lenient(dest, destCapacity, pDestLength, src, srcLength, pErrorCode);
}
UChar * u_strFromUTF8WithSub_android(UChar * dest, int32_t destCapacity, int32_t * pDestLength, const char * src, int32_t srcLength, UChar32 subchar, int32_t * pNumSubstitutions, UErrorCode * pErrorCode) {
  return u_strFromUTF8WithSub(dest, destCapacity, pDestLength, src, srcLength, subchar, pNumSubstitutions, pErrorCode);
}
UChar * u_strFromWCS_android(UChar * dest, int32_t destCapacity, int32_t * pDestLength, const wchar_t * src, int32_t srcLength, UErrorCode * pErrorCode) {
  return u_strFromWCS(dest, destCapacity, pDestLength, src, srcLength, pErrorCode);
}
UBool u_strHasMoreChar32Than_android(const UChar * s, int32_t length, int32_t number) {
  return u_strHasMoreChar32Than(s, length, number);
}
char * u_strToJavaModifiedUTF8_android(char * dest, int32_t destCapacity, int32_t * pDestLength, const UChar * src, int32_t srcLength, UErrorCode * pErrorCode) {
  return u_strToJavaModifiedUTF8(dest, destCapacity, pDestLength, src, srcLength, pErrorCode);
}
int32_t u_strToLower_android(UChar * dest, int32_t destCapacity, const UChar * src, int32_t srcLength, const char * locale, UErrorCode * pErrorCode) {
  return u_strToLower(dest, destCapacity, src, srcLength, locale, pErrorCode);
}
int32_t u_strToTitle_android(UChar * dest, int32_t destCapacity, const UChar * src, int32_t srcLength, UBreakIterator * titleIter, const char * locale, UErrorCode * pErrorCode) {
  return u_strToTitle(dest, destCapacity, src, srcLength, titleIter, locale, pErrorCode);
}
UChar32 * u_strToUTF32_android(UChar32 * dest, int32_t destCapacity, int32_t * pDestLength, const UChar * src, int32_t srcLength, UErrorCode * pErrorCode) {
  return u_strToUTF32(dest, destCapacity, pDestLength, src, srcLength, pErrorCode);
}
UChar32 * u_strToUTF32WithSub_android(UChar32 * dest, int32_t destCapacity, int32_t * pDestLength, const UChar * src, int32_t srcLength, UChar32 subchar, int32_t * pNumSubstitutions, UErrorCode * pErrorCode) {
  return u_strToUTF32WithSub(dest, destCapacity, pDestLength, src, srcLength, subchar, pNumSubstitutions, pErrorCode);
}
char * u_strToUTF8_android(char * dest, int32_t destCapacity, int32_t * pDestLength, const UChar * src, int32_t srcLength, UErrorCode * pErrorCode) {
  return u_strToUTF8(dest, destCapacity, pDestLength, src, srcLength, pErrorCode);
}
char * u_strToUTF8WithSub_android(char * dest, int32_t destCapacity, int32_t * pDestLength, const UChar * src, int32_t srcLength, UChar32 subchar, int32_t * pNumSubstitutions, UErrorCode * pErrorCode) {
  return u_strToUTF8WithSub(dest, destCapacity, pDestLength, src, srcLength, subchar, pNumSubstitutions, pErrorCode);
}
int32_t u_strToUpper_android(UChar * dest, int32_t destCapacity, const UChar * src, int32_t srcLength, const char * locale, UErrorCode * pErrorCode) {
  return u_strToUpper(dest, destCapacity, src, srcLength, locale, pErrorCode);
}
wchar_t * u_strToWCS_android(wchar_t * dest, int32_t destCapacity, int32_t * pDestLength, const UChar * src, int32_t srcLength, UErrorCode * pErrorCode) {
  return u_strToWCS(dest, destCapacity, pDestLength, src, srcLength, pErrorCode);
}
int32_t u_strcasecmp_android(const UChar * s1, const UChar * s2, uint32_t options) {
  return u_strcasecmp(s1, s2, options);
}
UChar * u_strcat_android(UChar * dst, const UChar * src) {
  return u_strcat(dst, src);
}
UChar * u_strchr_android(const UChar * s, UChar c) {
  return u_strchr(s, c);
}
UChar * u_strchr32_android(const UChar * s, UChar32 c) {
  return u_strchr32(s, c);
}
int32_t u_strcmp_android(const UChar * s1, const UChar * s2) {
  return u_strcmp(s1, s2);
}
int32_t u_strcmpCodePointOrder_android(const UChar * s1, const UChar * s2) {
  return u_strcmpCodePointOrder(s1, s2);
}
UChar * u_strcpy_android(UChar * dst, const UChar * src) {
  return u_strcpy(dst, src);
}
int32_t u_strcspn_android(const UChar * string, const UChar * matchSet) {
  return u_strcspn(string, matchSet);
}
int32_t u_strlen_android(const UChar * s) {
  return u_strlen(s);
}
int32_t u_strncasecmp_android(const UChar * s1, const UChar * s2, int32_t n, uint32_t options) {
  return u_strncasecmp(s1, s2, n, options);
}
UChar * u_strncat_android(UChar * dst, const UChar * src, int32_t n) {
  return u_strncat(dst, src, n);
}
int32_t u_strncmp_android(const UChar * ucs1, const UChar * ucs2, int32_t n) {
  return u_strncmp(ucs1, ucs2, n);
}
int32_t u_strncmpCodePointOrder_android(const UChar * s1, const UChar * s2, int32_t n) {
  return u_strncmpCodePointOrder(s1, s2, n);
}
UChar * u_strncpy_android(UChar * dst, const UChar * src, int32_t n) {
  return u_strncpy(dst, src, n);
}
UChar * u_strpbrk_android(const UChar * string, const UChar * matchSet) {
  return u_strpbrk(string, matchSet);
}
UChar * u_strrchr_android(const UChar * s, UChar c) {
  return u_strrchr(s, c);
}
UChar * u_strrchr32_android(const UChar * s, UChar32 c) {
  return u_strrchr32(s, c);
}
UChar * u_strrstr_android(const UChar * s, const UChar * substring) {
  return u_strrstr(s, substring);
}
int32_t u_strspn_android(const UChar * string, const UChar * matchSet) {
  return u_strspn(string, matchSet);
}
UChar * u_strstr_android(const UChar * s, const UChar * substring) {
  return u_strstr(s, substring);
}
UChar * u_strtok_r_android(UChar * src, const UChar * delim, UChar ** saveState) {
  return u_strtok_r(src, delim, saveState);
}
UChar32 u_tolower_android(UChar32 c) {
  return u_tolower(c);
}
UChar32 u_totitle_android(UChar32 c) {
  return u_totitle(c);
}
UChar32 u_toupper_android(UChar32 c) {
  return u_toupper(c);
}
UChar * u_uastrcpy_android(UChar * dst, const char * src) {
  return u_uastrcpy(dst, src);
}
UChar * u_uastrncpy_android(UChar * dst, const char * src, int32_t n) {
  return u_uastrncpy(dst, src, n);
}
int32_t u_unescape_android(const char * src, UChar * dest, int32_t destCapacity) {
  return u_unescape(src, dest, destCapacity);
}
UChar32 u_unescapeAt_android(UNESCAPE_CHAR_AT charAt, int32_t * offset, int32_t length, void * context) {
  return u_unescapeAt(charAt, offset, length, context);
}
void u_versionFromString_android(UVersionInfo versionArray, const char * versionString) {
  u_versionFromString(versionArray, versionString);
}
void u_versionFromUString_android(UVersionInfo versionArray, const UChar * versionString) {
  u_versionFromUString(versionArray, versionString);
}
void u_versionToString_android(const UVersionInfo versionArray, char * versionString) {
  u_versionToString(versionArray, versionString);
}
int32_t u_vformatMessage_android(const char * locale, const UChar * pattern, int32_t patternLength, UChar * result, int32_t resultLength, va_list ap, UErrorCode * status) {
  return u_vformatMessage(locale, pattern, patternLength, result, resultLength, ap, status);
}
int32_t u_vformatMessageWithError_android(const char * locale, const UChar * pattern, int32_t patternLength, UChar * result, int32_t resultLength, UParseError * parseError, va_list ap, UErrorCode * status) {
  return u_vformatMessageWithError(locale, pattern, patternLength, result, resultLength, parseError, ap, status);
}
void u_vparseMessage_android(const char * locale, const UChar * pattern, int32_t patternLength, const UChar * source, int32_t sourceLength, va_list ap, UErrorCode * status) {
  u_vparseMessage(locale, pattern, patternLength, source, sourceLength, ap, status);
}
void u_vparseMessageWithError_android(const char * locale, const UChar * pattern, int32_t patternLength, const UChar * source, int32_t sourceLength, va_list ap, UParseError * parseError, UErrorCode * status) {
  u_vparseMessageWithError(locale, pattern, patternLength, source, sourceLength, ap, parseError, status);
}
void ubidi_close_android(UBiDi * pBiDi) {
  ubidi_close(pBiDi);
}
int32_t ubidi_countParagraphs_android(UBiDi * pBiDi) {
  return ubidi_countParagraphs(pBiDi);
}
int32_t ubidi_countRuns_android(UBiDi * pBiDi, UErrorCode * pErrorCode) {
  return ubidi_countRuns(pBiDi, pErrorCode);
}
UBiDiDirection ubidi_getBaseDirection_android(const UChar * text, int32_t length) {
  return ubidi_getBaseDirection(text, length);
}
void ubidi_getClassCallback_android(UBiDi * pBiDi, UBiDiClassCallback ** fn, const void ** context) {
  ubidi_getClassCallback(pBiDi, fn, context);
}
UCharDirection ubidi_getCustomizedClass_android(UBiDi * pBiDi, UChar32 c) {
  return ubidi_getCustomizedClass(pBiDi, c);
}
UBiDiDirection ubidi_getDirection_android(const UBiDi * pBiDi) {
  return ubidi_getDirection(pBiDi);
}
int32_t ubidi_getLength_android(const UBiDi * pBiDi) {
  return ubidi_getLength(pBiDi);
}
UBiDiLevel ubidi_getLevelAt_android(const UBiDi * pBiDi, int32_t charIndex) {
  return ubidi_getLevelAt(pBiDi, charIndex);
}
const UBiDiLevel * ubidi_getLevels_android(UBiDi * pBiDi, UErrorCode * pErrorCode) {
  return ubidi_getLevels(pBiDi, pErrorCode);
}
int32_t ubidi_getLogicalIndex_android(UBiDi * pBiDi, int32_t visualIndex, UErrorCode * pErrorCode) {
  return ubidi_getLogicalIndex(pBiDi, visualIndex, pErrorCode);
}
void ubidi_getLogicalMap_android(UBiDi * pBiDi, int32_t * indexMap, UErrorCode * pErrorCode) {
  ubidi_getLogicalMap(pBiDi, indexMap, pErrorCode);
}
void ubidi_getLogicalRun_android(const UBiDi * pBiDi, int32_t logicalPosition, int32_t * pLogicalLimit, UBiDiLevel * pLevel) {
  ubidi_getLogicalRun(pBiDi, logicalPosition, pLogicalLimit, pLevel);
}
UBiDiLevel ubidi_getParaLevel_android(const UBiDi * pBiDi) {
  return ubidi_getParaLevel(pBiDi);
}
int32_t ubidi_getParagraph_android(const UBiDi * pBiDi, int32_t charIndex, int32_t * pParaStart, int32_t * pParaLimit, UBiDiLevel * pParaLevel, UErrorCode * pErrorCode) {
  return ubidi_getParagraph(pBiDi, charIndex, pParaStart, pParaLimit, pParaLevel, pErrorCode);
}
void ubidi_getParagraphByIndex_android(const UBiDi * pBiDi, int32_t paraIndex, int32_t * pParaStart, int32_t * pParaLimit, UBiDiLevel * pParaLevel, UErrorCode * pErrorCode) {
  ubidi_getParagraphByIndex(pBiDi, paraIndex, pParaStart, pParaLimit, pParaLevel, pErrorCode);
}
int32_t ubidi_getProcessedLength_android(const UBiDi * pBiDi) {
  return ubidi_getProcessedLength(pBiDi);
}
UBiDiReorderingMode ubidi_getReorderingMode_android(UBiDi * pBiDi) {
  return ubidi_getReorderingMode(pBiDi);
}
uint32_t ubidi_getReorderingOptions_android(UBiDi * pBiDi) {
  return ubidi_getReorderingOptions(pBiDi);
}
int32_t ubidi_getResultLength_android(const UBiDi * pBiDi) {
  return ubidi_getResultLength(pBiDi);
}
const UChar * ubidi_getText_android(const UBiDi * pBiDi) {
  return ubidi_getText(pBiDi);
}
int32_t ubidi_getVisualIndex_android(UBiDi * pBiDi, int32_t logicalIndex, UErrorCode * pErrorCode) {
  return ubidi_getVisualIndex(pBiDi, logicalIndex, pErrorCode);
}
void ubidi_getVisualMap_android(UBiDi * pBiDi, int32_t * indexMap, UErrorCode * pErrorCode) {
  ubidi_getVisualMap(pBiDi, indexMap, pErrorCode);
}
UBiDiDirection ubidi_getVisualRun_android(UBiDi * pBiDi, int32_t runIndex, int32_t * pLogicalStart, int32_t * pLength) {
  return ubidi_getVisualRun(pBiDi, runIndex, pLogicalStart, pLength);
}
void ubidi_invertMap_android(const int32_t * srcMap, int32_t * destMap, int32_t length) {
  ubidi_invertMap(srcMap, destMap, length);
}
UBool ubidi_isInverse_android(UBiDi * pBiDi) {
  return ubidi_isInverse(pBiDi);
}
UBool ubidi_isOrderParagraphsLTR_android(UBiDi * pBiDi) {
  return ubidi_isOrderParagraphsLTR(pBiDi);
}
UBiDi * ubidi_open_android() {
  return ubidi_open();
}
UBiDi * ubidi_openSized_android(int32_t maxLength, int32_t maxRunCount, UErrorCode * pErrorCode) {
  return ubidi_openSized(maxLength, maxRunCount, pErrorCode);
}
void ubidi_orderParagraphsLTR_android(UBiDi * pBiDi, UBool orderParagraphsLTR) {
  ubidi_orderParagraphsLTR(pBiDi, orderParagraphsLTR);
}
void ubidi_reorderLogical_android(const UBiDiLevel * levels, int32_t length, int32_t * indexMap) {
  ubidi_reorderLogical(levels, length, indexMap);
}
void ubidi_reorderVisual_android(const UBiDiLevel * levels, int32_t length, int32_t * indexMap) {
  ubidi_reorderVisual(levels, length, indexMap);
}
void ubidi_setClassCallback_android(UBiDi * pBiDi, UBiDiClassCallback * newFn, const void * newContext, UBiDiClassCallback ** oldFn, const void ** oldContext, UErrorCode * pErrorCode) {
  ubidi_setClassCallback(pBiDi, newFn, newContext, oldFn, oldContext, pErrorCode);
}
void ubidi_setContext_android(UBiDi * pBiDi, const UChar * prologue, int32_t proLength, const UChar * epilogue, int32_t epiLength, UErrorCode * pErrorCode) {
  ubidi_setContext(pBiDi, prologue, proLength, epilogue, epiLength, pErrorCode);
}
void ubidi_setInverse_android(UBiDi * pBiDi, UBool isInverse) {
  ubidi_setInverse(pBiDi, isInverse);
}
void ubidi_setLine_android(const UBiDi * pParaBiDi, int32_t start, int32_t limit, UBiDi * pLineBiDi, UErrorCode * pErrorCode) {
  ubidi_setLine(pParaBiDi, start, limit, pLineBiDi, pErrorCode);
}
void ubidi_setPara_android(UBiDi * pBiDi, const UChar * text, int32_t length, UBiDiLevel paraLevel, UBiDiLevel * embeddingLevels, UErrorCode * pErrorCode) {
  ubidi_setPara(pBiDi, text, length, paraLevel, embeddingLevels, pErrorCode);
}
void ubidi_setReorderingMode_android(UBiDi * pBiDi, UBiDiReorderingMode reorderingMode) {
  ubidi_setReorderingMode(pBiDi, reorderingMode);
}
void ubidi_setReorderingOptions_android(UBiDi * pBiDi, uint32_t reorderingOptions) {
  ubidi_setReorderingOptions(pBiDi, reorderingOptions);
}
int32_t ubidi_writeReordered_android(UBiDi * pBiDi, UChar * dest, int32_t destSize, uint16_t options, UErrorCode * pErrorCode) {
  return ubidi_writeReordered(pBiDi, dest, destSize, options, pErrorCode);
}
int32_t ubidi_writeReverse_android(const UChar * src, int32_t srcLength, UChar * dest, int32_t destSize, uint16_t options, UErrorCode * pErrorCode) {
  return ubidi_writeReverse(src, srcLength, dest, destSize, options, pErrorCode);
}
void ubiditransform_close_android(UBiDiTransform * pBidiTransform) {
  ubiditransform_close(pBidiTransform);
}
UBiDiTransform * ubiditransform_open_android(UErrorCode * pErrorCode) {
  return ubiditransform_open(pErrorCode);
}
uint32_t ubiditransform_transform_android(UBiDiTransform * pBiDiTransform, const UChar * src, int32_t srcLength, UChar * dest, int32_t destSize, UBiDiLevel inParaLevel, UBiDiOrder inOrder, UBiDiLevel outParaLevel, UBiDiOrder outOrder, UBiDiMirroring doMirroring, uint32_t shapingOptions, UErrorCode * pErrorCode) {
  return ubiditransform_transform(pBiDiTransform, src, srcLength, dest, destSize, inParaLevel, inOrder, outParaLevel, outOrder, doMirroring, shapingOptions, pErrorCode);
}
UBlockCode ublock_getCode_android(UChar32 c) {
  return ublock_getCode(c);
}
void ubrk_close_android(UBreakIterator * bi) {
  ubrk_close(bi);
}
int32_t ubrk_countAvailable_android() {
  return ubrk_countAvailable();
}
int32_t ubrk_current_android(const UBreakIterator * bi) {
  return ubrk_current(bi);
}
int32_t ubrk_first_android(UBreakIterator * bi) {
  return ubrk_first(bi);
}
int32_t ubrk_following_android(UBreakIterator * bi, int32_t offset) {
  return ubrk_following(bi, offset);
}
const char * ubrk_getAvailable_android(int32_t index) {
  return ubrk_getAvailable(index);
}
int32_t ubrk_getBinaryRules_android(UBreakIterator * bi, uint8_t * binaryRules, int32_t rulesCapacity, UErrorCode * status) {
  return ubrk_getBinaryRules(bi, binaryRules, rulesCapacity, status);
}
const char * ubrk_getLocaleByType_android(const UBreakIterator * bi, ULocDataLocaleType type, UErrorCode * status) {
  return ubrk_getLocaleByType(bi, type, status);
}
int32_t ubrk_getRuleStatus_android(UBreakIterator * bi) {
  return ubrk_getRuleStatus(bi);
}
int32_t ubrk_getRuleStatusVec_android(UBreakIterator * bi, int32_t * fillInVec, int32_t capacity, UErrorCode * status) {
  return ubrk_getRuleStatusVec(bi, fillInVec, capacity, status);
}
UBool ubrk_isBoundary_android(UBreakIterator * bi, int32_t offset) {
  return ubrk_isBoundary(bi, offset);
}
int32_t ubrk_last_android(UBreakIterator * bi) {
  return ubrk_last(bi);
}
int32_t ubrk_next_android(UBreakIterator * bi) {
  return ubrk_next(bi);
}
UBreakIterator * ubrk_open_android(UBreakIteratorType type, const char * locale, const UChar * text, int32_t textLength, UErrorCode * status) {
  return ubrk_open(type, locale, text, textLength, status);
}
UBreakIterator * ubrk_openBinaryRules_android(const uint8_t * binaryRules, int32_t rulesLength, const UChar * text, int32_t textLength, UErrorCode * status) {
  return ubrk_openBinaryRules(binaryRules, rulesLength, text, textLength, status);
}
UBreakIterator * ubrk_openRules_android(const UChar * rules, int32_t rulesLength, const UChar * text, int32_t textLength, UParseError * parseErr, UErrorCode * status) {
  return ubrk_openRules(rules, rulesLength, text, textLength, parseErr, status);
}
int32_t ubrk_preceding_android(UBreakIterator * bi, int32_t offset) {
  return ubrk_preceding(bi, offset);
}
int32_t ubrk_previous_android(UBreakIterator * bi) {
  return ubrk_previous(bi);
}
void ubrk_refreshUText_android(UBreakIterator * bi, UText * text, UErrorCode * status) {
  ubrk_refreshUText(bi, text, status);
}
UBreakIterator * ubrk_safeClone_android(const UBreakIterator * bi, void * stackBuffer, int32_t * pBufferSize, UErrorCode * status) {
  return ubrk_safeClone(bi, stackBuffer, pBufferSize, status);
}
void ubrk_setText_android(UBreakIterator * bi, const UChar * text, int32_t textLength, UErrorCode * status) {
  ubrk_setText(bi, text, textLength, status);
}
void ubrk_setUText_android(UBreakIterator * bi, UText * text, UErrorCode * status) {
  ubrk_setUText(bi, text, status);
}
void ucal_add_android(UCalendar * cal, UCalendarDateFields field, int32_t amount, UErrorCode * status) {
  ucal_add(cal, field, amount, status);
}
void ucal_clear_android(UCalendar * calendar) {
  ucal_clear(calendar);
}
void ucal_clearField_android(UCalendar * cal, UCalendarDateFields field) {
  ucal_clearField(cal, field);
}
UCalendar * ucal_clone_android(const UCalendar * cal, UErrorCode * status) {
  return ucal_clone(cal, status);
}
void ucal_close_android(UCalendar * cal) {
  ucal_close(cal);
}
int32_t ucal_countAvailable_android() {
  return ucal_countAvailable();
}
UBool ucal_equivalentTo_android(const UCalendar * cal1, const UCalendar * cal2) {
  return ucal_equivalentTo(cal1, cal2);
}
int32_t ucal_get_android(const UCalendar * cal, UCalendarDateFields field, UErrorCode * status) {
  return ucal_get(cal, field, status);
}
int32_t ucal_getAttribute_android(const UCalendar * cal, UCalendarAttribute attr) {
  return ucal_getAttribute(cal, attr);
}
const char * ucal_getAvailable_android(int32_t localeIndex) {
  return ucal_getAvailable(localeIndex);
}
int32_t ucal_getCanonicalTimeZoneID_android(const UChar * id, int32_t len, UChar * result, int32_t resultCapacity, UBool * isSystemID, UErrorCode * status) {
  return ucal_getCanonicalTimeZoneID(id, len, result, resultCapacity, isSystemID, status);
}
int32_t ucal_getDSTSavings_android(const UChar * zoneID, UErrorCode * ec) {
  return ucal_getDSTSavings(zoneID, ec);
}
UCalendarWeekdayType ucal_getDayOfWeekType_android(const UCalendar * cal, UCalendarDaysOfWeek dayOfWeek, UErrorCode * status) {
  return ucal_getDayOfWeekType(cal, dayOfWeek, status);
}
int32_t ucal_getDefaultTimeZone_android(UChar * result, int32_t resultCapacity, UErrorCode * ec) {
  return ucal_getDefaultTimeZone(result, resultCapacity, ec);
}
int32_t ucal_getFieldDifference_android(UCalendar * cal, UDate target, UCalendarDateFields field, UErrorCode * status) {
  return ucal_getFieldDifference(cal, target, field, status);
}
UDate ucal_getGregorianChange_android(const UCalendar * cal, UErrorCode * pErrorCode) {
  return ucal_getGregorianChange(cal, pErrorCode);
}
UEnumeration * ucal_getKeywordValuesForLocale_android(const char * key, const char * locale, UBool commonlyUsed, UErrorCode * status) {
  return ucal_getKeywordValuesForLocale(key, locale, commonlyUsed, status);
}
int32_t ucal_getLimit_android(const UCalendar * cal, UCalendarDateFields field, UCalendarLimitType type, UErrorCode * status) {
  return ucal_getLimit(cal, field, type, status);
}
const char * ucal_getLocaleByType_android(const UCalendar * cal, ULocDataLocaleType type, UErrorCode * status) {
  return ucal_getLocaleByType(cal, type, status);
}
UDate ucal_getMillis_android(const UCalendar * cal, UErrorCode * status) {
  return ucal_getMillis(cal, status);
}
UDate ucal_getNow_android() {
  return ucal_getNow();
}
const char * ucal_getTZDataVersion_android(UErrorCode * status) {
  return ucal_getTZDataVersion(status);
}
int32_t ucal_getTimeZoneDisplayName_android(const UCalendar * cal, UCalendarDisplayNameType type, const char * locale, UChar * result, int32_t resultLength, UErrorCode * status) {
  return ucal_getTimeZoneDisplayName(cal, type, locale, result, resultLength, status);
}
int32_t ucal_getTimeZoneID_android(const UCalendar * cal, UChar * result, int32_t resultLength, UErrorCode * status) {
  return ucal_getTimeZoneID(cal, result, resultLength, status);
}
int32_t ucal_getTimeZoneIDForWindowsID_android(const UChar * winid, int32_t len, const char * region, UChar * id, int32_t idCapacity, UErrorCode * status) {
  return ucal_getTimeZoneIDForWindowsID(winid, len, region, id, idCapacity, status);
}
UBool ucal_getTimeZoneTransitionDate_android(const UCalendar * cal, UTimeZoneTransitionType type, UDate * transition, UErrorCode * status) {
  return ucal_getTimeZoneTransitionDate(cal, type, transition, status);
}
const char * ucal_getType_android(const UCalendar * cal, UErrorCode * status) {
  return ucal_getType(cal, status);
}
int32_t ucal_getWeekendTransition_android(const UCalendar * cal, UCalendarDaysOfWeek dayOfWeek, UErrorCode * status) {
  return ucal_getWeekendTransition(cal, dayOfWeek, status);
}
int32_t ucal_getWindowsTimeZoneID_android(const UChar * id, int32_t len, UChar * winid, int32_t winidCapacity, UErrorCode * status) {
  return ucal_getWindowsTimeZoneID(id, len, winid, winidCapacity, status);
}
UBool ucal_inDaylightTime_android(const UCalendar * cal, UErrorCode * status) {
  return ucal_inDaylightTime(cal, status);
}
UBool ucal_isSet_android(const UCalendar * cal, UCalendarDateFields field) {
  return ucal_isSet(cal, field);
}
UBool ucal_isWeekend_android(const UCalendar * cal, UDate date, UErrorCode * status) {
  return ucal_isWeekend(cal, date, status);
}
UCalendar * ucal_open_android(const UChar * zoneID, int32_t len, const char * locale, UCalendarType type, UErrorCode * status) {
  return ucal_open(zoneID, len, locale, type, status);
}
UEnumeration * ucal_openCountryTimeZones_android(const char * country, UErrorCode * ec) {
  return ucal_openCountryTimeZones(country, ec);
}
UEnumeration * ucal_openTimeZoneIDEnumeration_android(USystemTimeZoneType zoneType, const char * region, const int32_t * rawOffset, UErrorCode * ec) {
  return ucal_openTimeZoneIDEnumeration(zoneType, region, rawOffset, ec);
}
UEnumeration * ucal_openTimeZones_android(UErrorCode * ec) {
  return ucal_openTimeZones(ec);
}
void ucal_roll_android(UCalendar * cal, UCalendarDateFields field, int32_t amount, UErrorCode * status) {
  ucal_roll(cal, field, amount, status);
}
void ucal_set_android(UCalendar * cal, UCalendarDateFields field, int32_t value) {
  ucal_set(cal, field, value);
}
void ucal_setAttribute_android(UCalendar * cal, UCalendarAttribute attr, int32_t newValue) {
  ucal_setAttribute(cal, attr, newValue);
}
void ucal_setDate_android(UCalendar * cal, int32_t year, int32_t month, int32_t date, UErrorCode * status) {
  ucal_setDate(cal, year, month, date, status);
}
void ucal_setDateTime_android(UCalendar * cal, int32_t year, int32_t month, int32_t date, int32_t hour, int32_t minute, int32_t second, UErrorCode * status) {
  ucal_setDateTime(cal, year, month, date, hour, minute, second, status);
}
void ucal_setDefaultTimeZone_android(const UChar * zoneID, UErrorCode * ec) {
  ucal_setDefaultTimeZone(zoneID, ec);
}
void ucal_setGregorianChange_android(UCalendar * cal, UDate date, UErrorCode * pErrorCode) {
  ucal_setGregorianChange(cal, date, pErrorCode);
}
void ucal_setMillis_android(UCalendar * cal, UDate dateTime, UErrorCode * status) {
  ucal_setMillis(cal, dateTime, status);
}
void ucal_setTimeZone_android(UCalendar * cal, const UChar * zoneID, int32_t len, UErrorCode * status) {
  ucal_setTimeZone(cal, zoneID, len, status);
}
void ucasemap_close_android(UCaseMap * csm) {
  ucasemap_close(csm);
}
const UBreakIterator * ucasemap_getBreakIterator_android(const UCaseMap * csm) {
  return ucasemap_getBreakIterator(csm);
}
const char * ucasemap_getLocale_android(const UCaseMap * csm) {
  return ucasemap_getLocale(csm);
}
uint32_t ucasemap_getOptions_android(const UCaseMap * csm) {
  return ucasemap_getOptions(csm);
}
UCaseMap * ucasemap_open_android(const char * locale, uint32_t options, UErrorCode * pErrorCode) {
  return ucasemap_open(locale, options, pErrorCode);
}
void ucasemap_setBreakIterator_android(UCaseMap * csm, UBreakIterator * iterToAdopt, UErrorCode * pErrorCode) {
  ucasemap_setBreakIterator(csm, iterToAdopt, pErrorCode);
}
void ucasemap_setLocale_android(UCaseMap * csm, const char * locale, UErrorCode * pErrorCode) {
  ucasemap_setLocale(csm, locale, pErrorCode);
}
void ucasemap_setOptions_android(UCaseMap * csm, uint32_t options, UErrorCode * pErrorCode) {
  ucasemap_setOptions(csm, options, pErrorCode);
}
int32_t ucasemap_toTitle_android(UCaseMap * csm, UChar * dest, int32_t destCapacity, const UChar * src, int32_t srcLength, UErrorCode * pErrorCode) {
  return ucasemap_toTitle(csm, dest, destCapacity, src, srcLength, pErrorCode);
}
int32_t ucasemap_utf8FoldCase_android(const UCaseMap * csm, char * dest, int32_t destCapacity, const char * src, int32_t srcLength, UErrorCode * pErrorCode) {
  return ucasemap_utf8FoldCase(csm, dest, destCapacity, src, srcLength, pErrorCode);
}
int32_t ucasemap_utf8ToLower_android(const UCaseMap * csm, char * dest, int32_t destCapacity, const char * src, int32_t srcLength, UErrorCode * pErrorCode) {
  return ucasemap_utf8ToLower(csm, dest, destCapacity, src, srcLength, pErrorCode);
}
int32_t ucasemap_utf8ToTitle_android(UCaseMap * csm, char * dest, int32_t destCapacity, const char * src, int32_t srcLength, UErrorCode * pErrorCode) {
  return ucasemap_utf8ToTitle(csm, dest, destCapacity, src, srcLength, pErrorCode);
}
int32_t ucasemap_utf8ToUpper_android(const UCaseMap * csm, char * dest, int32_t destCapacity, const char * src, int32_t srcLength, UErrorCode * pErrorCode) {
  return ucasemap_utf8ToUpper(csm, dest, destCapacity, src, srcLength, pErrorCode);
}
void ucnv_cbFromUWriteBytes_android(UConverterFromUnicodeArgs * args, const char * source, int32_t length, int32_t offsetIndex, UErrorCode * err) {
  ucnv_cbFromUWriteBytes(args, source, length, offsetIndex, err);
}
void ucnv_cbFromUWriteSub_android(UConverterFromUnicodeArgs * args, int32_t offsetIndex, UErrorCode * err) {
  ucnv_cbFromUWriteSub(args, offsetIndex, err);
}
void ucnv_cbFromUWriteUChars_android(UConverterFromUnicodeArgs * args, const UChar ** source, const UChar * sourceLimit, int32_t offsetIndex, UErrorCode * err) {
  ucnv_cbFromUWriteUChars(args, source, sourceLimit, offsetIndex, err);
}
void ucnv_cbToUWriteSub_android(UConverterToUnicodeArgs * args, int32_t offsetIndex, UErrorCode * err) {
  ucnv_cbToUWriteSub(args, offsetIndex, err);
}
void ucnv_cbToUWriteUChars_android(UConverterToUnicodeArgs * args, const UChar * source, int32_t length, int32_t offsetIndex, UErrorCode * err) {
  ucnv_cbToUWriteUChars(args, source, length, offsetIndex, err);
}
void ucnv_close_android(UConverter * converter) {
  ucnv_close(converter);
}
int ucnv_compareNames_android(const char * name1, const char * name2) {
  return ucnv_compareNames(name1, name2);
}
int32_t ucnv_convert_android(const char * toConverterName, const char * fromConverterName, char * target, int32_t targetCapacity, const char * source, int32_t sourceLength, UErrorCode * pErrorCode) {
  return ucnv_convert(toConverterName, fromConverterName, target, targetCapacity, source, sourceLength, pErrorCode);
}
void ucnv_convertEx_android(UConverter * targetCnv, UConverter * sourceCnv, char ** target, const char * targetLimit, const char ** source, const char * sourceLimit, UChar * pivotStart, UChar ** pivotSource, UChar ** pivotTarget, const UChar * pivotLimit, UBool reset, UBool flush, UErrorCode * pErrorCode) {
  ucnv_convertEx(targetCnv, sourceCnv, target, targetLimit, source, sourceLimit, pivotStart, pivotSource, pivotTarget, pivotLimit, reset, flush, pErrorCode);
}
uint16_t ucnv_countAliases_android(const char * alias, UErrorCode * pErrorCode) {
  return ucnv_countAliases(alias, pErrorCode);
}
int32_t ucnv_countAvailable_android() {
  return ucnv_countAvailable();
}
uint16_t ucnv_countStandards_android() {
  return ucnv_countStandards();
}
const char * ucnv_detectUnicodeSignature_android(const char * source, int32_t sourceLength, int32_t * signatureLength, UErrorCode * pErrorCode) {
  return ucnv_detectUnicodeSignature(source, sourceLength, signatureLength, pErrorCode);
}
void ucnv_fixFileSeparator_android(const UConverter * cnv, UChar * source, int32_t sourceLen) {
  ucnv_fixFileSeparator(cnv, source, sourceLen);
}
int32_t ucnv_flushCache_android() {
  return ucnv_flushCache();
}
int32_t ucnv_fromAlgorithmic_android(UConverter * cnv, UConverterType algorithmicType, char * target, int32_t targetCapacity, const char * source, int32_t sourceLength, UErrorCode * pErrorCode) {
  return ucnv_fromAlgorithmic(cnv, algorithmicType, target, targetCapacity, source, sourceLength, pErrorCode);
}
int32_t ucnv_fromUChars_android(UConverter * cnv, char * dest, int32_t destCapacity, const UChar * src, int32_t srcLength, UErrorCode * pErrorCode) {
  return ucnv_fromUChars(cnv, dest, destCapacity, src, srcLength, pErrorCode);
}
int32_t ucnv_fromUCountPending_android(const UConverter * cnv, UErrorCode * status) {
  return ucnv_fromUCountPending(cnv, status);
}
void ucnv_fromUnicode_android(UConverter * converter, char ** target, const char * targetLimit, const UChar ** source, const UChar * sourceLimit, int32_t * offsets, UBool flush, UErrorCode * err) {
  ucnv_fromUnicode(converter, target, targetLimit, source, sourceLimit, offsets, flush, err);
}
const char * ucnv_getAlias_android(const char * alias, uint16_t n, UErrorCode * pErrorCode) {
  return ucnv_getAlias(alias, n, pErrorCode);
}
void ucnv_getAliases_android(const char * alias, const char ** aliases, UErrorCode * pErrorCode) {
  ucnv_getAliases(alias, aliases, pErrorCode);
}
const char * ucnv_getAvailableName_android(int32_t n) {
  return ucnv_getAvailableName(n);
}
int32_t ucnv_getCCSID_android(const UConverter * converter, UErrorCode * err) {
  return ucnv_getCCSID(converter, err);
}
const char * ucnv_getCanonicalName_android(const char * alias, const char * standard, UErrorCode * pErrorCode) {
  return ucnv_getCanonicalName(alias, standard, pErrorCode);
}
const char * ucnv_getDefaultName_android() {
  return ucnv_getDefaultName();
}
int32_t ucnv_getDisplayName_android(const UConverter * converter, const char * displayLocale, UChar * displayName, int32_t displayNameCapacity, UErrorCode * err) {
  return ucnv_getDisplayName(converter, displayLocale, displayName, displayNameCapacity, err);
}
void ucnv_getFromUCallBack_android(const UConverter * converter, UConverterFromUCallback * action, const void ** context) {
  ucnv_getFromUCallBack(converter, action, context);
}
void ucnv_getInvalidChars_android(const UConverter * converter, char * errBytes, int8_t * len, UErrorCode * err) {
  ucnv_getInvalidChars(converter, errBytes, len, err);
}
void ucnv_getInvalidUChars_android(const UConverter * converter, UChar * errUChars, int8_t * len, UErrorCode * err) {
  ucnv_getInvalidUChars(converter, errUChars, len, err);
}
int8_t ucnv_getMaxCharSize_android(const UConverter * converter) {
  return ucnv_getMaxCharSize(converter);
}
int8_t ucnv_getMinCharSize_android(const UConverter * converter) {
  return ucnv_getMinCharSize(converter);
}
const char * ucnv_getName_android(const UConverter * converter, UErrorCode * err) {
  return ucnv_getName(converter, err);
}
UChar32 ucnv_getNextUChar_android(UConverter * converter, const char ** source, const char * sourceLimit, UErrorCode * err) {
  return ucnv_getNextUChar(converter, source, sourceLimit, err);
}
UConverterPlatform ucnv_getPlatform_android(const UConverter * converter, UErrorCode * err) {
  return ucnv_getPlatform(converter, err);
}
const char * ucnv_getStandard_android(uint16_t n, UErrorCode * pErrorCode) {
  return ucnv_getStandard(n, pErrorCode);
}
const char * ucnv_getStandardName_android(const char * name, const char * standard, UErrorCode * pErrorCode) {
  return ucnv_getStandardName(name, standard, pErrorCode);
}
void ucnv_getStarters_android(const UConverter * converter, UBool  starters[256], UErrorCode * err) {
  ucnv_getStarters(converter, starters, err);
}
void ucnv_getSubstChars_android(const UConverter * converter, char * subChars, int8_t * len, UErrorCode * err) {
  ucnv_getSubstChars(converter, subChars, len, err);
}
void ucnv_getToUCallBack_android(const UConverter * converter, UConverterToUCallback * action, const void ** context) {
  ucnv_getToUCallBack(converter, action, context);
}
UConverterType ucnv_getType_android(const UConverter * converter) {
  return ucnv_getType(converter);
}
void ucnv_getUnicodeSet_android(const UConverter * cnv, USet * setFillIn, UConverterUnicodeSet whichSet, UErrorCode * pErrorCode) {
  ucnv_getUnicodeSet(cnv, setFillIn, whichSet, pErrorCode);
}
UBool ucnv_isAmbiguous_android(const UConverter * cnv) {
  return ucnv_isAmbiguous(cnv);
}
UBool ucnv_isFixedWidth_android(UConverter * cnv, UErrorCode * status) {
  return ucnv_isFixedWidth(cnv, status);
}
UConverter * ucnv_open_android(const char * converterName, UErrorCode * err) {
  return ucnv_open(converterName, err);
}
UEnumeration * ucnv_openAllNames_android(UErrorCode * pErrorCode) {
  return ucnv_openAllNames(pErrorCode);
}
UConverter * ucnv_openCCSID_android(int32_t codepage, UConverterPlatform platform, UErrorCode * err) {
  return ucnv_openCCSID(codepage, platform, err);
}
UConverter * ucnv_openPackage_android(const char * packageName, const char * converterName, UErrorCode * err) {
  return ucnv_openPackage(packageName, converterName, err);
}
UEnumeration * ucnv_openStandardNames_android(const char * convName, const char * standard, UErrorCode * pErrorCode) {
  return ucnv_openStandardNames(convName, standard, pErrorCode);
}
UConverter * ucnv_openU_android(const UChar * name, UErrorCode * err) {
  return ucnv_openU(name, err);
}
void ucnv_reset_android(UConverter * converter) {
  ucnv_reset(converter);
}
void ucnv_resetFromUnicode_android(UConverter * converter) {
  ucnv_resetFromUnicode(converter);
}
void ucnv_resetToUnicode_android(UConverter * converter) {
  ucnv_resetToUnicode(converter);
}
UConverter * ucnv_safeClone_android(const UConverter * cnv, void * stackBuffer, int32_t * pBufferSize, UErrorCode * status) {
  return ucnv_safeClone(cnv, stackBuffer, pBufferSize, status);
}
void ucnv_setDefaultName_android(const char * name) {
  ucnv_setDefaultName(name);
}
void ucnv_setFallback_android(UConverter * cnv, UBool usesFallback) {
  ucnv_setFallback(cnv, usesFallback);
}
void ucnv_setFromUCallBack_android(UConverter * converter, UConverterFromUCallback newAction, const void * newContext, UConverterFromUCallback * oldAction, const void ** oldContext, UErrorCode * err) {
  ucnv_setFromUCallBack(converter, newAction, newContext, oldAction, oldContext, err);
}
void ucnv_setSubstChars_android(UConverter * converter, const char * subChars, int8_t len, UErrorCode * err) {
  ucnv_setSubstChars(converter, subChars, len, err);
}
void ucnv_setSubstString_android(UConverter * cnv, const UChar * s, int32_t length, UErrorCode * err) {
  ucnv_setSubstString(cnv, s, length, err);
}
void ucnv_setToUCallBack_android(UConverter * converter, UConverterToUCallback newAction, const void * newContext, UConverterToUCallback * oldAction, const void ** oldContext, UErrorCode * err) {
  ucnv_setToUCallBack(converter, newAction, newContext, oldAction, oldContext, err);
}
int32_t ucnv_toAlgorithmic_android(UConverterType algorithmicType, UConverter * cnv, char * target, int32_t targetCapacity, const char * source, int32_t sourceLength, UErrorCode * pErrorCode) {
  return ucnv_toAlgorithmic(algorithmicType, cnv, target, targetCapacity, source, sourceLength, pErrorCode);
}
int32_t ucnv_toUChars_android(UConverter * cnv, UChar * dest, int32_t destCapacity, const char * src, int32_t srcLength, UErrorCode * pErrorCode) {
  return ucnv_toUChars(cnv, dest, destCapacity, src, srcLength, pErrorCode);
}
int32_t ucnv_toUCountPending_android(const UConverter * cnv, UErrorCode * status) {
  return ucnv_toUCountPending(cnv, status);
}
void ucnv_toUnicode_android(UConverter * converter, UChar ** target, const UChar * targetLimit, const char ** source, const char * sourceLimit, int32_t * offsets, UBool flush, UErrorCode * err) {
  ucnv_toUnicode(converter, target, targetLimit, source, sourceLimit, offsets, flush, err);
}
UBool ucnv_usesFallback_android(const UConverter * cnv) {
  return ucnv_usesFallback(cnv);
}
void ucnvsel_close_android(UConverterSelector * sel) {
  ucnvsel_close(sel);
}
UConverterSelector * ucnvsel_open_android(const char *const * converterList, int32_t converterListSize, const USet * excludedCodePoints, const UConverterUnicodeSet whichSet, UErrorCode * status) {
  return ucnvsel_open(converterList, converterListSize, excludedCodePoints, whichSet, status);
}
UConverterSelector * ucnvsel_openFromSerialized_android(const void * buffer, int32_t length, UErrorCode * status) {
  return ucnvsel_openFromSerialized(buffer, length, status);
}
UEnumeration * ucnvsel_selectForString_android(const UConverterSelector * sel, const UChar * s, int32_t length, UErrorCode * status) {
  return ucnvsel_selectForString(sel, s, length, status);
}
UEnumeration * ucnvsel_selectForUTF8_android(const UConverterSelector * sel, const char * s, int32_t length, UErrorCode * status) {
  return ucnvsel_selectForUTF8(sel, s, length, status);
}
int32_t ucnvsel_serialize_android(const UConverterSelector * sel, void * buffer, int32_t bufferCapacity, UErrorCode * status) {
  return ucnvsel_serialize(sel, buffer, bufferCapacity, status);
}
int32_t ucol_cloneBinary_android(const UCollator * coll, uint8_t * buffer, int32_t capacity, UErrorCode * status) {
  return ucol_cloneBinary(coll, buffer, capacity, status);
}
void ucol_close_android(UCollator * coll) {
  ucol_close(coll);
}
void ucol_closeElements_android(UCollationElements * elems) {
  ucol_closeElements(elems);
}
int32_t ucol_countAvailable_android() {
  return ucol_countAvailable();
}
UBool ucol_equal_android(const UCollator * coll, const UChar * source, int32_t sourceLength, const UChar * target, int32_t targetLength) {
  return ucol_equal(coll, source, sourceLength, target, targetLength);
}
UColAttributeValue ucol_getAttribute_android(const UCollator * coll, UColAttribute attr, UErrorCode * status) {
  return ucol_getAttribute(coll, attr, status);
}
const char * ucol_getAvailable_android(int32_t localeIndex) {
  return ucol_getAvailable(localeIndex);
}
int32_t ucol_getBound_android(const uint8_t * source, int32_t sourceLength, UColBoundMode boundType, uint32_t noOfLevels, uint8_t * result, int32_t resultLength, UErrorCode * status) {
  return ucol_getBound(source, sourceLength, boundType, noOfLevels, result, resultLength, status);
}
void ucol_getContractionsAndExpansions_android(const UCollator * coll, USet * contractions, USet * expansions, UBool addPrefixes, UErrorCode * status) {
  ucol_getContractionsAndExpansions(coll, contractions, expansions, addPrefixes, status);
}
int32_t ucol_getDisplayName_android(const char * objLoc, const char * dispLoc, UChar * result, int32_t resultLength, UErrorCode * status) {
  return ucol_getDisplayName(objLoc, dispLoc, result, resultLength, status);
}
int32_t ucol_getEquivalentReorderCodes_android(int32_t reorderCode, int32_t * dest, int32_t destCapacity, UErrorCode * pErrorCode) {
  return ucol_getEquivalentReorderCodes(reorderCode, dest, destCapacity, pErrorCode);
}
int32_t ucol_getFunctionalEquivalent_android(char * result, int32_t resultCapacity, const char * keyword, const char * locale, UBool * isAvailable, UErrorCode * status) {
  return ucol_getFunctionalEquivalent(result, resultCapacity, keyword, locale, isAvailable, status);
}
UEnumeration * ucol_getKeywordValues_android(const char * keyword, UErrorCode * status) {
  return ucol_getKeywordValues(keyword, status);
}
UEnumeration * ucol_getKeywordValuesForLocale_android(const char * key, const char * locale, UBool commonlyUsed, UErrorCode * status) {
  return ucol_getKeywordValuesForLocale(key, locale, commonlyUsed, status);
}
UEnumeration * ucol_getKeywords_android(UErrorCode * status) {
  return ucol_getKeywords(status);
}
const char * ucol_getLocaleByType_android(const UCollator * coll, ULocDataLocaleType type, UErrorCode * status) {
  return ucol_getLocaleByType(coll, type, status);
}
int32_t ucol_getMaxExpansion_android(const UCollationElements * elems, int32_t order) {
  return ucol_getMaxExpansion(elems, order);
}
UColReorderCode ucol_getMaxVariable_android(const UCollator * coll) {
  return ucol_getMaxVariable(coll);
}
int32_t ucol_getOffset_android(const UCollationElements * elems) {
  return ucol_getOffset(elems);
}
int32_t ucol_getReorderCodes_android(const UCollator * coll, int32_t * dest, int32_t destCapacity, UErrorCode * pErrorCode) {
  return ucol_getReorderCodes(coll, dest, destCapacity, pErrorCode);
}
const UChar * ucol_getRules_android(const UCollator * coll, int32_t * length) {
  return ucol_getRules(coll, length);
}
int32_t ucol_getRulesEx_android(const UCollator * coll, UColRuleOption delta, UChar * buffer, int32_t bufferLen) {
  return ucol_getRulesEx(coll, delta, buffer, bufferLen);
}
int32_t ucol_getSortKey_android(const UCollator * coll, const UChar * source, int32_t sourceLength, uint8_t * result, int32_t resultLength) {
  return ucol_getSortKey(coll, source, sourceLength, result, resultLength);
}
UCollationStrength ucol_getStrength_android(const UCollator * coll) {
  return ucol_getStrength(coll);
}
USet * ucol_getTailoredSet_android(const UCollator * coll, UErrorCode * status) {
  return ucol_getTailoredSet(coll, status);
}
void ucol_getUCAVersion_android(const UCollator * coll, UVersionInfo info) {
  ucol_getUCAVersion(coll, info);
}
uint32_t ucol_getVariableTop_android(const UCollator * coll, UErrorCode * status) {
  return ucol_getVariableTop(coll, status);
}
void ucol_getVersion_android(const UCollator * coll, UVersionInfo info) {
  ucol_getVersion(coll, info);
}
UBool ucol_greater_android(const UCollator * coll, const UChar * source, int32_t sourceLength, const UChar * target, int32_t targetLength) {
  return ucol_greater(coll, source, sourceLength, target, targetLength);
}
UBool ucol_greaterOrEqual_android(const UCollator * coll, const UChar * source, int32_t sourceLength, const UChar * target, int32_t targetLength) {
  return ucol_greaterOrEqual(coll, source, sourceLength, target, targetLength);
}
int32_t ucol_keyHashCode_android(const uint8_t * key, int32_t length) {
  return ucol_keyHashCode(key, length);
}
int32_t ucol_mergeSortkeys_android(const uint8_t * src1, int32_t src1Length, const uint8_t * src2, int32_t src2Length, uint8_t * dest, int32_t destCapacity) {
  return ucol_mergeSortkeys(src1, src1Length, src2, src2Length, dest, destCapacity);
}
int32_t ucol_next_android(UCollationElements * elems, UErrorCode * status) {
  return ucol_next(elems, status);
}
int32_t ucol_nextSortKeyPart_android(const UCollator * coll, UCharIterator * iter, uint32_t  state[2], uint8_t * dest, int32_t count, UErrorCode * status) {
  return ucol_nextSortKeyPart(coll, iter, state, dest, count, status);
}
UCollator * ucol_open_android(const char * loc, UErrorCode * status) {
  return ucol_open(loc, status);
}
UEnumeration * ucol_openAvailableLocales_android(UErrorCode * status) {
  return ucol_openAvailableLocales(status);
}
UCollator * ucol_openBinary_android(const uint8_t * bin, int32_t length, const UCollator * base, UErrorCode * status) {
  return ucol_openBinary(bin, length, base, status);
}
UCollationElements * ucol_openElements_android(const UCollator * coll, const UChar * text, int32_t textLength, UErrorCode * status) {
  return ucol_openElements(coll, text, textLength, status);
}
UCollator * ucol_openRules_android(const UChar * rules, int32_t rulesLength, UColAttributeValue normalizationMode, UCollationStrength strength, UParseError * parseError, UErrorCode * status) {
  return ucol_openRules(rules, rulesLength, normalizationMode, strength, parseError, status);
}
int32_t ucol_previous_android(UCollationElements * elems, UErrorCode * status) {
  return ucol_previous(elems, status);
}
int32_t ucol_primaryOrder_android(int32_t order) {
  return ucol_primaryOrder(order);
}
void ucol_reset_android(UCollationElements * elems) {
  ucol_reset(elems);
}
UCollator * ucol_safeClone_android(const UCollator * coll, void * stackBuffer, int32_t * pBufferSize, UErrorCode * status) {
  return ucol_safeClone(coll, stackBuffer, pBufferSize, status);
}
int32_t ucol_secondaryOrder_android(int32_t order) {
  return ucol_secondaryOrder(order);
}
void ucol_setAttribute_android(UCollator * coll, UColAttribute attr, UColAttributeValue value, UErrorCode * status) {
  ucol_setAttribute(coll, attr, value, status);
}
void ucol_setMaxVariable_android(UCollator * coll, UColReorderCode group, UErrorCode * pErrorCode) {
  ucol_setMaxVariable(coll, group, pErrorCode);
}
void ucol_setOffset_android(UCollationElements * elems, int32_t offset, UErrorCode * status) {
  ucol_setOffset(elems, offset, status);
}
void ucol_setReorderCodes_android(UCollator * coll, const int32_t * reorderCodes, int32_t reorderCodesLength, UErrorCode * pErrorCode) {
  ucol_setReorderCodes(coll, reorderCodes, reorderCodesLength, pErrorCode);
}
void ucol_setStrength_android(UCollator * coll, UCollationStrength strength) {
  ucol_setStrength(coll, strength);
}
void ucol_setText_android(UCollationElements * elems, const UChar * text, int32_t textLength, UErrorCode * status) {
  ucol_setText(elems, text, textLength, status);
}
UCollationResult ucol_strcoll_android(const UCollator * coll, const UChar * source, int32_t sourceLength, const UChar * target, int32_t targetLength) {
  return ucol_strcoll(coll, source, sourceLength, target, targetLength);
}
UCollationResult ucol_strcollIter_android(const UCollator * coll, UCharIterator * sIter, UCharIterator * tIter, UErrorCode * status) {
  return ucol_strcollIter(coll, sIter, tIter, status);
}
UCollationResult ucol_strcollUTF8_android(const UCollator * coll, const char * source, int32_t sourceLength, const char * target, int32_t targetLength, UErrorCode * status) {
  return ucol_strcollUTF8(coll, source, sourceLength, target, targetLength, status);
}
int32_t ucol_tertiaryOrder_android(int32_t order) {
  return ucol_tertiaryOrder(order);
}
uint32_t ucpmap_get_android(const UCPMap * map, UChar32 c) {
  return ucpmap_get(map, c);
}
UChar32 ucpmap_getRange_android(const UCPMap * map, UChar32 start, UCPMapRangeOption option, uint32_t surrogateValue, UCPMapValueFilter * filter, const void * context, uint32_t * pValue) {
  return ucpmap_getRange(map, start, option, surrogateValue, filter, context, pValue);
}
void ucptrie_close_android(UCPTrie * trie) {
  ucptrie_close(trie);
}
uint32_t ucptrie_get_android(const UCPTrie * trie, UChar32 c) {
  return ucptrie_get(trie, c);
}
UChar32 ucptrie_getRange_android(const UCPTrie * trie, UChar32 start, UCPMapRangeOption option, uint32_t surrogateValue, UCPMapValueFilter * filter, const void * context, uint32_t * pValue) {
  return ucptrie_getRange(trie, start, option, surrogateValue, filter, context, pValue);
}
UCPTrieType ucptrie_getType_android(const UCPTrie * trie) {
  return ucptrie_getType(trie);
}
UCPTrieValueWidth ucptrie_getValueWidth_android(const UCPTrie * trie) {
  return ucptrie_getValueWidth(trie);
}
UCPTrie * ucptrie_openFromBinary_android(UCPTrieType type, UCPTrieValueWidth valueWidth, const void * data, int32_t length, int32_t * pActualLength, UErrorCode * pErrorCode) {
  return ucptrie_openFromBinary(type, valueWidth, data, length, pActualLength, pErrorCode);
}
int32_t ucptrie_toBinary_android(const UCPTrie * trie, void * data, int32_t capacity, UErrorCode * pErrorCode) {
  return ucptrie_toBinary(trie, data, capacity, pErrorCode);
}
void ucsdet_close_android(UCharsetDetector * ucsd) {
  ucsdet_close(ucsd);
}
const UCharsetMatch * ucsdet_detect_android(UCharsetDetector * ucsd, UErrorCode * status) {
  return ucsdet_detect(ucsd, status);
}
const UCharsetMatch ** ucsdet_detectAll_android(UCharsetDetector * ucsd, int32_t * matchesFound, UErrorCode * status) {
  return ucsdet_detectAll(ucsd, matchesFound, status);
}
UBool ucsdet_enableInputFilter_android(UCharsetDetector * ucsd, UBool filter) {
  return ucsdet_enableInputFilter(ucsd, filter);
}
UEnumeration * ucsdet_getAllDetectableCharsets_android(const UCharsetDetector * ucsd, UErrorCode * status) {
  return ucsdet_getAllDetectableCharsets(ucsd, status);
}
int32_t ucsdet_getConfidence_android(const UCharsetMatch * ucsm, UErrorCode * status) {
  return ucsdet_getConfidence(ucsm, status);
}
const char * ucsdet_getLanguage_android(const UCharsetMatch * ucsm, UErrorCode * status) {
  return ucsdet_getLanguage(ucsm, status);
}
const char * ucsdet_getName_android(const UCharsetMatch * ucsm, UErrorCode * status) {
  return ucsdet_getName(ucsm, status);
}
int32_t ucsdet_getUChars_android(const UCharsetMatch * ucsm, UChar * buf, int32_t cap, UErrorCode * status) {
  return ucsdet_getUChars(ucsm, buf, cap, status);
}
UBool ucsdet_isInputFilterEnabled_android(const UCharsetDetector * ucsd) {
  return ucsdet_isInputFilterEnabled(ucsd);
}
UCharsetDetector * ucsdet_open_android(UErrorCode * status) {
  return ucsdet_open(status);
}
void ucsdet_setDeclaredEncoding_android(UCharsetDetector * ucsd, const char * encoding, int32_t length, UErrorCode * status) {
  ucsdet_setDeclaredEncoding(ucsd, encoding, length, status);
}
void ucsdet_setText_android(UCharsetDetector * ucsd, const char * textIn, int32_t len, UErrorCode * status) {
  ucsdet_setText(ucsd, textIn, len, status);
}
int32_t ucurr_countCurrencies_android(const char * locale, UDate date, UErrorCode * ec) {
  return ucurr_countCurrencies(locale, date, ec);
}
int32_t ucurr_forLocale_android(const char * locale, UChar * buff, int32_t buffCapacity, UErrorCode * ec) {
  return ucurr_forLocale(locale, buff, buffCapacity, ec);
}
int32_t ucurr_forLocaleAndDate_android(const char * locale, UDate date, int32_t index, UChar * buff, int32_t buffCapacity, UErrorCode * ec) {
  return ucurr_forLocaleAndDate(locale, date, index, buff, buffCapacity, ec);
}
int32_t ucurr_getDefaultFractionDigits_android(const UChar * currency, UErrorCode * ec) {
  return ucurr_getDefaultFractionDigits(currency, ec);
}
int32_t ucurr_getDefaultFractionDigitsForUsage_android(const UChar * currency, const UCurrencyUsage usage, UErrorCode * ec) {
  return ucurr_getDefaultFractionDigitsForUsage(currency, usage, ec);
}
UEnumeration * ucurr_getKeywordValuesForLocale_android(const char * key, const char * locale, UBool commonlyUsed, UErrorCode * status) {
  return ucurr_getKeywordValuesForLocale(key, locale, commonlyUsed, status);
}
const UChar * ucurr_getName_android(const UChar * currency, const char * locale, UCurrNameStyle nameStyle, UBool * isChoiceFormat, int32_t * len, UErrorCode * ec) {
  return ucurr_getName(currency, locale, nameStyle, isChoiceFormat, len, ec);
}
int32_t ucurr_getNumericCode_android(const UChar * currency) {
  return ucurr_getNumericCode(currency);
}
const UChar * ucurr_getPluralName_android(const UChar * currency, const char * locale, UBool * isChoiceFormat, const char * pluralCount, int32_t * len, UErrorCode * ec) {
  return ucurr_getPluralName(currency, locale, isChoiceFormat, pluralCount, len, ec);
}
double ucurr_getRoundingIncrement_android(const UChar * currency, UErrorCode * ec) {
  return ucurr_getRoundingIncrement(currency, ec);
}
double ucurr_getRoundingIncrementForUsage_android(const UChar * currency, const UCurrencyUsage usage, UErrorCode * ec) {
  return ucurr_getRoundingIncrementForUsage(currency, usage, ec);
}
UBool ucurr_isAvailable_android(const UChar * isoCode, UDate from, UDate to, UErrorCode * errorCode) {
  return ucurr_isAvailable(isoCode, from, to, errorCode);
}
UEnumeration * ucurr_openISOCurrencies_android(uint32_t currType, UErrorCode * pErrorCode) {
  return ucurr_openISOCurrencies(currType, pErrorCode);
}
UCurrRegistryKey ucurr_register_android(const UChar * isoCode, const char * locale, UErrorCode * status) {
  return ucurr_register(isoCode, locale, status);
}
UBool ucurr_unregister_android(UCurrRegistryKey key, UErrorCode * status) {
  return ucurr_unregister(key, status);
}
void udat_adoptNumberFormat_android(UDateFormat * fmt, UNumberFormat * numberFormatToAdopt) {
  udat_adoptNumberFormat(fmt, numberFormatToAdopt);
}
void udat_adoptNumberFormatForFields_android(UDateFormat * fmt, const UChar * fields, UNumberFormat * numberFormatToSet, UErrorCode * status) {
  udat_adoptNumberFormatForFields(fmt, fields, numberFormatToSet, status);
}
void udat_applyPattern_android(UDateFormat * format, UBool localized, const UChar * pattern, int32_t patternLength) {
  udat_applyPattern(format, localized, pattern, patternLength);
}
UDateFormat * udat_clone_android(const UDateFormat * fmt, UErrorCode * status) {
  return udat_clone(fmt, status);
}
void udat_close_android(UDateFormat * format) {
  udat_close(format);
}
int32_t udat_countAvailable_android() {
  return udat_countAvailable();
}
int32_t udat_countSymbols_android(const UDateFormat * fmt, UDateFormatSymbolType type) {
  return udat_countSymbols(fmt, type);
}
int32_t udat_format_android(const UDateFormat * format, UDate dateToFormat, UChar * result, int32_t resultLength, UFieldPosition * position, UErrorCode * status) {
  return udat_format(format, dateToFormat, result, resultLength, position, status);
}
int32_t udat_formatCalendar_android(const UDateFormat * format, UCalendar * calendar, UChar * result, int32_t capacity, UFieldPosition * position, UErrorCode * status) {
  return udat_formatCalendar(format, calendar, result, capacity, position, status);
}
int32_t udat_formatCalendarForFields_android(const UDateFormat * format, UCalendar * calendar, UChar * result, int32_t capacity, UFieldPositionIterator * fpositer, UErrorCode * status) {
  return udat_formatCalendarForFields(format, calendar, result, capacity, fpositer, status);
}
int32_t udat_formatForFields_android(const UDateFormat * format, UDate dateToFormat, UChar * result, int32_t resultLength, UFieldPositionIterator * fpositer, UErrorCode * status) {
  return udat_formatForFields(format, dateToFormat, result, resultLength, fpositer, status);
}
UDate udat_get2DigitYearStart_android(const UDateFormat * fmt, UErrorCode * status) {
  return udat_get2DigitYearStart(fmt, status);
}
const char * udat_getAvailable_android(int32_t localeIndex) {
  return udat_getAvailable(localeIndex);
}
UBool udat_getBooleanAttribute_android(const UDateFormat * fmt, UDateFormatBooleanAttribute attr, UErrorCode * status) {
  return udat_getBooleanAttribute(fmt, attr, status);
}
const UCalendar * udat_getCalendar_android(const UDateFormat * fmt) {
  return udat_getCalendar(fmt);
}
UDisplayContext udat_getContext_android(const UDateFormat * fmt, UDisplayContextType type, UErrorCode * status) {
  return udat_getContext(fmt, type, status);
}
const char * udat_getLocaleByType_android(const UDateFormat * fmt, ULocDataLocaleType type, UErrorCode * status) {
  return udat_getLocaleByType(fmt, type, status);
}
const UNumberFormat * udat_getNumberFormat_android(const UDateFormat * fmt) {
  return udat_getNumberFormat(fmt);
}
const UNumberFormat * udat_getNumberFormatForField_android(const UDateFormat * fmt, UChar field) {
  return udat_getNumberFormatForField(fmt, field);
}
int32_t udat_getSymbols_android(const UDateFormat * fmt, UDateFormatSymbolType type, int32_t symbolIndex, UChar * result, int32_t resultLength, UErrorCode * status) {
  return udat_getSymbols(fmt, type, symbolIndex, result, resultLength, status);
}
UBool udat_isLenient_android(const UDateFormat * fmt) {
  return udat_isLenient(fmt);
}
UDateFormat * udat_open_android(UDateFormatStyle timeStyle, UDateFormatStyle dateStyle, const char * locale, const UChar * tzID, int32_t tzIDLength, const UChar * pattern, int32_t patternLength, UErrorCode * status) {
  return udat_open(timeStyle, dateStyle, locale, tzID, tzIDLength, pattern, patternLength, status);
}
UDate udat_parse_android(const UDateFormat * format, const UChar * text, int32_t textLength, int32_t * parsePos, UErrorCode * status) {
  return udat_parse(format, text, textLength, parsePos, status);
}
void udat_parseCalendar_android(const UDateFormat * format, UCalendar * calendar, const UChar * text, int32_t textLength, int32_t * parsePos, UErrorCode * status) {
  udat_parseCalendar(format, calendar, text, textLength, parsePos, status);
}
void udat_set2DigitYearStart_android(UDateFormat * fmt, UDate d, UErrorCode * status) {
  udat_set2DigitYearStart(fmt, d, status);
}
void udat_setBooleanAttribute_android(UDateFormat * fmt, UDateFormatBooleanAttribute attr, UBool newValue, UErrorCode * status) {
  udat_setBooleanAttribute(fmt, attr, newValue, status);
}
void udat_setCalendar_android(UDateFormat * fmt, const UCalendar * calendarToSet) {
  udat_setCalendar(fmt, calendarToSet);
}
void udat_setContext_android(UDateFormat * fmt, UDisplayContext value, UErrorCode * status) {
  udat_setContext(fmt, value, status);
}
void udat_setLenient_android(UDateFormat * fmt, UBool isLenient) {
  udat_setLenient(fmt, isLenient);
}
void udat_setNumberFormat_android(UDateFormat * fmt, const UNumberFormat * numberFormatToSet) {
  udat_setNumberFormat(fmt, numberFormatToSet);
}
void udat_setSymbols_android(UDateFormat * format, UDateFormatSymbolType type, int32_t symbolIndex, UChar * value, int32_t valueLength, UErrorCode * status) {
  udat_setSymbols(format, type, symbolIndex, value, valueLength, status);
}
UCalendarDateFields udat_toCalendarDateField_android(UDateFormatField field) {
  return udat_toCalendarDateField(field);
}
int32_t udat_toPattern_android(const UDateFormat * fmt, UBool localized, UChar * result, int32_t resultLength, UErrorCode * status) {
  return udat_toPattern(fmt, localized, result, resultLength, status);
}
void udata_close_android(UDataMemory * pData) {
  udata_close(pData);
}
void udata_getInfo_android(UDataMemory * pData, UDataInfo * pInfo) {
  udata_getInfo(pData, pInfo);
}
const void * udata_getMemory_android(UDataMemory * pData) {
  return udata_getMemory(pData);
}
UDataMemory * udata_open_android(const char * path, const char * type, const char * name, UErrorCode * pErrorCode) {
  return udata_open(path, type, name, pErrorCode);
}
UDataMemory * udata_openChoice_android(const char * path, const char * type, const char * name, UDataMemoryIsAcceptable * isAcceptable, void * context, UErrorCode * pErrorCode) {
  return udata_openChoice(path, type, name, isAcceptable, context, pErrorCode);
}
void udata_setAppData_android(const char * packageName, const void * data, UErrorCode * err) {
  udata_setAppData(packageName, data, err);
}
void udata_setCommonData_android(const void * data, UErrorCode * err) {
  udata_setCommonData(data, err);
}
void udata_setFileAccess_android(UDataFileAccess access, UErrorCode * status) {
  udata_setFileAccess(access, status);
}
UDateTimePatternConflict udatpg_addPattern_android(UDateTimePatternGenerator * dtpg, const UChar * pattern, int32_t patternLength, UBool override, UChar * conflictingPattern, int32_t capacity, int32_t * pLength, UErrorCode * pErrorCode) {
  return udatpg_addPattern(dtpg, pattern, patternLength, override, conflictingPattern, capacity, pLength, pErrorCode);
}
UDateTimePatternGenerator * udatpg_clone_android(const UDateTimePatternGenerator * dtpg, UErrorCode * pErrorCode) {
  return udatpg_clone(dtpg, pErrorCode);
}
void udatpg_close_android(UDateTimePatternGenerator * dtpg) {
  udatpg_close(dtpg);
}
const UChar * udatpg_getAppendItemFormat_android(const UDateTimePatternGenerator * dtpg, UDateTimePatternField field, int32_t * pLength) {
  return udatpg_getAppendItemFormat(dtpg, field, pLength);
}
const UChar * udatpg_getAppendItemName_android(const UDateTimePatternGenerator * dtpg, UDateTimePatternField field, int32_t * pLength) {
  return udatpg_getAppendItemName(dtpg, field, pLength);
}
int32_t udatpg_getBaseSkeleton_android(UDateTimePatternGenerator * unusedDtpg, const UChar * pattern, int32_t length, UChar * baseSkeleton, int32_t capacity, UErrorCode * pErrorCode) {
  return udatpg_getBaseSkeleton(unusedDtpg, pattern, length, baseSkeleton, capacity, pErrorCode);
}
int32_t udatpg_getBestPattern_android(UDateTimePatternGenerator * dtpg, const UChar * skeleton, int32_t length, UChar * bestPattern, int32_t capacity, UErrorCode * pErrorCode) {
  return udatpg_getBestPattern(dtpg, skeleton, length, bestPattern, capacity, pErrorCode);
}
int32_t udatpg_getBestPatternWithOptions_android(UDateTimePatternGenerator * dtpg, const UChar * skeleton, int32_t length, UDateTimePatternMatchOptions options, UChar * bestPattern, int32_t capacity, UErrorCode * pErrorCode) {
  return udatpg_getBestPatternWithOptions(dtpg, skeleton, length, options, bestPattern, capacity, pErrorCode);
}
const UChar * udatpg_getDateTimeFormat_android(const UDateTimePatternGenerator * dtpg, int32_t * pLength) {
  return udatpg_getDateTimeFormat(dtpg, pLength);
}
const UChar * udatpg_getDecimal_android(const UDateTimePatternGenerator * dtpg, int32_t * pLength) {
  return udatpg_getDecimal(dtpg, pLength);
}
int32_t udatpg_getFieldDisplayName_android(const UDateTimePatternGenerator * dtpg, UDateTimePatternField field, UDateTimePGDisplayWidth width, UChar * fieldName, int32_t capacity, UErrorCode * pErrorCode) {
  return udatpg_getFieldDisplayName(dtpg, field, width, fieldName, capacity, pErrorCode);
}
const UChar * udatpg_getPatternForSkeleton_android(const UDateTimePatternGenerator * dtpg, const UChar * skeleton, int32_t skeletonLength, int32_t * pLength) {
  return udatpg_getPatternForSkeleton(dtpg, skeleton, skeletonLength, pLength);
}
int32_t udatpg_getSkeleton_android(UDateTimePatternGenerator * unusedDtpg, const UChar * pattern, int32_t length, UChar * skeleton, int32_t capacity, UErrorCode * pErrorCode) {
  return udatpg_getSkeleton(unusedDtpg, pattern, length, skeleton, capacity, pErrorCode);
}
UDateTimePatternGenerator * udatpg_open_android(const char * locale, UErrorCode * pErrorCode) {
  return udatpg_open(locale, pErrorCode);
}
UEnumeration * udatpg_openBaseSkeletons_android(const UDateTimePatternGenerator * dtpg, UErrorCode * pErrorCode) {
  return udatpg_openBaseSkeletons(dtpg, pErrorCode);
}
UDateTimePatternGenerator * udatpg_openEmpty_android(UErrorCode * pErrorCode) {
  return udatpg_openEmpty(pErrorCode);
}
UEnumeration * udatpg_openSkeletons_android(const UDateTimePatternGenerator * dtpg, UErrorCode * pErrorCode) {
  return udatpg_openSkeletons(dtpg, pErrorCode);
}
int32_t udatpg_replaceFieldTypes_android(UDateTimePatternGenerator * dtpg, const UChar * pattern, int32_t patternLength, const UChar * skeleton, int32_t skeletonLength, UChar * dest, int32_t destCapacity, UErrorCode * pErrorCode) {
  return udatpg_replaceFieldTypes(dtpg, pattern, patternLength, skeleton, skeletonLength, dest, destCapacity, pErrorCode);
}
int32_t udatpg_replaceFieldTypesWithOptions_android(UDateTimePatternGenerator * dtpg, const UChar * pattern, int32_t patternLength, const UChar * skeleton, int32_t skeletonLength, UDateTimePatternMatchOptions options, UChar * dest, int32_t destCapacity, UErrorCode * pErrorCode) {
  return udatpg_replaceFieldTypesWithOptions(dtpg, pattern, patternLength, skeleton, skeletonLength, options, dest, destCapacity, pErrorCode);
}
void udatpg_setAppendItemFormat_android(UDateTimePatternGenerator * dtpg, UDateTimePatternField field, const UChar * value, int32_t length) {
  udatpg_setAppendItemFormat(dtpg, field, value, length);
}
void udatpg_setAppendItemName_android(UDateTimePatternGenerator * dtpg, UDateTimePatternField field, const UChar * value, int32_t length) {
  udatpg_setAppendItemName(dtpg, field, value, length);
}
void udatpg_setDateTimeFormat_android(const UDateTimePatternGenerator * dtpg, const UChar * dtFormat, int32_t length) {
  udatpg_setDateTimeFormat(dtpg, dtFormat, length);
}
void udatpg_setDecimal_android(UDateTimePatternGenerator * dtpg, const UChar * decimal, int32_t length) {
  udatpg_setDecimal(dtpg, decimal, length);
}
void udtitvfmt_close_android(UDateIntervalFormat * formatter) {
  udtitvfmt_close(formatter);
}
int32_t udtitvfmt_format_android(const UDateIntervalFormat * formatter, UDate fromDate, UDate toDate, UChar * result, int32_t resultCapacity, UFieldPosition * position, UErrorCode * status) {
  return udtitvfmt_format(formatter, fromDate, toDate, result, resultCapacity, position, status);
}
UDateIntervalFormat * udtitvfmt_open_android(const char * locale, const UChar * skeleton, int32_t skeletonLength, const UChar * tzID, int32_t tzIDLength, UErrorCode * status) {
  return udtitvfmt_open(locale, skeleton, skeletonLength, tzID, tzIDLength, status);
}
void uenum_close_android(UEnumeration * en) {
  uenum_close(en);
}
int32_t uenum_count_android(UEnumeration * en, UErrorCode * status) {
  return uenum_count(en, status);
}
const char * uenum_next_android(UEnumeration * en, int32_t * resultLength, UErrorCode * status) {
  return uenum_next(en, resultLength, status);
}
UEnumeration * uenum_openCharStringsEnumeration_android(const char *const  strings[], int32_t count, UErrorCode * ec) {
  return uenum_openCharStringsEnumeration(strings, count, ec);
}
UEnumeration * uenum_openUCharStringsEnumeration_android(const UChar *const  strings[], int32_t count, UErrorCode * ec) {
  return uenum_openUCharStringsEnumeration(strings, count, ec);
}
void uenum_reset_android(UEnumeration * en, UErrorCode * status) {
  uenum_reset(en, status);
}
const UChar * uenum_unext_android(UEnumeration * en, int32_t * resultLength, UErrorCode * status) {
  return uenum_unext(en, resultLength, status);
}
void ufieldpositer_close_android(UFieldPositionIterator * fpositer) {
  ufieldpositer_close(fpositer);
}
int32_t ufieldpositer_next_android(UFieldPositionIterator * fpositer, int32_t * beginIndex, int32_t * endIndex) {
  return ufieldpositer_next(fpositer, beginIndex, endIndex);
}
UFieldPositionIterator * ufieldpositer_open_android(UErrorCode * status) {
  return ufieldpositer_open(status);
}
void ufmt_close_android(UFormattable * fmt) {
  ufmt_close(fmt);
}
UFormattable * ufmt_getArrayItemByIndex_android(UFormattable * fmt, int32_t n, UErrorCode * status) {
  return ufmt_getArrayItemByIndex(fmt, n, status);
}
int32_t ufmt_getArrayLength_android(const UFormattable * fmt, UErrorCode * status) {
  return ufmt_getArrayLength(fmt, status);
}
UDate ufmt_getDate_android(const UFormattable * fmt, UErrorCode * status) {
  return ufmt_getDate(fmt, status);
}
const char * ufmt_getDecNumChars_android(UFormattable * fmt, int32_t * len, UErrorCode * status) {
  return ufmt_getDecNumChars(fmt, len, status);
}
double ufmt_getDouble_android(UFormattable * fmt, UErrorCode * status) {
  return ufmt_getDouble(fmt, status);
}
int64_t ufmt_getInt64_android(UFormattable * fmt, UErrorCode * status) {
  return ufmt_getInt64(fmt, status);
}
int32_t ufmt_getLong_android(UFormattable * fmt, UErrorCode * status) {
  return ufmt_getLong(fmt, status);
}
const void * ufmt_getObject_android(const UFormattable * fmt, UErrorCode * status) {
  return ufmt_getObject(fmt, status);
}
UFormattableType ufmt_getType_android(const UFormattable * fmt, UErrorCode * status) {
  return ufmt_getType(fmt, status);
}
const UChar * ufmt_getUChars_android(UFormattable * fmt, int32_t * len, UErrorCode * status) {
  return ufmt_getUChars(fmt, len, status);
}
UBool ufmt_isNumeric_android(const UFormattable * fmt) {
  return ufmt_isNumeric(fmt);
}
UFormattable * ufmt_open_android(UErrorCode * status) {
  return ufmt_open(status);
}
const UGenderInfo * ugender_getInstance_android(const char * locale, UErrorCode * status) {
  return ugender_getInstance(locale, status);
}
UGender ugender_getListGender_android(const UGenderInfo * genderInfo, const UGender * genders, int32_t size, UErrorCode * status) {
  return ugender_getListGender(genderInfo, genders, size, status);
}
void uidna_close_android(UIDNA * idna) {
  uidna_close(idna);
}
int32_t uidna_labelToASCII_android(const UIDNA * idna, const UChar * label, int32_t length, UChar * dest, int32_t capacity, UIDNAInfo * pInfo, UErrorCode * pErrorCode) {
  return uidna_labelToASCII(idna, label, length, dest, capacity, pInfo, pErrorCode);
}
int32_t uidna_labelToASCII_UTF8_android(const UIDNA * idna, const char * label, int32_t length, char * dest, int32_t capacity, UIDNAInfo * pInfo, UErrorCode * pErrorCode) {
  return uidna_labelToASCII_UTF8(idna, label, length, dest, capacity, pInfo, pErrorCode);
}
int32_t uidna_labelToUnicode_android(const UIDNA * idna, const UChar * label, int32_t length, UChar * dest, int32_t capacity, UIDNAInfo * pInfo, UErrorCode * pErrorCode) {
  return uidna_labelToUnicode(idna, label, length, dest, capacity, pInfo, pErrorCode);
}
int32_t uidna_labelToUnicodeUTF8_android(const UIDNA * idna, const char * label, int32_t length, char * dest, int32_t capacity, UIDNAInfo * pInfo, UErrorCode * pErrorCode) {
  return uidna_labelToUnicodeUTF8(idna, label, length, dest, capacity, pInfo, pErrorCode);
}
int32_t uidna_nameToASCII_android(const UIDNA * idna, const UChar * name, int32_t length, UChar * dest, int32_t capacity, UIDNAInfo * pInfo, UErrorCode * pErrorCode) {
  return uidna_nameToASCII(idna, name, length, dest, capacity, pInfo, pErrorCode);
}
int32_t uidna_nameToASCII_UTF8_android(const UIDNA * idna, const char * name, int32_t length, char * dest, int32_t capacity, UIDNAInfo * pInfo, UErrorCode * pErrorCode) {
  return uidna_nameToASCII_UTF8(idna, name, length, dest, capacity, pInfo, pErrorCode);
}
int32_t uidna_nameToUnicode_android(const UIDNA * idna, const UChar * name, int32_t length, UChar * dest, int32_t capacity, UIDNAInfo * pInfo, UErrorCode * pErrorCode) {
  return uidna_nameToUnicode(idna, name, length, dest, capacity, pInfo, pErrorCode);
}
int32_t uidna_nameToUnicodeUTF8_android(const UIDNA * idna, const char * name, int32_t length, char * dest, int32_t capacity, UIDNAInfo * pInfo, UErrorCode * pErrorCode) {
  return uidna_nameToUnicodeUTF8(idna, name, length, dest, capacity, pInfo, pErrorCode);
}
UIDNA * uidna_openUTS46_android(uint32_t options, UErrorCode * pErrorCode) {
  return uidna_openUTS46(options, pErrorCode);
}
UChar32 uiter_current32_android(UCharIterator * iter) {
  return uiter_current32(iter);
}
uint32_t uiter_getState_android(const UCharIterator * iter) {
  return uiter_getState(iter);
}
UChar32 uiter_next32_android(UCharIterator * iter) {
  return uiter_next32(iter);
}
UChar32 uiter_previous32_android(UCharIterator * iter) {
  return uiter_previous32(iter);
}
void uiter_setState_android(UCharIterator * iter, uint32_t state, UErrorCode * pErrorCode) {
  uiter_setState(iter, state, pErrorCode);
}
void uiter_setString_android(UCharIterator * iter, const UChar * s, int32_t length) {
  uiter_setString(iter, s, length);
}
void uiter_setUTF16BE_android(UCharIterator * iter, const char * s, int32_t length) {
  uiter_setUTF16BE(iter, s, length);
}
void uiter_setUTF8_android(UCharIterator * iter, const char * s, int32_t length) {
  uiter_setUTF8(iter, s, length);
}
void uldn_close_android(ULocaleDisplayNames * ldn) {
  uldn_close(ldn);
}
UDisplayContext uldn_getContext_android(const ULocaleDisplayNames * ldn, UDisplayContextType type, UErrorCode * pErrorCode) {
  return uldn_getContext(ldn, type, pErrorCode);
}
UDialectHandling uldn_getDialectHandling_android(const ULocaleDisplayNames * ldn) {
  return uldn_getDialectHandling(ldn);
}
const char * uldn_getLocale_android(const ULocaleDisplayNames * ldn) {
  return uldn_getLocale(ldn);
}
int32_t uldn_keyDisplayName_android(const ULocaleDisplayNames * ldn, const char * key, UChar * result, int32_t maxResultSize, UErrorCode * pErrorCode) {
  return uldn_keyDisplayName(ldn, key, result, maxResultSize, pErrorCode);
}
int32_t uldn_keyValueDisplayName_android(const ULocaleDisplayNames * ldn, const char * key, const char * value, UChar * result, int32_t maxResultSize, UErrorCode * pErrorCode) {
  return uldn_keyValueDisplayName(ldn, key, value, result, maxResultSize, pErrorCode);
}
int32_t uldn_languageDisplayName_android(const ULocaleDisplayNames * ldn, const char * lang, UChar * result, int32_t maxResultSize, UErrorCode * pErrorCode) {
  return uldn_languageDisplayName(ldn, lang, result, maxResultSize, pErrorCode);
}
int32_t uldn_localeDisplayName_android(const ULocaleDisplayNames * ldn, const char * locale, UChar * result, int32_t maxResultSize, UErrorCode * pErrorCode) {
  return uldn_localeDisplayName(ldn, locale, result, maxResultSize, pErrorCode);
}
ULocaleDisplayNames * uldn_open_android(const char * locale, UDialectHandling dialectHandling, UErrorCode * pErrorCode) {
  return uldn_open(locale, dialectHandling, pErrorCode);
}
ULocaleDisplayNames * uldn_openForContext_android(const char * locale, UDisplayContext * contexts, int32_t length, UErrorCode * pErrorCode) {
  return uldn_openForContext(locale, contexts, length, pErrorCode);
}
int32_t uldn_regionDisplayName_android(const ULocaleDisplayNames * ldn, const char * region, UChar * result, int32_t maxResultSize, UErrorCode * pErrorCode) {
  return uldn_regionDisplayName(ldn, region, result, maxResultSize, pErrorCode);
}
int32_t uldn_scriptCodeDisplayName_android(const ULocaleDisplayNames * ldn, UScriptCode scriptCode, UChar * result, int32_t maxResultSize, UErrorCode * pErrorCode) {
  return uldn_scriptCodeDisplayName(ldn, scriptCode, result, maxResultSize, pErrorCode);
}
int32_t uldn_scriptDisplayName_android(const ULocaleDisplayNames * ldn, const char * script, UChar * result, int32_t maxResultSize, UErrorCode * pErrorCode) {
  return uldn_scriptDisplayName(ldn, script, result, maxResultSize, pErrorCode);
}
int32_t uldn_variantDisplayName_android(const ULocaleDisplayNames * ldn, const char * variant, UChar * result, int32_t maxResultSize, UErrorCode * pErrorCode) {
  return uldn_variantDisplayName(ldn, variant, result, maxResultSize, pErrorCode);
}
void ulistfmt_close_android(UListFormatter * listfmt) {
  ulistfmt_close(listfmt);
}
int32_t ulistfmt_format_android(const UListFormatter * listfmt, const UChar *const  strings[], const int32_t * stringLengths, int32_t stringCount, UChar * result, int32_t resultCapacity, UErrorCode * status) {
  return ulistfmt_format(listfmt, strings, stringLengths, stringCount, result, resultCapacity, status);
}
UListFormatter * ulistfmt_open_android(const char * locale, UErrorCode * status) {
  return ulistfmt_open(locale, status);
}
int32_t uloc_acceptLanguage_android(char * result, int32_t resultAvailable, UAcceptResult * outResult, const char ** acceptList, int32_t acceptListCount, UEnumeration * availableLocales, UErrorCode * status) {
  return uloc_acceptLanguage(result, resultAvailable, outResult, acceptList, acceptListCount, availableLocales, status);
}
int32_t uloc_acceptLanguageFromHTTP_android(char * result, int32_t resultAvailable, UAcceptResult * outResult, const char * httpAcceptLanguage, UEnumeration * availableLocales, UErrorCode * status) {
  return uloc_acceptLanguageFromHTTP(result, resultAvailable, outResult, httpAcceptLanguage, availableLocales, status);
}
int32_t uloc_addLikelySubtags_android(const char * localeID, char * maximizedLocaleID, int32_t maximizedLocaleIDCapacity, UErrorCode * err) {
  return uloc_addLikelySubtags(localeID, maximizedLocaleID, maximizedLocaleIDCapacity, err);
}
int32_t uloc_canonicalize_android(const char * localeID, char * name, int32_t nameCapacity, UErrorCode * err) {
  return uloc_canonicalize(localeID, name, nameCapacity, err);
}
int32_t uloc_countAvailable_android() {
  return uloc_countAvailable();
}
int32_t uloc_forLanguageTag_android(const char * langtag, char * localeID, int32_t localeIDCapacity, int32_t * parsedLength, UErrorCode * err) {
  return uloc_forLanguageTag(langtag, localeID, localeIDCapacity, parsedLength, err);
}
const char * uloc_getAvailable_android(int32_t n) {
  return uloc_getAvailable(n);
}
int32_t uloc_getBaseName_android(const char * localeID, char * name, int32_t nameCapacity, UErrorCode * err) {
  return uloc_getBaseName(localeID, name, nameCapacity, err);
}
ULayoutType uloc_getCharacterOrientation_android(const char * localeId, UErrorCode * status) {
  return uloc_getCharacterOrientation(localeId, status);
}
int32_t uloc_getCountry_android(const char * localeID, char * country, int32_t countryCapacity, UErrorCode * err) {
  return uloc_getCountry(localeID, country, countryCapacity, err);
}
const char * uloc_getDefault_android() {
  return uloc_getDefault();
}
int32_t uloc_getDisplayCountry_android(const char * locale, const char * displayLocale, UChar * country, int32_t countryCapacity, UErrorCode * status) {
  return uloc_getDisplayCountry(locale, displayLocale, country, countryCapacity, status);
}
int32_t uloc_getDisplayKeyword_android(const char * keyword, const char * displayLocale, UChar * dest, int32_t destCapacity, UErrorCode * status) {
  return uloc_getDisplayKeyword(keyword, displayLocale, dest, destCapacity, status);
}
int32_t uloc_getDisplayKeywordValue_android(const char * locale, const char * keyword, const char * displayLocale, UChar * dest, int32_t destCapacity, UErrorCode * status) {
  return uloc_getDisplayKeywordValue(locale, keyword, displayLocale, dest, destCapacity, status);
}
int32_t uloc_getDisplayLanguage_android(const char * locale, const char * displayLocale, UChar * language, int32_t languageCapacity, UErrorCode * status) {
  return uloc_getDisplayLanguage(locale, displayLocale, language, languageCapacity, status);
}
int32_t uloc_getDisplayName_android(const char * localeID, const char * inLocaleID, UChar * result, int32_t maxResultSize, UErrorCode * err) {
  return uloc_getDisplayName(localeID, inLocaleID, result, maxResultSize, err);
}
int32_t uloc_getDisplayScript_android(const char * locale, const char * displayLocale, UChar * script, int32_t scriptCapacity, UErrorCode * status) {
  return uloc_getDisplayScript(locale, displayLocale, script, scriptCapacity, status);
}
int32_t uloc_getDisplayVariant_android(const char * locale, const char * displayLocale, UChar * variant, int32_t variantCapacity, UErrorCode * status) {
  return uloc_getDisplayVariant(locale, displayLocale, variant, variantCapacity, status);
}
const char * uloc_getISO3Country_android(const char * localeID) {
  return uloc_getISO3Country(localeID);
}
const char * uloc_getISO3Language_android(const char * localeID) {
  return uloc_getISO3Language(localeID);
}
const char *const * uloc_getISOCountries_android() {
  return uloc_getISOCountries();
}
const char *const * uloc_getISOLanguages_android() {
  return uloc_getISOLanguages();
}
int32_t uloc_getKeywordValue_android(const char * localeID, const char * keywordName, char * buffer, int32_t bufferCapacity, UErrorCode * status) {
  return uloc_getKeywordValue(localeID, keywordName, buffer, bufferCapacity, status);
}
uint32_t uloc_getLCID_android(const char * localeID) {
  return uloc_getLCID(localeID);
}
int32_t uloc_getLanguage_android(const char * localeID, char * language, int32_t languageCapacity, UErrorCode * err) {
  return uloc_getLanguage(localeID, language, languageCapacity, err);
}
ULayoutType uloc_getLineOrientation_android(const char * localeId, UErrorCode * status) {
  return uloc_getLineOrientation(localeId, status);
}
int32_t uloc_getLocaleForLCID_android(uint32_t hostID, char * locale, int32_t localeCapacity, UErrorCode * status) {
  return uloc_getLocaleForLCID(hostID, locale, localeCapacity, status);
}
int32_t uloc_getName_android(const char * localeID, char * name, int32_t nameCapacity, UErrorCode * err) {
  return uloc_getName(localeID, name, nameCapacity, err);
}
int32_t uloc_getParent_android(const char * localeID, char * parent, int32_t parentCapacity, UErrorCode * err) {
  return uloc_getParent(localeID, parent, parentCapacity, err);
}
int32_t uloc_getScript_android(const char * localeID, char * script, int32_t scriptCapacity, UErrorCode * err) {
  return uloc_getScript(localeID, script, scriptCapacity, err);
}
int32_t uloc_getVariant_android(const char * localeID, char * variant, int32_t variantCapacity, UErrorCode * err) {
  return uloc_getVariant(localeID, variant, variantCapacity, err);
}
UBool uloc_isRightToLeft_android(const char * locale) {
  return uloc_isRightToLeft(locale);
}
int32_t uloc_minimizeSubtags_android(const char * localeID, char * minimizedLocaleID, int32_t minimizedLocaleIDCapacity, UErrorCode * err) {
  return uloc_minimizeSubtags(localeID, minimizedLocaleID, minimizedLocaleIDCapacity, err);
}
UEnumeration * uloc_openKeywords_android(const char * localeID, UErrorCode * status) {
  return uloc_openKeywords(localeID, status);
}
void uloc_setDefault_android(const char * localeID, UErrorCode * status) {
  uloc_setDefault(localeID, status);
}
int32_t uloc_setKeywordValue_android(const char * keywordName, const char * keywordValue, char * buffer, int32_t bufferCapacity, UErrorCode * status) {
  return uloc_setKeywordValue(keywordName, keywordValue, buffer, bufferCapacity, status);
}
int32_t uloc_toLanguageTag_android(const char * localeID, char * langtag, int32_t langtagCapacity, UBool strict, UErrorCode * err) {
  return uloc_toLanguageTag(localeID, langtag, langtagCapacity, strict, err);
}
const char * uloc_toLegacyKey_android(const char * keyword) {
  return uloc_toLegacyKey(keyword);
}
const char * uloc_toLegacyType_android(const char * keyword, const char * value) {
  return uloc_toLegacyType(keyword, value);
}
const char * uloc_toUnicodeLocaleKey_android(const char * keyword) {
  return uloc_toUnicodeLocaleKey(keyword);
}
const char * uloc_toUnicodeLocaleType_android(const char * keyword, const char * value) {
  return uloc_toUnicodeLocaleType(keyword, value);
}
void ulocdata_close_android(ULocaleData * uld) {
  ulocdata_close(uld);
}
void ulocdata_getCLDRVersion_android(UVersionInfo versionArray, UErrorCode * status) {
  ulocdata_getCLDRVersion(versionArray, status);
}
int32_t ulocdata_getDelimiter_android(ULocaleData * uld, ULocaleDataDelimiterType type, UChar * result, int32_t resultLength, UErrorCode * status) {
  return ulocdata_getDelimiter(uld, type, result, resultLength, status);
}
USet * ulocdata_getExemplarSet_android(ULocaleData * uld, USet * fillIn, uint32_t options, ULocaleDataExemplarSetType extype, UErrorCode * status) {
  return ulocdata_getExemplarSet(uld, fillIn, options, extype, status);
}
int32_t ulocdata_getLocaleDisplayPattern_android(ULocaleData * uld, UChar * pattern, int32_t patternCapacity, UErrorCode * status) {
  return ulocdata_getLocaleDisplayPattern(uld, pattern, patternCapacity, status);
}
int32_t ulocdata_getLocaleSeparator_android(ULocaleData * uld, UChar * separator, int32_t separatorCapacity, UErrorCode * status) {
  return ulocdata_getLocaleSeparator(uld, separator, separatorCapacity, status);
}
UMeasurementSystem ulocdata_getMeasurementSystem_android(const char * localeID, UErrorCode * status) {
  return ulocdata_getMeasurementSystem(localeID, status);
}
UBool ulocdata_getNoSubstitute_android(ULocaleData * uld) {
  return ulocdata_getNoSubstitute(uld);
}
void ulocdata_getPaperSize_android(const char * localeID, int32_t * height, int32_t * width, UErrorCode * status) {
  ulocdata_getPaperSize(localeID, height, width, status);
}
ULocaleData * ulocdata_open_android(const char * localeID, UErrorCode * status) {
  return ulocdata_open(localeID, status);
}
void ulocdata_setNoSubstitute_android(ULocaleData * uld, UBool setting) {
  ulocdata_setNoSubstitute(uld, setting);
}
void umsg_applyPattern_android(UMessageFormat * fmt, const UChar * pattern, int32_t patternLength, UParseError * parseError, UErrorCode * status) {
  umsg_applyPattern(fmt, pattern, patternLength, parseError, status);
}
int32_t umsg_autoQuoteApostrophe_android(const UChar * pattern, int32_t patternLength, UChar * dest, int32_t destCapacity, UErrorCode * ec) {
  return umsg_autoQuoteApostrophe(pattern, patternLength, dest, destCapacity, ec);
}
UMessageFormat umsg_clone_android(const UMessageFormat * fmt, UErrorCode * status) {
  return umsg_clone(fmt, status);
}
void umsg_close_android(UMessageFormat * format) {
  umsg_close(format);
}
int32_t umsg_format_android(const UMessageFormat * fmt, UChar * result, int32_t resultLength, UErrorCode * status, ...) {
  va_list args;
  va_start(args, status);
  int32_t ret = umsg_vformat(fmt, result, resultLength, args, status);
  va_end(args);
  return ret;
}
const char * umsg_getLocale_android(const UMessageFormat * fmt) {
  return umsg_getLocale(fmt);
}
UMessageFormat * umsg_open_android(const UChar * pattern, int32_t patternLength, const char * locale, UParseError * parseError, UErrorCode * status) {
  return umsg_open(pattern, patternLength, locale, parseError, status);
}
void umsg_parse_android(const UMessageFormat * fmt, const UChar * source, int32_t sourceLength, int32_t * count, UErrorCode * status, ...) {
  va_list args;
  va_start(args, status);
  umsg_vparse(fmt, source, sourceLength, count, args, status);
  va_end(args);
  return;
}
void umsg_setLocale_android(UMessageFormat * fmt, const char * locale) {
  umsg_setLocale(fmt, locale);
}
int32_t umsg_toPattern_android(const UMessageFormat * fmt, UChar * result, int32_t resultLength, UErrorCode * status) {
  return umsg_toPattern(fmt, result, resultLength, status);
}
int32_t umsg_vformat_android(const UMessageFormat * fmt, UChar * result, int32_t resultLength, va_list ap, UErrorCode * status) {
  return umsg_vformat(fmt, result, resultLength, ap, status);
}
void umsg_vparse_android(const UMessageFormat * fmt, const UChar * source, int32_t sourceLength, int32_t * count, va_list ap, UErrorCode * status) {
  umsg_vparse(fmt, source, sourceLength, count, ap, status);
}
UCPTrie * umutablecptrie_buildImmutable_android(UMutableCPTrie * trie, UCPTrieType type, UCPTrieValueWidth valueWidth, UErrorCode * pErrorCode) {
  return umutablecptrie_buildImmutable(trie, type, valueWidth, pErrorCode);
}
UMutableCPTrie * umutablecptrie_clone_android(const UMutableCPTrie * other, UErrorCode * pErrorCode) {
  return umutablecptrie_clone(other, pErrorCode);
}
void umutablecptrie_close_android(UMutableCPTrie * trie) {
  umutablecptrie_close(trie);
}
UMutableCPTrie * umutablecptrie_fromUCPMap_android(const UCPMap * map, UErrorCode * pErrorCode) {
  return umutablecptrie_fromUCPMap(map, pErrorCode);
}
UMutableCPTrie * umutablecptrie_fromUCPTrie_android(const UCPTrie * trie, UErrorCode * pErrorCode) {
  return umutablecptrie_fromUCPTrie(trie, pErrorCode);
}
uint32_t umutablecptrie_get_android(const UMutableCPTrie * trie, UChar32 c) {
  return umutablecptrie_get(trie, c);
}
UChar32 umutablecptrie_getRange_android(const UMutableCPTrie * trie, UChar32 start, UCPMapRangeOption option, uint32_t surrogateValue, UCPMapValueFilter * filter, const void * context, uint32_t * pValue) {
  return umutablecptrie_getRange(trie, start, option, surrogateValue, filter, context, pValue);
}
UMutableCPTrie * umutablecptrie_open_android(uint32_t initialValue, uint32_t errorValue, UErrorCode * pErrorCode) {
  return umutablecptrie_open(initialValue, errorValue, pErrorCode);
}
void umutablecptrie_set_android(UMutableCPTrie * trie, UChar32 c, uint32_t value, UErrorCode * pErrorCode) {
  umutablecptrie_set(trie, c, value, pErrorCode);
}
void umutablecptrie_setRange_android(UMutableCPTrie * trie, UChar32 start, UChar32 end, uint32_t value, UErrorCode * pErrorCode) {
  umutablecptrie_setRange(trie, start, end, value, pErrorCode);
}
int32_t unorm2_append_android(const UNormalizer2 * norm2, UChar * first, int32_t firstLength, int32_t firstCapacity, const UChar * second, int32_t secondLength, UErrorCode * pErrorCode) {
  return unorm2_append(norm2, first, firstLength, firstCapacity, second, secondLength, pErrorCode);
}
void unorm2_close_android(UNormalizer2 * norm2) {
  unorm2_close(norm2);
}
UChar32 unorm2_composePair_android(const UNormalizer2 * norm2, UChar32 a, UChar32 b) {
  return unorm2_composePair(norm2, a, b);
}
uint8_t unorm2_getCombiningClass_android(const UNormalizer2 * norm2, UChar32 c) {
  return unorm2_getCombiningClass(norm2, c);
}
int32_t unorm2_getDecomposition_android(const UNormalizer2 * norm2, UChar32 c, UChar * decomposition, int32_t capacity, UErrorCode * pErrorCode) {
  return unorm2_getDecomposition(norm2, c, decomposition, capacity, pErrorCode);
}
const UNormalizer2 * unorm2_getInstance_android(const char * packageName, const char * name, UNormalization2Mode mode, UErrorCode * pErrorCode) {
  return unorm2_getInstance(packageName, name, mode, pErrorCode);
}
const UNormalizer2 * unorm2_getNFCInstance_android(UErrorCode * pErrorCode) {
  return unorm2_getNFCInstance(pErrorCode);
}
const UNormalizer2 * unorm2_getNFDInstance_android(UErrorCode * pErrorCode) {
  return unorm2_getNFDInstance(pErrorCode);
}
const UNormalizer2 * unorm2_getNFKCCasefoldInstance_android(UErrorCode * pErrorCode) {
  return unorm2_getNFKCCasefoldInstance(pErrorCode);
}
const UNormalizer2 * unorm2_getNFKCInstance_android(UErrorCode * pErrorCode) {
  return unorm2_getNFKCInstance(pErrorCode);
}
const UNormalizer2 * unorm2_getNFKDInstance_android(UErrorCode * pErrorCode) {
  return unorm2_getNFKDInstance(pErrorCode);
}
int32_t unorm2_getRawDecomposition_android(const UNormalizer2 * norm2, UChar32 c, UChar * decomposition, int32_t capacity, UErrorCode * pErrorCode) {
  return unorm2_getRawDecomposition(norm2, c, decomposition, capacity, pErrorCode);
}
UBool unorm2_hasBoundaryAfter_android(const UNormalizer2 * norm2, UChar32 c) {
  return unorm2_hasBoundaryAfter(norm2, c);
}
UBool unorm2_hasBoundaryBefore_android(const UNormalizer2 * norm2, UChar32 c) {
  return unorm2_hasBoundaryBefore(norm2, c);
}
UBool unorm2_isInert_android(const UNormalizer2 * norm2, UChar32 c) {
  return unorm2_isInert(norm2, c);
}
UBool unorm2_isNormalized_android(const UNormalizer2 * norm2, const UChar * s, int32_t length, UErrorCode * pErrorCode) {
  return unorm2_isNormalized(norm2, s, length, pErrorCode);
}
int32_t unorm2_normalize_android(const UNormalizer2 * norm2, const UChar * src, int32_t length, UChar * dest, int32_t capacity, UErrorCode * pErrorCode) {
  return unorm2_normalize(norm2, src, length, dest, capacity, pErrorCode);
}
int32_t unorm2_normalizeSecondAndAppend_android(const UNormalizer2 * norm2, UChar * first, int32_t firstLength, int32_t firstCapacity, const UChar * second, int32_t secondLength, UErrorCode * pErrorCode) {
  return unorm2_normalizeSecondAndAppend(norm2, first, firstLength, firstCapacity, second, secondLength, pErrorCode);
}
UNormalizer2 * unorm2_openFiltered_android(const UNormalizer2 * norm2, const USet * filterSet, UErrorCode * pErrorCode) {
  return unorm2_openFiltered(norm2, filterSet, pErrorCode);
}
UNormalizationCheckResult unorm2_quickCheck_android(const UNormalizer2 * norm2, const UChar * s, int32_t length, UErrorCode * pErrorCode) {
  return unorm2_quickCheck(norm2, s, length, pErrorCode);
}
int32_t unorm2_spanQuickCheckYes_android(const UNormalizer2 * norm2, const UChar * s, int32_t length, UErrorCode * pErrorCode) {
  return unorm2_spanQuickCheckYes(norm2, s, length, pErrorCode);
}
int32_t unorm_compare_android(const UChar * s1, int32_t length1, const UChar * s2, int32_t length2, uint32_t options, UErrorCode * pErrorCode) {
  return unorm_compare(s1, length1, s2, length2, options, pErrorCode);
}
void unum_applyPattern_android(UNumberFormat * format, UBool localized, const UChar * pattern, int32_t patternLength, UParseError * parseError, UErrorCode * status) {
  unum_applyPattern(format, localized, pattern, patternLength, parseError, status);
}
UNumberFormat * unum_clone_android(const UNumberFormat * fmt, UErrorCode * status) {
  return unum_clone(fmt, status);
}
void unum_close_android(UNumberFormat * fmt) {
  unum_close(fmt);
}
int32_t unum_countAvailable_android() {
  return unum_countAvailable();
}
int32_t unum_format_android(const UNumberFormat * fmt, int32_t number, UChar * result, int32_t resultLength, UFieldPosition * pos, UErrorCode * status) {
  return unum_format(fmt, number, result, resultLength, pos, status);
}
int32_t unum_formatDecimal_android(const UNumberFormat * fmt, const char * number, int32_t length, UChar * result, int32_t resultLength, UFieldPosition * pos, UErrorCode * status) {
  return unum_formatDecimal(fmt, number, length, result, resultLength, pos, status);
}
int32_t unum_formatDouble_android(const UNumberFormat * fmt, double number, UChar * result, int32_t resultLength, UFieldPosition * pos, UErrorCode * status) {
  return unum_formatDouble(fmt, number, result, resultLength, pos, status);
}
int32_t unum_formatDoubleCurrency_android(const UNumberFormat * fmt, double number, UChar * currency, UChar * result, int32_t resultLength, UFieldPosition * pos, UErrorCode * status) {
  return unum_formatDoubleCurrency(fmt, number, currency, result, resultLength, pos, status);
}
int32_t unum_formatDoubleForFields_android(const UNumberFormat * format, double number, UChar * result, int32_t resultLength, UFieldPositionIterator * fpositer, UErrorCode * status) {
  return unum_formatDoubleForFields(format, number, result, resultLength, fpositer, status);
}
int32_t unum_formatInt64_android(const UNumberFormat * fmt, int64_t number, UChar * result, int32_t resultLength, UFieldPosition * pos, UErrorCode * status) {
  return unum_formatInt64(fmt, number, result, resultLength, pos, status);
}
int32_t unum_formatUFormattable_android(const UNumberFormat * fmt, const UFormattable * number, UChar * result, int32_t resultLength, UFieldPosition * pos, UErrorCode * status) {
  return unum_formatUFormattable(fmt, number, result, resultLength, pos, status);
}
int32_t unum_getAttribute_android(const UNumberFormat * fmt, UNumberFormatAttribute attr) {
  return unum_getAttribute(fmt, attr);
}
const char * unum_getAvailable_android(int32_t localeIndex) {
  return unum_getAvailable(localeIndex);
}
UDisplayContext unum_getContext_android(const UNumberFormat * fmt, UDisplayContextType type, UErrorCode * status) {
  return unum_getContext(fmt, type, status);
}
double unum_getDoubleAttribute_android(const UNumberFormat * fmt, UNumberFormatAttribute attr) {
  return unum_getDoubleAttribute(fmt, attr);
}
const char * unum_getLocaleByType_android(const UNumberFormat * fmt, ULocDataLocaleType type, UErrorCode * status) {
  return unum_getLocaleByType(fmt, type, status);
}
int32_t unum_getSymbol_android(const UNumberFormat * fmt, UNumberFormatSymbol symbol, UChar * buffer, int32_t size, UErrorCode * status) {
  return unum_getSymbol(fmt, symbol, buffer, size, status);
}
int32_t unum_getTextAttribute_android(const UNumberFormat * fmt, UNumberFormatTextAttribute tag, UChar * result, int32_t resultLength, UErrorCode * status) {
  return unum_getTextAttribute(fmt, tag, result, resultLength, status);
}
UNumberFormat * unum_open_android(UNumberFormatStyle style, const UChar * pattern, int32_t patternLength, const char * locale, UParseError * parseErr, UErrorCode * status) {
  return unum_open(style, pattern, patternLength, locale, parseErr, status);
}
int32_t unum_parse_android(const UNumberFormat * fmt, const UChar * text, int32_t textLength, int32_t * parsePos, UErrorCode * status) {
  return unum_parse(fmt, text, textLength, parsePos, status);
}
int32_t unum_parseDecimal_android(const UNumberFormat * fmt, const UChar * text, int32_t textLength, int32_t * parsePos, char * outBuf, int32_t outBufLength, UErrorCode * status) {
  return unum_parseDecimal(fmt, text, textLength, parsePos, outBuf, outBufLength, status);
}
double unum_parseDouble_android(const UNumberFormat * fmt, const UChar * text, int32_t textLength, int32_t * parsePos, UErrorCode * status) {
  return unum_parseDouble(fmt, text, textLength, parsePos, status);
}
double unum_parseDoubleCurrency_android(const UNumberFormat * fmt, const UChar * text, int32_t textLength, int32_t * parsePos, UChar * currency, UErrorCode * status) {
  return unum_parseDoubleCurrency(fmt, text, textLength, parsePos, currency, status);
}
int64_t unum_parseInt64_android(const UNumberFormat * fmt, const UChar * text, int32_t textLength, int32_t * parsePos, UErrorCode * status) {
  return unum_parseInt64(fmt, text, textLength, parsePos, status);
}
UFormattable * unum_parseToUFormattable_android(const UNumberFormat * fmt, UFormattable * result, const UChar * text, int32_t textLength, int32_t * parsePos, UErrorCode * status) {
  return unum_parseToUFormattable(fmt, result, text, textLength, parsePos, status);
}
void unum_setAttribute_android(UNumberFormat * fmt, UNumberFormatAttribute attr, int32_t newValue) {
  unum_setAttribute(fmt, attr, newValue);
}
void unum_setContext_android(UNumberFormat * fmt, UDisplayContext value, UErrorCode * status) {
  unum_setContext(fmt, value, status);
}
void unum_setDoubleAttribute_android(UNumberFormat * fmt, UNumberFormatAttribute attr, double newValue) {
  unum_setDoubleAttribute(fmt, attr, newValue);
}
void unum_setSymbol_android(UNumberFormat * fmt, UNumberFormatSymbol symbol, const UChar * value, int32_t length, UErrorCode * status) {
  unum_setSymbol(fmt, symbol, value, length, status);
}
void unum_setTextAttribute_android(UNumberFormat * fmt, UNumberFormatTextAttribute tag, const UChar * newValue, int32_t newValueLength, UErrorCode * status) {
  unum_setTextAttribute(fmt, tag, newValue, newValueLength, status);
}
int32_t unum_toPattern_android(const UNumberFormat * fmt, UBool isPatternLocalized, UChar * result, int32_t resultLength, UErrorCode * status) {
  return unum_toPattern(fmt, isPatternLocalized, result, resultLength, status);
}
void unumf_close_android(UNumberFormatter * uformatter) {
  unumf_close(uformatter);
}
void unumf_closeResult_android(UFormattedNumber * uresult) {
  unumf_closeResult(uresult);
}
void unumf_formatDecimal_android(const UNumberFormatter * uformatter, const char * value, int32_t valueLen, UFormattedNumber * uresult, UErrorCode * ec) {
  unumf_formatDecimal(uformatter, value, valueLen, uresult, ec);
}
void unumf_formatDouble_android(const UNumberFormatter * uformatter, double value, UFormattedNumber * uresult, UErrorCode * ec) {
  unumf_formatDouble(uformatter, value, uresult, ec);
}
void unumf_formatInt_android(const UNumberFormatter * uformatter, int64_t value, UFormattedNumber * uresult, UErrorCode * ec) {
  unumf_formatInt(uformatter, value, uresult, ec);
}
UNumberFormatter * unumf_openForSkeletonAndLocale_android(const UChar * skeleton, int32_t skeletonLen, const char * locale, UErrorCode * ec) {
  return unumf_openForSkeletonAndLocale(skeleton, skeletonLen, locale, ec);
}
UFormattedNumber * unumf_openResult_android(UErrorCode * ec) {
  return unumf_openResult(ec);
}
void unumf_resultGetAllFieldPositions_android(const UFormattedNumber * uresult, UFieldPositionIterator * ufpositer, UErrorCode * ec) {
  unumf_resultGetAllFieldPositions(uresult, ufpositer, ec);
}
UBool unumf_resultNextFieldPosition_android(const UFormattedNumber * uresult, UFieldPosition * ufpos, UErrorCode * ec) {
  return unumf_resultNextFieldPosition(uresult, ufpos, ec);
}
int32_t unumf_resultToString_android(const UFormattedNumber * uresult, UChar * buffer, int32_t bufferCapacity, UErrorCode * ec) {
  return unumf_resultToString(uresult, buffer, bufferCapacity, ec);
}
void unumsys_close_android(UNumberingSystem * unumsys) {
  unumsys_close(unumsys);
}
int32_t unumsys_getDescription_android(const UNumberingSystem * unumsys, UChar * result, int32_t resultLength, UErrorCode * status) {
  return unumsys_getDescription(unumsys, result, resultLength, status);
}
const char * unumsys_getName_android(const UNumberingSystem * unumsys) {
  return unumsys_getName(unumsys);
}
int32_t unumsys_getRadix_android(const UNumberingSystem * unumsys) {
  return unumsys_getRadix(unumsys);
}
UBool unumsys_isAlgorithmic_android(const UNumberingSystem * unumsys) {
  return unumsys_isAlgorithmic(unumsys);
}
UNumberingSystem * unumsys_open_android(const char * locale, UErrorCode * status) {
  return unumsys_open(locale, status);
}
UEnumeration * unumsys_openAvailableNames_android(UErrorCode * status) {
  return unumsys_openAvailableNames(status);
}
UNumberingSystem * unumsys_openByName_android(const char * name, UErrorCode * status) {
  return unumsys_openByName(name, status);
}
void uplrules_close_android(UPluralRules * uplrules) {
  uplrules_close(uplrules);
}
UEnumeration * uplrules_getKeywords_android(const UPluralRules * uplrules, UErrorCode * status) {
  return uplrules_getKeywords(uplrules, status);
}
UPluralRules * uplrules_open_android(const char * locale, UErrorCode * status) {
  return uplrules_open(locale, status);
}
UPluralRules * uplrules_openForType_android(const char * locale, UPluralType type, UErrorCode * status) {
  return uplrules_openForType(locale, type, status);
}
int32_t uplrules_select_android(const UPluralRules * uplrules, double number, UChar * keyword, int32_t capacity, UErrorCode * status) {
  return uplrules_select(uplrules, number, keyword, capacity, status);
}
int32_t uregex_appendReplacement_android(URegularExpression * regexp, const UChar * replacementText, int32_t replacementLength, UChar ** destBuf, int32_t * destCapacity, UErrorCode * status) {
  return uregex_appendReplacement(regexp, replacementText, replacementLength, destBuf, destCapacity, status);
}
void uregex_appendReplacementUText_android(URegularExpression * regexp, UText * replacementText, UText * dest, UErrorCode * status) {
  uregex_appendReplacementUText(regexp, replacementText, dest, status);
}
int32_t uregex_appendTail_android(URegularExpression * regexp, UChar ** destBuf, int32_t * destCapacity, UErrorCode * status) {
  return uregex_appendTail(regexp, destBuf, destCapacity, status);
}
UText * uregex_appendTailUText_android(URegularExpression * regexp, UText * dest, UErrorCode * status) {
  return uregex_appendTailUText(regexp, dest, status);
}
URegularExpression * uregex_clone_android(const URegularExpression * regexp, UErrorCode * status) {
  return uregex_clone(regexp, status);
}
void uregex_close_android(URegularExpression * regexp) {
  uregex_close(regexp);
}
int32_t uregex_end_android(URegularExpression * regexp, int32_t groupNum, UErrorCode * status) {
  return uregex_end(regexp, groupNum, status);
}
int64_t uregex_end64_android(URegularExpression * regexp, int32_t groupNum, UErrorCode * status) {
  return uregex_end64(regexp, groupNum, status);
}
UBool uregex_find_android(URegularExpression * regexp, int32_t startIndex, UErrorCode * status) {
  return uregex_find(regexp, startIndex, status);
}
UBool uregex_find64_android(URegularExpression * regexp, int64_t startIndex, UErrorCode * status) {
  return uregex_find64(regexp, startIndex, status);
}
UBool uregex_findNext_android(URegularExpression * regexp, UErrorCode * status) {
  return uregex_findNext(regexp, status);
}
int32_t uregex_flags_android(const URegularExpression * regexp, UErrorCode * status) {
  return uregex_flags(regexp, status);
}
void uregex_getFindProgressCallback_android(const URegularExpression * regexp, URegexFindProgressCallback ** callback, const void ** context, UErrorCode * status) {
  uregex_getFindProgressCallback(regexp, callback, context, status);
}
void uregex_getMatchCallback_android(const URegularExpression * regexp, URegexMatchCallback ** callback, const void ** context, UErrorCode * status) {
  uregex_getMatchCallback(regexp, callback, context, status);
}
int32_t uregex_getStackLimit_android(const URegularExpression * regexp, UErrorCode * status) {
  return uregex_getStackLimit(regexp, status);
}
const UChar * uregex_getText_android(URegularExpression * regexp, int32_t * textLength, UErrorCode * status) {
  return uregex_getText(regexp, textLength, status);
}
int32_t uregex_getTimeLimit_android(const URegularExpression * regexp, UErrorCode * status) {
  return uregex_getTimeLimit(regexp, status);
}
UText * uregex_getUText_android(URegularExpression * regexp, UText * dest, UErrorCode * status) {
  return uregex_getUText(regexp, dest, status);
}
int32_t uregex_group_android(URegularExpression * regexp, int32_t groupNum, UChar * dest, int32_t destCapacity, UErrorCode * status) {
  return uregex_group(regexp, groupNum, dest, destCapacity, status);
}
int32_t uregex_groupCount_android(URegularExpression * regexp, UErrorCode * status) {
  return uregex_groupCount(regexp, status);
}
int32_t uregex_groupNumberFromCName_android(URegularExpression * regexp, const char * groupName, int32_t nameLength, UErrorCode * status) {
  return uregex_groupNumberFromCName(regexp, groupName, nameLength, status);
}
int32_t uregex_groupNumberFromName_android(URegularExpression * regexp, const UChar * groupName, int32_t nameLength, UErrorCode * status) {
  return uregex_groupNumberFromName(regexp, groupName, nameLength, status);
}
UText * uregex_groupUText_android(URegularExpression * regexp, int32_t groupNum, UText * dest, int64_t * groupLength, UErrorCode * status) {
  return uregex_groupUText(regexp, groupNum, dest, groupLength, status);
}
UBool uregex_hasAnchoringBounds_android(const URegularExpression * regexp, UErrorCode * status) {
  return uregex_hasAnchoringBounds(regexp, status);
}
UBool uregex_hasTransparentBounds_android(const URegularExpression * regexp, UErrorCode * status) {
  return uregex_hasTransparentBounds(regexp, status);
}
UBool uregex_hitEnd_android(const URegularExpression * regexp, UErrorCode * status) {
  return uregex_hitEnd(regexp, status);
}
UBool uregex_lookingAt_android(URegularExpression * regexp, int32_t startIndex, UErrorCode * status) {
  return uregex_lookingAt(regexp, startIndex, status);
}
UBool uregex_lookingAt64_android(URegularExpression * regexp, int64_t startIndex, UErrorCode * status) {
  return uregex_lookingAt64(regexp, startIndex, status);
}
UBool uregex_matches_android(URegularExpression * regexp, int32_t startIndex, UErrorCode * status) {
  return uregex_matches(regexp, startIndex, status);
}
UBool uregex_matches64_android(URegularExpression * regexp, int64_t startIndex, UErrorCode * status) {
  return uregex_matches64(regexp, startIndex, status);
}
URegularExpression * uregex_open_android(const UChar * pattern, int32_t patternLength, uint32_t flags, UParseError * pe, UErrorCode * status) {
  return uregex_open(pattern, patternLength, flags, pe, status);
}
URegularExpression * uregex_openC_android(const char * pattern, uint32_t flags, UParseError * pe, UErrorCode * status) {
  return uregex_openC(pattern, flags, pe, status);
}
URegularExpression * uregex_openUText_android(UText * pattern, uint32_t flags, UParseError * pe, UErrorCode * status) {
  return uregex_openUText(pattern, flags, pe, status);
}
const UChar * uregex_pattern_android(const URegularExpression * regexp, int32_t * patLength, UErrorCode * status) {
  return uregex_pattern(regexp, patLength, status);
}
UText * uregex_patternUText_android(const URegularExpression * regexp, UErrorCode * status) {
  return uregex_patternUText(regexp, status);
}
void uregex_refreshUText_android(URegularExpression * regexp, UText * text, UErrorCode * status) {
  uregex_refreshUText(regexp, text, status);
}
int32_t uregex_regionEnd_android(const URegularExpression * regexp, UErrorCode * status) {
  return uregex_regionEnd(regexp, status);
}
int64_t uregex_regionEnd64_android(const URegularExpression * regexp, UErrorCode * status) {
  return uregex_regionEnd64(regexp, status);
}
int32_t uregex_regionStart_android(const URegularExpression * regexp, UErrorCode * status) {
  return uregex_regionStart(regexp, status);
}
int64_t uregex_regionStart64_android(const URegularExpression * regexp, UErrorCode * status) {
  return uregex_regionStart64(regexp, status);
}
int32_t uregex_replaceAll_android(URegularExpression * regexp, const UChar * replacementText, int32_t replacementLength, UChar * destBuf, int32_t destCapacity, UErrorCode * status) {
  return uregex_replaceAll(regexp, replacementText, replacementLength, destBuf, destCapacity, status);
}
UText * uregex_replaceAllUText_android(URegularExpression * regexp, UText * replacement, UText * dest, UErrorCode * status) {
  return uregex_replaceAllUText(regexp, replacement, dest, status);
}
int32_t uregex_replaceFirst_android(URegularExpression * regexp, const UChar * replacementText, int32_t replacementLength, UChar * destBuf, int32_t destCapacity, UErrorCode * status) {
  return uregex_replaceFirst(regexp, replacementText, replacementLength, destBuf, destCapacity, status);
}
UText * uregex_replaceFirstUText_android(URegularExpression * regexp, UText * replacement, UText * dest, UErrorCode * status) {
  return uregex_replaceFirstUText(regexp, replacement, dest, status);
}
UBool uregex_requireEnd_android(const URegularExpression * regexp, UErrorCode * status) {
  return uregex_requireEnd(regexp, status);
}
void uregex_reset_android(URegularExpression * regexp, int32_t index, UErrorCode * status) {
  uregex_reset(regexp, index, status);
}
void uregex_reset64_android(URegularExpression * regexp, int64_t index, UErrorCode * status) {
  uregex_reset64(regexp, index, status);
}
void uregex_setFindProgressCallback_android(URegularExpression * regexp, URegexFindProgressCallback * callback, const void * context, UErrorCode * status) {
  uregex_setFindProgressCallback(regexp, callback, context, status);
}
void uregex_setMatchCallback_android(URegularExpression * regexp, URegexMatchCallback * callback, const void * context, UErrorCode * status) {
  uregex_setMatchCallback(regexp, callback, context, status);
}
void uregex_setRegion_android(URegularExpression * regexp, int32_t regionStart, int32_t regionLimit, UErrorCode * status) {
  uregex_setRegion(regexp, regionStart, regionLimit, status);
}
void uregex_setRegion64_android(URegularExpression * regexp, int64_t regionStart, int64_t regionLimit, UErrorCode * status) {
  uregex_setRegion64(regexp, regionStart, regionLimit, status);
}
void uregex_setRegionAndStart_android(URegularExpression * regexp, int64_t regionStart, int64_t regionLimit, int64_t startIndex, UErrorCode * status) {
  uregex_setRegionAndStart(regexp, regionStart, regionLimit, startIndex, status);
}
void uregex_setStackLimit_android(URegularExpression * regexp, int32_t limit, UErrorCode * status) {
  uregex_setStackLimit(regexp, limit, status);
}
void uregex_setText_android(URegularExpression * regexp, const UChar * text, int32_t textLength, UErrorCode * status) {
  uregex_setText(regexp, text, textLength, status);
}
void uregex_setTimeLimit_android(URegularExpression * regexp, int32_t limit, UErrorCode * status) {
  uregex_setTimeLimit(regexp, limit, status);
}
void uregex_setUText_android(URegularExpression * regexp, UText * text, UErrorCode * status) {
  uregex_setUText(regexp, text, status);
}
int32_t uregex_split_android(URegularExpression * regexp, UChar * destBuf, int32_t destCapacity, int32_t * requiredCapacity, UChar * destFields[], int32_t destFieldsCapacity, UErrorCode * status) {
  return uregex_split(regexp, destBuf, destCapacity, requiredCapacity, destFields, destFieldsCapacity, status);
}
int32_t uregex_splitUText_android(URegularExpression * regexp, UText * destFields[], int32_t destFieldsCapacity, UErrorCode * status) {
  return uregex_splitUText(regexp, destFields, destFieldsCapacity, status);
}
int32_t uregex_start_android(URegularExpression * regexp, int32_t groupNum, UErrorCode * status) {
  return uregex_start(regexp, groupNum, status);
}
int64_t uregex_start64_android(URegularExpression * regexp, int32_t groupNum, UErrorCode * status) {
  return uregex_start64(regexp, groupNum, status);
}
void uregex_useAnchoringBounds_android(URegularExpression * regexp, UBool b, UErrorCode * status) {
  uregex_useAnchoringBounds(regexp, b, status);
}
void uregex_useTransparentBounds_android(URegularExpression * regexp, UBool b, UErrorCode * status) {
  uregex_useTransparentBounds(regexp, b, status);
}
UBool uregion_areEqual_android(const URegion * uregion, const URegion * otherRegion) {
  return uregion_areEqual(uregion, otherRegion);
}
UBool uregion_contains_android(const URegion * uregion, const URegion * otherRegion) {
  return uregion_contains(uregion, otherRegion);
}
UEnumeration * uregion_getAvailable_android(URegionType type, UErrorCode * status) {
  return uregion_getAvailable(type, status);
}
UEnumeration * uregion_getContainedRegions_android(const URegion * uregion, UErrorCode * status) {
  return uregion_getContainedRegions(uregion, status);
}
UEnumeration * uregion_getContainedRegionsOfType_android(const URegion * uregion, URegionType type, UErrorCode * status) {
  return uregion_getContainedRegionsOfType(uregion, type, status);
}
const URegion * uregion_getContainingRegion_android(const URegion * uregion) {
  return uregion_getContainingRegion(uregion);
}
const URegion * uregion_getContainingRegionOfType_android(const URegion * uregion, URegionType type) {
  return uregion_getContainingRegionOfType(uregion, type);
}
int32_t uregion_getNumericCode_android(const URegion * uregion) {
  return uregion_getNumericCode(uregion);
}
UEnumeration * uregion_getPreferredValues_android(const URegion * uregion, UErrorCode * status) {
  return uregion_getPreferredValues(uregion, status);
}
const char * uregion_getRegionCode_android(const URegion * uregion) {
  return uregion_getRegionCode(uregion);
}
const URegion * uregion_getRegionFromCode_android(const char * regionCode, UErrorCode * status) {
  return uregion_getRegionFromCode(regionCode, status);
}
const URegion * uregion_getRegionFromNumericCode_android(int32_t code, UErrorCode * status) {
  return uregion_getRegionFromNumericCode(code, status);
}
URegionType uregion_getType_android(const URegion * uregion) {
  return uregion_getType(uregion);
}
void ureldatefmt_close_android(URelativeDateTimeFormatter * reldatefmt) {
  ureldatefmt_close(reldatefmt);
}
int32_t ureldatefmt_combineDateAndTime_android(const URelativeDateTimeFormatter * reldatefmt, const UChar * relativeDateString, int32_t relativeDateStringLen, const UChar * timeString, int32_t timeStringLen, UChar * result, int32_t resultCapacity, UErrorCode * status) {
  return ureldatefmt_combineDateAndTime(reldatefmt, relativeDateString, relativeDateStringLen, timeString, timeStringLen, result, resultCapacity, status);
}
int32_t ureldatefmt_format_android(const URelativeDateTimeFormatter * reldatefmt, double offset, URelativeDateTimeUnit unit, UChar * result, int32_t resultCapacity, UErrorCode * status) {
  return ureldatefmt_format(reldatefmt, offset, unit, result, resultCapacity, status);
}
int32_t ureldatefmt_formatNumeric_android(const URelativeDateTimeFormatter * reldatefmt, double offset, URelativeDateTimeUnit unit, UChar * result, int32_t resultCapacity, UErrorCode * status) {
  return ureldatefmt_formatNumeric(reldatefmt, offset, unit, result, resultCapacity, status);
}
URelativeDateTimeFormatter * ureldatefmt_open_android(const char * locale, UNumberFormat * nfToAdopt, UDateRelativeDateTimeFormatterStyle width, UDisplayContext capitalizationContext, UErrorCode * status) {
  return ureldatefmt_open(locale, nfToAdopt, width, capitalizationContext, status);
}
void ures_close_android(UResourceBundle * resourceBundle) {
  ures_close(resourceBundle);
}
const uint8_t * ures_getBinary_android(const UResourceBundle * resourceBundle, int32_t * len, UErrorCode * status) {
  return ures_getBinary(resourceBundle, len, status);
}
UResourceBundle * ures_getByIndex_android(const UResourceBundle * resourceBundle, int32_t indexR, UResourceBundle * fillIn, UErrorCode * status) {
  return ures_getByIndex(resourceBundle, indexR, fillIn, status);
}
UResourceBundle * ures_getByKey_android(const UResourceBundle * resourceBundle, const char * key, UResourceBundle * fillIn, UErrorCode * status) {
  return ures_getByKey(resourceBundle, key, fillIn, status);
}
int32_t ures_getInt_android(const UResourceBundle * resourceBundle, UErrorCode * status) {
  return ures_getInt(resourceBundle, status);
}
const int32_t * ures_getIntVector_android(const UResourceBundle * resourceBundle, int32_t * len, UErrorCode * status) {
  return ures_getIntVector(resourceBundle, len, status);
}
const char * ures_getKey_android(const UResourceBundle * resourceBundle) {
  return ures_getKey(resourceBundle);
}
const char * ures_getLocaleByType_android(const UResourceBundle * resourceBundle, ULocDataLocaleType type, UErrorCode * status) {
  return ures_getLocaleByType(resourceBundle, type, status);
}
UResourceBundle * ures_getNextResource_android(UResourceBundle * resourceBundle, UResourceBundle * fillIn, UErrorCode * status) {
  return ures_getNextResource(resourceBundle, fillIn, status);
}
const UChar * ures_getNextString_android(UResourceBundle * resourceBundle, int32_t * len, const char ** key, UErrorCode * status) {
  return ures_getNextString(resourceBundle, len, key, status);
}
int32_t ures_getSize_android(const UResourceBundle * resourceBundle) {
  return ures_getSize(resourceBundle);
}
const UChar * ures_getString_android(const UResourceBundle * resourceBundle, int32_t * len, UErrorCode * status) {
  return ures_getString(resourceBundle, len, status);
}
const UChar * ures_getStringByIndex_android(const UResourceBundle * resourceBundle, int32_t indexS, int32_t * len, UErrorCode * status) {
  return ures_getStringByIndex(resourceBundle, indexS, len, status);
}
const UChar * ures_getStringByKey_android(const UResourceBundle * resB, const char * key, int32_t * len, UErrorCode * status) {
  return ures_getStringByKey(resB, key, len, status);
}
UResType ures_getType_android(const UResourceBundle * resourceBundle) {
  return ures_getType(resourceBundle);
}
uint32_t ures_getUInt_android(const UResourceBundle * resourceBundle, UErrorCode * status) {
  return ures_getUInt(resourceBundle, status);
}
const char * ures_getUTF8String_android(const UResourceBundle * resB, char * dest, int32_t * length, UBool forceCopy, UErrorCode * status) {
  return ures_getUTF8String(resB, dest, length, forceCopy, status);
}
const char * ures_getUTF8StringByIndex_android(const UResourceBundle * resB, int32_t stringIndex, char * dest, int32_t * pLength, UBool forceCopy, UErrorCode * status) {
  return ures_getUTF8StringByIndex(resB, stringIndex, dest, pLength, forceCopy, status);
}
const char * ures_getUTF8StringByKey_android(const UResourceBundle * resB, const char * key, char * dest, int32_t * pLength, UBool forceCopy, UErrorCode * status) {
  return ures_getUTF8StringByKey(resB, key, dest, pLength, forceCopy, status);
}
void ures_getVersion_android(const UResourceBundle * resB, UVersionInfo versionInfo) {
  ures_getVersion(resB, versionInfo);
}
UBool ures_hasNext_android(const UResourceBundle * resourceBundle) {
  return ures_hasNext(resourceBundle);
}
UResourceBundle * ures_open_android(const char * packageName, const char * locale, UErrorCode * status) {
  return ures_open(packageName, locale, status);
}
UEnumeration * ures_openAvailableLocales_android(const char * packageName, UErrorCode * status) {
  return ures_openAvailableLocales(packageName, status);
}
UResourceBundle * ures_openDirect_android(const char * packageName, const char * locale, UErrorCode * status) {
  return ures_openDirect(packageName, locale, status);
}
UResourceBundle * ures_openU_android(const UChar * packageName, const char * locale, UErrorCode * status) {
  return ures_openU(packageName, locale, status);
}
void ures_resetIterator_android(UResourceBundle * resourceBundle) {
  ures_resetIterator(resourceBundle);
}
UBool uscript_breaksBetweenLetters_android(UScriptCode script) {
  return uscript_breaksBetweenLetters(script);
}
int32_t uscript_getCode_android(const char * nameOrAbbrOrLocale, UScriptCode * fillIn, int32_t capacity, UErrorCode * err) {
  return uscript_getCode(nameOrAbbrOrLocale, fillIn, capacity, err);
}
const char * uscript_getName_android(UScriptCode scriptCode) {
  return uscript_getName(scriptCode);
}
int32_t uscript_getSampleString_android(UScriptCode script, UChar * dest, int32_t capacity, UErrorCode * pErrorCode) {
  return uscript_getSampleString(script, dest, capacity, pErrorCode);
}
UScriptCode uscript_getScript_android(UChar32 codepoint, UErrorCode * err) {
  return uscript_getScript(codepoint, err);
}
int32_t uscript_getScriptExtensions_android(UChar32 c, UScriptCode * scripts, int32_t capacity, UErrorCode * errorCode) {
  return uscript_getScriptExtensions(c, scripts, capacity, errorCode);
}
const char * uscript_getShortName_android(UScriptCode scriptCode) {
  return uscript_getShortName(scriptCode);
}
UScriptUsage uscript_getUsage_android(UScriptCode script) {
  return uscript_getUsage(script);
}
UBool uscript_hasScript_android(UChar32 c, UScriptCode sc) {
  return uscript_hasScript(c, sc);
}
UBool uscript_isCased_android(UScriptCode script) {
  return uscript_isCased(script);
}
UBool uscript_isRightToLeft_android(UScriptCode script) {
  return uscript_isRightToLeft(script);
}
void usearch_close_android(UStringSearch * searchiter) {
  usearch_close(searchiter);
}
int32_t usearch_first_android(UStringSearch * strsrch, UErrorCode * status) {
  return usearch_first(strsrch, status);
}
int32_t usearch_following_android(UStringSearch * strsrch, int32_t position, UErrorCode * status) {
  return usearch_following(strsrch, position, status);
}
USearchAttributeValue usearch_getAttribute_android(const UStringSearch * strsrch, USearchAttribute attribute) {
  return usearch_getAttribute(strsrch, attribute);
}
const UBreakIterator * usearch_getBreakIterator_android(const UStringSearch * strsrch) {
  return usearch_getBreakIterator(strsrch);
}
UCollator * usearch_getCollator_android(const UStringSearch * strsrch) {
  return usearch_getCollator(strsrch);
}
int32_t usearch_getMatchedLength_android(const UStringSearch * strsrch) {
  return usearch_getMatchedLength(strsrch);
}
int32_t usearch_getMatchedStart_android(const UStringSearch * strsrch) {
  return usearch_getMatchedStart(strsrch);
}
int32_t usearch_getMatchedText_android(const UStringSearch * strsrch, UChar * result, int32_t resultCapacity, UErrorCode * status) {
  return usearch_getMatchedText(strsrch, result, resultCapacity, status);
}
int32_t usearch_getOffset_android(const UStringSearch * strsrch) {
  return usearch_getOffset(strsrch);
}
const UChar * usearch_getPattern_android(const UStringSearch * strsrch, int32_t * length) {
  return usearch_getPattern(strsrch, length);
}
const UChar * usearch_getText_android(const UStringSearch * strsrch, int32_t * length) {
  return usearch_getText(strsrch, length);
}
int32_t usearch_last_android(UStringSearch * strsrch, UErrorCode * status) {
  return usearch_last(strsrch, status);
}
int32_t usearch_next_android(UStringSearch * strsrch, UErrorCode * status) {
  return usearch_next(strsrch, status);
}
UStringSearch * usearch_open_android(const UChar * pattern, int32_t patternlength, const UChar * text, int32_t textlength, const char * locale, UBreakIterator * breakiter, UErrorCode * status) {
  return usearch_open(pattern, patternlength, text, textlength, locale, breakiter, status);
}
UStringSearch * usearch_openFromCollator_android(const UChar * pattern, int32_t patternlength, const UChar * text, int32_t textlength, const UCollator * collator, UBreakIterator * breakiter, UErrorCode * status) {
  return usearch_openFromCollator(pattern, patternlength, text, textlength, collator, breakiter, status);
}
int32_t usearch_preceding_android(UStringSearch * strsrch, int32_t position, UErrorCode * status) {
  return usearch_preceding(strsrch, position, status);
}
int32_t usearch_previous_android(UStringSearch * strsrch, UErrorCode * status) {
  return usearch_previous(strsrch, status);
}
void usearch_reset_android(UStringSearch * strsrch) {
  usearch_reset(strsrch);
}
void usearch_setAttribute_android(UStringSearch * strsrch, USearchAttribute attribute, USearchAttributeValue value, UErrorCode * status) {
  usearch_setAttribute(strsrch, attribute, value, status);
}
void usearch_setBreakIterator_android(UStringSearch * strsrch, UBreakIterator * breakiter, UErrorCode * status) {
  usearch_setBreakIterator(strsrch, breakiter, status);
}
void usearch_setCollator_android(UStringSearch * strsrch, const UCollator * collator, UErrorCode * status) {
  usearch_setCollator(strsrch, collator, status);
}
void usearch_setOffset_android(UStringSearch * strsrch, int32_t position, UErrorCode * status) {
  usearch_setOffset(strsrch, position, status);
}
void usearch_setPattern_android(UStringSearch * strsrch, const UChar * pattern, int32_t patternlength, UErrorCode * status) {
  usearch_setPattern(strsrch, pattern, patternlength, status);
}
void usearch_setText_android(UStringSearch * strsrch, const UChar * text, int32_t textlength, UErrorCode * status) {
  usearch_setText(strsrch, text, textlength, status);
}
void uset_add_android(USet * set, UChar32 c) {
  uset_add(set, c);
}
void uset_addAll_android(USet * set, const USet * additionalSet) {
  uset_addAll(set, additionalSet);
}
void uset_addAllCodePoints_android(USet * set, const UChar * str, int32_t strLen) {
  uset_addAllCodePoints(set, str, strLen);
}
void uset_addRange_android(USet * set, UChar32 start, UChar32 end) {
  uset_addRange(set, start, end);
}
void uset_addString_android(USet * set, const UChar * str, int32_t strLen) {
  uset_addString(set, str, strLen);
}
void uset_applyIntPropertyValue_android(USet * set, UProperty prop, int32_t value, UErrorCode * ec) {
  uset_applyIntPropertyValue(set, prop, value, ec);
}
int32_t uset_applyPattern_android(USet * set, const UChar * pattern, int32_t patternLength, uint32_t options, UErrorCode * status) {
  return uset_applyPattern(set, pattern, patternLength, options, status);
}
void uset_applyPropertyAlias_android(USet * set, const UChar * prop, int32_t propLength, const UChar * value, int32_t valueLength, UErrorCode * ec) {
  uset_applyPropertyAlias(set, prop, propLength, value, valueLength, ec);
}
UChar32 uset_charAt_android(const USet * set, int32_t charIndex) {
  return uset_charAt(set, charIndex);
}
void uset_clear_android(USet * set) {
  uset_clear(set);
}
USet * uset_clone_android(const USet * set) {
  return uset_clone(set);
}
USet * uset_cloneAsThawed_android(const USet * set) {
  return uset_cloneAsThawed(set);
}
void uset_close_android(USet * set) {
  uset_close(set);
}
void uset_closeOver_android(USet * set, int32_t attributes) {
  uset_closeOver(set, attributes);
}
void uset_compact_android(USet * set) {
  uset_compact(set);
}
void uset_complement_android(USet * set) {
  uset_complement(set);
}
void uset_complementAll_android(USet * set, const USet * complement) {
  uset_complementAll(set, complement);
}
UBool uset_contains_android(const USet * set, UChar32 c) {
  return uset_contains(set, c);
}
UBool uset_containsAll_android(const USet * set1, const USet * set2) {
  return uset_containsAll(set1, set2);
}
UBool uset_containsAllCodePoints_android(const USet * set, const UChar * str, int32_t strLen) {
  return uset_containsAllCodePoints(set, str, strLen);
}
UBool uset_containsNone_android(const USet * set1, const USet * set2) {
  return uset_containsNone(set1, set2);
}
UBool uset_containsRange_android(const USet * set, UChar32 start, UChar32 end) {
  return uset_containsRange(set, start, end);
}
UBool uset_containsSome_android(const USet * set1, const USet * set2) {
  return uset_containsSome(set1, set2);
}
UBool uset_containsString_android(const USet * set, const UChar * str, int32_t strLen) {
  return uset_containsString(set, str, strLen);
}
UBool uset_equals_android(const USet * set1, const USet * set2) {
  return uset_equals(set1, set2);
}
void uset_freeze_android(USet * set) {
  uset_freeze(set);
}
int32_t uset_getItem_android(const USet * set, int32_t itemIndex, UChar32 * start, UChar32 * end, UChar * str, int32_t strCapacity, UErrorCode * ec) {
  return uset_getItem(set, itemIndex, start, end, str, strCapacity, ec);
}
int32_t uset_getItemCount_android(const USet * set) {
  return uset_getItemCount(set);
}
UBool uset_getSerializedRange_android(const USerializedSet * set, int32_t rangeIndex, UChar32 * pStart, UChar32 * pEnd) {
  return uset_getSerializedRange(set, rangeIndex, pStart, pEnd);
}
int32_t uset_getSerializedRangeCount_android(const USerializedSet * set) {
  return uset_getSerializedRangeCount(set);
}
UBool uset_getSerializedSet_android(USerializedSet * fillSet, const uint16_t * src, int32_t srcLength) {
  return uset_getSerializedSet(fillSet, src, srcLength);
}
int32_t uset_indexOf_android(const USet * set, UChar32 c) {
  return uset_indexOf(set, c);
}
UBool uset_isEmpty_android(const USet * set) {
  return uset_isEmpty(set);
}
UBool uset_isFrozen_android(const USet * set) {
  return uset_isFrozen(set);
}
USet * uset_open_android(UChar32 start, UChar32 end) {
  return uset_open(start, end);
}
USet * uset_openEmpty_android() {
  return uset_openEmpty();
}
USet * uset_openPattern_android(const UChar * pattern, int32_t patternLength, UErrorCode * ec) {
  return uset_openPattern(pattern, patternLength, ec);
}
USet * uset_openPatternOptions_android(const UChar * pattern, int32_t patternLength, uint32_t options, UErrorCode * ec) {
  return uset_openPatternOptions(pattern, patternLength, options, ec);
}
void uset_remove_android(USet * set, UChar32 c) {
  uset_remove(set, c);
}
void uset_removeAll_android(USet * set, const USet * removeSet) {
  uset_removeAll(set, removeSet);
}
void uset_removeAllStrings_android(USet * set) {
  uset_removeAllStrings(set);
}
void uset_removeRange_android(USet * set, UChar32 start, UChar32 end) {
  uset_removeRange(set, start, end);
}
void uset_removeString_android(USet * set, const UChar * str, int32_t strLen) {
  uset_removeString(set, str, strLen);
}
UBool uset_resemblesPattern_android(const UChar * pattern, int32_t patternLength, int32_t pos) {
  return uset_resemblesPattern(pattern, patternLength, pos);
}
void uset_retain_android(USet * set, UChar32 start, UChar32 end) {
  uset_retain(set, start, end);
}
void uset_retainAll_android(USet * set, const USet * retain) {
  uset_retainAll(set, retain);
}
int32_t uset_serialize_android(const USet * set, uint16_t * dest, int32_t destCapacity, UErrorCode * pErrorCode) {
  return uset_serialize(set, dest, destCapacity, pErrorCode);
}
UBool uset_serializedContains_android(const USerializedSet * set, UChar32 c) {
  return uset_serializedContains(set, c);
}
void uset_set_android(USet * set, UChar32 start, UChar32 end) {
  uset_set(set, start, end);
}
void uset_setSerializedToOne_android(USerializedSet * fillSet, UChar32 c) {
  uset_setSerializedToOne(fillSet, c);
}
int32_t uset_size_android(const USet * set) {
  return uset_size(set);
}
int32_t uset_span_android(const USet * set, const UChar * s, int32_t length, USetSpanCondition spanCondition) {
  return uset_span(set, s, length, spanCondition);
}
int32_t uset_spanBack_android(const USet * set, const UChar * s, int32_t length, USetSpanCondition spanCondition) {
  return uset_spanBack(set, s, length, spanCondition);
}
int32_t uset_spanBackUTF8_android(const USet * set, const char * s, int32_t length, USetSpanCondition spanCondition) {
  return uset_spanBackUTF8(set, s, length, spanCondition);
}
int32_t uset_spanUTF8_android(const USet * set, const char * s, int32_t length, USetSpanCondition spanCondition) {
  return uset_spanUTF8(set, s, length, spanCondition);
}
int32_t uset_toPattern_android(const USet * set, UChar * result, int32_t resultCapacity, UBool escapeUnprintable, UErrorCode * ec) {
  return uset_toPattern(set, result, resultCapacity, escapeUnprintable, ec);
}
int32_t uspoof_areConfusable_android(const USpoofChecker * sc, const UChar * id1, int32_t length1, const UChar * id2, int32_t length2, UErrorCode * status) {
  return uspoof_areConfusable(sc, id1, length1, id2, length2, status);
}
int32_t uspoof_areConfusableUTF8_android(const USpoofChecker * sc, const char * id1, int32_t length1, const char * id2, int32_t length2, UErrorCode * status) {
  return uspoof_areConfusableUTF8(sc, id1, length1, id2, length2, status);
}
int32_t uspoof_check_android(const USpoofChecker * sc, const UChar * id, int32_t length, int32_t * position, UErrorCode * status) {
  return uspoof_check(sc, id, length, position, status);
}
int32_t uspoof_check2_android(const USpoofChecker * sc, const UChar * id, int32_t length, USpoofCheckResult * checkResult, UErrorCode * status) {
  return uspoof_check2(sc, id, length, checkResult, status);
}
int32_t uspoof_check2UTF8_android(const USpoofChecker * sc, const char * id, int32_t length, USpoofCheckResult * checkResult, UErrorCode * status) {
  return uspoof_check2UTF8(sc, id, length, checkResult, status);
}
int32_t uspoof_checkUTF8_android(const USpoofChecker * sc, const char * id, int32_t length, int32_t * position, UErrorCode * status) {
  return uspoof_checkUTF8(sc, id, length, position, status);
}
USpoofChecker * uspoof_clone_android(const USpoofChecker * sc, UErrorCode * status) {
  return uspoof_clone(sc, status);
}
void uspoof_close_android(USpoofChecker * sc) {
  uspoof_close(sc);
}
void uspoof_closeCheckResult_android(USpoofCheckResult * checkResult) {
  uspoof_closeCheckResult(checkResult);
}
const USet * uspoof_getAllowedChars_android(const USpoofChecker * sc, UErrorCode * status) {
  return uspoof_getAllowedChars(sc, status);
}
const char * uspoof_getAllowedLocales_android(USpoofChecker * sc, UErrorCode * status) {
  return uspoof_getAllowedLocales(sc, status);
}
int32_t uspoof_getCheckResultChecks_android(const USpoofCheckResult * checkResult, UErrorCode * status) {
  return uspoof_getCheckResultChecks(checkResult, status);
}
const USet * uspoof_getCheckResultNumerics_android(const USpoofCheckResult * checkResult, UErrorCode * status) {
  return uspoof_getCheckResultNumerics(checkResult, status);
}
URestrictionLevel uspoof_getCheckResultRestrictionLevel_android(const USpoofCheckResult * checkResult, UErrorCode * status) {
  return uspoof_getCheckResultRestrictionLevel(checkResult, status);
}
int32_t uspoof_getChecks_android(const USpoofChecker * sc, UErrorCode * status) {
  return uspoof_getChecks(sc, status);
}
const USet * uspoof_getInclusionSet_android(UErrorCode * status) {
  return uspoof_getInclusionSet(status);
}
const USet * uspoof_getRecommendedSet_android(UErrorCode * status) {
  return uspoof_getRecommendedSet(status);
}
URestrictionLevel uspoof_getRestrictionLevel_android(const USpoofChecker * sc) {
  return uspoof_getRestrictionLevel(sc);
}
int32_t uspoof_getSkeleton_android(const USpoofChecker * sc, uint32_t type, const UChar * id, int32_t length, UChar * dest, int32_t destCapacity, UErrorCode * status) {
  return uspoof_getSkeleton(sc, type, id, length, dest, destCapacity, status);
}
int32_t uspoof_getSkeletonUTF8_android(const USpoofChecker * sc, uint32_t type, const char * id, int32_t length, char * dest, int32_t destCapacity, UErrorCode * status) {
  return uspoof_getSkeletonUTF8(sc, type, id, length, dest, destCapacity, status);
}
USpoofChecker * uspoof_open_android(UErrorCode * status) {
  return uspoof_open(status);
}
USpoofCheckResult * uspoof_openCheckResult_android(UErrorCode * status) {
  return uspoof_openCheckResult(status);
}
USpoofChecker * uspoof_openFromSerialized_android(const void * data, int32_t length, int32_t * pActualLength, UErrorCode * pErrorCode) {
  return uspoof_openFromSerialized(data, length, pActualLength, pErrorCode);
}
USpoofChecker * uspoof_openFromSource_android(const char * confusables, int32_t confusablesLen, const char * confusablesWholeScript, int32_t confusablesWholeScriptLen, int32_t * errType, UParseError * pe, UErrorCode * status) {
  return uspoof_openFromSource(confusables, confusablesLen, confusablesWholeScript, confusablesWholeScriptLen, errType, pe, status);
}
int32_t uspoof_serialize_android(USpoofChecker * sc, void * data, int32_t capacity, UErrorCode * status) {
  return uspoof_serialize(sc, data, capacity, status);
}
void uspoof_setAllowedChars_android(USpoofChecker * sc, const USet * chars, UErrorCode * status) {
  uspoof_setAllowedChars(sc, chars, status);
}
void uspoof_setAllowedLocales_android(USpoofChecker * sc, const char * localesList, UErrorCode * status) {
  uspoof_setAllowedLocales(sc, localesList, status);
}
void uspoof_setChecks_android(USpoofChecker * sc, int32_t checks, UErrorCode * status) {
  uspoof_setChecks(sc, checks, status);
}
void uspoof_setRestrictionLevel_android(USpoofChecker * sc, URestrictionLevel restrictionLevel) {
  uspoof_setRestrictionLevel(sc, restrictionLevel);
}
void usprep_close_android(UStringPrepProfile * profile) {
  usprep_close(profile);
}
UStringPrepProfile * usprep_open_android(const char * path, const char * fileName, UErrorCode * status) {
  return usprep_open(path, fileName, status);
}
UStringPrepProfile * usprep_openByType_android(UStringPrepProfileType type, UErrorCode * status) {
  return usprep_openByType(type, status);
}
int32_t usprep_prepare_android(const UStringPrepProfile * prep, const UChar * src, int32_t srcLength, UChar * dest, int32_t destCapacity, int32_t options, UParseError * parseError, UErrorCode * status) {
  return usprep_prepare(prep, src, srcLength, dest, destCapacity, options, parseError, status);
}
UChar32 utext_char32At_android(UText * ut, int64_t nativeIndex) {
  return utext_char32At(ut, nativeIndex);
}
UText * utext_clone_android(UText * dest, const UText * src, UBool deep, UBool readOnly, UErrorCode * status) {
  return utext_clone(dest, src, deep, readOnly, status);
}
UText * utext_close_android(UText * ut) {
  return utext_close(ut);
}
void utext_copy_android(UText * ut, int64_t nativeStart, int64_t nativeLimit, int64_t destIndex, UBool move, UErrorCode * status) {
  utext_copy(ut, nativeStart, nativeLimit, destIndex, move, status);
}
UChar32 utext_current32_android(UText * ut) {
  return utext_current32(ut);
}
UBool utext_equals_android(const UText * a, const UText * b) {
  return utext_equals(a, b);
}
int32_t utext_extract_android(UText * ut, int64_t nativeStart, int64_t nativeLimit, UChar * dest, int32_t destCapacity, UErrorCode * status) {
  return utext_extract(ut, nativeStart, nativeLimit, dest, destCapacity, status);
}
void utext_freeze_android(UText * ut) {
  utext_freeze(ut);
}
int64_t utext_getNativeIndex_android(const UText * ut) {
  return utext_getNativeIndex(ut);
}
int64_t utext_getPreviousNativeIndex_android(UText * ut) {
  return utext_getPreviousNativeIndex(ut);
}
UBool utext_hasMetaData_android(const UText * ut) {
  return utext_hasMetaData(ut);
}
UBool utext_isLengthExpensive_android(const UText * ut) {
  return utext_isLengthExpensive(ut);
}
UBool utext_isWritable_android(const UText * ut) {
  return utext_isWritable(ut);
}
UBool utext_moveIndex32_android(UText * ut, int32_t delta) {
  return utext_moveIndex32(ut, delta);
}
int64_t utext_nativeLength_android(UText * ut) {
  return utext_nativeLength(ut);
}
UChar32 utext_next32_android(UText * ut) {
  return utext_next32(ut);
}
UChar32 utext_next32From_android(UText * ut, int64_t nativeIndex) {
  return utext_next32From(ut, nativeIndex);
}
UText * utext_openUChars_android(UText * ut, const UChar * s, int64_t length, UErrorCode * status) {
  return utext_openUChars(ut, s, length, status);
}
UText * utext_openUTF8_android(UText * ut, const char * s, int64_t length, UErrorCode * status) {
  return utext_openUTF8(ut, s, length, status);
}
UChar32 utext_previous32_android(UText * ut) {
  return utext_previous32(ut);
}
UChar32 utext_previous32From_android(UText * ut, int64_t nativeIndex) {
  return utext_previous32From(ut, nativeIndex);
}
int32_t utext_replace_android(UText * ut, int64_t nativeStart, int64_t nativeLimit, const UChar * replacementText, int32_t replacementLength, UErrorCode * status) {
  return utext_replace(ut, nativeStart, nativeLimit, replacementText, replacementLength, status);
}
void utext_setNativeIndex_android(UText * ut, int64_t nativeIndex) {
  utext_setNativeIndex(ut, nativeIndex);
}
UText * utext_setup_android(UText * ut, int32_t extraSpace, UErrorCode * status) {
  return utext_setup(ut, extraSpace, status);
}
int32_t utf8_appendCharSafeBody_android(uint8_t * s, int32_t i, int32_t length, UChar32 c, UBool * pIsError) {
  return utf8_appendCharSafeBody(s, i, length, c, pIsError);
}
int32_t utf8_back1SafeBody_android(const uint8_t * s, int32_t start, int32_t i) {
  return utf8_back1SafeBody(s, start, i);
}
UChar32 utf8_nextCharSafeBody_android(const uint8_t * s, int32_t * pi, int32_t length, UChar32 c, UBool strict) {
  return utf8_nextCharSafeBody(s, pi, length, c, strict);
}
UChar32 utf8_prevCharSafeBody_android(const uint8_t * s, int32_t start, int32_t * pi, UChar32 c, UBool strict) {
  return utf8_prevCharSafeBody(s, start, pi, c, strict);
}
int64_t utmscale_fromInt64_android(int64_t otherTime, UDateTimeScale timeScale, UErrorCode * status) {
  return utmscale_fromInt64(otherTime, timeScale, status);
}
int64_t utmscale_getTimeScaleValue_android(UDateTimeScale timeScale, UTimeScaleValue value, UErrorCode * status) {
  return utmscale_getTimeScaleValue(timeScale, value, status);
}
int64_t utmscale_toInt64_android(int64_t universalTime, UDateTimeScale timeScale, UErrorCode * status) {
  return utmscale_toInt64(universalTime, timeScale, status);
}
int32_t utrace_format_android(char * outBuf, int32_t capacity, int32_t indent, const char * fmt, ...) {
  va_list args;
  va_start(args, fmt);
  int32_t ret = utrace_vformat(outBuf, capacity, indent, fmt, args);
  va_end(args);
  return ret;
}
const char * utrace_functionName_android(int32_t fnNumber) {
  return utrace_functionName(fnNumber);
}
void utrace_getFunctions_android(const void ** context, UTraceEntry ** e, UTraceExit ** x, UTraceData ** d) {
  utrace_getFunctions(context, e, x, d);
}
int32_t utrace_getLevel_android() {
  return utrace_getLevel();
}
void utrace_setFunctions_android(const void * context, UTraceEntry * e, UTraceExit * x, UTraceData * d) {
  utrace_setFunctions(context, e, x, d);
}
void utrace_setLevel_android(int32_t traceLevel) {
  utrace_setLevel(traceLevel);
}
int32_t utrace_vformat_android(char * outBuf, int32_t capacity, int32_t indent, const char * fmt, va_list args) {
  return utrace_vformat(outBuf, capacity, indent, fmt, args);
}
UTransliterator * utrans_clone_android(const UTransliterator * trans, UErrorCode * status) {
  return utrans_clone(trans, status);
}
void utrans_close_android(UTransliterator * trans) {
  utrans_close(trans);
}
int32_t utrans_countAvailableIDs_android() {
  return utrans_countAvailableIDs();
}
USet * utrans_getSourceSet_android(const UTransliterator * trans, UBool ignoreFilter, USet * fillIn, UErrorCode * status) {
  return utrans_getSourceSet(trans, ignoreFilter, fillIn, status);
}
const UChar * utrans_getUnicodeID_android(const UTransliterator * trans, int32_t * resultLength) {
  return utrans_getUnicodeID(trans, resultLength);
}
UEnumeration * utrans_openIDs_android(UErrorCode * pErrorCode) {
  return utrans_openIDs(pErrorCode);
}
UTransliterator * utrans_openInverse_android(const UTransliterator * trans, UErrorCode * status) {
  return utrans_openInverse(trans, status);
}
UTransliterator * utrans_openU_android(const UChar * id, int32_t idLength, UTransDirection dir, const UChar * rules, int32_t rulesLength, UParseError * parseError, UErrorCode * pErrorCode) {
  return utrans_openU(id, idLength, dir, rules, rulesLength, parseError, pErrorCode);
}
void utrans_register_android(UTransliterator * adoptedTrans, UErrorCode * status) {
  utrans_register(adoptedTrans, status);
}
void utrans_setFilter_android(UTransliterator * trans, const UChar * filterPattern, int32_t filterPatternLen, UErrorCode * status) {
  utrans_setFilter(trans, filterPattern, filterPatternLen, status);
}
int32_t utrans_toRules_android(const UTransliterator * trans, UBool escapeUnprintable, UChar * result, int32_t resultLength, UErrorCode * status) {
  return utrans_toRules(trans, escapeUnprintable, result, resultLength, status);
}
void utrans_trans_android(const UTransliterator * trans, UReplaceable * rep, const UReplaceableCallbacks * repFunc, int32_t start, int32_t * limit, UErrorCode * status) {
  utrans_trans(trans, rep, repFunc, start, limit, status);
}
void utrans_transIncremental_android(const UTransliterator * trans, UReplaceable * rep, const UReplaceableCallbacks * repFunc, UTransPosition * pos, UErrorCode * status) {
  utrans_transIncremental(trans, rep, repFunc, pos, status);
}
void utrans_transIncrementalUChars_android(const UTransliterator * trans, UChar * text, int32_t * textLength, int32_t textCapacity, UTransPosition * pos, UErrorCode * status) {
  utrans_transIncrementalUChars(trans, text, textLength, textCapacity, pos, status);
}
void utrans_transUChars_android(const UTransliterator * trans, UChar * text, int32_t * textLength, int32_t textCapacity, int32_t start, int32_t * limit, UErrorCode * status) {
  utrans_transUChars(trans, text, textLength, textCapacity, start, limit, status);
}
void utrans_unregisterID_android(const UChar * id, int32_t idLength) {
  utrans_unregisterID(id, idLength);
}
}