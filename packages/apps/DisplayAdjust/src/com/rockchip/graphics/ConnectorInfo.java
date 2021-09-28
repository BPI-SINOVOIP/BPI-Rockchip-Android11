package com.rockchip.graphics;

import android.util.Log;

import java.util.regex.Matcher;
import java.util.regex.Pattern;

public class ConnectorInfo {

    public static final String[] CONNECTOR_TYPE = {"Unknown", "VGA", "DVII", "DVID", "DVIA", "Composite", "SVIDEO", "LVDS", "Component",
            "9PinDIN", "DisplayPort", "HDMIA", "HDMIB", "TV", "eDP", "VIRTUAL", "DSI", "DPI"
    };

    private int type;
    private int id;
    private int state;
    private int dpy;

    private static final String REGEX_TYPE = "type:(\\d+)";
    private static final String REGEX_ID = "id:(\\d+)";
    private static final String REGEX_STATE = "state:(\\d+)";

    public ConnectorInfo(String info, int dpy) {
        Pattern p = Pattern.compile(REGEX_TYPE);
        Matcher m = p.matcher(info);
        if (m.find()) {
            type = Integer.parseInt(m.group(1));
        }
        p = Pattern.compile(REGEX_ID);
        m = p.matcher(info);
        if (m.find()) {
            id = Integer.parseInt(m.group(1));
        }
        p = Pattern.compile(REGEX_STATE);
        m = p.matcher(info);
        if (m.find()) {
            state = Integer.parseInt(m.group(1));
        }
        this.dpy = dpy;
    }

    public int getDpy() {
        return dpy;
    }

    public int getType() {
        return type;
    }

    public int getId() {
        return id;
    }

    public int getState() {
        return state;
    }

    @Override
    public String toString() {
        return "ConnectorInfo{" +
                "dpy=" + dpy +
                "type=" + type +
                ", id=" + id +
                ", state=" + state +
                '}';
    }
}
