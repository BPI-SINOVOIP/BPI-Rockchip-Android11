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

package android.automotive.watchdog;
@VintfStability
interface ICarWatchdog {
  void registerClient(in android.automotive.watchdog.ICarWatchdogClient client, in android.automotive.watchdog.TimeoutLength timeout);
  void unregisterClient(in android.automotive.watchdog.ICarWatchdogClient client);
  void registerMediator(in android.automotive.watchdog.ICarWatchdogClient mediator);
  void unregisterMediator(in android.automotive.watchdog.ICarWatchdogClient mediator);
  void registerMonitor(in android.automotive.watchdog.ICarWatchdogMonitor monitor);
  void unregisterMonitor(in android.automotive.watchdog.ICarWatchdogMonitor monitor);
  void tellClientAlive(in android.automotive.watchdog.ICarWatchdogClient client, in int sessionId);
  void tellMediatorAlive(in android.automotive.watchdog.ICarWatchdogClient mediator, in int[] clientsNotResponding, in int sessionId);
  void tellDumpFinished(in android.automotive.watchdog.ICarWatchdogMonitor monitor, in int pid);
  void notifySystemStateChange(in android.automotive.watchdog.StateType type, in int arg1, in int arg2);
}
