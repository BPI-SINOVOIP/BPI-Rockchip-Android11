#define LOG_TAG "Sensors"

#include <hardware/sensors.h>
#include <fcntl.h>
#include <errno.h>
#include <dirent.h>
#include <math.h>
#include <poll.h>
#include <pthread.h>
#include <sys/cdefs.h>
#include <sys/types.h>

#include <linux/input.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "gsensor_test.h"
#include "common.h"
#include "test_case.h"
#include "language.h"

struct sysfs_attrbs {
       char *chip_enable;
       char *power_state;
       char *dmp_firmware;
       char *firmware_loaded;
       char *dmp_on;
       char *dmp_int_on;
       char *dmp_event_int_on;
       char *dmp_output_rate;
       char *tap_on;
       char *key;
       char *self_test;
       char *temperature;

       char *gyro_enable;
       char *gyro_fifo_rate;
       char *gyro_fsr;
       char *gyro_orient;
       char *gyro_x_fifo_enable;
       char *gyro_y_fifo_enable;
       char *gyro_z_fifo_enable;

       char *accel_enable;
       char *accel_fifo_rate;
       char *accel_fsr;
       char *accel_bias;
       char *accel_orient;
       char *accel_x_fifo_enable;
       char *accel_y_fifo_enable;
       char *accel_z_fifo_enable;

       char *quaternion_on;
       char *in_quat_r_en;
       char *in_quat_x_en;
       char *in_quat_y_en;
       char *in_quat_z_en;

       char *in_timestamp_en;
       char *trigger_name;
       char *current_trigger;
       char *buffer_length;

       char *display_orientation_on;
       char *event_display_orientation;

	   char *compass_enable;
       char *compass_x_fifo_enable;
       char *compass_y_fifo_enable;
       char *compass_z_fifo_enable;
       char *compass_rate;
       char *compass_scale;
       char *compass_orient;
} ;

#define EVENT_TYPE_ACCEL_X          ABS_X
#define EVENT_TYPE_ACCEL_Y          ABS_Z
#define EVENT_TYPE_ACCEL_Z          ABS_Y
#define ACCELERATION_RATIO_ANDROID_TO_HW        (9.80665f / 1000000)



//static long lg_x = 0;
//static long lg_y = 0;
//static long lg_z = 0;

static long g_x = 0;
static long g_y = 0;
static long g_z = 0;

static long c_x = 0;
static long c_y = 0;
static long c_z = 0;



#define MAX_CHIP_ID_LEN			(20)
#define MAX_SYSFS_NAME_LEN		(100)
#define IIO_BUFFER_LENGTH		(480)


#define IIO_DEVICE_NODE			"/dev/iio:device0"

static char* sysfs_names_ptr;

#define MAX_SYSFS_ATTRB (sizeof(struct sysfs_attrbs) / sizeof(char*))

struct sysfs_attrbs mpu;
int iio_fd = -1;

char mIIOBuffer[(16 + 8 * 3 + 8) * IIO_BUFFER_LENGTH];

static int int_flag = 0;


static int inv_init_sysfs_attributes(void)
{
    unsigned char i = 0;
    char sysfs_path[MAX_SYSFS_NAME_LEN];
    char iio_trigger_path[MAX_SYSFS_NAME_LEN], tbuf[2];
    char *sptr;
    char **dptr;
    int num;

    memset(sysfs_path, 0, sizeof(sysfs_path));
    memset(iio_trigger_path, 0, sizeof(iio_trigger_path));

    sysfs_names_ptr =
            (char*)malloc(sizeof(char[MAX_SYSFS_ATTRB][MAX_SYSFS_NAME_LEN]));
    sptr = sysfs_names_ptr;
    if (sptr != NULL) {
        dptr = (char**)&mpu;
        do {
            *dptr++ = sptr;
            memset(sptr, 0, sizeof(sptr));
            sptr += sizeof(char[MAX_SYSFS_NAME_LEN]);
        } while (++i < MAX_SYSFS_ATTRB);
    } else {
        printf("%s line=%d:couldn't alloc mem for sysfs paths \n", __FUNCTION__, __LINE__);
        return -1;
    }

    // get proper (in absolute/relative) IIO path & build MPU's sysfs paths
    //inv_get_sysfs_path(sysfs_path);
    //inv_get_iio_trigger_path(iio_trigger_path);
	strcpy(sysfs_path,"/sys/bus/iio/devices/iio:device0");
	strcpy(iio_trigger_path,"/sys/bus/iio/devices/trigger0");

    if (strcmp(sysfs_path, "") == 0  || strcmp(iio_trigger_path, "") == 0)
        return 0;

    sprintf(mpu.key, "%s%s", sysfs_path, "/key");
    sprintf(mpu.chip_enable, "%s%s", sysfs_path, "/buffer/enable");
    sprintf(mpu.buffer_length, "%s%s", sysfs_path, "/buffer/length");
    sprintf(mpu.power_state, "%s%s", sysfs_path, "/power_state");
    sprintf(mpu.in_timestamp_en, "%s%s", sysfs_path, 
            "/scan_elements/in_timestamp_en");
    sprintf(mpu.trigger_name, "%s%s", iio_trigger_path, "/name");
    sprintf(mpu.current_trigger, "%s%s", sysfs_path, "/trigger/current_trigger");

    sprintf(mpu.dmp_firmware, "%s%s", sysfs_path,"/dmp_firmware");
    sprintf(mpu.firmware_loaded,"%s%s", sysfs_path, "/firmware_loaded");
    sprintf(mpu.dmp_on,"%s%s", sysfs_path, "/dmp_on");
    sprintf(mpu.dmp_int_on,"%s%s", sysfs_path, "/dmp_int_on");
    sprintf(mpu.dmp_event_int_on,"%s%s", sysfs_path, "/dmp_event_int_on");
    sprintf(mpu.dmp_output_rate,"%s%s", sysfs_path, "/dmp_output_rate");
    sprintf(mpu.tap_on, "%s%s", sysfs_path, "/tap_on");

    sprintf(mpu.self_test, "%s%s", sysfs_path, "/self_test");

    sprintf(mpu.temperature, "%s%s", sysfs_path, "/temperature");
    sprintf(mpu.gyro_enable, "%s%s", sysfs_path, "/gyro_enable");
    sprintf(mpu.gyro_fifo_rate, "%s%s", sysfs_path, "/sampling_frequency");
    sprintf(mpu.gyro_orient, "%s%s", sysfs_path, "/gyro_matrix");
    sprintf(mpu.gyro_x_fifo_enable, "%s%s", sysfs_path, 
            "/scan_elements/in_anglvel_x_en");
    sprintf(mpu.gyro_y_fifo_enable, "%s%s", sysfs_path, 
            "/scan_elements/in_anglvel_y_en");
    sprintf(mpu.gyro_z_fifo_enable, "%s%s", sysfs_path, 
            "/scan_elements/in_anglvel_z_en");
    sprintf(mpu.gyro_fsr, "%s%s", sysfs_path, "/in_anglvel_scale");

    sprintf(mpu.accel_enable, "%s%s", sysfs_path, "/accl_enable");
    sprintf(mpu.accel_fifo_rate, "%s%s", sysfs_path, "/sampling_frequency");
    sprintf(mpu.accel_orient, "%s%s", sysfs_path, "/accl_matrix");

#ifndef THIRD_PARTY_ACCEL
    sprintf(mpu.accel_fsr, "%s%s", sysfs_path, "/in_accel_scale");
    sprintf(mpu.accel_bias, "%s%s", sysfs_path, "/accl_bias");
#endif

    sprintf(mpu.accel_x_fifo_enable, "%s%s", sysfs_path, 
            "/scan_elements/in_accel_x_en");
    sprintf(mpu.accel_y_fifo_enable, "%s%s", sysfs_path, 
            "/scan_elements/in_accel_y_en");
    sprintf(mpu.accel_z_fifo_enable, "%s%s", sysfs_path, 
            "/scan_elements/in_accel_z_en");

    sprintf(mpu.quaternion_on, "%s%s", sysfs_path, "/quaternion_on");
    sprintf(mpu.in_quat_r_en, "%s%s", sysfs_path, 
            "/scan_elements/in_quaternion_r_en");
    sprintf(mpu.in_quat_x_en, "%s%s", sysfs_path, 
            "/scan_elements/in_quaternion_x_en");
    sprintf(mpu.in_quat_y_en, "%s%s", sysfs_path, 
            "/scan_elements/in_quaternion_y_en");
    sprintf(mpu.in_quat_z_en, "%s%s", sysfs_path, 
            "/scan_elements/in_quaternion_z_en");

    sprintf(mpu.display_orientation_on, "%s%s", sysfs_path, 
            "/display_orientation_on");
    sprintf(mpu.event_display_orientation, "%s%s", sysfs_path, 
            "/event_display_orientation");

	sprintf(mpu.compass_enable, "%s%s", sysfs_path, "/compass_enable");
    sprintf(mpu.compass_x_fifo_enable, "%s%s", sysfs_path, "/scan_elements/in_magn_x_en");
    sprintf(mpu.compass_y_fifo_enable, "%s%s", sysfs_path, "/scan_elements/in_magn_y_en");
    sprintf(mpu.compass_z_fifo_enable, "%s%s", sysfs_path, "/scan_elements/in_magn_z_en");
    sprintf(mpu.compass_rate, "%s%s", sysfs_path, "/sampling_frequency");
    sprintf(mpu.compass_scale, "%s%s", sysfs_path, "/in_magn_scale");
    sprintf(mpu.compass_orient, "%s%s", sysfs_path, "/compass_matrix");

    return 0;
}



static void enable_iio_sysfs()
{


    char iio_trigger_name[MAX_CHIP_ID_LEN];
    FILE *tempFp = NULL;


    // fopen()/open() would both be okay for sysfs access
    // developers could choose what they want
    // with fopen(), benefits from fprintf()/fscanf() would be available
    tempFp = fopen(mpu.in_timestamp_en, "w");
    if (tempFp == NULL) {
        printf("%s line=%d:could not open timestamp enable \n", __FUNCTION__, __LINE__);
    } else {
        if(fprintf(tempFp, "%d", 1) < 0) {
            printf("%s line=%d:could not enable timestamp \n", __FUNCTION__, __LINE__);
        }
        fclose(tempFp);
    }


    tempFp = fopen(mpu.trigger_name, "r");
    if (tempFp == NULL) {
        printf("%s line=%d :could not open trigger name \n", __FUNCTION__, __LINE__);
    } else {
        if (fscanf(tempFp, "%s", iio_trigger_name) < 0) {
            printf("%s line=%d:could not read trigger name \n", __FUNCTION__, __LINE__);
        }
        fclose(tempFp);
    }


    tempFp = fopen(mpu.current_trigger, "w");
    if (tempFp == NULL) {
        printf("%s line=%d:could not open current trigger \n", __FUNCTION__, __LINE__);
    } else {
        if (fprintf(tempFp, "%s", iio_trigger_name) < 0) {
            printf("%s line=%d :could not write current trigger \n", __FUNCTION__, __LINE__);
        }
        fclose(tempFp);
    }


    tempFp = fopen(mpu.buffer_length, "w");
    if (tempFp == NULL) {
        printf("%s line=%d:could not open buffer length \n", __FUNCTION__, __LINE__);
    } else {
        if (fprintf(tempFp, "%d", IIO_BUFFER_LENGTH) < 0) {
            printf("%s line=%d:could not write buffer length \n", __FUNCTION__, __LINE__);
        }
        fclose(tempFp);
    }

   
    iio_fd = open(IIO_DEVICE_NODE, O_RDONLY);
    if (iio_fd < 0) {
        printf("%s line=%d:could not open iio device node \n", __FUNCTION__, __LINE__);
    } else {
        printf("%s line=%d:iio iio_fd opened : %d \n", __FUNCTION__, __LINE__,  iio_fd);
    }
}



/* This one DOES NOT close FDs for you */
static int read_attribute_sensor(int fd, char* data, unsigned int size)
{
    int count = 0;
    if (fd > 0) {
        count = pread(fd, data, size, 0);
        if(count < 1) {
            printf("%s line=%d:read fails with error code=%d", __FUNCTION__, __LINE__, count);
        }
    }
    return count;
}


static int enable_sysfs_sensor(int fd, int en)
{
    int nb;
    int err = 0;

    char c = en ? '1' : '0';
    nb = write(fd, &c, 1);

    if (nb <= 0) {
        err = errno;
        printf("%s line=%d:enable_sysfs_sensor - write %c returned %d (%s / %d) \n", __FUNCTION__, __LINE__, 
             c, nb, strerror(err), err);
    }
    close(fd);


    return -err;
}



static int onPower(int en)
{
    int res = 0;
    char buf[sizeof(int)+1];
    int count, curr_power_state;


    int tempFd = open(mpu.power_state, O_RDWR);
    res = errno;
    if(tempFd < 0){
        printf("%s line=%d :Open of %s failed with '%s' (%d)\n", __FUNCTION__, __LINE__, 
             mpu.power_state, strerror(res), res);
    } else {
        // check and set new power state
        count = read_attribute_sensor(tempFd, buf, sizeof(buf));
        if(count < 1) {
            printf("%s line=%d:Error reading power state \n", __FUNCTION__, __LINE__);
            // will set power_state anyway
            curr_power_state= -1;
        } else {
            sscanf(buf, "%d", &curr_power_state);
        }

        if (en!=curr_power_state) {
            if((res=enable_sysfs_sensor(tempFd, en)) < 0) {
                printf("%s line=%d:Couldn't write power state \n", __FUNCTION__, __LINE__);
            }
        } else {
            printf("%s line=%d:Power state already enable/disable curr=%d new=%d", __FUNCTION__, __LINE__,
                    curr_power_state, en);
            close(tempFd);
        }
    }
    return res;
}



static int masterEnable(int en)
{

    int res = 0;
;
    int tempFd = open(mpu.chip_enable, O_RDWR);
    res = errno;
    if(tempFd < 0){
        printf("%s line=%d:open of %s failed with '%s' (%d)", __FUNCTION__, __LINE__, 
             mpu.chip_enable, strerror(res), res);
        return res;
    }
    res = enable_sysfs_sensor(tempFd, en);
    return res;
}


static int enableAccel(int en)
{
    int res;

    /* need to also turn on/off the master enable */
    printf("%s line=%d :sysfs:echo %d > %s \n", __FUNCTION__, __LINE__, en, mpu.accel_enable);
    int tempFd = open(mpu.accel_enable, O_RDWR);
    res = errno;
    if (tempFd > 0) {
        res = enable_sysfs_sensor(tempFd, en);
    } else {
        printf("%s line=%d :open of %s failed with '%s' (%d) \n", __FUNCTION__, __LINE__,
             mpu.accel_enable, strerror(res), res);
    }

    if (!en) {
        printf("%s line=%d :MPL:inv_accel_was_turned_off \n", __FUNCTION__, __LINE__);
        //inv_accel_was_turned_off();
    } else {
        tempFd = open(mpu.accel_x_fifo_enable, O_RDWR);
        res = errno;
        if (tempFd > 0) {
            res = enable_sysfs_sensor(tempFd, en);
        } else {
            printf("%s line=%d :open of %s failed with '%s' (%d) \n", __FUNCTION__, __LINE__,
                 mpu.accel_x_fifo_enable, strerror(res), res);
        }

        tempFd = open(mpu.accel_y_fifo_enable, O_RDWR);
        res = errno;
        if (tempFd > 0) {
            res = enable_sysfs_sensor(tempFd, en);
        } else {
            printf("%s line=%d :open of %s failed with '%s' (%d) \n", __FUNCTION__, __LINE__, 
                 mpu.accel_y_fifo_enable, strerror(res), res);
        }


        tempFd = open(mpu.accel_z_fifo_enable, O_RDWR);
        res = errno;
        if (tempFd > 0) {
            res = enable_sysfs_sensor(tempFd, en);
        } else {
            printf("%s line=%d :open of %s failed with '%s' (%d) \n", __FUNCTION__, __LINE__,
                 mpu.accel_z_fifo_enable, strerror(res), res);
        }
    }

    return res;
}



static int disableGyro(/*int en*/)
{


    int res = 0;

    /* need to also turn on/off the master enable */
;
    int tempFd = open(mpu.gyro_enable, O_RDWR);
    res = errno;
    if (tempFd > 0) {
        res = enable_sysfs_sensor(tempFd, 0);
    } else {
        printf("%s line=%d :open of %s failed with '%s' (%d) \n", __FUNCTION__, __LINE__,
             mpu.gyro_enable, strerror(res), res);
    }

    
    return res;
}



static int enableCompass(int en) 
{
    int tempFd;
    int res = 0;


    tempFd = open(mpu.compass_enable, O_RDWR);
    if(tempFd < 0) {
        printf("%s line=%d open of %s failed with '%s' (%d) \n", __FUNCTION__, __LINE__,
             mpu.compass_enable, strerror(res), res);
        return res;
    }
    res = enable_sysfs_sensor(tempFd, en);
	if(res < 0)
	{
		printf("%s line=%d enable compass failed \n", __FUNCTION__, __LINE__);
	}
    close(tempFd);

    if (en) {
        tempFd = open(mpu.compass_x_fifo_enable, O_RDWR);
        if (tempFd > 0) {
            res = enable_sysfs_sensor(tempFd, en);
            close(tempFd);
        } else {
            printf("%s line=%d:open of %s failed with '%s' (%d) \n", __FUNCTION__, __LINE__,
                 mpu.compass_x_fifo_enable, strerror(res), res);
        }


        tempFd = open(mpu.compass_y_fifo_enable, O_RDWR);
        if (tempFd > 0) {
            res = enable_sysfs_sensor(tempFd, en);
            close(tempFd);
        } else {
            printf("%s line=%d:open of %s failed with '%s' (%d) \n",  __FUNCTION__, __LINE__, 
                 mpu.compass_y_fifo_enable, strerror(res), res);
        }


        tempFd = open(mpu.compass_z_fifo_enable, O_RDWR);
        res = errno;
        if (tempFd > 0) {
            res = enable_sysfs_sensor(tempFd, en);
            close(tempFd);
        } else {
            printf("%s line=%d :open of %s failed with '%s' (%d) \n", __FUNCTION__, __LINE__, 
                 mpu.compass_z_fifo_enable, strerror(res), res);
        }
    }

    return res;
}



static int mpu_int(void)
{
	int result= 0;
	
	printf("%s line=%d \n", __FUNCTION__, __LINE__);

	
	if(int_flag)
	{
		printf("%s line=%d int_flag=%d already call.\n", __FUNCTION__, __LINE__, int_flag);
		return 0;
	}

	memset(mIIOBuffer, 0, sizeof(mIIOBuffer));

	
	inv_init_sysfs_attributes();



	enable_iio_sysfs();



    /* turn on power state */
    onPower(1);
    /* reset driver master enable */
    masterEnable(0);
	disableGyro(0);
	enableAccel(0);
	enableCompass(0);
	onPower(0);


	onPower(1);
	masterEnable(0);
	result = enableAccel(1);
	enableCompass(1);

	masterEnable(1);//shall open
	printf("%s line=%d result=%d \n", __FUNCTION__, __LINE__, result);

	if(result < 0)
		return -1;

	int_flag = 1;
	return 0;
}
 
#if 0
static int build_event(void)
{
	int nbyte;
	int i, nb;
	int sensors;
	ssize_t rsize;

	char *rdata = mIIOBuffer;

	sensors = 1;
	
	nbyte= /*(8 * sensors + 8) * 1*/16;

	memset(mIIOBuffer, 0, sizeof(mIIOBuffer));

	//printf("%s line=%d iio_fd=%d nbyte=%d \n", __FUNCTION__, __LINE__, iio_fd, nbyte);
	rsize = read(iio_fd, rdata, nbyte);
	//printf("%s line=%d iio_fd=%d, rsize=%d \n", __FUNCTION__, __LINE__, iio_fd, rsize);

	g_x = *((short *) (rdata + 0 ));
	g_y = *((short *) (rdata + 2 ));
	g_z = *((short *) (rdata + 4 ));

	//g_x = lg_x*ACCELERATION_RATIO_ANDROID_TO_HW;
	//g_y = lg_y*ACCELERATION_RATIO_ANDROID_TO_HW;
	//g_z = lg_z*ACCELERATION_RATIO_ANDROID_TO_HW;

	//printf("%s line=%d x=%d y=%d z=%d \n", __FUNCTION__, __LINE__, g_x, g_y, g_z);

	return 0;
}
#else
static int build_event(void)
{
	int nbyte;
	int i, nb;
	int sensors;
	ssize_t rsize;

	char *rdata = mIIOBuffer;

	sensors = 2;
	
	nbyte= (8 * sensors + 8) * 1;

	memset(mIIOBuffer, 0, sizeof(mIIOBuffer));

	//printf("%s line=%d iio_fd=%d nbyte=%d \n", __FUNCTION__, __LINE__, iio_fd, nbyte);
	rsize = read(iio_fd, rdata, nbyte);
	//printf("%s line=%d iio_fd=%d, rsize=%d \n", __FUNCTION__, __LINE__, iio_fd, rsize);

	g_x = *((short *) (rdata + 0 ));
	g_y = *((short *) (rdata + 2 ));
	g_z = *((short *) (rdata + 4 ));

	c_x = *((short *) (rdata + 0 + 6));
	c_y = *((short *) (rdata + 2 + 6));
	c_z = *((short *) (rdata + 4 + 6));

	//g_x = lg_x*ACCELERATION_RATIO_ANDROID_TO_HW;
	//g_y = lg_y*ACCELERATION_RATIO_ANDROID_TO_HW;
	//g_z = lg_z*ACCELERATION_RATIO_ANDROID_TO_HW;

	//printf("%s line=%d x=%d y=%d z=%d \n", __FUNCTION__, __LINE__, g_x, g_y, g_z);

	return 0;
}

#endif

 void* gsensor_test_mpu(void *argv)
 {

	int ret;
	int fd;
 	//struct gsensor_msg *g_msg =  (struct gsensor_msg *)malloc(sizeof(struct gsensor_msg));
        struct gsensor_msg g_msg;
	struct testcase_info *tc_info = (struct testcase_info*)argv;
		
	/*remind ddr test*/
	if(tc_info->y <= 0)
		tc_info->y  = get_cur_print_y();	

	g_msg.y = tc_info->y;
	ui_print_xy_rgba(0,g_msg.y,255,255,0,255,"%s:[%s..] \n",PCBA_GSENSOR,PCBA_TESTING);
	tc_info->result = 0;

    if (mpu_int() < 0)
	{
 		ui_print_xy_rgba(0,g_msg.y,255,0,0,255,"%s:[%s]\n",PCBA_GSENSOR,PCBA_FAILED);
		g_msg.result = -1;
		tc_info->result = -1;
		int_flag = 0;
		if(iio_fd > 0)
	    {
	    	close(iio_fd);
	    }

		if(sysfs_names_ptr)
			free(sysfs_names_ptr);
		return argv;
 	}
        
 	if(iio_fd < 0)
 	{
 		ui_print_xy_rgba(0,g_msg.y,255,0,0,255,"%s:[%s]\n",PCBA_GSENSOR,PCBA_FAILED);
		g_msg.result = -1;
		tc_info->result = -1;
		int_flag = 0;
		if(iio_fd > 0)
	    {
	    	close(iio_fd);
	    }

		if(sysfs_names_ptr)
			free(sysfs_names_ptr);
		return argv;
 	}
	
	for(;;)
	{
		//printf("%s line=%d \n", __FUNCTION__, __LINE__);
		build_event();
		//printf("%s line=%d \n", __FUNCTION__, __LINE__);
		//readEvents(fd);
		//ui_print_xy_rgba(0,g_msg.y,0,255,0,255,"%s:[%s] { %2d,%2d,%2d }\n",PCBA_GSENSOR,PCBA_SECCESS,(int)g_x,(int)g_y,(int)g_z);
		ui_display_sync(0,g_msg.y,0,255,0,255,"%s:[%s] { %2d,%2d,%2d }\n",PCBA_GSENSOR,PCBA_SECCESS,(int)g_x,(int)g_y,(int)g_z);
		//ui_print_xy_rgba(0,g_msg->y,0,0,255,255,"gsensor x:%f y:%f z:%f\n",g_x,g_y,g_z);
		usleep(100000);
	}

    //close(fd);
    //close(fd_dev);
    if(iio_fd > 0)
    {
    	close(iio_fd);
    }

	if(sysfs_names_ptr)
		free(sysfs_names_ptr);

    ui_print_xy_rgba(0,g_msg.y,0,255,0,255,"%s:[%s]\n",PCBA_GSENSOR,PCBA_SECCESS);
	tc_info->result = 0;
	int_flag = 0;
	return argv;
 }


 void* compass_test_mpu(void *argv)
 {

	int ret;
	int fd;
 	//struct gsensor_msg *g_msg =  (struct gsensor_msg *)malloc(sizeof(struct gsensor_msg));
        struct gsensor_msg g_msg;
	struct testcase_info *tc_info = (struct testcase_info*)argv;
		
	/*remind ddr test*/
	if(tc_info->y <= 0)
		tc_info->y  = get_cur_print_y();	

	g_msg.y = tc_info->y;
	ui_print_xy_rgba(0,g_msg.y,255,255,0,255,"%s:[%s..] \n",PCBA_COMPASS,PCBA_TESTING);
	tc_info->result = 0;

	printf("%s line=%d MPU compass \n", __FUNCTION__, __LINE__);

    if (mpu_int() < 0)
	{
 		ui_print_xy_rgba(0,g_msg.y,255,0,0,255,"%s:[%s]\n",PCBA_COMPASS,PCBA_FAILED);
		g_msg.result = -1;
		tc_info->result = -1;
		int_flag = 0;
		if(iio_fd > 0)
	    {
	    	close(iio_fd);
	    }

		if(sysfs_names_ptr)
			free(sysfs_names_ptr);
		return argv;
 	}
        
 	if(iio_fd < 0)
 	{
 		ui_print_xy_rgba(0,g_msg.y,255,0,0,255,"%s:[%s]\n",PCBA_COMPASS,PCBA_FAILED);
		g_msg.result = -1;
		tc_info->result = -1;
		int_flag = 0;
		if(iio_fd > 0)
	    {
	    	close(iio_fd);
	    }

		if(sysfs_names_ptr)
			free(sysfs_names_ptr);
		return argv;
 	}
	
	for(;;)
	{
		//printf("%s line=%d \n", __FUNCTION__, __LINE__);
		build_event();
		//printf("%s line=%d \n", __FUNCTION__, __LINE__);
		//readEvents(fd);
		//ui_print_xy_rgba(0,g_msg.y,0,255,0,255,"%s:[%s] { %2d,%2d,%2d }\n",PCBA_GSENSOR,PCBA_SECCESS,(int)g_x,(int)g_y,(int)g_z);
		ui_display_sync(0,g_msg.y,0,255,0,255,"%s:[%s] { %2d,%2d,%2d }\n",PCBA_COMPASS,PCBA_SECCESS,(int)c_x,(int)c_y,(int)c_z);
		//ui_print_xy_rgba(0,g_msg->y,0,0,255,255,"gsensor x:%f y:%f z:%f\n",g_x,g_y,g_z);
		usleep(100000);
	}

    //close(fd);
    //close(fd_dev);
    if(iio_fd > 0)
    {
    	close(iio_fd);
    }

	if(sysfs_names_ptr)
		free(sysfs_names_ptr);

    ui_print_xy_rgba(0,g_msg.y,0,255,0,255,"%s:[%s]\n",PCBA_COMPASS,PCBA_SECCESS);
	tc_info->result = 0;
	int_flag = 0;
	return argv;
 }

 
