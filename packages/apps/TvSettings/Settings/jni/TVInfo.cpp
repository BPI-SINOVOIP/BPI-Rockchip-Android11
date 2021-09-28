#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <fcntl.h>
#include <string.h>
#define LOG_TAG "RKTVINFO"
#include <cutils/log.h>

#include "TVInfo.h"

struct fb_chroma {
        __u32 redx;     /* in fraction of 1024 */
        __u32 greenx;
        __u32 bluex;
        __u32 whitex;
        __u32 redy;
        __u32 greeny;
        __u32 bluey;
        __u32 whitey;
};

struct fb_monspecs {
        struct fb_chroma chroma;
        struct fb_videomode *modedb;    /* mode database */
        __u8  manufacturer[4];          /* Manufacturer */
        __u8  monitor[14];              /* Monitor String */
        __u8  serial_no[14];            /* Serial Number */
        __u8  ascii[14];                /* ? */
        __u32 modedb_len;               /* mode database length */
        __u32 model;                    /* Monitor Model */
        __u32 serial;                   /* Serial Number - Integer */
        __u32 year;                     /* Year manufactured */
        __u32 week;                     /* Week Manufactured */
        __u32 hfmin;                    /* hfreq lower limit (Hz) */
        __u32 hfmax;                    /* hfreq upper limit (Hz) */
        __u32 dclkmin;                  /* pixelclock lower limit (Hz) */
        __u32 dclkmax;                  /* pixelclock upper limit (Hz) */
        __u16 input;                    /* display type - see FB_DISP_* */
        __u16 dpms;                     /* DPMS support - see FB_DPMS_ */
        __u16 signal;                   /* Signal Type - see FB_SIGNAL_* */
        __u16 vfmin;                    /* vfreq lower limit (Hz) */
        __u16 vfmax;                    /* vfreq upper limit (Hz) */
        __u16 gamma;                    /* Gamma - in fractions of 100 */
        __u16 gtf       : 1;            /* supports GTF */
        __u16 misc;                     /* Misc flags - see FB_MISC_* */
        __u8  version;                  /* EDID version... */
        __u8  revision;                 /* ...and revision */
        __u8  max_x;                    /* Maximum horizontal size (cm) */
        __u8  max_y;                    /* Maximum vertical size (cm) */
};

static int IsHDMIConnected(void)
{
	FILE *fd = fopen("/sys/class/display/HDMI/connect", "r");
	char buf[12];

	if(fd) {
		memset(buf, 0, 12);
		fgets(buf, 12, fd);
		fclose(fd);
		return atoi(buf);
	} else {
		return 0;
	}
}

static char *itoa(int val, char *buf, unsigned radix)
{
	char   *p;             
	char   *firstdig;      
	char	temp;           
	unsigned   digval;

	p = buf;
	if(val <0) {
		*p++ = '-';
		val = (unsigned long)(-(long)val);
	}
	firstdig = p; 
	do {
	digval = (unsigned)(val % radix);
	val /= radix;

	if (digval > 9)
	    *p++ = (char)(digval - 10 + 'a'); 
	else
	    *p++ = (char)(digval + '0');      
	} while(val > 0);

	*p-- = '\0';
	do {
		temp = *p;
		*p = *firstdig;
		*firstdig = temp;
		--p;
		++firstdig;        
	} while(firstdig < p);
	return buf;
}

int PortingOutputIoctl(HMW_HDMIRK_Ioctl_E op, HMW_VOID* arg)
{
	int fd;
	struct fb_monspecs monspecs;
	HMW_TV_INFO_S *tvinfo = (HMW_TV_INFO_S*)arg;

	if (arg == NULL || !IsHDMIConnected() || op != HMW_HDMI_RK_GET_TV_INFO) {
		ALOGE("arg %p, HDMI connect %d, OP %d", arg, IsHDMIConnected(), op);
		return -1;
	}
	FILE *ffd = NULL;
	char buf[32];
	ffd = fopen("/sys/class/display/HDMI/debug", "r");
	if (!ffd) {
		ALOGE("no hdmi debug node");
		return -1;
	}
	memset(buf, 0, 32);
	fgets(buf, 32, ffd);
	fclose(ffd);
	ALOGD("buf %s", buf);
	if (memcmp(buf, "EDID status:Okay", 16)) {
		ALOGE("EDID read failed");
		return -1;
	}
	fd = open("/sys/class/display/HDMI/monspecs", O_RDONLY);
	if (fd < 0) {
		ALOGE("open monspec failed");
		return -1;
	}
	unsigned int length = lseek(fd, 0L, SEEK_END);
	lseek(fd, 0L, SEEK_SET);
	if (length < sizeof(struct fb_monspecs))
		return -1;
	int len = read(fd, &monspecs, sizeof(struct fb_monspecs));
	close(fd);
	if (len != sizeof(struct fb_monspecs)) {
		ALOGE("read size is not eqaul to fb_monspecs");
		return -1;
	}
	memcpy(tvinfo->manufName, monspecs.manufacturer, 4);
	tvinfo->manufModel = monspecs.model;
	tvinfo->manufYear = monspecs.year;
	ALOGD("x %d y %d\n", monspecs.max_x, monspecs.max_y);
	int size = sqrt(monspecs.max_x * monspecs.max_x + monspecs.max_y * monspecs.max_y)/2.54 + 0.5;
	ALOGD("size %d\n", size);
	itoa(size, (char *)tvinfo->displaySize, 10);
	return 0;
}

enum {
	HDMI_COLORIMETRY_EXTEND_XVYCC_601,
	HDMI_COLORIMETRY_EXTEND_XVYCC_709,
	HDMI_COLORIMETRY_EXTEND_SYCC_601,
	HDMI_COLORIMETRY_EXTEND_ADOBE_YCC601,
	HDMI_COLORIMETRY_EXTEND_ADOBE_RGB,
	HDMI_COLORIMETRY_EXTEND_BT_2020_YCC_C, /*constant luminance*/
	HDMI_COLORIMETRY_EXTEND_BT_2020_YCC,
	HDMI_COLORIMETRY_EXTEND_BT_2020_RGB,
};

enum hdmi_hdr_eotf {
	EOTF_TRADITIONAL_GMMA_SDR = 1,
	EOFT_TRADITIONAL_GMMA_HDR = 2,
	EOTF_ST_2084 = 4,
};

int HdmiSupportedDataSpace(void)
{
	FILE *ffd = NULL;
	char buf[64];
	int colorimetry, dataspace = 0;
	unsigned int eotf;

	if (!IsHDMIConnected())
		return 0;

	ffd = fopen("/sys/class/display/HDMI/color", "r");
	if (!ffd) {
		ALOGE("no hdmi color node");
		return 0;
	}
	memset(buf, 0, 64);
	while(fgets(buf, 64, ffd) != NULL) {
		if (!memcmp(buf, "Supported Colorimetry", 21)) {
			sscanf(buf, "Supported Colorimetry: %d", &colorimetry);
		} else if (!memcmp(buf, "Supported EOTF", 14)) {
			sscanf(buf, "Supported EOTF: 0x%x", &eotf);
		}
		memset(buf, 0, 64);
	}
	fclose(ffd);
	ALOGD("colorimetry %d, eotf 0x%x\n", colorimetry, eotf);
	if (colorimetry & HDMI_COLORIMETRY_EXTEND_BT_2020_YCC ||
	    colorimetry & HDMI_COLORIMETRY_EXTEND_BT_2020_RGB)
		dataspace |= HAL_DATASPACE_STANDARD_BT2020;
	if (colorimetry & HDMI_COLORIMETRY_EXTEND_BT_2020_YCC_C)
		dataspace |= HAL_DATASPACE_STANDARD_BT2020_CONSTANT_LUMINANCE;

	if (eotf & EOTF_ST_2084)
		dataspace |= HAL_DATASPACE_TRANSFER_ST2084;
	return dataspace;
}

int setHdmiHDR(int enable)
{
	FILE *ffd = NULL;
	char buf[64];
	int eotf;

	ffd = fopen("/sys/class/display/HDMI/color", "w");
	if (!ffd) {
		ALOGE("no hdmi color node");
		return -1;
	}
	if (enable)
		eotf = EOTF_ST_2084;
	else
		eotf = 0;
	memset(buf, 0, 64);
	sprintf(buf, "hdr=%d", eotf);
	ALOGD("%s", buf);
	fwrite(buf, 1, strlen(buf), ffd);
	fclose(ffd);
	return 0;
}

int HdmiSetColorimetry(android_dataspace_t Colorimetry)
{
	FILE *ffd = NULL;
	char buf[64];
	char colorimetry;

	ffd = fopen("/sys/class/display/HDMI/color", "w");
	if (!ffd) {
		ALOGE("no hdmi color node");
		return -1;
	}
	if (Colorimetry == HAL_DATASPACE_STANDARD_BT2020)
		colorimetry = HDMI_COLORIMETRY_EXTEND_BT_2020_YCC + 3;
	else if (Colorimetry == HAL_DATASPACE_STANDARD_BT2020_CONSTANT_LUMINANCE)
		colorimetry = HDMI_COLORIMETRY_EXTEND_BT_2020_YCC_C + 3;
	else
		colorimetry = 0;
	memset(buf, 0, 64);
	sprintf(buf, "colorimetry=%d", colorimetry);
	fwrite(buf, 1, strlen(buf), ffd);
	fclose(ffd);
	return 0;
}

#if 0
int main(int argc, char **argv)
{
	HMW_TV_INFO_S TVInfo;

	int rc = PortingOutputIoctl(HMW_HDMI_RK_GET_TV_INFO, &TVInfo);
	if (!rc) {
	printf("manufName %s\n", TVInfo.manufName);
	printf("manufModel %d\n", TVInfo.manufModel);
	printf("manufYear %d\n", TVInfo.manufYear);
	printf("displaySize %s\n", TVInfo.displaySize);
	} else
		printf("failed\n");
		
	HdmiSupportedDataSpace();
	return 0;
}
#endif
