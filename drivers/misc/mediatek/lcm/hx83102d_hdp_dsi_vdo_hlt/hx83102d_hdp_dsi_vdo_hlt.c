/********************************************
 ** Copyright (C) 2018 OPPO Mobile Comm Corp. Ltd.
 ** ODM_WT_EDIT
 ** File: ili9881_hd_dsi_vdo_txd_boe.c
 ** Description: Source file for LCD driver
 **          To Control LCD driver
 ** Version :1.0
 ** Date : 2018/11/29
 ** Author: Zhenzhen.Wt@ODM_WT.Multimedia.LCD
 ** ---------------- Revision History: --------------------------
 ** <version>    <date>          < author >              <desc>
 **  1.0           2018/11/29   Wangxianfei@ODM_HQ   Source file for LCD driver
 ********************************************/

#include "../inc/lcm_drv.h"
#include "../inc/lcm_gpio.h"

#ifndef BUILD_LK
#include <linux/string.h>
#include <linux/kernel.h>
#include <linux/regulator/consumer.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/list.h>
#include <linux/i2c.h>
#include <linux/irq.h>
#include <linux/uaccess.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/platform_device.h>
#endif
#ifdef BUILD_LK
#include <platform/upmu_common.h>
#include <platform/mt_gpio.h>
#include <platform/mt_i2c.h>
#include <platform/mt_pmic.h>
#include <string.h>
#elif defined(BUILD_UBOOT)
#include <asm/arch/mt_gpio.h>
#endif

#define LOG_TAG "LCM_HLT_HX_WT"

#ifdef BUILD_LK
#define LCM_LOGI(string, args...)  dprintf(ALWAYS, "[LK/"LOG_TAG"]"string, ##args)
#define LCM_LOGD(string, args...)  dprintf(INFO, "[LK/"LOG_TAG"]"string, ##args)
#else
#define LCM_LOGI(fmt, args...)  printk("[KERNEL/"LOG_TAG"]"fmt, ##args)
#define LCM_LOGD(fmt, args...)  printk("[KERNEL/"LOG_TAG"]"fmt, ##args)
#endif

static struct LCM_UTIL_FUNCS lcm_util;

#define SET_RESET_PIN(v)    (lcm_util.set_reset_pin((v)))
#define MDELAY(n)       (lcm_util.mdelay(n))
#define UDELAY(n)       (lcm_util.udelay(n))


/* --------------------------------------------------------------------------- */
/* Local Functions */
/* --------------------------------------------------------------------------- */

#define dsi_set_cmdq_V22(cmdq, cmd, count, ppara, force_update) \
	lcm_util.dsi_set_cmdq_V22(cmdq, cmd, count, ppara, force_update)
#define dsi_set_cmdq_V2(cmd, count, ppara, force_update) \
    lcm_util.dsi_set_cmdq_V2(cmd, count, ppara, force_update)
#define dsi_set_cmdq(pdata, queue_size, force_update) \
        lcm_util.dsi_set_cmdq(pdata, queue_size, force_update)
#define wrtie_cmd(cmd) lcm_util.dsi_write_cmd(cmd)
#define write_regs(addr, pdata, byte_nums) \
        lcm_util.dsi_write_regs(addr, pdata, byte_nums)
#define read_reg(cmd) \
      lcm_util.dsi_dcs_read_lcm_reg(cmd)
#define read_reg_v2(cmd, buffer, buffer_size) \
        lcm_util.dsi_dcs_read_lcm_reg_v2(cmd, buffer, buffer_size)

#ifndef BUILD_LK
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/list.h>
#include <linux/i2c.h>
#include <linux/irq.h>
#include <linux/uaccess.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/platform_device.h>
#endif
/* --------------------------------------------------------------------------- */
/* Local Constants */
/* --------------------------------------------------------------------------- */
#define LCM_DSI_CMD_MODE                                    0
#define FRAME_WIDTH                                     (720)
#define FRAME_HEIGHT                                    (1520)

//#define GPIO_LCM_ID_0 GPIO163
//#define GPIO_LCM_ID_1 GPIO164
//#define GPIO_LCM_ID_2 GPIO167
#define GPIO_LCD_LED_EN_N GPIO151
#define GPIO_LCD_LED_EN_P GPIO150


#define REGFLAG_DELAY       0xFFFC
#define REGFLAG_UDELAY  0xFFFB
#define REGFLAG_END_OF_TABLE    0xFFFD
#define REGFLAG_RESET_LOW   0xFFFE
#define REGFLAG_RESET_HIGH  0xFFFF

#ifndef TRUE
#define TRUE 1
#endif

#ifndef FALSE
#define FALSE 0
#endif
//extern unsigned int esd_recovery_backlight_level;
struct LCM_setting_table {
	unsigned int cmd;
	unsigned char count;
	unsigned char para_list[64];
};

static struct LCM_setting_table lcm_suspend_setting[] = {
	//{0xFF, 0x03, {0x98,0x81,0x00}},
	{0x28, 0, {}},
	{REGFLAG_DELAY, 20, {}},
	{0x10, 0, {}},
	{REGFLAG_DELAY, 120, {}}
};
static unsigned char lcd_bias_id;

//cmdline lcd_bias_id
static int __init bias_id(char *str)
{
	int id;

	if (get_option(&str , &id))
		lcd_bias_id = (unsigned char)id;
	return 0;
}
__setup("lcd_bias_id=", bias_id);
/* wangxianfei@ODM.HQ.Multimedia.LCM 2018/12/21 modified for backlight remapping*/
static int blmap_table[] = {
	36, 16,
	16, 22,
	17, 21,
	19, 20,
	19, 20,
	20, 17,
	22, 15,
	22, 14,
	24, 10,
	24, 8,
	26, 4,
	27, 0,
	29, 9,
	29, 9,
	30, 14,
	33, 25,
	34, 30,
	36, 44,
	37, 49,
	40, 65,
	40, 69,
	43, 88,
	46, 109,
	47, 112,
	50, 135,
	53, 161,
	53, 163,
	60, 220,
	60, 223,
	64, 257,
	63, 255,
	71, 334,
	71, 331,
	75, 375,
	80, 422,
	84, 473,
	89, 529,
	88, 518,
	99, 653,
	98, 640,
	103, 707,
	117, 878,
	115, 862,
	122, 947,
	128, 1039,
	135, 1138,
	132, 1102,
	149, 1355,
	157, 1478,
	166, 1611,
	163, 1563,
	183, 1900,
	180, 1844,
	203, 2232,
	199, 2169,
	209, 2344,
	236, 2821,
	232, 2742,
	243, 2958,
	255, 3188,
	268, 3433,
	282, 3705,
	317, 4400,
	176, 1555};
static struct LCM_setting_table init_setting[] = {
	{0xB9, 3, {0x83,0x10,0x2D}},
    {0xB1, 11, {0x22,0x22,0x33,0x33,0x33,0x54,0x46,0x57,0x08,0x08,0x08}},
    {0xB2, 14, {0x00,0x00,0x05,0xF0,0x00,0x10,0xBE,0x37,0x00,0x00,0x00,0x00,0xF4,0xA0}},
    {0xB4, 14, {0x55,0x55,0x55,0x55,0x55,0x55,0x55,0x55,0x03,0xFF,0x01,0x20,0x00,0xFF}},
    {0xCC, 1, {0x02}},
    {0xD3, 38, {0x00,0x00,0x00,0x00,0x00,0x08,0x08,0x47,0x00,0x44,0x33,0x0B,0x0B,0x00,0x00,0x32,0x10,0x08,0x00,0x0B,0x32,0x16,0x29,0x06,0x29,0x32,0x10,0x08,0x00,0x00,0x00,0x00,0x00,0x1B,0x00,0x00,0x00,0x1B}},
    {REGFLAG_DELAY, 5, {}},
    {0xD5, 44, {0x18,0x18,0x39,0x39,0x18,0x18,0x22,0x23,0x20,0x21,0x18,0x18,0x04,0x05,0x06,0x07,0x00,0x01,0x02,0x03,0x38,0x38,0x19,0x19,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x18}},
    {REGFLAG_DELAY, 5, {}},
    {0xD6, 44, {0x18,0x18,0x39,0x39,0x19,0x19,0x21,0x20,0x23,0x22,0x18,0x18,0x03,0x02,0x01,0x00,0x07,0x06,0x05,0x04,0x38,0x38,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x18}},
    {REGFLAG_DELAY, 5, {}},
    {0xE7, 4, {0xFF,0x14,0x00,0x00}},
    {0xBD, 1, {0x01}},
    {0xE7, 1, {0x01}},
    {0xBD, 1, {0x00}},
    {0xB6, 3, {0x57,0x57,0xE0}},
    {0xE0, 46, {0x00,0x00,0x05,0x08,0x0B,0x07,0x17,0x1B,0x21,0x1F,0x3D,0x48,0x55,0x6C,0x71,0x81,0x8F,0xA7,0xAA,0x56,0x60,0x6D,0x7F,0x00,0x00,0x05,0x08,0x0B,0x07,0x17,0x1B,0x21,0x1F,0x3D,0x48,0x55,0x6C,0x71,0x81,0x8F,0xA7,0xAA,0x56,0x60,0x6D,0x7F}},
    {REGFLAG_DELAY, 5, {}},
    {0xBA, 19, {0x71,0x23,0xA8,0x93,0xB2,0xC0,0xC0,0x01,0x10,0x00,0x00,0x00,0x0C,0x3D,0x82,0x77,0x04,0x01,0x00}},
    {0xC0, 6, {0x33,0x33,0x00,0x00,0x19,0x21}},
    {0xBD, 1, {0x01}},
    {0xCB, 1, {0x01}},
    {0xD3, 3, {0x01,0x00,0xF9}},
    {REGFLAG_DELAY, 5, {}},
    {0xBD, 1, {0x00}},
    {0xCB, 5, {0x00,0x53,0x00,0x02,0x63}}, //OSC Trim
    {0xBF, 7, {0xFC,0x00,0x24,0x9E,0xF6,0x00,0x5D}},
    {0xBD, 1, {0x02}},
    {0xB4, 8, {0x42,0x00,0x33,0x02,0x33,0x88,0xB3,0x00}},
    {0xBD, 1, {0x00}},
    {0xD1, 2, {0x20,0x01}},
    {0xBD, 1, {0x02}},
    {0xB1, 3, {0x7F,0x03,0xF5}},
    {0xBD, 1, {0x00}},
    {0x55, 1, {0x01}},
    {0x53, 1, {0x24}},
    {0xC9, 3, {0x04,0x08,0x80}}, //PWM freq 30Khz
    {0x35, 1, {0x00}},
    {0x11, 0, {0x11}},
    {REGFLAG_DELAY, 80, {}},
    {0x29, 0, {0x29}},
    {REGFLAG_DELAY, 20, {}},
};

static struct LCM_setting_table bl_level[] = {
	//{0xFF, 0x03, {0x98, 0x81, 0x00} },
	{0x51, 0x02, {0x00, 0x00} },
	//{0xFF, 0x03, {0x98, 0x81, 0x0F} },
	{REGFLAG_END_OF_TABLE, 0x00, {} }
};

static struct LCM_setting_table lcm_cabc_enter_setting[] = {
	{0x55, 0x01, {0x01} },
	{REGFLAG_END_OF_TABLE, 0x00, {}}
};

static struct LCM_setting_table lcm_cabc_enter_setting1[] = {
	{0x55, 0x01, {0x03} },
	{REGFLAG_END_OF_TABLE, 0x00, {}}
};

static struct LCM_setting_table lcm_cabc_exit_setting[] = {
	{0x55, 0x01, {0x00} },
	{REGFLAG_END_OF_TABLE, 0x00, {}}
};
#ifdef ODM_WT_EDIT
//Zhenzhen.Wu@ODM_WT.MM.Display.LCD, 2019/12/15, add LCD dimming control
static struct LCM_setting_table lcm_dimming_enable_setting[] = {
	{0x53, 0x01, {0x2c} },
	{REGFLAG_END_OF_TABLE, 0x00, {}}
};

static struct LCM_setting_table lcm_dimming_disable_setting[] = {
	{0x53, 0x01, {0x24} },
	{REGFLAG_END_OF_TABLE, 0x00, {}}
};
#endif

//static struct LCM_setting_table lcm_cabc_enter_setting3[] = {
//	{0xFF, 0x03, {0x98, 0x81, 0x03} },
//	{0xAC, 0x01, {0xDD} },
//	{0xFF, 0x03, {0x98, 0x81, 0x00} },
//	{0x53, 0x01, {0x2c} },//enable dimming
//	{0x55, 0x01, {0x03} },//enable cabc 2
//	{REGFLAG_END_OF_TABLE, 0x00, {}}
//};

static void push_table(void *cmdq, struct LCM_setting_table *table,
	unsigned int count, unsigned char force_update)
{
	unsigned int i, cmd;

	for (i = 0; i < count; i++) {
		cmd = table[i].cmd;

		switch (cmd) {

		case REGFLAG_DELAY:
			if (table[i].count <= 10)
				MDELAY(table[i].count);
			else
				MDELAY(table[i].count);
			break;

		case REGFLAG_UDELAY:
			UDELAY(table[i].count);
			break;

		case REGFLAG_END_OF_TABLE:
			break;

		default:
			dsi_set_cmdq_V22(cmdq, cmd, table[i].count,
				table[i].para_list, force_update);
		}
	}
}

/* --------------------------------------------------------------------------- */
/* LCM Driver Implementations */
/* --------------------------------------------------------------------------- */

static void lcm_set_util_funcs(const struct LCM_UTIL_FUNCS *util)
{
	memcpy(&lcm_util, util, sizeof(struct LCM_UTIL_FUNCS));
}

static void lcm_get_params(struct LCM_PARAMS *params)
{
	memset(params, 0, sizeof(struct LCM_PARAMS));

	params->type = LCM_TYPE_DSI;
	params->width = FRAME_WIDTH;
	params->height = FRAME_HEIGHT;
	params->physical_width = 68;
	params->physical_height = 143;

	#if (LCM_DSI_CMD_MODE)
	params->dsi.mode = CMD_MODE;
	params->dsi.switch_mode = SYNC_PULSE_VDO_MODE;
	#else
	params->dsi.mode = SYNC_PULSE_VDO_MODE;
	params->dsi.switch_mode = CMD_MODE;
	#endif
	params->dsi.switch_mode_enable = 0;

	/* DSI */
	/* Command mode setting */
	params->dsi.LANE_NUM = LCM_THREE_LANE;
	/* The following defined the fomat for data coming from LCD engine. */
	params->dsi.data_format.color_order = LCM_COLOR_ORDER_RGB;
	params->dsi.data_format.trans_seq = LCM_DSI_TRANS_SEQ_MSB_FIRST;
	params->dsi.data_format.padding = LCM_DSI_PADDING_ON_LSB;
	params->dsi.data_format.format = LCM_DSI_FORMAT_RGB888;

	/* Highly depends on LCD driver capability. */
	params->dsi.packet_size = 256;
	/* video mode timing */

	params->dsi.PS = LCM_PACKED_PS_24BIT_RGB888;

	params->dsi.vertical_sync_active = 2;
	params->dsi.vertical_backporch = 14;
	params->dsi.vertical_frontporch = 235;
//	params->dsi.vertical_frontporch_for_low_power = 540;
	params->dsi.vertical_active_line = FRAME_HEIGHT;

	params->dsi.horizontal_sync_active = 20;
	params->dsi.horizontal_backporch = 28;
	params->dsi.horizontal_frontporch = 30;
	params->dsi.horizontal_active_pixel = FRAME_WIDTH;
	params->dsi.ssc_disable = 1; /*SSC config disable*/
	params->dsi.ssc_range = 1; /*SSC range*/
	params->dsi.PLL_CLOCK = 360; /* this value must be in MTK suggested table */

	params->dsi.CLK_TRAIL = 8;
	params->dsi.clk_lp_per_line_enable = 0;
	params->dsi.esd_check_enable = 1;
	params->dsi.customization_esd_check_enable = 1;
	params->dsi.lcm_esd_check_table[0].cmd = 0x0A;
	params->dsi.lcm_esd_check_table[0].count = 1;
	params->dsi.lcm_esd_check_table[0].para_list[0] = 0x9D;

//	params->dsi.lcm_esd_check_table[0].cmd = 0x09;
//	params->dsi.lcm_esd_check_table[0].count = 3;
//	params->dsi.lcm_esd_check_table[0].para_list[0] = 0x80;//for abnormal(RGB) color
//	params->dsi.lcm_esd_check_table[0].para_list[1] = 0x03;//include ESD REG:0Ah
//	params->dsi.lcm_esd_check_table[0].para_list[2] = 0x06;//for abnormal(BW) color
#ifdef CONFIG_MTK_ROUND_CORNER_SUPPORT
	params->round_corner_en = 1;
	params->full_content = 1;
	params->corner_pattern_width = 720;
	params->corner_pattern_height = 82;
	params->corner_pattern_height_bot = 82;
#endif
/* wangxianfei@ODM.HQ.Multimedia.LCM 2018/12/21 modified for backlight remapping*/
	params->blmap = blmap_table;
	params->blmap_size = sizeof(blmap_table)/sizeof(blmap_table[0]);
	params->brightness_max = 2047;
	params->brightness_min = 6;
}
extern void lcm_set_pwr(int val);
extern void lcm_set_pwr_n(int val);
static void lcm_resume_power(void);
static void lcm_init_power(void)/*esd recover will call it */
{
	LCM_LOGI("%s: enter\n", __func__);
	lcm_resume_power();
	LCM_LOGI("%s: exit\n", __func__);
}

#ifdef ODM_HQ_EDIT
extern int gesture_flag;
#endif
static void lcm_suspend_power(void)
{
	if (gesture_flag == 1) {
		LCM_LOGI("%s: enter\n", __func__);
		LCM_LOGI("%s: exit\n", __func__);
	} else {
		LCM_LOGI("%s: enter\n", __func__);
		lcm_set_pwr_n(0);
		MDELAY(2);
		lcm_set_pwr(0);
		LCM_LOGI("%s: exit\n", __func__);
	}
}

#ifdef ODM_WT_EDIT
static void lcm_shutdown_power(void)
{
	LCM_LOGI("%s: enter\n", __func__);
	SET_RESET_PIN(0);
	MDELAY(1);
	lcm_set_pwr_n(0);
	MDELAY(2);
	lcm_set_pwr(0);
	LCM_LOGI("%s: exit\n", __func__);

}
#endif

extern int sm5109_read_bytes(u8 addr, void *data);
extern int sm5109_write_bytes(unsigned char addr, unsigned char value);
static void lcm_resume_power(void)
{
	char buf[2]={0};
	LCM_LOGI("%s: enter\n", __func__);
	lcm_set_pwr(1);
	MDELAY(3);//powertiming
	lcm_set_pwr_n(1);
	MDELAY(1);
	//SM5109 reg[03] default value=0x03 NT50358CG reg[03]=0x33
	//printk("read 0x00=%d,0x01=%d,lcd_bias_id=%d\n",buf[0],buf[1],lcd_bias_id);
	if(lcd_bias_id==0x03){
		sm5109_write_bytes(0x00,0x34);//6v avdd SM5109
		sm5109_write_bytes(0x01,0x14);//-6v avee
	}else{
		sm5109_write_bytes(0x00,0x14);//6v avdd NT50358CG
		sm5109_write_bytes(0x01,0x14);//-6v avee
	}

	sm5109_read_bytes(0x00,&buf[0]);
	sm5109_read_bytes(0x01,&buf[1]);

	LCM_LOGI("%s: read 0x00=%d,0x01=%d exit\n", __func__,buf[0],buf[1]);
}

//extern unsigned int esd_recovery_state;
#ifdef ODM_HQ_EDIT
/*Jiangrunran@ODM.BSP.TP 2019/10/11 add for TP resume*/
extern void lcd_queue_load_tp_fw(void);
#endif

static void lcm_init_lcm(void)
{
	pr_err("ILITEK:[INFO]: lcm reset %s: \n", __func__);

	#ifdef ODM_WT_EDIT
	//Zhenzhen.Wu@ODM_WT.MM.Display.LCD, 2019/12/15, modify for LCD resume time
	//SET_RESET_PIN(1);
	//MDELAY(20);
	SET_RESET_PIN(0);
	MDELAY(5);
	SET_RESET_PIN(1);
	MDELAY(15);
	#endif

	pr_err("ILITEK:[INFO]: lcd call tp resume %s:\n", __func__);
	lcd_queue_load_tp_fw();

	push_table(NULL, init_setting, sizeof(init_setting) / sizeof(struct LCM_setting_table), 1);
}

static void lcm_suspend(void)
{
	LCM_LOGI("%s: enter\n", __func__);
	push_table(NULL, lcm_suspend_setting, sizeof(lcm_suspend_setting) / sizeof(struct LCM_setting_table), 1);
	SET_RESET_PIN(1);	//when entering sleep reset pin supposed to be high
	MDELAY(2);/*for power timing*/
	LCM_LOGI("%s: exit\n", __func__);
}

static void lcm_resume(void)
{
	lcm_init_lcm();
}

static unsigned int lcm_compare_id(void)
{
	LCM_LOGI("%s: enter\n", __func__);
	return 0;
}
#define MAX_LEVEL 2047
#define MAX_BL_LEVEL 4095
static void lcm_setbacklight_cmdq(void *handle, unsigned int level)//255 0xf f //0x0f 0xf0
{
	unsigned int bl_value;
//	LCM_LOGI(" %s,backlight: level = %d\n", __func__, level);
	bl_value=(level*MAX_BL_LEVEL)/MAX_LEVEL;
	bl_level[0].para_list[0] = 0x000F&(bl_value>>8);
	bl_level[0].para_list[1] = 0x00FF&(bl_value);
	push_table(handle, bl_level, sizeof(bl_level) / sizeof(struct LCM_setting_table), 1);
	LCM_LOGI(" %s, level = %d,bl_value=%d  0x51[0]=0x%x, 0x51[1]=0x%x\n", __func__, level,bl_value, bl_level[0].para_list[0], bl_level[0].para_list[1]);
}

static void lcm_set_cabc_mode_cmdq(void *handle, unsigned int level)
{
	printk("%s [lcd] cabc_mode is %d \n", __func__, level);

	if(1==level){
		push_table(handle, lcm_cabc_enter_setting, sizeof(lcm_cabc_enter_setting) / sizeof(struct LCM_setting_table), 1);
	}else if(3==level){
		push_table(handle, lcm_cabc_enter_setting1, sizeof(lcm_cabc_enter_setting1) / sizeof(struct LCM_setting_table), 1);
	}else {
		push_table(handle, lcm_cabc_exit_setting, sizeof(lcm_cabc_exit_setting) / sizeof(struct LCM_setting_table), 1);
	}
}

#ifdef ODM_WT_EDIT
//Zhenzhen.Wu@ODM_WT.MM.Display.LCD, 2019/12/15, add LCD dimming control
static void lcm_set_dimming_mode_cmdq(void *handle, unsigned int level)
{
	printk("%s [lcd] dimming_mode is %d \n", __func__, level);

	if(1==level){
		push_table(handle, lcm_dimming_enable_setting, sizeof(lcm_dimming_enable_setting) / sizeof(struct LCM_setting_table), 1);
	}else {
		push_table(handle, lcm_dimming_disable_setting, sizeof(lcm_dimming_disable_setting) / sizeof(struct LCM_setting_table), 1);
	}
}
#endif
struct LCM_DRIVER hx83102d_hdp_dsi_vdo_hlt_lcm_drv = {
	.name = "hx83102d_hdp_dsi_vdo_hlt",
	.set_util_funcs = lcm_set_util_funcs,
	.get_params = lcm_get_params,
	.init = lcm_init_lcm,
	.suspend = lcm_suspend,
	.resume = lcm_resume,
	.compare_id = lcm_compare_id,
	.init_power = lcm_init_power,
	.resume_power = lcm_resume_power,
#ifdef ODM_WT_EDIT
	.shutdown_power = lcm_shutdown_power,
#endif
	.suspend_power = lcm_suspend_power,
	.set_cabc_mode_cmdq = lcm_set_cabc_mode_cmdq,
#ifdef ODM_WT_EDIT
	//Zhenzhen.Wu@ODM_WT.MM.Display.LCD, 2019/12/15, add LCD dimming control
	.set_dimming_mode_cmdq = lcm_set_dimming_mode_cmdq,
#endif
	.set_backlight_cmdq = lcm_setbacklight_cmdq,
};