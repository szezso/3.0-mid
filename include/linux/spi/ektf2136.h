#ifndef LINUX_SPI_EKTF2136_H
#define LINUX_SPI_EKTF2136_H

#include <linux/gpio.h>

struct ektf2136_platform_data {
	int	gpio;		/* The GPIO pin for the device */
};

struct elan_sr_hdr {
	u32 frame_num;
	u32 frame_size;
	u32 timestamp;
};

#define SRIOCWM _IOW('r', 1, int)		/* SR mode on/off */
#define SRIOCRM _IOR('r', 2, int)		/* Read SR mode */
#define SRIOCRCC _IOR('r', 3, int)		/* Read cell column number */
#define SRIOCRCR _IOR('r', 4, int)		/* Read cell row number */
#define SRIOCRX _IOR('r', 5, int)		/* Read panel resolution in X */
#define SRIOCRY _IOR('r', 6, int)		/* Read panel resolution in Y */
#define SRIOCRV _IOR('r', 7, int)		/* Read vendor ID */
#define SRIOCRP _IOR('r', 8, int)		/* Read product ID */

#define DEV_IOCTLID 0xD0

#define IOCTL_MAJOR_FW_VER  _IOR(DEV_IOCTLID, 1, int)
#define IOCTL_MINOR_FW_VER  _IOR(DEV_IOCTLID, 2, int)
#define IOCTL_MAJOR_HW_ID   _IOR(DEV_IOCTLID, 3, int)
#define IOCTL_MINOR_HW_ID   _IOR(DEV_IOCTLID, 4, int)
#define IOCTL_MAJOR_BC_VER  _IOR(DEV_IOCTLID, 5, int)
#define IOCTL_MINOR_BC_VER  _IOR(DEV_IOCTLID, 6, int)

#endif
