"""Class for Wireless config."""

NET_IFACE = "lan"


class WirelessConfig(object):
  """Creates an object to hold wireless config.

  Attributes:
    name: name of the wireless config
    ssid: SSID of the network.
    security: security of the wifi network.
    band: band of the wifi network.
    iface: network interface of the wifi network.
    password: password for psk network.
    wep_key: wep keys for wep network.
    wep_key_num: key number for wep network.
    radius_server_ip: IP address of radius server.
    radius_server_port: Port number of radius server.
    radius_server_secret: Secret key of radius server.
    hidden: Boolean, if the wifi network is hidden.
    ieee80211w: PMF bit of the wifi network.
  """

  def __init__(
      self,
      name,
      ssid,
      security,
      band,
      iface=NET_IFACE,
      password=None,
      wep_key=None,
      wep_key_num=1,
      radius_server_ip=None,
      radius_server_port=None,
      radius_server_secret=None,
      hidden=False,
      ieee80211w=None):
    self.name = name
    self.ssid = ssid
    self.security = security
    self.band = band
    self.iface = iface
    self.password = password
    self.wep_key = wep_key
    self.wep_key_num = wep_key_num
    self.radius_server_ip = radius_server_ip
    self.radius_server_port = radius_server_port
    self.radius_server_secret = radius_server_secret
    self.hidden = hidden
    self.ieee80211w = ieee80211w

