## 7.3\. Sensors

If device implementations include a particular sensor type that has a
corresponding API for third-party developers, the device implementation
MUST implement that API as described in the Android SDK documentation and
the Android Open Source documentation on [sensors](
http://source.android.com/devices/sensors/).

Device implementations:

*   [C-0-1] MUST accurately report the presence or absence of sensors per the
[`android.content.pm.PackageManager`](
http://developer.android.com/reference/android/content/pm/PackageManager.html)
class.
*   [C-0-2] MUST return an accurate list of supported sensors via the
`SensorManager.getSensorList()` and similar methods.
*   [C-0-3] MUST behave reasonably for all other sensor APIs (for example, by
returning `true` or `false` as appropriate when applications attempt to register
listeners, not calling sensor listeners when the corresponding sensors are not
present; etc.).

If device implementations include a particular sensor type that has a
corresponding API for third-party developers, they:

*   [C-1-1] MUST [report all sensor measurements](
http://developer.android.com/reference/android/hardware/SensorEvent.html)
using the relevant International System of Units (metric) values for each
sensor type as defined in the Android SDK documentation.
*   [C-1-2] MUST report sensor data with a maximum latency of 100
milliseconds + 2 * sample_time for the case of a sensor stream with a
maximum requested latency of 0 ms when the application processor is active.
This delay does not include any filtering delays.
*   [C-1-3] MUST report the first sensor sample within 400 milliseconds + 2 *
sample_time of the sensor being activated. It is acceptable for this sample to
have an accuracy of 0.
*   [C-1-4] For any API indicated by the Android SDK documentation to be a
     [continuous sensor](
     https://source.android.com/devices/sensors/report-modes.html#continuous),
     device implementations MUST continuously provide
     periodic data samples that SHOULD have a jitter below 3%,
     where jitter is defined as the standard deviation of the difference of the
     reported timestamp values between consecutive events.
*   [C-1-5] MUST ensure that the sensor event stream
     MUST NOT prevent the device CPU from entering a suspend state or waking up
     from a suspend state.
*   [C-1-6] MUST [report the event time](
http://developer.android.com/reference/android/hardware/SensorEvent.html#timestamp)
in nanoseconds as defined in the Android SDK documentation, representing the
time the event happened and synchronized with the
SystemClock.elapsedRealtimeNano() clock.
*   [C-SR] Are STRONGLY RECOMMENDED to have timestamp synchronization error
below 100 milliseconds, and SHOULD have timestamp synchronization error below 1
millisecond.
*   When several sensors are activated, the power consumption SHOULD NOT exceed
     the sum of the individual sensor’s reported power consumption.

The list above is not comprehensive; the documented behavior of the Android SDK
and the Android Open Source Documentations on
[sensors](http://source.android.com/devices/sensors/) is to be considered
authoritative.


If device implementations include a particular sensor type that has a
corresponding API for third-party developers, they:

*   [C-1-6] MUST set a non-zero resolution for all sensors, and report the value
    via the [`Sensor.getResolution()`](https://developer.android.com/reference/android/hardware/Sensor#getResolution%28%29)
    API method.

Some sensor types are composite, meaning they can be derived from data provided
by one or more other sensors. (Examples include the orientation sensor and the
linear acceleration sensor.)

Device implementations:

*   SHOULD implement these sensor types, when they
include the prerequisite physical sensors as described
in [sensor types](https://source.android.com/devices/sensors/sensor-types.html).

If device implementations include a composite sensor, they:

*    [C-2-1] MUST implement the sensor as described in the Android Open Source
documentation on [composite sensors](
https://source.android.com/devices/sensors/sensor-types.html#composite_sensor_type_summary).


If device implementations include a particular sensor type that has a
corresponding API for third-party developers and the sensor only reports one
value, then device implementations:

*   [C-3-1] MUST set the resolution to 1 for the sensor and report the value
    via the [`Sensor.getResolution()`](https://developer.android.com/reference/android/hardware/Sensor#getResolution%28%29)
    API method.

If device implementations include a particular sensor type which supports
[SensorAdditionalInfo#TYPE_VEC3_CALIBRATION](https://developer.android.com/reference/android/hardware/SensorAdditionalInfo#TYPE_VEC3_CALIBRATION)
and the sensor is exposed to third-party developers, they:

*   [C-4-1] MUST NOT include any fixed, factory-determined calibration
    parameters in the data provided.

If device implementations include a combination of 3-axis accelerometer, a
3-axis gyroscope sensor, or a magnetometer sensor, they are:

*   [C-SR] STRONGLY RECOMMENDED to ensure the accelerometer, gyroscope and
    magnetometer have a fixed relative position, such that if the device is
    transformable (e.g. foldable), the sensor axes remain aligned and consistent
    with the sensor coordinate system throughout all possible device
    transformation states.

### 7.3.1\. Accelerometer

Device implementations:

*   [C-SR] Are STRONGLY RECOMMENDED to include a 3-axis accelerometer.

If device implementations include a 3-axis accelerometer, they:

*   [C-1-1] MUST be able to report events up to a frequency of at least 50 Hz.
*   [C-1-2] MUST implement and report [`TYPE_ACCELEROMETER`](
    http://developer.android.com/reference/android/hardware/Sensor.html#TYPE_ACCELEROMETER)
    sensor.
*   [C-1-3] MUST comply with the [Android sensor coordinate system](
http://developer.android.com/reference/android/hardware/SensorEvent.html)
as detailed in the Android APIs.
*   [C-1-4] MUST be capable of measuring from freefall up to four times the
gravity(4g) or more on any axis.
*   [C-1-5] MUST have a resolution of at least 12-bits.
*   [C-1-6] MUST have a standard deviation no greater than 0.05 m/s^, where
the standard deviation should be calculated on a per axis basis on samples
collected over a period of at least 3 seconds at the fastest sampling rate.
*   [SR] are **STRONGLY RECOMMENDED** to implement the `TYPE_SIGNIFICANT_MOTION`
    composite sensor.
*   [SR] are STRONGLY RECOMMENDED to implement and report [`TYPE_ACCELEROMETER_UNCALIBRATED`]
(https://developer.android.com/reference/android/hardware/Sensor.html#STRING_TYPE_ACCELEROMETER_UNCALIBRATED)
sensor. Android devices are STRONGLY RECOMMENDED to meet this requirement so
they will be able to upgrade to the future platform release where this might
become REQUIRED.
*   SHOULD implement the `TYPE_SIGNIFICANT_MOTION`, `TYPE_TILT_DETECTOR`,
`TYPE_STEP_DETECTOR`, `TYPE_STEP_COUNTER` composite sensors as described
in the Android SDK document.
*   SHOULD report events up to at least 200 Hz.
*   SHOULD have a resolution of at least 16-bits.
*   SHOULD be calibrated while in use if the characteristics changes over
the life cycle and compensated, and preserve the compensation parameters
between device reboots.
*   SHOULD be temperature compensated.

If device implementations include a 3-axis accelerometer and any of the
`TYPE_SIGNIFICANT_MOTION`, `TYPE_TILT_DETECTOR`, `TYPE_STEP_DETECTOR`,
`TYPE_STEP_COUNTER` composite sensors are implemented:

*   [C-2-1] The sum of their power consumption MUST always be less than 4 mW.
*   SHOULD each be below 2 mW and 0.5 mW for when the device is in a dynamic or
    static condition.

If device implementations include a 3-axis accelerometer and a 3-axis gyroscope sensor,
they:

*   [C-3-1] MUST implement the `TYPE_GRAVITY` and `TYPE_LINEAR_ACCELERATION`
composite sensors.
*   [C-SR] Are STRONGLY RECOMMENDED to implement the [`TYPE_GAME_ROTATION_VECTOR`](
https://developer.android.com/reference/android/hardware/Sensor.html#TYPE_GAME_ROTATION_VECTOR)
composite sensor.

If device implementations include a 3-axis accelerometer, a 3-axis gyroscope sensor,
and a magnetometer sensor, they:

*   [C-4-1] MUST implement a `TYPE_ROTATION_VECTOR` composite sensor.

### 7.3.2\. Magnetometer

Device implementations:

*   [C-SR] Are STRONGLY RECOMMENDED to include a 3-axis magnetometer (compass).

If device implementations include a 3-axis magnetometer, they:

*   [C-1-1] MUST implement the `TYPE_MAGNETIC_FIELD` sensor.
*   [C-1-2] MUST be able to report events up to a frequency of at least 10 Hz
and SHOULD report events up to at least 50 Hz.
*   [C-1-3] MUST comply with the [Android sensor coordinate system](
    http://developer.android.com/reference/android/hardware/SensorEvent.html)
    as detailed in the
    Android APIs.
*   [C-1-4] MUST be capable of measuring between -900 µT and +900 µT on each
axis before saturating.
*   [C-1-5] MUST have a hard iron offset value less than 700 µT and SHOULD have
a value below 200 µT, by placing the magnetometer far from
dynamic (current-induced) and static (magnet-induced) magnetic fields.
*   [C-1-6] MUST have a resolution equal or denser than 0.6 µT.
*   [C-1-7] MUST support online calibration and compensation of the hard iron
    bias, and preserve the compensation parameters between device reboots.
*   [C-1-8] MUST have the soft iron compensation applied—the calibration can be
done either while in use or during the production of the device.
*   [C-1-9] MUST have a standard deviation, calculated on a per axis basis on
samples collected over a period of at least 3 seconds at the fastest sampling
rate, no greater than 1.5 µT; SHOULD have a standard deviation no greater than
0.5 µT.
*   [C-SR] Are STRONGLY RECOMMENDED to implement
    [`TYPE_MAGNETIC_FIELD_UNCALIBRATED`](https://developer.android.com/reference/android/hardware/Sensor#STRING_TYPE_MAGNETIC_FIELD_UNCALIBRATED)
    sensor.

If device implementations include a 3-axis magnetometer, an accelerometer
sensor, and a 3-axis gyroscope sensor, they:

*   [C-2-1] MUST implement a `TYPE_ROTATION_VECTOR` composite sensor.

If device implementations include a 3-axis magnetometer, an accelerometer, they:

*   MAY implement the `TYPE_GEOMAGNETIC_ROTATION_VECTOR` sensor.

If device implementations include a 3-axis magnetometer, an accelerometer and
`TYPE_GEOMAGNETIC_ROTATION_VECTOR` sensor, they:

*   [C-3-1] MUST consume less than 10 mW.
*   SHOULD consume less than 3 mW when the sensor is registered for batch mode
at 10 Hz.

### 7.3.3\. GPS

Device implementations:

*   [C-SR] Are STRONGLY RECOMMENDED to include a GPS/GNSS receiver.

If device implementations include a GPS/GNSS receiver and report the capability
to applications through the `android.hardware.location.gps` feature flag, they:

*   [C-1-1] MUST support location outputs at a rate of at least 1 Hz when
requested via `LocationManager#requestLocationUpdate`.
*   [C-1-2] MUST be able to determine the location in open-sky conditions
    (strong signals, negligible multipath, HDOP < 2) within 10 seconds (fast
    time to first fix), when connected to a 0.5 Mbps or faster data speed
    internet connection. This requirement is typically met by the use of some
    form of Assisted or Predicted GPS/GNSS technique
    to minimize GPS/GNSS lock-on time (Assistance data includes Reference Time,
    Reference Location and Satellite Ephemeris/Clock).
       * [C-1-6] After making such a location calculation, device
         implementations MUST determine its location, in open sky, within
         5 seconds, when location requests are restarted, up to an hour after
         the initial location calculation, even when the subsequent request is
         made without a data connection, and/or after a power cycle.
*   In open sky conditions after determining the location, while stationary or
    moving with less than 1 meter per second squared of acceleration:

       * [C-1-3] MUST be able to determine location within 20 meters, and speed
         within 0.5 meters per second, at least 95% of the time.
       * [C-1-4] MUST simultaneously track and report via
       [`GnssStatus.Callback`](
       https://developer.android.com/reference/android/location/GnssStatus.Callback.html#GnssStatus.Callback()')
         at least 8 satellites from one constellation.
       * SHOULD be able to simultaneously track at least 24 satellites, from
       multiple constellations (e.g. GPS + at least one of Glonass, Beidou,
       Galileo).
*    [C-SR] Are STRONGLY RECOMMENDED to continue to deliver normal GPS/GNSS
location outputs through GNSS Location Provider API's during an emergency phone
call.
*    [C-SR] Are STRONGLY RECOMMENDED to report GNSS measurements from all
constellations tracked (as reported in GnssStatus messages), with the exception
of SBAS.
*    [C-SR] Are STRONGLY RECOMMENDED to report AGC, and Frequency of GNSS
measurement.
*    [C-SR] Are STRONGLY RECOMMENDED to report all accuracy estimates
(including Bearing, Speed, and Vertical) as part of each GPS/GNSS location.
*    [C-SR] Are STRONGLY RECOMMENDED to report GNSS measurements, as soon as
they are found, even if a location calculated from GPS/GNSS is not yet
reported.
*    [C-SR] Are STRONGLY RECOMMENDED to report GNSS pseudoranges and
pseudorange rates, that, in open-sky conditions after determining the location,
while stationary or moving with less than 0.2 meter per second squared of
acceleration, are sufficient to calculate position within 20 meters, and speed
within 0.2 meters per second, at least 95% of the time.


### 7.3.4\. Gyroscope

Device implementations:

*   [C-SR] Are STRONGLY RECOMMENDED to include a gyroscope sensor.

If device implementations include a 3-axis gyroscope, they:

*   [C-1-1] MUST be able to report events up to a frequency of at least 50 Hz.
*   [C-1-2] MUST implement the `TYPE_GYROSCOPE` sensor and are STRONGLY
    RECOMMENDED to also implement the
    [`TYPE_GYROSCOPE_UNCALIBRATED`](https://developer.android.com/reference/android/hardware/Sensor.html#TYPE_GYROSCOPE_UNCALIBRATED)
    sensor.
*   [C-1-4] MUST have a resolution of 12-bits or more and SHOULD have a
resolution of 16-bits or more.
*   [C-1-5] MUST be temperature compensated.
*   [C-1-6] MUST be calibrated and compensated while in use, and preserve the
    compensation parameters between device reboots.
*   [C-1-7] MUST have a variance no greater than 1e-7 rad^2 / s^2 per Hz
(variance per Hz, or rad^2 / s). The variance is allowed to vary with the
sampling rate, but MUST be constrained by this value. In other words, if you
measure the variance of the gyro at 1 Hz sampling rate it SHOULD be no greater
than 1e-7 rad^2/s^2.
*   [SR] Calibration error is STRONGLY RECOMMENDED to be less than 0.01 rad/s
when device is stationary at room temperature.
*   SHOULD report events up to at least 200 Hz.

If device implementations include a 3-axis gyroscope, an accelerometer sensor
and a magnetometer sensor, they:

*   [C-2-1] MUST implement a `TYPE_ROTATION_VECTOR` composite sensor.

If device implementations include a 3-axis accelerometer and a 3-axis gyroscope
sensor, they:

*   [C-3-1] MUST implement the `TYPE_GRAVITY` and
`TYPE_LINEAR_ACCELERATION` composite sensors.
*   [C-SR] Are STRONGLY RECOMMENDED to implement the
    [`TYPE_GAME_ROTATION_VECTOR`](https://developer.android.com/reference/android/hardware/Sensor.html#TYPE_GAME_ROTATION_VECTOR)
    composite sensor.

### 7.3.5\. Barometer

Device implementations:

*   [C-SR] Are STRONGLY RECOMMENDED to include a barometer (ambient air pressure
    sensor).

If device implementations include a barometer, they:

*   [C-1-1] MUST implement and report `TYPE_PRESSURE` sensor.
*   [C-1-2] MUST be able to deliver events at 5 Hz or greater.
*   [C-1-3] MUST be temperature compensated.
*   [SR] STRONGLY RECOMMENDED to be able to report pressure measurements in the
    range 300hPa to 1100hPa.
*   SHOULD have an absolute accuracy of 1hPa.
*   SHOULD have a relative accuracy of 0.12hPa over 20hPa range
    (equivalent to ~1m accuracy over ~200m change at sea level).

### 7.3.6\. Thermometer

If device implementations include an ambient thermometer (temperature sensor),
they:

*   [C-1-1] MUST define [`SENSOR_TYPE_AMBIENT_TEMPERATURE`](https://developer.android.com/reference/android/hardware/Sensor#TYPE_AMBIENT_TEMPERATURE)
    for the ambient temperature sensor and the sensor MUST measure the ambient
    (room/vehicle cabin) temperature from where the user is interacting with the
    device in degrees Celsius.

If device implementations include a thermometer sensor that measures a
temperature other than ambient temperature, such as CPU temperature, they:

*   [C-2-1] MUST NOT define [`SENSOR_TYPE_AMBIENT_TEMPERATURE`](https://developer.android.com/reference/android/hardware/Sensor#TYPE_AMBIENT_TEMPERATURE)
    for the temperature sensor.

### 7.3.7\. Photometer

*   Device implementations MAY include a photometer (ambient light sensor).

### 7.3.8\. Proximity Sensor

*   Device implementations MAY include a proximity sensor.

If device implementations include a proximity sensor, they:

*   [C-1-1] MUST measure the proximity of an object in the same direction as the
    screen. That is, the proximity sensor MUST be oriented to detect objects
    close to the screen, as the primary intent of this sensor type is to
    detect a phone in use by the user. If device implementations include a
    proximity sensor with any other orientation, it MUST NOT be accessible
    through this API.
*   [C-1-2] MUST have 1-bit of accuracy or more.


### 7.3.9\. High Fidelity Sensors

If device implementations include a set of higher quality sensors as defined
in this section, and make available them to third-party apps, they:

*   [C-1-1] MUST identify the capability through the
`android.hardware.sensor.hifi_sensors` feature flag.

If device implementations declare `android.hardware.sensor.hifi_sensors`,
they:

*   [C-2-1] MUST have a `TYPE_ACCELEROMETER` sensor which:
    *   MUST have a measurement range between at least -8g and +8g, and is
        STRONGLY RECOMMENDED to have a measurement range between at least -16g
        and +16g.
    *   MUST have a measurement resolution of at least 2048 LSB/g.
    *   MUST have a minimum measurement frequency of 12.5 Hz or lower.
    *   MUST have a maximum measurement frequency of 400 Hz or higher; SHOULD
        support the SensorDirectChannel [`RATE_VERY_FAST`](
        https://developer.android.com/reference/android/hardware/SensorDirectChannel.html#RATE_VERY_FAST).
    *   MUST have a measurement noise not above 400 μg/√Hz.
    *   MUST implement a non-wake-up form of this sensor with a buffering
        capability of at least 3000 sensor events.
    *   MUST have a batching power consumption not worse than 3 mW.
    *   [C-SR] Is STRONGLY RECOMMENDED to have 3dB measurement bandwidth of at
        least 80% of Nyquist frequency, and white noise spectrum within this
        bandwidth.
    *   SHOULD have an acceleration random walk less than 30 μg √Hz tested at
        room temperature.
    *   SHOULD have a bias change vs. temperature of ≤ +/- 1 mg/°C.
    *   SHOULD have a best-fit line non-linearity of ≤ 0.5%, and sensitivity change vs. temperature of ≤
        0.03%/C°.
    *   SHOULD have cross-axis sensitivity of < 2.5 % and variation of
        cross-axis sensitivity < 0.2% in device operation temperature range.

*   [C-2-2] MUST have a `TYPE_ACCELEROMETER_UNCALIBRATED` with the same
quality requirements as `TYPE_ACCELEROMETER`.

*   [C-2-3] MUST have a `TYPE_GYROSCOPE` sensor which:
    *   MUST have a measurement range between at least -1000 and +1000 dps.
    *   MUST have a measurement resolution of at least 16 LSB/dps.
    *   MUST have a minimum measurement frequency of 12.5 Hz or lower.
    *   MUST have a maximum measurement frequency of 400 Hz or higher; SHOULD
        support the SensorDirectChannel [`RATE_VERY_FAST`](
        https://developer.android.com/reference/android/hardware/SensorDirectChannel.html#RATE_VERY_FAST).
    *   MUST have a measurement noise not above 0.014°/s/√Hz.
    *   [C-SR] Is STRONGLY RECOMMENDED to have 3dB measurement bandwidth of at
        least 80% of Nyquist frequency, and white noise spectrum within this
        bandwidth.
    *   SHOULD have a rate random walk less than 0.001 °/s √Hz tested at room
        temperature.
    *   SHOULD have a bias change vs. temperature of ≤ +/- 0.05 °/ s / °C.
    *   SHOULD have a sensitivity change vs. temperature of ≤ 0.02% / °C.
    *   SHOULD have a best-fit line non-linearity of ≤ 0.2%.
    *   SHOULD have a noise density of ≤ 0.007 °/s/√Hz.
    *   SHOULD have calibration error less than 0.002 rad/s in
        temperature range 10 ~ 40 ℃ when device is stationary.
    *   SHOULD have g-sensitivity less than 0.1°/s/g.
    *   SHOULD have cross-axis sensitivity of < 4.0 % and cross-axis sensitivity
        variation < 0.3% in device operation temperature range.
*   [C-2-4] MUST have a `TYPE_GYROSCOPE_UNCALIBRATED` with the same quality
requirements as `TYPE_GYROSCOPE`.

*   [C-2-5] MUST have a `TYPE_GEOMAGNETIC_FIELD` sensor which:
    *   MUST have a measurement range between at least -900 and +900 μT.
    *   MUST have a measurement resolution of at least 5 LSB/uT.
    *   MUST have a minimum measurement frequency of 5 Hz or lower.
    *   MUST have a maximum measurement frequency of 50 Hz or higher.
    *   MUST have a measurement noise not above 0.5 uT.
*   [C-2-6] MUST have a `TYPE_MAGNETIC_FIELD_UNCALIBRATED` with the same quality
requirements as `TYPE_GEOMAGNETIC_FIELD` and in addition:
    *   MUST implement a non-wake-up form of this sensor with a buffering
        capability of at least 600 sensor events.
    *   [C-SR] Is STRONGLY RECOMMENDED to have white noise spectrum from 1 Hz to
        at least 10 Hz when the report rate is 50 Hz or higher.

*   [C-2-7] MUST have a `TYPE_PRESSURE` sensor which:
    *   MUST have a measurement range between at least 300 and 1100 hPa.
    *   MUST have a measurement resolution of at least 80 LSB/hPa.
    *   MUST have a minimum measurement frequency of 1 Hz or lower.
    *   MUST have a maximum measurement frequency of 10 Hz or higher.
    *   MUST have a measurement noise not above 2 Pa/√Hz.
    *   MUST implement a non-wake-up form of this sensor with a buffering
        capability of at least 300 sensor events.
    *   MUST have a batching power consumption not worse than 2 mW.
*   [C-2-8] MUST have a `TYPE_GAME_ROTATION_VECTOR` sensor.
*   [C-2-9] MUST have a `TYPE_SIGNIFICANT_MOTION` sensor which:
    *   MUST have a power consumption not worse than 0.5 mW when device is
        static and 1.5 mW when device is moving.
*   [C-2-10] MUST have a `TYPE_STEP_DETECTOR` sensor which:
    *   MUST implement a non-wake-up form of this sensor with a buffering
        capability of at least 100 sensor events.
    *   MUST have a power consumption not worse than 0.5 mW when device is
        static and 1.5 mW when device is moving.
    *   MUST have a batching power consumption not worse than 4 mW.
*   [C-2-11] MUST have a `TYPE_STEP_COUNTER` sensor which:
    *   MUST have a power consumption not worse than 0.5 mW when device is
        static and 1.5 mW when device is moving.
*   [C-2-12] MUST have a `TILT_DETECTOR` sensor which:
    *   MUST have a power consumption not worse than 0.5 mW when device is
        static and 1.5 mW when device is moving.
*   [C-2-13] The event timestamp of the same physical event reported by the
Accelerometer, Gyroscope, and Magnetometer MUST be within 2.5 milliseconds
of each other. The event timestamp of the same physical event reported by
the Accelerometer and Gyroscope SHOULD be within 0.25 milliseconds of each
other.
*   [C-2-14] MUST have Gyroscope sensor event timestamps on the same time
base as the camera subsystem and within 1 milliseconds of error.
*   [C-2-15] MUST deliver samples to applications within 5 milliseconds from
the time when the data is available on any of the above physical sensors
to the application.
*   [C-2-16] MUST NOT have a power consumption higher than 0.5 mW
when device is static and 2.0 mW when device is moving
when any combination of the following sensors are enabled:
    *   `SENSOR_TYPE_SIGNIFICANT_MOTION`
    *   `SENSOR_TYPE_STEP_DETECTOR`
    *   `SENSOR_TYPE_STEP_COUNTER`
    *   `SENSOR_TILT_DETECTORS`
*   [C-2-17] MAY have a `TYPE_PROXIMITY` sensor, but if present MUST have
a minimum buffer capability of 100 sensor events.

Note that all power consumption requirements in this section do not include the
power consumption of the Application Processor. It is inclusive of the power
drawn by the entire sensor chain—the sensor, any supporting circuitry, any
dedicated sensor processing system, etc.

If device implementations include direct sensor support, they:

* [C-3-1] MUST correctly declare support of direct channel types and direct
  report rates level through the [`isDirectChannelTypeSupported`](
  https://developer.android.com/reference/android/hardware/Sensor.html#isDirectChannelTypeSupported%28int%29)
  and [`getHighestDirectReportRateLevel`](
  https://developer.android.com/reference/android/hardware/Sensor.html#getHighestDirectReportRateLevel%28%29)
  API.
* [C-3-2] MUST support at least one of the two sensor direct channel types
  for all sensors that declare support for sensor direct channel.
    *   [`TYPE_HARDWARE_BUFFER`](https://developer.android.com/reference/android/hardware/SensorDirectChannel.html#TYPE_HARDWARE_BUFFER)
    *   [`TYPE_MEMORY_FILE`](https://developer.android.com/reference/android/hardware/SensorDirectChannel.html#TYPE_MEMORY_FILE)
* SHOULD support event reporting through sensor direct channel for primary
  sensor (non-wakeup variant) of the following types:
     *   `TYPE_ACCELEROMETER`
     *   `TYPE_ACCELEROMETER_UNCALIBRATED`
     *   `TYPE_GYROSCOPE`
     *   `TYPE_GYROSCOPE_UNCALIBRATED`
     *   `TYPE_MAGNETIC_FIELD`
     *   `TYPE_MAGNETIC_FIELD_UNCALIBRATED`

### 7.3.10\. Biometric Sensors


For additional background on Measuring Biometric Unlock Security, please see
[Measuring Biometric Security documentation](https://source.android.com/security/biometric/measure).

If device implementations include a secure lock screen, they:

*   SHOULD include a biometric sensor

Biometric sensors can be classified as **Class 3** (formerly **Strong**),
**Class 2** (formerly **Weak**), or **Class 1** (formerly **Convenience**)
based on their spoof and imposter acceptance rates, and on the security of the
biometric pipeline. This classification determines the capabilities the
biometric sensor has to interface with the platform and with third-party
applications. Sensors are classified as **Class 1** by default, and need
to meet additional requirements as detailed below if they wish to be classified
as either **Class 2** or **Class 3**. Both **Class 2** and **Class 3**
biometrics get additional capabilities as detailed below.

If device implementations make a biometric sensor available to third-party
applications via [android.hardware.biometrics.BiometricManager](https://developer.android.com/reference/android/hardware/biometrics/BiometricManager),
[android.hardware.biometrics.BiometricPrompt](https://developer.android.com/reference/android/hardware/biometrics/BiometricPrompt),
and [android.provider.Settings.ACTION_BIOMETRIC_ENROLL](https://developer.android.com/reference/android/provider/Settings#ACTION_BIOMETRIC_ENROLL),
they:

*   [C-4-1] MUST meet the requirements for **Class 3** or **Class 2** biometric
    as defined in this document.
*   [C-4-2] MUST recognize and honor each parameter name defined as a constant
    in the [Authenticators](https://developer.android.com/reference/android/hardware/biometrics/BiometricManager.Authenticators)
    class and any combinations thereof.
    Conversely, MUST NOT honor or recognize integer constants passed to the
    [canAuthenticate(int)](https://developer.android.com/reference/android/hardware/biometrics/BiometricManager#canAuthenticate%28int%29)
    and [setAllowedAuthenticators(int)](https://developer.android.com/reference/android/hardware/biometrics/BiometricPrompt.Builder#setAllowedAuthenticators%28int%29)
    methods other than those documented as public constants in
    [Authenticators](https://developer.android.com/reference/android/hardware/biometrics/BiometricManager.Authenticators)
    and any combinations thereof.
*   [C-4-3] MUST implement the [ACTION_BIOMETRIC_ENROLL](https://developer.android.com/reference/android/provider/Settings#ACTION_BIOMETRIC_ENROLL)
    action on devices that have either **Class 3** or **Class 2** biometrics.
    This action MUST only present the enrollment entry points for **Class 3**
    or **Class 2** biometrics.

If device implementations support passive biometrics, they:

*   [C-5-1] MUST by default require an additional confirmation step
    (e.g. a button press).
*   [C-SR] Are STRONGLY RECOMMENDED to have a setting to allow users to
    override application preference and always require accompanying
    confirmation step.
*   [C-SR] Are STRONGLY RECOMMENDED to have the confirm action be secured
    such that an operating system or kernel compromise cannot spoof it.
    For example, this means that the confirm action based on a physical button
    is routed through an input-only general-purpose input/output (GPIO) pin of
    a secure element (SE) that cannot be driven by any other means than a
    physical button press.
*   [C-5-2] MUST additionally implement an implicit authentication flow
    (without confirmation step) corresponding to
    [setConfirmationRequired(boolean)](https://developer.android.com/reference/android/hardware/biometrics/BiometricPrompt.Builder#setConfirmationRequired%28boolean%29),
    which applications can set to utilize for sign-in flows.

If device implementations have multiple biometric sensors, they:

*   [C-SR] Are STRONGLY RECOMMENDED to require only one biometric be confirmed
    per authentication (e.g. if both fingerprint and face sensors are available
    on the device, [onAuthenticationSucceeded](https://developer.android.com/reference/android/hardware/biometrics/BiometricPrompt.AuthenticationCallback.html#onAuthenticationSucceeded%28android.hardware.biometrics.BiometricPrompt.AuthenticationResult%29)
    should be sent after any one of them is confirmed).

In order for device implementations to allow access to keystore keys to
third-party applications, they:

*   [C-6-1] MUST meet the requirements for **Class 3** as defined in this
    section below.
*   [C-6-2] MUST present only **Class 3** biometrics when the authentication
    requires [BIOMETRIC_STRONG](https://developer.android.com/reference/android/hardware/biometrics/BiometricManager.Authenticators#BIOMETRIC_STRONG),
    or the authentication is invoked with a
    [CryptoObject](https://developer.android.com/reference/android/hardware/biometrics/BiometricPrompt.CryptoObject).

If device implementations wish to treat a biometric sensor as **Class 1**
(formerly **Convenience**), they:

*   [C-1-1] MUST have a false acceptance rate less than 0.002%.
*   [C-1-2] MUST disclose that this mode may be less secure than a strong PIN,
    pattern, or password and clearly enumerate the risks of enabling it, if the
    spoof and imposter acceptance rates are higher than 7% as measured by the
    [Android Biometrics Test Protocols](https://source.android.com/security/biometric/measure).
*   [C-1-3] MUST rate limit attempts for at least 30 seconds after five false
    trials for biometric verification - where a false trial is one with an
    adequate capture quality ([`BIOMETRIC_ACQUIRED_GOOD`](
    https://developer.android.com/reference/android/hardware/biometrics/BiometricPrompt.html#BIOMETRIC_ACQUIRED_GOOD))
    that does not match an enrolled biometric.
*   [C-1-4] MUST prevent adding new biometrics without first establishing a
    chain of trust by having the user confirm existing or add a new device
    credential (PIN/pattern/password) that's secured by TEE; the Android Open
    Source Project implementation provides the mechanism in the framework to do
    so.
*   [C-1-5] MUST completely remove all identifiable biometric data for a user
    when the user's account is removed (including via a factory reset).
*   [C-1-6] MUST honor the individual flag for that biometric (i.e.
    [`DevicePolicyManager.KEYGUARD_DISABLE_FINGERPRINT`](
    https://developer.android.com/reference/android/app/admin/DevicePolicyManager.html#KEYGUARD_DISABLE_FINGERPRINT),
    [`DevicePolicymanager.KEYGUARD_DISABLE_FACE`](
    https://developer.android.com/reference/android/app/admin/DevicePolicyManager.html#KEYGUARD_DISABLE_FACE)
    , or
    [`DevicePolicymanager.KEYGUARD_DISABLE_IRIS`](
    https://developer.android.com/reference/android/app/admin/DevicePolicyManager.html#KEYGUARD_DISABLE_IRIS)
    ).
*   [C-1-7] MUST challenge the user for the recommended primary
    authentication (e.g. PIN, pattern, password) once every 24 hours
    or less for new devices launching with Android version 10, once every
    72 hours or less for devices upgrading from earlier Android version.
*   [C-1-8] MUST challenge the user for the recommended primary
     authentication (eg: PIN, pattern, password) after one of the
     following:
     *    a 4-hour idle timeout period, OR
     *    3 failed biometric authentication attempts.
     *    The idle timeout period and the failed authentication count is reset
          after any successful confirmation of the device credentials.
*   [C-SR] Are STRONGLY RECOMMENDED to use the logic in the framework provided
    by the Android Open Source Project to enforce constraints specified in
    [C-1-7] and [C-1-8] for new devices.
*   [C-SR] Are STRONGLY RECOMMENDED to have a false rejection rate of less than
    10%, as measured on the device.
*   [C-SR] Are STRONGLY RECOMMENDED to have a latency below 1 second, measured
    from when the biometric is detected, until the screen is unlocked, for each
    enrolled biometric.

If device implementations wish to treat a biometric sensor as **Class 2**
(formerly **Weak**), they:

*   [C-2-1] MUST meet all requirements for **Class 1** above.
*   [C-2-2] MUST have a spoof and imposter acceptance rate not higher than 20%
    as measured by the [Android Biometrics Test Protocols](https://source.android.com/security/biometric/measure).
*   [C-2-3] MUST perform the
    biometric matching in an isolated execution environment outside Android
    user or kernel space, such as the Trusted Execution Environment (TEE), or
    on a chip with a secure channel to the isolated execution environment.
*   [C-2-4] MUST have all identifiable data encrypted and cryptographically
    authenticated such that they cannot be acquired, read or altered outside of
    the isolated execution environment or a chip with a secure channel to the
    isolated execution environment as documented in the [implementation
    guidelines](https://source.android.com/security/biometric#hal-implementation)
    on the Android Open Source Project site.
*   [C-2-5] For camera based biometrics, while biometric based authentication
    or enrollment is happening:
    *    MUST operate the camera in a mode that prevents camera frames from
    being read or altered outside the isolated execution environment or a chip
    with a secure channel to the isolated execution environment.
    *    For RGB single-camera solutions, the camera frames CAN be
    readable outside the isolated execution environment to support operations
    such as preview for enrollment, but MUST still NOT be alterable.
*   [C-2-6] MUST NOT enable third-party applications to distinguish between
    individual biometric enrollments.
*   [C-2-7] MUST NOT allow unencrypted access to identifiable
    biometric data or any data derived from it (such as embeddings) to
    the Application Processor outside the context of the TEE.
*   [C-2-8] MUST have a secure processing pipeline such that an operating
    system or kernel compromise cannot allow data to be directly injected to
    falsely authenticate as the user.

    If device implementations are already launched on an earlier Android
    version and cannot meet the requirement C-2-8 through a system software
    update, they MAY be exempted from the requirement.

*   [C-SR] Are STRONGLY RECOMMENDED to include liveness detection for all
    biometric modalities and attention detection for Face biometrics.

If device implementations wish to treat a biometric sensor as **Class 3**
(formerly **Strong**), they:

*   [C-3-1] MUST meet all the requirements of **Class 2** above, except for
    [C-1-7] and [C-1-8]. Upgrading devices from an earlier Android version
    are not exempted from C-2-7.
*   [C-3-2] MUST have a hardware-backed keystore implementation.
*   [C-3-3] MUST have a spoof and imposter acceptance rate not higher than 7%
    as measured by the [Android Biometrics Test Protocols](https://source.android.com/security/biometric/measure).
*   [C-3-4] MUST challenge the user for the recommended primary
    authentication (e.g. PIN, pattern, password) once every 72 hours
    or less.

### 7.3.12\. Pose Sensor

Device implementations:

*   MAY support pose sensor with 6 degrees of freedom.

If device implementations support pose sensor with 6 degrees of freedom, they:

*   [C-1-1] MUST implement and report [`TYPE_POSE_6DOF`](
https://developer.android.com/reference/android/hardware/Sensor.html#TYPE_POSE_6DOF)
sensor.
*   [C-1-2] MUST be more accurate than the rotation vector alone.

### 7.3.13\. Hinge Angle Sensor

If device implementations support a hinge angle sensor, they:

*   [C-1-1] MUST implement and report [`TYPE_HINGLE_ANGLE`](https://developer.android.com/reference/android/hardware/Sensor#STRING_TYPE_HINGE_ANGLE).
*   [C-1-2] MUST support at least two readings between 0 and 360 degrees
    (inclusive i.e including 0 and 360 degrees).
*   [C-1-3] MUST return a [wakeup](https://developer.android.com/reference/android/hardware/Sensor.html#isWakeUpSensor%28%29)
    sensor for [`getDefaultSensor(SENSOR_TYPE_HINGE_ANGLE)`](https://developer.android.com/reference/android/hardware/SensorManager#getDefaultSensor%28int%29).