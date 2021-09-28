package com.android.server.rkdisplay;

import java.io.InputStream;
import java.io.StringWriter;
import java.util.ArrayList;
import java.util.List;

import org.xmlpull.v1.XmlPullParser;
import org.xmlpull.v1.XmlSerializer;
import android.util.Log;
import android.util.Xml;

public class ResolutionParser {
    private static final String TAG = "PullResolutionParser";
    private static final int SYNC_FLAG = 0xF;
    private static final int FLAG_INTERLACE = 1<<4;
    private static final int FLAG_RADIO = 0xF<<19;

    public ResolutionParser(){
    }

    public static final class RkResolutionInfo{
        public int clock;
        public int hdisplay;
        public int hsync_start;
        public int hsync_end;
        public int htotal;
        public int hskew;
        public int vdisplay;
        public int vsync_start;
        public int vsync_end;
        public int vtotal;
        public int vscan;
        public float vrefresh;
        public int flags;

        public RkResolutionInfo(){
        }

        public boolean hasMatchingMode(RkDisplayModes.RkPhysicalDisplayInfo info){
            int checkFlags = SYNC_FLAG | FLAG_INTERLACE;

            return hdisplay == info.width
                && vdisplay == info.height
                && clock == info.clock
                && vtotal == info.vtotal
                && htotal == info.htotal
                && hsync_start == info.hsync_start
                && hsync_end == info.hsync_end
                && vsync_start == info.vsync_start
                && vsync_end == info.vsync_end
                && (flags&checkFlags)==(info.flags&checkFlags);
        }

        public void sethdisplay(int hdisplay){
            this.hdisplay = hdisplay;
        }

        public void setvdisplay(int vdisplay){
            this.vdisplay = vdisplay;
        }

        public void sethsync_start(int hsync_start){
            this.hsync_start = hsync_start;
        }

        public void setvsync_end(int vsync_end){
            this.vsync_end = vsync_end;
        }

        public void setvsync_start(int vsync_start){
            this.vsync_start = vsync_start;
        }

        public void sethsync_end(int hsync_end){
            this.hsync_end = hsync_end;
        }

        public void sethtotal(int htotal){
            this.htotal = htotal;
        }

        public void setvtotal(int vtotal){
            this.vtotal = vtotal;
        }

        public void setvscan(int vscan){
            this.vscan = vscan;
        }

        public void sethskew(int hskew){
            this.hskew = hskew;
        }

        public void setclock(int clock){
            this.clock = clock;
        }

        public void setflags(int flags){
            this.flags = flags;
        }

        public void setvrefresh(float vrefresh){
            this.vrefresh = vrefresh;
        }

        public int gethdisplay(){
            return hdisplay;
        }

        public int getvdisplay(){
            return vdisplay;
        }

        public int gethsync_start(){
            return hsync_start;
        }

        public int getvsync_start(){
            return vsync_start;
        }

        public int gethsync_end(){
            return hsync_end;
        }

        public int gethtotal(){
            return htotal;
        }

        public int getvtotal(){
            return vtotal;
        }

        public int getvscan(){
            return vscan;
        }

        public int gethskew(){
            return hskew;
        }

        public int getclock(){
            return clock;
        }

        public int getflags(){
            return flags;
        }

        public float getvrefresh(){
            return vrefresh;
        }
    }

    public static List<RkResolutionInfo> parse(InputStream is) throws Exception {
        List<RkResolutionInfo> resolutions = null;
        RkResolutionInfo reso = null;
        XmlPullParser parser = Xml.newPullParser();

        parser.setInput(is, "UTF-8");
        int eventType = parser.getEventType();
        while (eventType != XmlPullParser.END_DOCUMENT) {
            switch (eventType) {
                case XmlPullParser.START_DOCUMENT:
                resolutions = new ArrayList<RkResolutionInfo>();
                break;
                case XmlPullParser.START_TAG:
                    if (parser.getName().equals("resolution")) {
                        reso = new RkResolutionInfo();
                    } else if (parser.getName().equals("clock")) {
                        eventType = parser.next();
                        reso.setclock(Integer.parseInt(parser.getText()));
                        Log.w(TAG, "parse resolution + clock " + reso.getclock() + " txt: " + parser.getText());
                    } else if (parser.getName().equals("hdisplay")) {
                        eventType = parser.next();
                        reso.sethdisplay(Integer.parseInt(parser.getText()));
                    } else if (parser.getName().equals("hsync_start")) {
                        eventType = parser.next();
                        reso.sethsync_start(Integer.parseInt(parser.getText()));
                    } else if (parser.getName().equals("hsync_end")) {
                        eventType = parser.next();
                        reso.sethsync_end(Integer.parseInt(parser.getText()));
                    } else if (parser.getName().equals("htotal")) {
                        eventType = parser.next();
                        reso.sethtotal(Integer.parseInt(parser.getText()));
                    } else if (parser.getName().equals("hskew")) {
                        eventType = parser.next();
                        reso.sethskew(Integer.parseInt(parser.getText()));
                    } else if (parser.getName().equals("vdisplay")) {
                        eventType = parser.next();
                        reso.setvdisplay(Integer.parseInt(parser.getText()));
                    } else if (parser.getName().equals("vsync_start")) {
                        eventType = parser.next();
                        reso.setvsync_start(Integer.parseInt(parser.getText()));
                    } else if (parser.getName().equals("vsync_end")) {
                        eventType = parser.next();
                        reso.setvsync_end(Integer.parseInt(parser.getText()));
                    } else if (parser.getName().equals("vtotal")) {
                        eventType = parser.next();
                        reso.setvtotal(Integer.parseInt(parser.getText()));
                    } else if (parser.getName().equals("vscan")) {
                        eventType = parser.next();
                        reso.setvscan(Integer.parseInt(parser.getText()));
                    } else if (parser.getName().equals("flags")) {
                        eventType = parser.next();
                        reso.setflags(Integer.parseInt(parser.getText(),16));
                        Log.w(TAG, "parse resolution flags:  " + reso.getflags() + "  txt: " + parser.getText());
                    } else if (parser.getName().equals("vrefresh")) {
                        eventType = parser.next();
                        reso.setvrefresh(Float.parseFloat(parser.getText()));
                    }
                    break;
                case XmlPullParser.END_TAG:
                    if (parser.getName().equals("resolution")) {
                        resolutions.add(reso);
                        reso = null;
                        break;
                    }
            }
            eventType = parser.next();
        }
        return resolutions;
    }

    public String serialize(List<RkResolutionInfo> resolutions) throws Exception {
        XmlSerializer serializer = Xml.newSerializer();
        StringWriter writer = new StringWriter();

        serializer.setOutput(writer);
        serializer.startDocument("UTF-8", true);
        serializer.startTag("", "resolutions");
        for (RkResolutionInfo reso : resolutions) {
            serializer.startTag("", "resolution");
            //serializer.attribute("", "clock", reso.getclock() + "");
            serializer.startTag("", "clock");
            serializer.text(reso.getclock()+ "");
            serializer.endTag("", "clock");
            serializer.startTag("", "hdisplay");
            serializer.text(reso.gethdisplay()+ "");
            serializer.endTag("", "hdisplay");

            serializer.startTag("", "hsync_start");
            serializer.text(reso.gethsync_start() + "");
            serializer.endTag("", "hsync_start");

            serializer.endTag("", "resolution");
        }
        serializer.endTag("", "resolutions");
        serializer.endDocument();

        return writer.toString();
    }
}


