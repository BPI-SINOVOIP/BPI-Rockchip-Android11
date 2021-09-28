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

#include "SingleManifestTest.h"

#include <aidl/metadata.h>
#include <android-base/properties.h>
#include <android-base/strings.h>
#include <binder/IServiceManager.h>
#include <binder/Parcel.h>
#include <binder/Status.h>
#include <gmock/gmock.h>
#include <hidl-util/FqInstance.h>
#include <hidl/HidlTransportUtils.h>
#include <vintf/parse_string.h>

#include <algorithm>

#include "utils.h"

using ::testing::AnyOf;

namespace android {
namespace vintf {
namespace testing {

using android::FqInstance;
using android::vintf::toFQNameString;

// For devices that launched <= Android O-MR1, systems/hals/implementations
// were delivered to companies which either don't start up on device boot.
bool LegacyAndExempt(const FQName &fq_name) {
  return GetShippingApiLevel() <= 27 && !IsAndroidPlatformInterface(fq_name);
}

void FailureHalMissing(const FQName &fq_name, const std::string &instance) {
  if (LegacyAndExempt(fq_name)) {
    cout << "[  WARNING ] " << fq_name.string() << "/" << instance
         << " not available but is exempted because it is legacy. It is still "
            "recommended to fix this."
         << endl;
  } else {
    ADD_FAILURE() << fq_name.string() << "/" << instance << " not available.";
  }
}

void FailureHashMissing(const FQName &fq_name,
                        bool vehicle_hal_in_automotive_device) {
  if (LegacyAndExempt(fq_name)) {
    cout << "[  WARNING ] " << fq_name.string()
         << " has an empty hash but is exempted because it is legacy. It is "
            "still recommended to fix this. This is because it was compiled "
            "without being frozen in a corresponding current.txt file."
         << endl;
  } else if (vehicle_hal_in_automotive_device) {
    cout << "[  WARNING ] " << fq_name.string()
         << " has an empty hash but is exempted because it is IVehicle in an"
            "automotive device."
         << endl;
  } else {
    ADD_FAILURE()
        << fq_name.string()
        << " has an empty hash. This is because it was compiled "
           "without being frozen in a corresponding current.txt file.";
  }
}

template <typename It>
static string RangeInstancesToString(const std::pair<It, It> &range) {
  std::stringstream ss;
  for (auto it = range.first; it != range.second; ++it) {
    if (it != range.first) ss << ", ";
    ss << it->second.string();
  }
  return ss.str();
}

template <typename Container>
static string InstancesToString(const Container &container) {
  std::stringstream ss;
  for (auto it = container.begin(); it != container.end(); ++it) {
    if (it != container.begin()) ss << ", ";
    ss << *it;
  }
  return ss.str();
}

static FqInstance ToFqInstance(const string &interface,
                               const string &instance) {
  FqInstance fq_interface;
  FqInstance ret;

  if (!fq_interface.setTo(interface)) {
    ADD_FAILURE() << interface << " is not a valid FQName";
    return ret;
  }
  if (!ret.setTo(fq_interface.getPackage(), fq_interface.getMajorVersion(),
                 fq_interface.getMinorVersion(), fq_interface.getInterface(),
                 instance)) {
    ADD_FAILURE() << "Cannot convert to FqInstance: " << interface << "/"
                  << instance;
  }
  return ret;
}

// Given android.foo.bar@x.y::IFoo/default, attempt to get
// android.foo.bar@x.y::IFoo/default, android.foo.bar@x.(y-1)::IFoo/default,
// ... android.foo.bar@x.0::IFoo/default until the passthrough HAL is retrieved.
static sp<IBase> GetPassthroughService(const FqInstance &fq_instance) {
  for (size_t minor_version = fq_instance.getMinorVersion();; --minor_version) {
    // String out instance name from fq_instance.
    FqInstance interface;
    if (!interface.setTo(fq_instance.getPackage(),
                         fq_instance.getMajorVersion(), minor_version,
                         fq_instance.getInterface())) {
      ADD_FAILURE() << fq_instance.string()
                    << " doesn't contain a valid FQName";
      return nullptr;
    }

    auto hal_service = VtsTrebleVintfTestBase::GetHalService(
        interface.string(), fq_instance.getInstance(), Transport::PASSTHROUGH);

    if (hal_service != nullptr) {
      bool interface_chain_valid = false;
      hal_service->interfaceChain([&](const auto &chain) {
        for (const auto &intf : chain) {
          if (intf == interface.string()) {
            interface_chain_valid = true;
            return;
          }
        }
      });
      if (!interface_chain_valid) {
        ADD_FAILURE() << "Retrieved " << interface.string() << "/"
                      << fq_instance.getInstance() << " as "
                      << fq_instance.string()
                      << " but interfaceChain() doesn't contain "
                      << fq_instance.string();
        return nullptr;
      }
      cout << "Retrieved " << interface.string() << "/"
           << fq_instance.getInstance() << " as " << fq_instance.string()
           << endl;
      return hal_service;
    }

    if (minor_version == 0) {
      return nullptr;
    }
  }
  ADD_FAILURE() << "Should not reach here";
  return nullptr;
}

// Tests that no HAL outside of the allowed set is specified as passthrough in
// VINTF.
TEST_P(SingleManifestTest, HalsAreBinderized) {
  multimap<Transport, FqInstance> instances;
  ForEachHidlHalInstance(GetParam(), [&instances](const FQName &fq_name,
                                                  const string &instance_name,
                                                  Transport transport) {
    FqInstance fqInstance;
    ASSERT_TRUE(fqInstance.setTo(
        fq_name.package(), fq_name.getPackageMajorVersion(),
        fq_name.getPackageMinorVersion(), fq_name.name(), instance_name));
    instances.emplace(transport, std::move(fqInstance));
  });

  for (auto it = instances.begin(); it != instances.end();
       it = instances.upper_bound(it->first)) {
    EXPECT_THAT(it->first, AnyOf(Transport::HWBINDER, Transport::PASSTHROUGH))
        << "The following HALs has unknown transport specified in VINTF ("
        << it->first << ", ordinal "
        << static_cast<std::underlying_type_t<Transport>>(it->first) << ")"
        << RangeInstancesToString(instances.equal_range(it->first));
  }

  auto passthrough_declared_range =
      instances.equal_range(Transport::PASSTHROUGH);
  set<FqInstance> passthrough_declared;
  std::transform(
      passthrough_declared_range.first, passthrough_declared_range.second,
      std::inserter(passthrough_declared, passthrough_declared.begin()),
      [](const auto &pair) { return pair.second; });

  set<FqInstance> passthrough_allowed;
  for (const auto &declared_instance : passthrough_declared) {
    auto hal_service = GetPassthroughService(declared_instance);

    // For vendor extensions, hal_service may be null because we don't know
    // its interfaceChain()[1] to call getService(). However, the base interface
    // should be declared in the manifest, so other iterations of this for-loop
    // verify that vendor extension.
    if (hal_service == nullptr) {
      cout << "Skip calling interfaceChain on " << declared_instance.string()
           << " because it can't be retrieved directly." << endl;
      continue;
    }

    // For example, given the following interfaceChain when
    // hal_service is "android.hardware.mapper@2.0::IMapper/default":
    // ["vendor.foo.mapper@1.0::IMapper",
    //  "android.hardware.mapper@2.1::IMapper",
    //  "android.hardware.mapper@2.0::IMapper",
    //  "android.hidl.base@1.0::IBase"],
    // Allow the following:
    // ["vendor.foo.mapper@1.0::IMapper/default",
    //  "android.hardware.mapper@2.1::IMapper/default",
    //  "android.hardware.mapper@2.0::IMapper/default"]
    hal_service->interfaceChain([&](const auto &chain) {
      vector<FqInstance> fq_instances;
      std::transform(
          chain.begin(), chain.end(), std::back_inserter(fq_instances),
          [&](const auto &interface) {
            return ToFqInstance(interface, declared_instance.getInstance());
          });

      bool allowing = false;
      for (auto it = fq_instances.rbegin(); it != fq_instances.rend(); ++it) {
        if (kPassthroughHals.find(it->getPackage()) != kPassthroughHals.end()) {
          allowing = true;
        }
        if (allowing) {
          cout << it->string() << " is allowed to be passthrough" << endl;
          passthrough_allowed.insert(*it);
        }
      }
    });
  }

  set<FqInstance> passthrough_not_allowed;
  std::set_difference(
      passthrough_declared.begin(), passthrough_declared.end(),
      passthrough_allowed.begin(), passthrough_allowed.end(),
      std::inserter(passthrough_not_allowed, passthrough_not_allowed.begin()));

  EXPECT_TRUE(passthrough_not_allowed.empty())
      << "The following HALs can't be passthrough under Treble rules: ["
      << InstancesToString(passthrough_not_allowed) << "].";
}

// Tests that all HALs specified in the VINTF are available through service
// manager.
// This tests (HAL in manifest) => (HAL is served)
TEST_P(SingleManifestTest, HalsAreServed) {
  // Returns a function that verifies that HAL is available through service
  // manager and is served from a specific set of partitions.
  auto is_available_from =
      [this](Partition expected_partition) -> HidlVerifyFn {
    return [this, expected_partition](const FQName &fq_name,
                                      const string &instance_name,
                                      Transport transport) {
      sp<IBase> hal_service;

      if (transport == Transport::PASSTHROUGH) {
        using android::hardware::details::canCastInterface;

        // Passthrough services all start with minor version 0.
        // there are only three of them listed above. They are looked
        // up based on their binary location. For instance,
        // V1_0::IFoo::getService() might correspond to looking up
        // android.hardware.foo@1.0-impl for the symbol
        // HIDL_FETCH_IFoo. For @1.1::IFoo to continue to work with
        // 1.0 clients, it must also be present in a library that is
        // called the 1.0 name. Clients can say:
        //     mFoo1_0 = V1_0::IFoo::getService();
        //     mFoo1_1 = V1_1::IFoo::castFrom(mFoo1_0);
        // This is the standard pattern for making a service work
        // for both versions (mFoo1_1 != nullptr => you have 1.1)
        // and a 1.0 client still works with the 1.1 interface.

        if (!IsAndroidPlatformInterface(fq_name)) {
          // This isn't the case for extensions of core Google interfaces.
          return;
        }

        const FQName lowest_name =
            fq_name.withVersion(fq_name.getPackageMajorVersion(), 0);
        hal_service = GetHalService(lowest_name, instance_name, transport);
        EXPECT_TRUE(
            canCastInterface(hal_service.get(), fq_name.string().c_str()))
            << fq_name.string() << " is not on the device.";
      } else {
        hal_service = GetHalService(fq_name, instance_name, transport);
      }

      if (hal_service == nullptr) {
        FailureHalMissing(fq_name, instance_name);
        return;
      }

      EXPECT_EQ(transport == Transport::HWBINDER, hal_service->isRemote())
          << "transport is " << transport << "but HAL service is "
          << (hal_service->isRemote() ? "" : "not") << " remote.";
      EXPECT_EQ(transport == Transport::PASSTHROUGH, !hal_service->isRemote())
          << "transport is " << transport << "but HAL service is "
          << (hal_service->isRemote() ? "" : "not") << " remote.";

      if (!hal_service->isRemote()) return;

      Partition partition = GetPartition(hal_service);
      if (partition == Partition::UNKNOWN) return;
      EXPECT_EQ(expected_partition, partition)
          << fq_name.string() << "/" << instance_name << " is in partition "
          << partition << " but is expected to be in " << expected_partition;
    };
  };

  auto manifest = GetParam();
  ForEachHidlHalInstance(manifest,
                         is_available_from(PartitionOfType(manifest->type())));
}

// Tests that all HALs which are served are specified in the VINTF
// This tests (HAL is served) => (HAL in manifest)
TEST_P(SingleManifestTest, ServedHwbinderHalsAreInManifest) {
  auto manifest = GetParam();
  auto expected_partition = PartitionOfType(manifest->type());
  std::set<std::string> manifest_hwbinder_hals_ = GetHwbinderHals(manifest);

  Return<void> ret = default_manager_->list([&](const auto &list) {
    for (const auto &name : list) {
      if (std::string(name).find(IBase::descriptor) == 0) continue;

      FqInstance fqInstanceName;
      EXPECT_TRUE(fqInstanceName.setTo(name));

      auto service =
          GetHalService(toFQNameString(fqInstanceName.getPackage(),
                                       fqInstanceName.getVersion(),
                                       fqInstanceName.getInterface()),
                        fqInstanceName.getInstance(), Transport::HWBINDER);
      ASSERT_NE(service, nullptr);

      Partition partition = GetPartition(service);
      if (partition == Partition::UNKNOWN) {
        // Caught by SystemVendorTest.ServedHwbinderHalsAreInManifest
        // if that test is run.
        return;
      }
      if (partition == expected_partition) {
        EXPECT_NE(manifest_hwbinder_hals_.find(name),
                  manifest_hwbinder_hals_.end())
            << name << " is being served, but it is not in a manifest.";
      }
    }
  });
  EXPECT_TRUE(ret.isOk());
}

TEST_P(SingleManifestTest, ServedPassthroughHalsAreInManifest) {
  auto manifest = GetParam();
  std::set<std::string> manifest_passthrough_hals_ =
      GetPassthroughHals(manifest);

  auto passthrough_interfaces_declared = [&manifest_passthrough_hals_](
                                             const FQName &fq_name,
                                             const string &instance_name,
                                             Transport transport) {
    if (transport != Transport::PASSTHROUGH) return;

    // See HalsAreServed. These are always retrieved through the base interface
    // and if it is not a google defined interface, it must be an extension of
    // one.
    if (!IsAndroidPlatformInterface(fq_name)) return;

    const FQName lowest_name =
        fq_name.withVersion(fq_name.getPackageMajorVersion(), 0);
    sp<IBase> hal_service =
        GetHalService(lowest_name, instance_name, transport);
    if (hal_service == nullptr) {
      ADD_FAILURE() << "Could not get service " << fq_name.string() << "/"
                    << instance_name;
      return;
    }

    Return<void> ret = hal_service->interfaceChain(
        [&manifest_passthrough_hals_, &instance_name](const auto &interfaces) {
          for (const auto &interface : interfaces) {
            if (std::string(interface) == IBase::descriptor) continue;

            const std::string instance =
                std::string(interface) + "/" + instance_name;
            EXPECT_NE(manifest_passthrough_hals_.find(instance),
                      manifest_passthrough_hals_.end())
                << "Instance missing from manifest: " << instance;
          }
        });
    EXPECT_TRUE(ret.isOk());
  };
  ForEachHidlHalInstance(manifest, passthrough_interfaces_declared);
}

// Tests that HAL interfaces are officially released.
TEST_P(SingleManifestTest, InterfacesAreReleased) {
  // Device support automotive features.
  const static bool automotive_device =
      DeviceSupportsFeature("android.hardware.type.automotive");
  // Verifies that HAL are released by fetching the hash of the interface and
  // comparing it to the set of known hashes of released interfaces.
  HidlVerifyFn is_released = [](const FQName &fq_name,
                                const string &instance_name,
                                Transport transport) {
    // See HalsAreServed. These are always retrieved through the base interface
    // and if it is not a google defined interface, it must be an extension of
    // one.
    if (transport == Transport::PASSTHROUGH &&
        (!IsAndroidPlatformInterface(fq_name) ||
         fq_name.getPackageMinorVersion() != 0)) {
      return;
    }

    sp<IBase> hal_service = GetHalService(fq_name, instance_name, transport);

    if (hal_service == nullptr) {
      FailureHalMissing(fq_name, instance_name);
      return;
    }

    vector<string> iface_chain = GetInterfaceChain(hal_service);

    vector<string> hash_chain{};
    hal_service->getHashChain(
        [&hash_chain](const hidl_vec<HashCharArray> &chain) {
          for (const HashCharArray &hash_array : chain) {
            vector<uint8_t> hash{hash_array.data(),
                                 hash_array.data() + hash_array.size()};
            hash_chain.push_back(Hash::hexString(hash));
          }
        });

    ASSERT_EQ(iface_chain.size(), hash_chain.size());
    for (size_t i = 0; i < iface_chain.size(); ++i) {
      FQName fq_iface_name;
      if (!FQName::parse(iface_chain[i], &fq_iface_name)) {
        ADD_FAILURE() << "Could not parse iface name " << iface_chain[i]
                      << " from interface chain of " << fq_name.string();
        return;
      }
      string hash = hash_chain[i];

      bool vehicle_hal_in_automotive_device =
          automotive_device &&
          fq_iface_name.string() ==
              "android.hardware.automotive.vehicle@2.0::IVehicle";
      if (hash == Hash::hexString(Hash::kEmptyHash)) {
        FailureHashMissing(fq_iface_name, vehicle_hal_in_automotive_device);
      }

      if (IsAndroidPlatformInterface(fq_iface_name) &&
          !vehicle_hal_in_automotive_device) {
        set<string> released_hashes = ReleasedHashes(fq_iface_name);
        EXPECT_NE(released_hashes.find(hash), released_hashes.end())
            << "Hash not found. This interface was not released." << endl
            << "Interface name: " << fq_iface_name.string() << endl
            << "Hash: " << hash << endl;
      }
    }
  };

  ForEachHidlHalInstance(GetParam(), is_released);
}

static std::vector<std::string> hashesForInterface(const std::string &name) {
  for (const auto &module : AidlInterfaceMetadata::all()) {
    if (std::find(module.types.begin(), module.types.end(), name) !=
        module.types.end()) {
      return module.hashes;
    }
  }
  return {};
}

// TODO(b/150155678): using standard code to do this
static std::string getInterfaceHash(const sp<IBinder> &binder) {
  Parcel data;
  Parcel reply;
  data.writeInterfaceToken(binder->getInterfaceDescriptor());
  status_t err =
      binder->transact(IBinder::LAST_CALL_TRANSACTION - 1, data, &reply, 0);
  if (err == UNKNOWN_TRANSACTION) {
    return "";
  }
  EXPECT_EQ(OK, err);
  binder::Status status;
  EXPECT_EQ(OK, status.readFromParcel(reply));
  EXPECT_TRUE(status.isOk()) << status.toString8().c_str();
  std::string str;
  EXPECT_EQ(OK, reply.readUtf8FromUtf16(&str));
  return str;
}

// An AIDL HAL with VINTF stability can only be registered if it is in the
// manifest. However, we still must manually check that every declared HAL is
// actually present on the device.
TEST_P(SingleManifestTest, ManifestAidlHalsServed) {
  AidlVerifyFn expect_available = [](const string &package,
                                     const string &interface,
                                     const string &instance) {
    const std::string type = package + "." + interface;
    const std::string name = type + "/" + instance;
    sp<IBinder> binder =
        defaultServiceManager()->waitForService(String16(name.c_str()));
    EXPECT_NE(binder, nullptr) << "Failed to get " << name;

    const std::string hash = getInterfaceHash(binder);
    const std::vector<std::string> hashes = hashesForInterface(type);

    const bool is_aosp = base::StartsWith(package, "android.");
    const bool is_release =
        base::GetProperty("ro.build.version.codename", "") == "REL";
    const bool found_hash =
        std::find(hashes.begin(), hashes.end(), hash) != hashes.end();

    if (is_aosp) {
      if (!found_hash) {
        if (is_release) {
          ADD_FAILURE() << "Interface " << name
                        << " has an unrecognized hash: '" << hash
                        << "'. The following hashes are known:\n"
                        << base::Join(hashes, '\n')
                        << "\nHAL interfaces must be released and unchanged.";
        } else {
          std::cout << "INFO: using unfrozen hash '" << hash << "' for " << type
                    << ". This will become an error upon release." << std::endl;
        }
      }
    } else {
      // is extension
      //
      // we only require that these are frozen, but we cannot check them for
      // accuracy
      if (hash.empty()) {
        if (is_release) {
          ADD_FAILURE() << "Interface " << name
                        << " is used but not frozen (cannot find hash for it).";
        } else {
          std::cout << "INFO: missing hash for " << type
                    << ". This will become an error upon release." << std::endl;
        }
      }
    }

  };

  ForEachAidlHalInstance(GetParam(), expect_available);
}

}  // namespace testing
}  // namespace vintf
}  // namespace android
