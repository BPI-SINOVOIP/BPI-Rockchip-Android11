# Codec2.0 End-to-end Test

## Manually run the test

You need to be running Android with v4l2_codec2 (e.g. ARC++, ARCVM) to run the
tests. Make sure the device under test (DUT) is connected with adb.

1.  Build the e2e test

    ```
    (From the android tree)
    $ mmm external/v4l2_codec2/tests/c2_e2e_test
    ```

2.  Install the test app and grant the necessary permissions

    ```
    (Outside CrOS chroot)
    $ adb install \
      out/target/product/bertha_x86_64/testcases/C2E2ETest/x86_64/C2E2ETest.apk
    $ adb shell pm grant \
      org.chromium.c2.test android.permission.WRITE_EXTERNAL_STORAGE
    $ adb shell pm grant \
      org.chromium.c2.test android.permission.READ_EXTERNAL_STORAGE
    ```

3.  Run E2E tests

    (1) Push the test stream and frame-wise MD5 file to DUT

    ```
    $ adb push test-25fps.h264 /sdcard/Download
    $ adb push test-25fps.h264.frames.md5 /sdcard/Download
    ```

    (2) Run the test binary

    ```
    $ adb shell am start -n \
        org.chromium.c2.test/.E2eTestActivity \
        --ez do-encode [true|false] \
        --esa test-args [test-arg]... \
        --es log-file "/sdcard/Download/gtest_logs.txt"'
    ```

    where do-encode controls whether the encoder or decoder tests are run.

    (3) View the test output

    ```
    $ adb shell cat /sdcard/Download/gtest_logs.txt
    ```

Required test-args:

    test_video_data=<format> : see arc_accel_video.go in the chromeos tree for format

Optional test-args:

    output_frames_path=path : path at which to save decoded frame data
    loop : if present, videos loop until the activity is signaled with a new intent (e.g.
           `adb shell am start -n .../.E2eTestActivity --activity-single-top`)
    use_sw_decoder : if present, use a software decoder instead of a hardware decoder
    gtest arguments : see gtest documentation

Example of test-args:

    --esa test-args "--test_video_data=/sdcard/Download/test-25fps.h264:320:240:250:258:::1:25",\
                    "--gtest_filter=C2VideoDecoderSurfaceE2ETest.TestFPS",--use_sw_decoder
