#!/system/bin/sh

module_path_bcmdhd=/pcba/lib/modules/rkwifi/bcmdhd/bcmdhd.ko
module_path_8188eu=/pcba/lib/modules/rtl8188eu/8188eu.ko
module_path_8723bu=/pcba/lib/modules/rtl8723bu/8723bu.ko
module_path_8723bs=/pcba/lib/modules/rtl8723bs/8723bs.ko
module_path_8723cs=/pcba/lib/modules/rtl8723cs/8723cs.ko
module_path_8723ds=/pcba/lib/modules/rtl8723ds/8723ds.ko
module_path_8188fu=/pcba/lib/modules/rtl8188fu/8188fu.ko
module_path_8822bs=/pcba/lib/modules/rtl8822bs/8822bs.ko
module_path_8189es=/pcba/lib/modules/rtl8189es/8189es.ko
module_path_8189fs=/pcba/lib/modules/rtl8189fs/8189fs.ko
module_path_8822ce=/pcba/lib/modules/rtl8822ce/8822ce.ko
module_path_8822be=/pcba/lib/modules/rtl8822be/8822be.ko

result_file=/data/scan_result.txt
result_file2=/data/scan_result2.txt
chip_type_path=/data/wifi_chip
driver_node=/sys/class/rkwifi/driver
pcba_node=/sys/class/rkwifi/pcba
version_path=/proc/version
module_path=$module_path_wlan
chip_broadcom=false
driver_buildin=false
interface_up=true
version=.3.0.36+
mt5931_kitkat=false
android_kitkat=false

jmax=3

if  cat $chip_type_path |  grep AP; then
  module_path=$module_path_bcmdhd
  chip_broadcom=true
  echo 1 > $pcba_node
fi

if  cat $chip_type_path |  grep RTL8188EU; then
  jmax=6
  module_path=$module_path_8188eu
fi

if  cat $chip_type_path |  grep RTL8723AU; then
  module_path=$module_path_8723bu
fi

if  cat $chip_type_path |  grep RTL8723BS; then
  module_path=$module_path_8723bs
fi

if  cat $chip_type_path |  grep RTL8723CS; then
  module_path=$module_path_8723cs
fi

if  cat $chip_type_path |  grep RTL8723DS; then
  module_path=$module_path_8723ds
fi

if  cat $chip_type_path |  grep RTL8188FU; then
  jmax=6
  module_path=$module_path_8188fu
fi

if  cat $chip_type_path |  grep RTL8822BS; then
  module_path=$module_path_8822bs
fi

if  cat $chip_type_path |  grep RTL8189ES; then
  module_path=$module_path_8189es
fi

if  cat $chip_type_path |  grep RTL8189FS; then
  module_path=$module_path_8189fs
fi

if  cat $chip_type_path |  grep RTL8822BE; then
  module_path=$module_path_8822be
fi

if  cat $chip_type_path |  grep RTL8822CE; then
  module_path=$module_path_8822ce
fi

if  cat $version_path |  grep 3.0.36+; then
  echo "kernel version 3.0.36+"
  if [ -e $module_path$version ]; then
    module_path=$module_path$version
  fi
fi

if  ls /dev/wmtWifi |  grep wmtWifi; then
  echo "mt5931_kitkat=true"
  mt5931_kitkat=true
fi

if  ifconfig wlan0; then
  echo "android_kitkat=true"
  android_kitkat=true
fi

#if  ls $driver_node; then
#  echo "wifi driver is buildin"
#  driver_buildin=true
#fi

echo "touch $result_file"
touch $result_file

j=0

echo "get scan results"
while [ $j -lt $jmax ]; 
do
    echo "insmod wifi driver"
    if [ $mt5931_kitkat = "true" ]; then
        echo "echo 1 > /dev/wmtWifi"
        echo 1 > /dev/wmtWifi
    else
      if [ $android_kitkat = "false" ]; then
        if [ $driver_buildin = "true" ]; then
          echo "echo 1 > $driver_node"
          echo 1 > "$driver_node"
        else
          echo "insmod $module_path"
          insmod "$module_path"
        fi
      fi
    fi
    if [ $? -ne 0 ]; then
        echo "insmod failed"
        exit 0
    fi

    echo "sleep 3s"
     sleep 3

    if  ifconfig wlan0; then
        if [ $interface_up = "true" ]; then
             ifconfig wlan0 up
        fi
        #if [ $? -ne 0 ]; then
        #    echo "ifconfig wlan0 up failed"
        #    exit 0
        #fi
    
        iwlist wlan0 scanning > $result_file
        if [ $chip_broadcom = "true" ]; then
            echo "sleep 3s"
             sleep 3    
        fi
        iwlist wlan0 scanning last |  grep SSID > $result_file
         cat $result_file
        iwlist wlan0 scanning last |  grep "Signal level" > $result_file2
         cat $result_file2
        echo "success"
        exit 1
    fi

    echo "remove wifi driver"
    if [ $mt5931_kitkat = "true" ]; then
        echo "echo 0 > /dev/wmtWifi"
        echo 0 > /dev/wmtWifi
    else
      if [ $android_kitkat = "false" ]; then
        if [ $driver_buildin = "true" ]; then
          echo "echo 0 > $driver_node"
          echo 0 > "$driver_node"
        else
          echo "rmmod wlan"
          rmmod wlan
        fi
      fi
    fi
     sleep 1
    
    j=$((j+1))
done

echo "wlan test failed"
exit 0

