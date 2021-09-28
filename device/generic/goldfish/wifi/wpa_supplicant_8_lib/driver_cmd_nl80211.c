/*
 * Driver interaction with extended Linux CFG8021
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Alternatively, this software may be distributed under the terms of BSD
 * license.
 */

#include "includes.h"
#include "driver_cmd_nl80211.h"

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

//#if GCE_PLATFORM_SDK_AFTER(L_MR1)
// Android M exposes headers more directly.
#include <netinet/in.h>
#include <linux/if.h>
#include "driver_nl80211.h"
/*
#elif GCE_PLATFORM_SDK_AFTER(J_MR2)
// Android versions K and L put structures in hardware_legacy
#include "hardware_legacy/driver_nl80211.h"
#else
// Android version J does not expose structures directly. These structures are
// manually defined later.
#include <netinet/in.h>
#include <linux/if.h>
#endif
*/

#include "common.h"
#include "wpa_supplicant_i.h"
#include "config.h"
#include "android_drv.h"
#include "linux_ioctl.h"


int wpa_driver_nl80211_driver_cmd(
    void* priv, char* cmd, char* buf, size_t buf_len) {
  struct i802_bss* bss = priv;
  struct wpa_driver_nl80211_data* drv = bss->drv;
  int ret = 0;

  D("%s: called", __FUNCTION__);
  if (os_strcasecmp(cmd, "STOP") == 0) {
    linux_set_iface_flags(drv->global->ioctl_sock, bss->ifname, 0);
    wpa_msg(drv->ctx, MSG_INFO, WPA_EVENT_DRIVER_STATE "STOPPED");
  } else if (os_strcasecmp(cmd, "START") == 0) {
    linux_set_iface_flags(drv->global->ioctl_sock, bss->ifname, 1);
    wpa_msg(drv->ctx, MSG_INFO, WPA_EVENT_DRIVER_STATE "STARTED");
  } else if (os_strcasecmp(cmd, "MACADDR") == 0) {
    u8 macaddr[ETH_ALEN] = {};

    ret = linux_get_ifhwaddr(drv->global->ioctl_sock, bss->ifname, macaddr);
    if (!ret)
      ret = os_snprintf(
          buf, buf_len, "Macaddr = " MACSTR "\n", MAC2STR(macaddr));
  } else if (os_strcasecmp(cmd, "RELOAD") == 0) {
    wpa_msg(drv->ctx, MSG_INFO, WPA_EVENT_DRIVER_STATE "HANGED");
  } else {  // Use private command
    return 0;
  }
  return ret;
}


int wpa_driver_set_p2p_noa(
    __attribute__((__unused__)) void* priv,
    __attribute__((__unused__)) u8 count,
    __attribute__((__unused__)) int start,
    __attribute__((__unused__)) int duration) {
  D("%s: called", __FUNCTION__);
  return 0;
}


int wpa_driver_get_p2p_noa(
    __attribute__((__unused__)) void* priv,
    __attribute__((__unused__)) u8* buf,
    __attribute__((__unused__)) size_t len) {
  D("%s: called", __FUNCTION__);
  return 0;
}


int wpa_driver_set_p2p_ps(
    __attribute__((__unused__)) void* priv,
    __attribute__((__unused__)) int legacy_ps,
    __attribute__((__unused__)) int opp_ps,
    __attribute__((__unused__)) int ctwindow) {
  D("%s: called", __FUNCTION__);
  return -1;
}


int wpa_driver_set_ap_wps_p2p_ie(
    __attribute__((__unused__)) void* priv,
    __attribute__((__unused__)) const struct wpabuf* beacon,
    __attribute__((__unused__)) const struct wpabuf* proberesp,
    __attribute__((__unused__)) const struct wpabuf* assocresp) {
  D("%s: called", __FUNCTION__);
  return 0;
}
