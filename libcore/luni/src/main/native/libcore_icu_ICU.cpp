/*
 * Copyright (C) 2008 The Android Open Source Project
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

#define LOG_NDEBUG 1
#define LOG_TAG "ICU"

#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <sys/mman.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

#include <memory>
#include <vector>

#include <androidicuinit/IcuRegistration.h>
#include <log/log.h>
#include <nativehelper/JNIHelp.h>
#include <nativehelper/ScopedLocalRef.h>
#include <nativehelper/ScopedStringChars.h>
#include <nativehelper/ScopedUtfChars.h>
#include <nativehelper/jni_macros.h>
#include <nativehelper/toStringArray.h>

#include "IcuUtilities.h"
#include "JniConstants.h"
#include "JniException.h"
#include "ScopedIcuLocale.h"
#include "ScopedIcuULoc.h"
#include "ScopedJavaUnicodeString.h"
#include "unicode/brkiter.h"
#include "unicode/char16ptr.h"
#include "unicode/calendar.h"
#include "unicode/datefmt.h"
#include "unicode/dcfmtsym.h"
#include "unicode/decimfmt.h"
#include "unicode/dtfmtsym.h"
#include "unicode/gregocal.h"
#include "unicode/locid.h"
#include "unicode/numfmt.h"
#include "unicode/strenum.h"
#include "unicode/ubrk.h"
#include "unicode/ucal.h"
#include "unicode/ucasemap.h"
#include "unicode/ucol.h"
#include "unicode/ucurr.h"
#include "unicode/udat.h"
#include "unicode/udatpg.h"
#include "unicode/uloc.h"
#include "unicode/ures.h"
#include "unicode/ustring.h"
#include "ureslocs.h"
#include "valueOf.h"

class ScopedResourceBundle {
 public:
  explicit ScopedResourceBundle(UResourceBundle* bundle) : bundle_(bundle) {
  }

  ~ScopedResourceBundle() {
    if (bundle_ != NULL) {
      ures_close(bundle_);
    }
  }

  UResourceBundle* get() {
    return bundle_;
  }

  bool hasKey(const char* key) {
    UErrorCode status = U_ZERO_ERROR;
    ures_getStringByKey(bundle_, key, NULL, &status);
    return U_SUCCESS(status);
  }

 private:
  UResourceBundle* bundle_;
  DISALLOW_COPY_AND_ASSIGN(ScopedResourceBundle);
};

static jstring ICU_getScript(JNIEnv* env, jclass, jstring javaLocaleName) {
  ScopedIcuULoc icuLocale(env, javaLocaleName);
  if (!icuLocale.valid()) {
    return NULL;
  }
  // Normal script part is 4-char long. Being conservative for allocation size
  // because if the locale contains script part, it should not be longer than the locale itself.
  int32_t capacity = std::max(ULOC_SCRIPT_CAPACITY, icuLocale.locale_length() + 1);
  std::unique_ptr<char[]> buffer(new char(capacity));
  UErrorCode status = U_ZERO_ERROR;
  uloc_getScript(icuLocale.locale(), buffer.get(), capacity, &status);
  if (U_FAILURE(status)) {
    return NULL;
  }
  return env->NewStringUTF(buffer.get());
}

// TODO: rewrite this with int32_t ucurr_forLocale(const char* locale, UChar* buff, int32_t buffCapacity, UErrorCode* ec)...
static jstring ICU_getCurrencyCode(JNIEnv* env, jclass, jstring javaCountryCode) {
    UErrorCode status = U_ZERO_ERROR;
    ScopedResourceBundle supplData(ures_openDirect(U_ICUDATA_CURR, "supplementalData", &status));
    if (U_FAILURE(status)) {
        return NULL;
    }

    ScopedResourceBundle currencyMap(ures_getByKey(supplData.get(), "CurrencyMap", NULL, &status));
    if (U_FAILURE(status)) {
        return NULL;
    }

    ScopedUtfChars countryCode(env, javaCountryCode);
    ScopedResourceBundle currency(ures_getByKey(currencyMap.get(), countryCode.c_str(), NULL, &status));
    if (U_FAILURE(status)) {
        return NULL;
    }

    ScopedResourceBundle currencyElem(ures_getByIndex(currency.get(), 0, NULL, &status));
    if (U_FAILURE(status)) {
        return env->NewStringUTF("XXX");
    }

    // Check if there's a 'to' date. If there is, the currency isn't used anymore.
    ScopedResourceBundle currencyTo(ures_getByKey(currencyElem.get(), "to", NULL, &status));
    if (!U_FAILURE(status)) {
        return NULL;
    }
    // Ignore the failure to find a 'to' date.
    status = U_ZERO_ERROR;

    ScopedResourceBundle currencyId(ures_getByKey(currencyElem.get(), "id", NULL, &status));
    if (U_FAILURE(status)) {
        // No id defined for this country
        return env->NewStringUTF("XXX");
    }

    int32_t charCount;
    const UChar* chars = ures_getString(currencyId.get(), &charCount, &status);
    return (charCount == 0) ? env->NewStringUTF("XXX") : jniCreateString(env, chars, charCount);
}

static jstring getCurrencyName(JNIEnv* env, jstring javaLanguageTag, jstring javaCurrencyCode, UCurrNameStyle nameStyle) {
  ScopedUtfChars languageTag(env, javaLanguageTag);
  if (languageTag.c_str() == NULL) {
    return NULL;
  }
  ScopedJavaUnicodeString currencyCode(env, javaCurrencyCode);
  if (!currencyCode.valid()) {
    return NULL;
  }
  icu::UnicodeString icuCurrencyCode(currencyCode.unicodeString());
  UErrorCode status = U_ZERO_ERROR;
  UBool isChoiceFormat = false;
  int32_t charCount;
  const UChar* chars = ucurr_getName(icuCurrencyCode.getTerminatedBuffer(), languageTag.c_str(),
                                     nameStyle, &isChoiceFormat, &charCount, &status);
  if (status == U_USING_DEFAULT_WARNING) {
    if (nameStyle == UCURR_SYMBOL_NAME) {
      // ICU doesn't distinguish between falling back to the root locale and meeting a genuinely
      // unknown currency. The Currency class does.
      if (!ucurr_isAvailable(icuCurrencyCode.getTerminatedBuffer(), U_DATE_MIN, U_DATE_MAX, &status)) {
        return NULL;
      }
    }
    if (nameStyle == UCURR_LONG_NAME) {
      // ICU's default is English. We want the ISO 4217 currency code instead.
      chars = icuCurrencyCode.getBuffer();
      charCount = icuCurrencyCode.length();
    }
  }
  return (charCount == 0) ? NULL : jniCreateString(env, chars, charCount);
}

static jstring ICU_getISO3Country(JNIEnv* env, jclass, jstring javaLanguageTag) {
  ScopedIcuULoc icuLocale(env, javaLanguageTag);
  if (!icuLocale.valid()) {
    return NULL;
  }
  return env->NewStringUTF(uloc_getISO3Country(icuLocale.locale()));
}

static jstring ICU_getISO3Language(JNIEnv* env, jclass, jstring javaLanguageTag) {
  ScopedIcuULoc icuLocale(env, javaLanguageTag);
  if (!icuLocale.valid()) {
    return NULL;
  }
  return env->NewStringUTF(uloc_getISO3Language(icuLocale.locale()));
}

static jobjectArray ICU_getISOCountriesNative(JNIEnv* env, jclass) {
    return toStringArray(env, uloc_getISOCountries());
}

static jobjectArray ICU_getISOLanguagesNative(JNIEnv* env, jclass) {
    return toStringArray(env, uloc_getISOLanguages());
}

static jobjectArray ICU_getAvailableLocalesNative(JNIEnv* env, jclass) {
    return toStringArray(env, uloc_countAvailable, uloc_getAvailable);
}

static bool setIntegerField(JNIEnv* env, jobject obj, const char* fieldName, int value) {
    ScopedLocalRef<jobject> integerValue(env, integerValueOf(env, value));
    if (integerValue.get() == NULL) return false;
    jfieldID fid = env->GetFieldID(JniConstants::GetLocaleDataClass(env), fieldName, "Ljava/lang/Integer;");
    env->SetObjectField(obj, fid, integerValue.get());
    return true;
}

static void setStringField(JNIEnv* env, jobject obj, const char* fieldName, jstring value) {
    jfieldID fid = env->GetFieldID(JniConstants::GetLocaleDataClass(env), fieldName, "Ljava/lang/String;");
    env->SetObjectField(obj, fid, value);
    env->DeleteLocalRef(value);
}

static void setStringArrayField(JNIEnv* env, jobject obj, const char* fieldName, jobjectArray value) {
    jfieldID fid = env->GetFieldID(JniConstants::GetLocaleDataClass(env), fieldName, "[Ljava/lang/String;");
    env->SetObjectField(obj, fid, value);
}

static void setStringArrayField(JNIEnv* env, jobject obj, const char* fieldName, const icu::UnicodeString* valueArray, int32_t size) {
    ScopedLocalRef<jobjectArray> result(env, env->NewObjectArray(size, JniConstants::GetStringClass(env), NULL));
    for (int32_t i = 0; i < size ; i++) {
        ScopedLocalRef<jstring> s(env, jniCreateString(env, valueArray[i].getBuffer(),valueArray[i].length()));
        if (env->ExceptionCheck()) {
            return;
        }
        env->SetObjectArrayElement(result.get(), i, s.get());
        if (env->ExceptionCheck()) {
            return;
        }
    }
    setStringArrayField(env, obj, fieldName, result.get());
}

static void setStringField(JNIEnv* env, jobject obj, const char* fieldName, UResourceBundle* bundle, int index) {
  UErrorCode status = U_ZERO_ERROR;
  int charCount;
  const UChar* chars;
  UResourceBundle* currentBundle = ures_getByIndex(bundle, index, NULL, &status);
  switch (ures_getType(currentBundle)) {
      case URES_STRING:
         chars = ures_getString(currentBundle, &charCount, &status);
         break;
      case URES_ARRAY:
         // In case there is an array, Android currently only cares about the
         // first string of that array, the rest of the array is used by ICU
         // for additional data ignored by Android.
         chars = ures_getStringByIndex(currentBundle, 0, &charCount, &status);
         break;
      default:
         status = U_INVALID_FORMAT_ERROR;
  }
  ures_close(currentBundle);
  if (U_SUCCESS(status)) {
    setStringField(env, obj, fieldName, jniCreateString(env, chars, charCount));
  } else {
    ALOGE("Error setting String field %s from ICU resource (index %d): %s", fieldName, index, u_errorName(status));
  }
}

static void setCharField(JNIEnv* env, jobject obj, const char* fieldName, const icu::UnicodeString& value) {
    if (value.length() == 0) {
        return;
    }
    jfieldID fid = env->GetFieldID(JniConstants::GetLocaleDataClass(env), fieldName, "C");
    env->SetCharField(obj, fid, value.charAt(0));
}

static void setStringField(JNIEnv* env, jobject obj, const char* fieldName, const icu::UnicodeString& value) {
    const UChar* chars = value.getBuffer();
    setStringField(env, obj, fieldName, jniCreateString(env, chars, value.length()));
}

static void setNumberPatterns(JNIEnv* env, jobject obj, icu::Locale& locale) {
    UErrorCode status = U_ZERO_ERROR;

    icu::UnicodeString pattern;
    std::unique_ptr<icu::DecimalFormat> fmt(static_cast<icu::DecimalFormat*>(icu::NumberFormat::createInstance(locale, UNUM_CURRENCY, status)));
    pattern = fmt->toPattern(pattern.remove());
    setStringField(env, obj, "currencyPattern", pattern);

    fmt.reset(static_cast<icu::DecimalFormat*>(icu::NumberFormat::createInstance(locale, UNUM_DECIMAL, status)));
    pattern = fmt->toPattern(pattern.remove());
    setStringField(env, obj, "numberPattern", pattern);

    fmt.reset(static_cast<icu::DecimalFormat*>(icu::NumberFormat::createInstance(locale, UNUM_PERCENT, status)));
    pattern = fmt->toPattern(pattern.remove());
    setStringField(env, obj, "percentPattern", pattern);
}

static void setDecimalFormatSymbolsData(JNIEnv* env, jobject obj, icu::Locale& locale) {
    UErrorCode status = U_ZERO_ERROR;
    icu::DecimalFormatSymbols dfs(locale, status);

    setCharField(env, obj, "decimalSeparator", dfs.getSymbol(icu::DecimalFormatSymbols::kDecimalSeparatorSymbol));
    setCharField(env, obj, "groupingSeparator", dfs.getSymbol(icu::DecimalFormatSymbols::kGroupingSeparatorSymbol));
    setCharField(env, obj, "patternSeparator", dfs.getSymbol(icu::DecimalFormatSymbols::kPatternSeparatorSymbol));
    setStringField(env, obj, "percent", dfs.getSymbol(icu::DecimalFormatSymbols::kPercentSymbol));
    setStringField(env, obj, "perMill", dfs.getSymbol(icu::DecimalFormatSymbols::kPerMillSymbol));
    setCharField(env, obj, "monetarySeparator", dfs.getSymbol(icu::DecimalFormatSymbols::kMonetarySeparatorSymbol));
    setStringField(env, obj, "minusSign", dfs.getSymbol(icu::DecimalFormatSymbols:: kMinusSignSymbol));
    setStringField(env, obj, "exponentSeparator", dfs.getSymbol(icu::DecimalFormatSymbols::kExponentialSymbol));
    setStringField(env, obj, "infinity", dfs.getSymbol(icu::DecimalFormatSymbols::kInfinitySymbol));
    setStringField(env, obj, "NaN", dfs.getSymbol(icu::DecimalFormatSymbols::kNaNSymbol));
    setCharField(env, obj, "zeroDigit", dfs.getSymbol(icu::DecimalFormatSymbols::kZeroDigitSymbol));
}


// Iterates up through the locale hierarchy. So "en_US" would return "en_US", "en", "".
class LocaleNameIterator {
 public:
  LocaleNameIterator(const char* locale_name, UErrorCode& status) : status_(status), has_next_(true) {
    strcpy(locale_name_, locale_name);
    locale_name_length_ = strlen(locale_name_);
  }

  const char* Get() {
      return locale_name_;
  }

  bool HasNext() {
    return has_next_;
  }

  void Up() {
    if (locale_name_length_ == 0) {
      has_next_ = false;
    } else {
      locale_name_length_ = uloc_getParent(locale_name_, locale_name_, sizeof(locale_name_), &status_);
    }
  }

 private:
  UErrorCode& status_;
  bool has_next_;
  char locale_name_[ULOC_FULLNAME_CAPACITY];
  int32_t locale_name_length_;

  DISALLOW_COPY_AND_ASSIGN(LocaleNameIterator);
};

static bool getAmPmMarkersNarrow(JNIEnv* env, jobject localeData, const char* locale_name) {
  UErrorCode status = U_ZERO_ERROR;
  ScopedResourceBundle root(ures_open(NULL, locale_name, &status));
  if (U_FAILURE(status)) {
    return false;
  }
  ScopedResourceBundle calendar(ures_getByKey(root.get(), "calendar", NULL, &status));
  if (U_FAILURE(status)) {
    return false;
  }
  ScopedResourceBundle gregorian(ures_getByKey(calendar.get(), "gregorian", NULL, &status));
  if (U_FAILURE(status)) {
    return false;
  }
  ScopedResourceBundle amPmMarkersNarrow(ures_getByKey(gregorian.get(), "AmPmMarkersNarrow", NULL, &status));
  if (U_FAILURE(status)) {
    return false;
  }
  setStringField(env, localeData, "narrowAm", amPmMarkersNarrow.get(), 0);
  setStringField(env, localeData, "narrowPm", amPmMarkersNarrow.get(), 1);
  return true;
}

static bool getDateTimePatterns(JNIEnv* env, jobject localeData, const char* locale_name) {
  UErrorCode status = U_ZERO_ERROR;
  ScopedResourceBundle root(ures_open(NULL, locale_name, &status));
  if (U_FAILURE(status)) {
    return false;
  }
  ScopedResourceBundle calendar(ures_getByKey(root.get(), "calendar", NULL, &status));
  if (U_FAILURE(status)) {
    return false;
  }
  ScopedResourceBundle gregorian(ures_getByKey(calendar.get(), "gregorian", NULL, &status));
  if (U_FAILURE(status)) {
    return false;
  }
  ScopedResourceBundle dateTimePatterns(ures_getByKey(gregorian.get(), "DateTimePatterns", NULL, &status));
  if (U_FAILURE(status)) {
    return false;
  }
  setStringField(env, localeData, "fullTimeFormat", dateTimePatterns.get(), 0);
  setStringField(env, localeData, "longTimeFormat", dateTimePatterns.get(), 1);
  setStringField(env, localeData, "mediumTimeFormat", dateTimePatterns.get(), 2);
  setStringField(env, localeData, "shortTimeFormat", dateTimePatterns.get(), 3);
  setStringField(env, localeData, "fullDateFormat", dateTimePatterns.get(), 4);
  setStringField(env, localeData, "longDateFormat", dateTimePatterns.get(), 5);
  setStringField(env, localeData, "mediumDateFormat", dateTimePatterns.get(), 6);
  setStringField(env, localeData, "shortDateFormat", dateTimePatterns.get(), 7);
  return true;
}

static bool getYesterdayTodayAndTomorrow(JNIEnv* env, jobject localeData, const icu::Locale& locale, const char* locale_name) {
  UErrorCode status = U_ZERO_ERROR;
  ScopedResourceBundle root(ures_open(NULL, locale_name, &status));
  ScopedResourceBundle fields(ures_getByKey(root.get(), "fields", NULL, &status));
  ScopedResourceBundle day(ures_getByKey(fields.get(), "day", NULL, &status));
  ScopedResourceBundle relative(ures_getByKey(day.get(), "relative", NULL, &status));
  if (U_FAILURE(status)) {
    return false;
  }

  icu::UnicodeString yesterday(icu::ures_getUnicodeStringByKey(relative.get(), "-1", &status));
  icu::UnicodeString today(icu::ures_getUnicodeStringByKey(relative.get(), "0", &status));
  icu::UnicodeString tomorrow(icu::ures_getUnicodeStringByKey(relative.get(), "1", &status));
  if (U_FAILURE(status)) {
    ALOGE("Error getting yesterday/today/tomorrow for %s: %s", locale_name, u_errorName(status));
    return false;
  }

  // We title-case the strings so they have consistent capitalization (http://b/14493853).
  std::unique_ptr<icu::BreakIterator> brk(icu::BreakIterator::createSentenceInstance(locale, status));
  if (U_FAILURE(status)) {
    ALOGE("Error getting yesterday/today/tomorrow break iterator for %s: %s", locale_name, u_errorName(status));
    return false;
  }
  yesterday.toTitle(brk.get(), locale, U_TITLECASE_NO_LOWERCASE | U_TITLECASE_NO_BREAK_ADJUSTMENT);
  today.toTitle(brk.get(), locale, U_TITLECASE_NO_LOWERCASE | U_TITLECASE_NO_BREAK_ADJUSTMENT);
  tomorrow.toTitle(brk.get(), locale, U_TITLECASE_NO_LOWERCASE | U_TITLECASE_NO_BREAK_ADJUSTMENT);

  setStringField(env, localeData, "yesterday", yesterday);
  setStringField(env, localeData, "today", today);
  setStringField(env, localeData, "tomorrow", tomorrow);
  return true;
}

static jboolean ICU_initLocaleDataNative(JNIEnv* env, jclass, jstring javaLanguageTag, jobject localeData) {
    ScopedUtfChars languageTag(env, javaLanguageTag);
    if (languageTag.c_str() == NULL) {
        return JNI_FALSE;
    }
    if (languageTag.size() >= ULOC_FULLNAME_CAPACITY) {
        return JNI_FALSE; // ICU has a fixed-length limit.
    }

    ScopedIcuLocale icuLocale(env, javaLanguageTag);
    if (!icuLocale.valid()) {
      return JNI_FALSE;
    }

    // Get the DateTimePatterns.
    UErrorCode status = U_ZERO_ERROR;
    bool foundDateTimePatterns = false;
    for (LocaleNameIterator it(icuLocale.locale().getBaseName(), status); it.HasNext(); it.Up()) {
      if (getDateTimePatterns(env, localeData, it.Get())) {
          foundDateTimePatterns = true;
          break;
      }
    }
    if (!foundDateTimePatterns) {
        ALOGE("Couldn't find ICU DateTimePatterns for %s", languageTag.c_str());
        return JNI_FALSE;
    }

    // Get the "Yesterday", "Today", and "Tomorrow" strings.
    bool foundYesterdayTodayAndTomorrow = false;
    for (LocaleNameIterator it(icuLocale.locale().getBaseName(), status); it.HasNext(); it.Up()) {
      if (getYesterdayTodayAndTomorrow(env, localeData, icuLocale.locale(), it.Get())) {
        foundYesterdayTodayAndTomorrow = true;
        break;
      }
    }
    if (!foundYesterdayTodayAndTomorrow) {
      ALOGE("Couldn't find ICU yesterday/today/tomorrow for %s", languageTag.c_str());
      return JNI_FALSE;
    }

    // Get the narrow "AM" and "PM" strings.
    bool foundAmPmMarkersNarrow = false;
    for (LocaleNameIterator it(icuLocale.locale().getBaseName(), status); it.HasNext(); it.Up()) {
      if (getAmPmMarkersNarrow(env, localeData, it.Get())) {
        foundAmPmMarkersNarrow = true;
        break;
      }
    }
    if (!foundAmPmMarkersNarrow) {
      ALOGE("Couldn't find ICU AmPmMarkersNarrow for %s", languageTag.c_str());
      return JNI_FALSE;
    }

    status = U_ZERO_ERROR;
    std::unique_ptr<icu::Calendar> cal(icu::Calendar::createInstance(icuLocale.locale(), status));
    if (U_FAILURE(status)) {
        return JNI_FALSE;
    }
    if (!setIntegerField(env, localeData, "firstDayOfWeek", cal->getFirstDayOfWeek())) {
      return JNI_FALSE;
    }
    if (!setIntegerField(env, localeData, "minimalDaysInFirstWeek", cal->getMinimalDaysInFirstWeek())) {
      return JNI_FALSE;
    }

    // Get DateFormatSymbols.
    status = U_ZERO_ERROR;
    icu::DateFormatSymbols dateFormatSym(icuLocale.locale(), status);
    if (U_FAILURE(status)) {
        return JNI_FALSE;
    }

    // Get AM/PM and BC/AD.
    int32_t count = 0;
    const icu::UnicodeString* amPmStrs = dateFormatSym.getAmPmStrings(count);
    setStringArrayField(env, localeData, "amPm", amPmStrs, count);
    const icu::UnicodeString* erasStrs = dateFormatSym.getEras(count);
    setStringArrayField(env, localeData, "eras", erasStrs, count);

    const icu::UnicodeString* longMonthNames =
       dateFormatSym.getMonths(count, icu::DateFormatSymbols::FORMAT, icu::DateFormatSymbols::WIDE);
    setStringArrayField(env, localeData, "longMonthNames", longMonthNames, count);
    const icu::UnicodeString* shortMonthNames =
        dateFormatSym.getMonths(count, icu::DateFormatSymbols::FORMAT, icu::DateFormatSymbols::ABBREVIATED);
    setStringArrayField(env, localeData, "shortMonthNames", shortMonthNames, count);
    const icu::UnicodeString* tinyMonthNames =
        dateFormatSym.getMonths(count, icu::DateFormatSymbols::FORMAT, icu::DateFormatSymbols::NARROW);
    setStringArrayField(env, localeData, "tinyMonthNames", tinyMonthNames, count);
    const icu::UnicodeString* longWeekdayNames =
        dateFormatSym.getWeekdays(count, icu::DateFormatSymbols::FORMAT, icu::DateFormatSymbols::WIDE);
    setStringArrayField(env, localeData, "longWeekdayNames", longWeekdayNames, count);
    const icu::UnicodeString* shortWeekdayNames =
        dateFormatSym.getWeekdays(count, icu::DateFormatSymbols::FORMAT, icu::DateFormatSymbols::ABBREVIATED);
    setStringArrayField(env, localeData, "shortWeekdayNames", shortWeekdayNames, count);
    const icu::UnicodeString* tinyWeekdayNames =
        dateFormatSym.getWeekdays(count, icu::DateFormatSymbols::FORMAT, icu::DateFormatSymbols::NARROW);
    setStringArrayField(env, localeData, "tinyWeekdayNames", tinyWeekdayNames, count);

    const icu::UnicodeString* longStandAloneMonthNames =
        dateFormatSym.getMonths(count, icu::DateFormatSymbols::STANDALONE, icu::DateFormatSymbols::WIDE);
    setStringArrayField(env, localeData, "longStandAloneMonthNames", longStandAloneMonthNames, count);
    const icu::UnicodeString* shortStandAloneMonthNames =
        dateFormatSym.getMonths(count, icu::DateFormatSymbols::STANDALONE, icu::DateFormatSymbols::ABBREVIATED);
    setStringArrayField(env, localeData, "shortStandAloneMonthNames", shortStandAloneMonthNames, count);
    const icu::UnicodeString* tinyStandAloneMonthNames =
        dateFormatSym.getMonths(count, icu::DateFormatSymbols::STANDALONE, icu::DateFormatSymbols::NARROW);
    setStringArrayField(env, localeData, "tinyStandAloneMonthNames", tinyStandAloneMonthNames, count);
    const icu::UnicodeString* longStandAloneWeekdayNames =
        dateFormatSym.getWeekdays(count, icu::DateFormatSymbols::STANDALONE, icu::DateFormatSymbols::WIDE);
    setStringArrayField(env, localeData, "longStandAloneWeekdayNames", longStandAloneWeekdayNames, count);
    const icu::UnicodeString* shortStandAloneWeekdayNames =
        dateFormatSym.getWeekdays(count, icu::DateFormatSymbols::STANDALONE, icu::DateFormatSymbols::ABBREVIATED);
    setStringArrayField(env, localeData, "shortStandAloneWeekdayNames", shortStandAloneWeekdayNames, count);
    const icu::UnicodeString* tinyStandAloneWeekdayNames =
        dateFormatSym.getWeekdays(count, icu::DateFormatSymbols::STANDALONE, icu::DateFormatSymbols::NARROW);
    setStringArrayField(env, localeData, "tinyStandAloneWeekdayNames", tinyStandAloneWeekdayNames, count);

    status = U_ZERO_ERROR;

    // For numberPatterns and symbols.
    setNumberPatterns(env, localeData, icuLocale.locale());
    setDecimalFormatSymbolsData(env, localeData, icuLocale.locale());

    jstring countryCode = env->NewStringUTF(icuLocale.locale().getCountry());
    jstring internationalCurrencySymbol = ICU_getCurrencyCode(env, NULL, countryCode);
    env->DeleteLocalRef(countryCode);
    countryCode = NULL;

    jstring currencySymbol = NULL;
    if (internationalCurrencySymbol != NULL) {
        currencySymbol = getCurrencyName(env, javaLanguageTag, internationalCurrencySymbol, UCURR_SYMBOL_NAME);
    } else {
        internationalCurrencySymbol = env->NewStringUTF("XXX");
    }
    if (currencySymbol == NULL) {
        // This is the UTF-8 encoding of U+00A4 (CURRENCY SIGN).
        currencySymbol = env->NewStringUTF("\xc2\xa4");
    }
    setStringField(env, localeData, "currencySymbol", currencySymbol);
    setStringField(env, localeData, "internationalCurrencySymbol", internationalCurrencySymbol);

    return JNI_TRUE;
}

static jstring ICU_getBestDateTimePatternNative(JNIEnv* env, jclass, jstring javaSkeleton, jstring javaLanguageTag) {
  ScopedIcuULoc icuLocale(env, javaLanguageTag);
  if (!icuLocale.valid()) {
    return NULL;
  }

  UErrorCode status = U_ZERO_ERROR;
  std::unique_ptr<UDateTimePatternGenerator, decltype(&udatpg_close)> generator(
    udatpg_open(icuLocale.locale(), &status), &udatpg_close);
  if (maybeThrowIcuException(env, "udatpg_open", status)) {
    return NULL;
  }

  const ScopedStringChars skeletonHolder(env, javaSkeleton);
  // Convert jchar* to UChar* with the inline-able utility provided by char16ptr.h
  // which prevents certain compiler optimization than reinterpret_cast.
  icu::ConstChar16Ptr skeletonPtr(skeletonHolder.get());
  const UChar* skeleton = icu::toUCharPtr(skeletonPtr.get());

  int32_t patternLength;
  // Try with fixed-size buffer. 128 chars should be enough for most patterns.
  // If the buffer is not sufficient, run the below case of U_BUFFER_OVERFLOW_ERROR.
  #define PATTERN_BUFFER_SIZE 128
  {
    UChar buffer[PATTERN_BUFFER_SIZE];
    status = U_ZERO_ERROR;
    patternLength = udatpg_getBestPattern(generator.get(), skeleton,
      skeletonHolder.size(), buffer, PATTERN_BUFFER_SIZE, &status);
    if (U_SUCCESS(status)) {
      return jniCreateString(env, buffer, patternLength);
    } else if (status != U_BUFFER_OVERFLOW_ERROR) {
      maybeThrowIcuException(env, "udatpg_getBestPattern", status);
      return NULL;
    }
  }
  #undef PATTERN_BUFFER_SIZE

  // Case U_BUFFER_OVERFLOW_ERROR
  std::unique_ptr<UChar[]> buffer(new UChar[patternLength+1]);
  status = U_ZERO_ERROR;
  patternLength = udatpg_getBestPattern(generator.get(), skeleton,
      skeletonHolder.size(), buffer.get(), patternLength+1, &status);
  if (maybeThrowIcuException(env, "udatpg_getBestPattern", status)) {
    return NULL;
  }

  return jniCreateString(env, buffer.get(), patternLength);
}

static void ICU_setDefaultLocale(JNIEnv* env, jclass, jstring javaLanguageTag) {
  ScopedIcuULoc icuLocale(env, javaLanguageTag);
  if (!icuLocale.valid()) {
    return;
  }

  UErrorCode status = U_ZERO_ERROR;
  uloc_setDefault(icuLocale.locale(), &status);
  maybeThrowIcuException(env, "uloc_setDefault", status);
}

static jstring ICU_getDefaultLocale(JNIEnv* env, jclass) {
  return env->NewStringUTF(uloc_getDefault());
}

static JNINativeMethod gMethods[] = {
    NATIVE_METHOD(ICU, getAvailableLocalesNative, "()[Ljava/lang/String;"),
    NATIVE_METHOD(ICU, getBestDateTimePatternNative, "(Ljava/lang/String;Ljava/lang/String;)Ljava/lang/String;"),
    NATIVE_METHOD(ICU, getCurrencyCode, "(Ljava/lang/String;)Ljava/lang/String;"),
    NATIVE_METHOD(ICU, getDefaultLocale, "()Ljava/lang/String;"),
    NATIVE_METHOD(ICU, getISO3Country, "(Ljava/lang/String;)Ljava/lang/String;"),
    NATIVE_METHOD(ICU, getISO3Language, "(Ljava/lang/String;)Ljava/lang/String;"),
    NATIVE_METHOD(ICU, getISOCountriesNative, "()[Ljava/lang/String;"),
    NATIVE_METHOD(ICU, getISOLanguagesNative, "()[Ljava/lang/String;"),
    NATIVE_METHOD(ICU, getScript, "(Ljava/lang/String;)Ljava/lang/String;"),
    NATIVE_METHOD(ICU, initLocaleDataNative, "(Ljava/lang/String;Llibcore/icu/LocaleData;)Z"),
    NATIVE_METHOD(ICU, setDefaultLocale, "(Ljava/lang/String;)V"),
};

//
// Global initialization & Teardown for ICU Setup
//   - Contains handlers for JNI_OnLoad and JNI_OnUnload
//

// Init ICU, configuring it and loading the data files.
void register_libcore_icu_ICU(JNIEnv* env) {
  androidicuinit::IcuRegistration::Register();

  jniRegisterNativeMethods(env, "libcore/icu/ICU", gMethods, NELEM(gMethods));
}

// De-init ICU, unloading the data files. Do the opposite of the above function.
void unregister_libcore_icu_ICU() {
  // Skip unregistering JNI methods explicitly, class unloading takes care of
  // it.

  androidicuinit::IcuRegistration::Deregister();
}
