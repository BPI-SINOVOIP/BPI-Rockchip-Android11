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

package android.automotive.computepipe.runner;
@VintfStability
interface IPipeRunner {
  void init(in android.automotive.computepipe.runner.IPipeStateCallback statecb);
  android.automotive.computepipe.runner.PipeDescriptor getPipeDescriptor();
  void setPipeInputSource(in int configId);
  void setPipeOffloadOptions(in int configId);
  void setPipeTermination(in int configId);
  void setPipeOutputConfig(in int configId, in int maxInFlightCount, in android.automotive.computepipe.runner.IPipeStream handler);
  void applyPipeConfigs();
  void resetPipeConfigs();
  void startPipe();
  void stopPipe();
  void doneWithPacket(in int bufferId, in int streamId);
  android.automotive.computepipe.runner.IPipeDebugger getPipeDebugger();
  void releaseRunner();
}
