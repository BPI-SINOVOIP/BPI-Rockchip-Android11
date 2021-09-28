# V4L2-based Codec2 Component Implementation

## Description of Sub-folders

* accel/
Core V4L2 API and codec utilities, ported from Chromium project.

* common/
Common helper classes for both components/ and store/.

* components/
The C2Component implementations based on V4L2 API.

* store/
The implementation of C2ComponentStore. It is used for creating all the
C2Components implemented at components/ folder.

* service/
The Codec2's V4L2 IComponentStore service. The service initiates the component
store implemented at store/ folder, and registers it as the default service.
