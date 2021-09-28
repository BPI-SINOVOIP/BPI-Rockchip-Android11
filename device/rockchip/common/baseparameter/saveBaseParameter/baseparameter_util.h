#ifndef BASEPARAMETER_UTIL_H_
#define BASEPARAMETER_UTIL_H_

#include <cutils/log.h>

class BaseParameterUtil {
public:
    BaseParameterUtil(){};
    virtual ~BaseParameterUtil(){}
    virtual void print() = 0;
    virtual bool validate() = 0;
    virtual int dump_baseparameter(const char *file_path) = 0;
    virtual void setDisplayId(int dpy) = 0;
    virtual void setConnectorTypeAndId(int connectorType, int connectorId) = 0;
    virtual int setBcsh(int b, int c, int s, int h) = 0;
    virtual int setOverscan(int left, int top, int right, int bottom) = 0;
    virtual int setFramebufferInfo(int width, int height, int fps) = 0;
    virtual int setColor(int format, int depth, int feature) = 0;
    virtual int setResolution(int hdisplay, int vdisplay, int vrefresh, int hsync_start, int hsync_end, int htotal, int vsync_start, int vsync_end, int vtotal, int vscan, int flags, int clock, int feature) = 0;
};

class BaseParameterUtilV1 : public BaseParameterUtil{
public:
    BaseParameterUtilV1();
    ~BaseParameterUtilV1();
    virtual void print();
    virtual bool validate();
    virtual int dump_baseparameter(const char *file_path);
    virtual void setDisplayId(int dpy);
    virtual void setConnectorTypeAndId(int connectorType, int connectorId);
    virtual int setBcsh(int b, int c, int s, int h);
    virtual int setOverscan(int left, int top, int right, int bottom);
    virtual int setFramebufferInfo(int width, int height, int fps);
    virtual int setColor(int format, int depth, int feature);
    virtual int setResolution(int hdisplay, int vdisplay, int vrefresh, int hsync_start, int hsync_end, int htotal, int vsync_start, int vsync_end, int vtotal, int vscan, int flags, int clock, int feature);
};

class BaseParameterUtilV2 : public BaseParameterUtil{
public:
    BaseParameterUtilV2();
    ~BaseParameterUtilV2();
    virtual void print();
    virtual bool validate();
    virtual int dump_baseparameter(const char *file_path);
    virtual void setDisplayId(int dpy);
    virtual void setConnectorTypeAndId(int connectorType, int connectorId);
    virtual int setBcsh(int b, int c, int s, int h);
    virtual int setOverscan(int left, int top, int right, int bottom);
    virtual int setFramebufferInfo(int width, int height, int fps);
    virtual int setColor(int format, int depth, int feature);
    virtual int setResolution(int hdisplay, int vdisplay, int vrefresh, int hsync_start, int hsync_end, int htotal, int vsync_start, int vsync_end, int vtotal, int vscan, int flags, int clock, int feature);
};
#endif
