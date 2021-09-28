/*
 * Definitions for akm8963 compass chip.
 */
#ifndef AKM8963_H
#define AKM8963_H

#include <linux/ioctl.h>

#define AKM8963_I2C_NAME "akm8963"

#define SENSOR_DATA_SIZE	8
#define YPR_DATA_SIZE		12
#define RWBUF_SIZE		16

#define ACC_DATA_FLAG		0
#define MAG_DATA_FLAG		1
#define ORI_DATA_FLAG		2
#define AKM_NUM_SENSORS		3

#define ACC_DATA_READY		(1<<(ACC_DATA_FLAG))
#define MAG_DATA_READY		(1<<(MAG_DATA_FLAG))
#define ORI_DATA_READY		(1<<(ORI_DATA_FLAG))

/*! \name AK8963 constant definition
 \anchor AK8963_Def
 Constant definitions of the AK8963.*/
#define AK8963_MEASUREMENT_TIME_US	10000

/*! \name AK8963 operation mode
 \anchor AK8963_Mode
 Defines an operation mode of the AK8963.*/
/*! @{*/
#define AK8963_MODE_SNG_MEASURE	0x01
#define	AK8963_MODE_SELF_TEST	0x08
#define	AK8963_MODE_FUSE_ACCESS	0x0F
#define	AK8963_MODE_POWERDOWN	0x00

/*! @}*/

/*! \name AK8963 register address
\anchor AK8963_REG
Defines a register address of the AK8963.*/
/*! @{*/
#define AK8963_REG_WIA		0x00
#define AK8963_REG_INFO		0x01
#define AK8963_REG_ST1		0x02
#define AK8963_REG_HXL		0x03
#define AK8963_REG_HXH		0x04
#define AK8963_REG_HYL		0x05
#define AK8963_REG_HYH		0x06
#define AK8963_REG_HZL		0x07
#define AK8963_REG_HZH		0x08
#define AK8963_REG_ST2		0x09
#define AK8963_REG_CNTL1	0x0A
#define AK8963_REG_CNTL2	0x0B
#define AK8963_REG_ASTC		0x0C
#define AK8963_REG_TS1		0x0D
#define AK8963_REG_TS2		0x0E
#define AK8963_REG_I2CDIS	0x0F
/*! @}*/

/*! \name AK8963 fuse-rom address
\anchor AK8963_FUSE
Defines a read-only address of the fuse ROM of the AK8963.*/
/*! @{*/
#define AK8963_FUSE_ASAX	0x10
#define AK8963_FUSE_ASAY	0x11
#define AK8963_FUSE_ASAZ	0x12
/*! @}*/

struct akm_platform_data {
	short m_layout[4][3][3];
	char project_name[64];
	char layout;
	char outbit;
	int gpio_DRDY;
	int gpio_RST;
};


#define COMPASS_IOCTL_MAGIC                   'c'
#define SENSOR_DATA_SIZE		8	/* Rx buffer size, i.e from ST1 to ST2 */

/* IOCTLs for AKM library */
#define ECS_IOCTL_WRITE                 _IOW(COMPASS_IOCTL_MAGIC, 0x01, char*)
#define ECS_IOCTL_READ                  _IOWR(COMPASS_IOCTL_MAGIC, 0x02, char*)
#define ECS_IOCTL_RESET      	        _IO(COMPASS_IOCTL_MAGIC, 0x03) /* NOT used in AK8975 */
#define ECS_IOCTL_SET_MODE              _IOW(COMPASS_IOCTL_MAGIC, 0x04, short)
#define ECS_IOCTL_GETDATA               _IOR(COMPASS_IOCTL_MAGIC, 0x05, char[SENSOR_DATA_SIZE])
#define ECS_IOCTL_SET_YPR               _IOW(COMPASS_IOCTL_MAGIC, 0x06, short[12])
#define ECS_IOCTL_GET_OPEN_STATUS       _IOR(COMPASS_IOCTL_MAGIC, 0x07, int)
#define ECS_IOCTL_GET_CLOSE_STATUS      _IOR(COMPASS_IOCTL_MAGIC, 0x08, int)
#define ECS_IOCTL_GET_LAYOUT        	_IOR(COMPASS_IOCTL_MAGIC, 0x09, char)
#define ECS_IOCTL_GET_ACCEL         	_IOR(COMPASS_IOCTL_MAGIC, 0x0A, short[3])
#define ECS_IOCTL_GET_OUTBIT        	_IOR(COMPASS_IOCTL_MAGIC, 0x0B, char)
#define ECS_IOCTL_GET_DELAY             _IOR(COMPASS_IOCTL_MAGIC, 0x30, short)
#define ECS_IOCTL_GET_PROJECT_NAME      _IOR(COMPASS_IOCTL_MAGIC, 0x0D, char[64])
#define ECS_IOCTL_GET_MATRIX            _IOR(COMPASS_IOCTL_MAGIC, 0x0E, short [4][3][3])
#define ECS_IOCTL_GET_PLATFORM_DATA     _IOR(COMPASS_IOCTL_MAGIC, 0x0E, struct akm_platform_data)

#endif

