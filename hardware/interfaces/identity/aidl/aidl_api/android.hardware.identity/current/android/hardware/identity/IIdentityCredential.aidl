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

package android.hardware.identity;
@VintfStability
interface IIdentityCredential {
  byte[] deleteCredential();
  byte[] createEphemeralKeyPair();
  void setReaderEphemeralPublicKey(in byte[] publicKey);
  long createAuthChallenge();
  void startRetrieval(in android.hardware.identity.SecureAccessControlProfile[] accessControlProfiles, in android.hardware.keymaster.HardwareAuthToken authToken, in byte[] itemsRequest, in byte[] signingKeyBlob, in byte[] sessionTranscript, in byte[] readerSignature, in int[] requestCounts);
  void startRetrieveEntryValue(in @utf8InCpp String nameSpace, in @utf8InCpp String name, in int entrySize, in int[] accessControlProfileIds);
  byte[] retrieveEntryValue(in byte[] encryptedContent);
  void finishRetrieval(out byte[] mac, out byte[] deviceNameSpaces);
  android.hardware.identity.Certificate generateSigningKeyPair(out byte[] signingKeyBlob);
  void setRequestedNamespaces(in android.hardware.identity.RequestNamespace[] requestNamespaces);
  void setVerificationToken(in android.hardware.keymaster.VerificationToken verificationToken);
}
