/*
 * Copyright (C) 2016, The Android Open Source Project
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

#include "tests/test_data.h"

namespace android {
namespace aidl {
namespace test_data {
namespace string_constants {

const char kCanonicalName[] = "android.os.IStringConstants";
const char kInterfaceDefinition[] = R"(
package android.os;

interface IStringConstants {
  const String EXAMPLE_CONSTANT = "foo";
}
)";

const char kJavaOutputPath[] = "some/path/to/output.java";
const char kExpectedJavaOutput[] =
    R"(/*
 * This file is auto-generated.  DO NOT MODIFY.
 */
package android.os;
public interface IStringConstants extends android.os.IInterface
{
  /** Default implementation for IStringConstants. */
  public static class Default implements android.os.IStringConstants
  {
    @Override
    public android.os.IBinder asBinder() {
      return null;
    }
  }
  /** Local-side IPC implementation stub class. */
  public static abstract class Stub extends android.os.Binder implements android.os.IStringConstants
  {
    private static final java.lang.String DESCRIPTOR = "android.os.IStringConstants";
    /** Construct the stub at attach it to the interface. */
    public Stub()
    {
      this.attachInterface(this, DESCRIPTOR);
    }
    /**
     * Cast an IBinder object into an android.os.IStringConstants interface,
     * generating a proxy if needed.
     */
    public static android.os.IStringConstants asInterface(android.os.IBinder obj)
    {
      if ((obj==null)) {
        return null;
      }
      android.os.IInterface iin = obj.queryLocalInterface(DESCRIPTOR);
      if (((iin!=null)&&(iin instanceof android.os.IStringConstants))) {
        return ((android.os.IStringConstants)iin);
      }
      return new android.os.IStringConstants.Stub.Proxy(obj);
    }
    @Override public android.os.IBinder asBinder()
    {
      return this;
    }
    @Override public boolean onTransact(int code, android.os.Parcel data, android.os.Parcel reply, int flags) throws android.os.RemoteException
    {
      java.lang.String descriptor = DESCRIPTOR;
      switch (code)
      {
        case INTERFACE_TRANSACTION:
        {
          reply.writeString(descriptor);
          return true;
        }
        default:
        {
          return super.onTransact(code, data, reply, flags);
        }
      }
    }
    private static class Proxy implements android.os.IStringConstants
    {
      private android.os.IBinder mRemote;
      Proxy(android.os.IBinder remote)
      {
        mRemote = remote;
      }
      @Override public android.os.IBinder asBinder()
      {
        return mRemote;
      }
      public java.lang.String getInterfaceDescriptor()
      {
        return DESCRIPTOR;
      }
      public static android.os.IStringConstants sDefaultImpl;
    }
    public static boolean setDefaultImpl(android.os.IStringConstants impl) {
      // Only one user of this interface can use this function
      // at a time. This is a heuristic to detect if two different
      // users in the same process use this function.
      if (Stub.Proxy.sDefaultImpl != null) {
        throw new IllegalStateException("setDefaultImpl() called twice");
      }
      if (impl != null) {
        Stub.Proxy.sDefaultImpl = impl;
        return true;
      }
      return false;
    }
    public static android.os.IStringConstants getDefaultImpl() {
      return Stub.Proxy.sDefaultImpl;
    }
  }
  public static final String EXAMPLE_CONSTANT = "foo";
}
)";

const char kCppOutputPath[] = "some/path/to/output.cpp";
const char kGenHeaderDir[] = "output";
const char kGenInterfaceHeaderPath[] = "output/android/os/IStringConstants.h";
const char kExpectedIHeaderOutput[] =
    R"(#ifndef AIDL_GENERATED_ANDROID_OS_I_STRING_CONSTANTS_H_
#define AIDL_GENERATED_ANDROID_OS_I_STRING_CONSTANTS_H_

#include <binder/IBinder.h>
#include <binder/IInterface.h>
#include <binder/Status.h>
#include <utils/String16.h>
#include <utils/StrongPointer.h>

namespace android {

namespace os {

class IStringConstants : public ::android::IInterface {
public:
  DECLARE_META_INTERFACE(StringConstants)
  static const ::android::String16& EXAMPLE_CONSTANT();
};  // class IStringConstants

class IStringConstantsDefault : public IStringConstants {
public:
  ::android::IBinder* onAsBinder() override {
    return nullptr;
  }
};  // class IStringConstantsDefault

}  // namespace os

}  // namespace android

#endif  // AIDL_GENERATED_ANDROID_OS_I_STRING_CONSTANTS_H_
)";

const char kExpectedCppOutput[] =
    R"(#include <android/os/IStringConstants.h>
#include <android/os/BpStringConstants.h>

namespace android {

namespace os {

DO_NOT_DIRECTLY_USE_ME_IMPLEMENT_META_INTERFACE(StringConstants, "android.os.IStringConstants")

const ::android::String16& IStringConstants::EXAMPLE_CONSTANT() {
  static const ::android::String16 value(::android::String16("foo"));
  return value;
}

}  // namespace os

}  // namespace android
#include <android/os/BpStringConstants.h>
#include <binder/Parcel.h>
#include <android-base/macros.h>

namespace android {

namespace os {

BpStringConstants::BpStringConstants(const ::android::sp<::android::IBinder>& _aidl_impl)
    : BpInterface<IStringConstants>(_aidl_impl){
}

}  // namespace os

}  // namespace android
#include <android/os/BnStringConstants.h>
#include <binder/Parcel.h>
#include <binder/Stability.h>

namespace android {

namespace os {

BnStringConstants::BnStringConstants()
{
  ::android::internal::Stability::markCompilationUnit(this);
}

::android::status_t BnStringConstants::onTransact(uint32_t _aidl_code, const ::android::Parcel& _aidl_data, ::android::Parcel* _aidl_reply, uint32_t _aidl_flags) {
  ::android::status_t _aidl_ret_status = ::android::OK;
  switch (_aidl_code) {
  default:
  {
    _aidl_ret_status = ::android::BBinder::onTransact(_aidl_code, _aidl_data, _aidl_reply, _aidl_flags);
  }
  break;
  }
  if (_aidl_ret_status == ::android::UNEXPECTED_NULL) {
    _aidl_ret_status = ::android::binder::Status::fromExceptionCode(::android::binder::Status::EX_NULL_POINTER).writeToParcel(_aidl_reply);
  }
  return _aidl_ret_status;
}

}  // namespace os

}  // namespace android
)";

const char kExpectedJavaOutputWithVersionAndHash[] =
    R"(/*
 * This file is auto-generated.  DO NOT MODIFY.
 */
package android.os;
public interface IStringConstants extends android.os.IInterface
{
  /**
   * The version of this interface that the caller is built against.
   * This might be different from what {@link #getInterfaceVersion()
   * getInterfaceVersion} returns as that is the version of the interface
   * that the remote object is implementing.
   */
  public static final int VERSION = 10;
  public static final String HASH = "abcdefg";
  /** Default implementation for IStringConstants. */
  public static class Default implements android.os.IStringConstants
  {
    @Override
    public int getInterfaceVersion() {
      return 0;
    }
    @Override
    public String getInterfaceHash() {
      return "";
    }
    @Override
    public android.os.IBinder asBinder() {
      return null;
    }
  }
  /** Local-side IPC implementation stub class. */
  public static abstract class Stub extends android.os.Binder implements android.os.IStringConstants
  {
    private static final java.lang.String DESCRIPTOR = "android.os.IStringConstants";
    /** Construct the stub at attach it to the interface. */
    public Stub()
    {
      this.attachInterface(this, DESCRIPTOR);
    }
    /**
     * Cast an IBinder object into an android.os.IStringConstants interface,
     * generating a proxy if needed.
     */
    public static android.os.IStringConstants asInterface(android.os.IBinder obj)
    {
      if ((obj==null)) {
        return null;
      }
      android.os.IInterface iin = obj.queryLocalInterface(DESCRIPTOR);
      if (((iin!=null)&&(iin instanceof android.os.IStringConstants))) {
        return ((android.os.IStringConstants)iin);
      }
      return new android.os.IStringConstants.Stub.Proxy(obj);
    }
    @Override public android.os.IBinder asBinder()
    {
      return this;
    }
    @Override public boolean onTransact(int code, android.os.Parcel data, android.os.Parcel reply, int flags) throws android.os.RemoteException
    {
      java.lang.String descriptor = DESCRIPTOR;
      switch (code)
      {
        case INTERFACE_TRANSACTION:
        {
          reply.writeString(descriptor);
          return true;
        }
        case TRANSACTION_getInterfaceVersion:
        {
          data.enforceInterface(descriptor);
          reply.writeNoException();
          reply.writeInt(getInterfaceVersion());
          return true;
        }
        case TRANSACTION_getInterfaceHash:
        {
          data.enforceInterface(descriptor);
          reply.writeNoException();
          reply.writeString(getInterfaceHash());
          return true;
        }
        default:
        {
          return super.onTransact(code, data, reply, flags);
        }
      }
    }
    private static class Proxy implements android.os.IStringConstants
    {
      private android.os.IBinder mRemote;
      Proxy(android.os.IBinder remote)
      {
        mRemote = remote;
      }
      private int mCachedVersion = -1;
      private String mCachedHash = "-1";
      @Override public android.os.IBinder asBinder()
      {
        return mRemote;
      }
      public java.lang.String getInterfaceDescriptor()
      {
        return DESCRIPTOR;
      }
      @Override
      public int getInterfaceVersion() throws android.os.RemoteException {
        if (mCachedVersion == -1) {
          android.os.Parcel data = android.os.Parcel.obtain();
          android.os.Parcel reply = android.os.Parcel.obtain();
          try {
            data.writeInterfaceToken(DESCRIPTOR);
            boolean _status = mRemote.transact(Stub.TRANSACTION_getInterfaceVersion, data, reply, 0);
            if (!_status) {
              if (getDefaultImpl() != null) {
                return getDefaultImpl().getInterfaceVersion();
              }
            }
            reply.readException();
            mCachedVersion = reply.readInt();
          } finally {
            reply.recycle();
            data.recycle();
          }
        }
        return mCachedVersion;
      }
      @Override
      public synchronized String getInterfaceHash() throws android.os.RemoteException {
        if ("-1".equals(mCachedHash)) {
          android.os.Parcel data = android.os.Parcel.obtain();
          android.os.Parcel reply = android.os.Parcel.obtain();
          try {
            data.writeInterfaceToken(DESCRIPTOR);
            boolean _status = mRemote.transact(Stub.TRANSACTION_getInterfaceHash, data, reply, 0);
            if (!_status) {
              if (getDefaultImpl() != null) {
                return getDefaultImpl().getInterfaceHash();
              }
            }
            reply.readException();
            mCachedHash = reply.readString();
          } finally {
            reply.recycle();
            data.recycle();
          }
        }
        return mCachedHash;
      }
      public static android.os.IStringConstants sDefaultImpl;
    }
    static final int TRANSACTION_getInterfaceVersion = (android.os.IBinder.FIRST_CALL_TRANSACTION + 16777214);
    static final int TRANSACTION_getInterfaceHash = (android.os.IBinder.FIRST_CALL_TRANSACTION + 16777213);
    public static boolean setDefaultImpl(android.os.IStringConstants impl) {
      // Only one user of this interface can use this function
      // at a time. This is a heuristic to detect if two different
      // users in the same process use this function.
      if (Stub.Proxy.sDefaultImpl != null) {
        throw new IllegalStateException("setDefaultImpl() called twice");
      }
      if (impl != null) {
        Stub.Proxy.sDefaultImpl = impl;
        return true;
      }
      return false;
    }
    public static android.os.IStringConstants getDefaultImpl() {
      return Stub.Proxy.sDefaultImpl;
    }
  }
  public static final String EXAMPLE_CONSTANT = "foo";
  public int getInterfaceVersion() throws android.os.RemoteException;
  public String getInterfaceHash() throws android.os.RemoteException;
}
)";

const char kExpectedIHeaderOutputWithVersionAndHash[] =
    R"(#ifndef AIDL_GENERATED_ANDROID_OS_I_STRING_CONSTANTS_H_
#define AIDL_GENERATED_ANDROID_OS_I_STRING_CONSTANTS_H_

#include <binder/IBinder.h>
#include <binder/IInterface.h>
#include <binder/Status.h>
#include <cstdint>
#include <utils/String16.h>
#include <utils/StrongPointer.h>

namespace android {

namespace os {

class IStringConstants : public ::android::IInterface {
public:
  DECLARE_META_INTERFACE(StringConstants)
  const int32_t VERSION = 10;
  const std::string HASH = "abcdefg";
  static const ::android::String16& EXAMPLE_CONSTANT();
  virtual int32_t getInterfaceVersion() = 0;
  virtual std::string getInterfaceHash() = 0;
};  // class IStringConstants

class IStringConstantsDefault : public IStringConstants {
public:
  ::android::IBinder* onAsBinder() override {
    return nullptr;
  }
  int32_t getInterfaceVersion() override {
    return 0;
  }
  std::string getInterfaceHash() override {
    return "";
  }
};  // class IStringConstantsDefault

}  // namespace os

}  // namespace android

#endif  // AIDL_GENERATED_ANDROID_OS_I_STRING_CONSTANTS_H_
)";

const char kExpectedCppOutputWithVersionAndHash[] =
    R"(#include <android/os/IStringConstants.h>
#include <android/os/BpStringConstants.h>

namespace android {

namespace os {

DO_NOT_DIRECTLY_USE_ME_IMPLEMENT_META_INTERFACE(StringConstants, "android.os.IStringConstants")

const ::android::String16& IStringConstants::EXAMPLE_CONSTANT() {
  static const ::android::String16 value(::android::String16("foo"));
  return value;
}

}  // namespace os

}  // namespace android
#include <android/os/BpStringConstants.h>
#include <binder/Parcel.h>
#include <android-base/macros.h>

namespace android {

namespace os {

BpStringConstants::BpStringConstants(const ::android::sp<::android::IBinder>& _aidl_impl)
    : BpInterface<IStringConstants>(_aidl_impl){
}

int32_t BpStringConstants::getInterfaceVersion() {
  if (cached_version_ == -1) {
    ::android::Parcel data;
    ::android::Parcel reply;
    data.writeInterfaceToken(getInterfaceDescriptor());
    ::android::status_t err = remote()->transact(::android::IBinder::FIRST_CALL_TRANSACTION + 16777214 /* getInterfaceVersion */, data, &reply);
    if (err == ::android::OK) {
      ::android::binder::Status _aidl_status;
      err = _aidl_status.readFromParcel(reply);
      if (err == ::android::OK && _aidl_status.isOk()) {
        cached_version_ = reply.readInt32();
      }
    }
  }
  return cached_version_;
}

std::string BpStringConstants::getInterfaceHash() {
  std::lock_guard<std::mutex> lockGuard(cached_hash_mutex_);
  if (cached_hash_ == "-1") {
    ::android::Parcel data;
    ::android::Parcel reply;
    data.writeInterfaceToken(getInterfaceDescriptor());
    ::android::status_t err = remote()->transact(::android::IBinder::FIRST_CALL_TRANSACTION + 16777213 /* getInterfaceHash */, data, &reply);
    if (err == ::android::OK) {
      ::android::binder::Status _aidl_status;
      err = _aidl_status.readFromParcel(reply);
      if (err == ::android::OK && _aidl_status.isOk()) {
        reply.readUtf8FromUtf16(&cached_hash_);
      }
    }
  }
  return cached_hash_;
}

}  // namespace os

}  // namespace android
#include <android/os/BnStringConstants.h>
#include <binder/Parcel.h>
#include <binder/Stability.h>

namespace android {

namespace os {

BnStringConstants::BnStringConstants()
{
  ::android::internal::Stability::markCompilationUnit(this);
}

::android::status_t BnStringConstants::onTransact(uint32_t _aidl_code, const ::android::Parcel& _aidl_data, ::android::Parcel* _aidl_reply, uint32_t _aidl_flags) {
  ::android::status_t _aidl_ret_status = ::android::OK;
  switch (_aidl_code) {
  case ::android::IBinder::FIRST_CALL_TRANSACTION + 16777214 /* getInterfaceVersion */:
  {
    _aidl_data.checkInterface(this);
    _aidl_reply->writeNoException();
    _aidl_reply->writeInt32(IStringConstants::VERSION);
  }
  break;
  case ::android::IBinder::FIRST_CALL_TRANSACTION + 16777213 /* getInterfaceHash */:
  {
    _aidl_data.checkInterface(this);
    _aidl_reply->writeNoException();
    _aidl_reply->writeUtf8AsUtf16(IStringConstants::HASH);
  }
  break;
  default:
  {
    _aidl_ret_status = ::android::BBinder::onTransact(_aidl_code, _aidl_data, _aidl_reply, _aidl_flags);
  }
  break;
  }
  if (_aidl_ret_status == ::android::UNEXPECTED_NULL) {
    _aidl_ret_status = ::android::binder::Status::fromExceptionCode(::android::binder::Status::EX_NULL_POINTER).writeToParcel(_aidl_reply);
  }
  return _aidl_ret_status;
}

int32_t BnStringConstants::getInterfaceVersion() {
  return IStringConstants::VERSION;
}

std::string BnStringConstants::getInterfaceHash() {
  return IStringConstants::HASH;
}

}  // namespace os

}  // namespace android
)";

}  // namespace string_constants
}  // namespace test_data
}  // namespace aidl
}  // namespace android
