#!/system/bin/sh
#
# Copyright (C) 2020 The Android Open Source Project
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

# performance test setup for 2020 devices
function disable_thermal()
{
    thermal_path='/sys/devices/virtual/thermal/'

    nz=$(ls -al $thermal_path | grep thermal_zone | wc -l)
    i=0
    while [ $i -lt $nz ]; do
        tz_path=$thermal_path'thermal_zone'$i'/'
        mode_path=$tz_path'mode'

        if [ -f $mode_path ]; then
            echo disabled > $tz_path'mode'
        fi
        i=$(($i + 1));
    done
}

disable_thermal
setprop vendor.powerhal.init 0
setprop ctl.restart vendor.power-hal-aidl

cpubase=/sys/devices/system/cpu
gov=cpufreq/scaling_governor

cpu=6
top=8
#Big Core 652800  940800 1152000 1478400 1728000 1900800 2092800 2208000
#Big Plus 806400 1094400 1401600 1766400 1996800 2188800 2304000 2400000
cpufreq=2092800
# Set the bigcores around 2G
while [ $((cpu < $top)) -eq 1 ]; do
  echo 1 > $cpubase/cpu${cpu}/online
  echo userspace > $cpubase/cpu${cpu}/$gov
  echo $cpufreq > /sys/devices/system/cpu/cpu$cpu/cpufreq/scaling_max_freq
  echo $cpufreq > /sys/devices/system/cpu/cpu$cpu/cpufreq/scaling_setspeed
  S=`cat $cpubase/cpu${cpu}/cpufreq/scaling_cur_freq`
  echo "set cpu $cpu to $S kHz"
  cpu=$(($cpu + 1))
done

cpu=0
top=6
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

#625 500 400 275 mhz
echo performance > /sys/class/kgsl/kgsl-3d0/devfreq/governor
echo 0 > /sys/class/kgsl/kgsl-3d0/max_pwrlevel
echo 0 > /sys/class/kgsl/kgsl-3d0/min_pwrlevel
echo -n "set GPU frequency to max at "
cat /sys/class/kgsl/kgsl-3d0/devfreq/cur_freq
