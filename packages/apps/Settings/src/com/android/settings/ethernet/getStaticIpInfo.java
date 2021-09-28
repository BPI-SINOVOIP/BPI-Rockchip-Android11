package com.android.settings.ethernet;

public interface getStaticIpInfo {
    public boolean getStaticIp(String ipAddr);
    public boolean getStaticNetMask(String netMask);
    public boolean getStaticGateway(String gateway);
    public boolean getStaticDns1(String dns1);
    public boolean getStaticDns2(String dns2);
}
