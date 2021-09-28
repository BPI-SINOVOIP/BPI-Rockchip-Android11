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
interface INetworkMonitorCallbacks {
  oneway void onNetworkMonitorCreated(in android.net.INetworkMonitor networkMonitor) = 0;
  oneway void notifyNetworkTested(int testResult, @nullable String redirectUrl) = 1;
  oneway void notifyPrivateDnsConfigResolved(in android.net.PrivateDnsConfigParcel config) = 2;
  oneway void showProvisioningNotification(String action, String packageName) = 3;
  oneway void hideProvisioningNotification() = 4;
  oneway void notifyProbeStatusChanged(int probesCompleted, int probesSucceeded) = 5;
  oneway void notifyNetworkTestedWithExtras(in android.net.NetworkTestResultParcelable result) = 6;
  oneway void notifyDataStallSuspected(in android.net.DataStallReportParcelable report) = 7;
  oneway void notifyCaptivePortalDataChanged(in android.net.CaptivePortalData data) = 8;
}
