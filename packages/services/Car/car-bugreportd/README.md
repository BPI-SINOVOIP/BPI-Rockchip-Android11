# car-bugreportd

Android Automotive OS only service. Please use `CarBugreportManager` API.

It takes bugreport, appends car specific files and then proxies them to
`CarBugreportManagerService`.

To start, set the value of the system property `ctl.start` to `car-bugreportd`,
e.g. `SystemProperties.set("ctl.start", "car-bugreportd");`
