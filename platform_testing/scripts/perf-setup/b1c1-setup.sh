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

# performance test setup for 2018 devices

stop vendor.thermal-engine
setprop vendor.powerhal.init 0
setprop ctl.restart vendor.power-hal-aidl
setprop ctl.interface_restart android.hardware.power@1.0::IPower/default

cpubase=/sys/devices/system/cpu
gov=cpufreq/scaling_governor

cpu=4
top=8

cpufreq=2092800
# Set the golden cores at 2092800.
# 825600 902400 979200 1056000 1209600 1286400 1363200 1459200 1536000
# 1612800 1689600 1766400 1843200 1920000 1996800 2092800 2169600
# 2246400 2323200 2400000 2476800 2553600 2649600
while [ $((cpu < $top)) -eq 1 ]; do
  echo 1 > $cpubase/cpu${cpu}/online
  echo performance > $cpubase/cpu${cpu}/$gov
  echo $cpufreq > /sys/devices/system/cpu/cpu$cpu/cpufreq/scaling_max_freq
  S=`cat $cpubase/cpu${cpu}/cpufreq/scaling_cur_freq`
  echo "set cpu $cpu to $S kHz"
  cpu=$(($cpu + 1))
done

cpu=0
top=4

# Disable the silver cores.
while [ $((cpu < $top)) -eq 1 ]; do
  echo "disable cpu $cpu"
  echo 0 > $cpubase/cpu${cpu}/online
  cpu=$(($cpu + 1))
done

echo "disable GPU bus split"
echo 0 > /sys/class/kgsl/kgsl-3d0/bus_split
echo "enable GPU force clock on"
echo 1 > /sys/class/kgsl/kgsl-3d0/force_clk_on
echo "set GPU idle timer to 10000"
echo 10000 > /sys/class/kgsl/kgsl-3d0/idle_timer

# 0 381 572 762 1144 1571 2086 2597 2929 3879 4943 5931 6881
echo performance > /sys/class/devfreq/soc:qcom,gpubw/governor
echo -n "set GPU bus frequency to max at "
cat /sys/class/devfreq/soc:qcom,gpubw/cur_freq

# 257000000 342000000 414000000 520000000 596000000 675000000 710000000
G=596000000
echo performance > /sys/class/kgsl/kgsl-3d0/devfreq/governor
echo $G > /sys/class/kgsl/kgsl-3d0/devfreq/min_freq
echo $G > /sys/class/kgsl/kgsl-3d0/devfreq/max_freq
echo -n "set GPU frequency to "
cat /sys/class/kgsl/kgsl-3d0/devfreq/cur_freq
