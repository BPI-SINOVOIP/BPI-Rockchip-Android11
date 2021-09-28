This file roughly describes the files contained in the directory.

camera3_profiles.xml

  This xml file is used to define the cameras which the board has.
  The main sections in this file are detailed as follow:
  -- section "profiles"
     Each camera connected to the board is described by one "profiles"
     section. Please notice that the sequence of description for cameras
     should be fixed now. And we should decide the order by this way:
     using the command "media-ctl -p" to get the entitis graph, then the
     the order of sensor subdev showing up is the sequence for the "profiles".
     We will remove this limitation in future.
  -- section "Android_metadata"
     Defines the camera system infos and capability. You could refer to
     http://newandroidbook.com/code/android-6.0.0_r1/system/media/camera/docs/docs.html
     for details. And We will also details this in other specification docs in future.  
     To add this section for a new camera, compared to the default xml, you
     just concern about the following items:
     <control.aeAvailableTargetFpsRanges> /* define the support fps */
     <scaler.availableStreamConfigurations> /* define the available resolution combines*/
     <scaler.availableMinFrameDurations> /* define min frame durations, realted to fps */
     <scaler.availableStallDurations>
     <sensor.info.activeArraySize> /* often equal to the full sensor output size */
     <sensor.info.pixelArraySize>  /* often equal to the full sensor output size */
     <sensor.orientation>
  -- section "Hal_tuning_RKISP1"
     <graphSettingsFile> /* specify the camera graph settings xml file */
     <iqTuningFile> /* specify the iq xml if RAW sensor is used */
     <forceAutoGenAndroidMetas> /* enable auto generate mode or not */
  -- section "Sensor_info_RKISP1"
     <sensorType> /* value is SENSOR_TYPE_SOC or SENSOR_TYPE_RAW */
  -- section "MediaCtl_elements_RKISP1"
     describe media system entities, name infos in each element could be retrieved by
     media-ctl tool. The sample items are as follow:
     <element name="gc2145 0-003c" type="pixel_array"/> /* sensor entity */
     <element name="rockchip-mipi-dphy-rx" type="csi_receiver"/> /* mipi phy, may be absent */
     <element name="stream_cif" type="isys_backend"/> /* CIF controller if sensor connected to CIF */
     <element name="rkisp1-isp-subdev" type="isys_backend"/> /* ISP controller if sensor connected to ISP */
graph_settings_<sensorname>.xml
  The file is generated automatically by tools/gen_sensor_graph_setting.py. It is used to select
  the proper pipeline configuration for user various reuqired streams combinations. As an example,
  you could generate graph_settings_gc2145.xml by:
  ./gen_sensor_graph_settings.py --name=gc2145 --bayer_order=UYVY --sensor_fmt=UYVY --width=1600 \
  --height=1200 --binner_width=800 --binner_height=600

graph_descriptor.xml
  This file describles the camera system pipeline, and do not edit it.

rkisp1/
  Store iq tuning files which comes from iq team for RAW sensors.
