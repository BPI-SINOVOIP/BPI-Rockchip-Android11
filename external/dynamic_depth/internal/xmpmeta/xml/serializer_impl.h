#ifndef DYNAMIC_DEPTH_INTERNAL_XMPMETA_XML_SERIALIZER_IMPL_H_  // NOLINT
#define DYNAMIC_DEPTH_INTERNAL_XMPMETA_XML_SERIALIZER_IMPL_H_  // NOLINT

#include <libxml/tree.h>

#include <string>
#include <unordered_map>

#include "xmpmeta/xml/serializer.h"

namespace dynamic_depth {
namespace xmpmeta {
namespace xml {

// Writes properties, lists, and child nodes into an XML structure.
//
// Usage example:
//  std::unordered_map<string, xmlNsPtr> namespaces;
//  string device_name("Device");
//  string cameras_name("Cameras");
//  string camera_name("Camera");
//  string audio_name("Audio");
//  string image_name("Image");
//  PopulateNamespaces(&namespaces);
//  DoSerialization();
//
//  // Serialization example.
//  void DoSerialization() {
//    xmlNodePtr device_node = xmlNewNode(nullptr, device_name);
//    Serializer device_serializer(namespaces, device_node);
//
//    std::unique_ptr<Serializer> cameras_serializer =
//        serializer->CreateListSerializer(cameras_name);
//    for (XdmCamera *camera : camera_list_) {
//      std::unique_ptr<Serializer> camera_serializer =
//          cameras_serializer->CreateItemSerializer("Device", camera_name);
//      success &= camera->Serialize(camera_serializer.get());
//
//      // Serialize Audio.
//      std::unique_ptr<Serializer> audio_serializer =
//          camera_serializer->CreateSerializer("Camera", audio_name);
//      audio_serializer->WriteProperty(camera_name, "Data", audio_data);
//      audio_serializer->WriteProperty(camera_name, "Mime", "audio/mp4");
//
//      // Serialize Image.
//      std::unique_ptr<Serializer> image_serializer =
//          camera_serializer->CreateSerializer("Camera", image_name);
//      image_serializer->WriteProperty(image_name, "Data", image_data);
//      image_serializer->WriteProperty(image_name, "Mime", "image/jpeg");
//
//      // Serialize ImagingModel.
//      std::unique_ptr<Serializer> imaging_model_serializer =
//          camera_serializer->CreateSerializer("Camera", "ImagingModel");
//      std::unique_ptr<Serializer> equirect_model_serializer =
//          imaging_model_serializer->CreateSerializer("Camera",
//                                                     "EquirectModel");
//      // Serializer equirect model fields here.
//    }
//  }
//
// Resulting XML structure:
// /*
//  * <Device>
//  *   <Device:Cameras>
//  *     <rdf:Seq>
//  *       <rdf:li>
//  *         <Device:Camera>
//  *             <Camera:Audio Audio:Mime="audio/mp4" Audio:Data="DataValue"/>
//  *             <Camera:Image Image:Mime="image/jpeg" Image:Data="DataValue"/>
//  *             <Camera:ImagingModel>
//  *               <Camera:EquirectModel ...properties/>
//  *             </Camera:ImagingModel>
//  *         </Device:Camera>
//  *       </rdf:li>
//  *     </rdf:Seq>
//  *   </Device:Cameras>
//  * </Device>
//  */
//
// // Namespace population example.
// void PopulateNamespaces(std::unordered_map<string, xmlNsPtr>* namespaces) {
//   xmlNsPtr device_ns =
//       xmlNewNs(nullptr, ToXmlChar("http://ns.xdm.org/photos/1.0/device")
//                ToXmlChar(device_name.data()));
//   xmlNsPtr camera_ns =
//       xmlNewNs(nullptr, ToXmlChar("http://ns.xdm.org/photos/1.0/camera")
//                ToXmlChar(camera_name.data()));
//   xmlNsPtr audio_ns =
//       xmlNewNs(nullptr, ToXmlChar("http://ns.xdm.org/photos/1.0/audio")
//                ToXmlChar(audio_name.data()));
//   xmlNsPtr image_ns =
//       xmlNewNs(nullptr, ToXmlChar("http://ns.xdm.org/photos/1.0/image")
//                ToXmlChar(image_name.data()));
//   namespaces->insert(device_name, device_ns);
//   namespaces->insert(camera_name, camera_ns);
//   namespaces->insert(audio_name, audio_ns);
//   namespaces->insert(image_name, image_ns);
// }

class SerializerImpl : public Serializer {
 public:
  // Constructor.
  // The prefix map is required if one of the CreateSerializer methods will be
  // called on this object. In particular, the RDF namespace must be present in
  // the prefix map if CreateItemSerializer or CreateListSerializer will be
  // called.
  // The namespaces map serves to keep XML namespace creation out of this
  // Serializer, to simplify memory management issues. Note that the libxml
  // xmlDocPtr will own all namespace and node pointers.
  // The namespaces parameter is a map of node names to full namespaces.
  // This contains all the namespaces (nodes and properties) that will be used
  // in serialization.
  // The node parameter is the caller node. This will be the node in which
  // serialization takes place in WriteProperties.
  SerializerImpl(const std::unordered_map<string, xmlNsPtr>& namespaces,
                 xmlNodePtr node);

  // Returns a new Serializer for an object that is part of an rdf:Seq list
  // of objects.
  // The parent serializer must be created with CreateListSerializer.
  std::unique_ptr<Serializer> CreateItemSerializer(
      const string& prefix, const string& item_name) const override;

  // Returns a new Serializer for a list of objects that correspond to an
  // rdf:Seq XML node, where each object is to be serialized as a child node of
  // every rdf:li node in the list.
  // The serializer is created on an rdf:Seq node, which is the child of a
  // newly created XML node with the name list_name.
  std::unique_ptr<Serializer> CreateListSerializer(
      const string& prefix, const string& list_name) const override;

  // Creates a serializer from the current serializer.
  // @param node_name The name of the caller node. This will be the parent of
  // any new nodes or properties set by this serializer.
  std::unique_ptr<Serializer> CreateSerializer(
      const string& node_ns_name, const string& node_name) const override;

  // Writes the property into the current node, prefixed with prefix if it
  // has a corresponding namespace href in namespaces_, fails otherwise.
  // Returns true if serialization is successful.
  // If prefix is empty, the property will not be set on an XML namespace.
  // name must not be empty.
  // value may be empty.
  bool WriteBoolProperty(const string& prefix, const string& name,
                         bool value) const override;
  bool WriteProperty(const string& prefix, const string& name,
                     const string& value) const override;

  // Writes the collection of numbers into a child rdf:Seq node.
  bool WriteIntArray(const string& prefix, const string& array_name,
                     const std::vector<int>& values) const override;
  bool WriteDoubleArray(const string& prefix, const string& array_name,
                        const std::vector<double>& values) const override;

  // Class-specific methods.
  // Constructs a serializer object and writes the xmlNsPtr objects in
  // namespaces_ to node_.
  static std::unique_ptr<SerializerImpl> FromDataAndSerializeNamespaces(
      const std::unordered_map<string, xmlNsPtr>& namespaces, xmlNodePtr node);

  // Disallow copying.
  SerializerImpl(const SerializerImpl&) = delete;
  void operator=(const SerializerImpl&) = delete;

 private:
  // Writes the xmlNsPtr objects in namespaces_ to node_.
  // Modifies namespaces_ by setting each xmlNsPtr's next pointer to the
  // subsequent entry in the collection.
  bool SerializeNamespaces();

  xmlNodePtr node_;
  std::unordered_map<string, xmlNsPtr> namespaces_;
};

}  // namespace xml
}  // namespace xmpmeta
}  // namespace dynamic_depth

#endif // DYNAMIC_DEPTH_INTERNAL_XMPMETA_XML_SERIALIZER_IMPL_H_  // NOLINT
