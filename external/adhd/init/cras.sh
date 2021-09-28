# Copyright 2016 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Unified build config.
device_config_dir="$(cros_config /audio/main cras-config-dir)"
internal_ucm_suffix="$(cros_config /audio/main ucm-suffix)"

# Handle legacy config.
if [ -z "${device_config_dir}" ]; then
  # Disable HSP/HFP on Google WiFi (Gale) with UART-HCI Bluetooth
  # which is incapable of handling SCO audio.
  platform_name="$(mosys platform name)"
  if [ "$platform_name" = "Gale" ]; then
      DISABLE_PROFILE="--disable_profile=hfp,hsp"
  fi
  # For boards that need a different device config, check which config
  # directory to use. Use that directory for both volume curves
  # and DSP config.
  if [ -f /etc/cras/get_device_config_dir ]; then
    device_config_dir="$(sh /etc/cras/get_device_config_dir)"
  fi
  if [ -f /etc/cras/get_internal_ucm_suffix ]; then
    internal_ucm_suffix="$(sh /etc/cras/get_internal_ucm_suffix)"
  fi
else
  device_config_dir="/etc/cras/${device_config_dir}"
fi

if [ -n "${device_config_dir}" ]; then
  DEVICE_CONFIG_DIR="--device_config_dir=${device_config_dir}"
  DSP_CONFIG="--dsp_config=${device_config_dir}/dsp.ini"
fi
if [ -n "${internal_ucm_suffix}" ]; then
  INTERNAL_UCM_SUFFIX="--internal_ucm_suffix=${internal_ucm_suffix}"
fi

# Leave cras in the init pid namespace as it uses its PID as an IPC identifier.
exec minijail0 -u cras -g cras -G --uts -v -l \
        -T static \
        -P /mnt/empty \
        -b /,/ \
        -k 'tmpfs,/run,tmpfs,MS_NODEV|MS_NOEXEC|MS_NOSUID,mode=755,size=10M' \
        -b /run/cras,/run/cras,1 \
        -b /run/dbus,/run/dbus,1 \
        -b /run/systemd/journal \
        -b /run/udev,/run/udev \
        -b /dev,/dev \
        -b /dev/shm,/dev/shm,1 \
        -k proc,/proc,proc \
        -b /sys,/sys \
        -k 'tmpfs,/var,tmpfs,MS_NODEV|MS_NOEXEC|MS_NOSUID,mode=755,size=10M' \
        -b /var/lib/metrics/,/var/lib/metrics/,1 \
        -- \
        /sbin/minijail0 -n \
        -S /usr/share/policy/cras-seccomp.policy \
        -- \
        /usr/bin/cras \
        ${DSP_CONFIG} ${DEVICE_CONFIG_DIR} ${DISABLE_PROFILE} \
        ${INTERNAL_UCM_SUFFIX}
