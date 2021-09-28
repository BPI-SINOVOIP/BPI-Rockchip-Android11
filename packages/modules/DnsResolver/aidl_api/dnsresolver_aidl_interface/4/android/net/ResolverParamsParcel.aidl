///////////////////////////////////////////////////////////////////////////////
// THIS FILE IS IMMUTABLE. DO NOT EDIT IN ANY CASE.                          //
///////////////////////////////////////////////////////////////////////////////

// This file is a snapshot of an AIDL interface (or parcelable). Do not try to
// edit this file. It looks like you are doing that because you have modified
// an AIDL interface in a backward-incompatible way, e.g., deleting a function
// from an interface or a field from a parcelable and it broke the build. That
// breakage is intended.
//
// You must not make a backward incompatible changes to the AIDL files built
// with the aidl_interface module type with versions property set. The module
// type is used to build AIDL files in a way that they can be used across
// independently updatable components of the system. If a device is shipped
// with such a backward incompatible change, it has a high risk of breaking
// later when a module using the interface is updated, e.g., Mainline modules.

package android.net;
/* @hide */
parcelable ResolverParamsParcel {
  int netId;
  int sampleValiditySeconds;
  int successThreshold;
  int minSamples;
  int maxSamples;
  int baseTimeoutMsec;
  int retryCount;
  @utf8InCpp String[] servers;
  @utf8InCpp String[] domains;
  @utf8InCpp String tlsName;
  @utf8InCpp String[] tlsServers;
  @utf8InCpp String[] tlsFingerprints = {};
  @utf8InCpp String caCertificate = "";
  int tlsConnectTimeoutMs = 0;
  android.net.ResolverOptionsParcel resolverOptions;
  int[] transportTypes = {};
}
