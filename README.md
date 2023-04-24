# BPI-Rockchip-Android11

----------

**Prepare**

Get the docker image from [Sinovoip Docker Hub](https://hub.docker.com/r/sinovoip/bpi-build-android-11/) , Build the android source with this docker environment.

Other environment requirements, please refer to google Android's official build environment [requirements](https://source.android.com/setup/build/requirements) and [set up guide](https://source.android.com/setup/build/initializing) 

----------

Get source code

    $ git clone https://github.com/Dangku/BPI-Rockchip-Android11 --depth=1

Because github limit 100MB size for single file, please download the [oversize files](https://download.banana-pi.dev/d/ca025d76afd448aabc63/files/?p=%2FSource_Code%2Fr2pro%2Fgithub_oversize_files.zip) and merge them to correct directory before build.

Another way is get the source code tar archive from [BaiduPan(pincode: 8888)](https://pan.baidu.com/s/1c2vw-df4hh55VB3gSsM6Uw?pwd=8888) or [GoogleDrive](https://drive.google.com/drive/folders/1_DkE_6dsTQ-HZoEDGdvFsYtf5_ARQXoh?usp=share_link)

----------

**Build**

    $ cd BPI-Rockchip-Android11
    $ source build/envsetup.sh
    $ lunch
    
    You're building on Linux

    Lunch menu... pick a combo:

    ...
    16. bananapi_r2pro-user
    17. bananapi_r2pro-userdebug
    18. bananapi_r2pro_box-user
    19. bananapi_r2pro_box-userdebug
    ...
    ---------------------------------------------------------------
    Which would you like? [aosp_arm-eng] 17
    ...

Target bananapi_r2pro is a tablet type UI, bananapi_r2pro_box is a box type UI and only support hdmi display.

If bananapi_r2pro target choosen , then

    $ ./build.sh -AUCKu

Or bananapi_r2pro_box target choosen, then

    $ ./build.sh -AUCKu -d rk3568-bananapi-r2pro-hdmi
----------
**Flash**

The target  download image is rockdev/Image-bananapi_r2pro*/update.img, flash it to your device by Rockchip USB Download Tool. Please refer to [Bananapi R2Pro wiki](https://wiki.banana-pi.org/Getting_Started_with_R2PRO) for how to flash the android image.
