baseparameter.img说明：
baseparameter_info中预置两组disp_info，两组的connector_type都是HDMIA，connector_id分别是0和1；

两组disp_info的overscan_info参数为：
overscan_info.maxvalue = 100;
overscan_info.leftscale = 100;
overscan_info.rightscale = 100;
overscan_info.topscale = 100;
overscan_info.bottomscale = 100;

drm_display_mode参数为：
drm_display_mode.hdisplay = 1920;
drm_display_mode.vdisplay = 1080;
drm_display_mode.vrefresh = 60;
drm_display_mode.hsync_start = 2008;
drm_display_mode.hsync_end = 2052;
drm_display_mode.htotal = 2200;
drm_display_mode.vsync_start = 1084;
drm_display_mode.vsync_end = 1089;
drm_display_mode.vtotal = 1125;
drm_display_mode.vscan = 0;
drm_display_mode.flags = 5;
drm_display_mode.clock = 148500;

framebuffer_info参数为：
framebuffer_info.framebuffer_width = 0;
framebuffer_info.framebuffer_height = 0;
framebuffer_info.fps = 60;

bcsh_info参数为:
bcsh_info.brightness = 50;
bcsh_info.contrast = 50;
bcsh_info.saturation = 50;
bcsh_info.hue = 50;

其余参数均为0