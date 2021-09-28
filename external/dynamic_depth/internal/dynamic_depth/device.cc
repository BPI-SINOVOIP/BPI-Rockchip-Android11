#include "dynamic_depth/device.h"

#include <libxml/tree.h>

#include "android-base/logging.h"
#include "dynamic_depth/const.h"
#include "dynamic_depth/vendor_info.h"
#include "xmpmeta/xml/const.h"
#include "xmpmeta/xml/deserializer_impl.h"
#include "xmpmeta/xml/search.h"
#include "xmpmeta/xml/serializer_impl.h"
#include "xmpmeta/xml/utils.h"
#include "xmpmeta/xmp_data.h"
#include "xmpmeta/xmp_parser.h"
#include "xmpmeta/xmp_writer.h"

using ::dynamic_depth::xmpmeta::CreateXmpData;
using ::dynamic_depth::xmpmeta::XmpData;
using ::dynamic_depth::xmpmeta::xml::DepthFirstSearch;
using ::dynamic_depth::xmpmeta::xml::DeserializerImpl;
using ::dynamic_depth::xmpmeta::xml::GetFirstDescriptionElement;
using ::dynamic_depth::xmpmeta::xml::Serializer;
using ::dynamic_depth::xmpmeta::xml::SerializerImpl;
using ::dynamic_depth::xmpmeta::xml::ToXmlChar;
using ::dynamic_depth::xmpmeta::xml::XmlConst;

namespace dynamic_depth {
namespace {

const char kRevision[] = "Revision";
const char kNamespaceHref[] = "http://ns.google.com/photos/dd/1.0/device/";

// Parses Device fields and children elements from xmlDocPtr.
std::unique_ptr<Device> ParseFields(const xmlDocPtr& xmlDoc) {
  // Find and parse the Device node.
  // Only these two fields are required to be present; the rest are optional.
  // TODO(miraleung): Search for Device by namespace.
  xmlNodePtr description_node =
      DepthFirstSearch(xmlDoc, XmlConst::RdfDescription());
  if (description_node == nullptr) {
    LOG(ERROR) << "No rdf description found";
    return nullptr;
  }

  const DeserializerImpl deserializer(description_node);
  auto cameras = Cameras::FromDeserializer(deserializer);
  if (cameras == nullptr) {
    LOG(ERROR) << "No cameras found";
    return nullptr;
  }

  auto container = Container::FromDeserializer(deserializer);

  // The list of cameras is now guaranteed to have at least one element, because
  // of implementation in cameras.cc
  auto planes = Planes::FromDeserializer(deserializer);
  auto earth_pose = EarthPose::FromDeserializer(deserializer);
  auto pose = Pose::FromDeserializer(deserializer, DynamicDepthConst::Device());
  auto profiles = Profiles::FromDeserializer(deserializer);
  auto vendor_info =
      VendorInfo::FromDeserializer(deserializer, DynamicDepthConst::Device());
  auto app_info =
      AppInfo::FromDeserializer(deserializer, DynamicDepthConst::Device());

  std::unique_ptr<DeviceParams> params(
      new DeviceParams(std::move(cameras)));  // NOLINT
  params->container = std::move(container);
  params->planes = std::move(planes);
  params->earth_pose = std::move(earth_pose);
  params->pose = std::move(pose);
  params->profiles = std::move(profiles);
  params->vendor_info = std::move(vendor_info);
  params->app_info = std::move(app_info);
  return Device::FromData(std::move(params));
}

// Parses Device fields and children elements from XmpData.
std::unique_ptr<Device> ParseFields(const XmpData& xmp) {
  if (xmp.ExtendedSection() == nullptr) {
    LOG(ERROR) << "XMP extended section is null";
    return nullptr;
  }

  return ParseFields(xmp.ExtendedSection());
}

}  // namespace

// Private constructor.
Device::Device(std::unique_ptr<DeviceParams> params) {
  params_ = std::move(params);
}

// Public methods.
std::unique_ptr<Device> Device::FromData(std::unique_ptr<DeviceParams> params) {
  if (params->cameras == nullptr) {
    LOG(ERROR) << "At least one camera must be provided";
    return nullptr;
  }

  // The list of cameras is now guaranteed to have at least one element, because
  // of the implementation in cameras.cc
  return std::unique_ptr<Device>(new Device(std::move(params)));  // NOLINT
}

std::unique_ptr<Device> Device::FromXmp(const XmpData& xmp) {
  return ParseFields(xmp);
}

std::unique_ptr<Device> Device::FromJpegFile(const string& filename) {
  XmpData xmp;
  const bool kSkipExtended = false;
  if (!ReadXmpHeader(filename, kSkipExtended, &xmp)) {
    return nullptr;
  }
  return FromXmp(xmp);
}

// Creates a Device by parsing XML file containing the metadata.
std::unique_ptr<Device> Device::FromXmlFile(const string& filename) {
  xmlDocPtr xmlDoc = xmlReadFile(filename.c_str(), nullptr, 0);
  if (xmlDoc == nullptr) {
    LOG(ERROR) << "Failed to read file: " << filename;
    return nullptr;
  }

  auto device = ParseFields(xmlDoc);
  xmlFreeDoc(xmlDoc);
  return device;
}

const Cameras* Device::GetCameras() const { return params_->cameras.get(); }

const Container* Device::GetContainer() const {
  return params_->container.get();
}

const EarthPose* Device::GetEarthPose() const {
  return params_->earth_pose.get();
}

const Pose* Device::GetPose() const { return params_->pose.get(); }

const Planes* Device::GetPlanes() const { return params_->planes.get(); }

const Profiles* Device::GetProfiles() const { return params_->profiles.get(); }

const VendorInfo* Device::GetVendorInfo() const {
  return params_->vendor_info.get();
}

const AppInfo* Device::GetAppInfo() const { return params_->app_info.get(); }

// This cannot be const because of memory management for the namespaces.
// namespaces_ are freed when the XML document(s) in xmp are freed.
// If namespaces_ are populated at object creation time and this
// object is serialized, freeing the xmlNs objects in the destructor will result
// memory management errors.
bool Device::SerializeToXmp(XmpData* xmp) {
  if (xmp == nullptr || xmp->StandardSection() == nullptr ||
      xmp->ExtendedSection() == nullptr) {
    LOG(ERROR) << "XmpData or its sections are null";
    return false;
  }
  return Serialize(xmp->MutableExtendedSection());
}

bool Device::SerializeToXmlFile(const char* filename) {
  std::unique_ptr<XmpData> xmp_data = CreateXmpData(true);
  if (!Serialize(xmp_data->MutableExtendedSection())) {
    return false;
  }
  return xmlSaveFile(filename, xmp_data->ExtendedSection()) != -1;
}

// Private methods.
bool Device::Serialize(xmlDocPtr* xmlDoc) {
  xmlNodePtr root_node = GetFirstDescriptionElement(*xmlDoc);
  if (root_node == nullptr) {
    LOG(ERROR) << "Extended section has no rdf:Description node";
    return false;
  }

  if (params_->cameras == nullptr) {
    LOG(ERROR) << "At least one camera must be present, stopping serialization";
    return false;
  }

  PopulateNamespaces();
  xmlNsPtr prev_ns = root_node->ns;
  for (const auto& entry : namespaces_) {
    if (prev_ns != nullptr) {
      prev_ns->next = entry.second;
    }
    prev_ns = entry.second;
  }

  // Set up serialization on the first description node in the extended section.
  SerializerImpl device_serializer(namespaces_, root_node);

  // Serialize elements.
  if (params_->container &&
      !params_->container->Serialize(&device_serializer)) {
    return false;
  }

  if (params_->earth_pose) {
    std::unique_ptr<Serializer> earth_pose_serializer =
        device_serializer.CreateSerializer(
            DynamicDepthConst::Namespace(DynamicDepthConst::EarthPose()),
            DynamicDepthConst::EarthPose());
    if (!params_->earth_pose->Serialize(earth_pose_serializer.get())) {
      return false;
    }
  }

  if (params_->pose) {
    std::unique_ptr<Serializer> pose_serializer =
        device_serializer.CreateSerializer(DynamicDepthConst::Device(),
                                           DynamicDepthConst::Pose());
    if (!params_->pose->Serialize(pose_serializer.get())) {
      return false;
    }
  }

  if (params_->profiles && !params_->profiles->Serialize(&device_serializer)) {
    return false;
  }

  // Serialize Planes before Cameras, since the data in Planes is likely to be
  // significantly smaller than the potential media types in a Camera.
  if (params_->planes && !params_->planes->Serialize(&device_serializer)) {
    return false;
  }

  if (params_->cameras && !params_->cameras->Serialize(&device_serializer)) {
    return false;
  }

  if (params_->vendor_info) {
    std::unique_ptr<Serializer> vendor_info_serializer =
        device_serializer.CreateSerializer(DynamicDepthConst::Device(),
                                           DynamicDepthConst::VendorInfo());
    if (!params_->vendor_info->Serialize(vendor_info_serializer.get())) {
      return false;
    }
  }

  if (params_->app_info) {
    std::unique_ptr<Serializer> app_info_serializer =
        device_serializer.CreateSerializer(DynamicDepthConst::Device(),
                                           DynamicDepthConst::AppInfo());
    if (!params_->app_info->Serialize(app_info_serializer.get())) {
      return false;
    }
  }

  return true;
}
void Device::GetNamespaces(
    std::unordered_map<string, string>* ns_name_href_map) const {
  if (ns_name_href_map == nullptr) {
    LOG(ERROR) << "Namespace list is null";
    return;
  }
  ns_name_href_map->emplace(XmlConst::RdfPrefix(), XmlConst::RdfNodeNs());
  ns_name_href_map->emplace(DynamicDepthConst::Device(), kNamespaceHref);
  if (params_->earth_pose) {
    params_->earth_pose->GetNamespaces(ns_name_href_map);
  }
  if (params_->pose) {
    params_->pose->GetNamespaces(ns_name_href_map);
  }
  if (params_->profiles) {
    params_->profiles->GetNamespaces(ns_name_href_map);
  }
  if (params_->planes) {
    params_->planes->GetNamespaces(ns_name_href_map);
  }
  if (params_->cameras) {
    params_->cameras->GetNamespaces(ns_name_href_map);
  }
  if (params_->container) {
    params_->container->GetNamespaces(ns_name_href_map);
  }
  if (params_->vendor_info) {
    params_->vendor_info->GetNamespaces(ns_name_href_map);
  }
  if (params_->app_info) {
    params_->app_info->GetNamespaces(ns_name_href_map);
  }
}

// Gathers all the XML namespaces of child elements.
void Device::PopulateNamespaces() {
  std::unordered_map<string, string> ns_name_href_map;
  GetNamespaces(&ns_name_href_map);
  for (const auto& entry : ns_name_href_map) {
    if (!namespaces_.count(entry.first)) {
      namespaces_.emplace(entry.first,
                          xmlNewNs(nullptr, ToXmlChar(entry.second.data()),
                                   ToXmlChar(entry.first.data())));
    }
  }
}

}  // namespace dynamic_depth
