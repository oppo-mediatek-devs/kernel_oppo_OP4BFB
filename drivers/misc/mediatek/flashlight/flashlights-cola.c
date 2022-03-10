/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#define pr_fmt(fmt) KBUILD_MODNAME ": %s: " fmt, __func__

#include <linux/types.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/hrtimer.h>
#include <linux/ktime.h>
#include <linux/workqueue.h>
#include <linux/mutex.h>
#include <linux/of.h>
#include <linux/list.h>
#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/slab.h>
#include <linux/pinctrl/consumer.h>
/* Tao.Li@Camera.Driver, 2019/12/25, add for flashlight test start*/
#include <linux/leds.h>
/* Tao.Li@Camera.Driver, 2019/12/25, add for flashlight test end*/

#include "flashlight-core.h"
#include "flashlight-dt.h"
/* device tree should be defined in flashlight-dt.h */
#ifndef COLA_DTNAME
#define COLA_DTNAME "mediatek,flashlights_cola"
#endif
#ifndef COLA_DTNAME_I2C
#define COLA_DTNAME_I2C   "mediatek,strobe_main"
#endif

#undef pr_info
#define pr_info pr_err

#define COLA_NAME "flashlights-cola"

/* define registers */
#define COLA_REG_ENABLE           (0x01)
#define COLA_MASK_ENABLE_LED1     (0x01)
#define COLA_MASK_ENABLE_LED2     (0x02)
#define COLA_DISABLE              (0x00)
#define COLA_TORCH_MODE           (0x08)
#define COLA_FLASH_MODE           (0x0C)
#define COLA_ENABLE_LED1          (0x01)
#define COLA_ENABLE_LED1_TORCH    (0x09)
#define COLA_ENABLE_LED1_FLASH    (0x0D)
#define COLA_ENABLE_LED2          (0x02)
#define COLA_ENABLE_LED2_TORCH    (0x0A)
#define COLA_ENABLE_LED2_FLASH    (0x0E)

#define COLA_REG_TORCH_LEVEL_LED1 (0x05)
#define COLA_REG_FLASH_LEVEL_LED1 (0x03)
#define COLA_REG_TORCH_LEVEL_LED2 (0x06)
#define COLA_REG_FLASH_LEVEL_LED2 (0x04)

#define COLA_REG_TIMING_CONF      (0x08)
#define COLA_TORCH_RAMP_TIME      (0x10)
#define COLA_FLASH_TIMEOUT        (0x0F)



/* define channel, level */
#define COLA_CHANNEL_NUM          2
#define COLA_CHANNEL_CH1          0
#define COLA_CHANNEL_CH2          1
/* define level */
#define COLA_LEVEL_NUM 26
#define COLA_LEVEL_TORCH 7

#define COLA_HW_TIMEOUT 400 /* ms */

/* define mutex and work queue */
static DEFINE_MUTEX(cola_mutex);
static struct work_struct cola_work_ch1;
static struct work_struct cola_work_ch2;

/* define pinctrl */
#define COLA_PINCTRL_PIN_HWEN 0
#define COLA_PINCTRL_PINSTATE_LOW 0
#define COLA_PINCTRL_PINSTATE_HIGH 1
#define COLA_PINCTRL_STATE_HWEN_HIGH "hwen_high"
#define COLA_PINCTRL_STATE_HWEN_LOW  "hwen_low"

extern struct flashlight_operations sy7806_ops;
extern struct i2c_client *sy7806_i2c_client;

//////////////////////////////////////////////////////////////////////////////////////////////////////////////
static ssize_t cola_get_reg(struct device* cd,struct device_attribute *attr, char* buf);
static ssize_t cola_set_reg(struct device* cd, struct device_attribute *attr,const char* buf, size_t len);
static ssize_t cola_set_hwen(struct device* cd, struct device_attribute *attr,const char* buf, size_t len);
static ssize_t cola_get_hwen(struct device* cd, struct device_attribute *attr, char* buf);

static DEVICE_ATTR(reg, 0660, cola_get_reg,  cola_set_reg);
static DEVICE_ATTR(hwen, 0660, cola_get_hwen,  cola_set_hwen);

struct i2c_client *cola_flashlight_client;

static struct pinctrl *cola_pinctrl;
static struct pinctrl_state *cola_hwen_high;
static struct pinctrl_state *cola_hwen_low;

/* define usage count */
static int use_count;

/* define i2c */
static struct i2c_client *cola_i2c_client;

/* platform data */
struct cola_platform_data {
	int channel_num;
	struct flashlight_device_id *dev_id;
};

/* cola chip data */
struct cola_chip_data {
	struct i2c_client *client;
	struct cola_platform_data *pdata;
	struct mutex lock;
	u8 last_flag;
	u8 no_pdata;
};


/******************************************************************************
 * Pinctrl configuration
 *****************************************************************************/
static int cola_pinctrl_init(struct platform_device *pdev)
{
	int ret = 0;

       pr_err("cola_pinctrl_init start\n");
	/* get pinctrl */
	cola_pinctrl = devm_pinctrl_get(&pdev->dev);
	if (IS_ERR(cola_pinctrl)) {
		pr_err("Failed to get flashlight pinctrl.\n");
		ret = PTR_ERR(cola_pinctrl);
	}
	pr_err("devm_pinctrl_get finish %p\n", cola_pinctrl);

	/* Flashlight HWEN pin initialization */
	cola_hwen_high = pinctrl_lookup_state(cola_pinctrl, COLA_PINCTRL_STATE_HWEN_HIGH);
	if (IS_ERR(cola_hwen_high)) {
		pr_err("Failed to init (%s)\n", COLA_PINCTRL_STATE_HWEN_HIGH);
		ret = PTR_ERR(cola_hwen_high);
	}
	pr_err("pinctrl_lookup_state cola_hwen_high finish %p\n", cola_hwen_high);
	cola_hwen_low = pinctrl_lookup_state(cola_pinctrl, COLA_PINCTRL_STATE_HWEN_LOW);
	if (IS_ERR(cola_hwen_low)) {
		pr_err("Failed to init (%s)\n", COLA_PINCTRL_STATE_HWEN_LOW);
		ret = PTR_ERR(cola_hwen_low);
	}
	pr_err("pinctrl_lookup_state cola_hwen_low finish\n");

	return ret;
}

static int cola_pinctrl_set(int pin, int state)
{
	int ret = 0;

	if (IS_ERR(cola_pinctrl)) {
		pr_err("pinctrl is not available\n");
		return -1;
	}

	switch (pin) {
	case COLA_PINCTRL_PIN_HWEN:
		if (state == COLA_PINCTRL_PINSTATE_LOW && !IS_ERR(cola_hwen_low))
			pinctrl_select_state(cola_pinctrl, cola_hwen_low);
		else if (state == COLA_PINCTRL_PINSTATE_HIGH && !IS_ERR(cola_hwen_high))
			pinctrl_select_state(cola_pinctrl, cola_hwen_high);
		else
			pr_err("set err, pin(%d) state(%d)\n", pin, state);
		break;
	default:
		pr_err("set err, pin(%d) state(%d)\n", pin, state);
		break;
	}
	pr_info("pin(%d) state(%d)\n", pin, state);

	return ret;
}

/******************************************************************************
 * cola operations
 *****************************************************************************/
static const int *cola_current;
static const unsigned char *cola_torch_level;
static const unsigned char *cola_flash_level;


static const int aw3643_current[COLA_LEVEL_NUM] = {
	22,  46,  70,  93,  116, 140, 163, 198, 234, 281,
	328, 363, 410, 457,  492, 539, 586, 621, 668, 715,
	762, 809, 855, 902, 949, 996
};

static const unsigned char aw3643_torch_level[COLA_LEVEL_NUM] = {
    0x06, 0x0F, 0x17, 0x1F, 0x27, 0x2F, 0x37, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

static const unsigned char aw3643_flash_level[COLA_LEVEL_NUM] = {
	0x01, 0x03, 0x05, 0x07, 0x09, 0x0B, 0x0D, 0x10, 0x13, 0x17,
	0x1B, 0x1E, 0x22, 0x26, 0x29, 0x2D, 0x31, 0x34, 0x38, 0x3C,
	0x40, 0x44, 0x48, 0x4C, 0x50, 0x54};


static const int sy7806_current[COLA_LEVEL_NUM] = {
	24,  46, 70,  93,  116, 140, 164, 199, 234, 281,
	328, 363, 410, 457,  492, 539, 586, 621, 668, 715,
	762, 809, 855, 902, 949, 996
};

static const unsigned char sy7806_torch_level[COLA_LEVEL_NUM] = {
    0x08, 0x10, 0x18, 0x21, 0x29, 0x31, 0x3A, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

static const unsigned char sy7806_flash_level[COLA_LEVEL_NUM] = {
	0x01, 0x03, 0x05, 0x07, 0x09, 0x0B, 0x0D, 0x10, 0x13, 0x17,
	0x1B, 0x1E, 0x22, 0x26, 0x29, 0x2D, 0x31, 0x34, 0x38, 0x3C,
	0x40, 0x44, 0x48, 0x4C, 0x50, 0x54};

static volatile unsigned char cola_reg_enable;
static volatile int cola_level_ch1 = -1;
static volatile int cola_level_ch2 = -1;

static int cola_is_torch(int level)
{
	if (level >= COLA_LEVEL_TORCH)
		return -1;

	return 0;
}

static int cola_verify_level(int level)
{
	if (level < 0)
		level = 0;
	else if (level >= COLA_LEVEL_NUM)
		level = COLA_LEVEL_NUM - 1;

	return level;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////
// i2c write and read
//////////////////////////////////////////////////////////////////////////////////////////////////////////////
static int cola_write_reg(struct i2c_client *client, u8 reg, u8 val)
{
	int ret;
	struct cola_chip_data *chip = i2c_get_clientdata(client);

	mutex_lock(&chip->lock);
	ret = i2c_smbus_write_byte_data(client, reg, val);
	mutex_unlock(&chip->lock);

	if (ret < 0)
		pr_err("failed writing at 0x%02x\n", reg);

	return ret;
}

/* i2c wrapper function */
static int cola_read_reg(struct i2c_client *client, u8 reg)
{
	int val;
	struct cola_chip_data *chip = i2c_get_clientdata(client);

	mutex_lock(&chip->lock);
	val = i2c_smbus_read_byte_data(client, reg);
	mutex_unlock(&chip->lock);
	if (val < 0)
		pr_err("failed read at 0x%02x\n", reg);

	return val;
}

/* flashlight enable function */
static int cola_enable_ch1(void)
{
	unsigned char reg, val;

	reg = COLA_REG_ENABLE;
	if (!cola_is_torch(cola_level_ch1)) {
		/* torch mode */
		cola_reg_enable |= COLA_ENABLE_LED1_TORCH;
	} else {
		/* flash mode */
		cola_reg_enable |= COLA_ENABLE_LED1_FLASH;
	}
	val = cola_reg_enable;

	return cola_write_reg(cola_i2c_client, reg, val);
}

static int cola_enable_ch2(void)
{
	unsigned char reg, val;

	reg = COLA_REG_ENABLE;
	if (!cola_is_torch(cola_level_ch2)) {
		/* torch mode */
		cola_reg_enable |= COLA_ENABLE_LED2_TORCH;
	} else {
		/* flash mode */
		cola_reg_enable |= COLA_ENABLE_LED2_FLASH;
	}
	val = cola_reg_enable;

	return cola_write_reg(cola_i2c_client, reg, val);
}

static int cola_enable(int channel)
{
	if (channel == COLA_CHANNEL_CH1)
		cola_enable_ch1();
	else if (channel == COLA_CHANNEL_CH2)
		cola_enable_ch2();
	else {
		pr_err("Error channel\n");
		return -1;
	}

	return 0;
}

/* flashlight disable function */
static int cola_disable_ch1(void)
{
	unsigned char reg, val;

	reg = COLA_REG_ENABLE;
	if (cola_reg_enable & COLA_MASK_ENABLE_LED2) {
		/* if LED 2 is enable, disable LED 1 */
		cola_reg_enable &= (~COLA_ENABLE_LED1);
	} else {
		/* if LED 2 is enable, disable LED 1 and clear mode */
		cola_reg_enable &= (~COLA_ENABLE_LED1_FLASH);
	}
	val = cola_reg_enable;

	return cola_write_reg(cola_i2c_client, reg, val);
}

static int cola_disable_ch2(void)
{
	unsigned char reg, val;

	reg = COLA_REG_ENABLE;
	if (cola_reg_enable & COLA_MASK_ENABLE_LED1) {
		/* if LED 1 is enable, disable LED 2 */
		cola_reg_enable &= (~COLA_ENABLE_LED2);
	} else {
		/* if LED 1 is enable, disable LED 2 and clear mode */
		cola_reg_enable &= (~COLA_ENABLE_LED2_FLASH);
	}
	val = cola_reg_enable;

	return cola_write_reg(cola_i2c_client, reg, val);
}

static int cola_disable(int channel)
{
	if (channel == COLA_CHANNEL_CH1){
		cola_disable_ch1();
		pr_info("COLA_CHANNEL_CH1\n");
		}
	else if (channel == COLA_CHANNEL_CH2){
		cola_disable_ch2();
		pr_info("COLA_CHANNEL_CH2\n");
		}
	else {
		pr_err("Error channel\n");
		return -1;
	}

	return 0;
}

/* set flashlight level */
static int cola_set_level_ch1(int level)
{
	int ret;
	unsigned char reg, val;

	level = cola_verify_level(level);

	/* set torch brightness level */
	reg = COLA_REG_TORCH_LEVEL_LED1;
	val = cola_torch_level[level];
	ret = cola_write_reg(cola_i2c_client, reg, val);

	cola_level_ch1 = level;

	/* set flash brightness level */
	reg = COLA_REG_FLASH_LEVEL_LED1;
	val = cola_flash_level[level];
	ret = cola_write_reg(cola_i2c_client, reg, val);

	return ret;
}

int cola_set_level_ch2(int level)
{
	int ret;
	unsigned char reg, val;

	level = cola_verify_level(level);

	/* set torch brightness level */
	reg = COLA_REG_TORCH_LEVEL_LED2;
	val = cola_torch_level[level];
	ret = cola_write_reg(cola_i2c_client, reg, val);

	cola_level_ch2 = level;

	/* set flash brightness level */
	reg = COLA_REG_FLASH_LEVEL_LED2;
	val = cola_flash_level[level];
	ret = cola_write_reg(cola_i2c_client, reg, val);

	return ret;
}

static int cola_set_level(int channel, int level)
{

	if (channel == COLA_CHANNEL_CH1)
		cola_set_level_ch1(level);
	else if (channel == COLA_CHANNEL_CH2)
		cola_set_level_ch2(level);
	else {
		pr_err("Error channel\n");
		return -1;
	}

	return 0;
}

/* flashlight init */
int cola_init(void)
{
	int ret;
	unsigned char reg, val;

	cola_pinctrl_set(COLA_PINCTRL_PIN_HWEN, COLA_PINCTRL_PINSTATE_HIGH);
    msleep(2);

	/* clear enable register */
	reg = COLA_REG_ENABLE;
	val = COLA_DISABLE;
	ret = cola_write_reg(cola_i2c_client, reg, val);

	cola_reg_enable = val;

	/* set torch current ramp time and flash timeout */
	reg = COLA_REG_TIMING_CONF;
	val = COLA_TORCH_RAMP_TIME | COLA_FLASH_TIMEOUT;
	ret = cola_write_reg(cola_i2c_client, reg, val);

	return ret;
}

/* flashlight uninit */
int cola_uninit(void)
{
	cola_disable(COLA_CHANNEL_CH1);
	cola_disable(COLA_CHANNEL_CH2);
	cola_pinctrl_set(COLA_PINCTRL_PIN_HWEN, COLA_PINCTRL_PINSTATE_LOW);

	return 0;
}


/******************************************************************************
 * Timer and work queue
 *****************************************************************************/
static struct hrtimer cola_timer_ch1;
static struct hrtimer cola_timer_ch2;
static unsigned int cola_timeout_ms[COLA_CHANNEL_NUM];

static void cola_work_disable_ch1(struct work_struct *data)
{
	pr_info("ht work queue callback\n");

	cola_disable_ch1();
}

static void cola_work_disable_ch2(struct work_struct *data)
{
	pr_info("lt work queue callback\n");

	cola_disable_ch2();
}

static enum hrtimer_restart cola_timer_func_ch1(struct hrtimer *timer)
{
	schedule_work(&cola_work_ch1);
	return HRTIMER_NORESTART;
}

static enum hrtimer_restart cola_timer_func_ch2(struct hrtimer *timer)
{
	schedule_work(&cola_work_ch2);
	return HRTIMER_NORESTART;
}

int cola_timer_start(int channel, ktime_t ktime)
{
	if (channel == COLA_CHANNEL_CH1)
		hrtimer_start(&cola_timer_ch1, ktime, HRTIMER_MODE_REL);
	else if (channel == COLA_CHANNEL_CH2)
		hrtimer_start(&cola_timer_ch2, ktime, HRTIMER_MODE_REL);
	else {
		pr_err("Error channel\n");
		return -1;
	}

	return 0;
}

int cola_timer_cancel(int channel)
{
	if (channel == COLA_CHANNEL_CH1)
		hrtimer_cancel(&cola_timer_ch1);
	else if (channel == COLA_CHANNEL_CH2)
		hrtimer_cancel(&cola_timer_ch2);
	else {
		pr_err("Error channel\n");
		return -1;
	}

	return 0;
}


/******************************************************************************
 * Flashlight operations
 *****************************************************************************/
static int cola_ioctl(unsigned int cmd, unsigned long arg)
{
	struct flashlight_dev_arg *fl_arg;
	int channel;
	ktime_t ktime;

	fl_arg = (struct flashlight_dev_arg *)arg;
	channel = fl_arg->channel;

	/* verify channel */
	if (channel < 0 || channel >= COLA_CHANNEL_NUM) {
		pr_err("Failed with error channel\n");
		return -EINVAL;
	}
	switch (cmd) {
	case FLASH_IOC_SET_TIME_OUT_TIME_MS:
		pr_info("FLASH_IOC_SET_TIME_OUT_TIME_MS(%d): %d\n",
				channel, (int)fl_arg->arg);
		cola_timeout_ms[channel] = fl_arg->arg;
		break;

	case FLASH_IOC_SET_DUTY:
		pr_info("FLASH_IOC_SET_DUTY(%d): %d\n",
				channel, (int)fl_arg->arg);
		cola_set_level(channel, fl_arg->arg);
		break;

	case FLASH_IOC_SET_ONOFF:
		pr_info("FLASH_IOC_SET_ONOFF(%d): %d\n",
				channel, (int)fl_arg->arg);
		if (fl_arg->arg == 1) {
			if (cola_timeout_ms[channel]) {
				ktime = ktime_set(cola_timeout_ms[channel] / 1000,
						(cola_timeout_ms[channel] % 1000) * 1000000);
				cola_timer_start(channel, ktime);
			}
			cola_enable(channel);
		} else {
			cola_disable(channel);
			cola_timer_cancel(channel);
		}
		break;

	case FLASH_IOC_GET_DUTY_NUMBER:
		pr_info("FLASH_IOC_GET_DUTY_NUMBER(%d)\n", channel);
		fl_arg->arg = COLA_LEVEL_NUM;
		break;

	case FLASH_IOC_GET_MAX_TORCH_DUTY:
		pr_info("FLASH_IOC_GET_MAX_TORCH_DUTY(%d)\n", channel);
		fl_arg->arg = COLA_LEVEL_TORCH - 1;
		break;

	case FLASH_IOC_GET_DUTY_CURRENT:
		fl_arg->arg = cola_verify_level(fl_arg->arg);
		pr_info("FLASH_IOC_GET_DUTY_CURRENT(%d): %d\n",
				channel, (int)fl_arg->arg);
		fl_arg->arg = cola_current[fl_arg->arg];
		break;

	case FLASH_IOC_GET_HW_TIMEOUT:
		pr_info("FLASH_IOC_GET_HW_TIMEOUT(%d)\n", channel);
		fl_arg->arg = COLA_HW_TIMEOUT;
		break;

	default:
		pr_info("No such command and arg(%d): (%d, %d)\n",
				channel, _IOC_NR(cmd), (int)fl_arg->arg);
		return -ENOTTY;
	}

	return 0;
}

static int cola_open(void)
{
	/* Actual behavior move to set driver function since power saving issue */
	return 0;
}

static int cola_release(void)
{
	/* uninit chip and clear usage count */
/*
	mutex_lock(&cola_mutex);
	use_count--;
	if (!use_count)
		cola_uninit();
	if (use_count < 0)
		use_count = 0;
	mutex_unlock(&cola_mutex);

	pr_info("Release: %d\n", use_count);
*/
	return 0;
}

static int cola_set_driver(int set)
{
	int ret = 0;

	/* set chip and usage count */
	mutex_lock(&cola_mutex);
	if (set) {
		if (!use_count)
			ret = cola_init();
		use_count++;
		pr_info("Set driver: %d\n", use_count);
	} else {
		use_count--;
		if (!use_count)
			ret = cola_uninit();
		if (use_count < 0)
			use_count = 0;
		pr_info("Unset driver: %d\n", use_count);
	}
	mutex_unlock(&cola_mutex);

	return ret;
}

static ssize_t cola_strobe_store(struct flashlight_arg arg)
{
	cola_set_driver(1);
	cola_set_level(arg.channel, arg.level);
	cola_timeout_ms[arg.channel] = 0;
	cola_enable(arg.channel);
	msleep(arg.dur);
	cola_disable(arg.channel);
	//cola_release(NULL);
	cola_set_driver(0);
	return 0;
}

static struct flashlight_operations cola_ops = {
	cola_open,
	cola_release,
	cola_ioctl,
	cola_strobe_store,
	cola_set_driver
};

/******************************************************************************
 * I2C device and driver
 *****************************************************************************/
static int cola_chip_init(struct cola_chip_data *chip)
{
	/* NOTE: Chip initialication move to "set driver" operation for power saving issue.
	 * cola_init();
	 */

	return 0;
}


////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//COLA Debug file
////////////////////////////////////////////////////////////////////////////////////////////////////////////////
static ssize_t cola_get_reg(struct device* cd,struct device_attribute *attr, char* buf)
{
	unsigned char reg_val;
	unsigned char i;
	ssize_t len = 0;
	for(i=0;i<0x0E;i++)
	{
		reg_val = cola_read_reg(cola_i2c_client,i);
		len += snprintf(buf+len, PAGE_SIZE-len, "reg%2X = 0x%2X \n, ", i,reg_val);
	}
	len += snprintf(buf+len, PAGE_SIZE-len, "\r\n");
	return len;
}

static ssize_t cola_set_reg(struct device* cd, struct device_attribute *attr, const char* buf, size_t len)
{
	unsigned int databuf[2];
	if(2 == sscanf(buf,"%x %x",&databuf[0], &databuf[1]))
	{
		//i2c_write_reg(databuf[0],databuf[1]);
		cola_write_reg(cola_i2c_client,databuf[0],databuf[1]);
	}
	return len;
}

static ssize_t cola_get_hwen(struct device* cd,struct device_attribute *attr, char* buf)
{
	ssize_t len = 0;
	len += snprintf(buf+len, PAGE_SIZE-len, "//cola_hwen_on(void)\n");
	len += snprintf(buf+len, PAGE_SIZE-len, "echo 1 > hwen\n");
	len += snprintf(buf+len, PAGE_SIZE-len, "\n");
	len += snprintf(buf+len, PAGE_SIZE-len, "//cola_hwen_off(void)\n");
	len += snprintf(buf+len, PAGE_SIZE-len, "echo 0 > hwen\n");
	len += snprintf(buf+len, PAGE_SIZE-len, "\n");

	return len;
}

static ssize_t cola_set_hwen(struct device* cd, struct device_attribute *attr, const char* buf, size_t len)
{
	unsigned char databuf[16];

	sscanf(buf,"%c",&databuf[0]);
#if 1
	if(databuf[0] == 0) {			// OFF
		//cola_hwen_low();
	} else {				// ON
		//cola_hwen_high();
	}
#endif
	return len;
}

static int cola_create_sysfs(struct i2c_client *client)
{
	int err;
	struct device *dev = &(client->dev);

	err = device_create_file(dev, &dev_attr_reg);
	err = device_create_file(dev, &dev_attr_hwen);

	return err;
}

static int cola_parse_dt(struct device *dev,
		struct cola_platform_data *pdata)
{
	struct device_node *np, *cnp;
	u32 decouple = 0;
	int i = 0;

	if (!dev || !dev->of_node || !pdata)
		return -ENODEV;

	np = dev->of_node;

	pdata->channel_num = of_get_child_count(np);
	if (!pdata->channel_num) {
		pr_info("Parse no dt, node.\n");
		return 0;
	}
	pr_info("Channel number(%d).\n", pdata->channel_num);

	if (of_property_read_u32(np, "decouple", &decouple))
		pr_info("Parse no dt, decouple.\n");

	pdata->dev_id = devm_kzalloc(dev,
			pdata->channel_num *
			sizeof(struct flashlight_device_id),
			GFP_KERNEL);
	if (!pdata->dev_id)
		return -ENOMEM;

	for_each_child_of_node(np, cnp) {
		if (of_property_read_u32(cnp, "type", &pdata->dev_id[i].type))
			goto err_node_put;
		if (of_property_read_u32(cnp, "ct", &pdata->dev_id[i].ct))
			goto err_node_put;
		if (of_property_read_u32(cnp, "part", &pdata->dev_id[i].part))
			goto err_node_put;
		snprintf(pdata->dev_id[i].name, FLASHLIGHT_NAME_SIZE,
				COLA_NAME);
		pdata->dev_id[i].channel = i;
		pdata->dev_id[i].decouple = decouple;

		pr_info("Parse dt (type,ct,part,name,channel,decouple)=(%d,%d,%d,%s,%d,%d).\n",
				pdata->dev_id[i].type, pdata->dev_id[i].ct,
				pdata->dev_id[i].part, pdata->dev_id[i].name,
				pdata->dev_id[i].channel,
				pdata->dev_id[i].decouple);
		i++;
	}

	return 0;

err_node_put:
	of_node_put(cnp);
	return -EINVAL;
}

enum FLASHLIGHT_DEVICE {
	AW3643_SM = 0x12,
	SY7806_SM = 0x1c,
};

#define USE_AW3643_IC	0x0001
#define USE_SY7806_IC	0x0011
#define USE_NOT_PRO		0x1111

static int cola_chip_id(void)
{
	char chip_id;

	cola_pinctrl_set(COLA_PINCTRL_PIN_HWEN, COLA_PINCTRL_PINSTATE_HIGH);
	chip_id = cola_read_reg(cola_i2c_client, 0x0c);
	pr_info("flashlight chip id: reg:0x0c, data:0x%x", chip_id);
	cola_pinctrl_set(COLA_PINCTRL_PIN_HWEN, COLA_PINCTRL_PINSTATE_LOW);

	if (chip_id == AW3643_SM) {
		pr_info(" the device's flashlight driver IC is AW3643\n");
		return USE_AW3643_IC;
	} else if (chip_id == SY7806_SM){
		pr_info(" the device's flashlight driver IC is SY7806\n");
		return USE_SY7806_IC;
	} else {
		pr_err(" the device's flashlight driver IC is not used in our project!\n");
		return USE_NOT_PRO;
	}
}

/* Tao.Li@Camera.Driver, 2019/12/25, add for flashlight test start*/
static int global_level = 0;
static void cola_flashlight_brightness_set(struct led_classdev *led_cdev,
		enum led_brightness value)
{
	if((value < 0) || (value > 25)){
		pr_err("Error brightness value %d",value);
		return ;
	}
	pr_info("cola_flashlight_brightness_set value= %d",value);
	if(0 == value){
		cola_disable(COLA_CHANNEL_CH1);
		cola_pinctrl_set(COLA_PINCTRL_PIN_HWEN, COLA_PINCTRL_PINSTATE_LOW);
		//cola_timer_cancel(COLA_CHANNEL_CH1);

	}else{
		cola_pinctrl_set(COLA_PINCTRL_PIN_HWEN, COLA_PINCTRL_PINSTATE_HIGH);
		cola_set_level(COLA_CHANNEL_CH1,value);
		cola_enable(COLA_CHANNEL_CH1);

	}
	global_level = value;
}
/* Tao.Li@Camera.Driver, 2019/12/25, add for flashlight test end*/

static enum led_brightness cola_flashlight_brightness_get(struct led_classdev *led_cdev)
{
	return (global_level > 0) ? global_level : 0;
}

static struct led_classdev cola_flashlight_led = {
		.name           = "flashlight",
		.brightness_set = cola_flashlight_brightness_set,
		.brightness_get = cola_flashlight_brightness_get,
		.brightness     = LED_OFF,
};

static int cola_flashlight_create_classdev(struct platform_device *pdev)
{
	int32_t rc = 0;
	rc = led_classdev_register(&pdev->dev, &cola_flashlight_led);
	if (rc) {
		pr_err("Failed to register  led dev. rc = %d\n", rc);
		return rc;
	}
	return 0;
}

static int cola_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct cola_chip_data *chip;
	struct cola_platform_data *pdata = client->dev.platform_data;
	int err;
	int i;
	int chip_id;

	pr_info("Probe start.\n");

	/* check i2c */
	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		pr_err("Failed to check i2c functionality.\n");
		err = -ENODEV;
		goto err_out;
	}

	/* init chip private data */
	chip = kzalloc(sizeof(struct cola_chip_data), GFP_KERNEL);
	if (!chip) {
		err = -ENOMEM;
		goto err_out;
	}
	chip->client = client;

	/* init platform data */
	if (!pdata) {
		pr_err("Platform data does not exist\n");
		pdata = kzalloc(sizeof(struct cola_platform_data), GFP_KERNEL);
		if (!pdata) {
			err = -ENOMEM;
			goto err_free;
		}
		client->dev.platform_data = pdata;
		err = cola_parse_dt(&client->dev, pdata);
		if (err)
			goto err_free;
	}
	chip->pdata = pdata;
	i2c_set_clientdata(client, chip);
	cola_i2c_client = client;

	/* init mutex and spinlock */
	mutex_init(&chip->lock);

	/* init work queue */
	INIT_WORK(&cola_work_ch1, cola_work_disable_ch1);
	INIT_WORK(&cola_work_ch2, cola_work_disable_ch2);

	/* init timer */
	hrtimer_init(&cola_timer_ch1, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	cola_timer_ch1.function = cola_timer_func_ch1;
	hrtimer_init(&cola_timer_ch2, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	cola_timer_ch2.function = cola_timer_func_ch2;
	cola_timeout_ms[COLA_CHANNEL_CH1] = 100;
	cola_timeout_ms[COLA_CHANNEL_CH2] = 100;

	/* init chip hw */
	cola_chip_init(chip);

	chip_id = cola_chip_id();
	if(chip_id == USE_AW3643_IC){
		cola_current = aw3643_current;
		cola_torch_level = aw3643_torch_level;
		cola_flash_level = aw3643_flash_level;
	}
	else if (chip_id == USE_SY7806_IC){
		cola_current = sy7806_current;
		cola_torch_level = sy7806_torch_level;
		cola_flash_level = sy7806_flash_level;
	} else if (chip_id == USE_NOT_PRO){
		cola_current = aw3643_current;
		cola_torch_level = aw3643_torch_level;
		cola_flash_level = aw3643_flash_level;
	}

	/* register flashlight operations */
	if (pdata->channel_num) {
		for (i = 0; i < pdata->channel_num; i++)
			if (flashlight_dev_register_by_device_id(
						&pdata->dev_id[i],
						&cola_ops)) {
                pr_err("Failed to register flashlight device.\n");
				err = -EFAULT;
				goto err_free;
			}
	} else {
		if (flashlight_dev_register(COLA_NAME, &cola_ops)) {
			pr_err("Failed to register flashlight device.\n");
			err = -EFAULT;
			goto err_free;
		}
	}

    cola_create_sysfs(client);

	pr_info("Probe done.\n");

	return 0;

err_free:
	i2c_set_clientdata(client, NULL);
	kfree(chip);
err_out:
	return err;
}

static int cola_i2c_remove(struct i2c_client *client)
{
	struct cola_platform_data *pdata = dev_get_platdata(&client->dev);
	struct cola_chip_data *chip = i2c_get_clientdata(client);
	int i;

	pr_info("Remove start.\n");

	client->dev.platform_data = NULL;

	/* unregister flashlight device */
	if (pdata && pdata->channel_num)
		for (i = 0; i < pdata->channel_num; i++)
			flashlight_dev_unregister_by_device_id(
					&pdata->dev_id[i]);
	else
		flashlight_dev_unregister(COLA_NAME);
	/* flush work queue */
	flush_work(&cola_work_ch1);
	flush_work(&cola_work_ch2);

	/* unregister flashlight operations */
	flashlight_dev_unregister(COLA_NAME);

	/* free resource */
	if (chip->no_pdata)
		kfree(chip->pdata);
	kfree(chip);

	pr_info("Remove done.\n");

	return 0;
}

static const struct i2c_device_id cola_i2c_id[] = {
	{COLA_NAME, 0},
	{}
};

#ifdef CONFIG_OF
static const struct of_device_id cola_i2c_of_match[] = {
	{.compatible = COLA_DTNAME_I2C},
	{},
};
#endif

static struct i2c_driver cola_i2c_driver = {
	.driver = {
		   .name = COLA_NAME,
#ifdef CONFIG_OF
		   .of_match_table = cola_i2c_of_match,
#endif
		   },
	.probe = cola_i2c_probe,
	.remove = cola_i2c_remove,
	.id_table = cola_i2c_id,
};


/******************************************************************************
 * Platform device and driver
 *****************************************************************************/
static int cola_probe(struct platform_device *dev)
{
	pr_info("Probe start. \n");

	/* init pinctrl */
	if (cola_pinctrl_init(dev)) {
		printk("Failed to init pinctrl.\n");
		return -1;
	}
       pr_err("cola_pinctrl_init finish\n");
	if (i2c_add_driver(&cola_i2c_driver)) {
		printk("Failed to add i2c driver.\n");
		return -1;
	}
	/* Tao.Li@Camera.Driver, 2019/12/25, add for flashlight test start*/
	cola_flashlight_create_classdev(dev);
	/* Tao.Li@Camera.Driver, 2019/12/25, add for flashlight test end */
	pr_info("Probe done.\n");

	return 0;
}

static int cola_remove(struct platform_device *dev)
{
	pr_info("Remove start.\n");

	i2c_del_driver(&cola_i2c_driver);

	pr_info("Remove done.\n");

	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id cola_of_match[] = {
	{.compatible = COLA_DTNAME},
	{},
};
MODULE_DEVICE_TABLE(of, cola_of_match);
#else
static struct platform_device cola_platform_device[] = {
	{
		.name = COLA_NAME,
		.id = 0,
		.dev = {}
	},
	{}
};
MODULE_DEVICE_TABLE(platform, cola_platform_device);
#endif

static struct platform_driver cola_platform_driver = {
	.probe = cola_probe,
	.remove = cola_remove,
	.driver = {
		.name = COLA_NAME,
		.owner = THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table = cola_of_match,
#endif
	},
};

static int __init flashlight_cola_init(void)
{
	int ret;

	pr_info("flashlight_cola-Init start.\n");

#ifndef CONFIG_OF
	ret = platform_device_register(&cola_platform_device);
	if (ret) {
		pr_err("Failed to register platform device\n");
		return ret;
	}
#endif
	pr_info("platform_driver_register start.\n");

	ret = platform_driver_register(&cola_platform_driver);
	if (ret) {
		pr_err("Failed to register platform driver\n");
		return ret;
	}

	pr_info("flashlight_cola Init done.\n");

	return 0;
}

static void __exit flashlight_cola_exit(void)
{
	pr_info("flashlight_cola-Exit start.\n");

	platform_driver_unregister(&cola_platform_driver);

	pr_info("flashlight_cola Exit done.\n");
}


module_init(flashlight_cola_init);
module_exit(flashlight_cola_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Joseph <zhangzetao@awinic.com.cn>");
MODULE_DESCRIPTION("AW Flashlight COLA Driver");


