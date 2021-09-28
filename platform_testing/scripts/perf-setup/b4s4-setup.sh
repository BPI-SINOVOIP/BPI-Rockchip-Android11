#!/system/bin/sh
#
# Copyright (C) 2018 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

# performance test setup for b4/s4 devices

stop vendor.thermal-engine
setprop vendor.powerhal.init 0
setprop ctl.restart vendor.power-hal-aidl
setprop ctl.interface_restart android.hardware.power@1.0::IPower/default

cpubase=/sys/devices/system/cpu
gov=cpufreq/scaling_governor

cpu=6
top=8

# Enable golden cores at max frequency.
# 300000 652800 825600 979200 1132800 1363200 1536000 1747200 1843200 1996800 2016000
# 300000 576000 748800 998400 1209600 1324800 1516800 1612800 1708800
while [ $((cpu < $top)) -eq 1 ]; do
  echo 1 > $cpubase/cpu${cpu}/online
  echo performance > $cpubase/cpu${cpu}/$gov
  S=`cat $cpubase/cpu${cpu}/cpufreq/scaling_cur_freq`
  echo "setting cpu $cpu to max at $S kHz"
  cpu=$(($cpu + 1))
done

cpu=0
top=6

# Disable silver cores.
while [ $((cpu < $top)) -eq 1 ]; do
  echo "disable cpu $cpu"
  echo 0 > $cpubase/cpu${cpu}/online
  cpu=$(($cpu + 1))
done

echo "disable GPU bus split"
echo 0 > /sys/class/kgsl/kgsl-3d0/bus_split
echo "enable GPU force clocks on"
echo 1 > /sys/class/kgsl/kgsl-3d0/force_clk_on
echo "set GPU idle timer to 10000"
echo 10000 > /sys/class/kgsl/kgsl-3d0/idle_timer

# 0 381 762 1144 1720 2086 2597 3147 3879 5161 5931 6881
echo performance > /sys/class/devfreq/soc:qcom,gpubw/governor
echo -n "set GPU bus frequency to max at "
cat /sys/class/devfreq/soc:qcom,gpubw/cur_freq

# 180000000 267000000 355000000 430000000
echo performance > /sys/class/kgsl/kgsl-3d0/devfreq/governor
echo 0 > /sys/class/kgsl/kgsl-3d0/max_pwrlevel
echo 0 > /sys/class/kgsl/kgsl-3d0/min_pwrlevel
echo -n "set GPU frequency to max at "
cat /sys/class/kgsl/kgsl-3d0/devfreq/cur_freq
