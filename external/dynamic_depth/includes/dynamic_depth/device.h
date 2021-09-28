#ifndef DYNAMIC_DEPTH_INCLUDES_DYNAMIC_DEPTH_DEVICE_H_  // NOLINT
#define DYNAMIC_DEPTH_INCLUDES_DYNAMIC_DEPTH_DEVICE_H_  // NOLINT

#include <libxml/parser.h>
#include <libxml/tree.h>
#include <libxml/xmlmemory.h>
#include <libxml/xpath.h>

#include <memory>
#include <string>
#include <unordered_map>
#include <utility>

#include "dynamic_depth/app_info.h"
#include "dynamic_depth/cameras.h"
#include "dynamic_depth/container.h"
#include "dynamic_depth/earth_pose.h"
#include "dynamic_depth/planes.h"
#include "dynamic_depth/pose.h"
#include "dynamic_depth/profiles.h"
#include "dynamic_depth/vendor_info.h"
#include "xmpmeta/xmp_data.h"

namespace dynamic_depth {

struct DeviceParams {
  // Cameras must be present (i.e. contain at least one camera).
  std::unique_ptr<Cameras> cameras;

  // GContainer. Optional, depending on element presence or user choice.
  std::unique_ptr<Container> container;

  // Optional elements.
  std::unique_ptr<Profiles> profiles;
  std::unique_ptr<Planes> planes;
  std::unique_ptr<EarthPose> earth_pose;
  std::unique_ptr<Pose> pose;
  std::unique_ptr<VendorInfo> vendor_info;
  std::unique_ptr<AppInfo> app_info;

  explicit DeviceParams(std::unique_ptr<Cameras> new_cameras)
      : container(nullptr),
        profiles(nullptr),
        planes(nullptr),
        earth_pose(nullptr),
        pose(nullptr),
        vendor_info(nullptr),
        app_info(nullptr) {
    cameras = std::move(new_cameras);
  }

  inline bool operator==(const DeviceParams& other) const {
    return cameras.get() == other.cameras.get() &&
           container.get() == other.container.get() &&
           profiles.get() == other.profiles.get() &&
           planes.get() == other.planes.get() &&
           earth_pose.get() == other.earth_pose.get() &&
           pose.get() == other.pose.get() &&
           vendor_info.get() == other.vendor_info.get() &&
           app_info.get() == other.app_info.get();
  }

  inline bool operator!=(const DeviceParams& other) const {
    return !(*this == other);
  }
};

// Implements a Device from the Dynamic Depth specification, with serialization
// and deserialization.
// Does not implement the Element interface because Device is at the top level
// in the tree.
class Device {
 public:
  // Creates a Device from the given elements.
  static std::unique_ptr<Device> FromData(std::unique_ptr<DeviceParams> params);

  // Creates a Device from pre-extracted XMP metadata. Returns null if
  // parsing fails. Both the standard and extended XMP sections are required.
  static std::unique_ptr<Device> FromXmp(
      const ::dynamic_depth::xmpmeta::XmpData& xmp);

  // Creates a Device by extracting XMP metadata from a JPEG and parsing it.
  // If using XMP for other things as well, FromXmp() should be used instead to
  // prevent redundant extraction of XMP from the JPEG.
  static std::unique_ptr<Device> FromJpegFile(const string& filename);

  // Creates a Device by parsing XML file containing the metadata.
  static std::unique_ptr<Device> FromXmlFile(const string& filename);

  // Getters.
  // May return null values for optional fields.
  const Cameras* GetCameras() const;
  const Container* GetContainer() const;
  const EarthPose* GetEarthPose() const;
  const Pose* GetPose() const;
  const Planes* GetPlanes() const;
  const Profiles* GetProfiles() const;
  const VendorInfo* GetVendorInfo() const;
  const AppInfo* GetAppInfo() const;

  // Not const for XML memory management reasons. More info in source comments.
  bool SerializeToXmp(::dynamic_depth::xmpmeta::XmpData* xmp);

  // Saves Device metadata to a .xml file.
  bool SerializeToXmlFile(const char* filename);

  // Disallow copying.
  Device(const Device&) = delete;
  void operator=(const Device&) = delete;

 private:
  explicit Device(std::unique_ptr<DeviceParams> params);

  // Retrieves the namespaces of all child elements.
  void GetNamespaces(
      std::unordered_map<string, string>* ns_name_href_map) const;
  // Gathers all the XML namespaces of child elements.
  void PopulateNamespaces();
  bool Serialize(xmlDocPtr* xmlDoc);

  // Keep a reference to the XML namespaces, so that they are created only once
  // when Device is constructed.
  std::unordered_map<string, xmlNsPtr> namespaces_;

  std::unique_ptr<DeviceParams> params_;
};

}  // namespace dynamic_depth

#endif  // DYNAMIC_DEPTH_INCLUDES_DYNAMIC_DEPTH_DEVICE_H_  // NOLINT
