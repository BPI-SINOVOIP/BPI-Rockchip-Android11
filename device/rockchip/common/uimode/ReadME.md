#   Dynamic Load UiMode

##  Overview
In Box,some app didn't display normal.Because globals UiMode is Configuration.UI_MODE_TYPE_TELEVISION.Some App didn't support Tv Device. Now we provider the means that load UiMode dynamic for each App by White List Config(device/rockchip/common/uimode/package_uimode_config.xml in source, vendor/etc/package_uimode_config.xml in device, Read-write file path /data/system/package_uimode_config.xml). After the device is running, you can dynamically set the UIMode value of the app through TvSettings, which is convenient for debugging. Now this means only support box device.

##  Usage
+   Before compiling,you must to amend the config.

+   First, you need to know this app needs what config.Such as Configuration.UI_MODE_TYPE_NORMAL(1), Configuration.UI_MODE_TYPE_TELEVISION(4).

+   Second, you can edit the config in device/rockchip/common/uimode/package_uimode_config.xml.

    Such as add Amazon Prime Video

    ```<app package="com.amazon.avod.thirdpartyclient" uiMode="1"/>```

+   Last, save this config and compile the sources.

+   After the device is started, it can be set in TvSettings-Apps-See all apps-xxx App-UiMode to debug the application.


-----
#   动态加载UiMode

##  概述
在盒子里，有部分App无法正常显示。因为全局的UIMode是Configuration.UI_MODE_TYPE_TELEVISION，这部分App不支持Tv端的显示。现在我们提供了一种方式，通过白名单配置来动态的为App加载UIMode（源码路径：device/rockchip/common/uimode/package_uimode_config.xml,设备路径：vendor/etc/package_uimode_config.xml， 可读写文件路径/data/system/package_uimode_config.xml）。设备运行后可以通过TvSettings动态设置App的UIMode值，以此方便调试。目前此方式仅支持box设备。

##  使用说明

+   在编译前，必须要对白名单文件进行修改。

+   需要了解应用需要哪个配置，哪个配置能够使应用显示正常。例如：Configuration.UI_MODE_TYPE_NORMAL(1)，Configuration.UI_MODE_TYPE_TELEVISION(4)。

+   编辑白名单配置文件device/rockchip/common/uimode/package_uimode_config.xml。

    例如添加Amazon Prime Video：

    ```<app package="com.amazon.avod.thirdpartyclient" uiMode="1"/>```

+   保存文件后开始编译。

+   设备启动后，可在TvSettings-Apps-See all apps-xxx App-UiMode进行设置，方便对应用进行调试。
