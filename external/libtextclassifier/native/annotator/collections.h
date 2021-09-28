/*
 * Copyright (C) 2018 The Android Open Source Project
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

#ifndef LIBTEXTCLASSIFIER_ANNOTATOR_COLLECTIONS_H_
#define LIBTEXTCLASSIFIER_ANNOTATOR_COLLECTIONS_H_

#include <string>

namespace libtextclassifier3 {

// String collection names for various classes.
class Collections {
 public:
  static const std::string& Address() {
    static const std::string& value =
        *[]() { return new std::string("address"); }();
    return value;
  }
  static const std::string& App() {
    static const std::string& value =
        *[]() { return new std::string("app"); }();
    return value;
  }
  static const std::string& Contact() {
    static const std::string& value =
        *[]() { return new std::string("contact"); }();
    return value;
  }
  static const std::string& Date() {
    static const std::string& value =
        *[]() { return new std::string("date"); }();
    return value;
  }
  static const std::string& DateTime() {
    static const std::string& value =
        *[]() { return new std::string("datetime"); }();
    return value;
  }
  static const std::string& Dictionary() {
    static const std::string& value =
        *[]() { return new std::string("dictionary"); }();
    return value;
  }
  static const std::string& Duration() {
    static const std::string& value =
        *[]() { return new std::string("duration"); }();
    return value;
  }
  static const std::string& Email() {
    static const std::string& value =
        *[]() { return new std::string("email"); }();
    return value;
  }
  static const std::string& Entity() {
    static const std::string& value =
        *[]() { return new std::string("entity"); }();
    return value;
  }
  static const std::string& Flight() {
    static const std::string& value =
        *[]() { return new std::string("flight"); }();
    return value;
  }
  static const std::string& Iban() {
    static const std::string& value =
        *[]() { return new std::string("iban"); }();
    return value;
  }
  static const std::string& Isbn() {
    static const std::string& value =
        *[]() { return new std::string("isbn"); }();
    return value;
  }
  static const std::string& Money() {
    static const std::string& value =
        *[]() { return new std::string("money"); }();
    return value;
  }
  static const std::string& Unit() {
    static const std::string& value =
        *[]() { return new std::string("unit"); }();
    return value;
  }
  static const std::string& Number() {
    static const std::string& value =
        *[]() { return new std::string("number"); }();
    return value;
  }
  static const std::string& Other() {
    static const std::string& value =
        *[]() { return new std::string("other"); }();
    return value;
  }
  static const std::string& PaymentCard() {
    static const std::string& value =
        *[]() { return new std::string("payment_card"); }();
    return value;
  }
  static const std::string& Percentage() {
    static const std::string& value =
        *[]() { return new std::string("percentage"); }();
    return value;
  }
  static const std::string& PersonName() {
    static const std::string& value =
        *[]() { return new std::string("person_name"); }();
    return value;
  }
  static const std::string& Phone() {
    static const std::string& value =
        *[]() { return new std::string("phone"); }();
    return value;
  }
  static const std::string& TrackingNumber() {
    static const std::string& value =
        *[]() { return new std::string("tracking_number"); }();
    return value;
  }
  static const std::string& Translate() {
    static const std::string& value =
        *[]() { return new std::string("translate"); }();
    return value;
  }
  static const std::string& Url() {
    static const std::string& value =
        *[]() { return new std::string("url"); }();
    return value;
  }
  static const std::string& OtpCode() {
    static const std::string& value =
        *[]() { return new std::string("otp_code"); }();
    return value;
  }
};

}  // namespace libtextclassifier3

#endif  // LIBTEXTCLASSIFIER_ANNOTATOR_COLLECTIONS_H_
