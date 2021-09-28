# Treble build tools

Tools for building the Android Platform.

[TOC]

## Testing

To run all project tests:

```
./test.sh
```

## Filesystem overlays

Downstream consumers of the Android platform often create independent Android
branches for each device.

We can unify the development of all devices on the same Android Platform dessert
with the help of filesystem overlays.

For example, let's say we had two devices: Device A and Device B.
The filesystem overlays would create the following build time views.

```
+-------------------------------+ +--------------------------------------------+
|         Android repo          | |                Android repo                |
|          workspace            | |                 workspace                  |
|                               | |                                            |
| +--------------+              | | +----------------------------------------+ |
| | +----------+ | +----------+ | | | +----------+  +----------+             | |
| | | Platform | | | Device B | | | | | Platform |  | Device B |   Device B  | |
| | | projects | | | projects | | | | | projects |  | projects |  build view | |
| | +----------+ | +----------+ | | | +----------+  +----------+             | |
| |              |              | | +----------------------------------------+ |
| | +----------+ |              | |   +----------+                             |
| | | Device A | |              | |   | Device A |                             |
| | | projects | |              | |   | projects |                             |
| | +----------+ |              | |   +----------+                             |
| |              |              | +--------------------------------------------+
| |   Device A   |              |
| |  build view  |              |
| |              |              |
| +--------------+              |
+-------------------------------+
```

To support filesystem overlays the Android repo workspace is required to the
following structure.

### Root directory

Location: `${ANDROID_BUILD_TOP}`

All projects in the root directory that are not in the overlays
directory are shared among all Android targets.

### Overlays directory

Location: `${ANDROID_BUILD_TOP}/overlays`

Contains target specific projects. Each subdirectory under the overlays
directory can be mounted at the root directory to support different targets.

### Build out directory

Location: `${ANDROID_BUILD_TOP}/out`

Contains all files generated during a build. This includes target files
like system.img and host tools like adb.
