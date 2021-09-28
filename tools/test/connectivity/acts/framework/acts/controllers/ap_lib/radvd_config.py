#   Copyright 2020 - The Android Open Source Project
#
#   Licensed under the Apache License, Version 2.0 (the "License");
#   you may not use this file except in compliance with the License.
#   You may obtain a copy of the License at
#
#       http://www.apache.org/licenses/LICENSE-2.0
#
#   Unless required by applicable law or agreed to in writing, software
#   distributed under the License is distributed on an "AS IS" BASIS,
#   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#   See the License for the specific language governing permissions and
#   limitations under the License.

import collections


class RadvdConfig(object):
    """The root settings for the router advertisement daemon.

    All the settings for a router advertisement daemon.
    """
    def __init__(self,
                 prefix,
                 clients=[],
                 route=None,
                 rdnss=[],
                 ignore_if_missing=None,
                 adv_send_advert=None,
                 unicast_only=None,
                 max_rtr_adv_interval=None,
                 min_rtr_adv_interval=None,
                 min_delay_between_ras=None,
                 adv_managed_flag=None,
                 adv_other_config_flag=None,
                 adv_link_mtu=None,
                 adv_reachable_time=None,
                 adv_retrans_timer=None,
                 adv_cur_hop_limit=None,
                 adv_default_lifetime=None,
                 adv_default_preference=None,
                 adv_source_ll_address=None,
                 adv_home_agent_flag=None,
                 adv_home_agent_info=None,
                 home_agent_lifetime=None,
                 home_agent_preference=None,
                 adv_mob_rtr_support_flag=None,
                 adv_interval_opt=None,
                 adv_on_link=None,
                 adv_autonomous=None,
                 adv_router_addr=None,
                 adv_valid_lifetime=None,
                 adv_preferred_lifetime=None,
                 base_6to4_interface=None,
                 adv_route_lifetime=None,
                 adv_route_preference=None,
                 adv_rdnss_preference=None,
                 adv_rdnss_open=None,
                 adv_rdnss_lifetime=None):
        """Construct a RadvdConfig.

        Args:
            prefix: IPv6 prefix and length, ie fd::/64
            clients: A list of IPv6 link local addresses that will be the only
                clients served.  All other IPv6 addresses will be ignored if
                this list is present.
            route: A route for the router advertisement with prefix.
            rdnss: A list of recursive DNS servers
            ignore_if_missing: A flag indicating whether or not the interface
                is ignored if it does not exist at start-up. By default,
                radvd exits.
            adv_send_advert: A flag indicating whether or not the router sends
                periodic router advertisements and responds to router
                solicitations.
            unicast_only: Indicates that the interface link type only supports
                unicast.
            max_rtr_adv_interval:The maximum time allowed between sending
                unsolicited multicast router advertisements from the interface,
                in seconds. Must be no less than 4 seconds and no greater than
                1800 seconds.
            min_rtr_adv_interval: The minimum time allowed between sending
                unsolicited multicast router advertisements from the interface,
                in seconds. Must be no less than 3 seconds and no greater than
                0.75 * max_rtr_adv_interval.
            min_delay_between_ras: The minimum time allowed between sending
                multicast router advertisements from the interface, in seconds.,
            adv_managed_flag: When set, hosts use the administered (stateful)
                protocol for address autoconfiguration in addition to any
                addresses autoconfigured using stateless address
                autoconfiguration. The use of this flag is described in
                RFC 4862.
            adv_other_config_flag: When set, hosts use the administered
                (stateful) protocol for autoconfiguration of other (non-address)
                information. The use of this flag is described in RFC 4862.
            adv_link_mtu: The MTU option is used in router advertisement
                messages to insure that all nodes on a link use the same MTU
                value in those cases where the link MTU is not well known.
            adv_reachable_time: The time, in milliseconds, that a node assumes
                a neighbor is reachable after having received a reachability
                confirmation. Used by the Neighbor Unreachability Detection
                algorithm (see Section 7.3 of RFC 4861). A value of zero means
                unspecified (by this router).
            adv_retrans_timer: The time, in milliseconds, between retransmitted
                Neighbor Solicitation messages. Used by address resolution and
                the Neighbor Unreachability Detection algorithm (see Sections
                7.2 and 7.3 of RFC 4861). A value of zero means unspecified
                (by this router).
            adv_cur_hop_limit: The default value that should be placed in the
                Hop Count field of the IP header for outgoing (unicast) IP
                packets. The value should be set to the current diameter of the
                Internet. The value zero means unspecified (by this router).
            adv_default_lifetime: The lifetime associated with the default
                router in units of seconds. The maximum value corresponds to
                18.2 hours. A lifetime of 0 indicates that the router is not a
                default router and should not appear on the default router list.
                The router lifetime applies only to the router's usefulness as
                a default router; it does not apply to information contained in
                other message fields or options. Options that need time limits
                for their information include their own lifetime fields.
            adv_default_preference: The preference associated with the default
                router, as either "low", "medium", or "high".
            adv_source_ll_address: When set, the link-layer address of the
                outgoing interface is included in the RA.
            adv_home_agent_flag: When set, indicates that sending router is able
                to serve as Mobile IPv6 Home Agent. When set, minimum limits
                specified by Mobile IPv6 are used for MinRtrAdvInterval and
                MaxRtrAdvInterval.
            adv_home_agent_info: When set, Home Agent Information Option
                (specified by Mobile IPv6) is included in Router Advertisements.
                adv_home_agent_flag must also be set when using this option.
            home_agent_lifetime: The length of time in seconds (relative to the
                time the packet is sent) that the router is offering Mobile IPv6
                 Home Agent services. A value 0 must not be used. The maximum
                 lifetime is 65520 seconds (18.2 hours). This option is ignored,
                 if adv_home_agent_info is not set.
            home_agent_preference: The preference for the Home Agent sending
                this Router Advertisement. Values greater than 0 indicate more
                preferable Home Agent, values less than 0 indicate less
                preferable Home Agent. This option is ignored, if
                adv_home_agent_info is not set.
            adv_mob_rtr_support_flag: When set, the Home Agent signals it
                supports Mobile Router registrations (specified by NEMO Basic).
                adv_home_agent_info must also be set when using this option.
            adv_interval_opt: When set, Advertisement Interval Option
                (specified by Mobile IPv6) is included in Router Advertisements.
                When set, minimum limits specified by Mobile IPv6 are used for
                MinRtrAdvInterval and MaxRtrAdvInterval.
            adv_on_linkWhen set, indicates that this prefix can be used for
                on-link determination. When not set the advertisement makes no
                statement about on-link or off-link properties of the prefix.
                For instance, the prefix might be used for address configuration
                 with some of the addresses belonging to the prefix being
                 on-link and others being off-link.
            adv_autonomous: When set, indicates that this prefix can be used for
                autonomous address configuration as specified in RFC 4862.
            adv_router_addr: When set, indicates that the address of interface
                is sent instead of network prefix, as is required by Mobile
                IPv6. When set, minimum limits specified by Mobile IPv6 are used
                for MinRtrAdvInterval and MaxRtrAdvInterval.
            adv_valid_lifetime: The length of time in seconds (relative to the
                time the packet is sent) that the prefix is valid for the
                purpose of on-link determination. The symbolic value infinity
                represents infinity (i.e. a value of all one bits (0xffffffff)).
                 The valid lifetime is also used by RFC 4862.
            adv_preferred_lifetimeThe length of time in seconds (relative to the
                time the packet is sent) that addresses generated from the
                prefix via stateless address autoconfiguration remain preferred.
                The symbolic value infinity represents infinity (i.e. a value of
                all one bits (0xffffffff)). See RFC 4862.
            base_6to4_interface: If this option is specified, this prefix will
                be combined with the IPv4 address of interface name to produce
                a valid 6to4 prefix. The first 16 bits of this prefix will be
                replaced by 2002 and the next 32 bits of this prefix will be
                replaced by the IPv4 address assigned to interface name at
                configuration time. The remaining 80 bits of the prefix
                (including the SLA ID) will be advertised as specified in the
                configuration file.
            adv_route_lifetime: The lifetime associated with the route in units
                of seconds. The symbolic value infinity represents infinity
                (i.e. a value of all one bits (0xffffffff)).
            adv_route_preference: The preference associated with the default
                router, as either "low", "medium", or "high".
            adv_rdnss_preference: The preference of the DNS server, compared to
                other DNS servers advertised and used. 0 to 7 means less
                important than manually configured nameservers in resolv.conf,
                while 12 to 15 means more important.
            adv_rdnss_open: "Service Open" flag. When set, indicates that RDNSS
                continues to be available to hosts even if they moved to a
                different subnet.
            adv_rdnss_lifetime: The maximum duration how long the RDNSS entries
                are used for name resolution. A value of 0 means the nameserver
                should no longer be used. The maximum duration how long the
                RDNSS entries are used for name resolution. A value of 0 means
                the nameserver should no longer be used. The value, if not 0,
                must be at least max_rtr_adv_interval. To ensure stale RDNSS
                info gets removed in a timely fashion, this should not be
                greater than 2*max_rtr_adv_interval.
        """
        self._prefix = prefix
        self._clients = clients
        self._route = route
        self._rdnss = rdnss
        self._ignore_if_missing = ignore_if_missing
        self._adv_send_advert = adv_send_advert
        self._unicast_only = unicast_only
        self._max_rtr_adv_interval = max_rtr_adv_interval
        self._min_rtr_adv_interval = min_rtr_adv_interval
        self._min_delay_between_ras = min_delay_between_ras
        self._adv_managed_flag = adv_managed_flag
        self._adv_other_config_flag = adv_other_config_flag
        self._adv_link_mtu = adv_link_mtu
        self._adv_reachable_time = adv_reachable_time
        self._adv_retrans_timer = adv_retrans_timer
        self._adv_cur_hop_limit = adv_cur_hop_limit
        self._adv_default_lifetime = adv_default_lifetime
        self._adv_default_preference = adv_default_preference
        self._adv_source_ll_address = adv_source_ll_address
        self._adv_home_agent_flag = adv_home_agent_flag
        self._adv_home_agent_info = adv_home_agent_info
        self._home_agent_lifetime = home_agent_lifetime
        self._home_agent_preference = home_agent_preference
        self._adv_mob_rtr_support_flag = adv_mob_rtr_support_flag
        self._adv_interval_opt = adv_interval_opt
        self._adv_on_link = adv_on_link
        self._adv_autonomous = adv_autonomous
        self._adv_router_addr = adv_router_addr
        self._adv_valid_lifetime = adv_valid_lifetime
        self._adv_preferred_lifetime = adv_preferred_lifetime
        self._base_6to4_interface = base_6to4_interface
        self._adv_route_lifetime = adv_route_lifetime
        self._adv_route_preference = adv_route_preference
        self._adv_rdnss_preference = adv_rdnss_preference
        self._adv_rdnss_open = adv_rdnss_open
        self._adv_rdnss_lifetime = adv_rdnss_lifetime

    def package_configs(self):
        conf = dict()
        conf['prefix'] = self._prefix
        conf['clients'] = self._clients
        conf['route'] = self._route
        conf['rdnss'] = self._rdnss

        conf['interface_options'] = collections.OrderedDict(
            filter(lambda pair: pair[1] is not None,
                   (('IgnoreIfMissing', self._ignore_if_missing),
                    ('AdvSendAdvert', self._adv_send_advert),
                    ('UnicastOnly', self._unicast_only),
                    ('MaxRtrAdvInterval', self._max_rtr_adv_interval),
                    ('MinRtrAdvInterval', self._min_rtr_adv_interval),
                    ('MinDelayBetweenRAs', self._min_delay_between_ras),
                    ('AdvManagedFlag', self._adv_managed_flag),
                    ('AdvOtherConfigFlag', self._adv_other_config_flag),
                    ('AdvLinkMTU', self._adv_link_mtu),
                    ('AdvReachableTime', self._adv_reachable_time),
                    ('AdvRetransTimer', self._adv_retrans_timer),
                    ('AdvCurHopLimit', self._adv_cur_hop_limit),
                    ('AdvDefaultLifetime', self._adv_default_lifetime),
                    ('AdvDefaultPreference', self._adv_default_preference),
                    ('AdvSourceLLAddress', self._adv_source_ll_address),
                    ('AdvHomeAgentFlag', self._adv_home_agent_flag),
                    ('AdvHomeAgentInfo', self._adv_home_agent_info),
                    ('HomeAgentLifetime', self._home_agent_lifetime),
                    ('HomeAgentPreference', self._home_agent_preference),
                    ('AdvMobRtrSupportFlag', self._adv_mob_rtr_support_flag),
                    ('AdvIntervalOpt', self._adv_interval_opt))))

        conf['prefix_options'] = collections.OrderedDict(
            filter(lambda pair: pair[1] is not None,
                   (('AdvOnLink', self._adv_on_link),
                    ('AdvAutonomous', self._adv_autonomous),
                    ('AdvRouterAddr', self._adv_router_addr),
                    ('AdvValidLifetime', self._adv_valid_lifetime),
                    ('AdvPreferredLifetime', self._adv_preferred_lifetime),
                    ('Base6to4Interface', self._base_6to4_interface))))

        conf['route_options'] = collections.OrderedDict(
            filter(lambda pair: pair[1] is not None,
                   (('AdvRouteLifetime', self._adv_route_lifetime),
                    ('AdvRoutePreference', self._adv_route_preference))))

        conf['rdnss_options'] = collections.OrderedDict(
            filter(lambda pair: pair[1] is not None,
                   (('AdvRDNSSPreference', self._adv_rdnss_preference),
                    ('AdvRDNSSOpen', self._adv_rdnss_open),
                    ('AdvRDNSSLifetime', self._adv_rdnss_lifetime))))

        return conf
