/*
 *  ELAN touchscreen driver
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 */

#define pr_fmt(fmt)	"ektf2136_spi:" fmt

#include <linux/sched.h>
#include <linux/poll.h>
#include <linux/input.h>
#include <linux/device.h>
#include <linux/module.h>
#include <linux/miscdevice.h>
#include <linux/init.h>
#include <linux/jiffies.h>
#include <linux/interrupt.h>
#include <linux/spi/spi.h>
#include <linux/delay.h>
#include <linux/hrtimer.h>
#include <linux/gpio.h>
#include <linux/slab.h>
#include <linux/gpio.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/kfifo.h>
#include <linux/spi/ektf2136.h>
#include <linux/pm_runtime.h>

#define DRV_NAME	"ektf2136_spi"
#define DRIVER_VERSION	"v3.0.2"

#define DRV_MA_VER 3
#define DRV_MI_VER 0
#define DRV_SUB_MI_VER 2

static const char ELAN_TS_NAME[]	= "ektf2136_spi";

#define STA_NONINIT         0x00000001
#define STA_INIT            0x00000002
#define STA_INIT2           0x00000004
#define STA_PROBED          0x00000008
#define STA_ERR_HELLO_PKT   0x00000010
#define STA_USE_IRQ         0x00000020
#define STA_USE_SR          0x00000040

/* Firmware protocol status flag */
#define PRO_SPI_WRT_CMD_SYNC	0x00000001
#define PRO_HID_MOD_CHECKSUM	0x00000002

/* Convert from rows or columns into resolution */
#define ELAN_TS_RESOLUTION(n)		((n - 1) * 64)

#define IDX_PACKET_SIZE_RAW		32
#define IDX_SR_CAPABILITY_SIZE		26
#define IDX_SCAN_END_SIZE		8
/* 10 fingers packet length */
#define IDX_PACKET_SIZE_XY		34
#define IDX_PACKET_SIZE_XY_CHECKSUM	35

#define IDX_MAX_PACKET_SIZE		35

#define FINGER_ID			1

#define FIFO_SIZE			30000
#define MAX_REPORT_SIZE			4096

enum {
	idx_coordinate_packet_5_finger  = 0x5d,
	idx_coordinate_packet_10_finger = 0x62,
	SOFT_RESET			= 0x77,
};

static const char hello_packet[4] = { 0x55, 0x55, 0x55, 0x55 };

enum {
	idx_finger_state		= 2,
	idx_sr_mode_length		= 7,
};

struct elan_capability {
	u8 magic1;
	u8 magic2;
	u8 fw_ver_major;
	u8 fw_ver_minor;
	u8 columns;
	u8 rows;
	u8 pitch;		/* in 0.1mm */
	u8 flags;
#define ECAP_POSITION_BOTTOM	0x01
#define ECAP_DATA_SIGNED	0x02
#define ECAP_ORIGIN_RIGHT	0x04
#define ECAP_ORIGIN_BOTTOM	0x08
#define ECAP_LCD_ORIGIN_BOTTOM	0x10
#define ECAP_LCD_ORIGIN_RIGHT	0x20

	u8 frame_rate;
	u8 noise_comp;
	u32 left;		/* These are actually floats from the hw */
	u32 right;
	u32 top;
	u32 bottom;
};

struct elan_data {
	int intr_gpio;
	int use_irq;
	u8 major_fw_version;
	u8 minor_fw_version;
	u8 major_bc_version;
	u8 minor_bc_version;
	u8 major_hw_id;
	u8 minor_hw_id;
	int rows;			/* Panel geometry for input layer */
	int cols;
	u8 power_state;			/* Power state 0:sleep 1:active */
	u8 user_power;			/* Power forced on if 1 */

	struct hrtimer timer;
	struct work_struct work;
	struct spi_device *spi;
	struct input_dev *input;
	struct mutex mutex;		/* Protects SPI accesses to device */
	struct mutex sysfs_mutex;	/* Protects accesses to device via
					   sysfs */
	unsigned long busy;		/* Lock openers */
	struct miscdevice firmware;	/* Firmware device */
	bool fw_enabled;		/* True if firmware device enabled */
	unsigned int protocol;		/* Protocol stats for firmware */
	unsigned int rx_size;		/* Read size in use */
	unsigned int input_rx_size;	/* Input mode read size */
	bool wait_sync_int;		/* True if drv is waiting for device
					   to pull gpio low to sync */
	unsigned int sr_mode;		/* SR receive mode or 0 for normal */
#define SR_WAIT_CAP		1	/* Waiting capability packet */
#define SR_PACKET		2	/* SR packet mode */

	/* SR state machine */
	unsigned int report_size;	/* Size of report being processed */
	bool scan_end;			/* True if scan end seen */
	unsigned int scan_end_num;	/* Count of blocks expected */
	unsigned int report_count;	/* Blocks let to process */
	void *scan_report;		/* Data buffer for scan being
					   accumulated */
	struct elan_capability capability;	/* Capability block from hw */

	/* SR fifo and processing */
	struct kfifo fifo;		/* FIFO queue for data */
	struct mutex fifo_mutex;	/* Serialize operations around FIFO */
	unsigned int poll_time;		/* Computed from capability data */
	wait_queue_head_t wait;
	u8 *buffer;			/* Working buffer for frames */
	u32 frame_num;			/* Cycling frame counter */
	bool sr_enabled;		/* True if sr device enabled */
	struct miscdevice sr_dev;	/* SR mode device */
	spinlock_t rx_kfifo_lock;

	/* Add for TS driver debug */
	unsigned int status;
	long int irq_received;
	long int packet_received;
	long int packet_fail;
	long int touched_sync;
	long int no_touched_sync;
};

static struct workqueue_struct *elan_wq;

/*
 *	Sysfs interfaces - provide the hardware and firmware information
 */

static ssize_t show_fw_version_value(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct spi_device *spi = to_spi_device(dev);
	struct elan_data *ed = spi_get_drvdata(spi);
	return sprintf(buf, "%d %d\n",
			ed->major_fw_version,
			ed->minor_fw_version);
}

static DEVICE_ATTR(fw_version, S_IRUGO, show_fw_version_value, NULL);

static ssize_t show_bc_version_value(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct spi_device *spi = to_spi_device(dev);
	struct elan_data *ed = spi_get_drvdata(spi);
	return sprintf(buf, "%d %d\n",
			ed->major_bc_version,
			ed->minor_bc_version);
}

static DEVICE_ATTR(bc_version, S_IRUGO, show_bc_version_value, NULL);

static ssize_t show_hw_id_value(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct spi_device *spi = to_spi_device(dev);
	struct elan_data *ed = spi_get_drvdata(spi);
	return sprintf(buf, "%d %d\n",
			ed->major_hw_id,
			ed->minor_hw_id);
}

static DEVICE_ATTR(hw_id, S_IRUGO, show_hw_id_value, NULL);

static ssize_t show_drvver_value(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d %d %d\n",
			DRV_MA_VER, DRV_MI_VER, DRV_SUB_MI_VER);
}

static ssize_t show_adapter_row(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct spi_device *spi = to_spi_device(dev);
	struct elan_data *ed = spi_get_drvdata(spi);
	return sprintf(buf, "%d\n", ed->rows);
}

static ssize_t show_adapter_col(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct spi_device *spi = to_spi_device(dev);
	struct elan_data *ed = spi_get_drvdata(spi);
	return sprintf(buf, "%d\n", ed->cols);
}

static ssize_t show_adapter_status(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct spi_device *spi = to_spi_device(dev);
	struct elan_data *ed = spi_get_drvdata(spi);
	return sprintf(buf, "0x%x\n", ed->status);
}

static ssize_t show_adapter_irq_num(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct spi_device *spi = to_spi_device(dev);
	struct elan_data *ed = spi_get_drvdata(spi);
	return sprintf(buf, "%ld\n", ed->irq_received);
}

static ssize_t show_adapter_pkt_rvd(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct spi_device *spi = to_spi_device(dev);
	struct elan_data *ed = spi_get_drvdata(spi);
	return sprintf(buf, "%ld %ld\n", ed->packet_received, ed->packet_fail);
}

static ssize_t show_power_state(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct spi_device *spi = to_spi_device(dev);
	struct elan_data *ed = spi_get_drvdata(spi);
	return sprintf(buf, "%d\n", ed->power_state);
}

static ssize_t store_power_state(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	struct spi_device *spi = to_spi_device(dev);
	struct elan_data *ed = spi_get_drvdata(spi);
	unsigned long status = 0;
	int ret;

	mutex_lock(&ed->sysfs_mutex);

	ret = strict_strtol(buf, 0, &status);

	if (ret < 0)
		return -EINVAL;

	/* Force as 0 or 1 */
	if (status)
		status = 1;

	if (status != ed->user_power) {
		if (status)
			pm_runtime_get_sync(dev);
		else
			pm_runtime_put(dev);
		ed->user_power = status;
	}

	mutex_unlock(&ed->sysfs_mutex);

	return count;
}

static DEVICE_ATTR(drv_version, S_IRUGO, show_drvver_value, NULL);
static DEVICE_ATTR(ts_row, S_IRUGO, show_adapter_row, NULL);
static DEVICE_ATTR(ts_col, S_IRUGO, show_adapter_col, NULL);
static DEVICE_ATTR(drv_status, S_IRUGO, show_adapter_status, NULL);
static DEVICE_ATTR(ts_irq_num, S_IRUGO, show_adapter_irq_num, NULL);
static DEVICE_ATTR(ts_packet, S_IRUGO, show_adapter_pkt_rvd, NULL);
static DEVICE_ATTR(power_state, S_IRUGO|S_IWUGO, show_power_state,
							store_power_state);

static struct attribute *elan_attributes[] = {
	&dev_attr_fw_version.attr,
	&dev_attr_bc_version.attr,
	&dev_attr_hw_id.attr,
	&dev_attr_drv_version.attr,
	&dev_attr_ts_row.attr,
	&dev_attr_ts_col.attr,
	&dev_attr_drv_status.attr,
	&dev_attr_ts_irq_num.attr,
	&dev_attr_ts_packet.attr,
	&dev_attr_power_state.attr,
	NULL
};

static struct attribute_group elan_attribute_group = {
	.name	= "ektf2136_spi",
	.attrs	= elan_attributes
};

/**
 *	elan_spi_read_data	-	SPI read wrapper
 *	@spi: spi device
 *	@data: data to read
 *	@len: length of block
 *	@what: what are we doing
 *
 *	Wrap the spi_read_data method as the ELAN needs a 5uS delay between
 *	SPI data bytes
 */
static int elan_spi_read_data(struct spi_device *spi, const u8 *data,
						size_t len, const char *what)
{
	int rc = 0, i;
	struct spi_message msg;
	struct spi_transfer xfer;

	for (i = 0; i < len; i++) {
		spi_message_init(&msg);
		memset(&xfer, 0, sizeof(struct spi_transfer));
		xfer.rx_buf = (u8 *)(data + i);
		xfer.len = 1;
		xfer.delay_usecs = 10;

		spi_message_add_tail(&xfer, &msg);
		rc = spi_sync(spi, &msg);
		if (rc) {
			dev_err(&spi->dev, "elan_spi_read_data failed: %s\n", what);
			break;
		}
	}

	return rc;
}

/**
 *	elan_spi_write_data	-	SPI write wrapper
 *	@spi: spi device
 *	@data: data to write
 *	@len: length of block
 *	@what: what are we doing
 *
 *	Wrap the spi_write_data method as the ELAN needs a 10uS delay between
 *	SPI data bytes
 */
static int elan_spi_write_data(struct spi_device *spi, const u8 *data,
						size_t len, const char *what)
{
	int rc = 0, i;
	struct spi_message msg;
	struct spi_transfer xfer;

	for (i = 0; i < len; i++) {
		spi_message_init(&msg);
		memset(&xfer, 0, sizeof(struct spi_transfer));
		xfer.tx_buf = (u8 *)(data + i);
		xfer.len = 1;
		xfer.delay_usecs = 10;

		spi_message_add_tail(&xfer, &msg);
		rc = spi_sync(spi, &msg);
		if (rc) {
			dev_err(&spi->dev, "elan_spi_write_data failed: %s\n", what);
			break;
		}
	}

	return rc;
}

/* Add for command sync mechanism */
static int elan_touch_wait_cmd_sync(struct spi_device *spi)
{
	static const u8 wait_sync_cmd[4] = { 0xAA, 0xAA, 0xAA, 0xAA };
	int cmd_sync_timeout = 0, rc = 0, v = 0;
	struct elan_data *ed = spi_get_drvdata(spi);

	if (!(ed->protocol & PRO_SPI_WRT_CMD_SYNC))
		return rc;

	ed->wait_sync_int = true;

	rc = elan_spi_write_data(spi, wait_sync_cmd, sizeof(wait_sync_cmd),
							"wait_sync_cmd");
	if (rc < 0)
		return rc;

	v = gpio_get_value(ed->intr_gpio);

	while (v != 0) {
		v = gpio_get_value(ed->intr_gpio);
		if (v == 0)
			break;
		msleep(10);

		cmd_sync_timeout++;
		if (cmd_sync_timeout >= 1000) {
			rc = -EFAULT;
			break;
		}
	}

	ed->wait_sync_int = false;

	return rc;
}

/**
 *	elan_spi_write_cmd	-	SPI write wrapper
 *	@spi: spi device
 *	@data: data to write
 *	@len: length of block
 *	@what: what are we doing
 *
 *	Wrap the spi_write_cmd method as the ELAN needs a 10uS delay between
 *	SPI data bytes
 */
static int elan_spi_write_cmd(struct spi_device *spi, const u8 *data,
					size_t len, const char *what)
{
	elan_touch_wait_cmd_sync(spi);
	return elan_spi_write_data(spi, data, len, what);
}

/* Add for checksum mechanism */
static int elan_touch_checksum(struct spi_device *spi, u8 *buf)
{
	int i = 0, checksum = 0;
	struct elan_data *ed = spi_get_drvdata(spi);

	if (!(ed->protocol & PRO_HID_MOD_CHECKSUM))
		return 0;

	for (i = 0; i < IDX_PACKET_SIZE_XY_CHECKSUM - 1; i++)
		checksum = checksum + buf[i];

	checksum = checksum%256;

	if (checksum != buf[34])
		return -EFAULT;

	return 0;
}

/**
 *	elan_touch_poll		-	wait for the GPIO to go high
 *	@spi: the device we are waiting for
 *
 *	Waits up to 2 seconds for the device to respond. Returns 0 on
 *	success or -ETIMEDOUT on a failure
 */
static int elan_touch_poll(struct spi_device *spi)
{
	int status = 0, retry = 100;
	struct elan_data *ed = spi_get_drvdata(spi);

	do {
		status = gpio_get_value(ed->intr_gpio);
		retry--;
		msleep(20);
	} while (status == 1 && retry > 0);

	return (status == 0 ? 0 : -ETIMEDOUT);
}

/**
 *	elan_touch_parse_xy	-	unpack co-ordinates
 *	@data: input stream data
 *	@x: returned x co-ordinate
 *	@y: returned y co-ordinate
 *
 *	Unpack the 12bit co-ordinates into a pair of values
 */
static inline void elan_touch_parse_xy(u8 *data, u16 *x, u16 *y)
{
	*x = (data[0] & 0xf0);
	*x <<= 4;
	*x |= data[1];

	*y = (data[0] & 0x0f);
	*y <<= 8;
	*y |= data[2];
}

/**
 *	elan_touch_parse_fid	-	parse the 10 fid bits
 *	@data: the input bit stream
 *	@fid: an array of fid values
 *
 *	Unpack the 10 bits into an array.
 */
static inline void elan_touch_parse_fid(u8 *data, u8 *fid)
{
	fid[0] = (data[0] & 0x01);
	fid[1] = (data[0] & 0x02);
	fid[2] = (data[0] & 0x04);
	fid[3] = (data[0] & 0x08);
	fid[4] = (data[0] & 0x10);
	fid[5] = (data[0] & 0x20);
	fid[6] = (data[0] & 0x40);
	fid[7] = (data[0] & 0x80);
	fid[8] = (data[1] & 0x10);
	fid[9] = (data[1] & 0x20);
}

/**
 *	elan_touch_get_fw_ver	-	obtain firmware data from device
 *	@spi: the interface we are querying
 *
 *	Send a firmware version command and fill the results into the
 *	elan device structures. Caller must hold the mutex
 *
 *	Returns 0 or an error code
 */
static int elan_touch_get_fw_ver(struct spi_device *spi)
{
	struct elan_data *ed = spi_get_drvdata(spi);
	static const u8 get_fw_ver_cmd[] = {0x53, 0x00, 0x00, 0x01};
	u8 buf_recv[4];
	int rc;

	rc = elan_spi_write_cmd(spi, get_fw_ver_cmd, sizeof(get_fw_ver_cmd),
							"get_fw_ver_cmd");
	if (rc < 0)
		return rc;

	rc = elan_spi_read_data(spi, buf_recv, sizeof(buf_recv), "get_fw_ver");
	if (rc < 0) {
		dev_err(&spi->dev, "spi_read failed: get_fw_ver\n");
		return rc;
	}

	ed->major_fw_version = (((buf_recv[1] & 0x0f) << 4) |
						((buf_recv[2]&0xf0) >> 4));
	ed->minor_fw_version = (((buf_recv[2] & 0x0f) << 4) |
						((buf_recv[3]&0xf0) >> 4));

	dev_dbg(&spi->dev, "ELAN TOUCH MAJOR FW VERSION 0x%02x\n",
		ed->major_fw_version);
	dev_dbg(&spi->dev, "ELAN TOUCH MINOR FW VERSION 0x%02x\n",
		ed->minor_fw_version);

	return 0;
}

/**
 *      elan_touch_get_power_state   -       obtain power state from device
 *      @spi: the interface we are querying
 *
 *      Send a power state command and fill the results into the
 *      elan device structures. Caller must hold the mutex
 *
 *      Returns 0 or an error code
 */

static int elan_touch_get_power_state(struct spi_device *spi)
{
	struct elan_data *ed = spi_get_drvdata(spi);
	static const u8 get_power_state[] = {0x53, 0x50, 0x00, 0x01};
	u8 buf_recv[4];
	int ret = 0;

	ret = elan_spi_write_cmd(ed->spi, get_power_state,
				sizeof(get_power_state), "get_power_state");
	if (ret < 0)
		return ret;

	ret = elan_spi_read_data(ed->spi, buf_recv, sizeof(buf_recv), "get_power_state");
	if (ret < 0) {
		dev_err(&spi->dev, "spi_read failed: get_power_state\n");
		return ret;
	}

	if (buf_recv[0] != 0x52)
		return -EINVAL;

	ed->power_state = (buf_recv[1] & 0x08) >> 3 ;
	dev_dbg(&spi->dev, "ELAN TOUCH is in %s mode\n",
			ed->power_state ? "active" : "sleep");

	return 0;
}

/**
 *	elan_touch_get_bc_ver	-	obtain bootcode data from device
 *	@spi: the interface we are querying

 *
 *	Send a bootcode version command and fill the results into the
 *	elan device structures. Caller must hold the mutex
 *

 *	Returns 0 or an error code
 */
static int elan_touch_get_bc_ver(struct spi_device *spi)
{
	struct elan_data *ed = spi_get_drvdata(spi);
	static const u8 get_bc_ver_cmd[] = {0x53, 0x10, 0x00, 0x01};
	u8 buf_recv[4];
	int rc;

	rc = elan_spi_write_cmd(spi, get_bc_ver_cmd, sizeof(get_bc_ver_cmd),
							"get_bc_ver_cmd");
	if (rc < 0)
		return rc;

	rc = elan_spi_read_data(spi, buf_recv, sizeof(buf_recv), "get_bc_ver");
	if (rc < 0) {
		dev_err(&spi->dev, "spi_read failed: get_bc_ver\n");
		return rc;
	}

	ed->major_bc_version = (((buf_recv[1] & 0x0f) << 4) |
						((buf_recv[2]&0xf0) >> 4));
	ed->minor_bc_version = (((buf_recv[2] & 0x0f) << 4) |
						((buf_recv[3]&0xf0) >> 4));

	dev_dbg(&spi->dev, "ELAN TOUCH MAJOR BC VERSION 0x%02x\n",
		ed->major_bc_version);
	dev_dbg(&spi->dev, "ELAN TOUCH MINOR BC VERSION 0x%02x\n",
		ed->minor_bc_version);

	return 0;
}

/**
 *	elan_touch_get_hw_id	-	read the hardware id
 *	@spi: the interface we are querying
 *
 *	Send a firmware version command and fill the results into the
 *	elan device structures. Caller must hold the mutex.
 *
 *	Returns 0 or an error code
 */
static int elan_touch_get_hw_id(struct spi_device *spi)
{
	struct elan_data *ed = spi_get_drvdata(spi);
	static const u8 get_hw_id_cmd[] = {0x53, 0xF0, 0x00, 0x01};
	u8 buf_recv[4];
	int rc;

	rc = elan_spi_write_cmd(spi, get_hw_id_cmd, sizeof(get_hw_id_cmd),
							"get_hw_id_cmd");
	if (rc < 0)
		return rc;

	rc = elan_spi_read_data(spi, buf_recv, sizeof(buf_recv), "get_hw_id");
	if (rc < 0) {
		dev_err(&spi->dev, "spi_read failed: get_hw_id\n");
		return rc;
	}

	ed->major_hw_id = (((buf_recv[1] & 0x0f) << 4) |
						((buf_recv[2]&0xf0) >> 4));
	ed->minor_hw_id = (((buf_recv[2] & 0x0f) << 4) |
						((buf_recv[3]&0xf0) >> 4));

	dev_dbg(&spi->dev, "ELAN TOUCH MAJOR HW ID 0x%02x\n",
		ed->major_hw_id);
	dev_dbg(&spi->dev, "ELAN TOUCH MINOR HW ID 0x%02x\n",
		ed->minor_hw_id);

	return 0;
}

/**
 *	elan_touch_get_resolution	-	read the resolution
 *	@spi: the interface we are querying
 *
 *	Send a getting resolution command and fill the results into the
 *	elan device structures. Caller must hold the mutex.
 *
 *	Returns 0 or an error code
 */
static int elan_touch_get_resolution(struct spi_device *spi)
{
	struct elan_data *ed = spi_get_drvdata(spi);
	static const u8 get_resolution_cmd[] = {
		0x5B, 0x00, 0x00, 0x00, 0x00, 0x00
	};
	u8 buf_recv[17];
	int rc;

	rc = elan_spi_write_cmd(spi, get_resolution_cmd,
			sizeof(get_resolution_cmd), "get_resolution_cmd");
	if (rc < 0)
		return rc;

	rc = elan_spi_read_data(spi, buf_recv, sizeof(buf_recv), "get_resolution");
	if (rc < 0) {
		dev_err(&spi->dev, "spi_read failed: get_resolution\n");

		return rc;
	}

	if (buf_recv[0] != 0x9B)
		return -EINVAL;

	ed->rows = (buf_recv[2] + buf_recv[6] + buf_recv[10]);
	ed->cols = (buf_recv[3] + buf_recv[7] + buf_recv[11]);

	if (ed->rows < 2 || ed->cols < 2) {
		dev_err(&spi->dev,
			"elan_touch_get_resolution: invalid resolution (%d, %d)\n",
							ed->rows, ed->cols);
		return -EINVAL;
	}
	dev_dbg(&spi->dev, "resolution rows = 0x%02x, cols = 0x%02x\n",
							ed->rows, ed->cols);
	return 0;
}

/**
 *	elan_touch_init - hand shake with touch panel
 *	@spi: our panel
 *
 *	Wait for the GPIO and then read looking for a hello packet. Return
 *	0 on success. Caller must hold the mutex
 */
static int elan_touch_init_panel(struct spi_device *spi)
{
	u8 buf_recv[4];
	struct elan_data *ed = spi_get_drvdata(spi);
	int rc = elan_touch_poll(spi);

	if (rc < 0)
		return -EINVAL;

	ed->rx_size = IDX_PACKET_SIZE_XY;

	rc = elan_spi_read_data(spi, buf_recv, sizeof(buf_recv), "init_panel");
	if (rc)
		return rc;

	dev_dbg(&spi->dev, "hello packet: [0x%02x 0x%02x 0x%02x 0x%02x]\n",
		buf_recv[0], buf_recv[1], buf_recv[2], buf_recv[3]);

	if (memcmp(buf_recv, hello_packet, 4)) {
		if (memcmp(buf_recv, hello_packet, 2))
			return -EINVAL;

		if (buf_recv[3] & PRO_SPI_WRT_CMD_SYNC)
			ed->protocol = ed->protocol | PRO_SPI_WRT_CMD_SYNC;

		if (buf_recv[3] & PRO_HID_MOD_CHECKSUM) {
			ed->protocol = ed->protocol | PRO_HID_MOD_CHECKSUM;
			ed->rx_size = IDX_PACKET_SIZE_XY_CHECKSUM;
		}
	}
	/* Save the base rx size as we will need this once we switch in and
	   out of raw mode */
	ed->input_rx_size = ed->rx_size;

	return 0;
}

/**
 *	elan_touch_recv_data	-	read a packet from the device
 *	@spi: our panel
 *	@buf: buffer for input
 *
 *	Read a packet from the touch panel. Return an error code or 0
 *	on success.
 */
static int elan_touch_recv_data(struct spi_device *spi, u8 *buf)
{
	struct elan_data *ed = spi_get_drvdata(spi);
	int rc;

	/* Protect against parallel spi activity. Right now this isn't
	   strictly needed but if we add config features it will become
	   relevant */
	mutex_lock(&ed->mutex);
	rc = elan_spi_read_data(spi, buf, ed->rx_size, "recv_data");
	if (rc != 0)
		dev_err(&spi->dev, "recv_data: wrong data %d\n", rc);
	mutex_unlock(&ed->mutex);
	return rc;
}

/**
 *	elan_touch_report_fingers	-	process finger reports
 *	@ed: our touchscreen
 *	@finger_stat: number of fingers in packet
 *	@buf: received buffer
 *
 *	Walk the received report and process the finger data, extracting
 *	and reporting co-ordinates. No locking is needed here as the workqueue
 *	does our threading for us.
 */
static void elan_touch_report_fingers(struct elan_data *ed,
						int finger_stat, u8 *buf)
{
	int i;
	u8 fid[10];
	u16 x[10], y[10];

	elan_touch_parse_fid(&buf[1], &fid[0]);

	for (i = 0; i < 10; i++) {
		if (finger_stat == 0)
			return;

		if (fid[i] == 0)
			continue;

		elan_touch_parse_xy(&buf[3+i*3], &x[i], &y[i]);

		if (x[i] != 0 && y[i] != 0) {
			input_event(ed->input,
				EV_ABS, ABS_MT_POSITION_X, y[i]);
			input_event(ed->input,
				EV_ABS, ABS_MT_POSITION_Y, x[i]);
			input_event(ed->input,
				EV_ABS, ABS_MT_TRACKING_ID, (FINGER_ID + i));
			input_mt_sync(ed->input);
			finger_stat--;
		}
	}
}


/**
 *	elan_touch_report_data	-	turn data report into input events
 *	@spi: our panel
 *	@buf: the input buffer received
 *
 *	Each packet of data needs unpacking and turning into input
 *	co-ordinates and finger data then reporting to the input layer
 *	as an event.
 *
 *	All our input reporting is single threaded by the work queue.
 */
static void elan_touch_report_data(struct spi_device *spi, u8 *buf)
{
	struct elan_data *ed = spi_get_drvdata(spi);

	switch (buf[0]) {
	case idx_coordinate_packet_5_finger:
	case idx_coordinate_packet_10_finger:
	{
		u8 finger_stat  = buf[idx_finger_state] & 0x0f;
		dev_dbg(&spi->dev, "ELAN, finger_stat == %d\n", finger_stat);
		ed->packet_received++;
		if (finger_stat != 0) {
			elan_touch_report_fingers(ed, finger_stat, buf);
			ed->touched_sync++;
		} else {
			input_mt_sync(ed->input);
			ed->no_touched_sync++;
		}

		input_sync(ed->input);
		break;
	}
	default:
		break;
	}
}

/**
 *	elan_touch_pull_frame	-	pull a frame from the fifo
 *	@ed: our elan touch device
 *	@ehr: return buffer for the header
 *	@buf: data buffer
 *
 *	Pulls a frame from the FIFO into the provided ehr and data buffer.
 *	The data buffer must be at least MAX_REPORT_SIZE bytes long.
 */
static int elan_touch_pull_frame(struct elan_data *ed, struct elan_sr_hdr *ehr,
							u8 *buf)
{
	WARN_ON(kfifo_out_locked(&ed->fifo, ehr, sizeof(struct elan_sr_hdr),
		&ed->rx_kfifo_lock) != sizeof(struct elan_sr_hdr));
	memcpy(buf, ehr, sizeof(struct elan_sr_hdr));
	WARN_ON(kfifo_out_locked(&ed->fifo, buf+sizeof(struct elan_sr_hdr),
		ehr->frame_size, &ed->rx_kfifo_lock) != ehr->frame_size);
	ehr->frame_size += sizeof(struct elan_sr_hdr);
	return ehr->frame_size;
}

/**
 *	elan_touch_fifo_clean_old	-	Make room for new frames
 *	@ed: our elan touch device
 *	@room: space needed
 *
 *	Empty old frames out of the FIFO until we can fit the new one into
 *	the other end.
 */
static void elan_touch_fifo_clean_old(struct elan_data *ed, int room)
{
	struct elan_sr_hdr ehr;
	while (kfifo_len(&ed->fifo) + room >= FIFO_SIZE)
		elan_touch_pull_frame(ed, &ehr, ed->buffer);
}

/**
 *	elan_touch_report_buffer	-	report data to sr driver
 *	@ed: our elan touch device
 *	@buf: buffer holding the report
 *	@len: size of report
 *
 *	Queue a report to the sr layer. The blocks from the SPI driver
 *	have been reassembled into complete reports at this point
 */

static void elan_touch_report_buffer(struct elan_data *ed, u8 *buf, int len)
{
	struct elan_capability *ecap = (struct elan_capability *)buf;
	struct elan_sr_hdr ehr;
	int space = len + sizeof(ehr);

	if (len > 1 && buf[0] == 0xFF && buf[1] == 0xFF) {
		memcpy(&ed->capability, ecap, sizeof(struct elan_capability));
		if (ecap->frame_rate) {
			/* Into Hz */
			unsigned long t = 1000 / ecap->frame_rate;
			if (t)
				ed->poll_time = t;
			else
				ed->poll_time = 100;
		}
	}

	ehr.frame_num = ed->frame_num++;
	ehr.frame_size = len;
	ehr.timestamp = jiffies_to_msecs(jiffies);

	/* Queue the data, using the fifo lock to serialize the multiple
	   accesses to the FIFO */
	mutex_lock(&ed->fifo_mutex);
	if (kfifo_len(&ed->fifo) + space >= FIFO_SIZE)
		/* Make room, make room */
		elan_touch_fifo_clean_old(ed, space);
	/* Push the header */
	kfifo_in_locked(&ed->fifo, &ehr, sizeof(ehr), &ed->rx_kfifo_lock);
	/* Push the data */
	kfifo_in_locked(&ed->fifo, buf, len, &ed->rx_kfifo_lock);
	mutex_unlock(&ed->fifo_mutex);
	wake_up(&ed->wait);
}

/**
 *	elan_touch_report_capability	-	process capability frame
 *	@spi: our panel
 *	@buf: the input buffer received
 *
 *	Process the capability frame on switching to raw mode. This is
 *	serialized by the work queue against other input paths.
 */
static void elan_touch_report_capability(struct spi_device *spi, u8 *buf)
{
	struct elan_data *ed = spi_get_drvdata(spi);
	if (buf[0] == 0xFF && buf[1] == 0xFF)
		elan_touch_report_buffer(ed, buf, IDX_SR_CAPABILITY_SIZE);
}

/**
 *	elan_touch_report_raw		-	process raw frame
 *	@spi: our panel
 *	@buf: the input buffer received
 *
 *	Process the capability frame on switching to raw mode. This is
 *	serialized by the work queue against other input paths.
 */
static void elan_touch_report_raw(struct spi_device *spi, u8 *buf)
{
	struct elan_data *ed = spi_get_drvdata(spi);

	if (buf[0] == 0xFF && buf[1] == 0xFE) {
		/* Report a scan end - simples */
		if (memcmp(buf + 4, "\0\0\0\0", 4) == 0) {
			ed->report_size = IDX_SCAN_END_SIZE;
			if (++ed->scan_end_num <= 10) {
				ed->scan_end = true;
				elan_touch_report_buffer(ed, buf,
							ed->report_size);
				ed->report_size = 0;
			}
		} else {
			/* Pull out the size and see if this report
			   fitted into one block or not */
			ed->scan_end = false;
			ed->scan_end_num = 0;
			ed->report_size = ((buf[7] << 8) | buf[6]) + 8;
			if (ed->report_size <= 32) {
				/* Report single block */
				elan_touch_report_buffer(ed, buf,
							ed->report_size);
				ed->report_size = 0;
			} else {
				/* Allocate a working buffer to accumulate
				   the report. */
				ed->scan_report = kzalloc(ed->report_size,
								GFP_KERNEL);
				if (ed->scan_report == NULL)
					goto fail;
				/* How many blocks to copy */
				ed->report_count = ed->report_size / 32;
				/* And copy the rest of this block */
				memcpy(ed->scan_report, buf, ed->report_size);
			}
		}
	} else if (buf[0] == 0xFF && buf[1] == 0xFF) {
		/* Capability report */
		elan_touch_report_buffer(ed, buf, IDX_SR_CAPABILITY_SIZE);
	} else {
		/* More blocks for the report we are accumulating */
		if (ed->report_size > 32 && ed->report_count > 0) {
			int tmp = ed->report_size / 32 + 1;
			int index = (tmp - ed->report_count) * 32;
			if (ed->report_size - index <= 32) {
				memcpy(ed->scan_report + index, buf,
						ed->report_size - index);
				elan_touch_report_buffer(ed, ed->scan_report,
							ed->report_size);
				kfree(ed->scan_report);
				ed->scan_report = NULL;
			} else {
				memcpy(ed->scan_report + index, buf,
							IDX_PACKET_SIZE_RAW);
				ed->report_count--;
			}
		}
	}
	return;
fail:
	/* We now have no buffer but potentially chunks of event left over
	 * discard time */
	ed->report_size = 0;
}

/**
 *	elan_touch_work_func	-	process the elan device
 *	@work: our work struct
 *
 *	Triggered on each interrupt or timer poll if running in polled mode.
 *	Read and process a buffer from the device.
 */
static void elan_touch_work_func(struct work_struct *work)
{
	struct elan_data *ed = container_of(work, struct elan_data, work);
	struct spi_device *spi = ed->spi;
	u8 buf[IDX_MAX_PACKET_SIZE];

	if (gpio_get_value(ed->intr_gpio)) {
		dev_err(&spi->dev, "elan_touch_work_func: gpio_get_value\n");
		return;
	}
	if (elan_touch_recv_data(spi, buf) != 0)
		return;
	switch (ed->sr_mode) {
	case 0:
		if (elan_touch_checksum(spi, buf) == 0)
			elan_touch_report_data(spi, buf);
		break;
	case SR_WAIT_CAP:
		/* Capability packet received, data now follows */
		elan_touch_report_capability(spi, buf);
		ed->sr_mode = SR_PACKET;
		ed->rx_size = IDX_PACKET_SIZE_RAW;
		break;
	case SR_PACKET:
		/* Raw data packet */
		elan_touch_report_raw(spi, buf);
		break;
	}
}

/**
 *	elan_touch_ts_interrupt	-	queue async processing
 *	@irq: irq handler
 *	@dev_id: our device
 *
 *	An interrupt has occurred for our touch panel, queue up the
 *	work to process it. We could switch to a threaded IRQ but this
 *	would mean more duplication between irq and polled paths it seems
 */
static irqreturn_t elan_touch_ts_interrupt(int irq, void *dev_id)
{
	struct elan_data *ed = dev_id;

	if (ed->wait_sync_int)
		return IRQ_HANDLED;

	ed->irq_received++;
	queue_work(elan_wq, &ed->work);

	return IRQ_HANDLED;
}

/**
 *	elan_touch_timer_func	-	poll processing
 *	@timer: our timer
 *
 *	Queue polled processing to occur on our touch panel and kick the timer
 *	off again
 *
 *	CHECK: we have no guarantee that the timer will not run multiple times
 *	within one poll, does it matter ?
 */
static enum hrtimer_restart elan_touch_timer_func(struct hrtimer *timer)
{
	struct elan_data *ed = container_of(timer, struct elan_data, timer);
	queue_work(elan_wq, &ed->work);
	hrtimer_start(&ed->timer,
				ktime_set(0, 12500000), HRTIMER_MODE_REL);

	return HRTIMER_NORESTART;
}

/**
 *	elan_touch_register_interrupt	-	set up IRQ or polling
 *	@spi: our SPI device
 *
 *	If our device has an interrupt we want to use it, but if not, or if
 *	it cannot be allocated then we will fall back to polling,
 */
static int __devinit elan_touch_register_interrupt(struct spi_device *spi)
{
	struct elan_data *ed = spi_get_drvdata(spi);
	if (spi->irq) {
		int err;

		ed->use_irq = 1;

		err = request_threaded_irq(spi->irq, NULL,
                                elan_touch_ts_interrupt, IRQF_TRIGGER_FALLING,
                                        ELAN_TS_NAME, ed);
		if (err < 0) {
			dev_err(&spi->dev,
				"can't allocate irq %d - %d\n", spi->irq, err);
			ed->use_irq = 0;
		}
	}

	if (!ed->use_irq) {
		hrtimer_init(&ed->timer,
					CLOCK_MONOTONIC, HRTIMER_MODE_REL);
		ed->timer.function = elan_touch_timer_func;
		hrtimer_start(&ed->timer, ktime_set(1, 0),
					HRTIMER_MODE_REL);
	}
	dev_dbg(&spi->dev, "elan ts starts in %s mode.\n",
			ed->use_irq == 1 ? "interrupt" : "polling");
	ed->status |= STA_USE_IRQ;
	return 0;
}

/**
 *	elan_send_soft_reset	-	request the panel resets
 *	@spi: our panel
 *
 *	Send a reset request. Log any error reported when we try this.
 */
static void elan_send_soft_reset(struct spi_device *spi)
{
	static const u8 soft_reset_cmd[] = {
		SOFT_RESET, SOFT_RESET, SOFT_RESET, SOFT_RESET
	};

	elan_spi_write_cmd(spi, soft_reset_cmd, sizeof(soft_reset_cmd),
						"soft_reset_cmd");
}

/**
 *	elan_open		-	open locking
 *	@input: input device
 *
 *	Open the input device. Ensure we do not mix input use and firmware
 *	updating.
 */
static int elan_open(struct input_dev *input)
{
	struct elan_data *ed = input_get_drvdata(input);
	if (test_and_set_bit(0, &ed->busy))
		return -EBUSY;
	/* Power up the device when the input layer is open */
	pm_runtime_get_sync(&ed->spi->dev);
	return 0;
}

/**
 *	elan_close		-	close input device
 *	@input: input device
 *
 *	Open the input device. Ensure we do not mix input use and firmware
 *	updating.
 */
static void elan_close(struct input_dev *input)
{
	struct elan_data *ed = input_get_drvdata(input);
	/* Drop power, the runtime pm refcounting will do all our work */
	pm_runtime_put(&ed->spi->dev);
	clear_bit(0, &ed->busy);
}

static unsigned int elan_sr_poll(struct file *filp, poll_table *wait)
{
	struct elan_data *ed = container_of(filp->private_data,
						struct elan_data, sr_dev);
	poll_wait(filp, &ed->wait, wait);
	if (kfifo_len(&ed->fifo))
		return POLLIN | POLLRDNORM;
	return 0;
}

static ssize_t elan_sr_read(struct file *filp, char __user *buf, size_t count,
	loff_t *f_pos)
{
	struct elan_data *ed = container_of(filp->private_data,
						struct elan_data, sr_dev);
	struct elan_sr_hdr ehr;
	int copied;

	mutex_lock(&ed->fifo_mutex);
	/* We will wait for non O_NONBLOCK handles until a signal or data */
	while (kfifo_len(&ed->fifo) == 0) {
		if (filp->f_flags & O_NONBLOCK) {
			mutex_unlock(&ed->fifo_mutex);
			return -EWOULDBLOCK;
		}
		mutex_unlock(&ed->fifo_mutex);
		if (wait_event_interruptible(ed->wait,
					kfifo_len(&ed->fifo)) < 0) {
			return -EINTR;
		}
		mutex_lock(&ed->fifo_mutex);
	}
	copied = elan_touch_pull_frame(ed, &ehr, ed->buffer);
	mutex_unlock(&ed->fifo_mutex);
	if (copied > count)
		copied = count;
	copied -= copy_to_user(buf, ed->buffer, copied);
	if (copied == 0)
		copied = -EFAULT;
	return copied;
}

/**
 *	elan_sr_do_open		-	open in raw mode
 *	@spi: our device
 *
 *	Open the interface in raw packet mode. Use the busy flag to lock
 *	against other users as we will be using a different protocol so
 *	cannot mix with input layer use.
 */
static int elan_sr_do_open(struct spi_device *spi)
{
	const u8 buf[4] = { 0x6A, 0x6A, 0x6A, 0x6A };
	struct elan_data *ed = spi_get_drvdata(spi);
	int rc;

	if (test_and_set_bit(0, &ed->busy))
		return -EBUSY;

	ed->buffer = kmalloc(MAX_REPORT_SIZE, GFP_KERNEL);
	if (ed->buffer == NULL) {
		dev_dbg(&spi->dev, "no report buf space\n");
		clear_bit(0, &ed->busy);
		return -ENOMEM;
	}

	if (kfifo_alloc(&ed->fifo, FIFO_SIZE, GFP_KERNEL) < 0) {
		dev_dbg(&spi->dev, "no fifo space\n");
		kfree(ed->buffer);
		clear_bit(0, &ed->busy);
		return -ENOMEM;
	}

	pm_runtime_get_sync(&spi->dev);
	/*
	 * Prevent the recv_data path running before we have set up our
	 * data structures
	 */
	mutex_lock(&ed->mutex);
	/* Initiate raw mode */
	rc = elan_spi_write_cmd(spi, buf, 4, "sr_open");
	if (rc < 0) {
		mutex_unlock(&ed->mutex);
		pm_runtime_put(&spi->dev);
		clear_bit(0, &ed->busy);
		return rc;
	}
	/* The first block we will get */
	ed->rx_size = IDX_SR_CAPABILITY_SIZE;
	ed->sr_mode = SR_WAIT_CAP;
	ed->frame_num = 0;
	mutex_unlock(&ed->mutex);
	return 0;
}

/**
 *	elan_sr_do_close	-	exit raw protocol mode
 *	@spi: our SPI device
 *
 *	Switch the device back into input mode, clean up our configuration
 *	and go back to the old buffering sizes
 */
static int elan_sr_do_close(struct spi_device *spi)
{
	const u8 buf[4] = { 0x65, 0x65, 0x65, 0x65 };
	struct elan_data *ed = spi_get_drvdata(spi);
	int rc;

	mutex_lock(&ed->mutex);
	/* Exit raw mode */
	rc = elan_spi_write_cmd(spi, buf, 4, "sr_close");

	pm_runtime_put(&spi->dev);

	if (rc < 0) {
		mutex_unlock(&ed->mutex);
		clear_bit(0, &ed->busy);
		return rc;
	}
	ed->rx_size = ed->input_rx_size;
	ed->sr_mode = 0;
	kfifo_free(&ed->fifo);
	kfree(ed->buffer);
	mutex_unlock(&ed->mutex);
	clear_bit(0, &ed->busy);
	return 0;
}

/**
 *	elan_sr_ioctl		-	ioctl for direct sr mode interface
 *	@filp: file pointer
 *	@cmd: command
 *	@arg: pointer to user space argument
 *
 *	Implement the controlling ioctls for the sr interface. This allows the
 *	caller to control and switch SR mode on and off as well as obtain
 *	various 'vital statistics' from the open device node
 */
static long elan_sr_ioctl(struct file *filp, unsigned int cmd,
							unsigned long arg)
{
	int __user *ip = (int __user *)arg;
	struct elan_data *ed = container_of(filp->private_data,
						struct elan_data, sr_dev);
	int on;
	int ret = 0;

	switch (cmd) {
	case SRIOCWM:
		if (get_user(on, ip)) {
			ret =  -EFAULT;
			break;
		}
		if (on && ed->sr_mode)
			break;
		if (!on && !ed->sr_mode)
			break;
		if (on)
			ret = elan_sr_do_open(ed->spi);
		else
			ret = elan_sr_do_close(ed->spi);
		break;
	case SRIOCRM:
		ret = put_user(ed->sr_mode ? 1 : 0, ip);
		break;
	case SRIOCRCC:
		/* Or should we be using ed->rows/ed->cols here and not
		   keeping the capability block around */
		ret = put_user((int)ed->capability.columns, ip);
		break;
	case SRIOCRCR:
		ret = put_user((int)ed->capability.rows, ip);
		break;
	case SRIOCRV:
		ret = put_user((int)ed->input->id.vendor, ip);
		break;
	case SRIOCRP:
		ret = put_user((int)ed->input->id.product, ip);
		break;
	}
	return ret;
}

static int elan_sr_open(struct inode *inode, struct file *filp)
{
	return 0;
}

static int elan_sr_release(struct inode *inode, struct file *filp)
{
	struct elan_data *ed = container_of(filp->private_data,
						struct elan_data, sr_dev);
	if (ed->sr_mode)
		elan_sr_do_close(ed->spi);
	return 0;
}

static const struct file_operations elan_sr_fops = {
	.owner		= THIS_MODULE,
	.open		= elan_sr_open,
	.release	= elan_sr_release,
	.read		= elan_sr_read,
	.poll		= elan_sr_poll,
	.llseek		= no_llseek,
	.unlocked_ioctl	= elan_sr_ioctl
};

/*
 *	Device file interfaces - TS F/W upgrade interface
 */

static int elan_iap_open(struct inode *inode, struct file *filp)
{
	struct elan_data *ed = container_of(filp->private_data,
						struct elan_data, firmware);
	if (test_and_set_bit(0, &ed->busy))
		return -EBUSY;
	disable_irq(ed->spi->irq);
	pm_runtime_get_sync(&ed->spi->dev);
	return 0;
}

static int elan_iap_release(struct inode *inode, struct file *filp)
{
	struct elan_data *ed = container_of(filp->private_data,
						struct elan_data, firmware);
	pm_runtime_put(&ed->spi->dev);
	enable_irq(ed->spi->irq);
	clear_bit(0, &ed->busy);
	return 0;
}

static ssize_t elan_iap_write(struct file *filp, const char *buff,
						size_t count, loff_t *offp)
{
	static const u8 iap_reset_data[4] = { 0x77, 0x77, 0x77, 0x77 };
	struct elan_data *ed = container_of(filp->private_data,
						struct elan_data, firmware);
	int rc;
	u8 txbuf[256];

	if (count > 256)
		return -EMSGSIZE;

	if (copy_from_user(txbuf, (u8 *) buff, count))
		return -EFAULT;

	mutex_lock(&ed->mutex);

	if (count == 4) {
		if (!memcmp(txbuf, iap_reset_data, 4))
			rc = elan_spi_write_data(ed->spi, txbuf, count,
						"iap_reset_data");
		else
			rc = elan_spi_write_cmd(ed->spi, txbuf, count,
						"iap_write_cmd");
	} else
		rc = elan_spi_write_data(ed->spi, txbuf, count,
							"iap_write_data");

	mutex_unlock(&ed->mutex);

	if (rc == 0)
		rc = count;
	return rc;
}

static ssize_t elan_iap_read(struct file *filp, char *buff, size_t count,
								loff_t *offp)
{
	struct elan_data *ed = container_of(filp->private_data,
						struct elan_data, firmware);
	int rc;
	u8 rxbuf[256];

	if (count > 256)
		return -EMSGSIZE;

	mutex_lock(&ed->mutex);
	rc = elan_spi_read_data(ed->spi, rxbuf, count, "iap_read");
	mutex_unlock(&ed->mutex);

	if (rc < 0) {
		dev_err(&ed->spi->dev, "iap_read error\n");
		return rc;
	}
	if (copy_to_user(buff, rxbuf, count))
		return -EFAULT;
	return count;
}

static long elan_iap_ioctl(struct file *filp, unsigned int cmd,
							unsigned long arg)
{
	struct elan_data *ed = container_of(filp->private_data,
						struct elan_data, firmware);
	int __user *argp =  (int __user *) arg;

	switch (cmd) {
	case IOCTL_MAJOR_FW_VER:
		return put_user(ed->major_fw_version, argp);
	case IOCTL_MINOR_FW_VER:
		return put_user(ed->minor_fw_version, argp);
	case IOCTL_MAJOR_HW_ID:
		return put_user(ed->major_hw_id, argp);
	case IOCTL_MINOR_HW_ID:
		return put_user(ed->minor_hw_id, argp);
	case IOCTL_MAJOR_BC_VER:
		return put_user(ed->major_bc_version, argp);
	case IOCTL_MINOR_BC_VER:
		return put_user(ed->minor_bc_version, argp);
	default:
		return -ENOTTY;
	}
}

static const struct file_operations elan_iap_fops = {
	.owner		= THIS_MODULE,
	.open		= elan_iap_open,
	.write		= elan_iap_write,
	.read		= elan_iap_read,
	.release	= elan_iap_release,
	.unlocked_ioctl	= elan_iap_ioctl,
};

/**
 *	elan_touch_probe	-	probe for touchpad
 *	@spi: our SPI device
 *
 *	Perform setup and probe for our SPI device and if successful configure
 *	it up as an input device. If not then clean up and return an error
 *	code.
 */
static int __devinit elan_touch_probe(struct spi_device *spi)
{
	int err;
	int tries;
	char temp[64]; /* two packets size subtract one hello packet size */
	struct elan_data *ed = kzalloc(sizeof(struct elan_data), GFP_KERNEL);
	struct ektf2136_platform_data *pdata = NULL;

	if (ed == NULL)
		return -ENOMEM;

	ed->status |= STA_PROBED;

	spi_set_drvdata(spi, ed);

	spi->bits_per_word = 8;
	spi_setup(spi);

	dev_dbg(&spi->dev, "spi->bits_per_word= %d\n", spi->bits_per_word);
	dev_dbg(&spi->dev, "spi->mode= %d\n", spi->mode);
	dev_dbg(&spi->dev, "spi->max_speed_hz= %d\n", spi->max_speed_hz);
	dev_dbg(&spi->dev, "spi->master->bus_num= %d\n", spi->master->bus_num);
	dev_dbg(&spi->dev, "spi->chip_select= %d\n", spi->chip_select);

	ed->spi = spi;
	INIT_WORK(&ed->work, elan_touch_work_func);

	mutex_init(&ed->mutex);
	mutex_init(&ed->sysfs_mutex);
	mutex_init(&ed->fifo_mutex);
	init_waitqueue_head(&ed->wait);
	spin_lock_init(&ed->rx_kfifo_lock);

	pdata = spi->dev.platform_data;

	if (!pdata) {
		dev_dbg(&spi->dev, "no platform data\n");
		err = -EINVAL;
		goto fail;
	}

	ed->intr_gpio = pdata->gpio;

	ed->input = input_allocate_device();
	if (ed->input == NULL) {
		err = -ENOMEM;
		goto fail;
	}

	mutex_lock(&ed->mutex);

	for (tries = 0; tries < 100; tries++) {
		err = elan_touch_init_panel(spi);
		if (!err)
			break;
		/* init failed, clear read buffer and send reset command,
		  try again */
		elan_spi_read_data(spi, temp, sizeof(temp), "probe");
		elan_send_soft_reset(spi);
		msleep(10);
	}

	if (err < 0)
		goto fail_un;

	/* Get TS Power State*/
	err = elan_touch_get_power_state(spi);
	if (err < 0)
		goto fail_un;

	/* Get TS FW version */
	err = elan_touch_get_fw_ver(spi);
	if (err < 0)
		goto fail_un;

	/* Get TS BootCode version */
	err = elan_touch_get_bc_ver(spi);
	if (err < 0)
		goto fail_un;

	/* Get TS HW ID */
	err = elan_touch_get_hw_id(spi);
	if (err < 0)
		goto fail_un;

	/* Get TS Resolution */
	err = elan_touch_get_resolution(spi);
	if (err < 0)
		goto fail_un;

	ed->status |= STA_INIT;

	mutex_unlock(&ed->mutex);

	/* Fill in our input device */
	ed->input->name = ELAN_TS_NAME;
	ed->input->id.vendor  = 0x3030;
	ed->input->id.product = 0x4040;

	ed->input->open = elan_open;
	ed->input->close = elan_close;

	set_bit(EV_SYN, ed->input->evbit);
	set_bit(SYN_MT_REPORT, ed->input->evbit);

	set_bit(EV_ABS, ed->input->evbit);
	set_bit(ABS_MT_POSITION_X, ed->input->absbit);
	set_bit(ABS_MT_POSITION_Y, ed->input->absbit);
	set_bit(ABS_MT_TRACKING_ID, ed->input->absbit);
	set_bit(ABS_MT_PRESSURE, ed->input->absbit);

	input_set_abs_params(ed->input,
				ABS_MT_POSITION_X, 0,
				ELAN_TS_RESOLUTION(ed->rows), 0, 0);
	input_set_abs_params(ed->input,
				ABS_MT_POSITION_Y, 0,
				ELAN_TS_RESOLUTION(ed->cols), 0, 0);
	input_set_abs_params(ed->input,
				ABS_MT_PRESSURE, 0, 100, 0, 0);
	input_set_abs_params(ed->input,
				ABS_MT_TRACKING_ID, 0, 10000, 0, 0);

	/* set hint event for enlarge evdev event buffers */
	input_set_events_per_packet(ed->input, 60);

	input_set_drvdata(ed->input, ed);

	err = input_register_device(ed->input);
	if (err < 0)
		goto fail;

	elan_touch_register_interrupt(spi);

	if (sysfs_create_group(&spi->dev.kobj, &elan_attribute_group))
		dev_err(&spi->dev, "sysfs create group error\n");

	/* To support more we would need a naming scheme per port */
	ed->firmware.minor = MISC_DYNAMIC_MINOR;
	ed->firmware.name = "ektf2136-firmware";
	ed->firmware.fops = &elan_iap_fops;

	if (misc_register(&ed->firmware) < 0)
		dev_err(&spi->dev, "unable to create firmware update interface");
	else
		ed->fw_enabled = 1;
	/* If the firmware device fails we carry on as it doesn't stop normal
	   usage */

	ed->sr_dev.minor = MISC_DYNAMIC_MINOR;
	ed->sr_dev.name = "ektf2136-sr";
	ed->sr_dev.fops = &elan_sr_fops;

	if (misc_register(&ed->sr_dev) < 0)
		dev_err(&spi->dev, "unable to create firmware update interface");
	else
		ed->sr_enabled = 1;

	/* If the sr device fails we carry on as it doesn't stop normal
	   usage */

	ed->status |= STA_INIT2;

	pm_runtime_set_active(&ed->spi->dev);
	pm_runtime_enable(&ed->spi->dev);

	return 0;

fail_un:
	mutex_unlock(&ed->mutex);
fail:
	if (ed->input)
		input_free_device(ed->input);
	kfree(ed);
	return err;
}

/**
 *	elan_touch_remove	-	remove a panel
 *	@spi: device going away
 *
 *	Remove a panel driver. Unregister from the input layer and destroy
 *	any resources we are using
 */
static int __devexit elan_touch_remove(struct spi_device *spi)
{
	struct elan_data *ed = spi_get_drvdata(spi);

	if (ed->user_power)
		pm_runtime_put(&ed->spi->dev);

	pm_runtime_disable(&ed->spi->dev);
	pm_runtime_set_suspended(&ed->spi->dev);

	input_unregister_device(ed->input);

	if (ed->use_irq)
		free_irq(spi->irq, ed);
	else
		hrtimer_cancel(&ed->timer);

	mutex_lock(&ed->mutex);
	elan_send_soft_reset(spi);
	mutex_unlock(&ed->mutex);

	sysfs_remove_group(&spi->dev.kobj, &elan_attribute_group);

	if (ed->fw_enabled)
		misc_deregister(&ed->firmware);
	if (ed->sr_enabled)
		misc_deregister(&ed->sr_dev);
	return 0;
}

#ifdef CONFIG_PM_RUNTIME
static int ektf2136_runtime_suspend(struct device *dev)
{
	struct spi_device *spi = to_spi_device(dev);
	struct elan_data *ed = spi_get_drvdata(spi);
	static const u8 set_sleep_cmd[] = {0x54, 0x50, 0x00, 0x01};
	int ret;

	ret = elan_spi_write_cmd(ed->spi, set_sleep_cmd,
				sizeof(set_sleep_cmd), "set_sleep_cmd");
	if (ret < 0)
		return ret;

	mutex_lock(&ed->sysfs_mutex);
	ed->power_state = 0;
	mutex_unlock(&ed->sysfs_mutex);
	msleep(100);
	return 0;
}

static int ektf2136_runtime_resume(struct device *dev)
{
	struct spi_device *spi = to_spi_device(dev);
	struct elan_data *ed = spi_get_drvdata(spi);
	static const u8 set_active_cmd[] = {0x54, 0x58, 0x00, 0x01};
	int ret;

	ret = elan_spi_write_cmd(ed->spi, set_active_cmd,
			sizeof(set_active_cmd), "set_active_cmd");
	if (ret < 0)
		return ret;

	mutex_lock(&ed->sysfs_mutex);
	ed->power_state = 1;
	mutex_unlock(&ed->sysfs_mutex);
	msleep(100);
	return 0;
}

static const struct dev_pm_ops ektf2136_pm_ops = {
	.runtime_suspend = ektf2136_runtime_suspend,
	.runtime_resume  = ektf2136_runtime_resume,
};
#endif

static struct spi_driver elan_touch_driver = {
	.probe		= elan_touch_probe,
	.remove		= elan_touch_remove,
	.driver		= {
		.name = "ektf2136_spi",
		.bus    = &spi_bus_type,
		.owner = THIS_MODULE,
#ifdef CONFIG_PM_RUNTIME
		.pm	= &ektf2136_pm_ops,
#endif
	},
};

static int __init elan_touch_init(void)
{
	int ret;

	elan_wq = create_singlethread_workqueue("elan_wq");
	if (!elan_wq)
		return -ENOMEM;
	ret = spi_register_driver(&elan_touch_driver);
	if (ret)
		destroy_workqueue(elan_wq);
	return ret;
}

static void __exit elan_touch_exit(void)
{
	spi_unregister_driver(&elan_touch_driver);
	destroy_workqueue(elan_wq);
}

late_initcall(elan_touch_init);
module_exit(elan_touch_exit);

MODULE_VERSION(DRIVER_VERSION);
MODULE_DESCRIPTION("ELAN Touch Screen driver");
MODULE_LICENSE("GPL");
